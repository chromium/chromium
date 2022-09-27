// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/low_latency_video_renderer_algorithm.h"

#include <algorithm>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "media/base/media_log.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

// Default maximum  post decode queue size to trigger max queue size reduction.
constexpr int16_t kDefaultMaxPostDecodeQueueSize = 7;
// Default count of maximum number of consecutive frames to drop during a max
// queue size reduction. A value of 0 indicates to drop all the frames in the
// queue when maximum queue size is reached.
constexpr int16_t kDefaultMaxConsecutiveFramesToDrop = 0;
// Default count of consecutive rendered frames with a new frame in the queue to
// initiate a steady state reduction.
constexpr int16_t kDefaultReduceSteadyThreshold = 10;
// Vsyncs boundaries are not aligned to 16.667ms boundaries on some platforms
// due to hardware and software clock mismatch.
constexpr double kVsyncBoundaryErrorRate = 0.05;

}  // namespace

namespace features {

BASE_FEATURE(kLowLatencyVideoRendererAlgorithm,
             "LowLatencyVideoRendererAlgorithm",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features

LowLatencyVideoRendererAlgorithm::LowLatencyVideoRendererAlgorithm(
    media::MediaLog* media_log) {
  Reset();
  max_post_decode_queue_size_ = base::GetFieldTrialParamByFeatureAsInt(
      features::kLowLatencyVideoRendererAlgorithm, "max_post_decode_queue_size",
      kDefaultMaxPostDecodeQueueSize);
  max_consecutive_frames_to_drop_ = base::GetFieldTrialParamByFeatureAsInt(
      features::kLowLatencyVideoRendererAlgorithm,
      "max_consecutive_frames_to_drop", kDefaultMaxConsecutiveFramesToDrop);
  reduce_steady_state_queue_size_threshold_ =
      base::GetFieldTrialParamByFeatureAsInt(
          features::kLowLatencyVideoRendererAlgorithm,
          "reduce_steady_state_queue_size_threshold",
          kDefaultReduceSteadyThreshold);
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
  stats_.max_queue_length =
      std::max<int>(frame_queue_.size(), stats_.max_queue_length);
  // Determine how many fractional frames that should be rendered based on how
  // much time has passed since the last renderer deadline.
  double fractional_frames_to_render = 1.0;
  double vsync_error_allowed = 0.0;
  if (last_render_deadline_min_) {
    base::TimeDelta elapsed_time = deadline_min - *last_render_deadline_min_;
    // Fraction of media frame duration that is elapsed from the last vsync
    // call along with the fraction of frame duration that is unrendered from
    // the last vsync call.
    fractional_frames_to_render =
        elapsed_time.InMillisecondsF() /
            average_frame_duration().InMillisecondsF() +
        unrendered_fractional_frames_;

    // Different platformms follow different modes of vsync callbacks. Windows
    // and Chrome OS VideoFrameSubmitter::BeginFrame are based on hardware
    // callbacks. MacOS delivers consistent deadlines on major scenarios except
    // when vsync callbacks are missed.
    base::TimeDelta render_time_length = deadline_max - deadline_min;
    // VSync errors are added to calculate the renderer timestamp boundaries
    // only. This number is not the part of unrendered frame calculation, which
    // is carried forward to the next Render() call for vsync boundary
    // calculation.
    vsync_error_allowed =
        kVsyncBoundaryErrorRate * (render_time_length.InMillisecondsF() /
                                   average_frame_duration().InMillisecondsF());
  }

  // Adjusted fraction of media frame duration that should be rendered under
  // kNormal mode.
  double adjusted_fractional_frames_to_render =
      fractional_frames_to_render + vsync_error_allowed;
  // Find the number of complete frame duration (on media timeline) that should
  // be rendered for the current call.
  size_t number_of_frames_to_render = DetermineModeAndNumberOfFramesToRender(
      adjusted_fractional_frames_to_render);

  if (mode_ == Mode::kDrain) {
    // Render twice as many frames in drain mode.
    fractional_frames_to_render *= 2.0;
    adjusted_fractional_frames_to_render *= 2.0;
    stats_.drained_frames +=
        (fractional_frames_to_render - number_of_frames_to_render);
    // Recalculate the complete frame durations for the drain mode to render
    // twice as many frames.
    number_of_frames_to_render = adjusted_fractional_frames_to_render;
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
    // |number_of_frames_to_render| may be greater than
    // |fractional_frames_to_render| if the queue is full so that all frames are
    // dropped. If this happens, set |unrendered_fractional_frames_| to zero so
    // that the next available frame is rendered.
    unrendered_fractional_frames_ =
        fractional_frames_to_render >= number_of_frames_to_render
            ? fractional_frames_to_render - number_of_frames_to_render
            : 0.0;
    stats_.dropped_frames += number_of_frames_to_render - 1;
    ++stats_.render_frame;
  }

  if (last_deadline_min_stats_recorded_) {
    // Record stats for every 100 s, corresponding to roughly 6000 frames in
    // normal conditions.
    if (deadline_min - *last_deadline_min_stats_recorded_ >
        base::Seconds(100)) {
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
  DCHECK(!frame->metadata().end_of_stream);
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
    if (frame_queue_.size() > max_post_decode_queue_size_) {
      // If max_consecutive_frames_to_drop_ = 0, clear all but the last enqueued
      // frame and enter normal mode. Else, drop max_consecutive_frames_to_drop_
      // frames if max_consecutive_frames_to_drop_ is less than frame queue
      // size.
      if (max_consecutive_frames_to_drop_ != 0) {
        number_of_frames_to_render = std::min<uint16_t>(
            max_consecutive_frames_to_drop_ + 1, frame_queue_.size());
      } else {
        // Clear all but the last enqueued frame and enter normal mode.
        number_of_frames_to_render = frame_queue_.size();
      }
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
              .maximum_composition_delay_in_frames.value_or(
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
  // Reduce steady state queue if we have observed
  // `reduce_steady_state_queue_size_threshold_` count of consecutive rendered
  // frames where there was a newer frame in the queue that could have been
  // selected.
  bool reduce_steady_state_queue = false;
  // Has enough time passed so that at least one frame should be rendered?
  if (number_of_frames_to_render > 0) {
    // Is there a newer frame in the queue that could have been rendered?
    if (frame_queue_.size() >= number_of_frames_to_render + 1) {
      if (++consecutive_frames_with_back_up_ >
          reduce_steady_state_queue_size_threshold_) {
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
  base::UmaHistogramCounts100(uma_prefix + ".MaxQueueLength",
                              stats_.max_queue_length);
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
