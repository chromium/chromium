// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/muxer_timestamp_adapter.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "media/muxers/muxer.h"

namespace media {

MuxerTimestampAdapter::MuxerTimestampAdapter(std::unique_ptr<Muxer> muxer,
                                             bool has_video,
                                             bool has_audio)
    : has_video_(has_video), has_audio_(has_audio), muxer_(std::move(muxer)) {}

MuxerTimestampAdapter::~MuxerTimestampAdapter() {
  Flush();

  if (has_audio_ && !has_video_) {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioOnly.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioOnly.Audio",
        did_adjust_audio_timestamp_);
  } else if (!has_audio_ && has_video_) {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.VideoOnly.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.VideoOnly.Video",
        did_adjust_video_timestamp_);
  } else {
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Muxer",
        did_adjust_muxer_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Audio",
        did_adjust_audio_timestamp_);
    base::UmaHistogramBoolean(
        "Media.WebmMuxer.DidAdjustTimestamp.AudioVideo.Video",
        did_adjust_video_timestamp_);
  }
}

bool MuxerTimestampAdapter::OnEncodedVideo(
    const Muxer::VideoParameters& params,
    std::string encoded_data,
    std::string encoded_alpha,
    absl::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp,
    bool is_key_frame) {
  TRACE_EVENT2("media", __func__, "timestamp", timestamp - base::TimeTicks(),
               "is_key_frame", is_key_frame);
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B ts " << timestamp;

  has_seen_video_ = true;

  if (encoded_data.size() == 0u) {
    DLOG(WARNING) << __func__ << ": zero size encoded frame, skipping";
    // Some encoders give sporadic zero-size data, see https://crbug.com/716451.
    return true;
  }

  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_audio_ && !has_seen_audio_) {
    DVLOG(1) << __func__ << ": delaying until audio track ready.";
    if (is_key_frame) {  // Upon Key frame reception, empty the encoded queue.
      video_frames_.clear();
    }
  }

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  if (!video_timestamp_source_.has_value()) {
    video_timestamp_source_.emplace(timestamp_minus_paused,
                                    did_adjust_video_timestamp_);
  }
  video_frames_.push_back(EncodedFrame{
      {params, std::move(codec_description), std::move(encoded_data),
       std::move(encoded_alpha), is_key_frame},
      video_timestamp_source_->UpdateAndGetNext(timestamp_minus_paused)});
  return PartiallyFlushQueues();
}

bool MuxerTimestampAdapter::OnEncodedAudio(
    const AudioParameters& params,
    std::string encoded_data,
    absl::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  TRACE_EVENT1("media", __func__, "timestamp", timestamp - base::TimeTicks());
  DVLOG(2) << __func__ << " - " << encoded_data.size() << "B ts " << timestamp;

  has_seen_audio_ = true;

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  if (!audio_timestamp_source_.has_value()) {
    audio_timestamp_source_.emplace(timestamp_minus_paused,
                                    did_adjust_audio_timestamp_);
  }
  audio_frames_.push_back(EncodedFrame{
      {params, std::move(codec_description), encoded_data, std::string(),
       /*is_keyframe=*/true},
      audio_timestamp_source_->UpdateAndGetNext(timestamp_minus_paused)});
  return PartiallyFlushQueues();
}

void MuxerTimestampAdapter::SetLiveAndEnabled(bool track_live_and_enabled,
                                              bool is_video) {
  bool& written_track_live_and_enabled =
      is_video ? video_track_live_and_enabled_ : audio_track_live_and_enabled_;
  if (written_track_live_and_enabled != track_live_and_enabled) {
    DVLOG(1) << __func__ << (is_video ? " video " : " audio ")
             << "track live-and-enabled changed to " << track_live_and_enabled;
  }
  written_track_live_and_enabled = track_live_and_enabled;
}

void MuxerTimestampAdapter::Pause() {
  DVLOG(1) << __func__;
  if (!elapsed_time_in_pause_) {
    elapsed_time_in_pause_.emplace();
  }
}

void MuxerTimestampAdapter::Resume() {
  DVLOG(1) << __func__;
  if (elapsed_time_in_pause_) {
    total_time_in_pause_ += elapsed_time_in_pause_->Elapsed();
    elapsed_time_in_pause_.reset();
  }
}

bool MuxerTimestampAdapter::Flush() {
  FlushQueues();
  return muxer_->Flush();
}

void MuxerTimestampAdapter::FlushQueues() {
  while ((!video_frames_.empty() || !audio_frames_.empty()) &&
         FlushNextFrame()) {
  }
}

