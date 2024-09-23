// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer_state.h"

#include <set>
#include <string_view>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_switches.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/mime_util.h"
#include "media/base/stream_parser.h"
#include "media/base/video_codec_string_parsers.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/frame_processor.h"
#include "media/filters/source_buffer_stream.h"
#include "media/media_buildflags.h"

namespace media {

enum {
  // Limits the number of MEDIA_LOG() calls warning the user that a muxed stream
  // media segment is missing a block from at least one of the audio or video
  // tracks.
  kMaxMissingTrackInSegmentLogs = 10,
};

namespace {

base::TimeDelta EndTimestamp(const StreamParser::BufferQueue& queue) {
  return queue.back()->timestamp() + queue.back()->duration();
}

// Check the input |bytestream_ids| and return false if
// duplicate track ids are detected.
bool CheckBytestreamTrackIds(const MediaTracks& tracks) {
  std::set<StreamParser::TrackId> bytestream_ids;
  for (const auto& track : tracks.tracks()) {
    const StreamParser::TrackId& track_id = track->stream_id();
    if (bytestream_ids.find(track_id) != bytestream_ids.end()) {
      return false;
    }
    bytestream_ids.insert(track_id);
  }
  return true;
}

unsigned GetMSEBufferSizeLimitIfExists(std::string_view switch_string) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  unsigned memory_limit;
  if (command_line->HasSwitch(switch_string) &&
      base::StringToUint(command_line->GetSwitchValueASCII(switch_string),
                         &memory_limit)) {
    return memory_limit * 1024 * 1024;
  }
  return 0;
}

}  // namespace

// List of time ranges for each SourceBuffer.
// static
Ranges<base::TimeDelta> SourceBufferState::ComputeRangesIntersection(
    const RangesList& active_ranges,
    bool ended) {
  // Implementation of HTMLMediaElement.buffered algorithm in MSE spec.
  // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#dom-htmlmediaelement.buffered

  // Step 1: If activeSourceBuffers.length equals 0 then return an empty
  //  TimeRanges object and abort these steps.
  if (active_ranges.empty())
    return Ranges<base::TimeDelta>();

  // Step 2: Let active ranges be the ranges returned by buffered for each
  //  SourceBuffer object in activeSourceBuffers.
  // Step 3: Let highest end time be the largest range end time in the active
  //  ranges.
  base::TimeDelta highest_end_time;
  for (const auto& range : active_ranges) {
    if (!range.size())
      continue;

    highest_end_time = std::max(highest_end_time, range.end(range.size() - 1));
  }

  // Step 4: Let intersection ranges equal a TimeRange object containing a
  //  single range from 0 to highest end time.
  Ranges<base::TimeDelta> intersection_ranges;
  intersection_ranges.Add(base::TimeDelta(), highest_end_time);

  // Step 5: For each SourceBuffer object in activeSourceBuffers run the
  //  following steps:
  for (const auto& range : active_ranges) {
    // Step 5.1: Let source ranges equal the ranges returned by the buffered
    //  attribute on the current SourceBuffer.
    Ranges<base::TimeDelta> source_ranges = range;

    // Step 5.2: If readyState is "ended", then set the end time on the last
    //  range in source ranges to highest end time.
    if (ended && source_ranges.size()) {
      source_ranges.Add(source_ranges.start(source_ranges.size() - 1),
                        highest_end_time);
    }

    // Step 5.3: Let new intersection ranges equal the intersection between
    // the intersection ranges and the source ranges.
    // Step 5.4: Replace the ranges in intersection ranges with the new
    // intersection ranges.
    intersection_ranges = intersection_ranges.IntersectionWith(source_ranges);
  }

  return intersection_ranges;
}

SourceBufferState::SourceBufferState(
    std::unique_ptr<StreamParser> stream_parser,
    std::unique_ptr<FrameProcessor> frame_processor,
    CreateDemuxerStreamCB create_demuxer_stream_cb,
    MediaLog* media_log)
    : timestamp_offset_during_append_(nullptr),
      parsing_media_segment_(false),
      stream_parser_(stream_parser.release()),
      frame_processor_(frame_processor.release()),
      create_demuxer_stream_cb_(std::move(create_demuxer_stream_cb)),
      media_log_(media_log),
      state_(UNINITIALIZED) {
  DCHECK(create_demuxer_stream_cb_);
  DCHECK(frame_processor_);
}

