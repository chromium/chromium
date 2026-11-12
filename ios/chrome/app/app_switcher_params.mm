// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_switcher_params.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/startup/app_startup_utils.h"
#import "ios/chrome/browser/app_switcher/metrics/app_switcher_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/public/provider/chrome/browser/app_switcher/app_switcher_api.h"
#import "ios/public/provider/chrome/browser/application_mode_fetcher/application_mode_fetcher_api.h"
#import "url/gurl.h"

namespace {

// Time-out error domain reported by the application mode fetcher.
NSString* const kAppSwitcherTimeoutErrorDomain = @"AppSwitcherTimeoutError";

// This enum is used for Histogram. Items should not be removed or reordered and
// this enum should be kept synced with `IOSAppModeFetchingOutcomeType` in
// histograms.xml. The four states correspond to:
// - kIncognito: The application mode fetching response was successful and the
// fetched app mode is incognito.
// - kNonIncognito: The application mode fetching response was successful and
// the fetched app mode is non-incognito.
// - kFailure: The application mode fetching results in an error (different
// from a time out error).
// - kTimeOut: The application mode fetching results in a time out error.
enum class AppModeFetchingOutcomeType {
  kIncognito,
  kNonIncognito,
  kFailure,
  kTimeOut,
  kMaxValue = kTimeOut
};

// Converts the application mode fetcher response into `AppSwitcherParams`.
void OnApplicationModeFetched(base::TimeTicks fetch_start_time,
                              AppSwitcherParamsCallback callback,
                              bool is_incognito,
                              NSError* error) {
  AppSwitcherParams params;
  // The application mode fetcher is only used for callers allowlisted for
  // application mode, see `ShouldFetchAppSwitcherParams()`.
  params.can_change_application_mode = true;
  params.is_incognito = is_incognito;
  params.error = error;
  params.should_record_app_mode_fetching = true;
  params.fetch_start_time = fetch_start_time;
  std::move(callback).Run(params);
}

// Records the App Switcher fetch metrics and converts the App Switcher
// response into `AppSwitcherParams`.
void OnAppSwitcherParamsFetched(
    NSString* source_app_id,
    base::TimeTicks fetch_start_time,
    AppSwitcherParamsCallback callback,
    ios::provider::AppSwitcherUrlOpeningResult result) {
  bool success = !result.error;
  RecordAppSwitcherFetchOutcome(success);
  RecordAppSwitcherFetchDuration(base::TimeTicks::Now() - fetch_start_time);

  AppSwitcherParams params;
  params.fetch_start_time = fetch_start_time;
  if (IsCallerAppAllowListedForAISummarization(source_app_id)) {
    RecordAppSwitcherAISummarizationFetchOutcome(success);
    if (success && result.is_ai_summarization) {
      params.is_ai_summarization = true;
      params.hashed_user_id = base::SysUTF8ToNSString(result.hashed_user_id);
      RecordAppSwitcherAISummarizationEntrypoint();
    }
  }

  // Only callers allowlisted for application mode may change the mode.
  if (IsCallerAppAllowListedForApplicationMode(source_app_id)) {
    params.can_change_application_mode = true;
    params.is_incognito = result.is_incognito;
    params.error = result.error;
  }
  std::move(callback).Run(params);
}

}  // namespace

bool ShouldFetchAppSwitcherParams(NSString* source_app_id,
                                  bool is_incognito_forced) {
  if (IsAppSwitcherAISummarizationEnabled() &&
      IsCallerAppAllowListedForAISummarization(source_app_id)) {
    return true;
  }
  return IsCallerAppAllowListedForApplicationMode(source_app_id) &&
         !is_incognito_forced;
}

void RequestAppSwitcherParams(const GURL& url,
                              NSString* source_app_id,
                              AppSwitcherParamsCallback callback) {
  base::TimeTicks fetch_start_time = base::TimeTicks::Now();
  if (IsAppSwitcherAISummarizationEnabled()) {
    ios::provider::FetchAppSwitcherParams(
        url, base::SysNSStringToUTF8(source_app_id),
        base::BindOnce(&OnAppSwitcherParamsFetched, [source_app_id copy],
                       fetch_start_time, std::move(callback)));
    return;
  }
  ios::provider::FetchApplicationMode(
      url, source_app_id,
      base::BindOnce(&OnApplicationModeFetched, fetch_start_time,
                     std::move(callback)));
}

ApplicationModeForTabOpening ApplicationModeForAppSwitcherParams(
    const AppSwitcherParams& params,
    ApplicationModeForTabOpening current_mode) {
  if (!params.can_change_application_mode) {
    return current_mode;
  }
  if (params.is_incognito) {
    // When the `applicationMode` needs changing the error associated to the
    // response must be nil.
    CHECK(!params.error);
    return ApplicationModeForTabOpening::APP_SWITCHER_INCOGNITO;
  }
  if (params.error) {
    return ApplicationModeForTabOpening::APP_SWITCHER_UNDETERMINED;
  }
  return current_mode;
}

void RecordAppModeFetchingMetrics(const AppSwitcherParams& params) {
  if (!params.should_record_app_mode_fetching) {
    return;
  }
  AppModeFetchingOutcomeType outcome =
      AppModeFetchingOutcomeType::kNonIncognito;
  if (params.is_incognito) {
    outcome = AppModeFetchingOutcomeType::kIncognito;
  } else if (params.error) {
    outcome =
        [params.error.domain isEqualToString:kAppSwitcherTimeoutErrorDomain]
            ? AppModeFetchingOutcomeType::kTimeOut
            : AppModeFetchingOutcomeType::kFailure;
  }
  base::UmaHistogramEnumeration("IOS.AppModeFetching.Outcome", outcome);
  base::UmaHistogramTimes("IOS.AppModeFetching.Duration",
                          base::TimeTicks::Now() - params.fetch_start_time);
}
