// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_sharing/model/data_sharing_service_factory.h"

#import <memory>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/data_sharing/public/features.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
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
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(DataSharingServiceFactory::GetInstance(),
                              DataSharingServiceFactory::GetDefaultFactory());
    browser_state_ = std::move(builder).Build();
  }

  void TearDown() override { web_task_env_.RunUntilIdle(); }

  web::WebTaskEnvironment web_task_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(DataSharingServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitService(/*enable_feature=*/true);
  DataSharingService* service =
      DataSharingServiceFactory::GetForBrowserState(browser_state_.get());
  EXPECT_FALSE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitService(/*enable_feature=*/false);
  DataSharingService* service =
      DataSharingServiceFactory::GetForBrowserState(browser_state_.get());
  EXPECT_TRUE(service->IsEmptyService());
}

TEST_F(DataSharingServiceFactoryTest,
       FeatureEnabledUsesEmptyServiceInIncognito) {
  InitService(/*enable_feature=*/true);
  raw_ptr<ChromeBrowserState> otr_browser_state =
      browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories(
          {TestChromeBrowserState::TestingFactory{
              DataSharingServiceFactory::GetInstance(),
              DataSharingServiceFactory::GetDefaultFactory()}});
  DataSharingService* service =
      DataSharingServiceFactory::GetForBrowserState(otr_browser_state);
  EXPECT_TRUE(service->IsEmptyService());
}

}  // namespace data_sharing