SourceBufferState::~SourceBufferState() {
  Shutdown();
}

void SourceBufferState::Init(StreamParser::InitCB init_cb,
                             std::optional<std::string_view> expected_codecs,
                             const StreamParser::EncryptedMediaInitDataCB&
                                 encrypted_media_init_data_cb) {
  DCHECK_EQ(state_, UNINITIALIZED);
  init_cb_ = std::move(init_cb);
  encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  state_ = PENDING_PARSER_CONFIG;
  InitializeParser(expected_codecs);
}

void SourceBufferState::ChangeType(
    std::unique_ptr<StreamParser> new_stream_parser,
    const std::string& new_expected_codecs) {
  DCHECK_GE(state_, PENDING_PARSER_CONFIG);
  DCHECK_NE(state_, PENDING_PARSER_INIT);
  DCHECK(!parsing_media_segment_);

  // If this source buffer has already handled an initialization segment, avoid
  // running |init_cb_| again later.
  if (state_ == PARSER_INITIALIZED)
    state_ = PENDING_PARSER_RECONFIG;

  stream_parser_ = std::move(new_stream_parser);
  InitializeParser(new_expected_codecs);
}

void SourceBufferState::SetSequenceMode(bool sequence_mode) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetSequenceMode(sequence_mode);
}

void SourceBufferState::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DCHECK(!parsing_media_segment_);

  frame_processor_->SetGroupStartTimestampIfInSequenceMode(timestamp_offset);
}

void SourceBufferState::SetTracksWatcher(
    Demuxer::MediaTracksUpdatedCB tracks_updated_cb) {
  DCHECK(!init_segment_received_cb_);
  DCHECK(tracks_updated_cb);
  init_segment_received_cb_ = std::move(tracks_updated_cb);
}

void SourceBufferState::SetParseWarningCallback(
    SourceBufferParseWarningCB parse_warning_cb) {
  // Give the callback to |frame_processor_|; none of these warnings are
  // currently emitted elsewhere.
  frame_processor_->SetParseWarningCallback(std::move(parse_warning_cb));
}

bool SourceBufferState::AppendToParseBuffer(base::span<const uint8_t> data) {
  return stream_parser_->AppendToParseBuffer(data);
}

StreamParser::ParseStatus SourceBufferState::RunSegmentParserLoop(
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  DCHECK(!new_configs_possible_);
  new_configs_possible_ = true;
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;
  timestamp_offset_during_append_ = timestamp_offset;

  // TODO(wolenetz): Curry and pass a NewBuffersCB here bound with append window
  // and timestamp offset pointer. See http://crbug.com/351454.
  StreamParser::ParseStatus result =
      stream_parser_->Parse(StreamParser::kMaxPendingBytesPerParse);

  if (result == StreamParser::ParseStatus::kFailed) {
    MEDIA_LOG(ERROR, media_log_)
        << __func__ << ": stream parsing failed. append_window_start="
        << append_window_start.InSecondsF()
        << " append_window_end=" << append_window_end.InSecondsF();
  }

  timestamp_offset_during_append_ = nullptr;
  new_configs_possible_ = false;
  return result;
}

bool SourceBufferState::AppendChunks(
    std::unique_ptr<StreamParser::BufferQueue> buffer_queue,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  DCHECK(!new_configs_possible_);
  new_configs_possible_ = true;
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;
  timestamp_offset_during_append_ = timestamp_offset;

  // TODO(wolenetz): Curry and pass a NewBuffersCB here bound with append window
  // and timestamp offset pointer. See http://crbug.com/351454.
  bool result = stream_parser_->ProcessChunks(std::move(buffer_queue));
  if (!result) {
    MEDIA_LOG(ERROR, media_log_)
        << __func__ << ": Processing encoded chunks for buffering failed.";
  }

  timestamp_offset_during_append_ = nullptr;
  new_configs_possible_ = false;
  return result;
}

