// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/collaboration/public/collaboration_service.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace collaboration {

class CollaborationServiceFactoryTest : public PlatformTest {
 public:
  CollaborationServiceFactoryTest() = default;

  void InitService(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {
              kTabGroupSync,
              kTabGroupsIPad,
              kModernTabStrip,
              data_sharing::features::kDataSharingFeature,
          },
          /*disable_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{},
          /*disable_features=*/{{data_sharing::features::kDataSharingFeature}});
    }
    profile_ = TestProfileIOS::Builder().Build();
  }

  ~CollaborationServiceFactoryTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests the creation of the service in regular.
TEST_F(CollaborationServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitService(/*enable_feature=*/true);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

// Tests that the factory is returning a nil pointer for incognito.
TEST_F(CollaborationServiceFactoryTest,
       FeatureEnabledUsesEmptyServiceInIncognito) {
  InitService(/*enable_feature=*/true);
  CollaborationService* service = CollaborationServiceFactory::GetForProfile(
      profile_->GetOffTheRecordProfile());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(CollaborationServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitService(/*enable_feature=*/false);
  CollaborationService* service =
      CollaborationServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace collaboration
