// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/first_run/first_run_metrics.h"

namespace signin {
class IdentityManager;
}
namespace web {
class WebState;
}

class ChromeBrowserState;
@class FirstRunConfiguration;
@protocol SyncPresenter;

// Notification sent when the first run ends, right before dimissing the Terms
// of Service modal view.
extern NSString* const kChromeFirstRunUIWillFinishNotification;

// Notification sent when the first run has finished and has dismissed the Terms
// of Service modal view.
extern NSString* const kChromeFirstRunUIDidFinishNotification;

// Default value for metrics reporting state. "YES" corresponding to "opt-out"
// state.
extern const BOOL kDefaultMetricsReportingCheckboxValue;

// Records the result of the sign in steps for the First Run.
void RecordFirstRunSignInMetrics(
    signin::IdentityManager* identity_manager,
    first_run::SignInAttemptStatus sign_in_attempt_status,
    BOOL has_sso_accounts);

// Records the result of the sign in steps for the First Run.
void RecordFirstRunScrollButtonVisibilityMetrics(
    first_run::FirstRunScreenType screen_type,
    BOOL scroll_button_visible);

// Records the completion of the first run.
void WriteFirstRunSentinel();

// Methods for writing sentinel and recording metrics and posting notifications
void FinishFirstRun(ChromeBrowserState* browserState,
                    web::WebState* web_state,
                    FirstRunConfiguration* config,
                    id<SyncPresenter> presenter);

// Posts a notification that First Run did finish.
void FirstRunDismissed();

// Returns whether the First Run Experience should be presented.
bool ShouldPresentFirstRunExperience();

// Records what the default opt-in state for metrics reporting is in the local
// prefs, based on whether the consent checkbox should be selected by default.
void RecordMetricsReportingDefaultState();

// Returns whether the browser is managed based on the presence of policy data
// in the app configuration.
bool IsApplicationManaged();

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_FIRST_RUN_UTIL_H_