void SourceBufferState::ResetParserState(base::TimeDelta append_window_start,
                                         base::TimeDelta append_window_end,
                                         base::TimeDelta* timestamp_offset) {
  DCHECK(timestamp_offset);
  DCHECK(!timestamp_offset_during_append_);
  timestamp_offset_during_append_ = timestamp_offset;
  append_window_start_during_append_ = append_window_start;
  append_window_end_during_append_ = append_window_end;

  stream_parser_->Flush();
  timestamp_offset_during_append_ = nullptr;

  frame_processor_->Reset();
  parsing_media_segment_ = false;
  media_segment_has_data_for_track_.clear();
}

void SourceBufferState::Remove(base::TimeDelta start,
                               base::TimeDelta end,
                               base::TimeDelta duration) {
  for (const auto& it : audio_streams_) {
    it.second->Remove(start, end, duration);
  }

  for (const auto& it : video_streams_) {
    it.second->Remove(start, end, duration);
  }
}

bool SourceBufferState::EvictCodedFrames(base::TimeDelta media_time,
                                         size_t newDataSize) {
  size_t total_buffered_size = 0;
  for (const auto& it : audio_streams_)
    total_buffered_size += it.second->GetMemoryUsage();
  for (const auto& it : video_streams_)
    total_buffered_size += it.second->GetMemoryUsage();

  DVLOG(3) << __func__ << " media_time=" << media_time.InSecondsF()
           << " newDataSize=" << newDataSize
           << " total_buffered_size=" << total_buffered_size;

  if (total_buffered_size == 0)
    return true;

  bool success = true;
  for (const auto& it : audio_streams_) {
    uint64_t curr_size = it.second->GetMemoryUsage();
    if (curr_size == 0)
      continue;
    uint64_t estimated_new_size = newDataSize * curr_size / total_buffered_size;
    DCHECK_LE(estimated_new_size, SIZE_MAX);
    success &= it.second->EvictCodedFrames(
        media_time, static_cast<size_t>(estimated_new_size));
  }
  for (const auto& it : video_streams_) {
    uint64_t curr_size = it.second->GetMemoryUsage();
    if (curr_size == 0)
      continue;
    uint64_t estimated_new_size = newDataSize * curr_size / total_buffered_size;
    DCHECK_LE(estimated_new_size, SIZE_MAX);
    success &= it.second->EvictCodedFrames(
        media_time, static_cast<size_t>(estimated_new_size));
  }

  DVLOG(3) << __func__ << " success=" << success;
  return success;
}

void SourceBufferState::OnMemoryPressure(
    base::TimeDelta media_time,
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level,
    bool force_instant_gc) {
  // TODO(sebmarchand): Check if MEMORY_PRESSURE_LEVEL_MODERATE should also be
  // ignored.
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Notify video streams about memory pressure first, since video typically
  // takes up the most memory and that's where we can expect most savings.
  for (const auto& it : video_streams_) {
    it.second->OnMemoryPressure(media_time, memory_pressure_level,
                                force_instant_gc);
  }
  for (const auto& it : audio_streams_) {
    it.second->OnMemoryPressure(media_time, memory_pressure_level,
                                force_instant_gc);
  }
}

Ranges<base::TimeDelta> SourceBufferState::GetBufferedRanges(
    base::TimeDelta duration,
    bool ended) const {
  RangesList ranges_list;
  for (const auto& it : audio_streams_)
    ranges_list.push_back(it.second->GetBufferedRanges(duration));

  for (const auto& it : video_streams_)
    ranges_list.push_back(it.second->GetBufferedRanges(duration));

  return ComputeRangesIntersection(ranges_list, ended);
}

