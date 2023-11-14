// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_

#import <UIKit/UIKit.h>

#include <optional>

#include "ios/chrome/browser/first_run/model/first_run_metrics.h"

namespace base {
class Time;
}
namespace signin {
class IdentityManager;
}

@protocol SyncPresenter;

// Default value for metrics reporting state. "YES" corresponding to "opt-out"
// state.
extern const BOOL kDefaultMetricsReportingCheckboxValue;

// Records the result of the sign in steps for the First Run.
void RecordFirstRunSignInMetrics(
    signin::IdentityManager* identity_manager,
    first_run::SignInAttemptStatus sign_in_attempt_status,
    BOOL has_sso_accounts);

// Records the completion of the first run.
void WriteFirstRunSentinel();

// Returns whether the First Run Experience should be presented.
bool ShouldPresentFirstRunExperience();

// Records what the default opt-in state for metrics reporting is in the local
// prefs, based on whether the consent checkbox should be selected by default.
void RecordMetricsReportingDefaultState();

// If the first run sentinel file exist, returns the info; otherwise, return
// `std::nullopt`.
std::optional<base::Time> GetFirstRunTime();

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
