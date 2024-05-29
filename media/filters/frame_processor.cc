// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/frame_processor.h"

#include <stdint.h>
#include <memory>

#include <cstdlib>

#include "base/memory/raw_ptr.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"

namespace media {

const int kMaxDroppedPrerollWarnings = 10;
const int kMaxAudioNonKeyframeWarnings = 10;
const int kMaxNumKeyframeTimeGreaterThanDependantWarnings = 1;
const int kMaxMuxedSequenceModeWarnings = 1;
const int kMaxSkippedEmptyFrameWarnings = 5;
const int kMaxPartialDiscardWarnings = 5;
const int kMaxDroppedFrameWarnings = 10;

// Helper class to capture per-track details needed by a frame processor. Some
// of this information may be duplicated in the short-term in the associated
// ChunkDemuxerStream and SourceBufferStream for a track.
// This parallels the MSE spec each of a SourceBuffer's Track Buffers at
// http://www.w3.org/TR/media-source/#track-buffers.
class MseTrackBuffer {
 public:
  MseTrackBuffer(ChunkDemuxerStream* stream,
                 MediaLog* media_log,
                 SourceBufferParseWarningCB parse_warning_cb);

  MseTrackBuffer(const MseTrackBuffer&) = delete;
  MseTrackBuffer& operator=(const MseTrackBuffer&) = delete;

  ~MseTrackBuffer();

  // Get/set |last_decode_timestamp_|.
  DecodeTimestamp last_decode_timestamp() const {
    return last_decode_timestamp_;
  }
  void set_last_decode_timestamp(DecodeTimestamp timestamp) {
    last_decode_timestamp_ = timestamp;
  }

  // Get/set |last_frame_duration_|.
  base::TimeDelta last_frame_duration() const {
    return last_frame_duration_;
  }
  void set_last_frame_duration(base::TimeDelta duration) {
    last_frame_duration_ = duration;
  }

  // Gets |highest_presentation_timestamp_|.
  base::TimeDelta highest_presentation_timestamp() const {
    return highest_presentation_timestamp_;
  }

  // Get/set |needs_random_access_point_|.
  bool needs_random_access_point() const {
    return needs_random_access_point_;
  }
  void set_needs_random_access_point(bool needs_random_access_point) {
    needs_random_access_point_ = needs_random_access_point;
  }

  DecodeTimestamp last_processed_decode_timestamp() const {
    return last_processed_decode_timestamp_;
  }

  base::TimeDelta last_keyframe_presentation_timestamp() const {
    return last_keyframe_presentation_timestamp_;
  }

  base::TimeDelta pending_group_start_pts() const {
    return pending_group_start_pts_;
  }

  // Gets a pointer to this track's ChunkDemuxerStream.
  ChunkDemuxerStream* stream() const { return stream_; }

  // Unsets |last_decode_timestamp_|, unsets |last_frame_duration_|,
  // unsets |highest_presentation_timestamp_|, and sets
  // |needs_random_access_point_| to true.
  void Reset();

  // Unsets |highest_presentation_timestamp_|.
  void ResetHighestPresentationTimestamp();

  // If |highest_presentation_timestamp_| is unset or |timestamp| is greater
  // than |highest_presentation_timestamp_|, sets
  // |highest_presentation_timestamp_| to |timestamp|. Note that bidirectional
  // prediction between coded frames can cause |timestamp| to not be
  // monotonically increasing even though the decode timestamps are
  // monotonically increasing.
  void SetHighestPresentationTimestampIfIncreased(base::TimeDelta timestamp);

  // Adds |frame| to the end of |processed_frames_|. In some SAP-Type-2
  // conditions, may also flush any previously enqueued frames, which can fail.
  // Returns the result of such flushing, or true if no flushing was done.
  bool EnqueueProcessedFrame(scoped_refptr<StreamParserBuffer> frame);

  // Appends |processed_frames_|, if not empty, to |stream_| and clears
  // |processed_frames_|. Returns false if append failed, true otherwise.
  // |processed_frames_| is cleared in both cases.
  bool FlushProcessedFrames();

  // Signals this track buffer's stream that a coded frame group is starting
  // with |start_dts| and |start_pts|.
  void NotifyStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                    base::TimeDelta start_pts);

 private:
  // The decode timestamp of the last coded frame appended in the current coded
  // frame group. Initially kNoTimestamp, meaning "unset".
  DecodeTimestamp last_decode_timestamp_;

  // On signalling the stream of a new coded frame group start, this is reset to
  // that start decode time. Any buffers subsequently enqueued for emission to
  // the stream update this. This is managed separately from
  // |last_decode_timestamp_| because |last_processed_decode_timestamp_| is not
  // reset during Reset(), to especially be able to track the need to signal
  // coded frame group start time for muxed post-discontinuity edge cases. See
  // also FrameProcessor::ProcessFrame().
  DecodeTimestamp last_processed_decode_timestamp_;

  // On signalling the stream of a new coded frame group start, this is set to
  // the group start PTS. If the first frame for this track in the coded frame
  // group has a lower PTS, then this must be reset to that time. Once the first
  // frame for this track has been queued, this is reset to kNoTimestamp. Like
  // |last_processed_decode_timestamp_|, this is helpful for signalling an
  // updated coded frame group start time for muxed post-discontinuity edge
  // cases. See also FrameProcessor::ProcessFrame().
  base::TimeDelta pending_group_start_pts_;

  // This is kNoTimestamp if no frames have been enqueued ever or since the last
  // NotifyStartOfCodedFrameGroup() or Reset(). Otherwise, this is the most
  // recently enqueued keyframe's presentation timestamp.
  // This is used:
  // 1) to understand if the stream parser is producing random access
  //    points that are not SAP Type 1, whose support is likely going to be
  //    deprecated from MSE API pending real-world usage data, and
  // 2) (by owning FrameProcessor) to determine if it's hit a decreasing
  //    keyframe PTS sequence when buffering by PTS intervals, such that a new
  //    coded frame group needs to be signalled.
  base::TimeDelta last_keyframe_presentation_timestamp_;

