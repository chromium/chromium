// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"

#import <algorithm>
#import <cmath>
#import <utility>

namespace contextual_cueing {

namespace {

// Maximum exponent cap for exponential backoff calculations to prevent
// floating-point overflow.
constexpr size_t kMaxBackoffExponent = 20;

}  // namespace

#pragma mark - Config

ContextualCueingCapTrackerService::Config::Config() = default;
ContextualCueingCapTrackerService::Config::~Config() = default;
ContextualCueingCapTrackerService::Config::Config(const Config&) = default;
ContextualCueingCapTrackerService::Config&
ContextualCueingCapTrackerService::Config::operator=(const Config&) = default;
ContextualCueingCapTrackerService::Config::Config(Config&&) = default;
ContextualCueingCapTrackerService::Config&
ContextualCueingCapTrackerService::Config::operator=(Config&&) = default;

#pragma mark - ContextualCueingCapTrackerService

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService()
    : ContextualCueingCapTrackerService(Config()) {}

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService(
    Config config)
    : config_(std::move(config)),
      global_tracker_(config_.global_cap_count, config_.global_duration),
      origin_trackers_(config_.visited_origins_limit) {}

ContextualCueingCapTrackerService::~ContextualCueingCapTrackerService() =
    default;

ContextualCueingDecision ContextualCueingCapTrackerService::CanShowNudge(
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

void ContextualCueingCapTrackerService::RecordCueShown(const GURL& url) {
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

  // Calculates exponential backoff for shown / ignored cues without
  // interaction.
  double multiplier = std::pow(
      config_.ignore_backoff_multiplier_base,
      std::min(consecutive_shown_without_interaction_, kMaxBackoffExponent));
  base::TimeDelta backoff = config_.min_time_between_nudges * multiplier;
  shown_backoff_end_time_ = base::TimeTicks::Now() + backoff;

  consecutive_shown_without_interaction_++;

  // Records global timestamp.
  global_tracker_.CueingNudgeShown();

  // Records per-origin timestamp.
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

void ContextualCueingCapTrackerService::RecordCueDismissed(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Dismissing a cue means the cue was not ignored; reset ignore backoff.
  shown_backoff_end_time_ = std::nullopt;
  consecutive_shown_without_interaction_ = 0;

  // Calculates exponential backoff for explicit dismissals.
  double multiplier =
      std::pow(config_.dismiss_backoff_multiplier_base,
               std::min(consecutive_dismissals_, kMaxBackoffExponent));
  base::TimeDelta backoff = config_.base_dismiss_backoff_time * multiplier;
  dismiss_backoff_end_time_ = base::TimeTicks::Now() + backoff;

  consecutive_dismissals_++;
}

void ContextualCueingCapTrackerService::RecordCueClicked(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Resets exponential backoff counters and any active shown/dismiss cooldowns
  // upon positive user engagement.
  shown_backoff_end_time_ = std::nullopt;
  dismiss_backoff_end_time_ = std::nullopt;
  consecutive_dismissals_ = 0;
  consecutive_shown_without_interaction_ = 0;

  click_backoff_end_time_ = base::TimeTicks::Now() + config_.click_backoff_time;
}

void ContextualCueingCapTrackerService::RecordPageNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remaining_quiet_loads_ > 0) {
    remaining_quiet_loads_--;
  }
}

std::optional<base::TimeTicks>
ContextualCueingCapTrackerService::GetMostRecentNudgeTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return global_tracker_.GetMostRecentNudgeTime();
}

}  // namespace contextual_cueing
