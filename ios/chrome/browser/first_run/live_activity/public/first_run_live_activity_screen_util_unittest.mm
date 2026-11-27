// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/live_activity/public/first_run_live_activity_screen_util.h"

#import <algorithm>
#import <memory>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"
#import "ios/chrome/browser/first_run/live_activity/public/first_run_live_activity_constants.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

namespace {

// Creates a `regional_capabilities::RegionalCapabilitiesService` backed by a
// fake client configured for `country_id`.
std::unique_ptr<KeyedService> BuildRegionalCapabilitiesServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS& profile) {
  return regional_capabilities::CreateServiceWithFakeClient(*profile.GetPrefs(),
                                                            country_id);
}

// Builds a `ScopedFeatureList` pinning the baseline FRE feature flags before
// `WebTaskEnvironment` starts worker threads.
base::test::ScopedFeatureList CreateBaselineFeatureList() {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{kBestOfAppFRE, {{kBestOfAppFREVariantParam.name, "1"}}}},
      {first_run::kBestFeaturesScreenInFirstRun, first_run::kFREAblation});
  return feature_list;
}

class FirstRunLiveActivityScreenUtilTest : public PlatformTest {
 protected:
  // Creates a `TestProfileIOS` configured with a mock Chrome policy service,
  // the default `TemplateURLService`, and a fake `RegionalCapabilitiesService`
  // for `country_code`.
  std::unique_ptr<TestProfileIOS> CreateProfile(std::string country_code) {
    auto policy_service = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service, GetPolicies(::testing::Eq(policy::PolicyNamespace(
                                 policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));

    TestProfileIOS::Builder builder;
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        std::move(policy_service), &schema_registry_));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::RegionalCapabilitiesServiceFactory::GetInstance(),
        base::BindOnce(&BuildRegionalCapabilitiesServiceWithFakeClient,
                       country_codes::CountryId(country_code)));
    return std::move(builder).Build();
  }

  // Must be declared before `task_environment_` so feature overrides are set
  // before worker threads start and torn down after they join.
  base::test::ScopedFeatureList scoped_feature_list_ =
      CreateBaselineFeatureList();
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;
};

// Tests that `ScreenTypeForLiveActivityStep` maps milestones 1 through 4 back
// to their corresponding `ScreenType` and returns `std::nullopt` for
// `FirstRunLiveActivityStep::kNone`.
TEST_F(FirstRunLiveActivityScreenUtilTest,
       ScreenTypeForLiveActivityStepMapping) {
  EXPECT_EQ(ScreenTypeForLiveActivityStep(FirstRunLiveActivityStep::kSignIn),
            ScreenType::kSignIn);
  EXPECT_EQ(
      ScreenTypeForLiveActivityStep(FirstRunLiveActivityStep::kHistorySync),
      ScreenType::kHistorySync);
  EXPECT_EQ(ScreenTypeForLiveActivityStep(
                FirstRunLiveActivityStep::kDefaultBrowserPromo),
            ScreenType::kDefaultBrowserPromo);
  EXPECT_EQ(ScreenTypeForLiveActivityStep(
                FirstRunLiveActivityStep::kLensInteractivePromo),
            ScreenType::kLensInteractivePromo);
  EXPECT_EQ(ScreenTypeForLiveActivityStep(FirstRunLiveActivityStep::kNone),
            std::nullopt);
}

// Tests that the default baseline `FirstRunScreenProvider` screen sequence is
// eligible for the FRE Live Activity when a device identity is present.
TEST_F(FirstRunLiveActivityScreenUtilTest,
       DefaultBaselineSequenceIsLiveActivityEligibleWithDeviceIdentity) {
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);

  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];

  std::vector<ScreenType> user_facing_screens = provider.userFacingScreens;
  EXPECT_EQ(
      std::ranges::count(user_facing_screens, ScreenType::kStepsCompleted), 0);
  const std::vector<ScreenType> iphone_baseline_screens = {
      ScreenType::kSignIn,
      ScreenType::kHistorySync,
      ScreenType::kDefaultBrowserPromo,
      ScreenType::kLensInteractivePromo,
  };
  const std::vector<ScreenType> ipad_baseline_screens = {
      ScreenType::kSignIn,
      ScreenType::kHistorySync,
      ScreenType::kDefaultBrowserPromo,
  };
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    EXPECT_EQ(user_facing_screens.size(), 3u);
    EXPECT_TRUE(IsFreLiveActivityEligibleForScreens(ipad_baseline_screens));
    EXPECT_FALSE(IsFreLiveActivityEligibleForScreens(iphone_baseline_screens));
  } else {
    EXPECT_EQ(user_facing_screens.size(), 4u);
    EXPECT_TRUE(IsFreLiveActivityEligibleForScreens(iphone_baseline_screens));
    EXPECT_FALSE(IsFreLiveActivityEligibleForScreens(ipad_baseline_screens));
  }
  EXPECT_TRUE(IsFreLiveActivityEligibleForScreens(user_facing_screens));
}

// Tests that a zero-identity `FirstRunScreenProvider` sequence (which omits
// `ScreenType::kSignIn` and `ScreenType::kHistorySync`) is ineligible.
TEST_F(FirstRunLiveActivityScreenUtilTest,
       ZeroIdentitySequenceIsNotLiveActivityEligible) {
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* zero_identity_provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  EXPECT_FALSE(IsFreLiveActivityEligibleForScreens(
      zero_identity_provider.userFacingScreens));
}

// Tests that a `FirstRunScreenProvider` sequence containing the search engine
// choice screen (`ScreenType::kChoice`) is ineligible.
TEST_F(FirstRunLiveActivityScreenUtilTest,
       ChoiceScreenSequenceIsNotLiveActivityEligible) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  std::unique_ptr<TestProfileIOS> choice_profile = CreateProfile("BE");
  FirstRunScreenProvider* choice_provider =
      [[FirstRunScreenProvider alloc] initForProfile:choice_profile.get()];
  const std::vector<ScreenType> choice_screens =
      choice_provider.userFacingScreens;
  ASSERT_TRUE(std::ranges::contains(choice_screens, ScreenType::kSignIn));
  ASSERT_TRUE(std::ranges::contains(choice_screens, ScreenType::kChoice));
  EXPECT_FALSE(IsFreLiveActivityEligibleForScreens(choice_screens));
}

// Tests that reordered, empty, and unsupported screen sequences are ineligible.
TEST_F(FirstRunLiveActivityScreenUtilTest,
       ReorderedAndUnsupportedSequencesAreNotLiveActivityEligible) {
  const std::vector<ScreenType> kIneligibleSequences[] = {
      {ScreenType::kDefaultBrowserPromo, ScreenType::kSignIn,
       ScreenType::kHistorySync, ScreenType::kLensInteractivePromo},
      {ScreenType::kDefaultBrowserPromo, ScreenType::kSignIn,
       ScreenType::kHistorySync},
      {ScreenType::kSignIn, ScreenType::kHistorySync,
       ScreenType::kDefaultBrowserPromo, ScreenType::kBestFeatures},
      {ScreenType::kSignIn, ScreenType::kHistorySync,
       ScreenType::kBestFeatures},
      {ScreenType::kSignIn, ScreenType::kHistorySync,
       ScreenType::kDefaultBrowserPromo, ScreenType::kLensAnimatedPromo},
      {},
  };
  for (const std::vector<ScreenType>& sequence : kIneligibleSequences) {
    EXPECT_FALSE(IsFreLiveActivityEligibleForScreens(sequence));
  }
}

}  // namespace
