// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_UTIL_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_UTIL_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "ios/chrome/browser/first_run/model/first_run_metrics.h"

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

// Returns whether the First Run Sentinel exists.
bool HasFirstRunSentinel();

// Records what the default opt-in state for metrics reporting is in the local
// prefs, based on whether the consent checkbox should be selected by default.
void RecordMetricsReportingDefaultState();

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FIRST_RUN_UTIL_H_
