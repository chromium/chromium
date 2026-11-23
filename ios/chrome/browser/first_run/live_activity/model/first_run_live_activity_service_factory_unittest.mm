// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/model/first_run_live_activity_service_factory.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/first_run/live_activity/public/features.h"
#import "ios/chrome/browser/live_activity/model/ios_chrome_live_activity_manager_factory.h"
#import "ios/chrome/browser/live_activity/public/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Unit tests for `FirstRunLiveActivityServiceFactory`.
class FirstRunLiveActivityServiceFactoryTest : public PlatformTest {
 protected:
  // Builds a `TestProfileIOS` with the requested default factories registered.
  std::unique_ptr<TestProfileIOS> BuildProfile(
      bool register_manager_factory = true,
      bool register_service_factory = true) {
    TestProfileIOS::Builder builder;
    if (register_manager_factory) {
      builder.AddTestingFactory(
          IOSChromeLiveActivityManagerFactory::GetInstance(),
          IOSChromeLiveActivityManagerFactory::GetDefaultFactory());
    }
    if (register_service_factory) {
      builder.AddTestingFactory(
          FirstRunLiveActivityServiceFactory::GetInstance(),
          FirstRunLiveActivityServiceFactory::GetDefaultFactory());
    }
    return std::move(builder).Build();
  }

  web::WebTaskEnvironment task_environment_;
};

// Tests that `FirstRunLiveActivityServiceFactory` creates no service by default
// in unit tests unless `GetDefaultFactory()` is explicitly registered, even
// when `IOSChromeLiveActivityManager` is available.
TEST_F(FirstRunLiveActivityServiceFactoryTest,
       ReturnsNullByDefaultInTestsWithoutTestingFactory) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kIOSLiveActivityManager, kFreLiveActivity}, {});

  std::unique_ptr<TestProfileIOS> profile =
      BuildProfile(/*register_manager_factory=*/true,
                   /*register_service_factory=*/false);
  ASSERT_NE(IOSChromeLiveActivityManagerFactory::GetForProfile(*profile),
            nullptr);
  EXPECT_EQ(FirstRunLiveActivityServiceFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `FirstRunLiveActivityServiceFactory` returns `nullptr` when
// `kIOSLiveActivityManager` is disabled.
TEST_F(FirstRunLiveActivityServiceFactoryTest,
       ReturnsNullWhenManagerFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kFreLiveActivity},
                                       {kIOSLiveActivityManager});

  std::unique_ptr<TestProfileIOS> profile = BuildProfile();
  EXPECT_EQ(FirstRunLiveActivityServiceFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `FirstRunLiveActivityServiceFactory` returns `nullptr` when
// `kFreLiveActivity` is disabled.
TEST_F(FirstRunLiveActivityServiceFactoryTest,
       ReturnsNullWhenFreFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kIOSLiveActivityManager},
                                       {kFreLiveActivity});

  std::unique_ptr<TestProfileIOS> profile = BuildProfile();
  EXPECT_EQ(FirstRunLiveActivityServiceFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `FirstRunLiveActivityServiceFactory` returns `nullptr` when
// `IOSChromeLiveActivityManager` is unavailable for the profile.
TEST_F(FirstRunLiveActivityServiceFactoryTest,
       ReturnsNullWhenLiveActivityManagerUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kIOSLiveActivityManager, kFreLiveActivity}, {});

  std::unique_ptr<TestProfileIOS> profile =
      BuildProfile(/*register_manager_factory=*/false,
                   /*register_service_factory=*/true);
  EXPECT_EQ(FirstRunLiveActivityServiceFactory::GetForProfile(*profile),
            nullptr);
}

// Tests that `FirstRunLiveActivityServiceFactory` creates an instance eagerly
// for a regular profile when both flags are enabled and returns `nullptr` for
// an incognito profile even when both default factories are registered.
TEST_F(FirstRunLiveActivityServiceFactoryTest,
       ReturnsInstanceForRegularProfileAndNullForIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {kIOSLiveActivityManager, kFreLiveActivity}, {});

  std::unique_ptr<TestProfileIOS> profile = BuildProfile();
  EXPECT_NE(FirstRunLiveActivityServiceFactory::GetForProfileIfExists(*profile),
            nullptr);
  EXPECT_NE(FirstRunLiveActivityServiceFactory::GetForProfile(*profile),
            nullptr);

  TestProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
               IOSChromeLiveActivityManagerFactory::GetInstance(),
               IOSChromeLiveActivityManagerFactory::GetDefaultFactory()},
           TestProfileIOS::TestingFactory{
               FirstRunLiveActivityServiceFactory::GetInstance(),
               FirstRunLiveActivityServiceFactory::GetDefaultFactory()}});
  EXPECT_EQ(FirstRunLiveActivityServiceFactory::GetForProfile(*otr_profile),
            nullptr);
}

}  // namespace
