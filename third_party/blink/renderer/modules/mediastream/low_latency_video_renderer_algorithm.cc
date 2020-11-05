// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/low_latency_video_renderer_algorithm.h"

#include "media/base/media_log.h"

namespace blink {

LowLatencyVideoRendererAlgorithm::LowLatencyVideoRendererAlgorithm(
    media::MediaLog* media_log) {
  Reset();
}

LowLatencyVideoRendererAlgorithm::~LowLatencyVideoRendererAlgorithm() = default;

scoped_refptr<media::VideoFrame> LowLatencyVideoRendererAlgorithm::Render(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max,
    size_t* frames_dropped) {
  DCHECK_LE(deadline_min, deadline_max);
  // TODO(crbug.com/1138888): Handle the case where the screen refresh rate and
  // the video frame rate are not the same as well as occasional skips of
  // rendering intervals.

  if (frames_dropped) {
    *frames_dropped = 0;
  }

  if (frame_queue_.size() > 1) {
    constexpr size_t kMaxQueueSize = 30;
    if (frame_queue_.size() > kMaxQueueSize) {
      // The queue has grown too big. Clear all but the last enqueued frame and
      // enter normal mode.
      if (frames_dropped) {
        *frames_dropped += frame_queue_.size() - 1;
      }

      while (frame_queue_.size() > 1) {
        frame_queue_.pop_front();
      }
      mode_ = Mode::kNormal;
    } else {
      // There are several frames in the queue, determine if we should enter
      // drain mode based on queue length and the maximum composition delay that
      // is provided for the last enqueued frame.
      constexpr size_t kDefaultMaxCompositionDelayInFrames = 10;
      int max_queue_length = frame_queue_.back()
                                 ->metadata()
                                 ->maximum_composition_delay_in_frames.value_or(
                                     kDefaultMaxCompositionDelayInFrames);

      // The number of frames in the queue is in the range [2, kMaxQueueSize]
      // due to the conditions that lead up to this point. This means that the
      // active range of |max_queue_length| is [1, kMaxQueueSize].
      if (max_queue_length < static_cast<int>(frame_queue_.size()))
        mode_ = Mode::kDrain;

      if (mode_ == Mode::kDrain) {
        // Drop one frame if we're in drain moide.
        frame_queue_.pop_front();
        if (frames_dropped) {
          ++(*frames_dropped);
        }
      }
    }
  } else if (mode_ == Mode::kDrain) {
    // At most one frame in the queue, exit drain mode.
    mode_ = Mode::kNormal;
  }

  // Reduce steady-state queue length.
  // Drop one frame if we have observed 10 consecutive rendered frames where
  // there was a newer frame in the queue that could have been selected.
  constexpr int kReduceSteadyStateQueueSizeThreshold = 10;
  if (mode_ == Mode::kNormal && frame_queue_.size() >= 2) {
    if (++consecutive_frames_with_back_up_ >
        kReduceSteadyStateQueueSizeThreshold) {
      frame_queue_.pop_front();
      if (frames_dropped) {
        ++(*frames_dropped);
      }
      consecutive_frames_with_back_up_ = 0;
    }
  } else {
    consecutive_frames_with_back_up_ = 0;
  }

  // Select the first frame in the queue to be rendered.
  if (!frame_queue_.empty()) {
    current_frame_.swap(frame_queue_.front());
    frame_queue_.pop_front();
  }

  // Update the current render interval for subroutines.
  render_interval_ = deadline_max - deadline_min;

  return current_frame_;
}

void LowLatencyVideoRendererAlgorithm::Reset() {
  render_interval_ = base::TimeDelta();
  current_frame_.reset();
  frame_queue_.clear();
  mode_ = Mode::kNormal;
  consecutive_frames_with_back_up_ = 0;
}

void LowLatencyVideoRendererAlgorithm::EnqueueFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->metadata()->end_of_stream);
  frame_queue_.push_back(std::move(frame));
}

}  // namespace blink
