// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/platform_test.h"

// Test fixture for the share kit service factory.
class ShareKitServiceFactoryTest : public PlatformTest {
 protected:
  ShareKitServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync,
         data_sharing::features::kDataSharingFeature},
        {});
    profile_ = TestProfileIOS::Builder().Build();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that the factory isn't returning a service in incognito.
TEST_F(ShareKitServiceFactoryTest, NoProfileInIncognito) {
  ShareKitService* regular_service =
      ShareKitServiceFactory::GetForProfile(profile_.get());
  ShareKitService* off_the_record_service =
      ShareKitServiceFactory::GetForProfile(profile_->GetOffTheRecordProfile());
  EXPECT_NE(nullptr, regular_service);
  EXPECT_EQ(nullptr, off_the_record_service);
}