  // These are used to determine if more incremental flushing is needed to
  // correctly buffer a SAP-Type-2 non-keyframe when buffering by PTS.  They are
  // updated (if necessary) in FlushProcessedFrames() and
  // NotifyStartOfCodedFrameGroup(), and they are consulted (if necessary) in
  // EnqueueProcessedFrame().
  base::TimeDelta last_signalled_group_start_pts_;
  bool have_flushed_since_last_group_start_;

  // The coded frame duration of the last coded frame appended in the current
  // coded frame group. Initially kNoTimestamp, meaning "unset".
  base::TimeDelta last_frame_duration_;

  // The highest presentation timestamp encountered in a coded frame appended
  // in the current coded frame group. Initially kNoTimestamp, meaning
  // "unset".
  base::TimeDelta highest_presentation_timestamp_;

  // Keeps track of whether the track buffer is waiting for a random access
  // point coded frame. Initially set to true to indicate that a random access
  // point coded frame is needed before anything can be added to the track
  // buffer.
  bool needs_random_access_point_;

  // Pointer to the stream associated with this track. The stream is not owned
  // by |this|.
  const raw_ptr<ChunkDemuxerStream, DanglingUntriaged> stream_;

  // Queue of processed frames that have not yet been appended to |stream_|.
  // EnqueueProcessedFrame() adds to this queue, and FlushProcessedFrames()
  // clears it.
  StreamParser::BufferQueue processed_frames_;

  // MediaLog for reporting messages and properties to debug content and engine.
  raw_ptr<MediaLog> media_log_;

  // Callback for reporting problematic conditions that are not necessarily
  // errors.
  SourceBufferParseWarningCB parse_warning_cb_;

  // Counter that limits spam to |media_log_| for MseTrackBuffer warnings.
  int num_keyframe_time_greater_than_dependant_warnings_ = 0;
};

MseTrackBuffer::MseTrackBuffer(ChunkDemuxerStream* stream,
                               MediaLog* media_log,
                               SourceBufferParseWarningCB parse_warning_cb)
    : last_decode_timestamp_(kNoDecodeTimestamp),
      pending_group_start_pts_(kNoTimestamp),
      last_keyframe_presentation_timestamp_(kNoTimestamp),
      last_signalled_group_start_pts_(kNoTimestamp),
      have_flushed_since_last_group_start_(false),
      last_frame_duration_(kNoTimestamp),
      highest_presentation_timestamp_(kNoTimestamp),
      needs_random_access_point_(true),
      stream_(stream),
      media_log_(media_log),
      parse_warning_cb_(std::move(parse_warning_cb)) {
  DCHECK(stream_);
  DCHECK(parse_warning_cb_);
}

MseTrackBuffer::~MseTrackBuffer() {
  DVLOG(2) << __func__ << "()";
}

void MseTrackBuffer::Reset() {
  DVLOG(2) << __func__ << "()";

  last_decode_timestamp_ = kNoDecodeTimestamp;
  last_frame_duration_ = kNoTimestamp;
  highest_presentation_timestamp_ = kNoTimestamp;
  needs_random_access_point_ = true;
  last_keyframe_presentation_timestamp_ = kNoTimestamp;
}

void MseTrackBuffer::ResetHighestPresentationTimestamp() {
  highest_presentation_timestamp_ = kNoTimestamp;
}

void MseTrackBuffer::SetHighestPresentationTimestampIfIncreased(
    base::TimeDelta timestamp) {
  if (highest_presentation_timestamp_ == kNoTimestamp ||
      timestamp > highest_presentation_timestamp_) {
    highest_presentation_timestamp_ = timestamp;
  }
}

bool MseTrackBuffer::EnqueueProcessedFrame(
    scoped_refptr<StreamParserBuffer> frame) {
  if (frame->is_key_frame()) {
    last_keyframe_presentation_timestamp_ = frame->timestamp();
  } else {
    DCHECK(last_keyframe_presentation_timestamp_ != kNoTimestamp);
    // This is just one case of potentially problematic GOP structures, though
    // others are more clearly disallowed in at least some of the MSE bytestream
    // specs, especially ISOBMFF. See https://crbug.com/739931 for more
    // information.
    if (frame->timestamp() < last_keyframe_presentation_timestamp_) {
      if (!num_keyframe_time_greater_than_dependant_warnings_) {
        // At most once per each track (but potentially multiple times per
        // playback, if there are more than one tracks that exhibit this
        // sequence in a playback) run the warning's callback.
        DCHECK(parse_warning_cb_);
        parse_warning_cb_.Run(
            SourceBufferParseWarning::kKeyframeTimeGreaterThanDependant);
      }

      LIMITED_MEDIA_LOG(DEBUG, media_log_,
                        num_keyframe_time_greater_than_dependant_warnings_,
                        kMaxNumKeyframeTimeGreaterThanDependantWarnings)
          << "Warning: presentation time of most recently processed random "
             "access point ("
          << last_keyframe_presentation_timestamp_
          << ") is later than the presentation time of a non-keyframe ("
          << frame->timestamp()
          << ") that depends on it. This type of random access point is not "
             "well supported by MSE; buffered range reporting may be less "
             "precise.";

      // SAP-Type-2 GOPs, by definition, contain at least one non-keyframe with
      // PTS prior to the keyframe's PTS, with DTS continuous from keyframe
      // forward to at least that non-keyframe. If such a non-keyframe overlaps
      // the end of a previously buffered GOP sufficiently (such that, say, some
      // previous GOP's non-keyframes depending on the overlapped
      // non-keyframe(s) must be dropped), then a gap might need to result. But
      // if we attempt to buffer the new GOP's keyframe through at least that
      // first non-keyframe that does such overlapping all at once, the
      // buffering mechanism doesn't expect such a discontinuity could occur
      // (failing assumptions in places like SourceBufferRange).
      //
      // To prevent such failure, we can first flush what's previously been
      // enqueued (if anything), but do this conservatively to not flush
      // unnecessarily: we suppress such a flush if this nonkeyframe's PTS is
      // still higher than the last coded frame group start time signalled for
      // this track and no flush has yet occurred for this track since then, or
      // if there has been a flush since then but this nonkeyframe's PTS is no
      // lower than the PTS of the first frame pending flush currently.
      if (!processed_frames_.empty()) {
        DCHECK(kNoTimestamp != last_signalled_group_start_pts_);

        if (!have_flushed_since_last_group_start_) {
          if (frame->timestamp() < last_signalled_group_start_pts_) {
            if (!FlushProcessedFrames())
              return false;
          }
        } else {
          if (frame->timestamp() < processed_frames_.front()->timestamp()) {
            if (!FlushProcessedFrames())
              return false;
          }
        }
      }
    }
  }

  DCHECK(pending_group_start_pts_ == kNoTimestamp ||
         pending_group_start_pts_ <= frame->timestamp());
  pending_group_start_pts_ = kNoTimestamp;
  last_processed_decode_timestamp_ = frame->GetDecodeTimestamp();
  processed_frames_.emplace_back(std::move(frame));
  return true;
}

