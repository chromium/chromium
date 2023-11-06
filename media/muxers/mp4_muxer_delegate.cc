// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_delegate.h"

#include "base/logging.h"
#include "components/version_info/version_info.h"
#include "media/base/audio_parameters.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/muxers/box_byte_stream.h"
#include "media/muxers/mp4_box_writer.h"
#include "media/muxers/mp4_fragment_box_writer.h"
#include "media/muxers/mp4_movie_box_writer.h"
#include "media/muxers/mp4_type_conversion.h"
#include "media/muxers/output_position_tracker.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#endif

namespace media {

namespace {

using mp4::writable_boxes::FragmentSampleFlags;
using mp4::writable_boxes::TrackFragmentHeaderFlags;
using mp4::writable_boxes::TrackFragmentRunFlags;
using mp4::writable_boxes::TrackHeaderFlags;

constexpr char kVideoHandlerName[] = "VideoHandler";
constexpr char kAudioHandlerName[] = "SoundHandler";
constexpr char kUndefinedLanguageName[] = "und";

// Milliseconds time scale is set in the Movie header and it will
// be the base for the duration.
constexpr uint32_t kMillisecondsTimeScale = 1000u;
constexpr uint32_t kAudioSamplesPerFrame = 1024u;

template <typename T>
uint32_t BuildFlags(const std::vector<T>& build_flags) {
  uint32_t flags = 0;
  for (auto flag : build_flags) {
    flags |= static_cast<uint32_t>(flag);
  }

  return flags;
}

void BuildTrack(
    mp4::writable_boxes::Movie& moov,
    size_t track_index,
    bool is_audio,
    uint32_t timescale,
    const mp4::writable_boxes::SampleDescription& sample_description) {
  mp4::writable_boxes::Track& track = moov.tracks[track_index];
  // `tkhd`.
  track.header.flags = BuildFlags<TrackHeaderFlags>(
      {TrackHeaderFlags::kTrackEnabled, TrackHeaderFlags::kTrackInMovie});

  track.header.track_id = track_index + 1;
  track.header.is_audio = is_audio;

  // `mdhd`
  track.media.header.timescale = timescale;
  track.media.header.language =
      kUndefinedLanguageName;  // use 'und' as default at this time.

  // `dinf`, `dref`, `url`.
  mp4::writable_boxes::DataInformation data_info;
  mp4::writable_boxes::DataUrlEntry url;
  data_info.data_reference.entries.emplace_back(std::move(url));
  track.media.information.data_information = std::move(data_info);

  // `trex`.
  mp4::writable_boxes::TrackExtends& audio_extends =
      moov.extends.track_extends[track_index];
  audio_extends.track_id = track_index + 1;

  // TODO(crbug.com/1464063): Various MP4 samples doesn't need
  // default_sample_duration, default_sample_size, default_sample_flags. We need
  // to investigate it further though whether we need to set these fields.
  audio_extends.default_sample_description_index = 1;
  audio_extends.default_sample_size = 0;
  audio_extends.default_sample_duration = base::Milliseconds(0);
  audio_extends.default_sample_flags = 0;

  // `stbl`, `stco`, `stsz`, `stts`, `stsc'.
  mp4::writable_boxes::SampleTable sample_table = {};
  sample_table.sample_to_chunk = mp4::writable_boxes::SampleToChunk{};
  sample_table.decoding_time_to_sample =
      mp4::writable_boxes::DecodingTimeToSample();
  sample_table.sample_size = mp4::writable_boxes::SampleSize();
  sample_table.sample_chunk_offset = mp4::writable_boxes::SampleChunkOffset();
  sample_table.sample_description = std::move(sample_description);

  track.media.information.sample_table = std::move(sample_table);
}

void AddNewTrack(Mp4MuxerDelegate::Fragment& fragment,
                 uint32_t track_index,
                 base::TimeDelta decode_time,
                 bool is_audio) {
  mp4::writable_boxes::TrackFragment& track_fragment =
      fragment.moof.track_fragments[track_index];

  // `tfhd`.
  track_fragment.header.track_id = track_index + 1;

  // `default-sample-flags`.
  std::vector<mp4::writable_boxes::FragmentSampleFlags> sample_flags;
  if (is_audio) {
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagDependsNo);
  } else {
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagIsNonSync);
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagDependsYes);
  }

  track_fragment.header.default_sample_flags =
      BuildFlags<mp4::writable_boxes::FragmentSampleFlags>(sample_flags);

  track_fragment.header.default_sample_duration = base::TimeDelta();
  track_fragment.header.default_sample_size = 0;

  std::vector<mp4::writable_boxes::TrackFragmentHeaderFlags>
      fragment_header_flags = {
          TrackFragmentHeaderFlags::kDefaultBaseIsMoof,
          TrackFragmentHeaderFlags::kkDefaultSampleFlagsPresent
          // TODO(crbug.com/1464063).
          // TrackFragmentHeaderFlags::kDefaultSampleDurationPresent,
      };
  track_fragment.header.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentHeaderFlags>(
          fragment_header_flags);

  // `trun`.
  track_fragment.run = {};
  track_fragment.run.sample_count = 0;

  std::vector<mp4::writable_boxes::TrackFragmentRunFlags> fragment_run_flags = {
      TrackFragmentRunFlags::kDataOffsetPresent,
      TrackFragmentRunFlags::kSampleDurationPresent,
      TrackFragmentRunFlags::kSampleSizePresent};

  if (is_audio) {
    track_fragment.run.first_sample_flags = 0u;
  } else {
    fragment_run_flags.emplace_back(
        TrackFragmentRunFlags::kFirstSampleFlagsPresent);
    // The first sample in the `trun` uses the `first_sample_flags` and
    // other sample will use `default_sample_flags`.
    track_fragment.run.first_sample_flags = static_cast<uint32_t>(
        mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);
  }
  track_fragment.run.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentRunFlags>(
          fragment_run_flags);

  // `tfdt`.
  track_fragment.decode_time.track_id = track_fragment.header.track_id;
  track_fragment.decode_time.base_media_decode_time = decode_time;
}