bool MuxerTimestampAdapter::PartiallyFlushQueues() {
  bool result = true;
  // We strictly sort by timestamp unless a track is not live-and-enabled. In
  // that case we relax this and allow drainage of the live-and-enabled leg.
  while ((!has_video_ || !video_frames_.empty() ||
          !video_track_live_and_enabled_) &&
         (!has_audio_ || !audio_frames_.empty() ||
          !audio_track_live_and_enabled_) &&
         result) {
    if (video_frames_.empty() && audio_frames_.empty()) {
      return true;
    }
    result = FlushNextFrame();
  }
  return result;
}

bool MuxerTimestampAdapter::FlushNextFrame() {
  base::TimeTicks min_timestamp = base::TimeTicks::Max();
  base::circular_deque<EncodedFrame>* queue = &video_frames_;
  if (!video_frames_.empty()) {
    min_timestamp = video_frames_.front().timestamp_minus_paused;
  }

  if (!audio_frames_.empty() &&
      audio_frames_.front().timestamp_minus_paused < min_timestamp) {
    queue = &audio_frames_;
  }

  EncodedFrame frame = std::move(queue->front());
  queue->pop_front();

  // Update the first timestamp if necessary so we can write relative timestamps
  // into the muxer.
  if (first_timestamp_.is_null()) {
    first_timestamp_ = frame.timestamp_minus_paused;
  }

  // The logic tracking live-and-enabled that temporarily relaxes the strict
  // timestamp sorting allows for draining a track's queue completely in the
  // presence of the other track being muted. When the muted track becomes
  // live-and-enabled again the sorting recommences. However, tracks get encoded
  // data before live-and-enabled transitions to true. This can lead to us
  // emitting non-monotonic timestamps to the muxer, which results in an error
  // return. Fix this by enforcing monotonicity by rewriting timestamps.
  // TODO(crbug.com/1145203): If this causes audio glitches in the field,
  // reconsider this solution. For example, consider auto-marking a track
  // live-and-enabled when media appears and remove this catch-all.
  base::TimeDelta relative_timestamp =
      frame.timestamp_minus_paused - first_timestamp_;
  DLOG_IF(WARNING, relative_timestamp < last_timestamp_written_)
      << "Enforced a monotonically increasing timestamp. Last written "
      << last_timestamp_written_ << " new " << relative_timestamp;
  did_adjust_muxer_timestamp_ |= (relative_timestamp < last_timestamp_written_);
  relative_timestamp = std::max(relative_timestamp, last_timestamp_written_);
  last_timestamp_written_ = relative_timestamp;

  DCHECK(frame.frame.data.data());
  const bool is_video_frame = queue == &video_frames_;
  TRACE_EVENT2("media", __func__, "is_video", is_video_frame,
               "recorded_timestamp", relative_timestamp.InMicroseconds());
  return muxer_->PutFrame(std::move(frame.frame), relative_timestamp);
}

MuxerTimestampAdapter::MonotonicTimestampSequence::MonotonicTimestampSequence(
    base::TimeTicks first_timestamp,
    bool& did_adjust_timestamp)
    : last_timestamp_(first_timestamp),
      did_adjust_timestamp_(did_adjust_timestamp) {}

base::TimeTicks
MuxerTimestampAdapter::MonotonicTimestampSequence::UpdateAndGetNext(
    base::TimeTicks timestamp) {
  DVLOG(3) << __func__ << " ts " << timestamp << " last " << last_timestamp_;
  // In theory, time increases monotonically. In practice, it does not.
  // See http://crbug/618407.
  // TODO(crbug.com/1494273): consider not re-using the last timestamp for MP4.
  DLOG_IF(WARNING, timestamp < last_timestamp_)
      << "Encountered a non-monotonically increasing timestamp. Was: "
      << last_timestamp_ << ", timestamp: " << timestamp;
  *did_adjust_timestamp_ |= (timestamp < last_timestamp_);
  last_timestamp_ = std::max(last_timestamp_, timestamp);
  return last_timestamp_;
}

MuxerTimestampAdapter::EncodedFrame::EncodedFrame() = default;
MuxerTimestampAdapter::EncodedFrame::EncodedFrame(EncodedFrame&&) = default;
MuxerTimestampAdapter::EncodedFrame::EncodedFrame(
    Muxer::EncodedFrame frame,
    base::TimeTicks timestamp_minus_paused)
    : frame(std::move(frame)), timestamp_minus_paused(timestamp_minus_paused) {}
MuxerTimestampAdapter::EncodedFrame::~EncodedFrame() = default;

}  // namespace media
