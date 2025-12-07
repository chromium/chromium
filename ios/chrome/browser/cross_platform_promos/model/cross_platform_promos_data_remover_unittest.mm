// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_data_remover.h"

#import <UIKit/UIKit.h>

#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service.h"
#import "ios/chrome/browser/cross_platform_promos/model/cross_platform_promos_service_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test suite for the `CrossPlatformPromosDataRemover`.
class CrossPlatformPromosDataRemoverTest : public PlatformTest {
 public:
  CrossPlatformPromosDataRemoverTest() {
    profile_ = TestProfileIOS::Builder().Build();
    prefs_ = profile_->GetPrefs();
    remover_ = std::make_unique<CrossPlatformPromosDataRemover>(profile_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<PrefService> prefs_;
  std::unique_ptr<CrossPlatformPromosDataRemover> remover_;
};

// Tests that Remove() clears the relevant prefs.
TEST_F(CrossPlatformPromosDataRemoverTest, Remove_ClearsPrefs) {
  // Set some dummy values for the prefs.
  prefs_->SetTime(prefs::kCrossPlatformPromosIOS16thActiveDay,
                  base::Time::Now());
  base::Value::List active_days;
  active_days.Append(base::Value("test"));
  prefs_->SetList(prefs::kCrossPlatformPromosActiveDays,
                  std::move(active_days));

  EXPECT_EQ(1u, prefs_->GetList(prefs::kCrossPlatformPromosActiveDays).size());
  EXPECT_NE(base::Time(),
            prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay));

  // Clear the data.
  remover_->Remove();

  // Verify that the prefs are cleared.
  EXPECT_TRUE(prefs_->GetList(prefs::kCrossPlatformPromosActiveDays).empty());
  EXPECT_EQ(base::Time(),
            prefs_->GetTime(prefs::kCrossPlatformPromosIOS16thActiveDay));
}
