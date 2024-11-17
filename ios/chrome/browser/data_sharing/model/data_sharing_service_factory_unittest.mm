// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace data_sharing {

class DataSharingServiceFactoryTest : public PlatformTest {
 public:
  DataSharingServiceFactoryTest() = default;
  ~DataSharingServiceFactoryTest() override = default;

  void InitService(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{data_sharing::features::kDataSharingFeature, {}}}, {});
    } else {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {}, {{data_sharing::features::kDataSharingFeature}});
    }
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(DataSharingServiceFactory::GetInstance(),
                              DataSharingServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
  }

  void TearDown() override { web_task_env_.RunUntilIdle(); }

  web::WebTaskEnvironment web_task_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(DataSharingServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitService(/*enable_feature=*/true);
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitService(/*enable_feature=*/false);
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest,
       FeatureEnabledUsesEmptyServiceInIncognito) {
  InitService(/*enable_feature=*/true);
  raw_ptr<ProfileIOS> otr_profile =
      profile_->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              DataSharingServiceFactory::GetInstance(),
              DataSharingServiceFactory::GetDefaultFactory()}});
  DataSharingService* service =
      DataSharingServiceFactory::GetForProfile(otr_profile);
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace data_sharing