void AddTrackEntry(Mp4MuxerDelegate::Fragment& fragment,
                   uint32_t track_index,
                   base::TimeDelta decode_time,
                   bool is_audio) {
  mp4::writable_boxes::TrackFragment track_frag;
  fragment.moof.track_fragments.emplace_back(std::move(track_frag));
  fragment.mdat.track_data.emplace_back();

  AddNewTrack(fragment, track_index, decode_time, is_audio);
}

void CreateNewFragment(
    std::vector<std::unique_ptr<Mp4MuxerDelegate::Fragment>>& fragments,
    uint32_t track_index,
    base::TimeTicks timestamp,
    uint32_t total_tracks_count,
    base::TimeDelta decode_time,
    bool is_audio) {
  auto fragment = std::make_unique<Mp4MuxerDelegate::Fragment>();
  fragment->moof.header.sequence_number = fragments.size() + 1;

  // Keep consistent of the track index across the fragments.
  if (total_tracks_count == 1) {
    AddTrackEntry(*fragment, 0, decode_time, is_audio);
  } else if (total_tracks_count == 2) {
    // Ensures the order of the added track.
    bool audio_is_first_track =
        (is_audio && (track_index == 0)) || (!is_audio && (track_index == 1));
    if (audio_is_first_track) {
      AddTrackEntry(*fragment, 0, decode_time, /*is_audio=*/true);
    }
    AddTrackEntry(*fragment, audio_is_first_track ? 1 : 0, decode_time,
                  /*is_audio=*/false);
    if (!audio_is_first_track) {
      AddTrackEntry(*fragment, 1, decode_time, /*is_audio=*/true);
    }
  } else {
    // Supports only video and audio track now.
    NOTREACHED();
  }

  fragments.emplace_back(std::move(fragment));
}