base::TimeDelta SourceBufferState::GetLowestPresentationTimestamp() const {
  base::TimeDelta min_pts = kInfiniteDuration;

  for (const auto& it : audio_streams_) {
    min_pts = std::min(min_pts, it.second->GetLowestPresentationTimestamp());
  }

  for (const auto& it : video_streams_) {
    min_pts = std::min(min_pts, it.second->GetLowestPresentationTimestamp());
  }

  DCHECK_LE(base::TimeDelta(), min_pts);
  if (min_pts == kInfiniteDuration) {
    return base::TimeDelta();
  }

  return min_pts;
}

base::TimeDelta SourceBufferState::GetHighestPresentationTimestamp() const {
  base::TimeDelta max_pts;

  for (const auto& it : audio_streams_) {
    max_pts = std::max(max_pts, it.second->GetHighestPresentationTimestamp());
  }

  for (const auto& it : video_streams_) {
    max_pts = std::max(max_pts, it.second->GetHighestPresentationTimestamp());
  }

  return max_pts;
}

base::TimeDelta SourceBufferState::GetMaxBufferedDuration() const {
  base::TimeDelta max_duration;

  for (const auto& it : audio_streams_) {
    max_duration = std::max(max_duration, it.second->GetBufferedDuration());
  }

  for (const auto& it : video_streams_) {
    max_duration = std::max(max_duration, it.second->GetBufferedDuration());
  }

  return max_duration;
}

void SourceBufferState::StartReturningData() {
  for (const auto& it : audio_streams_) {
    it.second->StartReturningData();
  }

  for (const auto& it : video_streams_) {
    it.second->StartReturningData();
  }
}

void SourceBufferState::AbortReads() {
  for (const auto& it : audio_streams_) {
    it.second->AbortReads();
  }

  for (const auto& it : video_streams_) {
    it.second->AbortReads();
  }
}

void SourceBufferState::Seek(base::TimeDelta seek_time) {
  for (const auto& it : audio_streams_) {
    it.second->Seek(seek_time);
  }

  for (const auto& it : video_streams_) {
    it.second->Seek(seek_time);
  }
}

void SourceBufferState::CompletePendingReadIfPossible() {
  for (const auto& it : audio_streams_) {
    it.second->CompletePendingReadIfPossible();
  }

  for (const auto& it : video_streams_) {
    it.second->CompletePendingReadIfPossible();
  }
}

void SourceBufferState::OnSetDuration(base::TimeDelta duration) {
  for (const auto& it : audio_streams_) {
    it.second->OnSetDuration(duration);
  }

  for (const auto& it : video_streams_) {
    it.second->OnSetDuration(duration);
  }
}

void SourceBufferState::MarkEndOfStream() {
  for (const auto& it : audio_streams_) {
    it.second->MarkEndOfStream();
  }

  for (const auto& it : video_streams_) {
    it.second->MarkEndOfStream();
  }
}

void SourceBufferState::UnmarkEndOfStream() {
  for (const auto& it : audio_streams_) {
    it.second->UnmarkEndOfStream();
  }

  for (const auto& it : video_streams_) {
    it.second->UnmarkEndOfStream();
  }
}

void SourceBufferState::Shutdown() {
  for (const auto& it : audio_streams_) {
    it.second->Shutdown();
  }

  for (const auto& it : video_streams_) {
    it.second->Shutdown();
  }
}

