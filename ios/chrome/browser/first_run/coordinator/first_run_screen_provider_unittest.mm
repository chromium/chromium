// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_screen_provider.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/command_line.h"
#import "base/feature_list.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/field_trial_params.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/prefs/pref_service.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_switches.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

std::unique_ptr<KeyedService> BuildRegionalCapabilitiesServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS& profile) {
  return regional_capabilities::CreateServiceWithFakeClient(*profile.GetPrefs(),
                                                            country_id);
}

class FirstRunScreenProviderTest : public PlatformTest {
 protected:
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
        base::BindOnce(&BuildRegionalCapabilitiesServiceWithFakeClient,
                       country_codes::CountryId(country_code)));
    return std::move(builder).Build();
  }

  // Extracts the entire sequence of screens from the provider until steps are
  // completed.
  std::vector<ScreenType> GetScreensSequence(FirstRunScreenProvider* provider) {
    std::vector<ScreenType> screens;
    ScreenType type = [provider nextScreenType];
    while (type != ScreenType::kStepsCompleted) {
      screens.push_back(type);
      type = [provider nextScreenType];
    }
    return screens;
  }

  // Adds a fake identity on the device without signing in.
  void AddIdentityOnDevice() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  }

  // Reads the ordered list of deferred `ScreenType` values from `LocalState`.
  std::vector<ScreenType> GetDeferredScreens() {
    std::vector<ScreenType> deferred_screens;
    const base::ListValue& list =
        GetApplicationContext()->GetLocalState()->GetList(
            prefs::kFirstRunDeferredScreens);
    for (const base::Value& value : list) {
      deferred_screens.push_back(static_cast<ScreenType>(value.GetInt()));
    }
    return deferred_screens;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList feature_list_;
  policy::SchemaRegistry schema_registry_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
};

// Tests that when the search engine choice screen is required, the sequence
// starts with kSignIn, kHistorySync, and kChoice even without device
// identities.
TEST_F(FirstRunScreenProviderTest, ChoiceScreenEligibleShowsSignInAndChoice) {
  // Force choice screen eligibility.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("BE");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that the sequence starts with `kSignIn`, `kHistorySync`, then
  // `kChoice`.
  ASSERT_GE(screens.size(), 3u);
  EXPECT_EQ(screens[0], ScreenType::kSignIn);
  EXPECT_EQ(screens[1], ScreenType::kHistorySync);
  EXPECT_EQ(screens[2], ScreenType::kChoice);
}

// Tests that when the search engine choice screen is NOT required (e.g., in US
// region) and there are no device identities, sign-in is skipped and
// kDefaultBrowserPromo is the first screen.
TEST_F(FirstRunScreenProviderTest, NoIdentitiesSkipsSignIn) {
  // Create a profile in the US (non-choice eligible).
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that `kChoice` is NOT included.
  EXPECT_FALSE(std::ranges::contains(screens, ScreenType::kChoice));

  // Assert that `kDefaultBrowserPromo` is the first screen.
  ASSERT_FALSE(screens.empty());
  EXPECT_EQ(screens.front(), ScreenType::kDefaultBrowserPromo);
}

// Tests that `kFREAblation` is not evaluated (leaving its Finch trial inactive)
// and any stale deferred screens are cleared when there are no identities on
// the device.
TEST_F(FirstRunScreenProviderTest,
       FREAblationNotEvaluatedWhenNoIdentitiesOnDevice) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipAllExceptSignInAndChoice}}}},
      {kBestOfAppFRE});
  base::FieldTrial* trial =
      base::FeatureList::GetFieldTrial(first_run::kFREAblation);
  ASSERT_NE(trial, nullptr);

  // Seed a stale deferred screen to verify interrupted First Run state is
  // cleared when First Run restarts without identities.
  base::ListValue stale_deferred_screens;
  stale_deferred_screens.Append(static_cast<int>(ScreenType::kHistorySync));
  GetApplicationContext()->GetLocalState()->SetList(
      prefs::kFirstRunDeferredScreens, std::move(stale_deferred_screens));

  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
  EXPECT_THAT(screens,
              ::testing::ElementsAre(ScreenType::kDefaultBrowserPromo));
  EXPECT_TRUE(GetDeferredScreens().empty());
}

