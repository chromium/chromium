// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_adapter_video.h"

#include <stddef.h>

#include "base/logging.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

// Arbitrary decision about the frame duration when there is no previous
// hint about what could be the frame duration.
static const int kDefaultFrameDurationMs = 40;

// To calculate the frame duration, we make an assumption
// that the timestamp of the next frame in presentation order
// is no further than 5 frames away in decode order.
// TODO(damienv): the previous assumption should cover most of the practical
// cases. However, the right way to calculate the frame duration would be
// to emulate the H264 dpb bumping process.
static const size_t kHistorySize = 5;

EsAdapterVideo::EsAdapterVideo(NewVideoConfigCB new_video_config_cb,
                               EmitBufferCB emit_buffer_cb)
    : new_video_config_cb_(std::move(new_video_config_cb)),
      emit_buffer_cb_(std::move(emit_buffer_cb)),
      has_valid_config_(false),
      has_valid_frame_(false),
      last_frame_duration_(base::Milliseconds(kDefaultFrameDurationMs)),
      buffer_index_(0),
      has_valid_initial_timestamp_(false),
      discarded_frame_count_(0) {}

EsAdapterVideo::~EsAdapterVideo() {
}

void EsAdapterVideo::Flush() {
  ProcessPendingBuffers(true);
}

void EsAdapterVideo::Reset() {
  has_valid_config_ = false;
  has_valid_frame_ = false;

  last_frame_duration_ = base::Milliseconds(kDefaultFrameDurationMs);

  config_list_.clear();
  buffer_index_ = 0;
  buffer_list_.clear();
  emitted_pts_.clear();

  has_valid_initial_timestamp_ = false;
  min_pts_ = base::TimeDelta();
  min_dts_ = DecodeTimestamp();

  discarded_frame_count_ = 0;
}

void EsAdapterVideo::OnConfigChanged(
    const VideoDecoderConfig& video_decoder_config) {
  config_list_.push_back(
      ConfigEntry(buffer_index_ + buffer_list_.size(), video_decoder_config));
  has_valid_config_ = true;
  ProcessPendingBuffers(false);
}

bool EsAdapterVideo::OnNewBuffer(
    scoped_refptr<StreamParserBuffer> stream_parser_buffer) {
  if (stream_parser_buffer->timestamp() == kNoTimestamp) {
    if (has_valid_frame_) {
      // There is currently no error concealment for a missing timestamp
      // in the middle of the stream.
      DVLOG(1) << "Missing timestamp in the middle of the stream";
      return false;
    }

    if (!has_valid_initial_timestamp_) {
      // MPEG-2 TS requires the first access unit to be given a timestamp.
      // However, some streams do not comply with this requirement.
      // So simply drop the frame if it is a leading frame with no timestamp.
      DVLOG(1)
          << "Stream not compliant: ignoring leading frame with no timestamp";
      return true;
    }

    // In all the other cases, this frame will be replaced by the following
    // valid key frame, using timestamp interpolation.
    DCHECK(has_valid_initial_timestamp_);
    DCHECK_GE(discarded_frame_count_, 1);
    discarded_frame_count_++;
    return true;
  }

  // At this point, timestamps of the incoming frame are valid.
  if (!has_valid_initial_timestamp_) {
    min_pts_ = stream_parser_buffer->timestamp();
    min_dts_ = stream_parser_buffer->GetDecodeTimestamp();
    has_valid_initial_timestamp_ = true;
  }
  if (stream_parser_buffer->timestamp() < min_pts_)
    min_pts_ = stream_parser_buffer->timestamp();

  // Discard the incoming frame:
  // - if it is not associated with any config,
  // - or if no valid key frame has been found so far.
  if (!has_valid_config_ ||
      (!has_valid_frame_ && !stream_parser_buffer->is_key_frame())) {
    discarded_frame_count_++;
    return true;
  }

  has_valid_frame_ = true;

  if (discarded_frame_count_ > 0)
    ReplaceDiscardedFrames(*stream_parser_buffer);

  buffer_list_.emplace_back(std::move(stream_parser_buffer));
  ProcessPendingBuffers(false);
  return true;
}

