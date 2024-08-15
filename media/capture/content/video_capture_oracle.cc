// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/content/video_capture_oracle.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace media {

namespace {

// When a non-compositor event arrives after animation has halted, this
// controls how much time must elapse before deciding to allow a capture.
constexpr auto kAnimationHaltPeriodBeforeCaptureAllowed =
    base::Milliseconds(250);

// When estimating frame durations, this is the hard upper-bound on the
// estimate.
constexpr auto kUpperBoundsDurationEstimate = base::Seconds(1);

// The half-life of data points provided to the accumulator used when evaluating
// the recent utilization of the buffer pool.  This value is based on a
// simulation, and reacts quickly to change to avoid depleting the buffer pool
// (which would cause hard frame drops).
constexpr auto kBufferUtilizationEvaluationInterval = base::Milliseconds(200);

// The half-life of data points provided to the accumulator used when evaluating
// the recent resource utilization of the consumer.  The trade-off made here is
// reaction time versus over-reacting to outlier data points.
constexpr auto kConsumerCapabilityEvaluationInterval = base::Seconds(1);

// The maximum amount of time that may elapse without a feedback update.  Any
// longer, and currently-accumulated feedback is not considered recent enough to
// base decisions off of.  This prevents changes to the capture size when there
// is an unexpected pause in events.
constexpr auto kMaxTimeSinceLastFeedbackUpdate = base::Seconds(1);

// The amount of additional time, since content animation was last detected, to
// continue being extra-careful about increasing the capture size.  This is used
// to prevent brief periods of non-animating content from throwing off the
// heuristics that decide whether to increase the capture size.
constexpr auto kDebouncingPeriodForAnimatedContent = base::Seconds(3);

// When content is animating, this is the length of time the system must be
// contiguously under-utilized before increasing the capture size.
constexpr auto kProvingPeriodForAnimatedContent = base::Seconds(30);

// Given the amount of time between frames, compare to the expected amount of
// time between frames at |frame_rate| and return the fractional difference.
double FractionFromExpectedFrameRate(base::TimeDelta delta, int frame_rate) {
  DCHECK_GT(frame_rate, 0);
  const base::TimeDelta expected_delta = base::Seconds(1) / frame_rate;
  return (delta - expected_delta) / expected_delta;
}

// Returns the next-higher TimeTicks value.
base::TimeTicks JustAfter(base::TimeTicks t) {
  return t + base::Microseconds(1);
}

}  // anonymous namespace

// static
constexpr base::TimeDelta VideoCaptureOracle::kDefaultMinCapturePeriod;

// static
constexpr base::TimeDelta VideoCaptureOracle::kDefaultMinSizeChangePeriod;

VideoCaptureOracle::VideoCaptureOracle(bool enable_auto_throttling)
    : capture_size_throttling_mode_(
          enable_auto_throttling ? kThrottlingEnabled : kThrottlingDisabled),
      min_size_change_period_(kDefaultMinSizeChangePeriod),
      next_frame_number_(0),
      last_successfully_delivered_frame_number_(-1),
      num_frames_pending_(0),
      smoothing_sampler_(kDefaultMinCapturePeriod),
      content_sampler_(kDefaultMinCapturePeriod),
      min_capture_period_(kDefaultMinCapturePeriod),
      buffer_pool_utilization_(kBufferUtilizationEvaluationInterval),
      estimated_capable_area_(kConsumerCapabilityEvaluationInterval) {
  VLOG(1) << "Capture size auto-throttling is now "
          << (enable_auto_throttling ? "enabled." : "disabled.");
}

VideoCaptureOracle::~VideoCaptureOracle() = default;

void VideoCaptureOracle::SetMinCapturePeriod(base::TimeDelta period) {
  DCHECK_GT(period, base::TimeDelta());

  min_capture_period_ = period;
  smoothing_sampler_.SetMinCapturePeriod(period);
  content_sampler_.SetMinCapturePeriod(period);
}

void VideoCaptureOracle::SetCaptureSizeConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size,
    bool use_fixed_aspect_ratio) {
  resolution_chooser_.SetConstraints(min_size, max_size,
                                     use_fixed_aspect_ratio);
}

