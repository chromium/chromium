// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_
#define IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/startup/app_launch_metrics.h"

// Checks if the caller app is a first party app.
bool IsCallerAppFirstParty(MobileSessionCallerApp caller_app);

// Checks if the caller app is allowed for the AI summarization experiment.
bool IsCallerAppAllowListedForAISummarization(NSString* caller_app_id);

// Checks if the caller app is allowed for the youtube incognito experiment.
bool IsCallerAppAllowListedForApplicationMode(NSString* caller_app_id);

// Saves field trial values and capabilities for the group app in shared
// NSUserDefaults.
void SaveFieldTrialValuesForGroupApp();

#endif  // IOS_CHROME_APP_STARTUP_APP_STARTUP_UTILS_H_
