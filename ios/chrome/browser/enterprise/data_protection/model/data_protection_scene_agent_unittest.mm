// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_scene_agent.h"

#import "base/values.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

const char kProtectedURL[] = "https://protected.com";
const char kChromeVersionURL[] = "chrome://version";

// Testing extension used for verifying calls to `applyScreenshotProtection`.
@interface DataProtectionSceneAgent ()
- (void)applyScreenshotProtection:(BOOL)isProtected toWindow:(UIWindow*)window;
- (void)updateScreenshotProtection;
@end

// Base test fixture for DataProtectionSceneAgent. Subclasses customize the test
// set up for specific scenarios.
class DataProtectionSceneAgentTestBase : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    SetProfileStateInitStage(profile_state_, ProfileInitStage::kProfileLoaded);
    profile_state_.profile = profile_.get();

    scene_state_ = [[FakeSceneState alloc] initWithAppState:nil
                                                    profile:profile_.get()];
    scene_state_.profileState = profile_state_;
    scene_state_.window = [[UIWindow alloc]
        initWithWindowScene:chrome_test_util::GetAnyWindowScene()];
    scene_state_.UIEnabled = YES;
    scene_state_.incognitoState.incognitoContentVisible = NO;

    agent_ = [[DataProtectionSceneAgent alloc] init];

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
    incognito_browser_ =
        std::make_unique<TestBrowser>(otr_profile, scene_state_);

    StubBrowserProvider* main_provider = [[StubBrowserProvider alloc] init];
    main_provider.browser = browser_.get();

    StubBrowserProvider* incognito_provider =
        [[StubBrowserProvider alloc] init];
    incognito_provider.browser = incognito_browser_.get();

    StubBrowserProviderInterface* interface =
        [[StubBrowserProviderInterface alloc] init];
    interface.mainBrowserProvider = main_provider;
    interface.currentBrowserProvider = main_provider;
    interface.incognitoBrowserProvider = incognito_provider;

    scene_state_.browserProviderInterface = interface;

    mock_agent_ = OCMPartialMock(agent_);
    [mock_agent_ setExpectationOrderMatters:YES];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_agent_);
    [mock_agent_ stopMocking];
    mock_agent_ = nil;

    // Trigger `sceneStateDidDisableUI:` to clean up observers and the test
    // state.
    scene_state_.UIEnabled = NO;

    [scene_state_ shutdown];
    agent_ = nil;
    scene_state_ = nil;
    profile_state_ = nil;
    browser_.reset();
    incognito_browser_.reset();
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Sets a DataControlsRules policy that protects screenshots.
  void SetDataControlsRulesPolicyManaged() {
    data_controls::SetDataControls(profile_->GetTestingPrefService(), {
                                                                          R"({
          "name": "Test screenshot rule",
          "rule_id": "1234",
          "sources": { "urls": ["*"] },
          "restrictions": [
            {"class": "SCREENSHOT", "level": "BLOCK"}
          ]
        })"});
  }

  // Sets a DataControlsRules policy with a restriction OTHER than screenshots.
  void SetDataControlsRulesOtherPolicy() {
    data_controls::SetDataControls(profile_->GetTestingPrefService(), {
                                                                          R"({
          "name": "Test clipboard rule",
          "rule_id": "5678",
          "sources": { "urls": ["*"] },
          "restrictions": [
            {"class": "CLIPBOARD", "level": "BLOCK"}
          ]
        })"});
  }

  // Sets the EnterpriseRealTimeUrlCheckMode policy.
  void SetRealTimeUrlCheckModePolicy(int mode) {
    profile_->GetTestingPrefService()->SetManagedPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        base::Value(mode));
  }

  // Adds an expectation for a call to `applyScreenshotProtection` with the
  // given protection.
  void ExpectApplyScreenshotProtection(BOOL applied_protection) {
    // Add expectation to the mock agent then forward to the real one.
    // Forwarding makes sure that invariants in the real implementation are
    // checked by tests.
    OCMExpect([mock_agent_ applyScreenshotProtection:applied_protection
                                            toWindow:OCMOCK_ANY])
        .andForwardToRealObject();
  }

  // Makes the mock agent reject all calls to `applyScreenshotProtection`.
  void ExpectNoCallsToApplyScreenshotProtection() {
    OCMReject([mock_agent_ applyScreenshotProtection:OCMOCK_ANY
                                            toWindow:OCMOCK_ANY]);
  }

  // Adds a WebState to the regular or incognito browser with the given visible
  // url. Also forces the creation of DataProtectionTabHelper for the WebState.
  web::FakeWebState* AddActiveWebState(const GURL& url = GURL(),
                                       bool incognito = false) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state_ptr = web_state.get();
    ProfileIOS* profile =
        incognito ? profile_->GetOffTheRecordProfile() : profile_.get();
    web_state->SetBrowserState(profile);
    if (!url.is_empty()) {
      web_state->SetVisibleURL(url);
    }
    DataProtectionTabHelper::CreateForWebState(web_state_ptr);
    Browser* browser = incognito ? incognito_browser_.get() : browser_.get();
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
    return web_state_ptr;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;

  ProfileState* profile_state_;
  FakeSceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;

  DataProtectionSceneAgent* agent_;
  id mock_agent_;
};

