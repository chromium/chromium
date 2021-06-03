// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_FIRST_RUN_APP_STATE_AGENT_TESTING_H_
#define IOS_CHROME_APP_FIRST_RUN_APP_STATE_AGENT_TESTING_H_

#import "ios/chrome/app/first_run_app_state_agent.h"

// Interface for testing FirstRunAppAgent.
@interface FirstRunAppAgent (TestingOnly)

// TODO(crbug.com/1210246): Remove this once the chrome test fixture is adapted
// to startup testing.
// Shows the First Run UI for testing purpose.
- (void)showFirstRunUI;

@end

#endif  // IOS_CHROME_APP_FIRST_RUN_APP_STATE_AGENT_TESTING_H_
