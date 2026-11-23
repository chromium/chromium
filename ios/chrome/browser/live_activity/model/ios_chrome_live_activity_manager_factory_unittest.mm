// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager_factory.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/live_activity/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Unit tests for `IOSChromeLiveActivityManagerFactory`.
class IOSChromeLiveActivityManagerFactoryTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
};

// Tests that `IOSChromeLiveActivityManagerFactory` creates no service by
// default in unit tests unless `GetDefaultFactory()` is explicitly registered.
TEST_F(IOSChromeLiveActivityManagerFactoryTest,
       ReturnsNullByDefaultInTestsWithoutTestingFactory) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSLiveActivityManager);

  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();
  EXPECT_EQ(IOSChromeLiveActivityManagerFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `IOSChromeLiveActivityManagerFactory` returns `nullptr` when
// `kIOSLiveActivityManager` is disabled.
TEST_F(IOSChromeLiveActivityManagerFactoryTest,
       ReturnsNullWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kIOSLiveActivityManager);

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      IOSChromeLiveActivityManagerFactory::GetInstance(),
      IOSChromeLiveActivityManagerFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  EXPECT_EQ(IOSChromeLiveActivityManagerFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `IOSChromeLiveActivityManagerFactory` creates an instance eagerly
// for a regular profile when `kIOSLiveActivityManager` is enabled and returns
// `nullptr` for an incognito profile even when the default factory is
// registered.
TEST_F(IOSChromeLiveActivityManagerFactoryTest,
       ReturnsInstanceForRegularProfileAndNullForIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kIOSLiveActivityManager);

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(
      IOSChromeLiveActivityManagerFactory::GetInstance(),
      IOSChromeLiveActivityManagerFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  EXPECT_NE(
      IOSChromeLiveActivityManagerFactory::GetForProfileIfExists(*profile),
      nullptr);
  EXPECT_NE(IOSChromeLiveActivityManagerFactory::GetForProfile(*profile),
            nullptr);

  TestProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              IOSChromeLiveActivityManagerFactory::GetInstance(),
              IOSChromeLiveActivityManagerFactory::GetDefaultFactory()}});
  EXPECT_EQ(IOSChromeLiveActivityManagerFactory::GetForProfile(*otr_profile),
            nullptr);
}

}  // namespace