// Test fixture for verifying the initial state when the agent is connected.
class DataProtectionSceneAgentInitTest
    : public DataProtectionSceneAgentTestBase {};

// Tests that the agent doesn't apply any protection when no
// policies apply and the tab grid is hidden (Single Tab state).
TEST_F(DataProtectionSceneAgentInitTest, SingleTab_NoPolicy_NoOp) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  scene_state_.tabGridState.tabGridVisible = NO;

  ExpectNoCallsToApplyScreenshotProtection();
  [scene_state_ addAgent:agent_];
}

// Tests that the agent does not apply protection when there are no policies.
TEST_F(DataProtectionSceneAgentInitTest, TabGrid_NoPolicy_NoOp) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.tabGridState.tabGridVisible = YES;

  // Even when the scene is ready for protection nothing should be applied
  // because there are no policies.
  ExpectNoCallsToApplyScreenshotProtection();

  [scene_state_ addAgent:agent_];
}

// Tests that updateScreenshotProtection is a no-op when UI is disabled.
TEST_F(DataProtectionSceneAgentInitTest, TabGrid_UIDisabled_NoOp) {
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.tabGridState.tabGridVisible = YES;
  SetDataControlsRulesPolicyManaged();

  // No protection should apply until the UI is enabled.
  scene_state_.UIEnabled = NO;

  ExpectNoCallsToApplyScreenshotProtection();

  [scene_state_ addAgent:agent_];
}

// Tests that updateScreenshotProtection is a no-op when the profile hasn't been
// loaded.
TEST_F(DataProtectionSceneAgentInitTest, TabGrid_ProfileNotLoaded_NoOp) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);

  scene_state_.tabGridState.tabGridVisible = YES;

  profile_state_ = [[ProfileState alloc] initWithAppState:nil];
  profile_state_.profile = profile_.get();

  scene_state_.profileState = profile_state_;

  ASSERT_TRUE(scene_state_.profileState.initStage <
              ProfileInitStage::kProfileLoaded);

  SetDataControlsRulesPolicyManaged();

  ExpectNoCallsToApplyScreenshotProtection();

  // No protection should be applied until the profile is loaded.
  [scene_state_ addAgent:agent_];
}

// Tests that the agent initializes protection right away with YES when policies
// apply and the tab grid is visible.
TEST_F(DataProtectionSceneAgentInitTest, TabGrid_WithPolicy_Protection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  SetDataControlsRulesPolicyManaged();
  scene_state_.tabGridState.tabGridVisible = YES;

  ExpectApplyScreenshotProtection(YES);
  [scene_state_ addAgent:agent_];
}

// Tests that the agent doesn't apply any protection when displaying a single
// tab that has no protection.
TEST_F(DataProtectionSceneAgentInitTest, SingleTab_WithPolicy_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  SetDataControlsRulesPolicyManaged();
  scene_state_.tabGridState.tabGridVisible = NO;

  // No protection applied as this is the default state.
  ExpectNoCallsToApplyScreenshotProtection();

  [scene_state_ addAgent:agent_];
}

// Tests that the agent initializes protection right away with YES displaying a
// single tab that requires protection.
TEST_F(DataProtectionSceneAgentInitTest, SingleTab_WithPolicy_Protection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  SetDataControlsRulesPolicyManaged();
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState(GURL(kProtectedURL));
  ASSERT_TRUE(DataProtectionTabHelper::FromWebState(web_state)
                  ->IsScreenshotProtectionEnabled());

  ExpectApplyScreenshotProtection(YES);
  [scene_state_ addAgent:agent_];
}

// Test fixture for verifying state transitions after the agent is connected.
class DataProtectionSceneAgentTransitionTest
    : public DataProtectionSceneAgentTestBase {
 protected:
  void SetUp() override {
    DataProtectionSceneAgentTestBase::SetUp();
    [scene_state_ addAgent:agent_];
  }
};

// Tests that entering the Tab Grid with NO policies enabled does NOT protect
// the window.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_NoPolicies_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  ExpectNoCallsToApplyScreenshotProtection();

  // Simulate entering the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that entering the Tab Grid with DataControlsRules enabled protects the
