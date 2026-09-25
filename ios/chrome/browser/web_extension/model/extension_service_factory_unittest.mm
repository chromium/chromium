// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service_factory.h"

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/time/time.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/universal_optout/features.h"
#import "components/universal_optout/prefs.h"
#import "components/universal_optout/universal_optout_service.h"
#import "components/variations/service/test_variations_service.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/universal_optout/model/universal_optout_service_factory.h"
#import "ios/chrome/browser/web_extension/model/extension_service.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Returns whether the current OS version is supported. The GPC extension is
// only supported on iOS versions between iOS 18.4 and iOS 27.
bool IsSupportedOS() {
  if (@available(iOS 18.4, *)) {
    return !base::ios::IsRunningOnIOS27OrLater();
  }
  return false;
}

class ExtensionServiceFactoryTest : public PlatformTest {
 public:
  ExtensionServiceFactoryTest() {
    universal_optout::prefs::RegisterProfilePrefs(pref_service_.registry());
    variations::TestVariationsService::RegisterPrefs(pref_service_.registry());

    enabled_state_provider_ =
        std::make_unique<metrics::TestEnabledStateProvider>(/*consent=*/true,
                                                            /*enabled=*/true);
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        &pref_service_, enabled_state_provider_.get(),
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);

    variations_service_ = std::make_unique<variations::TestVariationsService>(
        &pref_service_, metrics_state_manager_.get());

    base::Time start_time;
    CHECK(base::Time::FromString("2026-08-11T12:00:00Z", &start_time));
    test_clock_.SetNow(start_time);
  }
  ~ExtensionServiceFactoryTest() override = default;

  std::unique_ptr<KeyedService> CreateOptOutService(bool eligible,
                                                    ProfileIOS* profile) {
    auto service = std::make_unique<universal_optout::UniversalOptOutService>(
        *profile->GetPrefs(), *variations_service_,
        *identity_test_env_.identity_manager(), test_clock_);
    profile->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEligible, eligible);
    return service;
  }

  std::unique_ptr<KeyedService> CreateOptedInOptOutService(
      bool eligible,
      ProfileIOS* profile) {
    auto service = CreateOptOutService(eligible, profile);
    profile->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEnabled, true);
    return service;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<metrics::TestEnabledStateProvider> enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::SimpleTestClock test_clock_;
};

// Tests that ExtensionService is not created if the extension flag is disabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenExtensionFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutSettings},
      /*disabled_features=*/{
          universal_optout::features::kUniversalOptOutExtension});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if kUniversalOptOut is disabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenUniversalOptOutFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{universal_optout::features::kUniversalOptOut});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if kUniversalOptOutSettings is
// disabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenSettingsFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{universal_optout::features::kUniversalOptOut,
                            universal_optout::features::
                                kUniversalOptOutExtension},
      /*disabled_features=*/{
          universal_optout::features::kUniversalOptOutSettings});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is not created if the user is not eligible.
TEST_F(ExtensionServiceFactoryTest, TestFactoryReturnsNullWhenIneligible) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/false));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is created when ineligible but the feature is
// enabled via preferences.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsServiceWhenIneligibleButOptedIn) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(
          &ExtensionServiceFactoryTest::CreateOptedInOptOutService,
          base::Unretained(this), /*eligible=*/false));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);
}

// Tests that ExtensionService is not created when eligible but forced off.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenEligibleButForcedOff) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  [[NSUserDefaults standardUserDefaults]
      setInteger:static_cast<NSInteger>(
                     experimental_flags::UniversalOptOutEligibilityOverride::
                         kForcedOff)
          forKey:@"UniversalOptOutEligibilityOverride"];

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:@"UniversalOptOutEligibilityOverride"];
}

// Tests that ExtensionService is created when ineligible but forced on.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsServiceWhenIneligibleButForcedOn) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  [[NSUserDefaults standardUserDefaults]
      setInteger:
          static_cast<NSInteger>(
              experimental_flags::UniversalOptOutEligibilityOverride::kForcedOn)
          forKey:@"UniversalOptOutEligibilityOverride"];

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/false));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:@"UniversalOptOutEligibilityOverride"];
}

// Tests that ExtensionService is not created if UniversalOptOutService is null.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsNullWhenOptOutServiceNull) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  // UniversalOptOutServiceFactory has kNoServiceForTests, so it returns null.
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

// Tests that ExtensionService is created when eligible and both flags are
// enabled.
TEST_F(ExtensionServiceFactoryTest,
       TestFactoryReturnsServiceWhenEligibleAndFlagsEnabled) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(service, nullptr);
}

// Tests that the service is redirected in incognito.
TEST_F(ExtensionServiceFactoryTest, TestFactoryRedirectedInIncognito) {
  if (!IsSupportedOS()) {
    GTEST_SKIP()
        << "ExtensionService is only created on supported OS versions (iOS "
           "18.4 to 26).";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  ExtensionService* regular_service =
      ExtensionServiceFactory::GetForProfile(profile.get());
  ExtensionService* otr_service =
      ExtensionServiceFactory::GetForProfile(profile->GetOffTheRecordProfile());
  EXPECT_NE(regular_service, nullptr);
  EXPECT_EQ(regular_service, otr_service);
}

// Tests that ExtensionService is not created on iOS 27 or later.
TEST_F(ExtensionServiceFactoryTest, TestFactoryReturnsNullOnIOS27OrLater) {
  if (!base::ios::IsRunningOnIOS27OrLater()) {
    GTEST_SKIP() << "Only runs on iOS 27 or later.";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutExtension,
       universal_optout::features::kUniversalOptOutSettings},
      /*disabled_features=*/{});
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(ExtensionServiceFactory::GetInstance(),
                            ExtensionServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      universal_optout::UniversalOptOutServiceFactory::GetInstance(),
      base::BindRepeating(&ExtensionServiceFactoryTest::CreateOptOutService,
                          base::Unretained(this), /*eligible=*/true));
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();
  EXPECT_EQ(ExtensionServiceFactory::GetForProfile(profile.get()), nullptr);
}

}  // namespace
