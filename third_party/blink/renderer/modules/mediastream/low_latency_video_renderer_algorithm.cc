// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/low_latency_video_renderer_algorithm.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
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
  if (frames_dropped) {
    *frames_dropped = 0;
  }

  stats_.accumulated_queue_length += frame_queue_.size();
  ++stats_.accumulated_queue_length_count;

  // Determine how many fractional frames that should be rendered based on how
  // much time has passed since the last renderer deadline.
  double fractional_frames_to_render = 1.0;
  if (last_render_deadline_min_) {
    base::TimeDelta elapsed_time = deadline_min - *last_render_deadline_min_;
    fractional_frames_to_render =
        elapsed_time.InMillisecondsF() /
            average_frame_duration().InMillisecondsF() +
        unrendered_fractional_frames_;
  }

  size_t number_of_frames_to_render =
      DetermineModeAndNumberOfFramesToRender(fractional_frames_to_render);

  if (mode_ == Mode::kDrain) {
    // Render twice as many frames in drain mode.
    fractional_frames_to_render *= 2.0;
    stats_.drained_frames +=
        (fractional_frames_to_render - number_of_frames_to_render);
    number_of_frames_to_render = fractional_frames_to_render;
  } else if (ReduceSteadyStateQueue(number_of_frames_to_render)) {
    // Increment counters to drop one extra frame.
    ++fractional_frames_to_render;
    ++number_of_frames_to_render;
    ++stats_.reduce_steady_state;
  }

  // Limit |number_of_frames_to_render| to a valid number. +1 in the min
  // operation to make sure that number_of_frames_to_render is not set to zero
  // unless it already was zero. |number_of_frames_to_render| > 0 signals that
  // enough time has passed so that a new frame should be rendered if possible.
  number_of_frames_to_render =
      std::min<size_t>(number_of_frames_to_render, frame_queue_.size() + 1);

  // Pop frames that should be dropped.
  for (size_t i = 1; i < number_of_frames_to_render; ++i) {
    frame_queue_.pop_front();
    if (frames_dropped) {
      ++(*frames_dropped);
    }
  }

  if (number_of_frames_to_render > 0) {
    SelectNextAvailableFrameAndUpdateLastDeadline(deadline_min);
    unrendered_fractional_frames_ =
        fractional_frames_to_render - number_of_frames_to_render;
    stats_.dropped_frames += number_of_frames_to_render - 1;
    ++stats_.render_frame;
  }

  if (last_deadline_min_stats_recorded_) {
    // Record stats for every 100 s, corresponding to roughly 6000 frames in
    // normal conditions.
    if (deadline_min - *last_deadline_min_stats_recorded_ >
        base::TimeDelta::FromSeconds(100)) {
      RecordAndResetStats();
      last_deadline_min_stats_recorded_ = deadline_min;
    }
  } else {
    last_deadline_min_stats_recorded_ = deadline_min;
  }
  return current_frame_;
}

void LowLatencyVideoRendererAlgorithm::Reset() {
  last_render_deadline_min_.reset();
  current_frame_.reset();
  frame_queue_.clear();
  mode_ = Mode::kNormal;
  unrendered_fractional_frames_ = 0;
  consecutive_frames_with_back_up_ = 0;
  stats_ = {};
}

void LowLatencyVideoRendererAlgorithm::EnqueueFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(frame);
  DCHECK(!frame->metadata()->end_of_stream);
  frame_queue_.push_back(std::move(frame));
  ++stats_.total_frames;
}

size_t LowLatencyVideoRendererAlgorithm::DetermineModeAndNumberOfFramesToRender(
    double fractional_frames_to_render) {
  // Determine number of entire frames that should be rendered and update
  // mode_.
  size_t number_of_frames_to_render = fractional_frames_to_render;
  if (number_of_frames_to_render < frame_queue_.size()) {
    // |kMaxQueueSize| is a safety mechanism that should be activated only in
    // rare circumstances. The drain mode should normally take care of high
    // queue levels. |kMaxQueueSize| should be set to the lowest possible value
    // that doesn't shortcut the drain mode. If the number of frames in the
    // queue is too high, we may run out of buffers in the HW decoder resulting
    // in a fallback to SW decoder.
    constexpr size_t kMaxQueueSize = 7;
    if (frame_queue_.size() > kMaxQueueSize) {
      // Clear all but the last enqueued frame and enter normal mode.
      number_of_frames_to_render = frame_queue_.size();
      mode_ = Mode::kNormal;
      ++stats_.max_size_drop_queue;
    } else {
      // There are several frames in the queue, determine if we should enter
      // drain mode based on queue length and the maximum composition delay that
      // is provided for the last enqueued frame.
      constexpr size_t kDefaultMaxCompositionDelayInFrames = 6;
      int max_remaining_queue_length =
          frame_queue_.back()
              ->metadata()
              ->maximum_composition_delay_in_frames.value_or(
                  kDefaultMaxCompositionDelayInFrames);

      // The number of frames in the queue is in the range
      // [number_of_frames_to_render + 1, kMaxQueueSize] due to the conditions
      // that lead up to this point. This means that the active range of
      // |max_queue_length| is [1, kMaxQueueSize].
      if (max_remaining_queue_length <
          static_cast<int>(frame_queue_.size() - number_of_frames_to_render +
                           1)) {
        mode_ = Mode::kDrain;
        ++stats_.enter_drain_mode;
      }
    }
  } else if (mode_ == Mode::kDrain) {
    // At most one frame in the queue, exit drain mode.
    mode_ = Mode::kNormal;
  }
  return number_of_frames_to_render;
}