void CreateFragmentOrAddTrackIfNeeded(
    bool is_audio,
    std::vector<std::unique_ptr<Mp4MuxerDelegate::Fragment>>& fragments,
    bool is_key_frame,
    uint32_t track_index,
    uint32_t total_tracks_count,
    base::TimeTicks timestamp,
    base::TimeDelta decode_time,
    base::TimeDelta audio_max_duration) {
  // There is no guarantee of order of incoming track.
  // So, fragment creation algorithm would be
  // Case 1. Audio -> Audio: Max audio duration as new fragment.
  // Case 2. Audio -> Video: Add track only.
  // Case 3. Video -> Video: is_key_frame as new fragment.
  // Case 4. Video -> Audio: Add track only.

  // When creating new fragment, it should ensure that all the existing
  // track entries are created, for example, fragment 1 has a audio (0 index),
  // and video (1 index), and the current input track_index is 1, then it has to
  // create an entry of 0 index in addition to 1 index.
  if (fragments.empty()) {
    CreateNewFragment(fragments, track_index, timestamp, total_tracks_count,
                      decode_time, is_audio);
    return;
  }

  mp4::writable_boxes::MovieFragment& moof = fragments.back()->moof;

  const bool has_existing_same_track =
      (moof.track_fragments.size() == 1) && (track_index == 0);

  const bool has_existing_different_track =
      (moof.track_fragments.size() == 1) && (track_index != 0);

  if (is_audio) {
    CHECK(!moof.track_fragments.empty());
    if (has_existing_same_track) {
      // Case 1. Audio only.
      mp4::writable_boxes::TrackFragment& track_fragment =
          moof.track_fragments[track_index];
      if (!track_fragment.run.sample_timestamps.empty() &&
          (timestamp - track_fragment.run.sample_timestamps[0] >=
           audio_max_duration)) {
        CreateNewFragment(fragments, track_index, timestamp, total_tracks_count,
                          decode_time, is_audio);
      }
    } else if (has_existing_different_track) {
      // Case 4.
      // Convert existing video-only fragment to video+audio.
      AddTrackEntry(*fragments.back(), track_index, decode_time, is_audio);
    }
  } else {
    if (is_key_frame) {
      if (has_existing_different_track) {
        // Case 2.
        // Convert existing audio-only fragment to audio+video.
        AddTrackEntry(*fragments.back(), track_index, decode_time, is_audio);
      } else {
        // Case 3.
        CreateNewFragment(fragments, track_index, timestamp, total_tracks_count,
                          decode_time, is_audio);
      }
    } else {
      // Non key frame has dependency of key frame so the track must
      // already exists.
      CHECK_NE(moof.track_fragments[track_index].header.track_id, -1u);
    }
  }
}

void AddDataToMdat(Mp4MuxerDelegate::Fragment& fragment,
                   int track_index,
                   std::string encoded_data) {
  // The parameter sets are supplied in-band at the sync samples.
  // It is a default on encoded stream, see
  // `VideoEncoder::produce_annexb=false`.

  // Copy the data to the mdat.
  // TODO(crbug.com/1458518): We'll want to store the data as a vector of
  // encoded buffers instead of a single block so you don't have to resize
  // a giant blob of memory to hold them all. We should only have one
  // copy into the final muxed output buffer in an ideal world.
  std::vector<uint8_t>& track_data = fragment.mdat.track_data[track_index];
  size_t current_size = track_data.size();
  if (current_size + encoded_data.size() > track_data.capacity()) {
    track_data.reserve((current_size + encoded_data.size()) * 1.5);
  }

  // TODO(crbug.com/1458518): encoded stream needs to be movable container.
  track_data.resize(current_size + encoded_data.size());
  memcpy(&track_data[current_size], encoded_data.data(), encoded_data.size());
}

void CopyCreationTimeAndDuration(mp4::writable_boxes::Track& track,
                                 const mp4::writable_boxes::MovieHeader& header,
                                 base::TimeDelta track_duration) {
  track.header.creation_time = header.creation_time;
  track.header.modification_time = header.modification_time;

  track.media.header.creation_time = header.creation_time;
  track.media.header.modification_time = header.modification_time;

  // video track duration on the `tkhd` and `mdhd`.
  track.header.duration = track_duration;
  track.media.header.duration = track_duration;
}

void AddTimestampToPriorFragment(Mp4MuxerDelegate::Fragment* prior_fragment,
                                 size_t index,
                                 base::TimeTicks timestamp) {
  if (prior_fragment->moof.track_fragments.size() <= index) {
    return;
  }

  auto& prior_track_frag = prior_fragment->moof.track_fragments[index];
  if (!prior_track_frag.run.sample_timestamps.empty()) {
    prior_track_frag.run.sample_timestamps.emplace_back(timestamp);
  }
}

}  // namespace

Mp4MuxerDelegate::Mp4MuxerDelegate(
    Muxer::WriteDataCB write_callback,
    base::TimeDelta max_audio_only_fragment_duration)
    : write_callback_(std::move(write_callback)),
      max_audio_only_fragment_duration_(max_audio_only_fragment_duration) {}

Mp4MuxerDelegate::~Mp4MuxerDelegate() = default;

