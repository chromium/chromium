// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/promo/search_engine_choice_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/country_codes/country_codes.h"
#import "components/policy/core/common/mock_policy_service.h"
#import "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/schema_registry.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/search_engines/search_engine_choice_utils.h"
#import "components/search_engines/search_engines_pref_names.h"
#import "components/search_engines/search_engines_switches.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/browser/policy/browser_state_policy_connector_mock.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SearchEngineChoiceSceneAgentTest : public PlatformTest {
 protected:
  SearchEngineChoiceSceneAgentTest() {
    feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    policy_service_ = std::make_unique<policy::MockPolicyService>();
    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.SetPolicyConnector(
        std::make_unique<BrowserStatePolicyConnectorMock>(
            std::move(policy_service_), &schema_registry_));
    browser_state_ = test_cbs_builder.Build();
    FakeStartupInformation* startup_information_ =
        [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
    scene_state_.scene = static_cast<UIWindowScene*>(
        [[[UIApplication sharedApplication] connectedScenes] anyObject]);

    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), scene_state_);
    promos_manager_ = std::make_unique<MockPromosManager>();
    agent_ = [[SearchEngineChoiceSceneAgent alloc]
        initWithPromosManager:promos_manager_.get()
              forBrowserState:browser_state_.get()];
    agent_.sceneState = scene_state_;

    pref_service_.registry()->RegisterInt64Pref(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp, 0);
  }

  ~SearchEngineChoiceSceneAgentTest() override {
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kSearchEngineChoiceCountry);
    agent_ = nil;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  SearchEngineChoiceSceneAgent* agent_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::SchemaRegistry schema_registry_;
  policy::PolicyMap policy_map_;
  // SceneState only weakly holds AppState, so keep it alive here.
  AppState* app_state_;
  SceneState* scene_state_;
};

// Tests that the promo gets registered for display when the conditions are met.
TEST_F(SearchEngineChoiceSceneAgentTest, TestPromoRegistration) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::Choice))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::Choice))
      .Times(0);

  // The search engine choice feature is only enabled for countries in the
  // EEA region. Override the country checks to simulate being in Belgium.
  // TODO(crbug.com/1499170): Set the country using the PrefService rqther than
  // command-line flags.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "BE");

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo does not get registered for display when the conditions
// aren't met.
TEST_F(SearchEngineChoiceSceneAgentTest, TestNoPromoRegistration) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::Choice))
      .Times(0);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::Choice))
      .Times(1);

  // The search engine choice feature is only enabled for countries in the
  // EEA region. Override the country checks to simulate being in the US.
  // TODO(crbug.com/1499170): Set the country using the PrefService rqther than
  // command-line flags.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}

// Tests that the promo gets deregistered when the conditions for display
// change.
TEST_F(SearchEngineChoiceSceneAgentTest, TestConditionsChange) {
  EXPECT_CALL(*promos_manager_.get(),
              RegisterPromoForSingleDisplay(promos_manager::Promo::Choice))
      .Times(1);
  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::Choice))
      .Times(0);

  // The search engine choice feature is only enabled for countries in the
  // EEA region. Override the country checks to simulate being in Belgium.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "BE");

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_CALL(*promos_manager_.get(),
              DeregisterPromo(promos_manager::Promo::Choice))
      .Times(1);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");

  // Disconnect and re-activate the UI.
  scene_state_.activationLevel = SceneActivationLevelDisconnected;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
}