void EsAdapterVideo::ProcessPendingBuffers(bool flush) {
  DCHECK(has_valid_config_);

  while (!buffer_list_.empty() &&
         (flush || buffer_list_.size() > kHistorySize)) {
    // Signal a config change, just before emitting the corresponding frame.
    if (!config_list_.empty() && config_list_.front().first == buffer_index_) {
      new_video_config_cb_.Run(config_list_.front().second);
      config_list_.pop_front();
    }

    scoped_refptr<StreamParserBuffer> buffer = std::move(buffer_list_.front());
    buffer_list_.pop_front();
    buffer_index_++;

    if (buffer->duration() == kNoTimestamp) {
      base::TimeDelta next_frame_pts = GetNextFramePts(buffer->timestamp());
      if (next_frame_pts == kNoTimestamp) {
        // This can happen when emitting the very last buffer
        // or if the stream do not meet the assumption behind |kHistorySize|.
        DVLOG(LOG_LEVEL_ES) << "Using last frame duration: "
                            << last_frame_duration_.InMilliseconds();
        buffer->set_duration(last_frame_duration_);
      } else {
        base::TimeDelta duration = next_frame_pts - buffer->timestamp();
        DVLOG(LOG_LEVEL_ES) << "Frame duration: " << duration.InMilliseconds();
        buffer->set_duration(duration);
      }
    }

    emitted_pts_.push_back(buffer->timestamp());
    if (emitted_pts_.size() > kHistorySize)
      emitted_pts_.pop_front();

    last_frame_duration_ = buffer->duration();
    emit_buffer_cb_.Run(std::move(buffer));
  }
}

base::TimeDelta EsAdapterVideo::GetNextFramePts(base::TimeDelta current_pts) {
  base::TimeDelta next_pts = kNoTimestamp;

  // Consider the timestamps of future frames (in decode order).
  // Note: the next frame is not enough when the GOP includes some B frames.
  for (BufferQueue::const_iterator it = buffer_list_.begin();
       it != buffer_list_.end(); ++it) {
    if ((*it)->timestamp() < current_pts)
      continue;
    if (next_pts == kNoTimestamp || next_pts > (*it)->timestamp())
      next_pts = (*it)->timestamp();
  }

  // Consider the timestamps of previous frames (in decode order).
  // In a simple GOP structure with B frames, the frame next to the last B
  // frame (in presentation order) is located before in decode order.
  for (std::list<base::TimeDelta>::const_iterator it = emitted_pts_.begin();
       it != emitted_pts_.end(); ++it) {
    if (*it < current_pts)
      continue;
    if (next_pts == kNoTimestamp || next_pts > *it)
      next_pts = *it;
  }

  return next_pts;
}

void EsAdapterVideo::ReplaceDiscardedFrames(
    const StreamParserBuffer& stream_parser_buffer) {
  DCHECK_GT(discarded_frame_count_, 0);
  DCHECK(stream_parser_buffer.is_key_frame());

  // PTS/DTS are interpolated between the min PTS/DTS of discarded frames
  // and the PTS/DTS of the first valid buffer.
  // Note: |pts_delta| and |dts_delta| are calculated using integer division.
  // Interpolation thus accumulutes small errors. However, since timestamps
  // are given in microseconds, only a high number of discarded frames
  // (in the order of 10000s) could have an impact and create a gap (from MSE
  // point of view) between the last interpolated frame and
  // |stream_parser_buffer|.
  base::TimeDelta pts = min_pts_;
  base::TimeDelta pts_delta =
      (stream_parser_buffer.timestamp() - pts) / discarded_frame_count_;
  DecodeTimestamp dts = min_dts_;
  base::TimeDelta dts_delta =
      (stream_parser_buffer.GetDecodeTimestamp() - dts) /
      discarded_frame_count_;

  for (int i = 0; i < discarded_frame_count_; i++) {
    scoped_refptr<StreamParserBuffer> frame = StreamParserBuffer::CopyFrom(
        stream_parser_buffer.data(), stream_parser_buffer.size(),
        stream_parser_buffer.is_key_frame(), stream_parser_buffer.type(),
        stream_parser_buffer.track_id());
    frame->SetDecodeTimestamp(dts);
    frame->set_timestamp(pts);
    frame->set_duration(pts_delta);
    buffer_list_.emplace_back(std::move(frame));
    pts += pts_delta;
    dts += dts_delta;
  }
  discarded_frame_count_ = 0;
}

}  // namespace mp2t
}  // namespace media
