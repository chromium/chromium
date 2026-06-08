// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import "base/command_line.h"
#import "base/test/scoped_feature_list.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildRegionalCapabilitiesServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS* profile) {
  return regional_capabilities::CreateServiceWithFakeClient(
      *profile->GetPrefs(), country_id);
}

class FirstRunScreenProviderTest : public PlatformTest {
 protected:
  void SetUp() override { PlatformTest::SetUp(); }

  std::unique_ptr<TestProfileIOS> CreateProfile(std::string country_code) {
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

  web::WebTaskEnvironment task_environment_;
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

}  // namespace
