// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/contextual_cueing_cap_tracker_service.h"

#import <algorithm>
#import <cmath>
#import <string>
#import <utility>

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace contextual_cueing {

namespace {

// Maximum exponent cap for exponential backoff calculations to prevent
// floating-point overflow.
constexpr size_t kMaxBackoffExponent = 20;

// Keys used in the `prefs::kIOSContextualCueingVerticalUiState` dictionary.
constexpr char kSwitchedToOmniboxKey[] = "switched_to_omnibox";
constexpr char kConsecutiveMessageIgnoresKey[] = "consecutive_message_ignores";

std::string CategoryKey(page_content_annotations::CategoryType category) {
  return base::NumberToString(static_cast<int>(category));
}

}  // namespace

#pragma mark - Config

ContextualCueingCapTrackerService::Config::Config()
    : global_cap_count(kGlobalCapCount.Get()),
      global_duration(kGlobalCapDuration.Get()),
      origin_cap_count(kOriginCapCount.Get()),
      origin_duration(kOriginCapDuration.Get()),
      visited_origins_limit(kVisitedOriginsLimit.Get()),
      min_page_count_between_nudges(kMinPageCountBetweenNudges.Get()),
      min_time_between_nudges(kMinTimeBetweenNudges.Get()),
      ignore_backoff_multiplier_base(kIgnoreBackoffMultiplierBase.Get()),
      base_dismiss_backoff_time(kBaseDismissBackoffTime.Get()),
      dismiss_backoff_multiplier_base(kDismissBackoffMultiplierBase.Get()),
      click_backoff_time(kClickBackoffTime.Get()),
      disable_frequency_capping_and_backoff(
          kDisableFrequencyCappingAndBackoff.Get() ||
          IsIgnoreContextualCueingThresholdsEnabled()),
      max_consecutive_message_ignores(kMaxConsecutiveMessageIgnores.Get()),
      force_message_ui_only(kForceMessageUiOnly.Get() ||
                            IsIgnoreContextualCueingThresholdsEnabled()),
      force_omnibox_chip_ui_only(kForceOmniboxChipUiOnly.Get()) {}
ContextualCueingCapTrackerService::Config::~Config() = default;
ContextualCueingCapTrackerService::Config::Config(const Config&) = default;
ContextualCueingCapTrackerService::Config&
ContextualCueingCapTrackerService::Config::operator=(const Config&) = default;
ContextualCueingCapTrackerService::Config::Config(Config&&) = default;
ContextualCueingCapTrackerService::Config&
ContextualCueingCapTrackerService::Config::operator=(Config&&) = default;

#pragma mark - ContextualCueingCapTrackerService

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService()
    : ContextualCueingCapTrackerService(/*pref_service=*/nullptr, Config()) {}

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService(
    PrefService* pref_service)
    : ContextualCueingCapTrackerService(pref_service, Config()) {}

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService(
    Config config)
    : ContextualCueingCapTrackerService(/*pref_service=*/nullptr,
                                        std::move(config)) {}

ContextualCueingCapTrackerService::ContextualCueingCapTrackerService(
    PrefService* pref_service,
    Config config)
    : pref_service_(pref_service),
      config_(std::move(config)),
      global_tracker_(config_.global_cap_count, config_.global_duration),
      origin_trackers_(config_.visited_origins_limit) {
  LoadVerticalUiStatesFromPrefs();
}

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

ContextualCueUiType ContextualCueingCapTrackerService::GetCueUiTypeForCategory(
    page_content_annotations::CategoryType category) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config_.force_message_ui_only) {
    return ContextualCueUiType::kMessage;
  }
  if (config_.force_omnibox_chip_ui_only) {
    return ContextualCueUiType::kOmniboxChip;
  }

  auto it = vertical_ui_states_.find(category);
  if (it == vertical_ui_states_.end()) {
    return ContextualCueUiType::kMessage;
  }

  const VerticalUiState& state = it->second;
  if (state.switched_to_omnibox) {
    return ContextualCueUiType::kOmniboxChip;
  }
  return ContextualCueUiType::kMessage;
}

void ContextualCueingCapTrackerService::RecordCueShown(
    const GURL& url,
    std::optional<page_content_annotations::CategoryType> category) {
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

  // Updates per-vertical Message-to-Omnibox UI transition state.
  if (category.has_value()) {
    ContextualCueUiType shown_ui_type = GetCueUiTypeForCategory(*category);
    VerticalUiState& state = vertical_ui_states_[*category];
    state.last_shown_ui_type = shown_ui_type;

    if (shown_ui_type == ContextualCueUiType::kMessage &&
        !config_.force_message_ui_only) {
      state.consecutive_message_ignores++;
      if (state.consecutive_message_ignores >=
          config_.max_consecutive_message_ignores) {
        state.switched_to_omnibox = true;
      }
      SaveVerticalUiState(*category);
    } else if (shown_ui_type == ContextualCueUiType::kOmniboxChip &&
               state.consecutive_message_ignores > 0) {
      // An Omnibox Chip is now being shown; clear any prior unclicked Message
      // ignore count so clicking this Omnibox Chip does not revert the
      // transition.
      state.consecutive_message_ignores = 0;
      SaveVerticalUiState(*category);
    }
  }
}