bool MseTrackBuffer::FlushProcessedFrames() {
  if (processed_frames_.empty())
    return true;

  bool result = stream_->Append(processed_frames_);
  processed_frames_.clear();
  have_flushed_since_last_group_start_ = true;

  DVLOG_IF(3, !result) << __func__
                       << "(): Failure appending processed frames to stream";

  return result;
}

void MseTrackBuffer::NotifyStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                                  base::TimeDelta start_pts) {
  last_keyframe_presentation_timestamp_ = kNoTimestamp;
  last_processed_decode_timestamp_ = start_dts;
  pending_group_start_pts_ = start_pts;
  have_flushed_since_last_group_start_ = false;
  last_signalled_group_start_pts_ = start_pts;
  stream_->OnStartOfCodedFrameGroup(start_dts, start_pts);
}

FrameProcessor::FrameProcessor(UpdateDurationCB update_duration_cb,
                               MediaLog* media_log)
    : group_start_timestamp_(kNoTimestamp),
      update_duration_cb_(std::move(update_duration_cb)),
      media_log_(media_log) {
  DVLOG(2) << __func__ << "()";
  DCHECK(update_duration_cb_);
}

FrameProcessor::~FrameProcessor() {
  DVLOG(2) << __func__ << "()";
}

void FrameProcessor::SetParseWarningCallback(
    SourceBufferParseWarningCB parse_warning_cb) {
  DCHECK(!parse_warning_cb_);
  DCHECK(parse_warning_cb);
  parse_warning_cb_ = std::move(parse_warning_cb);
}

void FrameProcessor::SetSequenceMode(bool sequence_mode) {
  DVLOG(2) << __func__ << "(" << sequence_mode << ")";
  // Per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#widl-SourceBuffer-mode
  // Step 7: If the new mode equals "sequence", then set the group start
  // timestamp to the group end timestamp.
  if (sequence_mode) {
    DCHECK(kNoTimestamp != group_end_timestamp_);
    group_start_timestamp_ = group_end_timestamp_;
  } else if (sequence_mode_) {
    // We're switching from 'sequence' to 'segments' mode. Be safe and signal a
    // new coded frame group on the next frame emitted.
    pending_notify_all_group_start_ = true;
  }

  // Step 8: Update the attribute to new mode.
  sequence_mode_ = sequence_mode;
}

bool FrameProcessor::ProcessFrames(
    const StreamParser::BufferQueueMap& buffer_queue_map,
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    base::TimeDelta* timestamp_offset) {
  StreamParser::BufferQueue frames;
  if (!MergeBufferQueues(buffer_queue_map, &frames)) {
    MEDIA_LOG(ERROR, media_log_) << "Parsed buffers not in DTS sequence";
    return false;
  }

  DCHECK(!frames.empty());

  if (sequence_mode_ && track_buffers_.size() > 1) {
    if (!num_muxed_sequence_mode_warnings_) {
      // At most once per SourceBuffer (but potentially multiple times per
      // playback, if there are more than one SourceBuffers used this way in a
      // playback) run the warning's callback.
      DCHECK(parse_warning_cb_);
      parse_warning_cb_.Run(SourceBufferParseWarning::kMuxedSequenceMode);
    }

    LIMITED_MEDIA_LOG(DEBUG, media_log_, num_muxed_sequence_mode_warnings_,
                      kMaxMuxedSequenceModeWarnings)
        << "Warning: using MSE 'sequence' AppendMode for a SourceBuffer with "
           "multiple tracks may cause loss of track synchronization. In some "
           "cases, buffered range gaps and playback stalls can occur. It is "
           "recommended to instead use 'segments' mode for a multitrack "
           "SourceBuffer.";
  }

  // Monitor |group_end_timestamp_| to detect any cases where it decreases while
  // processing |frames| (which should all be from no more than 1 media
  // segment), to see if (outside of mediasource fuzzers) real API usage hits
  // this case frequently enough to potentially warrant MSE spec clarification
  // of the last step in the coded frame processing algorithm. The previous
  // value is not used as a baseline, since the spec would already handle that
  // case interoperably (since we may be starting the processing of frames from
  // a new media segment.) See https://crbug.com/920853 and
  // https://github.com/w3c/media-source/issues/203.
  base::TimeDelta max_group_end_timestamp = kNoTimestamp;

  // Implements the coded frame processing algorithm's outer loop for step 1.
  // Note that ProcessFrame() implements an inner loop for a single frame that
  // handles "jump to the Loop Top step to restart processing of the current
  // coded frame" per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#sourcebuffer-coded-frame-processing
  // 1. For each coded frame in the media segment run the following steps:
  for (const auto& frame : frames) {
    // Skip any 0-byte audio or video buffers, since they cannot produce any
    // valid decode output (and are rejected by FFmpeg A/V decode.)
    if (!frame->size()) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_skipped_empty_frame_warnings_,
                        kMaxSkippedEmptyFrameWarnings)
          << "Discarding empty audio or video coded frame, PTS="
          << frame->timestamp().InMicroseconds()
          << "us, DTS=" << frame->GetDecodeTimestamp().InMicroseconds() << "us";
      continue;
    }

    if (!ProcessFrame(frame, append_window_start, append_window_end,
                      timestamp_offset)) {
      FlushProcessedFrames();
      return false;
    }

    max_group_end_timestamp =
        std::max(group_end_timestamp_, max_group_end_timestamp);
  }

  if (!FlushProcessedFrames())
    return false;

  // 2. - 4. Are handled by the WebMediaPlayer / Pipeline / Media Element.

  // 5. If the media segment contains data beyond the current duration, then run
  //    the duration change algorithm with new duration set to the maximum of
  //    the current duration and the group end timestamp.
  if (max_group_end_timestamp > group_end_timestamp_) {
    // Log a parse warning. For now at least, we don't also log this to
    // media-internals.
    DCHECK(parse_warning_cb_);
    parse_warning_cb_.Run(
        SourceBufferParseWarning::kGroupEndTimestampDecreaseWithinMediaSegment);
  }
  update_duration_cb_.Run(group_end_timestamp_);

  return true;
}

