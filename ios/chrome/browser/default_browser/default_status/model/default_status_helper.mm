// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/default_status/model/default_status_helper.h"

#import <UIKit/UIKit.h>

#import <optional>
#import <string_view>
#import <utility>

#import "base/check.h"
#import "base/check_is_test.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/rand_util.h"
#import "base/strings/stringprintf.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/default_status/model/default_status_helper_types.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace default_status {

namespace {

// The cohorts are monthly, aligned with the Gregorian calendar. The logic in
// this file should work for any cohort count between 1 and 12. Anything higher
// than that means the cohorts are no longer aligned to calendar months, so
// logic needs to be revisited.
//
// Changing this value requires updating the
// `IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort{Number}` histogram to make sure
// there is a variant for each cohort.
//
// LINT.IfChange(kCohortCount)
constexpr int kCohortCount = 4;
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/histograms.xml:CohortNumber)

// The minimum time required since completing the first run experience to be
// assigned to a cohort.
constexpr base::TimeDelta kMinimumNewClientFirstRunAge = base::Days(28);

// The cohort this client belongs to for reporting the result of the default
// status system API call.
constexpr std::string_view kDefaultStatusAPICohort = "DefaultStatusAPICohort";

// The timestamp of the last successful call to the default status API. A
// successful call is one that does not yield an error, regardless of the
// default status result.
constexpr std::string_view kDefaultStatusAPILastSuccessfulCall =
    "DefaultStatusAPILastSuccessfulCall";

// The timestamp after which the default status API should be called again for
// this client.
constexpr std::string_view kDefaultStatusAPINextRetry =
    "DefaultStatusAPINextRetry";

// The result of the last successful default status API call. The value is of
// type `DefaultStatusAPIResult` and defaults to
// `DefaultStatusAPIResult::kUnknown`.
constexpr std::string_view kDefaultStatusAPIResult = "DefaultStatusAPIResult";

// Compares the system default status and the heuristic default status.
DefaultStatusHeuristicAssessment AssessHeuristic(bool system_is_default,
                                                 bool heuristic_is_default) {
  if (heuristic_is_default) {
    return system_is_default ? DefaultStatusHeuristicAssessment::kTruePositive
                             : DefaultStatusHeuristicAssessment::kFalsePositive;
  }
  return system_is_default ? DefaultStatusHeuristicAssessment::kFalseNegative
                           : DefaultStatusHeuristicAssessment::kTrueNegative;
}

// Records the number of days left in the system call cooldown period.
void RecordCooldownErrorDaysLeft(int days_left) {
  base::UmaHistogramCounts1000("IOS.DefaultStatusAPI.CooldownError.DaysLeft",
                               days_left);
}

// Records how many days have passed since the last successful default status
// API call.
void RecordDaysSinceLastSuccessfulCall(int days) {
  base::UmaHistogramCounts1000(
      "IOS.DefaultStatusAPI.DaysSinceLastSuccessfulCall", days);
}

// Records the outcome type (error or success) of the default status API call.
void RecordDefaultStatusAPIOutcomeType(
    DefaultStatusAPIOutcomeType outcome_type) {
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.OutcomeType",
                                outcome_type);
}

// Records the result of a successful default status API call to cohort
// histograms.
void RecordDefaultStatusAPIResult(bool is_default,
                                  int cohort_number,
                                  bool is_within_cohort_window) {
  DUMP_WILL_BE_CHECK(cohort_number >= 1 && cohort_number <= kCohortCount);

  base::UmaHistogramBoolean("IOS.DefaultStatusAPI.IsDefaultBrowser",
                            is_default);

  base::UmaHistogramBoolean(
      base::StringPrintf("IOS.DefaultStatusAPI.IsDefaultBrowser.Cohort%d",
                         cohort_number),
      is_default);
  if (is_within_cohort_window) {
    base::UmaHistogramBoolean(
        "IOS.DefaultStatusAPI.IsDefaultBrowser.StrictCohorts", is_default);
  }
}

// Records the default status retention since the last successful default status
// API call.
void RecordDefaultStatusRetention(DefaultStatusRetention retention) {
  base::UmaHistogramEnumeration("IOS.DefaultStatusAPI.DefaultStatusRetention",
                                retention);
}

// Records the difference between `is_default_in_system_api` and Chrome's
// heuristics across lookback windows.
void RecordHeuristicAssessments(bool is_default_in_system_api) {
  static constexpr int kLookbackWindowsInDays[] = {1, 3, 7, 14, 21, 28, 35, 42};
  for (int days : kLookbackWindowsInDays) {
    DefaultStatusHeuristicAssessment assessment = AssessHeuristic(
        is_default_in_system_api, IsChromeLikelyDefaultBrowserXDays(days));
    base::UmaHistogramEnumeration(
        base::StringPrintf("IOS.DefaultStatusAPI.HeuristicAssessment%d", days),
        assessment);
  }
}

