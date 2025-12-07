// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/observing_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface SampleObservingProfileAgent : ObservingProfileAgent
@end

@implementation SampleObservingProfileAgent
@end

using ObservingProfileAgentTest = PlatformTest;

// Tests that adding an ObservingProfileAgent to ProfileState correctly
// sets the -profileState property.
TEST_F(ObservingProfileAgentTest, profileState) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];

  SampleObservingProfileAgent* agent =
      [[SampleObservingProfileAgent alloc] init];
  EXPECT_EQ(agent.profileState, nil);

  [state addAgent:agent];
  EXPECT_EQ(agent.profileState, state);
}

// Tests that adding an ObservingProfileAgent to ProfileState allow
// retrieving it via the -agentFromProfile: method.
TEST_F(ObservingProfileAgentTest, agentFromProfile) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  EXPECT_EQ([SampleObservingProfileAgent agentFromProfile:state], nil);

  SampleObservingProfileAgent* agent =
      [[SampleObservingProfileAgent alloc] init];
  EXPECT_EQ([SampleObservingProfileAgent agentFromProfile:state], nil);

  [state addAgent:agent];
  EXPECT_EQ([SampleObservingProfileAgent agentFromProfile:state], agent);
}