void FrameProcessor::SetGroupStartTimestampIfInSequenceMode(
    base::TimeDelta timestamp_offset) {
  DVLOG(2) << __func__ << "(" << timestamp_offset.InMicroseconds() << "us)";
  DCHECK(kNoTimestamp != timestamp_offset);
  if (sequence_mode_)
    group_start_timestamp_ = timestamp_offset;

  // Changes to timestampOffset should invalidate the preroll buffer.
  audio_preroll_buffer_.reset();
}

bool FrameProcessor::AddTrack(StreamParser::TrackId id,
                              ChunkDemuxerStream* stream) {
  DVLOG(2) << __func__ << "(): id=" << id;

  MseTrackBuffer* existing_track = FindTrack(id);
  DCHECK(!existing_track);
  if (existing_track) {
    MEDIA_LOG(ERROR, media_log_) << "Failure adding track with duplicate ID "
                                 << id;
    return false;
  }

  track_buffers_[id] =
      std::make_unique<MseTrackBuffer>(stream, media_log_, parse_warning_cb_);
  return true;
}

bool FrameProcessor::UpdateTrackIds(const TrackIdChanges& track_id_changes) {
  TrackBuffersMap& old_track_buffers = track_buffers_;
  TrackBuffersMap new_track_buffers;

  for (const auto& ids : track_id_changes) {
    if (old_track_buffers.find(ids.first) == old_track_buffers.end() ||
        new_track_buffers.find(ids.second) != new_track_buffers.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Failure updating track id from "
                                   << ids.first << " to " << ids.second;
      return false;
    }
    new_track_buffers[ids.second] = std::move(old_track_buffers[ids.first]);
    CHECK_EQ(1u, old_track_buffers.erase(ids.first));
  }

  // Process remaining track buffers with unchanged ids.
  for (const auto& t : old_track_buffers) {
    if (new_track_buffers.find(t.first) != new_track_buffers.end()) {
      MEDIA_LOG(ERROR, media_log_) << "Track id " << t.first << " conflict";
      return false;
    }
    new_track_buffers[t.first] = std::move(old_track_buffers[t.first]);
  }

  std::swap(track_buffers_, new_track_buffers);
  return true;
}

void FrameProcessor::SetAllTrackBuffersNeedRandomAccessPoint() {
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->set_needs_random_access_point(true);
  }
}

void FrameProcessor::Reset() {
  DVLOG(2) << __func__ << "()";
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->Reset();
  }

  // Maintain current |pending_notify_all_group_start_| state for Reset() during
  // sequence mode. Reset it here only if in segments mode. In sequence mode,
  // the current coded frame group may be continued across Reset() operations to
  // allow the stream to coalesce what might otherwise be gaps in the buffered
  // ranges. See also the declaration for |pending_notify_all_group_start_|.
  if (!sequence_mode_) {
    pending_notify_all_group_start_ = true;
    return;
  }

  // Sequence mode
  DCHECK(kNoTimestamp != group_end_timestamp_);
  group_start_timestamp_ = group_end_timestamp_;
}

void FrameProcessor::OnPossibleAudioConfigUpdate(
    const AudioDecoderConfig& config) {
  DCHECK(config.IsValidConfig());

  // Always clear the preroll buffer when a config update is received.
  audio_preroll_buffer_.reset();

  if (config.Matches(current_audio_config_))
    return;

  current_audio_config_ = config;
  sample_duration_ =
      base::Seconds(1.0 / current_audio_config_.samples_per_second());
  has_dependent_audio_frames_ =
      current_audio_config_.profile() == AudioCodecProfile::kXHE_AAC ||
      current_audio_config_.codec() == AudioCodec::kDTSXP2;
  last_audio_pts_for_nonkeyframe_monotonicity_check_ = kNoTimestamp;
}

MseTrackBuffer* FrameProcessor::FindTrack(StreamParser::TrackId id) {
  auto itr = track_buffers_.find(id);
  if (itr == track_buffers_.end())
    return NULL;

  return itr->second.get();
}

void FrameProcessor::NotifyStartOfCodedFrameGroup(DecodeTimestamp start_dts,
                                                  base::TimeDelta start_pts) {
  DVLOG(2) << __func__ << "(dts " << start_dts.InMicroseconds() << "us, pts "
           << start_pts.InMicroseconds() << "us)";

  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    itr->second->NotifyStartOfCodedFrameGroup(start_dts, start_pts);
  }
}

bool FrameProcessor::FlushProcessedFrames() {
  DVLOG(2) << __func__ << "()";

  bool result = true;
  for (auto itr = track_buffers_.begin(); itr != track_buffers_.end(); ++itr) {
    if (!itr->second->FlushProcessedFrames())
      result = false;
  }

  return result;
}

