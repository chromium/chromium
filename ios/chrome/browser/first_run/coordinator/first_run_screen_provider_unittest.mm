// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import "base/command_line.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/public/features.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

const char kDeviceSwitcherUserSegmentPrefKey[] =
    "segmentation_platform.device_switcher_util";

std::unique_ptr<KeyedService> BuildRegionalCapabilitiesServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS* profile) {
  return regional_capabilities::CreateServiceWithFakeClient(
      *profile->GetPrefs(), country_id);
}

std::unique_ptr<KeyedService> BuildSegmentationPlatformServiceWithLabels(
    std::optional<std::vector<std::string>> labels,
    ProfileIOS* profile) {
  if (labels.has_value()) {
    base::ListValue labels_list;
    for (const std::string& label : *labels) {
      labels_list.Append(label);
    }
    profile->GetPrefs()->SetDict(
        kDeviceSwitcherUserSegmentPrefKey,
        base::DictValue().Set(
            "result", base::DictValue().Set("labels", std::move(labels_list))));
  }
  return segmentation_platform::SegmentationPlatformServiceFactory::
      GetDefaultFactory()
          .Run(profile);
}

class FirstRunScreenProviderTest : public PlatformTest {
 protected:
  std::unique_ptr<TestProfileIOS> CreateProfile(
      std::string country_code,
      std::optional<std::vector<std::string>> device_switcher_labels =
          std::nullopt) {
    policy_service_ = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));

    TestProfileIOS::Builder builder;
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        std::move(policy_service_), &schema_registry_));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::RegionalCapabilitiesServiceFactory::GetInstance(),
        base::BindRepeating(&BuildRegionalCapabilitiesServiceWithFakeClient,
                            country_codes::CountryId(country_code)));
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        base::BindOnce(&BuildSegmentationPlatformServiceWithLabels,
                       std::move(device_switcher_labels)));
    return std::move(builder).Build();
  }

  // Extracts the entire sequence of screens from the provider until steps are
  // completed.
  std::vector<ScreenType> GetScreensSequence(FirstRunScreenProvider* provider) {
    std::vector<ScreenType> screens;
    ScreenType type = [provider nextScreenType];
    while (type != kStepsCompleted) {
      screens.push_back(type);
      type = [provider nextScreenType];
    }
    return screens;
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  policy::SchemaRegistry schema_registry_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
};

// Tests that when the search engine choice screen is required, any active
// UpdatedFirstRunSequence variation is disabled, and the sequence falls back to
// the standard control sequence (which includes kChoice).
TEST_F(FirstRunScreenProviderTest, FallbackToDisabledWhenChoiceScreenEligible) {
  feature_list_.InitAndEnableFeatureWithParameters(
      first_run::kUpdatedFirstRunSequence,
      {{"updated-first-run-sequence-param", "2"}});  // kRemoveSignInSync

  // Force choice screen eligibility.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("BE");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that kChoice is included in the sequence.
  EXPECT_TRUE(std::ranges::contains(screens, kChoice));

  // Assert that kSignIn is the first screen.
  ASSERT_FALSE(screens.empty());
  EXPECT_EQ(screens.front(), kSignIn);
}

// Tests that when the search engine choice screen is NOT required (e.g., in US
// region), the UpdatedFirstRunSequence variation remains active.
TEST_F(FirstRunScreenProviderTest, KeepUpdatedSequenceWhenNotChoiceEligible) {
  feature_list_.InitAndEnableFeatureWithParameters(
      first_run::kUpdatedFirstRunSequence,
      {{"updated-first-run-sequence-param", "2"}});  // kRemoveSignInSync

  // Create a profile in the US (non-choice eligible).
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that kChoice is NOT included.
  EXPECT_FALSE(std::ranges::contains(screens, kChoice));

  // Assert that kDefaultBrowserPromo is the first screen.
  ASSERT_FALSE(screens.empty());
  EXPECT_EQ(screens.front(), kDefaultBrowserPromo);
}

// Tests that the Default Browser Promo is kept when the sign-in screen is
// removed.
TEST_F(FirstRunScreenProviderTest, DBPromoKeptWhenSignInRemoved) {
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{first_run::kSkipDefaultBrowserPromoInFirstRun, {}},
                            {first_run::kUpdatedFirstRunSequence,
                             {{"updated-first-run-sequence-param",
                               "2"}}}},  // kRemoveSignInSync
      /*disabled_features=*/{});

  // Create a profile in the EEA (where DB promo is usually skipped).
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("BE");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that kSignIn is NOT included (because there are no identities).
  EXPECT_FALSE(std::ranges::contains(screens, kSignIn));

  // Assert that kDefaultBrowserPromo IS included because kSignIn was removed.
  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
}

