// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/model/search_engine_choice_util.h"

#import "base/check_deref.h"
#import "base/feature_list.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector_mock.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class SearchEngineChoiceUtilTest : public PlatformTest {
 public:
  SearchEngineChoiceUtilTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        switches::kSearchEngineChoiceTrigger,
        {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
          "false"}});
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    DefaultSearchManager::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    local_state_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);

    search_engine_choice_service_ =
        std::make_unique<search_engines::SearchEngineChoiceService>(
            pref_service_, &local_state_);

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    InitMockPolicyService();
    TestChromeBrowserState::Builder builder;
    builder.SetPolicyConnector(
        std::make_unique<BrowserStatePolicyConnectorMock>(
            std::move(policy_service_), &schema_registry_));
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    template_url_service_ = ios::TemplateURLServiceFactory::GetForBrowserState(
        browser_state_.get());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceSearchEngineChoiceScreen);
  }

  TestChromeBrowserState& browser_state() { return *browser_state_; }

  base::HistogramTester histogram_tester_;

  TemplateURLService& template_url_service() { return *template_url_service_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }

  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    return CHECK_DEREF(search_engine_choice_service_.get());
  }

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
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  // Owned by browser_state_.
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
};

TEST_F(SearchEngineChoiceUtilTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
      /*app_started_via_external_intent=*/false));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
}

TEST_F(SearchEngineChoiceUtilTest,
       ShowChoiceScreenIfPoliciesAreNotSetStartedByExternalIntent) {
  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
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
  PrefService* pref_service = browser_state().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      switches::kSearchEngineChoiceMaximumSkipCount.Get() - 1);

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
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
  PrefService* pref_service = browser_state().GetPrefs();
  pref_service->SetInteger(
      prefs::kDefaultSearchProviderChoiceScreenSkippedCount,
      switches::kSearchEngineChoiceMaximumSkipCount.Get());

  EXPECT_TRUE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
      /*app_started_via_external_intent=*/true));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::kEligible, 1);
}

TEST_F(SearchEngineChoiceUtilTest,
       DoNotShowChoiceScreenIfUserHasCustomSearchEngineSetAsDefault) {
  feature_list().Reset();
  feature_list().InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
        "false"},
       {switches::kSearchEngineChoiceTriggerSkipFor3p.name, "false"}});

  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
      /*app_started_via_external_intent=*/false));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kHasCustomSearchEngine,
      1);
}

TEST_F(SearchEngineChoiceUtilTest,
       DoNotShowChoiceScreenIfUserHasNonGoogleSearchEngineSetAsDefault) {
  feature_list().Reset();
  feature_list().InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerForTaggedProfilesOnly.name,
        "false"},
       {switches::kSearchEngineChoiceTriggerSkipFor3p.name, "true"}});

  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_FALSE(ShouldDisplaySearchEngineChoiceScreen(
      browser_state(), search_engines::ChoicePromo::kDialog,
      /*app_started_via_external_intent=*/false));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenProfileInitConditionsHistogram,
      search_engines::SearchEngineChoiceScreenConditions::
          kHasNonGoogleSearchEngine,
      1);
}
