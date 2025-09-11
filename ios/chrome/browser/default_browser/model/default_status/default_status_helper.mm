// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/check_is_test.h"
#import "base/functional/callback.h"
#import "base/rand_util.h"
#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_constants.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_metrics.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_prefs.h"
#import "ios/chrome/browser/default_browser/model/default_status/default_status_helper_types.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace default_status {

namespace internal {

namespace {

// The minimum time required since completing the first run experience to be
// assigned to a cohort.
const base::TimeDelta kMinimumNewClientFirstRunAge = base::Days(28);

// Gets the system call to use/override for testing. Uses a function so the
// static variable can be contained inside the function.

base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>&
GetSystemCallForTesting() API_AVAILABLE(ios(18.4)) {
  static base::NoDestructor<
      base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>>
      g_system_call_for_testing;
  return *g_system_call_for_testing;
}

// Performs the actual system call to the default status check API. Wrapped in
// its own function to facilitate mocking in tests.

UIApplicationCategoryDefaultStatus MakeSystemCall(NSError** error)
    API_AVAILABLE(ios(18.4)) {
  if (GetSystemCallForTesting()) {
    CHECK_IS_TEST();
    return std::move(GetSystemCallForTesting()).Run(error);  // IN-TEST
  }
  return [[UIApplication sharedApplication]
      defaultStatusForCategory:UIApplicationCategoryWebBrowser
                         error:error];
}

}  // namespace

bool IsNewClient() {
  return IsFirstRunRecent(kMinimumNewClientFirstRunAge);
}

bool HasCohortAssigned(PrefService* local_state) {
  return local_state->GetInteger(kDefaultStatusAPICohort) != 0;
}

int GenerateRandomCohort() {
  // The arguments passed to RandInt are inclusive.
  return base::RandInt(1, kCohortCount);
}

base::Time GetCohortNextStartDate(int cohort_number, bool after_current_month) {
  DUMP_WILL_BE_CHECK(cohort_number > 0 && cohort_number <= kCohortCount);

  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  DUMP_WILL_BE_CHECK(now_exploded.HasValidValues());
  base::Time::Exploded target_date;
  target_date.year = now_exploded.year;
  target_date.month = now_exploded.month;
  // Day of week does not matter when converting back to base::Time.
  target_date.day_of_week = 0;
  target_date.day_of_month = 1;
  target_date.hour = 0;
  target_date.minute = 0;
  target_date.second = 0;
  target_date.millisecond = 0;

  // Determine which cohort number the current month corresponds to. This
  // requires a modulo on the cohort count, but because the cohort number is
  // 1-based, substract one before and add one after.
  int current_cohort = (now_exploded.month - 1) % kCohortCount + 1;

  if (current_cohort == cohort_number && !after_current_month) {
    // If the current cohort is the same as the specified cohort, the target
    // month is the current month.
    target_date.year = now_exploded.year;
    target_date.month = now_exploded.month;
  } else {
    // Otherwise, find the "cohort cycle" in which the target month is - the
    // target month is the start of that cycle plus the cohort number. If the
    // specified cohort is after the current cohort, then the target month is
    // later in this cohort cycle. If it's before, it means the desired cohort
    // has already passed this cycle, so the target month is in the next.
    int target_cycle = (now_exploded.month - 1) / kCohortCount;
    if (current_cohort >= cohort_number) {
      ++target_cycle;
    }
    int target_month = target_cycle * kCohortCount + cohort_number;

    // If the target month is greater than 12, decrease it by 12 and bump the
    // target year.
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

void AssignNewCohortIfNeeded(PrefService* local_state) {
  if (!@available(iOS 18.4, *)) {
    return;
  }

  if (IsNewClient()) {
    return;
  }

  if (HasCohortAssigned(local_state)) {
    return;
  }

  int cohort = GenerateRandomCohort();
  local_state->SetInteger(kDefaultStatusAPICohort, cohort);
  base::Time next_retry = GetCohortNextStartDate(cohort);
  local_state->SetTime(kDefaultStatusAPINextRetry, next_retry);
}

DefaultStatusAPIResult SystemToLocalEnum(
    UIApplicationCategoryDefaultStatus system_enum) API_AVAILABLE(ios(18.4)) {
  switch (system_enum) {
    case UIApplicationCategoryDefaultStatusIsDefault:
      return DefaultStatusAPIResult::kIsDefault;
    case UIApplicationCategoryDefaultStatusNotDefault:
      return DefaultStatusAPIResult::kIsNotDefault;
    case UIApplicationCategoryDefaultStatusUnavailable:
      // If the call was unavailable, the default status of the user is
      // considered unknown.
      return DefaultStatusAPIResult::kUnknown;
    default:
      return DefaultStatusAPIResult::kUnknown;
  }
}

DefaultStatusRetention DetermineRetentionStatus(
    DefaultStatusAPIResult previous,
    DefaultStatusAPIResult current) {
  DUMP_WILL_BE_CHECK(previous != DefaultStatusAPIResult::kUnknown &&
                     current != DefaultStatusAPIResult::kUnknown);
  if (previous == DefaultStatusAPIResult::kIsDefault) {
    if (current == DefaultStatusAPIResult::kIsDefault) {
      return DefaultStatusRetention::kRemainedDefault;
    } else {
      return DefaultStatusRetention::kBecameNonDefault;
    }
  } else {
    if (current == DefaultStatusAPIResult::kIsDefault) {
      return DefaultStatusRetention::kBecameDefault;
    } else {
      return DefaultStatusRetention::kRemainedNonDefault;
    }
  }
}

void QueryDefaultStatusIfReadyAndLogResults(PrefService* local_state) {
  if (@available(iOS 18.4, *)) {
    int cohort_number = local_state->GetInteger(kDefaultStatusAPICohort);
    DUMP_WILL_BE_CHECK(cohort_number > 0 && cohort_number <= kCohortCount);

    base::Time now = base::Time::Now();
    base::Time next_retry = local_state->GetTime(kDefaultStatusAPINextRetry);
    if (now < next_retry) {
      return;
    }

    NSError* error = nil;
    UIApplicationCategoryDefaultStatus default_status = MakeSystemCall(&error);

    if (error) {
      if ([error code] == UIApplicationCategoryDefaultErrorRateLimited) {
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
    } else {
      RecordDefaultStatusAPIOutcomeType(DefaultStatusAPIOutcomeType::kSuccess);

      DefaultStatusAPIResult current_result = SystemToLocalEnum(default_status);
      DUMP_WILL_BE_CHECK(current_result != DefaultStatusAPIResult::kUnknown);

      base::Time cohort_start = GetCohortNextStartDate(cohort_number);
      int next_cohort_number = cohort_number % kCohortCount + 1;
      base::Time next_cohort_start = GetCohortNextStartDate(next_cohort_number);
      bool is_within_cohort_window =
          now >= cohort_start && now < next_cohort_start;
      bool is_default = current_result == DefaultStatusAPIResult::kIsDefault;
      RecordDefaultStatusAPIResult(is_default, cohort_number,
                                   is_within_cohort_window);
      RecordHeuristicAssessments(is_default);

      DefaultStatusAPIResult previous_result =
          static_cast<DefaultStatusAPIResult>(
              local_state->GetInteger(kDefaultStatusAPIResult));
      if (previous_result != DefaultStatusAPIResult::kUnknown) {
        base::Time last_successful_call =
            local_state->GetTime(kDefaultStatusAPILastSuccessfulCall);
        RecordDaysSinceLastSuccessfulCall(
            (now - last_successful_call).InDays());
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
  }
}

void OverrideSystemCallForTesting(  // IN-TEST
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)> cb)
    API_AVAILABLE(ios(18.4)) {
  CHECK_IS_TEST();
  internal::GetSystemCallForTesting() = std::move(cb);
}

}  // namespace internal

void TriggerDefaultStatusCheck() {
  if (!IsRunDefaultStatusCheckEnabled()) {
    return;
  }

  if (!@available(iOS 18.4, *)) {
    return;
  }

  if (internal::IsNewClient()) {
    return;
  }

  ApplicationContext* applicationContext = GetApplicationContext();
  PrefService* localState = applicationContext->GetLocalState();
  internal::AssignNewCohortIfNeeded(localState);
  internal::QueryDefaultStatusIfReadyAndLogResults(localState);
}

}  // namespace default_status
