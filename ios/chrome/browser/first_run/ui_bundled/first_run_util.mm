// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"

#import "base/files/file.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/metrics/metrics_reporting_default_state.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/startup_metric_utils/browser/startup_metric_utils.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/first_run/model/first_run.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

constexpr BOOL kDefaultMetricsReportingCheckboxValue = YES;

namespace {

// Trampoline method for Bind to create the sentinel file.
void CreateSentinel() {
  base::File::Error file_error;
  startup_metric_utils::FirstRunSentinelCreationResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  startup_metric_utils::GetBrowser().RecordFirstRunSentinelCreation(
      sentinel_created);
  if (sentinel_created ==
      startup_metric_utils::FirstRunSentinelCreationResult::kFileSystemError) {
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

void WriteFirstRunSentinel() {
  kFirstRunSentinelCreated = true;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
}

bool ShouldPresentFirstRunExperience() {
  if (experimental_flags::AlwaysDisplayFirstRun())
    return true;

  if (tests_hook::DisableDefaultFirstRun()) {
    return false;
  }
  return !HasFirstRunSentinel();
}

bool HasFirstRunSentinel() {
  if (kFirstRunSentinelCreated) {
    return true;
  }
  return !FirstRun::IsChromeFirstRun();
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
