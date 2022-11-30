// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/animated_content_sampler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/containers/adapters.h"

namespace media {

namespace {

// These specify the minimum/maximum amount of recent event history to examine
// to detect animated content.  If the values are too low, there is a greater
// risk of false-positive detections and low accuracy.  If they are too high,
// the the implementation will be slow to lock-in/out, and also will not react
// well to mildly-variable frame rate content (e.g., 25 +/- 1 FPS).
//
// These values were established by experimenting with a wide variety of
// scenarios, including 24/25/30 FPS videos, 60 FPS WebGL demos, and the
// transitions between static and animated content.
constexpr auto kMinObservationWindow = base::Seconds(1);
constexpr auto kMaxObservationWindow = base::Seconds(2);

// The maximum amount of time that can elapse before declaring two subsequent
// events as "not animating."  This is the same value found in
// cc::FrameRateCounter.
constexpr auto kNonAnimatingThreshold = base::Seconds(1) / 4;

// The slowest that content can be animating in order for AnimatedContentSampler
// to lock-in.  This is the threshold at which the "smoothness" problem is no
// longer relevant.
constexpr auto kMaxLockInPeriod = base::Seconds(1) / 12;

// The amount of time over which to fully correct the drift of the rewritten
// frame timestamps from the presentation event timestamps.  The lower the
// value, the higher the variance in frame timestamps.
constexpr auto kDriftCorrection = base::Seconds(2);

}  // anonymous namespace

AnimatedContentSampler::AnimatedContentSampler(
    base::TimeDelta min_capture_period)
    : min_capture_period_(min_capture_period), sampling_state_(NOT_SAMPLING) {
  DCHECK_GT(min_capture_period_, base::TimeDelta());
}

AnimatedContentSampler::~AnimatedContentSampler() = default;

void AnimatedContentSampler::SetMinCapturePeriod(base::TimeDelta period) {
  DCHECK_GT(period, base::TimeDelta());
  min_capture_period_ = period;
}

void AnimatedContentSampler::SetTargetSamplingPeriod(base::TimeDelta period) {
  target_sampling_period_ = period;
}

void AnimatedContentSampler::ConsiderPresentationEvent(
    const gfx::Rect& damage_rect,
    base::TimeTicks event_time) {
  // Analyze the current event and recent history to determine whether animating
  // content is detected.
  AddObservation(damage_rect, event_time);
  if (!AnalyzeObservations(event_time, &detected_region_, &detected_period_) ||
      detected_period_ <= base::TimeDelta() ||
      detected_period_ > kMaxLockInPeriod) {
    // Animated content not detected.
    detected_region_ = gfx::Rect();
    detected_period_ = base::TimeDelta();
    sampling_state_ = NOT_SAMPLING;
    return;
  }

  // At this point, animation is being detected.  Update the sampling period
  // since the client may call the accessor method even if the heuristics below
  // decide not to sample the current event.
  sampling_period_ = ComputeSamplingPeriod(
      detected_period_, target_sampling_period_, min_capture_period_);

  // If this is the first event causing animating content to be detected,
  // transition to the START_SAMPLING state.
  if (sampling_state_ == NOT_SAMPLING)
    sampling_state_ = START_SAMPLING;

  // If the current event does not represent a frame that is part of the
  // animation, do not sample.
  if (damage_rect != detected_region_) {
    if (sampling_state_ == SHOULD_SAMPLE)
      sampling_state_ = SHOULD_NOT_SAMPLE;
    return;
  }

  // When starting sampling, determine where to sync-up for sampling and frame
  // timestamp rewriting.  Otherwise, just add one animation period's worth of
  // tokens to the token bucket.
  if (sampling_state_ == START_SAMPLING) {
    if (event_time - frame_timestamp_ > sampling_period_) {
      // The frame timestamp sequence should start with the current event
      // time.
      frame_timestamp_ = event_time - sampling_period_;
      token_bucket_ = sampling_period_;
    } else {
      // The frame timestamp sequence will continue from the last recorded
      // frame timestamp.
      token_bucket_ = event_time - frame_timestamp_;
    }

    // Provide a little extra in the initial token bucket so that minor error in
    // the detected period won't prevent a reasonably-timed event from being
    // sampled.
    token_bucket_ += detected_period_ / 2;
  } else {
    token_bucket_ += detected_period_;
  }

  // If the token bucket is full enough, take tokens from it and propose
  // sampling.  Otherwise, do not sample.
  DCHECK_LE(detected_period_, sampling_period_);
  if (token_bucket_ >= sampling_period_) {
    token_bucket_ -= sampling_period_;
    frame_timestamp_ = ComputeNextFrameTimestamp(event_time);
    sampling_state_ = SHOULD_SAMPLE;
  } else {
    sampling_state_ = SHOULD_NOT_SAMPLE;
  }
}

bool AnimatedContentSampler::HasProposal() const {
  return sampling_state_ != NOT_SAMPLING;
}

bool AnimatedContentSampler::ShouldSample() const {
  return sampling_state_ == SHOULD_SAMPLE;
}

void AnimatedContentSampler::RecordSample(base::TimeTicks frame_timestamp) {
  if (sampling_state_ == NOT_SAMPLING)
    frame_timestamp_ = frame_timestamp;
  else if (sampling_state_ == SHOULD_SAMPLE)
    sampling_state_ = SHOULD_NOT_SAMPLE;
}

void AnimatedContentSampler::AddObservation(const gfx::Rect& damage_rect,
                                            base::TimeTicks event_time) {
  if (damage_rect.IsEmpty())
    return;  // Useless observation.

  // Add the observation to the FIFO queue.
  if (!observations_.empty() && observations_.back().event_time > event_time)
    return;  // The implementation assumes chronological order.
  observations_.push_back(Observation(damage_rect, event_time));

  // Prune-out old observations.
  while ((event_time - observations_.front().event_time) >
         kMaxObservationWindow)
    observations_.pop_front();
}

gfx::Rect AnimatedContentSampler::ElectMajorityDamageRect() const {
  // This is an derivative of the Boyer-Moore Majority Vote Algorithm where each
  // pixel in a candidate gets one vote, as opposed to each candidate getting
  // one vote.
  const gfx::Rect* candidate = NULL;
  int64_t votes = 0;
  for (ObservationFifo::const_iterator i = observations_.begin();
       i != observations_.end(); ++i) {
    DCHECK_GT(i->damage_rect.size().GetArea(), 0);
    if (votes == 0) {
      candidate = &(i->damage_rect);
      votes = candidate->size().GetArea();
    } else if (i->damage_rect == *candidate) {
      votes += i->damage_rect.size().GetArea();
    } else {
      votes -= i->damage_rect.size().GetArea();
      if (votes < 0) {
        candidate = &(i->damage_rect);
        votes = -votes;
      }
    }
  }
  return (votes > 0) ? *candidate : gfx::Rect();
}

bool AnimatedContentSampler::AnalyzeObservations(
    base::TimeTicks event_time,
    gfx::Rect* rect,
    base::TimeDelta* period) const {
  const gfx::Rect elected_rect = ElectMajorityDamageRect();
  if (elected_rect.IsEmpty())
    return false;  // There is no regular animation present.

  // Scan |observations_|, gathering metrics about the ones having a damage Rect
  // equivalent to the |elected_rect|.  Along the way, break early whenever the
  // event times reveal a non-animating period.
  int64_t num_pixels_damaged_in_all = 0;
  int64_t num_pixels_damaged_in_chosen = 0;
  base::TimeDelta sum_frame_durations;
  size_t count_frame_durations = 0;
  base::TimeTicks first_event_time;
  base::TimeTicks last_event_time;
  for (const auto& observation : base::Reversed(observations_)) {
    const int area = observation.damage_rect.size().GetArea();
    num_pixels_damaged_in_all += area;
    if (observation.damage_rect != elected_rect)
      continue;
    num_pixels_damaged_in_chosen += area;
    if (last_event_time.is_null()) {
      last_event_time = observation.event_time;
      if ((event_time - last_event_time) >= kNonAnimatingThreshold) {
        return false;  // Content animation has recently ended.
      }
    } else {
      const base::TimeDelta frame_duration =
          first_event_time - observation.event_time;
      if (frame_duration >= kNonAnimatingThreshold) {
        break;  // Content not animating before this point.
      }
      sum_frame_durations += frame_duration;
      ++count_frame_durations;
    }
    first_event_time = observation.event_time;
  }

  if ((last_event_time - first_event_time) < kMinObservationWindow) {
    return false;  // Content has not animated for long enough for accuracy.
  }
  if (num_pixels_damaged_in_chosen <= (num_pixels_damaged_in_all * 2 / 3))
    return false;  // Animation is not damaging a supermajority of pixels.

  *rect = elected_rect;
  DCHECK_GT(count_frame_durations, 0u);
  *period = sum_frame_durations / count_frame_durations;
  return true;
}

base::TimeTicks AnimatedContentSampler::ComputeNextFrameTimestamp(
    base::TimeTicks event_time) const {
  // The ideal next frame timestamp one sampling period since the last one.
  const base::TimeTicks ideal_timestamp = frame_timestamp_ + sampling_period_;

  // Account for two main sources of drift: 1) The clock drift of the system
  // clock relative to the video hardware, which affects the event times; and
  // 2) The small error introduced by this frame timestamp rewriting, as it is
  // based on averaging over recent events.
  const base::TimeDelta drift = ideal_timestamp - event_time;
  const int64_t correct_over_num_frames =
      kDriftCorrection.IntDiv(sampling_period_);
  DCHECK_GT(correct_over_num_frames, 0);

  return ideal_timestamp - drift / correct_over_num_frames;
}

// static
base::TimeDelta AnimatedContentSampler::ComputeSamplingPeriod(
    base::TimeDelta animation_period,
    base::TimeDelta target_sampling_period,
    base::TimeDelta min_capture_period) {
  // If the animation rate is unknown, return the ideal sampling period.
  if (animation_period.is_zero()) {
    return std::max(target_sampling_period, min_capture_period);
  }

  // Determine whether subsampling is needed.  If so, compute the sampling
  // period corresponding to the sampling rate is the closest integer division
  // of the animation frame rate to the target sampling rate.
  //
  // For example, consider a target sampling rate of 30 FPS and an animation
  // rate of 42 FPS.  Possible sampling rates would be 42/1 = 42, 42/2 = 21,
  // 42/3 = 14, and so on.  Of these candidates, 21 FPS is closest to 30.
  base::TimeDelta sampling_period;
  if (animation_period < target_sampling_period) {
    const int64_t ratio = target_sampling_period.IntDiv(animation_period);
    const double target_fps = 1.0 / target_sampling_period.InSecondsF();
    const double animation_fps = 1.0 / animation_period.InSecondsF();
    if (std::abs(animation_fps / ratio - target_fps) <
        std::abs(animation_fps / (ratio + 1) - target_fps)) {
      sampling_period = ratio * animation_period;
    } else {
      sampling_period = (ratio + 1) * animation_period;
    }
  } else {
    sampling_period = animation_period;
  }
  return std::max(sampling_period, min_capture_period);
}

}  // namespace media