bool FrameProcessor::HandlePartialAppendWindowTrimming(
    base::TimeDelta append_window_start,
    base::TimeDelta append_window_end,
    scoped_refptr<StreamParserBuffer> buffer) {
  DCHECK(buffer->duration() >= base::TimeDelta());
  DCHECK_EQ(DemuxerStream::AUDIO, buffer->type());
  DCHECK(has_dependent_audio_frames_ || buffer->is_key_frame());

  const base::TimeDelta frame_end_timestamp =
      buffer->timestamp() + buffer->duration();

  // If the buffer is entirely before |append_window_start|, save it as preroll
  // for the first buffer which overlaps |append_window_start|.
  if (buffer->timestamp() < append_window_start &&
      frame_end_timestamp <= append_window_start) {
    // But if the buffer is not a keyframe, do not use it for preroll, nor use
    // any previous preroll buffer for simplicity here.
    if (has_dependent_audio_frames_ && !buffer->is_key_frame()) {
      audio_preroll_buffer_.reset();
    } else {
      audio_preroll_buffer_ = std::move(buffer);
    }
    return false;
  }

  // If the buffer is entirely after |append_window_end| there's nothing to do.
  if (buffer->timestamp() >= append_window_end)
    return false;

  DCHECK(buffer->timestamp() >= append_window_start ||
         frame_end_timestamp > append_window_start);

  bool processed_buffer = false;

  // If we have a preroll buffer see if we can attach it to the first buffer
  // overlapping or after |append_window_start|.
  if (audio_preroll_buffer_) {
    // We only want to use the preroll buffer if it directly precedes (less
    // than one sample apart) the current buffer.
    const int64_t delta =
        (audio_preroll_buffer_->timestamp() +
         audio_preroll_buffer_->duration() - buffer->timestamp())
            .InMicroseconds();
    // The only value that can't be converted with std::abs.
    if (delta == std::numeric_limits<int64_t>::min()) {
      return false;
    }
    if (std::abs(delta) < sample_duration_.InMicroseconds() &&
        audio_preroll_buffer_->timestamp() <= buffer->timestamp()) {
      DVLOG(1) << "Attaching audio preroll buffer ["
               << audio_preroll_buffer_->timestamp().InMicroseconds() << "us, "
               << (audio_preroll_buffer_->timestamp() +
                   audio_preroll_buffer_->duration())
                      .InMicroseconds()
               << "us) to " << buffer->timestamp().InMicroseconds() << "us";
      buffer->SetPrerollBuffer(std::move(audio_preroll_buffer_));
      processed_buffer = true;
    } else {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_dropped_preroll_warnings_,
                        kMaxDroppedPrerollWarnings)
          << "Partial append window trimming dropping unused audio preroll "
             "buffer with PTS "
          << audio_preroll_buffer_->timestamp().InMicroseconds()
          << "us that ends too far (" << delta
          << "us) from next buffer with PTS "
          << buffer->timestamp().InMicroseconds() << "us";
      audio_preroll_buffer_.reset();
    }
  }

  // See if a partial discard can be done around |append_window_start|.
  if (buffer->timestamp() < append_window_start) {
    LIMITED_MEDIA_LOG(INFO, media_log_, num_partial_discard_warnings_,
                      kMaxPartialDiscardWarnings)
        << "Truncating audio buffer which overlaps append window start."
        << " PTS " << buffer->timestamp().InMicroseconds()
        << "us frame_end_timestamp " << frame_end_timestamp.InMicroseconds()
        << "us append_window_start " << append_window_start.InMicroseconds()
        << "us";

    // Mark the overlapping portion of the buffer for discard.
    // TODO(wolenetz): Is this correct to ignore any pre-existing discard
    // padding (e.g. WebM discard padding)? See https://crbug.com/969195.
    buffer->set_discard_padding(std::make_pair(
        append_window_start - buffer->timestamp(), base::TimeDelta()));

    // Adjust the timestamp of this buffer forward to |append_window_start| and
    // decrease the duration to compensate. Adjust DTS by the same delta as PTS
    // to help prevent spurious discontinuities when DTS > PTS.
    base::TimeDelta pts_delta = append_window_start - buffer->timestamp();
    buffer->set_timestamp(append_window_start);
    buffer->SetDecodeTimestamp(buffer->GetDecodeTimestamp() + pts_delta);
    buffer->set_duration(frame_end_timestamp - append_window_start);
    processed_buffer = true;
  }

  // See if a partial discard can be done around |append_window_end|.
  if (frame_end_timestamp > append_window_end) {
    LIMITED_MEDIA_LOG(INFO, media_log_, num_partial_discard_warnings_,
                      kMaxPartialDiscardWarnings)
        << "Truncating audio buffer which overlaps append window end."
        << " PTS " << buffer->timestamp().InMicroseconds()
        << "us frame_end_timestamp " << frame_end_timestamp.InMicroseconds()
        << "us append_window_end " << append_window_end.InMicroseconds() << "us"
        << (buffer->is_duration_estimated() ? " (frame duration is estimated)"
                                            : "");

    // Mark the overlapping portion of the buffer for discard.
    // TODO(wolenetz): Is this correct to ignore any pre-existing discard
    // padding (e.g. WebM discard padding)? See https://crbug.com/969195.
    buffer->set_discard_padding(
        std::make_pair(buffer->discard_padding().first,
                       frame_end_timestamp - append_window_end));

    // Decrease the duration of the buffer to remove the discarded portion.
    buffer->set_duration(append_window_end - buffer->timestamp());
    processed_buffer = true;
  }

  return processed_buffer;
}

bool FrameProcessor::CheckAudioPresentationOrder(
    const StreamParserBuffer& frame,
    bool track_buffer_needs_random_access_point) {
  DCHECK_EQ(DemuxerStream::AUDIO, frame.type());
  DCHECK(has_dependent_audio_frames_);
  if (frame.is_key_frame()) {
    // Audio keyframes trivially succeed here. They start a new PTS baseline for
    // the purpose of the checks in this method.
    last_audio_pts_for_nonkeyframe_monotonicity_check_ = frame.timestamp();
    return true;
  }
  if (track_buffer_needs_random_access_point) {
    // This nonkeyframe trivially succeeds here, though it will not be buffered
    // later in the caller since a keyframe is required first.
    last_audio_pts_for_nonkeyframe_monotonicity_check_ = kNoTimestamp;
    return true;
  }

  // We're not waiting for a random access point, so we must have a valid PTS
  // baseline.
  DCHECK_NE(kNoTimestamp, last_audio_pts_for_nonkeyframe_monotonicity_check_);

  if (frame.timestamp() >= last_audio_pts_for_nonkeyframe_monotonicity_check_) {
    last_audio_pts_for_nonkeyframe_monotonicity_check_ = frame.timestamp();
    return true;
  }

  last_audio_pts_for_nonkeyframe_monotonicity_check_ = kNoTimestamp;
  return false;  // Caller should fail parse in this case.
}

