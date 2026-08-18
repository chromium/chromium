// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker.h"

#import <algorithm>
#import <cmath>

namespace contextual_cueing {

namespace {

// Maximum exponent cap for exponential backoff calculations to prevent
// floating-point overflow.
constexpr size_t kMaxBackoffExponent = 20;

}  // namespace

#pragma mark - Config

ContextualCueingCapTracker::Config::Config() = default;
ContextualCueingCapTracker::Config::~Config() = default;
ContextualCueingCapTracker::Config::Config(const Config&) = default;
ContextualCueingCapTracker::Config&
ContextualCueingCapTracker::Config::operator=(const Config&) = default;

#pragma mark - ContextualCueingCapTracker

ContextualCueingCapTracker::ContextualCueingCapTracker()
    : ContextualCueingCapTracker(Config()) {}

ContextualCueingCapTracker::ContextualCueingCapTracker(Config config)
    : config_(config),
      global_tracker_(config.global_cap_count, config.global_duration),
      origin_trackers_(config.visited_origins_limit) {}

ContextualCueingCapTracker::~ContextualCueingCapTracker() = default;

ContextualCueingDecision ContextualCueingCapTracker::CanShowNudge(
    const GURL& url) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Bypasses all frequency capping and cooldown backoffs if explicitly disabled
  // in configuration.
  if (config_.disable_frequency_capping_and_backoff) {
    return ContextualCueingDecision::kSuccess;
  }

  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return ContextualCueingDecision::kUrlNotEligible;
  }

  if (remaining_quiet_loads_ > 0) {
    return ContextualCueingDecision::kNotEnoughPageLoadsSinceLastCue;
  }

  if (shown_backoff_end_time_ &&
      base::TimeTicks::Now() < *shown_backoff_end_time_) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastCue;
  }

  if (dismiss_backoff_end_time_ &&
      base::TimeTicks::Now() < *dismiss_backoff_end_time_) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastDismissal;
  }

  if (click_backoff_end_time_ &&
      base::TimeTicks::Now() < *click_backoff_end_time_) {
    return ContextualCueingDecision::kNotEnoughTimeSinceLastClick;
  }

  if (!global_tracker_.CanShowNudge()) {
    return ContextualCueingDecision::kTooManyCuesShownToTheUser;
  }

  if (config_.visited_origins_limit > 0) {
    url::Origin origin = url::Origin::Create(url);
    auto it = origin_trackers_.Peek(origin);
    if (it != origin_trackers_.end() && !it->second.CanShowNudge()) {
      return ContextualCueingDecision::kTooManyCuesShownToTheUserForOrigin;
    }
  }

  return ContextualCueingDecision::kSuccess;
}

void ContextualCueingCapTracker::RecordCueShown(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // An extra count (+ 1) is needed because each navigation (including the one
  // navigating to the next eligible page where a cue could be shown) decrements
  // `remaining_quiet_loads_`. Setting this to `N + 1` ensures exactly `N` quiet
  // page navigations occur before the (N + 1)-th page is permitted to show a
  // cue.
  remaining_quiet_loads_ = config_.min_page_count_between_nudges > 0
                               ? config_.min_page_count_between_nudges + 1
                               : 0;

  if (config_.min_time_between_nudges.is_zero()) {
    shown_backoff_end_time_.reset();
  } else {
    // When a cue is shown, optimistically treat it as un-interacted (ignored)
    // by applying the shown cooldown and incrementing the count. If the user
    // subsequently interacts (clicks or dismisses), the corresponding record
    // method will reset this counter and override the backoff timer.
    size_t capped_shown_count =
        std::min(consecutive_shown_without_interaction_, kMaxBackoffExponent);
    double multiplier =
        std::pow(config_.ignore_backoff_multiplier_base, capped_shown_count);
    base::TimeDelta backoff_duration =
        config_.min_time_between_nudges * multiplier;
    shown_backoff_end_time_ = base::TimeTicks::Now() + backoff_duration;
    consecutive_shown_without_interaction_++;
  }

  global_tracker_.CueingNudgeShown();

  if (config_.visited_origins_limit > 0) {
    url::Origin origin = url::Origin::Create(url);
    auto it = origin_trackers_.Get(origin);
    if (it == origin_trackers_.end()) {
      it = origin_trackers_.Put(
          origin,
          NudgeCapTracker(config_.origin_cap_count, config_.origin_duration));
    }
    it->second.CueingNudgeShown();
  }
}

void ContextualCueingCapTracker::RecordCueDismissed(
    [[maybe_unused]] const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `url` is accepted for interface consistency and potential future
  // per-origin backoff tracking, but dismissal backoff is currently global.
  // Cap the exponent to prevent float-cast overflow with large dismissal
  // counts.
  size_t capped_dismissals =
      std::min(consecutive_dismissals_, kMaxBackoffExponent);
  double multiplier =
      std::pow(config_.dismiss_backoff_multiplier_base, capped_dismissals);
  base::TimeDelta backoff_duration =
      config_.base_dismiss_backoff_time * multiplier;
  dismiss_backoff_end_time_ = base::TimeTicks::Now() + backoff_duration;
  consecutive_dismissals_++;
  consecutive_shown_without_interaction_ = 0;
  shown_backoff_end_time_.reset();
}

void ContextualCueingCapTracker::RecordCueClicked(
    [[maybe_unused]] const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `url` is accepted for interface consistency and potential future
  // per-origin backoff tracking, but click backoff is currently global.
  if (config_.click_backoff_time.is_zero()) {
    click_backoff_end_time_.reset();
  } else {
    click_backoff_end_time_ =
        base::TimeTicks::Now() + config_.click_backoff_time;
  }
  consecutive_dismissals_ = 0;
  consecutive_shown_without_interaction_ = 0;
  shown_backoff_end_time_.reset();
}

void ContextualCueingCapTracker::RecordPageNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remaining_quiet_loads_ > 0) {
    remaining_quiet_loads_--;
  }
}

std::optional<base::TimeTicks>
ContextualCueingCapTracker::GetMostRecentNudgeTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return global_tracker_.GetMostRecentNudgeTime();
}

}  // namespace contextual_cueing
