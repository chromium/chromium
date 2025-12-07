// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"

#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using TestProfileManagerIOSTest = PlatformTest;

// Tests that the list of loaded profiles is empty after construction.
TEST_F(TestProfileManagerIOSTest, Constructor) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  EXPECT_EQ(0U, profile_manager.GetLoadedProfiles().size());
}

// Tests that the list of loaded profiles has one element after calling
// AddProfileWithBuilder(...).
TEST_F(TestProfileManagerIOSTest, AddProfileWithBuilder) {
  web::WebTaskEnvironment task_environment;
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;
  profile_manager.AddProfileWithBuilder(TestProfileIOS::Builder());
  EXPECT_EQ(1U, profile_manager.GetLoadedProfiles().size());
}
