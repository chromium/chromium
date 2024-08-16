// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/test/test_profile_state_agent.h"

@implementation TestProfileStateAgent

- (void)setProfileState:(ProfileState*)profileState {
  _profileState = profileState;
}

@end
