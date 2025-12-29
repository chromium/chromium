// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_TESTING_H_
#define IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_TESTING_H_

#import "ios/chrome/app/profile/first_run_profile_agent.h"

enum class GuidedTourStep;

@interface FirstRunProfileAgent (Testing)

- (void)dismissGuidedTourPromo;
- (void)nextTappedForStep:(GuidedTourStep)step;
- (void)showSyncedSetUp;
- (void)showFirstRunUI;
- (void)performNextPostFirstRunAction;

@end

#endif  // IOS_CHROME_APP_PROFILE_FIRST_RUN_PROFILE_AGENT_TESTING_H_