void Mp4MuxerDelegate::AddVideoFrame(
    const Muxer::VideoParameters& params,
    std::string encoded_data,
    absl::optional<VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  if (!video_track_index_.has_value()) {
    DVLOG(1) << __func__ << ", " << params.AsHumanReadableString();

    CHECK(codec_description.has_value());
    CHECK(is_key_frame);
    CHECK(start_video_time_.is_null());

    EnsureInitialized();
    last_video_time_ = start_video_time_ = timestamp;

    CHECK_GT(params.frame_rate, 0);
    video_frame_rate_ = params.frame_rate;

    video_track_index_ = GetNextTrackIndex();
    DVLOG(1) << __func__
             << ", video track index:" << video_track_index_.value();

    uint32_t timescale = video_frame_rate_ * kMillisecondsTimeScale;
    context_->SetVideoTrack({video_track_index_.value(), timescale});
    DVLOG(1) << __func__ << ", video track timescale:" << timescale;

    mp4::writable_boxes::Track track;
    moov_->tracks.emplace_back(std::move(track));

    mp4::writable_boxes::TrackExtends trex;
    moov_->extends.track_extends.emplace_back(std::move(trex));

    BuildVideoTrackWithKeyframe(params, encoded_data,
                                codec_description.value());
  }
  last_video_time_ = timestamp;

  BuildVideoFragment(encoded_data, is_key_frame);
}

void Mp4MuxerDelegate::BuildVideoTrackWithKeyframe(
    const Muxer::VideoParameters& params,
    std::string encoded_data,
    VideoEncoder::CodecDescription codec_description) {
  DCHECK(video_track_index_.has_value());

  // `stsd`, `avc1`, `avcC`.
  mp4::writable_boxes::SampleDescription description = {};

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  mp4::writable_boxes::VisualSampleEntry visual_entry = {};

  visual_entry.coded_size = params.visible_rect_size;

  visual_entry.compressor_name = version_info::GetProductName();

  mp4::AVCDecoderConfigurationRecord avc_config;
  bool result =
      avc_config.Parse(codec_description.data(), codec_description.size());
  DCHECK(result);

  visual_entry.avc_decoder_configuration.avc_config_record =
      std::move(avc_config);
  visual_entry.pixel_aspect_ratio = mp4::writable_boxes::PixelAspectRatioBox();
  description.visual_sample_entry = std::move(visual_entry);
#endif
  description.entry_count = 1;

  BuildTrack(*moov_, *video_track_index_, false,
             context_->GetVideoTrack().value().timescale, description);

  mp4::writable_boxes::Track& video_track = moov_->tracks[*video_track_index_];

  // `tkhd`.
  video_track.header.natural_size = params.visible_rect_size;

  // `hdlr`
  video_track.media.handler.handler_type = media::mp4::FOURCC_VIDE;
  video_track.media.handler.name = kVideoHandlerName;

  // `minf`

  // `vmhd`
  mp4::writable_boxes::VideoMediaHeader video_header = {};
  video_track.media.information.video_header = std::move(video_header);

  DVLOG(1) << __func__ << ", video track created";
}

void Mp4MuxerDelegate::BuildVideoFragment(std::string encoded_data,
                                          bool is_key_frame) {
  DCHECK(video_track_index_.has_value());
  CreateFragmentOrAddTrackIfNeeded(
      false, fragments_, is_key_frame, *video_track_index_, next_track_index_,
      last_video_time_, last_video_time_ - start_video_time_,
      max_audio_only_fragment_duration_);

  Fragment* fragment = fragments_.back().get();
  if (!fragment) {
    DVLOG(1) << __func__ << ", no valid video fragment exists";
    return;
  }

  // Add sample.
  mp4::writable_boxes::TrackFragmentRun& video_trun =
      fragment->moof.track_fragments[*video_track_index_].run;

  // Additional entries may exist in various sample vectors, such as durations,
  // hence the use of 'sample_count' to ensure an accurate count of valid
  // samples.
  video_trun.sample_count += 1;

  // Add sample size, which is required.
  video_trun.sample_sizes.emplace_back(encoded_data.size());

  // Add sample timestamp.
  if (video_trun.sample_timestamps.empty() && fragments_.size() > 1) {
    // Needs to add timestamp to the previous fragment.
    AddTimestampToPriorFragment(fragments_[fragments_.size() - 2].get(),
                                *video_track_index_, last_video_time_);
  }
  video_trun.sample_timestamps.emplace_back(last_video_time_);

  // Add sample data to the data box.
  AddDataToMdat(*fragment, *video_track_index_, encoded_data);
}