void SourceBufferState::SetMemoryLimits(DemuxerStream::Type type,
                                        size_t memory_limit) {
  switch (type) {
    case DemuxerStream::AUDIO:
      for (const auto& it : audio_streams_) {
        it.second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::VIDEO:
      for (const auto& it : video_streams_) {
        it.second->SetStreamMemoryLimit(memory_limit);
      }
      break;
    case DemuxerStream::UNKNOWN:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

bool SourceBufferState::IsSeekWaitingForData() const {
  for (const auto& it : audio_streams_) {
    if (it.second->IsSeekWaitingForData())
      return true;
  }

  for (const auto& it : video_streams_) {
    if (it.second->IsSeekWaitingForData())
      return true;
  }

  return false;
}

void SourceBufferState::InitializeParser(
    std::optional<std::string_view> expected_codecs) {
  expected_audio_codecs_.clear();
  expected_video_codecs_.clear();

  std::vector<std::string> expected_codecs_parsed;
  if ((strict_codec_expectations_ = expected_codecs.has_value())) {
    DVLOG(1) << __func__ << " expected_codecs=" << expected_codecs.value();
    SplitCodecs(expected_codecs.value(), &expected_codecs_parsed);
  }

  std::vector<AudioCodec> expected_acodecs;
  std::vector<VideoCodec> expected_vcodecs;
  for (const auto& codec_id : expected_codecs_parsed) {
    AudioCodec acodec = StringToAudioCodec(codec_id);
    if (acodec != AudioCodec::kUnknown) {
      expected_audio_codecs_.push_back(acodec);
      continue;
    }
    VideoCodec vcodec = StringToVideoCodec(codec_id);
    if (vcodec != VideoCodec::kUnknown) {
      expected_video_codecs_.push_back(vcodec);
      continue;
    }
    MEDIA_LOG(INFO, media_log_) << "Unrecognized media codec: " << codec_id;
  }

  stream_parser_->Init(
      base::BindOnce(&SourceBufferState::OnSourceInitDone,
                     base::Unretained(this)),
      base::BindRepeating(&SourceBufferState::OnNewConfigs,
                          base::Unretained(this)),
      base::BindRepeating(&SourceBufferState::OnNewBuffers,
                          base::Unretained(this)),
      base::BindRepeating(&SourceBufferState::OnEncryptedMediaInitData,
                          base::Unretained(this)),
      base::BindRepeating(&SourceBufferState::OnNewMediaSegment,
                          base::Unretained(this)),
      base::BindRepeating(&SourceBufferState::OnEndOfMediaSegment,
                          base::Unretained(this)),
      media_log_);
}

bool SourceBufferState::OnNewConfigs(std::unique_ptr<MediaTracks> tracks) {
  DCHECK(tracks.get());
  DCHECK_GE(state_, PENDING_PARSER_CONFIG);

  // Check that there is no clashing bytestream track ids.
  if (!CheckBytestreamTrackIds(*tracks)) {
    MEDIA_LOG(ERROR, media_log_) << "Duplicate bytestream track ids detected";
    for (const auto& track : tracks->tracks()) {
      const StreamParser::TrackId& track_id = track->stream_id();
      MEDIA_LOG(DEBUG, media_log_) << TrackTypeToStr(track->type()) << " track "
                                   << " bytestream track id=" << track_id;
    }
    return false;
  }

  // MSE spec allows new configs to be emitted only during
  // RunSegmentParserLoop(), but not during Flush or parser reset operations.
  CHECK(new_configs_possible_);

  bool success = true;

  // TODO(wolenetz): Update codec string strictness, if necessary, once spec
  // issue https://github.com/w3c/media-source/issues/161 is resolved.
  std::vector<AudioCodec> expected_acodecs = expected_audio_codecs_;
  std::vector<VideoCodec> expected_vcodecs = expected_video_codecs_;

  // TODO(wolenetz): Once codec strictness is relaxed, we can change
  // |allow_codec_changes| to always be true. Until then, we only allow codec
  // changes on explicit ChangeType().
  const bool allow_codec_changes = state_ == PENDING_PARSER_RECONFIG;

  FrameProcessor::TrackIdChanges track_id_changes;
  for (const auto& track : tracks->tracks()) {
    const auto& track_id = track->stream_id();

    if (track->type() == MediaTrack::Type::kAudio) {
      AudioDecoderConfig audio_config = tracks->getAudioConfig(track_id);
      DVLOG(1) << "Audio track_id=" << track_id
               << " config: " << audio_config.AsHumanReadableString();
      DCHECK(audio_config.IsValidConfig());

      if (strict_codec_expectations_) {
        const auto& it =
            base::ranges::find(expected_acodecs, audio_config.codec());
        if (it == expected_acodecs.end()) {
          MEDIA_LOG(ERROR, media_log_)
              << "Audio stream codec " << GetCodecName(audio_config.codec())
              << " doesn't match SourceBuffer codecs.";
          return false;
        }
        expected_acodecs.erase(it);
      }

      ChunkDemuxerStream* stream = nullptr;
      if (!first_init_segment_received_) {
        DCHECK(audio_streams_.find(track_id) == audio_streams_.end());
        stream = create_demuxer_stream_cb_.Run(DemuxerStream::AUDIO);
        if (!stream || !frame_processor_->AddTrack(track_id, stream)) {
          MEDIA_LOG(ERROR, media_log_) << "Failed to create audio stream.";
          return false;
        }
        audio_streams_[track_id] = stream;
        media_log_->SetProperty<MediaLogProperty::kAudioTracks>(
            std::vector<AudioDecoderConfig>{audio_config});
      } else {
        if (audio_streams_.size() > 1) {
          auto stream_it = audio_streams_.find(track_id);
          if (stream_it != audio_streams_.end())
            stream = stream_it->second;
        } else {
          // If there is only one audio track then bytestream id might change in
          // a new init segment. So update our state and notify frame processor.
          const auto& stream_it = audio_streams_.begin();
          if (stream_it != audio_streams_.end()) {
            stream = stream_it->second;
            if (stream_it->first != track_id) {
              track_id_changes[stream_it->first] = track_id;
              audio_streams_[track_id] = stream;
              audio_streams_.erase(stream_it->first);
            }
          }
        }
        if (!stream) {
          MEDIA_LOG(ERROR, media_log_) << "Got unexpected audio track"
                                       << " track_id=" << track_id;
          return false;
        }
      }

      track->set_id(stream->media_track_id());
      frame_processor_->OnPossibleAudioConfigUpdate(audio_config);
      success &= stream->UpdateAudioConfig(audio_config, allow_codec_changes,
                                           media_log_);
    } else if (track->type() == MediaTrack::Type::kVideo) {
      VideoDecoderConfig video_config = tracks->getVideoConfig(track_id);
      DVLOG(1) << "Video track_id=" << track_id
               << " config: " << video_config.AsHumanReadableString();
      DCHECK(video_config.IsValidConfig());

#if BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)
      // When ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION is true, in general
      // encrypted Dolby Vision is allowed while clear Dolby Vision is not.
      if (video_config.codec() == VideoCodec::kDolbyVision) {
        // If `kPlatformEncryptedDolbyVision` is disabled, encrypted Dolby
        // Vision is also not allowed, so just return false.
        if (!base::FeatureList::IsEnabled(kPlatformEncryptedDolbyVision)) {
          MEDIA_LOG(ERROR, media_log_)
              << "MSE playback of DolbyVision is not supported because "
                 "kPlatformEncryptedDolbyVision feature is disabled.";
          return false;
        }

        // If `kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled` is
        // specified which force allow Dolby Vision in Media Source.
        if (!base::FeatureList::IsEnabled(
                kAllowClearDolbyVisionInMseWhenPlatformEncryptedDvEnabled) &&
            !video_config.is_encrypted()) {
          MEDIA_LOG(ERROR, media_log_)
              << "MSE playback of DolbyVision is only supported via platform "
                 "decryptor, but the provided DV track is not encrypted.";
          return false;
        }
      }
#endif  // BUILDFLAG(ENABLE_PLATFORM_ENCRYPTED_DOLBY_VISION)

      if (strict_codec_expectations_) {
        const auto& it =
            base::ranges::find(expected_vcodecs, video_config.codec());
        if (it == expected_vcodecs.end()) {
          MEDIA_LOG(ERROR, media_log_)
              << "Video stream codec " << GetCodecName(video_config.codec())
              << " doesn't match SourceBuffer codecs.";
          return false;
        }
        expected_vcodecs.erase(it);
      }

      ChunkDemuxerStream* stream = nullptr;
      if (!first_init_segment_received_) {
        DCHECK(video_streams_.find(track_id) == video_streams_.end());
        stream = create_demuxer_stream_cb_.Run(DemuxerStream::VIDEO);
        if (!stream || !frame_processor_->AddTrack(track_id, stream)) {
          MEDIA_LOG(ERROR, media_log_) << "Failed to create video stream.";
          return false;
        }
        video_streams_[track_id] = stream;

        media_log_->SetProperty<MediaLogProperty::kVideoTracks>(
            std::vector<VideoDecoderConfig>{video_config});
      } else {
        if (video_streams_.size() > 1) {
          auto stream_it = video_streams_.find(track_id);
          if (stream_it != video_streams_.end())
            stream = stream_it->second;
        } else {
          // If there is only one video track then bytestream id might change in
          // a new init segment. So update our state and notify frame processor.
          const auto& stream_it = video_streams_.begin();
          if (stream_it != video_streams_.end()) {
            stream = stream_it->second;
            if (stream_it->first != track_id) {
              track_id_changes[stream_it->first] = track_id;
              video_streams_[track_id] = stream;
              video_streams_.erase(stream_it->first);
            }
          }
        }
        if (!stream) {
          MEDIA_LOG(ERROR, media_log_) << "Got unexpected video track"
                                       << " track_id=" << track_id;
          return false;
        }
      }

      track->set_id(stream->media_track_id());
      success &= stream->UpdateVideoConfig(video_config, allow_codec_changes,
                                           media_log_);
    } else {
      MEDIA_LOG(ERROR, media_log_) << "Error: unsupported media track type "
                                   << base::to_underlying(track->type());
      return false;
    }
  }

  if (!expected_acodecs.empty() || !expected_vcodecs.empty()) {
    for (const auto& acodec : expected_acodecs) {
      MEDIA_LOG(ERROR, media_log_) << "Initialization segment misses expected "
                                   << GetCodecName(acodec) << " track.";
    }
    for (const auto& vcodec : expected_vcodecs) {
      MEDIA_LOG(ERROR, media_log_) << "Initialization segment misses expected "
                                   << GetCodecName(vcodec) << " track.";
    }
    return false;
  }

  if (audio_streams_.empty() && video_streams_.empty()) {
    DVLOG(1) << __func__ << ": couldn't find a valid audio or video stream";
    return false;
  }

  if (!frame_processor_->UpdateTrackIds(track_id_changes)) {
    DVLOG(1) << __func__ << ": failed to remap track ids in frame processor";
    return false;
  }

  frame_processor_->SetAllTrackBuffersNeedRandomAccessPoint();

  if (!first_init_segment_received_) {
    first_init_segment_received_ = true;
    SetStreamMemoryLimits();
  }

  DVLOG(1) << "OnNewConfigs() : " << (success ? "success" : "failed");
  if (success) {
    if (state_ == PENDING_PARSER_CONFIG)
      state_ = PENDING_PARSER_INIT;
    if (state_ == PENDING_PARSER_RECONFIG)
      state_ = PENDING_PARSER_REINIT;
    DCHECK(init_segment_received_cb_);
    init_segment_received_cb_.Run(std::move(tracks));
  }

  return success;
}

void SourceBufferState::SetStreamMemoryLimits() {
  size_t audio_buf_size_limit =
      GetMSEBufferSizeLimitIfExists(switches::kMSEAudioBufferSizeLimitMb);
  if (audio_buf_size_limit) {
    MEDIA_LOG(INFO, media_log_)
        << "Custom audio per-track SourceBuffer size limit="
        << audio_buf_size_limit;
    for (const auto& it : audio_streams_)
      it.second->SetStreamMemoryLimit(audio_buf_size_limit);
  }

  size_t video_buf_size_limit =
      GetMSEBufferSizeLimitIfExists(switches::kMSEVideoBufferSizeLimitMb);
  if (video_buf_size_limit) {
    MEDIA_LOG(INFO, media_log_)
        << "Custom video per-track SourceBuffer size limit="
        << video_buf_size_limit;
    for (const auto& it : video_streams_)
      it.second->SetStreamMemoryLimit(video_buf_size_limit);
  }
}

void SourceBufferState::OnNewMediaSegment() {
  DVLOG(2) << "OnNewMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = true;
  media_segment_has_data_for_track_.clear();
}

void SourceBufferState::OnEndOfMediaSegment() {
  DVLOG(2) << "OnEndOfMediaSegment()";
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  parsing_media_segment_ = false;

  for (const auto& it : audio_streams_) {
    if (!media_segment_has_data_for_track_[it.first]) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_missing_track_logs_,
                        kMaxMissingTrackInSegmentLogs)
          << "Media segment did not contain any coded frames for track "
          << it.first << ", mismatching initialization segment. Therefore, MSE"
                         " coded frame processing may not interoperably detect"
                         " discontinuities in appended media.";
    }
  }
  for (const auto& it : video_streams_) {
    if (!media_segment_has_data_for_track_[it.first]) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_missing_track_logs_,
                        kMaxMissingTrackInSegmentLogs)
          << "Media segment did not contain any coded frames for track "
          << it.first << ", mismatching initialization segment. Therefore, MSE"
                         " coded frame processing may not interoperably detect"
                         " discontinuities in appended media.";
    }
  }
}

