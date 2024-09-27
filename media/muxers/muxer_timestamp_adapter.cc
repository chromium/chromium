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
}

bool MuxerTimestampAdapter::OnEncodedVideo(
    const Muxer::VideoParameters& params,
    scoped_refptr<DecoderBuffer> encoded_data,
    std::optional<media::VideoEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  TRACE_EVENT2("media", __func__, "timestamp", timestamp - base::TimeTicks(),
               "is_key_frame", encoded_data->is_key_frame());
  DVLOG(2) << __func__ << " - " << encoded_data->AsHumanReadableString()
           << " ts " << timestamp;

  has_seen_video_ = true;

  if (encoded_data->empty()) {
    DLOG(WARNING) << __func__ << ": zero size encoded frame, skipping";
    // Some encoders give sporadic zero-size data, see https://crbug.com/716451.
    return true;
  }

  // TODO(ajose): Support multiple tracks: http://crbug.com/528523
  if (has_audio_ && !has_seen_audio_) {
    DVLOG(1) << __func__ << ": delaying until audio track ready.";
    if (encoded_data->is_key_frame()) {  // Upon Key frame reception, empty
                                         // the encoded queue.
      video_frames_.clear();
    }
  }

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  video_frames_.push_back(EncodedFrame{
      {params, std::move(codec_description), std::move(encoded_data)},
      UpdateLastTimestampAndGetNext(last_video_timestamp_,
                                    timestamp_minus_paused)});
  return PartiallyFlushQueues();
}

bool MuxerTimestampAdapter::OnEncodedAudio(
    const AudioParameters& params,
    scoped_refptr<DecoderBuffer> encoded_data,
    std::optional<media::AudioEncoder::CodecDescription> codec_description,
    base::TimeTicks timestamp) {
  TRACE_EVENT1("media", __func__, "timestamp", timestamp - base::TimeTicks());
  DVLOG(2) << __func__ << " - " << encoded_data->size() << "B ts " << timestamp;

  has_seen_audio_ = true;

  // Compensate for time in pause spent before the first frame.
  auto timestamp_minus_paused = timestamp - total_time_in_pause_;
  encoded_data->set_is_key_frame(true);
  audio_frames_.push_back(EncodedFrame{
      {params, std::move(codec_description), std::move(encoded_data)},
      UpdateLastTimestampAndGetNext(last_audio_timestamp_,
                                    timestamp_minus_paused)});
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
  DCHECK(!video_frames_.empty() || !audio_frames_.empty());
  bool take_video = !video_frames_.empty() &&
                    (audio_frames_.empty() ||
                     video_frames_.front().timestamp_minus_paused <=
                         audio_frames_.front().timestamp_minus_paused);
  auto& queue = take_video ? video_frames_ : audio_frames_;
  EncodedFrame frame = std::move(queue.front());
  queue.pop_front();

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
  // TODO(crbug.com/40155764): consider auto-marking a track live-and-enabled
  // when media appears and remove this catch-all.
  base::TimeDelta relative_timestamp =
      frame.timestamp_minus_paused - first_timestamp_;
  DLOG_IF(WARNING, relative_timestamp < last_timestamp_written_)
      << "Enforced a monotonically increasing timestamp. Last written "
      << last_timestamp_written_ << " new " << relative_timestamp;
  relative_timestamp = std::max(relative_timestamp, last_timestamp_written_);
  last_timestamp_written_ = relative_timestamp;

  DCHECK(!frame.frame.data->empty());
  TRACE_EVENT2("media", __func__, "is_video", take_video, "recorded_timestamp",
               relative_timestamp);
  return muxer_->PutFrame(std::move(frame.frame), relative_timestamp);
}

base::TimeTicks MuxerTimestampAdapter::UpdateLastTimestampAndGetNext(
    std::optional<base::TimeTicks>& last_timestamp,
    base::TimeTicks timestamp) {
  if (!last_timestamp.has_value()) {
    last_timestamp = timestamp;
    return timestamp;
  }
  DVLOG(3) << __func__ << " ts " << timestamp << " last " << *last_timestamp;
  // In theory, time increases monotonically. In practice, it does not.
  // See http://crbug/618407.
  // TODO(crbug.com/40286147): consider not re-using the last timestamp for
  // MP4.
  DLOG_IF(WARNING, timestamp < last_timestamp)
      << "Encountered a non-monotonically increasing timestamp. Was: "
      << *last_timestamp << ", timestamp: " << timestamp;
  last_timestamp = std::max(*last_timestamp, timestamp);
  return *last_timestamp;
}

MuxerTimestampAdapter::EncodedFrame::EncodedFrame() = default;
MuxerTimestampAdapter::EncodedFrame::EncodedFrame(EncodedFrame&&) = default;
MuxerTimestampAdapter::EncodedFrame::EncodedFrame(
    Muxer::EncodedFrame frame,
    base::TimeTicks timestamp_minus_paused)
    : frame(std::move(frame)), timestamp_minus_paused(timestamp_minus_paused) {}
MuxerTimestampAdapter::EncodedFrame::~EncodedFrame() = default;

}  // namespace media
