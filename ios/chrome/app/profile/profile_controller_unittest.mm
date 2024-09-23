// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using PlatformControllerTest = PlatformTest;

// Tests that creating a ProfileController also creates a ProfileState.
TEST_F(PlatformControllerTest, initializer) {
  ProfileController* controller =
      [[ProfileController alloc] initWithAppState:nil];
  EXPECT_NE(controller.state, nil);
}
