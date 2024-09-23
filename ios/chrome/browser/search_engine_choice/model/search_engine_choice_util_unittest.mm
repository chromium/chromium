// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "base/check_deref.h"
#import "base/command_line.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector_mock.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class SearchEngineChoiceUtilTest : public PlatformTest {
 public:
  SearchEngineChoiceUtilTest() {
    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    InitMockPolicyService();
    TestProfileIOS::Builder builder;
    builder.SetPolicyConnector(
        std::make_unique<BrowserStatePolicyConnectorMock>(
            std::move(policy_service_), &schema_registry_));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceSearchEngineChoiceScreen);
  }

  TestProfileIOS& profile() { return *profile_; }

  base::HistogramTester histogram_tester_;

  TemplateURLService& template_url_service() { return *template_url_service_; }

 private:
  void InitMockPolicyService() {
    policy_service_ = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  web::WebTaskEnvironment task_environment_;
  policy::SchemaRegistry schema_registry_;
  std::unique_ptr<TestProfileIOS> profile_;
  // Owned by profile_.
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
};

TEST_F(SearchEngineChoiceUtilTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/false));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
}

TEST_F(SearchEngineChoiceUtilTest,
       ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntent) {
  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kAppStartedByExternalIntent,
      1);
}

TEST_F(
    SearchEngineChoiceUtilTest,
    ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntentAlmostMaxCount) {
  ASSERT_GT(switches::kSearchEngineChoiceMaximumSkipCount.Get(), 0);
  PrefService* pref_service = profile().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      switches::kSearchEngineChoiceMaximumSkipCount.Get() - 1);

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kAppStartedByExternalIntent,
      1);
}

TEST_F(
    SearchEngineChoiceUtilTest,
    ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntentMaxCountReached) {
  PrefService* pref_service = profile().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      switches::kSearchEngineChoiceMaximumSkipCount.Get());

  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      profile(), /*is_first_run_entrypoint=*/false,
      /*app_started_via_external_intent=*/true));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
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
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kHasNonGoogleSearchEngine,
      1);
}
