// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import <optional>
#import <string>
#import <vector>

#import "base/check_deref.h"
#import "base/command_line.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/regional_capabilities/regional_capabilities_metrics.h"
#import "components/regional_capabilities/regional_capabilities_switches.h"
#import "components/regional_capabilities/regional_capabilities_test_utils.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/variations/variations_switches.h"
#import "ios/chrome/browser/policy/model/profile_policy_connector_mock.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using search_engines::SearchEngineChoiceScreenConditions;

namespace {

std::unique_ptr<KeyedService> BuildRegionalCapabilitiesServiceWithFakeClient(
    country_codes::CountryId country_id,
    ProfileIOS* profile) {
  return regional_capabilities::CreateServiceWithFakeClient(
      *profile->GetPrefs(), country_id);
}

}  // namespace

class SearchEngineChoiceUtilTest : public PlatformTest {
 public:
  SearchEngineChoiceUtilTest() : SearchEngineChoiceUtilTest("BE") {}

  SearchEngineChoiceUtilTest(std::string_view country_code)
      : country_code_(country_code) {
    // Override the country checks to simulate being in a specific country.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, country_code_);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    InitMockPolicyService();
    TestProfileIOS::Builder builder;
    builder.SetPolicyConnector(std::make_unique<ProfilePolicyConnectorMock>(
        std::move(policy_service_), &schema_registry_));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::RegionalCapabilitiesServiceFactory::GetInstance(),
        base::BindRepeating(&BuildRegionalCapabilitiesServiceWithFakeClient,
                            country_codes::CountryId(country_code_)));
    profile_ = std::move(builder).Build();
    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceSearchEngineChoiceScreen);
  }

  TestProfileIOS& profile() { return *profile_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  TemplateURLService& template_url_service() { return *template_url_service_; }

 private:
  void InitMockPolicyService() {
    policy_service_ = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  base::HistogramTester histogram_tester_;
  web::WebTaskEnvironment task_environment_;
  policy::SchemaRegistry schema_registry_;
  std::unique_ptr<TestProfileIOS> profile_;
  // Owned by profile_.
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;

  std::string country_code_;
};

TEST_F(SearchEngineChoiceUtilTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/false));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Reported",
      regional_capabilities::FunnelStage::kEligible, 1);
}

TEST_F(SearchEngineChoiceUtilTest,
       ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntent) {
  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Reported",
      regional_capabilities::FunnelStage::kNotEligible, 1);
}

TEST_F(
    SearchEngineChoiceUtilTest,
    ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntentAlmostMaxCount) {
  ASSERT_GT(kSearchEngineChoiceMaximumSkipCount, 0);
  PrefService* pref_service = profile().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      kSearchEngineChoiceMaximumSkipCount - 1);

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kAppStartedByExternalIntent, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Reported",
      regional_capabilities::FunnelStage::kNotEligible, 1);
}

TEST_F(
    SearchEngineChoiceUtilTest,
    ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntentMaxCountReached) {
  PrefService* pref_service = profile().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      kSearchEngineChoiceMaximumSkipCount);

  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Reported",
      regional_capabilities::FunnelStage::kEligible, 1);
}

TEST_F(SearchEngineChoiceUtilTest,
       DoNotShowChoiceScreenIfUserHasNonGoogleSearchEngineSetAsDefault) {
  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/false));
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      SearchEngineChoiceScreenConditions::kHasCustomSearchEngine, 1);
  histogram_tester().ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      SearchEngineChoiceScreenConditions::kHasCustomSearchEngine, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kHasCustomSearchEngine, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Reported",
      regional_capabilities::FunnelStage::kNotEligible, 1);
}

struct TayiakiTestParam {
  std::string test_name;
  // see switches::RegionalCapabilitiesChoiceScreenSurface
  std::optional<std::string> allowed_surface;
  // Triggering surface that calls ShouldDisplaySearchEngineChoiceScreen
  bool is_first_run_entrypoint;
  // Whether the choice screen is expected to be shown
  bool expected_should_display_choice_screen_output;
};