// window.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_DataControlsPolicy_ProtectsWindow) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  SetDataControlsRulesPolicyManaged();

  ExpectApplyScreenshotProtection(YES);

  // Simulate entering the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that entering the Tab Grid with a DataControls rule that does NOT
// apply to screenshots (e.g. CLIPBOARD) does NOT protect the window.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_DataControlsPolicy_OtherRule_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  SetDataControlsRulesOtherPolicy();

  ExpectNoCallsToApplyScreenshotProtection();

  // Simulate entering the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that entering the Tab Grid with RealTimeUrlCheckMode enabled protects
// the window.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_RealTimeUrlLookupPolicy_ProtectsWindow) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  // 1 = REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED
  SetRealTimeUrlCheckModePolicy(1);

  ExpectApplyScreenshotProtection(YES);

  // Simulate entering the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that entering the Tab Grid with RealTimeUrlCheckMode disabled does NOT
// protect the window.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_RealTimeUrlLookupPolicyDisabled_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  // 0 = REAL_TIME_CHECK_DISABLED
  SetRealTimeUrlCheckModePolicy(0);

  ExpectNoCallsToApplyScreenshotProtection();

  // Simulate entering the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that enabling a policy while already in the Tab Grid protects the
// window.
TEST_F(DataProtectionSceneAgentTransitionTest, TabGrid_DynamicPolicyEnable) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  // Start in the Tab Grid with NO policies.
  scene_state_.tabGridState.tabGridVisible = YES;

  ExpectApplyScreenshotProtection(YES);

  // Dynamically enable DataControlsRules policy.
  SetDataControlsRulesPolicyManaged();
}

// Tests that disabling a policy while already in the Tab Grid removes
// protection.
TEST_F(DataProtectionSceneAgentTransitionTest, TabGrid_DynamicPolicyDisable) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);
  ASSERT_FALSE(scene_state_.tabGridState.tabGridVisible);

  // Start in the Tab Grid with policy enabled.
  SetDataControlsRulesPolicyManaged();

  // Entering the tab grid should protect the window.
  ExpectApplyScreenshotProtection(YES);
  scene_state_.tabGridState.tabGridVisible = YES;

  // Data control rules are gone, protection should be removed.
  ExpectApplyScreenshotProtection(NO);
  profile_->GetTestingPrefService()->ClearPref(
      data_controls::kDataControlsRulesPref);
}

// Tests that repeated calls to update with the same state do not trigger the
// action.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_NoOpOnRepeatedSameState) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Start in the Tab Grid with policy enabled.
  SetDataControlsRulesPolicyManaged();

  ExpectApplyScreenshotProtection(YES);
  // Entering in the tab grid should apply protection.
  scene_state_.tabGridState.tabGridVisible = YES;

  // Dynamically enabling lookups shouldn't re-apply protection as it was
  // already applied.
  ExpectNoCallsToApplyScreenshotProtection();
  SetRealTimeUrlCheckModePolicy(1);
}

// Tests that switching to the Incognito Tab Grid correctly updates the
// protection state.
TEST_F(DataProtectionSceneAgentTransitionTest, TabGrid_SwitchToIncognito) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Start in regular profile
  scene_state_.tabGridState.tabGridVisible = YES;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  ExpectApplyScreenshotProtection(YES);
  SetRealTimeUrlCheckModePolicy(1);

  // Switch to Incognito Tab Grid should disable the protection as lookups don't
  // work on incognito.
  ExpectApplyScreenshotProtection(NO);
  scene_state_.incognitoState.incognitoContentVisible = YES;
}

// Tests that switching to the Regular Tab Grid correctly updates the protection
// state.
TEST_F(DataProtectionSceneAgentTransitionTest, TabGrid_SwitchToRegular) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Start in incognito profile.
  scene_state_.tabGridState.tabGridVisible = YES;
  scene_state_.incognitoState.incognitoContentVisible = YES;

  SetRealTimeUrlCheckModePolicy(1);

  // Switch to Regular Tab Grid should enable the protection.
  ExpectApplyScreenshotProtection(YES);
  scene_state_.incognitoState.incognitoContentVisible = NO;
}

// Tests that the agent is a no-op when UI is disabled while displaying a single
// tab.
TEST_F(DataProtectionSceneAgentTransitionTest, SingleTab_NoOpWhenUIDisabled) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.UIEnabled = NO;
  // Activating policies shouldn't trigger an update as the agent should stop
  // observing everything.
  ExpectNoCallsToApplyScreenshotProtection();
  SetDataControlsRulesPolicyManaged();
}