// Tests that when an identity exists on the device and `kFREAblation` is
// disabled, the Finch trial activates for Control and no screens are skipped or
// deferred.
TEST_F(FirstRunScreenProviderTest,
       FREAblationDisabledWithIdentityActivatesTrialAndKeepsAllScreens) {
  base::test::ScopedFeatureList isolated_trial_scope;
  isolated_trial_scope.InitWithEmptyFeatureAndFieldTrialLists();

  scoped_refptr<base::FieldTrial> trial =
      base::FieldTrialList::CreateFieldTrial("FREAblationControlTrial",
                                             "Control");
  ASSERT_NE(trial, nullptr);
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_list->RegisterFieldTrialOverride(
      first_run::kFREAblation.name, base::FeatureList::OVERRIDE_DISABLE_FEATURE,
      trial.get());
  feature_list->RegisterExtraFeatureOverrides(
      {{kBestOfAppFRE, base::FeatureList::OVERRIDE_DISABLE_FEATURE}});
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  AddIdentityOnDevice();
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
  EXPECT_THAT(screens, ::testing::ElementsAre(
                           ScreenType::kSignIn, ScreenType::kHistorySync,
                           ScreenType::kDefaultBrowserPromo));
  EXPECT_TRUE(GetDeferredScreens().empty());
}

// Tests that the `kSkipHistorySync` arm skips only `kHistorySync` during First
// Run and records `kHistorySync` in `prefs::kFirstRunDeferredScreens`.
TEST_F(FirstRunScreenProviderTest,
       FREAblationSkipHistorySyncDefersOnlyHistorySync) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipHistorySync}}}},
      {kBestOfAppFRE});

  AddIdentityOnDevice();
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_THAT(screens,
              ::testing::ElementsAre(ScreenType::kSignIn,
                                     ScreenType::kDefaultBrowserPromo));
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kHistorySync));
}

// Tests that the `kSkipAllExceptSignInAndChoice` arm in a non-EEA region keeps
// only `kSignIn` in First Run and records all skipped screens in sequence order
// in `prefs::kFirstRunDeferredScreens`.
TEST_F(FirstRunScreenProviderTest,
       FREAblationSkipAllExceptSignInAndChoiceInNonEEA) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipAllExceptSignInAndChoice}}},
       {kBestOfAppFRE, {{kBestOfAppFREVariantParam.name, "2"}}}},
      {});

  AddIdentityOnDevice();
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_THAT(screens, ::testing::ElementsAre(ScreenType::kSignIn));
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kHistorySync,
                                     ScreenType::kDefaultBrowserPromo,
                                     ScreenType::kLensAnimatedPromo));
}

// Tests that the `kSkipAllExceptSignInAndChoice` arm preserves both `kSignIn`
// and `kChoice` when the Search Engine Choice screen is required, and defers
// the remaining screens in order.
TEST_F(FirstRunScreenProviderTest,
       FREAblationSkipAllExceptSignInAndChoicePreservesChoiceInEEA) {
  feature_list_.InitWithFeaturesAndParameters(
      {{first_run::kFREAblation,
        {{first_run::kFREAblationParam,
          first_run::kFREAblationParamSkipAllExceptSignInAndChoice}}}},
      {kBestOfAppFRE});
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);

  AddIdentityOnDevice();
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("BE");
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  EXPECT_THAT(screens,
              ::testing::ElementsAre(ScreenType::kSignIn, ScreenType::kChoice));
  EXPECT_THAT(GetDeferredScreens(),
              ::testing::ElementsAre(ScreenType::kHistorySync,
                                     ScreenType::kDefaultBrowserPromo));
}

// Tests that when the search engine choice screen is NOT required and there
// are identities on the device, the sequence starts with kSignIn and
// kHistorySync, followed by kDefaultBrowserPromo.
TEST_F(FirstRunScreenProviderTest, WithIdentitiesShowsSignInBeforeDBPromo) {
  AddIdentityOnDevice();

  // Create a profile in the US (non-choice eligible).
  std::unique_ptr<TestProfileIOS> profile = CreateProfile("US");

  // Get the screen sequence from the provider.
  FirstRunScreenProvider* provider =
      [[FirstRunScreenProvider alloc] initForProfile:profile.get()];
  std::vector<ScreenType> screens = GetScreensSequence(provider);

  // Assert that `kChoice` is NOT included.
  EXPECT_FALSE(std::ranges::contains(screens, ScreenType::kChoice));

  // Assert that the sequence starts with `kSignIn`, `kHistorySync`, then
  // `kDefaultBrowserPromo`.
  ASSERT_GE(screens.size(), 3u);
  EXPECT_EQ(screens[0], ScreenType::kSignIn);
  EXPECT_EQ(screens[1], ScreenType::kHistorySync);
  EXPECT_EQ(screens[2], ScreenType::kDefaultBrowserPromo);
}

}  // namespace