// Queries the real `UIApplication` default status API.
UIApplicationCategoryDefaultStatus QuerySystemDefaultStatus(NSError** error)
    API_AVAILABLE(ios(18.4)) {
  return [[UIApplication sharedApplication]
      defaultStatusForCategory:UIApplicationCategoryWebBrowser
                         error:error];
}

// Returns true if the current client is too recent (based on time since
// completing the first run experience) to be assigned to a cohort.
bool IsNewClient() {
  return IsFirstRunRecent(kMinimumNewClientFirstRunAge);
}

// Returns true if the current client is already part of a cohort.
bool HasCohortAssigned(PrefService* local_state) {
  return local_state->GetInteger(kDefaultStatusAPICohort) != 0;
}

// Returns a random cohort number between 1 and `kCohortCount`.
int GenerateRandomCohort() {
  return base::RandIntInclusive(1, kCohortCount);
}

// Returns a `base::Time` representing the first day of the next reporting
// window for `cohort_number` in UTC. When `after_current_month` is true, the
// search starts at the beginning of next month.
base::Time GetCohortNextStartDate(int cohort_number, bool after_current_month) {
  DUMP_WILL_BE_CHECK(cohort_number > 0 && cohort_number <= kCohortCount);

  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  DUMP_WILL_BE_CHECK(now_exploded.HasValidValues());
  base::Time::Exploded target_date;
  target_date.year = now_exploded.year;
  target_date.month = now_exploded.month;
  // Day of week does not matter when converting back to `base::Time`.
  target_date.day_of_week = 0;
  target_date.day_of_month = 1;
  target_date.hour = 0;
  target_date.minute = 0;
  target_date.second = 0;
  target_date.millisecond = 0;

  // Determine which cohort number the current month corresponds to. This
  // requires a modulo on the cohort count, but because the cohort number is
  // 1-based, subtract one before and add one after.
  int current_cohort = (now_exploded.month - 1) % kCohortCount + 1;

  if (current_cohort == cohort_number && !after_current_month) {
    target_date.year = now_exploded.year;
    target_date.month = now_exploded.month;
  } else {
    int target_cycle = (now_exploded.month - 1) / kCohortCount;
    if (current_cohort >= cohort_number) {
      ++target_cycle;
    }
    int target_month = target_cycle * kCohortCount + cohort_number;

    if (target_month > 12) {
      ++target_date.year;
      target_month -= 12;
    }

    target_date.month = target_month;
  }

  base::Time result;
  DUMP_WILL_BE_CHECK(base::Time::FromUTCExploded(target_date, &result));
  return result;
}

// Assigns `local_state` to a reporting cohort (using `cohort_override` when
// provided) and schedules the initial reporting window if no cohort is
// assigned yet.
void AssignNewCohortIfNeeded(PrefService* local_state,
                             std::optional<int> cohort_override) {
  if (HasCohortAssigned(local_state)) {
    return;
  }

  int cohort = cohort_override.value_or(GenerateRandomCohort());
  DUMP_WILL_BE_CHECK(cohort > 0 && cohort <= kCohortCount);
  local_state->SetInteger(kDefaultStatusAPICohort, cohort);
  base::Time next_retry =
      GetCohortNextStartDate(cohort, /*after_current_month=*/false);
  local_state->SetTime(kDefaultStatusAPINextRetry, next_retry);
}

// Converts `system_enum` to `DefaultStatusAPIResult`.
DefaultStatusAPIResult SystemToLocalEnum(
    UIApplicationCategoryDefaultStatus system_enum) API_AVAILABLE(ios(18.4)) {
  switch (system_enum) {
    case UIApplicationCategoryDefaultStatusIsDefault:
      return DefaultStatusAPIResult::kIsDefault;
    case UIApplicationCategoryDefaultStatusNotDefault:
      return DefaultStatusAPIResult::kIsNotDefault;
    case UIApplicationCategoryDefaultStatusUnavailable:
      return DefaultStatusAPIResult::kUnknown;
    default:
      return DefaultStatusAPIResult::kUnknown;
  }
}

// Returns the retention type based on `previous` and `current`.
DefaultStatusRetention DetermineRetentionStatus(
    DefaultStatusAPIResult previous,
    DefaultStatusAPIResult current) {
  DUMP_WILL_BE_CHECK(previous != DefaultStatusAPIResult::kUnknown &&
                     current != DefaultStatusAPIResult::kUnknown);
  if (previous == DefaultStatusAPIResult::kIsDefault) {
    return current == DefaultStatusAPIResult::kIsDefault
               ? DefaultStatusRetention::kRemainedDefault
               : DefaultStatusRetention::kBecameNonDefault;
  }
  return current == DefaultStatusAPIResult::kIsDefault
             ? DefaultStatusRetention::kBecameDefault
             : DefaultStatusRetention::kRemainedNonDefault;
}