// Tests that going from a protected Tab Grid to a single unprotected tab
// disables the protection.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_To_SingleTab_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Start in the Tab Grid with policy enabled.
  SetDataControlsRulesPolicyManaged();
  ExpectApplyScreenshotProtection(YES);
  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // Simulate exiting the Tab Grid to an unprotected tab.
  ExpectApplyScreenshotProtection(NO);
  scene_state_.tabGridState.tabGridVisible = NO;
}

// Tests that going from a protected Tab Grid to a single protected tab keeps
// the protection enabled.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_To_SingleTab_Protection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Start in the Tab Grid.
  scene_state_.tabGridState.tabGridVisible = YES;

  web::FakeWebState* web_state = AddActiveWebState(GURL(kProtectedURL));
  ASSERT_FALSE(DataProtectionTabHelper::FromWebState(web_state)
                   ->IsScreenshotProtectionEnabled());

  ExpectApplyScreenshotProtection(YES);

  SetDataControlsRulesPolicyManaged();
  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // At this point the protection should already be enabled.
  // Switching to a single tab with protection should be a no-op as the window
  // is already protected.
  ExpectNoCallsToApplyScreenshotProtection();

  // Simulate exiting the Tab Grid to the protected tab.
  scene_state_.tabGridState.tabGridVisible = NO;
}

// Tests that switching to the Incognito Tab Grid correctly picks up
// Incognito-specific protection state.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_SwitchToIncognito_NoProtection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  // Enabling lookups should protect the regular tab grid but not the incognito
  // one. Lookups do not work in incognito.
  SetRealTimeUrlCheckModePolicy(1);

  // Start in Main Tab Grid with protection.
  ExpectApplyScreenshotProtection(YES);
  scene_state_.tabGridState.tabGridVisible = YES;
  scene_state_.incognitoState.incognitoContentVisible = NO;
  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // Switch to Incognito Tab Grid should disable the protection.
  ExpectApplyScreenshotProtection(NO);

  scene_state_.incognitoState.incognitoContentVisible = YES;
}

// Tests that activating a protected tab while in single tab mode applies the
// protection.
TEST_F(DataProtectionSceneAgentTransitionTest,
       SingleTab_ActiveTabChanged_Protection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.tabGridState.tabGridVisible = NO;

  SetDataControlsRulesPolicyManaged();
  ExpectApplyScreenshotProtection(YES);

  web::FakeWebState* web_state = AddActiveWebState(GURL(kProtectedURL));
  ASSERT_TRUE(DataProtectionTabHelper::FromWebState(web_state)
                  ->IsScreenshotProtectionEnabled());
}

// Tests that going from a single unprotected tab to the protected Tab Grid
// applies the protection.
TEST_F(DataProtectionSceneAgentTransitionTest,
       SingleTab_Enter_TabGrid_Protection) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  web::FakeWebState* web_state = AddActiveWebState();
  ASSERT_FALSE(DataProtectionTabHelper::FromWebState(web_state)
                   ->IsScreenshotProtectionEnabled());

  SetDataControlsRulesPolicyManaged();
  // Start in Single Tab mode with no protection on the active tab itself.
  scene_state_.tabGridState.tabGridVisible = NO;

  // Simulate entering the Tab Grid where the policy applies.
  ExpectApplyScreenshotProtection(YES);
  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that switching to the incognito active tab will apply the
// incognito-specific protection state.
TEST_F(DataProtectionSceneAgentTransitionTest, SingleTab_SwitchBrowser) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.tabGridState.tabGridVisible = NO;

  SetDataControlsRulesPolicyManaged();

  // Activating a protected tab in the current (main) browser will apply the
  // protection.
  ExpectApplyScreenshotProtection(YES);
  web::FakeWebState* web_state1 = AddActiveWebState(GURL(kProtectedURL));
  ASSERT_TRUE(DataProtectionTabHelper::FromWebState(web_state1)
                  ->IsScreenshotProtectionEnabled());

  // Add an unprotected tab to the incognito profile. Chrome internal urls are
  // not protected.
  web::FakeWebState* web_state2 =
      AddActiveWebState(GURL(kChromeVersionURL), /*incognito=*/true);
  ASSERT_FALSE(DataProtectionTabHelper::FromWebState(web_state2)
                   ->IsScreenshotProtectionEnabled());

  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // Switch to incognito browser, which has an unprotected tab.
  ExpectApplyScreenshotProtection(NO);

  scene_state_.browserProviderInterface.currentBrowserProvider =
      scene_state_.browserProviderInterface.incognitoBrowserProvider;
  scene_state_.incognitoState.incognitoContentVisible = YES;

  EXPECT_OCMOCK_VERIFY(mock_agent_);
}