void Mp4MuxerDelegate::AddAudioFrame(
    const AudioParameters& params,
    std::string encoded_data,
    absl::optional<AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  if (!audio_track_index_.has_value()) {
    DVLOG(1) << __func__ << ", " << params.AsHumanReadableString();

    CHECK(codec_description.has_value());
    CHECK(start_audio_time_.is_null());

    EnsureInitialized();
    last_audio_time_ = start_audio_time_ = timestamp;

    CHECK(params.IsValid());
    audio_sample_rate_ = params.sample_rate();

    audio_track_index_ = GetNextTrackIndex();
    context_->SetAudioTrack({audio_track_index_.value(),
                             static_cast<uint32_t>(audio_sample_rate_)});
    mp4::writable_boxes::Track track;
    moov_->tracks.emplace_back(std::move(track));

    mp4::writable_boxes::TrackExtends trex;
    moov_->extends.track_extends.emplace_back(std::move(trex));

    BuildAudioTrack(params, encoded_data, codec_description.value());
  }
  last_audio_time_ = timestamp;

  BuildAudioFragment(encoded_data);
}

void Mp4MuxerDelegate::BuildAudioTrack(
    const AudioParameters& params,
    std::string encoded_data,
    AudioEncoder::CodecDescription codec_description) {
  DCHECK(audio_track_index_.has_value());

  // `stsd`, `mp4a`, `esds`.
  mp4::writable_boxes::SampleDescription description = {};
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  mp4::writable_boxes::AudioSampleEntry audio_entry = {};
  audio_entry.sample_rate = audio_sample_rate_;
  audio_entry.elementary_stream_descriptor.aac_codec_description =
      std::move(codec_description);
  description.audio_sample_entry = std::move(audio_entry);
#endif
  description.entry_count = 1;

  BuildTrack(*moov_, *audio_track_index_, true,
             context_->GetAudioTrack().value().timescale, description);

  mp4::writable_boxes::Track& audio_track = moov_->tracks[*audio_track_index_];

  // `hdlr`
  audio_track.media.handler.handler_type = media::mp4::FOURCC_SOUN;
  audio_track.media.handler.name = kAudioHandlerName;

  // `minf`

  // `smhd`
  mp4::writable_boxes::SoundMediaHeader sound_header = {};
  audio_track.media.information.sound_header = std::move(sound_header);
  DVLOG(1) << __func__ << ", audio track created";
}

void Mp4MuxerDelegate::BuildAudioFragment(std::string encoded_data) {
  DCHECK(audio_track_index_.has_value());
  CreateFragmentOrAddTrackIfNeeded(true, fragments_, false, *audio_track_index_,
                                   next_track_index_, last_audio_time_,
                                   last_audio_time_ - start_audio_time_,
                                   max_audio_only_fragment_duration_);

  Fragment* fragment = fragments_.back().get();
  if (!fragment) {
    DVLOG(1) << __func__ << ", no valid audio fragment exists";
    return;
  }

  // Add sample.
  mp4::writable_boxes::TrackFragmentRun& audio_trun =
      fragment->moof.track_fragments[*audio_track_index_].run;

  // Additional entries may exist in various sample vectors, such as durations,
  // hence the use of 'sample_count' to ensure an accurate count of valid
  // samples.
  audio_trun.sample_count += 1;

  // Add sample size, which is required.
  audio_trun.sample_sizes.emplace_back(encoded_data.size());

  // Add sample timestamp.
  if (audio_trun.sample_timestamps.empty() && fragments_.size() > 1) {
    // Needs to add timestamp to the previous fragment.
    AddTimestampToPriorFragment(fragments_[fragments_.size() - 2].get(),
                                *audio_track_index_, last_audio_time_);
  }
  audio_trun.sample_timestamps.emplace_back(last_audio_time_);

  // Add sample data to the data box.
  AddDataToMdat(*fragment, *audio_track_index_, encoded_data);
}

