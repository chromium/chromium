// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

constexpr BOOL kDefaultMetricsReportingCheckboxValue = YES;

namespace {

// Trampoline method for Bind to create the sentinel file.
void CreateSentinel() {
  base::File::Error file_error;
  FirstRun::SentinelResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  base::UmaHistogramEnumeration("FirstRun.Sentinel.Created", sentinel_created,
                                FirstRun::SentinelResult::SENTINEL_RESULT_MAX);
  if (sentinel_created ==
      FirstRun::SentinelResult::SENTINEL_RESULT_FILE_ERROR) {
    base::UmaHistogramExactLinear("FirstRun.Sentinel.CreatedFileError",
                                  -file_error, -base::File::FILE_ERROR_MAX);
  }
}

bool kFirstRunSentinelCreated = false;

}  // namespace

void RecordFirstRunSignInMetrics(
    signin::IdentityManager* identity_manager,
    first_run::SignInAttemptStatus sign_in_attempt_status,
    BOOL has_sso_accounts) {
  bool user_signed_in =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  first_run::SignInStatus sign_in_status;
  if (user_signed_in) {
    sign_in_status = has_sso_accounts
                         ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL
                         : first_run::SIGNIN_SUCCESSFUL;
  } else {
    switch (sign_in_attempt_status) {
      case first_run::SignInAttemptStatus::NOT_ATTEMPTED:
        sign_in_status = has_sso_accounts
                             ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK
                             : first_run::SIGNIN_SKIPPED_QUICK;
        break;
      case first_run::SignInAttemptStatus::ATTEMPTED:
        sign_in_status = has_sso_accounts
                             ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP
                             : first_run::SIGNIN_SKIPPED_GIVEUP;
        break;
      case first_run::SignInAttemptStatus::SKIPPED_BY_POLICY:
        sign_in_status = first_run::SIGNIN_SKIPPED_POLICY;
        break;
      case first_run::SignInAttemptStatus::NOT_SUPPORTED:
        sign_in_status = first_run::SIGNIN_NOT_SUPPORTED;
        break;
    }
  }
  base::UmaHistogramEnumeration("FirstRun.SignIn", sign_in_status,
                                first_run::SIGNIN_SIZE);
}

void RecordFirstRunScrollButtonVisibilityMetrics(
    first_run::FirstRunScreenType screen_type,
    BOOL scroll_button_visible) {
  switch (screen_type) {
    case first_run::FirstRunScreenType::kDefaultBrowserPromoScreen:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.DefaultBrowserPromoScreen",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSignInScreenWithFooter:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SignInScreenWithFooter",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::
        kSignInScreenWithFooterAndIdentityPicker:
      base::UmaHistogramBoolean("IOS.FirstRun.ScrollButtonVisible."
                                "SignInScreenWithFooterAndIdentityPicker",
                                scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kSignInScreenWithIdentityPicker:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.SignInScreenWithIdentityPicker",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::
        kSignInScreenWithoutFooterOrIdentityPicker:
      base::UmaHistogramBoolean("IOS.FirstRun.ScrollButtonVisible."
                                "SignInScreenWithoutFooterOrIdentityPicker",
                                scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kWelcomeScreenWithoutUMACheckbox:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.WelcomeScreenWithoutUMACheckbox",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kWelcomeScreenWithUMACheckbox:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.WelcomeScreenWithUMACheckbox",
          scroll_button_visible);
      break;
    case first_run::FirstRunScreenType::kTangibleSyncScreen:
      base::UmaHistogramBoolean(
          "IOS.FirstRun.ScrollButtonVisible.TangibleSyncScreen",
          scroll_button_visible);
      break;
  }
}

void WriteFirstRunSentinel() {
  kFirstRunSentinelCreated = true;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
}

bool ShouldPresentFirstRunExperience() {
  if (experimental_flags::AlwaysDisplayFirstRun())
    return true;

  if (tests_hook::DisableFirstRun())
    return false;

  if (kFirstRunSentinelCreated)
    return false;

  return FirstRun::IsChromeFirstRun();
}

void RecordMetricsReportingDefaultState() {
  // Record metrics reporting as opt-in/opt-out only once.
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    // Don't call RecordMetricsReportingDefaultState twice. This can happen if
    // the app is quit before accepting the TOS, or via experiment settings.
    if (metrics::GetMetricsReportingDefaultState(
            GetApplicationContext()->GetLocalState()) !=
        metrics::EnableMetricsDefault::DEFAULT_UNKNOWN) {
      return;
    }

    metrics::RecordMetricsReportingDefaultState(
        GetApplicationContext()->GetLocalState(),
        kDefaultMetricsReportingCheckboxValue
            ? metrics::EnableMetricsDefault::OPT_OUT
            : metrics::EnableMetricsDefault::OPT_IN);
  });
}