bool SourceBufferState::OnNewBuffers(
    const StreamParser::BufferQueueMap& buffer_queue_map) {
  DVLOG(2) << __func__ << " buffer_queues=" << buffer_queue_map.size();
  DCHECK_EQ(state_, PARSER_INITIALIZED);
  DCHECK(timestamp_offset_during_append_);
  DCHECK(parsing_media_segment_);

  for (const auto& [track_id, buffer_queue] : buffer_queue_map) {
    DCHECK(!buffer_queue.empty());
    media_segment_has_data_for_track_[track_id] = true;
  }

  const base::TimeDelta timestamp_offset_before_processing =
      *timestamp_offset_during_append_;

  // Calculate the new timestamp offset for audio/video tracks if the stream
  // parser corresponds to MSE MIME type with 'Generate Timestamps Flag' set
  // true.
  base::TimeDelta predicted_timestamp_offset =
      timestamp_offset_before_processing;
  if (generate_timestamps_flag()) {
    base::TimeDelta min_end_timestamp = kNoTimestamp;
    for (const auto& [track_id, buffer_queue] : buffer_queue_map) {
      DCHECK(!buffer_queue.empty());
      if (min_end_timestamp == kNoTimestamp ||
          EndTimestamp(buffer_queue) < min_end_timestamp) {
        min_end_timestamp = EndTimestamp(buffer_queue);
        DCHECK_NE(kNoTimestamp, min_end_timestamp);
      }
    }
    if (min_end_timestamp != kNoTimestamp)
      predicted_timestamp_offset += min_end_timestamp;
  }

  if (!frame_processor_->ProcessFrames(
          buffer_queue_map, append_window_start_during_append_,
          append_window_end_during_append_, timestamp_offset_during_append_)) {
    return false;
  }

  // Only update the timestamp offset if the frame processor hasn't already.
  if (generate_timestamps_flag() &&
      timestamp_offset_before_processing == *timestamp_offset_during_append_) {
    // TODO(wolenetz): This prediction assumes the last frame in each track
    // isn't dropped by append window trimming. See https://crbug.com/850316.
    *timestamp_offset_during_append_ = predicted_timestamp_offset;
  }

  return true;
}

void SourceBufferState::OnEncryptedMediaInitData(
    EmeInitDataType type,
    const std::vector<uint8_t>& init_data) {
  encrypted_media_init_data_reported_ = true;
  encrypted_media_init_data_cb_.Run(type, init_data);
}

void SourceBufferState::OnSourceInitDone(
    const StreamParser::InitParameters& params) {
  // We've either yet-to-run |init_cb_| if pending init, or we've previously
  // run it if pending reinit.
  DCHECK((init_cb_ && state_ == PENDING_PARSER_INIT) ||
         (!init_cb_ && state_ == PENDING_PARSER_REINIT));
  State old_state = state_;
  state_ = PARSER_INITIALIZED;

  if (old_state == PENDING_PARSER_INIT)
    std::move(init_cb_).Run(params);
}

}  // namespace media