bool Mp4MuxerDelegate::Flush() {
  if (!video_track_index_.has_value() && !audio_track_index_.has_value()) {
    Reset();
    return false;
  }

  size_t written_offset = 0;
  // Build and write `FTYP` box.
  mp4::writable_boxes::FileType mp4_file_type_box;
  BuildFileTypeBox(mp4_file_type_box);
  Mp4FileTypeBoxWriter file_type_box_writer(*context_, mp4_file_type_box);
  written_offset += file_type_box_writer.WriteAndFlush();

  // Finish movie box and write.
  BuildMovieBox();

  // Log blob info.
  LogBoxInfo();

  // Write `moov` box and its children.
  Mp4MovieBoxWriter movie_box_writer(*context_, *moov_);
  written_offset += movie_box_writer.WriteAndFlush();

  // Finish fragment and write.
  if (video_track_index_.has_value()) {
    AddLastSampleTimestamp(
        *video_track_index_,
        base::Milliseconds(kMillisecondsTimeScale / video_frame_rate_));
  }

  if (audio_track_index_.has_value()) {
    int audio_frame_rate = audio_sample_rate_ / kAudioSamplesPerFrame;
    AddLastSampleTimestamp(
        *audio_track_index_,
        base::Milliseconds(kMillisecondsTimeScale / audio_frame_rate));
  }

  // Write `moof` box and its children as well as `mdat` box.
  mp4::writable_boxes::TrackFragmentRandomAccess video_track_random_access;
  for (auto& fragment : fragments_) {
    if (video_track_index_.has_value()) {
      BuildVideoTrackFragmentRandomAccess(video_track_random_access, *fragment,
                                          written_offset);
    }
    // `moof` and `mdat` should use same `BoxByteStream` as `moof`
    // has a dependency of `mdat` offset.
    Mp4MovieFragmentBoxWriter fragment_box_writer(*context_, fragment->moof);
    BoxByteStream box_byte_stream;
    fragment_box_writer.Write(box_byte_stream);

    // Write `mdat` box with `moof` boxes writer object.
    Mp4MediaDataBoxWriter mdat_box_writer(*context_, fragment->mdat);

    written_offset += mdat_box_writer.WriteAndFlush(box_byte_stream);
  }

  // Write `mfra` box as a last box for mp4 file.
  if (video_track_index_.has_value()) {
    mp4::writable_boxes::FragmentRandomAccess fragment_random_access;
    if (*video_track_index_ != 0) {
      // Add empty audio random access by its index position.
      mp4::writable_boxes::TrackFragmentRandomAccess audio_random_access;

      fragment_random_access.tracks.emplace_back(
          std::move(audio_random_access));
    }

    video_track_random_access.track_id = *video_track_index_ + 1;
    fragment_random_access.tracks.emplace_back(
        std::move(video_track_random_access));

    // Flush at requested.
    Mp4FragmentRandomAccessBoxWriter fragment_random_access_box_writer(
        *context_, fragment_random_access);
    fragment_random_access_box_writer.WriteAndFlush();
  }

  Reset();

  return true;
}

void Mp4MuxerDelegate::BuildFileTypeBox(
    mp4::writable_boxes::FileType& mp4_file_type_box) {
  mp4_file_type_box.major_brand = mp4::FOURCC_MP41;
  mp4_file_type_box.minor_version = 0;
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_ISOM);
  mp4_file_type_box.compatible_brands.emplace_back(mp4::FOURCC_AVC1);
}

void Mp4MuxerDelegate::BuildMovieBox() {
  // It will be called during Flush time.
  moov_->header.creation_time = base::Time::Now();
  moov_->header.modification_time = moov_->header.creation_time;

  // Milliseconds timescale for movie header.
  moov_->header.timescale = kMillisecondsTimeScale;

  // Use inverse video frame rate just in case when it has a single frame.
  base::TimeDelta video_track_duration = base::Seconds(0);
  if (video_track_index_.has_value()) {
    CHECK_NE(video_frame_rate_, 0);
    base::TimeTicks last_video_time_include_last_sample =
        last_video_time_ +
        base::Milliseconds(kMillisecondsTimeScale / video_frame_rate_);
    video_track_duration =
        last_video_time_include_last_sample - start_video_time_;
    CopyCreationTimeAndDuration(moov_->tracks[*video_track_index_],
                                moov_->header, video_track_duration);
  }

  // Use inverse audio sample rate just in case when it has a single frame.
  base::TimeDelta audio_track_duration = base::Seconds(0);
  if (audio_track_index_.has_value()) {
    CHECK_NE(audio_sample_rate_, 0);

    int audio_frame_rate = audio_sample_rate_ / kAudioSamplesPerFrame;
    base::TimeTicks last_audio_time_include_last_sample =
        last_audio_time_ +
        base::Milliseconds(kMillisecondsTimeScale / audio_frame_rate);
    audio_track_duration =
        last_audio_time_include_last_sample - start_audio_time_;
    CopyCreationTimeAndDuration(moov_->tracks[*audio_track_index_],
                                moov_->header, audio_track_duration);
  }

  // Update the track's duration that the longest duration on the track
  // whether it is video or audio.
  moov_->header.duration = std::max(video_track_duration, audio_track_duration);

  // next_track_id indicates a value to use for the track ID of the next
  // track to be added to this presentation.
  moov_->header.next_track_id = GetNextTrackIndex() + 1;
}