// Invokes `system_call` if the client is outside the cooldown period and logs
// the resulting metrics and prefs.
void QueryDefaultStatusIfReadyAndLogResults(
    PrefService* local_state,
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
        system_call) API_AVAILABLE(ios(18.4)) {
  int cohort_number = local_state->GetInteger(kDefaultStatusAPICohort);
  DUMP_WILL_BE_CHECK(cohort_number > 0 && cohort_number <= kCohortCount);

  base::Time now = base::Time::Now();
  base::Time next_retry = local_state->GetTime(kDefaultStatusAPINextRetry);
  if (now < next_retry) {
    return;
  }

  NSError* error = nil;
  UIApplicationCategoryDefaultStatus default_status =
      std::move(system_call).Run(&error);

  if (error) {
    if (error.code == UIApplicationCategoryDefaultErrorRateLimited) {
      DUMP_WILL_BE_CHECK(
          error.userInfo
              [UIApplicationCategoryDefaultRetryAvailabilityDateErrorKey] !=
          nil);
      RecordDefaultStatusAPIOutcomeType(
          DefaultStatusAPIOutcomeType::kCooldownError);
      next_retry = base::Time::FromNSDate(
          error.userInfo
              [UIApplicationCategoryDefaultRetryAvailabilityDateErrorKey]);
      local_state->SetTime(kDefaultStatusAPINextRetry, next_retry);
      int days_left = (next_retry - now).InDays();
      RecordCooldownErrorDaysLeft(days_left);
    } else {
      RecordDefaultStatusAPIOutcomeType(
          DefaultStatusAPIOutcomeType::kOtherError);
    }
    return;
  }

  RecordDefaultStatusAPIOutcomeType(DefaultStatusAPIOutcomeType::kSuccess);

  DefaultStatusAPIResult current_result = SystemToLocalEnum(default_status);
  DUMP_WILL_BE_CHECK(current_result != DefaultStatusAPIResult::kUnknown);

  base::Time cohort_start =
      GetCohortNextStartDate(cohort_number, /*after_current_month=*/false);
  int next_cohort_number = cohort_number % kCohortCount + 1;
  base::Time next_cohort_start =
      GetCohortNextStartDate(next_cohort_number, /*after_current_month=*/false);
  bool is_within_cohort_window = now >= cohort_start && now < next_cohort_start;
  bool is_default = current_result == DefaultStatusAPIResult::kIsDefault;
  RecordDefaultStatusAPIResult(is_default, cohort_number,
                               is_within_cohort_window);
  RecordHeuristicAssessments(is_default);

  DefaultStatusAPIResult previous_result = static_cast<DefaultStatusAPIResult>(
      local_state->GetInteger(kDefaultStatusAPIResult));
  if (previous_result != DefaultStatusAPIResult::kUnknown) {
    base::Time last_successful_call =
        local_state->GetTime(kDefaultStatusAPILastSuccessfulCall);
    RecordDaysSinceLastSuccessfulCall((now - last_successful_call).InDays());
    RecordDefaultStatusRetention(
        DetermineRetentionStatus(previous_result, current_result));
  }

  local_state->SetTime(kDefaultStatusAPILastSuccessfulCall, now);
  local_state->SetInteger(kDefaultStatusAPIResult,
                          static_cast<int>(current_result));
  next_retry =
      GetCohortNextStartDate(cohort_number, /*after_current_month=*/true);
  local_state->SetTime(kDefaultStatusAPINextRetry, next_retry);
}

// Runs the default status check pipeline for `local_state`.
void RunDefaultStatusCheck(
    PrefService* local_state,
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
        system_call,
    std::optional<int> cohort_override) API_AVAILABLE(ios(18.4)) {
  CHECK(local_state);
  if (IsNewClient() || !IsRunDefaultStatusCheckEnabled()) {
    return;
  }

  AssignNewCohortIfNeeded(local_state, cohort_override);
  QueryDefaultStatusIfReadyAndLogResults(local_state, std::move(system_call));
}

}  // namespace

void RegisterDefaultStatusPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kDefaultStatusAPICohort, 0);
  registry->RegisterTimePref(kDefaultStatusAPILastSuccessfulCall, base::Time());
  registry->RegisterTimePref(kDefaultStatusAPINextRetry, base::Time());
  registry->RegisterIntegerPref(
      kDefaultStatusAPIResult,
      static_cast<int>(DefaultStatusAPIResult::kUnknown));
}

void TriggerDefaultStatusCheck() {
  if (@available(iOS 18.4, *)) {
    ApplicationContext* application_context = GetApplicationContext();
    RunDefaultStatusCheck(application_context->GetLocalState(),
                          base::BindOnce(&QuerySystemDefaultStatus),
                          /*cohort_override=*/std::nullopt);
  }
}

void TriggerDefaultStatusCheckForTesting(
    PrefService* local_state,
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
        system_call,
    std::optional<int> cohort_override) API_AVAILABLE(ios(18.4)) {
  CHECK_IS_TEST();
  RunDefaultStatusCheck(local_state, std::move(system_call), cohort_override);
}

}  // namespace default_status
