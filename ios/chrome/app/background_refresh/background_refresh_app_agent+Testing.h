// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_TESTING_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_TESTING_H_

#import "ios/chrome/app/background_refresh/background_refresh_app_agent.h"

@class BGTask;

// Testing category exposing background refresh triggering for tests.
@interface BackgroundRefreshAppAgent (Testing)

// Simulates a background refresh for `task` by synchronously executing
// `-handleExecutionForTask:` on the main thread.
- (void)simulateRefreshWithTask:(BGTask*)task;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_BACKGROUND_REFRESH_APP_AGENT_TESTING_H_