bool FrameProcessor::ProcessFrame(scoped_refptr<StreamParserBuffer> frame,
                                  base::TimeDelta append_window_start,
                                  base::TimeDelta append_window_end,
                                  base::TimeDelta* timestamp_offset) {
  // Implements the loop within step 1 of the coded frame processing algorithm
  // for a single input frame per June 9, 2016 MSE spec editor's draft:
  // https://rawgit.com/w3c/media-source/d8f901f22/
  //     index.html#sourcebuffer-coded-frame-processing
  while (true) {
    // 1. Loop Top:
    // Otherwise case: (See also SourceBufferState::OnNewBuffer's conditional
    // modification of timestamp_offset after frame processing returns, when
    // generate_timestamps_flag is true).
    // 1.1. Let presentation timestamp be a double precision floating point
    //      representation of the coded frame's presentation timestamp in
    //      seconds.
    // 1.2. Let decode timestamp be a double precision floating point
    //      representation of the coded frame's decode timestamp in seconds.
    // 2. Let frame duration be a double precision floating point representation
    //    of the coded frame's duration in seconds.
    // We use base::TimeDelta and DecodeTimestamp instead of double.
    base::TimeDelta presentation_timestamp = frame->timestamp();
    DecodeTimestamp decode_timestamp = frame->GetDecodeTimestamp();
    base::TimeDelta frame_duration = frame->duration();

    DVLOG(3) << __func__ << ": Processing frame Type=" << frame->type()
             << ", TrackID=" << frame->track_id()
             << ", PTS=" << presentation_timestamp.InMicroseconds()
             << "us, DTS=" << decode_timestamp.InMicroseconds()
             << "us, DUR=" << frame_duration.InMicroseconds()
             << "us, RAP=" << frame->is_key_frame();

    // Buffering, splicing, append window trimming, etc., all depend on the
    // assumption that all audio coded frames are key frames. Metadata in the
    // bytestream may not indicate that, so we need to enforce that assumption
    // here with a warning log.
    if (frame->type() == DemuxerStream::AUDIO && !has_dependent_audio_frames_ &&
        !frame->is_key_frame()) {
      LIMITED_MEDIA_LOG(DEBUG, media_log_, num_audio_non_keyframe_warnings_,
                        kMaxAudioNonKeyframeWarnings)
          << "Bytestream with audio frame PTS "
          << presentation_timestamp.InMicroseconds() << "us and DTS "
          << decode_timestamp.InMicroseconds()
          << "us indicated the frame is not a random access point (key frame). "
             "All audio frames are expected to be key frames for the current "
             "audio codec.";
      frame->set_is_key_frame(true);
    }

    // Sanity check the timestamps.
    if (presentation_timestamp == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_) << "Unknown PTS for " << frame->GetTypeName()
                                   << " frame";
      return false;
    }

    // StreamParserBuffer's GetDecodeTimestamp() shouldn't return
    // kNoDecodeTimestamp if we already found the frame's PTS was kNoTimestamp
    // and failed processing.
    DCHECK(decode_timestamp != kNoDecodeTimestamp);

    if (presentation_timestamp.is_inf()) {
      MEDIA_LOG(ERROR, media_log_)
          << "Before adjusting by timestampOffset, PTS for "
          << frame->GetTypeName()
          << " frame exceeds range allowed by implementation";
      return false;
    }

    if (decode_timestamp.is_inf()) {
      MEDIA_LOG(ERROR, media_log_)
          << "Before adjusting by timestampOffset, DTS for "
          << frame->GetTypeName()
          << " frame exceeds range allowed by implementation";
      return false;
    }

    // TODO(wolenetz): Determine whether any DTS>PTS logging is needed. See
    // http://crbug.com/354518.
    DVLOG_IF(2, decode_timestamp.ToPresentationTime() > presentation_timestamp)
        << __func__ << ": WARNING: Frame DTS("
        << decode_timestamp.InMicroseconds() << "us) > PTS("
        << presentation_timestamp.InMicroseconds()
        << "us), frame type=" << frame->GetTypeName();

    // All stream parsers should emit valid (non-negative) frame durations.
    // Note that duration of 0 can occur for at least WebM alt-ref frames.
    if (frame_duration == kNoTimestamp) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unknown duration for " << frame->GetTypeName() << " frame at PTS "
          << presentation_timestamp.InMicroseconds() << "us";
      return false;
    }

    // See also partial protections in DecoderBuffer::set_duration().
    // Using stronger CHECK here in case any of the parsers become fragile to
    // fuzzer coverage gaps when calculating buffer durations.
    CHECK(frame_duration >= base::TimeDelta() &&
          frame_duration != kInfiniteDuration);

    // 3. If mode equals "sequence" and group start timestamp is set, then run
    //    the following steps:
    if (sequence_mode_ && group_start_timestamp_ != kNoTimestamp) {
      // 3.1. Set timestampOffset equal to group start timestamp -
      //      presentation timestamp.
      if (group_start_timestamp_.is_inf()) {
        // +Infinity may be set when app sets timestampOffset. We emit error in
        // such case upon next potential use of that offset here.
        DCHECK(group_start_timestamp_ == kInfiniteDuration);
        MEDIA_LOG(ERROR, media_log_)
            << "Sequence mode timestampOffset update prevented by a group "
               "start timestamp that exceeds range allowed by implementation";
        return false;
      }

      *timestamp_offset = group_start_timestamp_ - presentation_timestamp;
      if (timestamp_offset->is_inf()) {
        MEDIA_LOG(ERROR, media_log_)
            << "Sequence mode timestampOffset update resulted in an offset "
               "that exceeds range allowed by implementation";
        return false;
      }

      DVLOG(3) << __func__ << ": updated timestampOffset is now "
               << timestamp_offset->InMicroseconds() << "us";

      // 3.2. Set group end timestamp equal to group start timestamp.
      group_end_timestamp_ = group_start_timestamp_;

      // 3.3. Set the need random access point flag on all track buffers to
      //      true.
      SetAllTrackBuffersNeedRandomAccessPoint();

      // Remember to signal a new coded frame group. Note, this may introduce
      // gaps on large jumps forwards in sequence mode.
      pending_notify_all_group_start_ = true;

      // 3.4. Unset group start timestamp.
      group_start_timestamp_ = kNoTimestamp;
    }

    // 4. If timestampOffset is not 0, then run the following steps:
    if (!timestamp_offset->is_zero()) {
      if (timestamp_offset->is_inf()) {
        // This condition might occur if the app set timestampOffset while in
        // 'segments' append mode, skipping the 'sequence' mode offset update
        // checks, above.
        MEDIA_LOG(ERROR, media_log_)
            << "timestampOffset exceeds range allowed by implementation";
        return false;
      }

      // 4.1. Add timestampOffset to the presentation timestamp.
      // Note: |frame| PTS is only updated if it survives discontinuity
      // processing.
      presentation_timestamp += *timestamp_offset;
      if (presentation_timestamp.is_inf()) {
        MEDIA_LOG(ERROR, media_log_)
            << "After adjusting by timestampOffset, PTS for "
            << frame->GetTypeName()
            << " frame exceeds range allowed by implementation";
        return false;
      }

      // 4.2. Add timestampOffset to the decode timestamp.
      // Frame DTS is only updated if it survives discontinuity processing.
      decode_timestamp += *timestamp_offset;
      if (decode_timestamp.is_inf()) {
        MEDIA_LOG(ERROR, media_log_)
            << "After adjusting by timestampOffset, DTS for "
            << frame->GetTypeName()
            << " frame exceeds range allowed by implementation";
        return false;
      }
    }

    // 5. Let track buffer equal the track buffer that the coded frame will be
    //    added to.
    StreamParser::TrackId track_id = frame->track_id();
    MseTrackBuffer* track_buffer = FindTrack(track_id);
    if (!track_buffer) {
      MEDIA_LOG(ERROR, media_log_)
          << "Unknown track with type " << frame->GetTypeName()
          << ", frame processor track id " << track_id
          << ", and parser track id " << frame->track_id();
      return false;
    }
    if (frame->type() != track_buffer->stream()->type()) {
      MEDIA_LOG(ERROR, media_log_) << "Frame type " << frame->GetTypeName()
                                   << " doesn't match track buffer type "
                                   << track_buffer->stream()->type();
      return false;
    }

    // 6. If last decode timestamp for track buffer is set and decode timestamp
    //    is less than last decode timestamp
    //    OR
    //    If last decode timestamp for track buffer is set and the difference
    //    between decode timestamp and last decode timestamp is greater than 2
    //    times last frame duration:
    DecodeTimestamp track_last_decode_timestamp =
        track_buffer->last_decode_timestamp();
    if (track_last_decode_timestamp != kNoDecodeTimestamp) {
      base::TimeDelta track_dts_delta =
          decode_timestamp - track_last_decode_timestamp;
      if (track_dts_delta.is_negative() ||
          track_dts_delta > 2 * track_buffer->last_frame_duration()) {
        // 6.1. If mode equals "segments": Set group end timestamp to
        //      presentation timestamp.
        //      If mode equals "sequence": Set group start timestamp equal to
        //      the group end timestamp.
        if (!sequence_mode_) {
          group_end_timestamp_ = presentation_timestamp;
          // This triggers a discontinuity so we need to treat the next frames
          // appended within the append window as if they were the beginning of
          // a new coded frame group. |pending_notify_all_group_start_| is reset
          // in Reset(), below, for "segments" mode.
        } else {
          DVLOG(3) << __func__ << " : Sequence mode discontinuity, GETS: "
                   << group_end_timestamp_.InMicroseconds() << "us";
          // Reset(), below, performs the "Set group start timestamp equal to
          // the group end timestamp" operation for "sequence" mode.
        }

        // 6.2. - 6.5.:
        Reset();

        // 6.6. Jump to the Loop Top step above to restart processing of the
        //      current coded frame.
        DVLOG(3) << __func__ << ": Discontinuity: reprocessing frame";
        continue;
      }
    }

    // 7. Let frame end timestamp equal the sum of presentation timestamp and
    //    frame duration.
    base::TimeDelta frame_end_timestamp =
        presentation_timestamp + frame_duration;

    // 8.  If presentation timestamp is less than appendWindowStart, then set
    //     the need random access point flag to true, drop the coded frame, and
    //     jump to the top of the loop to start processing the next coded
    //     frame.
    // Note: We keep the result of partial discard of a buffer that overlaps
    //       |append_window_start| and does not end after |append_window_end|,
    //       for streams which support partial trimming.
    // 9. If frame end timestamp is greater than appendWindowEnd, then set the
    //    need random access point flag to true, drop the coded frame, and jump
    //    to the top of the loop to start processing the next coded frame.
    // Note: We keep the result of partial discard of a buffer that overlaps
    //       |append_window_end|, for streams which support partial trimming.
    frame->set_timestamp(presentation_timestamp);
    frame->SetDecodeTimestamp(decode_timestamp);

    if (has_dependent_audio_frames_ && frame->type() == DemuxerStream::AUDIO &&
        !CheckAudioPresentationOrder(
            *frame, track_buffer->needs_random_access_point())) {
      MEDIA_LOG(ERROR, media_log_)
          << "Dependent audio frame with invalid decreasing presentation "
             "timestamp detected.";
      return false;
    }

    // Attempt to trim audio exactly to fit the append window.
    if (frame->type() == DemuxerStream::AUDIO &&
        (frame->is_key_frame() || !track_buffer->needs_random_access_point()) &&
        HandlePartialAppendWindowTrimming(append_window_start,
                                          append_window_end, frame)) {
      // |frame| has been partially trimmed or had preroll added.  Though
      // |frame|'s duration may have changed, do not update |frame_duration|
      // here, so |track_buffer|'s last frame duration update uses original
      // frame duration and reduces spurious discontinuity detection.
      decode_timestamp = frame->GetDecodeTimestamp();
      presentation_timestamp = frame->timestamp();
      frame_end_timestamp = frame->timestamp() + frame->duration();
    }

    if (frame_end_timestamp.is_inf()) {
      MEDIA_LOG(ERROR, media_log_)
          << "Frame end timestamp for " << frame->GetTypeName()
          << " frame exceeds range allowed by implementation";
      return false;
    }

    if (presentation_timestamp < append_window_start ||
        frame_end_timestamp > append_window_end) {
      track_buffer->set_needs_random_access_point(true);

      LIMITED_MEDIA_LOG(INFO, media_log_, num_dropped_frame_warnings_,
                        kMaxDroppedFrameWarnings)
          << "Dropping " << frame->GetTypeName() << " frame (DTS "
          << decode_timestamp.InMicroseconds() << "us PTS "
          << presentation_timestamp.InMicroseconds() << "us,"
          << frame_end_timestamp.InMicroseconds()
          << "us) that is outside append window ["
          << append_window_start.InMicroseconds() << "us,"
          << append_window_end.InMicroseconds() << "us).";
      return true;
    }

    DCHECK(presentation_timestamp >= base::TimeDelta());

    // 10. If the need random access point flag on track buffer equals true,
    //     then run the following steps:
    if (track_buffer->needs_random_access_point()) {
      // 10.1. If the coded frame is not a random access point, then drop the
      //       coded frame and jump to the top of the loop to start processing
      //       the next coded frame.
      if (!frame->is_key_frame()) {
        DVLOG(3) << __func__
                 << ": Dropping frame that is not a random access point";
        return true;
      }

      // 10.2. Set the need random access point flag on track buffer to false.
      track_buffer->set_needs_random_access_point(false);
    }

    // We now have a processed buffer to append to the track buffer's stream.
    // If it is the first in a new coded frame group (such as following a
    // segments append mode discontinuity, or following a switch to segments
    // append mode from sequence append mode), notify all the track buffers
    // that a coded frame group is starting.
    bool signal_new_cfg = pending_notify_all_group_start_;

    // In muxed multi-track streams, it may occur that we already signaled a new
    // coded frame group (CFG) upon detecting a discontinuity in trackA, only to
    // now find that frames in trackB actually have an earlier timestamp. If
    // this is detected using last_processed_decode_timestamp() (which persists
    // across DTS-based discontinuity detection in sequence mode, and which
    // contains either the last processed DTS or last signalled CFG DTS for
    // trackB), re-signal trackB that a CFG is starting with its new earlier
    // DTS. Similarly, if this is detected using pending_group_start_pts()
    // (which is !kNoTimestamp only when the track hasn't yet been given the
    // first buffer in the CFG, and if so, it's the expected PTS start of that
    // CFG), re-signal trackB that a CFG is starting with its new earlier PTS.
    // Avoid re-signalling trackA, as it has already started processing frames
    // for this CFG.
    signal_new_cfg |=
        track_buffer->last_processed_decode_timestamp() > decode_timestamp ||
        (track_buffer->pending_group_start_pts() != kNoTimestamp &&
         track_buffer->pending_group_start_pts() > presentation_timestamp);

    if (frame->is_key_frame()) {
      // When a keyframe is discovered to have a decreasing PTS versus the
      // previous highest presentation timestamp for that track in the current
      // coded frame group, signal a new coded frame group for that track buffer
      // so that it can correctly process overlap-removals for the new GOP.
      if (track_buffer->highest_presentation_timestamp() != kNoTimestamp &&
          track_buffer->highest_presentation_timestamp() >
              presentation_timestamp) {
        signal_new_cfg = true;
        // In case there is currently a decreasing keyframe PTS relative to the
        // track buffer's highest PTS, that is later followed by a jump forward
        // requiring overlap removal of media prior to the track buffer's
        // highest PTS, reset that tracking now to ensure correctness of
        // signalling the need for such overlap removal later.
        track_buffer->ResetHighestPresentationTimestamp();
      }

      // When an otherwise continuous coded frame group (by DTS, and with
      // non-decreasing keyframe PTS) contains a keyframe with PTS in the future
      // significantly far enough that it may be outside of buffering fudge
      // room, signal a new coded frame group with start time set to the
      // previous highest frame end time in the coded frame group for this
      // track. This lets the stream coalesce a potential gap, and also pass
      // internal buffer adjacency checks.
      signal_new_cfg |=
          track_buffer->highest_presentation_timestamp() != kNoTimestamp &&
          track_buffer->highest_presentation_timestamp() + frame->duration() <
              presentation_timestamp;
    }

    if (signal_new_cfg) {
      DCHECK(frame->is_key_frame());

      // First, complete the append to track buffer streams of the previous
      // coded frame group's frames, if any.
      if (!FlushProcessedFrames())
        return false;

      if (pending_notify_all_group_start_) {
        NotifyStartOfCodedFrameGroup(decode_timestamp, presentation_timestamp);
        pending_notify_all_group_start_ = false;
      } else {
        DecodeTimestamp updated_dts = std::min(
            track_buffer->last_processed_decode_timestamp(), decode_timestamp);
        base::TimeDelta updated_pts = track_buffer->pending_group_start_pts();
        if (updated_pts == kNoTimestamp &&
            track_buffer->highest_presentation_timestamp() != kNoTimestamp &&
            track_buffer->highest_presentation_timestamp() <
                presentation_timestamp) {
          updated_pts = track_buffer->highest_presentation_timestamp();
        }
        if (updated_pts == kNoTimestamp || updated_pts > presentation_timestamp)
          updated_pts = presentation_timestamp;
        track_buffer->NotifyStartOfCodedFrameGroup(updated_dts, updated_pts);
      }
    }

    DVLOG(3) << __func__ << ": Enqueueing processed frame "
             << "PTS=" << presentation_timestamp.InMicroseconds()
             << "us, DTS=" << decode_timestamp.InMicroseconds() << "us";

    // Steps 11-16: Note, we optimize by appending groups of contiguous
    // processed frames for each track buffer at end of ProcessFrames() or prior
    // to signalling coded frame group starts.
    if (!track_buffer->EnqueueProcessedFrame(std::move(frame)))
      return false;

    // 17. Set last decode timestamp for track buffer to decode timestamp.
    track_buffer->set_last_decode_timestamp(decode_timestamp);

    // 18. Set last frame duration for track buffer to frame duration.
    track_buffer->set_last_frame_duration(frame_duration);

    // 19. If highest presentation timestamp for track buffer is unset or frame
    //     end timestamp is greater than highest presentation timestamp, then
    //     set highest presentation timestamp for track buffer to frame end
    //     timestamp.
    track_buffer->SetHighestPresentationTimestampIfIncreased(
        frame_end_timestamp);

    // 20. If frame end timestamp is greater than group end timestamp, then set
    //     group end timestamp equal to frame end timestamp.
    if (frame_end_timestamp > group_end_timestamp_)
      group_end_timestamp_ = frame_end_timestamp;
    DCHECK(group_end_timestamp_ >= base::TimeDelta());

    // TODO(wolenetz): Step 21 is currently approximated by predicted
    // frame_end_time by SourceBufferState::OnNewBuffers(). See
    // https://crbug.com/850316.

    return true;
  }
}

}  // namespace media