void VideoCaptureOracle::SetAutoThrottlingEnabled(bool enabled) {
  const bool was_enabled =
      (capture_size_throttling_mode_ != kThrottlingDisabled);
  if (was_enabled == enabled)
    return;
  capture_size_throttling_mode_ =
      enabled ? kThrottlingEnabled : kThrottlingDisabled;
  VLOG(1) << "Capture size auto-throttling is now "
          << (enabled ? "enabled." : "disabled.");

  // When not auto-throttling, have the CaptureResolutionChooser target the max
  // resolution within constraints.
  if (!enabled)
    resolution_chooser_.SetTargetFrameArea(std::numeric_limits<int>::max());

  if (next_frame_number_ > 0)
    CommitCaptureSizeAndReset(GetFrameTimestamp(next_frame_number_ - 1));
}

void VideoCaptureOracle::SetSourceSize(const gfx::Size& source_size) {
  resolution_chooser_.SetSourceSize(source_size);
  // If the |resolution_chooser_| computed a new capture size, that will become
  // visible via a future call to ObserveEventAndDecideCapture().
  source_size_change_time_ = (next_frame_number_ == 0) ?
      base::TimeTicks() : GetFrameTimestamp(next_frame_number_ - 1);
}

bool VideoCaptureOracle::ObserveEventAndDecideCapture(
    Event event,
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time) {
  DCHECK_GE(event, 0);
  DCHECK_LT(event, kNumEvents);
  if (event_time < last_event_time_[event]) {
    LOG(WARNING) << "Event time is not monotonically non-decreasing.  "
                 << "Deciding not to capture this frame.";
    return false;
  }
  last_event_time_[event] = event_time;

  bool should_sample = false;
  duration_of_next_frame_ = base::TimeDelta();
  switch (event) {
    // Refresh demands get the same priority as compositor updates.
    case kRefreshDemand:
      [[fallthrough]];

    case kCompositorUpdate: {
      smoothing_sampler_.ConsiderPresentationEvent(event_time);
      const bool had_proposal = content_sampler_.HasProposal();
      content_sampler_.ConsiderPresentationEvent(damage_rect, event_time);
      if (content_sampler_.HasProposal()) {
        VLOG_IF(1, !had_proposal) << "Content sampler now detects animation.";
        should_sample = content_sampler_.ShouldSample();
        if (should_sample) {
          event_time = content_sampler_.frame_timestamp();
          duration_of_next_frame_ = content_sampler_.sampling_period();
        }
        last_time_animation_was_detected_ = event_time;
      } else {
        VLOG_IF(1, had_proposal) << "Content sampler detects animation ended.";
        should_sample = smoothing_sampler_.ShouldSample();
      }
      break;
    }

    case kRefreshRequest:
      // Only allow non-compositor samplings when content has not recently been
      // animating, and only if there are no samplings currently in progress.
      if (num_frames_pending_ == 0) {
        if (!content_sampler_.HasProposal() ||
            ((event_time - last_time_animation_was_detected_) >
             kAnimationHaltPeriodBeforeCaptureAllowed)) {
          smoothing_sampler_.ConsiderPresentationEvent(event_time);
          should_sample = smoothing_sampler_.ShouldSample();
        }
      }
      break;

    case kNumEvents:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (!should_sample)
    return false;

  // If the exact duration of the next frame has not been determined, estimate
  // it using the difference between the current and last frame.
  if (duration_of_next_frame_.is_zero()) {
    if (next_frame_number_ > 0) {
      duration_of_next_frame_ =
          event_time - GetFrameTimestamp(next_frame_number_ - 1);
    }
    duration_of_next_frame_ = std::max(
        std::min(duration_of_next_frame_, kUpperBoundsDurationEstimate),
        min_capture_period());
  }

  // Update |capture_size_| and reset all feedback signal accumulators if
  // either: 1) this is the first frame; or 2) |resolution_chooser_| has an
  // updated capture size and sufficient time has passed since the last size
  // change.
  if (next_frame_number_ == 0) {
    CommitCaptureSizeAndReset(event_time - duration_of_next_frame_);
  } else if (capture_size_ != resolution_chooser_.capture_size()) {
    const base::TimeDelta time_since_last_change =
        event_time - buffer_pool_utilization_.reset_time();
    if (time_since_last_change >= min_size_change_period_ ||
        capture_size_throttling_mode_ != kThrottlingActive) {
      // Unless autothrottling has become active resolution should be changed
      // ASAP.
      CommitCaptureSizeAndReset(GetFrameTimestamp(next_frame_number_ - 1));
    }
  }

  SetFrameTimestamp(next_frame_number_, event_time);
  return true;
}

void VideoCaptureOracle::RecordCapture(float pool_utilization) {
  DCHECK(std::isfinite(pool_utilization) && pool_utilization >= 0.0f);

  smoothing_sampler_.RecordSample();
  const base::TimeTicks timestamp = GetFrameTimestamp(next_frame_number_);
  content_sampler_.RecordSample(timestamp);

  if (capture_size_throttling_mode_ == kThrottlingActive) {
    buffer_pool_utilization_.Update(pool_utilization, timestamp);
    AnalyzeAndAdjust(timestamp);
  }

  num_frames_pending_++;
  next_frame_number_++;
}

void VideoCaptureOracle::RecordWillNotCapture(float pool_utilization) {
  VLOG(1) << "Client rejects proposal to capture frame (at #"
          << next_frame_number_ << ").";

  if (capture_size_throttling_mode_ == kThrottlingActive) {
    DCHECK(std::isfinite(pool_utilization) && pool_utilization >= 0.0f);
    const base::TimeTicks timestamp = GetFrameTimestamp(next_frame_number_);
    buffer_pool_utilization_.Update(pool_utilization, timestamp);
    AnalyzeAndAdjust(timestamp);
  }

  // Note: Do not advance |next_frame_number_| since it will be re-used for the
  // next capture proposal.
}

bool VideoCaptureOracle::CompleteCapture(int frame_number,
                                         bool capture_was_successful,
                                         base::TimeTicks* frame_timestamp) {
  num_frames_pending_--;
  DCHECK_GE(num_frames_pending_, 0);

  // Drop frame if previously delivered frame number is higher.
  if (last_successfully_delivered_frame_number_ > frame_number) {
    LOG_IF(WARNING, capture_was_successful)
        << "Out of order frame delivery detected (have #" << frame_number
        << ", last was #" << last_successfully_delivered_frame_number_
        << ").  Dropping frame.";
    return false;
  }

  if (!IsFrameInRecentHistory(frame_number)) {
    LOG(WARNING) << "Very old capture being ignored: frame #" << frame_number;
    return false;
  }

  if (!capture_was_successful) {
    VLOG(2) << "Capture of frame #" << frame_number << " was not successful.";
    return false;
  }

  DCHECK_NE(last_successfully_delivered_frame_number_, frame_number);
  last_successfully_delivered_frame_number_ = frame_number;

  *frame_timestamp = GetFrameTimestamp(frame_number);

  // If enabled, log a measurement of how this frame timestamp has incremented
  // in relation to an ideal increment.
  if (VLOG_IS_ON(3) && frame_number > 0) {
    const base::TimeDelta delta =
        *frame_timestamp - GetFrameTimestamp(frame_number - 1);
    if (content_sampler_.HasProposal()) {
      const double estimated_frame_rate =
          1000000.0 / content_sampler_.detected_period().InMicroseconds();
      const int rounded_frame_rate =
          static_cast<int>(estimated_frame_rate + 0.5);
      VLOG_STREAM(3) << base::StringPrintf(
          "Captured #%d: delta=%" PRId64
          " usec"
          ", now locked into {%s}, %+0.1f%% slower than %d FPS",
          frame_number, delta.InMicroseconds(),
          content_sampler_.detected_region().ToString().c_str(),
          100.0 * FractionFromExpectedFrameRate(delta, rounded_frame_rate),
          rounded_frame_rate);
    } else {
      VLOG_STREAM(3) << base::StringPrintf(
          "Captured #%d: delta=%" PRId64
          " usec"
          ", d/30fps=%+0.1f%%, d/25fps=%+0.1f%%, d/24fps=%+0.1f%%",
          frame_number, delta.InMicroseconds(),
          100.0 * FractionFromExpectedFrameRate(delta, 30),
          100.0 * FractionFromExpectedFrameRate(delta, 25),
          100.0 * FractionFromExpectedFrameRate(delta, 24));
    }
  }

  return true;
}

void VideoCaptureOracle::CancelAllCaptures() {
  // The following is the desired behavior:
  //
  //   for (int i = num_frames_pending_; i > 0; --i) {
  //     CompleteCapture(next_frame_number_ - i, false, nullptr);
  //     --num_frames_pending_;
  //   }
  //
  // ...which simplifies to:
  num_frames_pending_ = 0;
}

void VideoCaptureOracle::RecordConsumerFeedback(
    int frame_number,
    const media::VideoCaptureFeedback& feedback) {
  // Max frame-rate constraint.

  base::TimeDelta period;
  if (std::isfinite(feedback.max_framerate_fps) &&
      feedback.max_framerate_fps > 0.0) {
    period =
        std::max(min_capture_period_, base::Hertz(feedback.max_framerate_fps));
  } else {
    period = min_capture_period_;
  }
  smoothing_sampler_.SetMinCapturePeriod(period);
  content_sampler_.SetMinCapturePeriod(period);

  // Max pixels constraint. Only respected if auto-throttling is off because
  // consumers could just rescale the image.

  if (capture_size_throttling_mode_ != kThrottlingActive) {
    int limit;
    if (feedback.max_pixels < std::numeric_limits<int>::max()) {
      // +1 so that |FindSmallerFrameSize| could return exact |max_pixels| size.
      limit = feedback.max_pixels + 1;
    } else {
      limit = std::numeric_limits<int>::max();
    }
    int area = resolution_chooser_.FindSmallerFrameSize(limit, 1).GetArea();
    resolution_chooser_.SetTargetFrameArea(area);
  }

  // resource_utilization feedback.

  if (capture_size_throttling_mode_ == kThrottlingDisabled)
    return;

  if (!std::isfinite(feedback.resource_utilization)) {
    LOG(DFATAL) << "Non-finite utilization provided by consumer for frame #"
                << frame_number << ": " << feedback.resource_utilization;
    return;
  }
  if (feedback.resource_utilization <= 0.0)
    return;  // Non-positive values are normal, meaning N/A.

  if (capture_size_throttling_mode_ != kThrottlingActive) {
    VLOG(1) << "Received consumer feedback at frame #" << frame_number
            << "; activating capture size auto-throttling.";
    capture_size_throttling_mode_ = kThrottlingActive;
  }

  if (!IsFrameInRecentHistory(frame_number)) {
    VLOG(1) << "Very old frame feedback being ignored: frame #" << frame_number;
    return;
  }
  const base::TimeTicks timestamp = GetFrameTimestamp(frame_number);

  // Translate the utilization metric to be in terms of the capable frame area
  // and update the feedback accumulators.  Research suggests utilization is at
  // most linearly proportional to area, and typically is sublinear.  Either
  // way, the end-to-end system should converge to the right place using the
  // more-conservative assumption (linear).
  const int area_at_full_utilization = base::saturated_cast<int>(
      capture_size_.GetArea() / feedback.resource_utilization);
  estimated_capable_area_.Update(area_at_full_utilization, timestamp);
}

void VideoCaptureOracle::SetMinSizeChangePeriod(base::TimeDelta period) {
  min_size_change_period_ = period;
}

gfx::Size VideoCaptureOracle::capture_size() const {
  return capture_size_;
}

// static
const char* VideoCaptureOracle::EventAsString(Event event) {
  switch (event) {
    case kCompositorUpdate:
      return "compositor";
    case kRefreshRequest:
      return "refresh";
    case kRefreshDemand:
      return "demand";
    case kNumEvents:
      break;
  }
  NOTREACHED();
}

base::TimeTicks VideoCaptureOracle::GetFrameTimestamp(int frame_number) const {
  DCHECK(IsFrameInRecentHistory(frame_number));
  return frame_timestamps_[frame_number % kMaxFrameTimestamps];
}

void VideoCaptureOracle::SetFrameTimestamp(int frame_number,
                                           base::TimeTicks timestamp) {
  DCHECK(IsFrameInRecentHistory(frame_number));
  frame_timestamps_[frame_number % kMaxFrameTimestamps] = timestamp;
}

NOINLINE bool VideoCaptureOracle::IsFrameInRecentHistory(
    int frame_number) const {
  // Adding (next_frame_number_ >= 0) helps the compiler deduce that there
  // is no possibility of overflow here. NOINLINE is also required to ensure the
  // compiler can make this deduction (some compilers fail to otherwise...).
  return (frame_number >= 0 && next_frame_number_ >= 0 &&
          frame_number <= next_frame_number_ &&
          (next_frame_number_ - frame_number) < kMaxFrameTimestamps);
}

void VideoCaptureOracle::CommitCaptureSizeAndReset(
    base::TimeTicks last_frame_time) {
  capture_size_ = resolution_chooser_.capture_size();
  VLOG(2) << "Now proposing a capture size of " << capture_size_.ToString();

  // Reset each short-term feedback accumulator with a stable-state starting
  // value.
  const base::TimeTicks ignore_before_time = JustAfter(last_frame_time);
  buffer_pool_utilization_.Reset(1.0, ignore_before_time);
  estimated_capable_area_.Reset(capture_size_.GetArea(), ignore_before_time);
}

void VideoCaptureOracle::AnalyzeAndAdjust(const base::TimeTicks analyze_time) {
  DCHECK(capture_size_throttling_mode_ == kThrottlingActive);

  const int decreased_area = AnalyzeForDecreasedArea(analyze_time);
  if (decreased_area > 0) {
    resolution_chooser_.SetTargetFrameArea(decreased_area);
    if (!emit_log_message_cb_.is_null()) {
      emit_log_message_cb_.Run(base::StringPrintf(
          "VFC: CaptureOracle - Decreasing resolution. "
          "buffer_utilization_: %lf "
          "estimated_cappable_area: %lf "
          "capture_size: %s ",
          buffer_pool_utilization_.current(), estimated_capable_area_.current(),
          capture_size_.ToString().c_str()));
    }
    return;
  }

  const int increased_area = AnalyzeForIncreasedArea(analyze_time);
  if (increased_area > 0) {
    resolution_chooser_.SetTargetFrameArea(increased_area);
    return;
  }

  // Explicitly set the target frame area to the current capture area.  This
  // cancels-out the results of a previous call to this method, where the
  // |resolution_chooser_| may have been instructed to increase or decrease the
  // capture size.  Conditions may have changed since then which indicate no
  // change should be committed (via CommitCaptureSizeAndReset()).
  resolution_chooser_.SetTargetFrameArea(capture_size_.GetArea());
}

int VideoCaptureOracle::AnalyzeForDecreasedArea(base::TimeTicks analyze_time) {
  const int current_area = capture_size_.GetArea();
  DCHECK_GT(current_area, 0);

  // Translate the recent-average buffer pool utilization to be in terms of
  // "capable number of pixels per frame," for an apples-to-apples comparison
  // below.
  int buffer_capable_area;
  if (HasSufficientRecentFeedback(buffer_pool_utilization_, analyze_time) &&
      buffer_pool_utilization_.current() > 1.0) {
    // This calculation is hand-wavy, but seems to work well in a variety of
    // situations.
    buffer_capable_area =
        static_cast<int>(current_area / buffer_pool_utilization_.current());
  } else {
    buffer_capable_area = current_area;
  }

  int consumer_capable_area;
  if (HasSufficientRecentFeedback(estimated_capable_area_, analyze_time)) {
    consumer_capable_area =
        base::saturated_cast<int>(estimated_capable_area_.current());
  } else {
    consumer_capable_area = current_area;
  }

  // If either of the "capable areas" is less than the current capture area,
  // decrease the capture area by AT LEAST one step.
  int decreased_area = -1;
  const int capable_area = std::min(buffer_capable_area, consumer_capable_area);
  if (capable_area < current_area) {
    decreased_area = std::min(
        capable_area,
        resolution_chooser_.FindSmallerFrameSize(current_area, 1).GetArea());
    VLOG_IF(2, !start_time_of_underutilization_.is_null())
        << "Contiguous period of under-utilization ends: "
           "System is suddenly over-utilized.";
    start_time_of_underutilization_ = base::TimeTicks();
    VLOG(2) << "Proposing a "
            << (100.0 * (current_area - decreased_area) / current_area)
            << "% decrease in capture area.  :-(";
  }

  // Always log the capability interpretations at verbose logging level 3.  At
  // level 2, only log when when proposing a decreased area.
  VLOG(decreased_area == -1 ? 3 : 2)
      << "Capability of pool=" << (100.0 * buffer_capable_area / current_area)
      << "%, consumer=" << (100.0 * consumer_capable_area / current_area)
      << '%';

  return decreased_area;
}

int VideoCaptureOracle::AnalyzeForIncreasedArea(base::TimeTicks analyze_time) {
  // Compute what one step up in capture size/area would be.  If the current
  // area is already at the maximum, no further analysis is necessary.
  const int current_area = capture_size_.GetArea();
  const int increased_area =
      resolution_chooser_.FindLargerFrameSize(current_area, 1).GetArea();
  if (increased_area <= current_area)
    return -1;

  // Determine whether the buffer pool could handle an increase in area.
  if (!HasSufficientRecentFeedback(buffer_pool_utilization_, analyze_time))
    return -1;
  if (buffer_pool_utilization_.current() > 0.0) {
    const int buffer_capable_area = base::saturated_cast<int>(
        current_area / buffer_pool_utilization_.current());
    if (buffer_capable_area < increased_area) {
      VLOG_IF(2, !start_time_of_underutilization_.is_null())
          << "Contiguous period of under-utilization ends: "
             "Buffer pool is no longer under-utilized.";
      start_time_of_underutilization_ = base::TimeTicks();
      return -1;  // Buffer pool is not under-utilized.
    }
  }

  // Determine whether the consumer could handle an increase in area.
  if (HasSufficientRecentFeedback(estimated_capable_area_, analyze_time)) {
    if (estimated_capable_area_.current() < increased_area) {
      VLOG_IF(2, !start_time_of_underutilization_.is_null())
          << "Contiguous period of under-utilization ends: "
             "Consumer is no longer under-utilized.";
      start_time_of_underutilization_ = base::TimeTicks();
      return -1;  // Consumer is not under-utilized.
    }
  } else if (estimated_capable_area_.update_time() ==
             estimated_capable_area_.reset_time()) {
    // The consumer does not provide any feedback.  In this case, the consumer's
    // capability isn't a consideration.
  } else {
    // Consumer is providing feedback, but hasn't reported it recently.  Just in
    // case it's stalled, don't make things worse by increasing the capture
    // area.
    return -1;
  }

  // At this point, the system is currently under-utilized.  Reset the start
  // time if the system was not under-utilized when the last analysis was made.
  if (start_time_of_underutilization_.is_null())
    start_time_of_underutilization_ = analyze_time;

  // If the under-utilization started soon after the last source size change,
  // permit an immediate increase in the capture area.  This allows the system
  // to quickly step-up to an ideal point.
  if (start_time_of_underutilization_ - source_size_change_time_ <=
      GetExplorationPeriodAfterSourceSizeChange()) {
    VLOG(2) << "Proposing a "
            << (100.0 * (increased_area - current_area) / current_area)
            << "% increase in capture area after source size change.  :-)";
    return increased_area;
  }

  // While content is animating, require a "proving period" of contiguous
  // under-utilization before increasing the capture area.  This will mitigate
  // the risk of frames getting dropped when the data volume increases.
  if ((analyze_time - last_time_animation_was_detected_) <
      kDebouncingPeriodForAnimatedContent) {
    if ((analyze_time - start_time_of_underutilization_) <
        kProvingPeriodForAnimatedContent) {
      // Content is animating but the system needs to be under-utilized for a
      // longer period of time.
      return -1;
    } else {
      // Content is animating and the system has been contiguously
      // under-utilized for a good long time.
      VLOG(2) << "Proposing a *cautious* "
              << (100.0 * (increased_area - current_area) / current_area)
              << "% increase in capture area while content is animating.  :-)";
      // Reset the "proving period."
      start_time_of_underutilization_ = base::TimeTicks();
      return increased_area;
    }
  }

  // Content is not animating, so permit an immediate increase in the capture
  // area.  This allows the system to quickly improve the quality of
  // non-animating content (frame drops are not much of a concern).
  VLOG(2) << "Proposing a "
          << (100.0 * (increased_area - current_area) / current_area)
          << "% increase in capture area for non-animating content.  :-)";
  return increased_area;
}

base::TimeDelta
VideoCaptureOracle::GetExplorationPeriodAfterSourceSizeChange() {
  return 3 * min_size_change_period_;
}

bool VideoCaptureOracle::HasSufficientRecentFeedback(
    const FeedbackSignalAccumulator<base::TimeTicks>& accumulator,
    base::TimeTicks now) {
  const base::TimeDelta amount_of_history =
      accumulator.update_time() - accumulator.reset_time();
  return (amount_of_history >= min_size_change_period_) &&
         (now - accumulator.update_time() <= kMaxTimeSinceLastFeedbackUpdate);
}

void VideoCaptureOracle::SetLogCallback(
    base::RepeatingCallback<void(const std::string&)> emit_log_cb) {
  emit_log_message_cb_ = std::move(emit_log_cb);
}

}  // namespace media