void Mp4MuxerDelegate::BuildVideoTrackFragmentRandomAccess(
    mp4::writable_boxes::TrackFragmentRandomAccess&
        fragment_random_access_box_writer,
    Fragment& fragment,
    size_t written_offset) {
  if (*video_track_index_ >= fragment.moof.track_fragments.size()) {
    // The front fragments could have only audio samples.
    return;
  }

  auto& run = fragment.moof.track_fragments[*video_track_index_].run;
  if (run.sample_count == 0) {
    // We don't add an entry if there is no sample in the fragment.
    return;
  }

  CHECK(!run.sample_timestamps.empty());
  mp4::writable_boxes::TrackFragmentRandomAccessEntry entry;
  // `time` is a duration since its target track's start until the
  // previous fragment, which is presentation time of the track.
  entry.time = run.sample_timestamps[0] - start_video_time_;
  entry.moof_offset = written_offset;
  entry.traf_number = 1;
  entry.trun_number = 1;
  entry.sample_number = 1;
  fragment_random_access_box_writer.entries.emplace_back(std::move(entry));
}

void Mp4MuxerDelegate::AddLastSampleTimestamp(int track_index,
                                              base::TimeDelta inverse_of_rate) {
  CHECK(!fragments_.empty());

  // Add last timestamp to the sample_timestamps on the last fragment.
  mp4::writable_boxes::TrackFragmentRun& last_trun =
      fragments_.back()->moof.track_fragments[track_index].run;

  if (last_trun.sample_timestamps.empty()) {
    return;
  }

  // Use duration based on the frame rate for the last duration of the
  // last fragment.
  base::TimeTicks last_timestamp_entry = last_trun.sample_timestamps.back();

  last_trun.sample_timestamps.emplace_back(last_timestamp_entry +
                                           inverse_of_rate);
}

void Mp4MuxerDelegate::EnsureInitialized() {
  if (context_) {
    return;
  }

  // `write_callback_` continue to be used even after `Reset`.
  auto output_position_tracker =
      std::make_unique<OutputPositionTracker>(write_callback_);

  context_ =
      std::make_unique<Mp4MuxerContext>(std::move(output_position_tracker));

  moov_ = std::make_unique<mp4::writable_boxes::Movie>();
}

void Mp4MuxerDelegate::Reset() {
  context_.reset();
  moov_.reset();
  fragments_.clear();

  video_track_index_.reset();
  audio_track_index_.reset();
  next_track_index_ = 0;
  start_video_time_ = base::TimeTicks();
  start_audio_time_ = base::TimeTicks();
  last_video_time_ = base::TimeTicks();
  last_audio_time_ = base::TimeTicks();
}

int Mp4MuxerDelegate::GetNextTrackIndex() {
  return next_track_index_++;
}

void Mp4MuxerDelegate::LogBoxInfo() const {
  std::ostringstream s;

  s << "movie timescale:" << moov_->header.timescale
    << ", duration in seconds:" << moov_->header.duration.InSeconds();
  if (video_track_index_.has_value()) {
    mp4::writable_boxes::Track& track = moov_->tracks[*video_track_index_];
    s << ", video track index:" << *video_track_index_
      << ", video track timescale:"
      << context_->GetVideoTrack().value().timescale
      << ", video track duration:" << track.header.duration;
  }

  if (audio_track_index_.has_value()) {
    mp4::writable_boxes::Track& track = moov_->tracks[*audio_track_index_];
    s << ", audio track index:" << *audio_track_index_
      << ", audio track timescale:"
      << context_->GetAudioTrack().value().timescale
      << ", audio track duration:" << track.header.duration;
  }
  s << ", Fragment counts:" << fragments_.size();

  DVLOG(1) << __func__ << ", " << s.str();
}

Mp4MuxerDelegate::Fragment::Fragment() = default;

}  // namespace media.