// Tests that the Default Browser Promo is removed when the sign-in screen is
// present.
TEST_F(FirstRunScreenProviderTest, DBPromoRemovedWhenSignInPresent) {
  feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/{{first_run::kSkipDefaultBrowserPromoInFirstRun, {}},
                            {first_run::kUpdatedFirstRunSequence,
                             {{"updated-first-run-sequence-param",
                               "1"}}}},  // kDBPromoFirst
      /*disabled_features=*/{});

  // Create a profile in the EEA (where DB promo is usually skipped).
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("BE");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that kSignIn IS included.
  EXPECT_TRUE(std::ranges::contains(screens, kSignIn));

  // Assert that kDefaultBrowserPromo is NOT included (skipped).
  EXPECT_FALSE(std::ranges::contains(screens, kDefaultBrowserPromo));
}

// Tests that when the user is segmented as an Android switcher, the
// kAndroidSwitcher result and success latency are recorded on UMA without
// hiding the Default Browser promo.
TEST_F(FirstRunScreenProviderTest,
       RecordsAndroidSwitcherSegmentationResultAndLatency) {
  feature_list_.InitAndEnableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile(
      "US",
      std::vector<std::string>{
          segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 0);
}

// Tests that when the user has both Android phone and iOS phone Chrome labels,
// the user is classified as kNotAndroidSwitcher (matching Bring Your Tabs).
TEST_F(FirstRunScreenProviderTest,
       RecordsNotAndroidSwitcherWhenIosPhoneLabelPresent) {
  feature_list_.InitAndEnableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile(
      "US",
      std::vector<std::string>{
          segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
          segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kNotAndroidSwitcher, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 0);
}

// Tests that when the segmentation request times out after 60 seconds,
// kNotReady and the 60-second failure latency are recorded on UMA.
TEST_F(FirstRunScreenProviderTest, RecordsNotReadySegmentationResultOnTimeout) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{first_run::kQueryDeviceSwitcherSignalsInFirstRun},
      /*disabled_features=*/{
          segmentation_platform::features::kSegmentationPlatformDeviceSwitcher,
          segmentation_platform::features::kSegmentationPlatformUmaFromSqlDb});
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);
  task_environment_.FastForwardBy(base::Seconds(60));

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 0);
  histogram_tester.ExpectUniqueTimeSample(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram,
      base::Seconds(60), 1);
}

// Tests that when the device switcher model times out waiting for sync device
// info (returning kNotSyncedLabel), kNotReady and failure latency are recorded
// on UMA.
TEST_F(FirstRunScreenProviderTest,
       RecordsNotReadySegmentationResultWhenNotSyncedLabelPresent) {
  feature_list_.InitAndEnableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile(
      "US", std::vector<std::string>{
                segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 1);
}

// Tests that when the segmentation result has no labels (failed
// classification), kFailed and failure latency are recorded on UMA.
TEST_F(FirstRunScreenProviderTest,
       RecordsFailedSegmentationResultWhenLabelsEmpty) {
  feature_list_.InitAndEnableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile =
      CreateProfile("US", std::vector<std::string>{});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kFailed, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 1);
}

// Tests that when the forced device switcher experimental flag is set, no
// segmentation metrics are recorded on UMA.
TEST_F(FirstRunScreenProviderTest,
       DoesNotRecordSegmentationMetricsWhenExperimentalFlagSet) {
  feature_list_.InitAndEnableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "force-device-switcher-experience",
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile(
      "US",
      std::vector<std::string>{
          segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 0);
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      "force-device-switcher-experience");
}

// Tests that when kQueryDeviceSwitcherSignalsInFirstRun is disabled, the
// segmentation dispatcher is not queried and no metrics are recorded.
TEST_F(FirstRunScreenProviderTest,
       DoesNotRecordSegmentationMetricsWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      first_run::kQueryDeviceSwitcherSignalsInFirstRun);
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestProfileIOS> profile = CreateProfile(
      "US",
      std::vector<std::string>{
          segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(std::ranges::contains(screens, kDefaultBrowserPromo));
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram, 0);
}

}  // namespace