// Verifies if the search engine choice screen is shown for Tayiaki in various
// flag configurations.
class SearchEngineChoiceUtilShouldDisplayChoiceScreenTest
    : public SearchEngineChoiceUtilTest,
      public ::testing::WithParamInterface<TayiakiTestParam> {
 public:
  SearchEngineChoiceUtilShouldDisplayChoiceScreenTest()
      : SearchEngineChoiceUtilTest("JP") {
    if (GetParam().allowed_surface.has_value()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          switches::kTaiyaki,
          {{"choice_screen_surface", *GetParam().allowed_surface}});
    } else {
      feature_list_.InitAndDisableFeature(switches::kTaiyaki);
    }
  }

  void SetUp() override {
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_PHONE) {
      GTEST_SKIP() << "Tayiaki is only enabled for phone form factors.";
    }
    SearchEngineChoiceUtilTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SearchEngineChoiceUtilShouldDisplayChoiceScreenTest,
       SurfacesChoiceScreen) {
  EXPECT_EQ(GetParam().expected_should_display_choice_screen_output,
            ShouldDisplaySearchEngineChoiceScreen(
                profile(), GetParam().is_first_run_entrypoint,
                /*app_started_via_external_intent=*/false));

  // Use histogram tester to verify the exact reason for not showing the
  // choice screen.
  if (!GetParam().allowed_surface.has_value()) {
    // Disabled Taiyaki case:
    histogram_tester().ExpectUniqueSample(
        "RegionalCapabilities.FunnelStage.Eligibility",
        SearchEngineChoiceScreenConditions::kNotInRegionalScope, 1);
    return;
  }

  if (GetParam().allowed_surface == "fre_only" &&
      !GetParam().is_first_run_entrypoint) {
    // Tayiaki is only enabled for FRE entrypoint, but choice screen was called
    // from non-FRE entrypoint.
    histogram_tester().ExpectUniqueSample(
        "RegionalCapabilities.FunnelStage.Triggering",
        SearchEngineChoiceScreenConditions::kIneligibleSurface, 1);
    histogram_tester().ExpectUniqueSample(
        "RegionalCapabilities.FunnelStage.Eligibility",
        SearchEngineChoiceScreenConditions::kEligible, 1);
    return;
  }

  // Tayiaki is enabled either everywhere or only for FRE, but the choice screen
  // was called from the allowed entrypoint.
  CHECK(GetParam().allowed_surface == "all" ||
        (GetParam().allowed_surface == "fre_only" &&
         GetParam().is_first_run_entrypoint));
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Eligibility",
      SearchEngineChoiceScreenConditions::kEligible, 1);
  histogram_tester().ExpectUniqueSample(
      "RegionalCapabilities.FunnelStage.Triggering",
      SearchEngineChoiceScreenConditions::kEligible, 1);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceUtilShouldDisplayChoiceScreenTest,
    ::testing::ValuesIn<TayiakiTestParam>({
        {.test_name = "TayiakiEverywhere_FirstRunSurface",
         .allowed_surface = "all",
         .is_first_run_entrypoint = true,
         .expected_should_display_choice_screen_output = true},
        {.test_name = "TayiakiEverywhere_NonFirstRunSurface",
         .allowed_surface = "all",
         .is_first_run_entrypoint = false,
         .expected_should_display_choice_screen_output = true},
        {.test_name = "TaiyakiInFirstRunOnly_FirstRunSurface",
         .allowed_surface = "fre_only",
         .is_first_run_entrypoint = true,
         .expected_should_display_choice_screen_output = true},
        {.test_name = "TaiyakiInFirstRunOnly_NonFirstRunSurface",
         .allowed_surface = "fre_only",
         .is_first_run_entrypoint = false,
         .expected_should_display_choice_screen_output = false},
        {.test_name = "TayiakiDisabled_FirstRunSurface",
         .allowed_surface = std::nullopt,
         .is_first_run_entrypoint = true,
         .expected_should_display_choice_screen_output = false},
        {.test_name = "TayiakiDisabled_NonFirstRunSurface",
         .allowed_surface = std::nullopt,
         .is_first_run_entrypoint = false,
         .expected_should_display_choice_screen_output = false},
    }),
    [](const ::testing::TestParamInfo<TayiakiTestParam>& info) {
      return info.param.test_name;
    });
