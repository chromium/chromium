// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Constants used for test.
static constexpr char kBoolKey[] = "IncognitoActive";
static constexpr char kTimeKey[] = "StartSurfaceSceneEnterIntoBackgroundTime";
static constexpr char kSessionName[] = "D5A906A5-A92C-4729-86B6-DB18F51D63C8";

}  // namespace

class SceneStatePrefsTest : public PlatformTest {
 public:
  SceneStatePrefsTest() = default;

  TestProfileManagerIOS* manager() { return &manager_; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS manager_;
};

// Test that SceneStatePrefs can save/retrieve the values for key.
TEST_F(SceneStatePrefsTest, ReadWritePrefs) {
  const std::string profile_name = manager()->ReserveNewProfileName();
  SceneStatePrefs* prefs =
      [[SceneStatePrefs alloc] initWithProfileManager:manager()
                                          profileName:profile_name
                                    sessionIdentifier:kSessionName];

  // Test that default values are returned if the prefs are not set.
  EXPECT_EQ([prefs boolForKey:kBoolKey], false);
  EXPECT_EQ([prefs timeForKey:kTimeKey], base::Time());

  // Test that after setting a value, it is correctly returned.
  const base::Time time = base::Time::Now();
  [prefs setBool:true forKey:kBoolKey];
  [prefs setTime:time forKey:kTimeKey];

  EXPECT_EQ([prefs boolForKey:kBoolKey], true);
  EXPECT_EQ([prefs timeForKey:kTimeKey], time);
}