void ContextualCueingCapTrackerService::RecordCueDismissed(
    const GURL& url,
    std::optional<page_content_annotations::CategoryType> category) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only Message UI supports explicit user dismissal. If the cue for `category`
  // was an Omnibox Chip, do not apply the 24-hour dismissal backoff.
  if (category.has_value()) {
    if (config_.force_omnibox_chip_ui_only) {
      return;
    }
    auto it = vertical_ui_states_.find(*category);
    if (it != vertical_ui_states_.end() &&
        it->second.last_shown_ui_type != ContextualCueUiType::kMessage) {
      return;
    }
  }

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

  // Dismissing a Message once permanently switches that vertical to Omnibox
  // Chip UI.
  if (category.has_value() && !config_.force_message_ui_only) {
    VerticalUiState& state = vertical_ui_states_[*category];
    state.switched_to_omnibox = true;
    state.consecutive_message_ignores = 0;
    SaveVerticalUiState(*category);
  }
}

void ContextualCueingCapTrackerService::RecordCueClicked(
    const GURL& url,
    std::optional<page_content_annotations::CategoryType> category) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Resets exponential backoff counters and any active shown/dismiss cooldowns
  // upon positive user engagement.
  shown_backoff_end_time_ = std::nullopt;
  dismiss_backoff_end_time_ = std::nullopt;
  consecutive_dismissals_ = 0;
  consecutive_shown_without_interaction_ = 0;

  click_backoff_end_time_ = base::TimeTicks::Now() + config_.click_backoff_time;

  // Accepting a Message UI cue allows showing Message UI again next time for
  // that vertical and resets consecutive ignore count. Once switched to
  // Omnibox Chip UI, clicks on the Omnibox Chip never switch back to Message.
  if (category.has_value() && !config_.force_message_ui_only &&
      !config_.force_omnibox_chip_ui_only) {
    VerticalUiState& state = vertical_ui_states_[*category];
    if (state.last_shown_ui_type == ContextualCueUiType::kMessage) {
      state.switched_to_omnibox = false;
      state.consecutive_message_ignores = 0;
      SaveVerticalUiState(*category);
    }
  }
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

void ContextualCueingCapTrackerService::LoadVerticalUiStatesFromPrefs() {
  if (!pref_service_ || !pref_service_->FindPreference(
                            prefs::kIOSContextualCueingVerticalUiState)) {
    return;
  }

  const base::DictValue& dict =
      pref_service_->GetDict(prefs::kIOSContextualCueingVerticalUiState);
  for (const auto [key, value] : dict) {
    int category_int = 0;
    if (!base::StringToInt(key, &category_int) || !value.is_dict()) {
      continue;
    }
    if (category_int < 0 ||
        category_int > static_cast<int>(
                           page_content_annotations::CategoryType::kMaxValue)) {
      continue;
    }
    const base::DictValue& state_dict = value.GetDict();
    VerticalUiState state;
    state.switched_to_omnibox =
        state_dict.FindBool(kSwitchedToOmniboxKey).value_or(false);
    int ignores = state_dict.FindInt(kConsecutiveMessageIgnoresKey).value_or(0);
    state.consecutive_message_ignores =
        ignores > 0 ? static_cast<size_t>(ignores) : 0;
    state.last_shown_ui_type = state.switched_to_omnibox
                                   ? ContextualCueUiType::kOmniboxChip
                                   : ContextualCueUiType::kMessage;

    vertical_ui_states_[static_cast<page_content_annotations::CategoryType>(
        category_int)] = state;
  }
}

void ContextualCueingCapTrackerService::SaveVerticalUiState(
    page_content_annotations::CategoryType category) {
  if (!pref_service_ || !pref_service_->FindPreference(
                            prefs::kIOSContextualCueingVerticalUiState)) {
    return;
  }

  auto it = vertical_ui_states_.find(category);
  if (it == vertical_ui_states_.end()) {
    return;
  }

  const VerticalUiState& state = it->second;
  ScopedDictPrefUpdate update(pref_service_,
                              prefs::kIOSContextualCueingVerticalUiState);
  base::DictValue state_dict;
  state_dict.Set(kSwitchedToOmniboxKey, state.switched_to_omnibox);
  state_dict.Set(kConsecutiveMessageIgnoresKey,
                 static_cast<int>(state.consecutive_message_ignores));
  update->Set(CategoryKey(category), std::move(state_dict));
}

}  // namespace contextual_cueing
