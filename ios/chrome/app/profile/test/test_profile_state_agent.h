// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_TEST_TEST_PROFILE_STATE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_TEST_TEST_PROFILE_STATE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_state_agent.h"

@interface TestProfileStateAgent : NSObject <ProfileStateAgent>

@property(nonatomic, weak) ProfileState* profileState;

@end

#endif  // IOS_CHROME_APP_PROFILE_TEST_TEST_PROFILE_STATE_AGENT_H_