bool LowLatencyVideoRendererAlgorithm::ReduceSteadyStateQueue(
    size_t number_of_frames_to_render) {
  // Reduce steady state queue if we have observed 10 consecutive rendered
  // frames where there was a newer frame in the queue that could have been
  // selected.
  bool reduce_steady_state_queue = false;
  constexpr int kReduceSteadyStateQueueSizeThreshold = 10;
  // Has enough time passed so that at least one frame should be rendered?
  if (number_of_frames_to_render > 0) {
    // Is there a newer frame in the queue that could have been rendered?
    if (frame_queue_.size() >= number_of_frames_to_render + 1) {
      if (++consecutive_frames_with_back_up_ >
          kReduceSteadyStateQueueSizeThreshold) {
        reduce_steady_state_queue = true;
        consecutive_frames_with_back_up_ = 0;
      }
    } else {
      consecutive_frames_with_back_up_ = 0;
    }
  }
  return reduce_steady_state_queue;
}

void LowLatencyVideoRendererAlgorithm::
    SelectNextAvailableFrameAndUpdateLastDeadline(
        base::TimeTicks deadline_min) {
  if (frame_queue_.empty()) {
    // No frame to render, reset |last_render_deadline_min_| so that the next
    // available frame is rendered immediately.
    last_render_deadline_min_.reset();
    ++stats_.no_new_frame_to_render;
  } else {
    // Select the first frame in the queue to be rendered.
    current_frame_.swap(frame_queue_.front());
    frame_queue_.pop_front();
    last_render_deadline_min_ = deadline_min;
  }
}

void LowLatencyVideoRendererAlgorithm::RecordAndResetStats() {
  // Record UMA stats for sanity check and tuning of the algorithm if needed.
  std::string uma_prefix = "Media.RtcLowLatencyVideoRenderer";
  // Total frames count.
  base::UmaHistogramCounts10000(uma_prefix + ".TotalFrames",
                                stats_.total_frames);
  if (stats_.total_frames > 0) {
    // Dropped frames per mille (=percentage scaled by 10 to get an integer
    // between 0-1000).
    base::UmaHistogramCounts1000(
        uma_prefix + ".DroppedFramesPermille",
        1000 * stats_.dropped_frames / stats_.total_frames);
    // Drained frames per mille.
    base::UmaHistogramCounts1000(
        uma_prefix + ".DrainedFramesPermille",
        1000 * stats_.drained_frames / stats_.total_frames);
  }

  // Render frame count.
  base::UmaHistogramCounts10000(uma_prefix + ".TryToRenderFrameCount",
                                stats_.render_frame);
  if (stats_.render_frame > 0) {
    // No new frame to render per mille.
    base::UmaHistogramCounts1000(
        uma_prefix + ".NoNewFrameToRenderPermille",
        1000 * stats_.no_new_frame_to_render / stats_.render_frame);
  }
  // Average queue length x 10 since this is expected to be in the range 1-3
  // frames.
  CHECK_GT(stats_.accumulated_queue_length_count, 0);
  base::UmaHistogramCounts1000(uma_prefix + ".AverageQueueLengthX10",
                               10 * stats_.accumulated_queue_length /
                                   stats_.accumulated_queue_length_count);
  // Enter drain mode count.
  base::UmaHistogramCounts10000(uma_prefix + ".EnterDrainModeCount",
                                stats_.enter_drain_mode);
  // Reduce steady state count.
  base::UmaHistogramCounts1000(uma_prefix + ".ReduceSteadyStateCount",
                               stats_.reduce_steady_state);
  // Max size drop queue count.
  base::UmaHistogramCounts1000(uma_prefix + ".MaxSizeDropQueueCount",
                               stats_.max_size_drop_queue);
  // Clear all stats.
  stats_ = {};
}
}  // namespace blink
