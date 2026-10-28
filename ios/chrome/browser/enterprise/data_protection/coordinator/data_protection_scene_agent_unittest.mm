// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/coordinator/data_protection_scene_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/values.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/public/features.h"
#import "ios/chrome/browser/enterprise/data_protection/public/pref_names.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kProtectedURL[] = "https://protected.com";
const char kChromeNewTabURL[] = "chrome://newtab";

// Returns the id<BrowserProvider> from `scene_state` corresponding to
// `is_off_the_record`.
id<BrowserProvider> BrowserProviderForMode(SceneState* scene_state,
                                           bool is_off_the_record) {
  id<BrowserProviderInterface> interface = scene_state.browserProviderInterface;
  return is_off_the_record ? interface.incognitoBrowserProvider
                           : interface.mainBrowserProvider;
}

}  // anonymous namespace

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

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.profileState = profile_state_;
    scene_state_.scene = scoped_key_window_.GetScene();
    scene_state_.window = scoped_key_window_.Get();
    scene_state_.UIEnabled = YES;

    agent_ = [[DataProtectionSceneAgent alloc] init];

    mock_agent_ = OCMPartialMock(agent_);
    [mock_agent_ setExpectationOrderMatters:YES];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_agent_);
    @autoreleasepool {
      [mock_agent_ stopMocking];
      mock_agent_ = nil;

      // Trigger `sceneStateDidDisableUI:` to clean up observers and the test
      // state.
      scene_state_.UIEnabled = NO;
      [scene_state_ shutdown];
      agent_ = nil;
      scene_state_ = nil;
      profile_state_ = nil;
    }
    profile_.reset();
    PlatformTest::TearDown();
  }

  // Sets a DataControlsRules policy that protects screenshots.
  void SetDataControlsRulesPolicyManaged() {
    data_controls::SetDataControls(profile_->GetPrefs(), {
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
    data_controls::SetDataControls(profile_->GetPrefs(), {
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
  void ExpectApplyScreenshotProtection(BOOL applied_protection,
                                       UIWindow* window = nil) {
    // Add expectation to the mock agent then forward to the real one.
    // Forwarding makes sure that invariants in the real implementation are
    // checked by tests.
    OCMExpect([mock_agent_
                  applyScreenshotProtection:applied_protection
                                   toWindow:window ? window : OCMOCK_ANY])
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
    Browser* browser = BrowserProviderForMode(scene_state_, incognito).browser;
    CHECK_NE(browser, nullptr);
    CHECK_EQ(browser->GetProfile(), profile);
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
    return web_state_ptr;
  }

  // Returns the single WatermarkView attached to the scene's window, or nil.
  // Asserts that at most one watermark view exists.
  WatermarkView* GetWatermarkView() {
    WatermarkView* watermark_view = nil;
    for (UIView* subview in scene_state_.window.subviews) {
      if ([subview isKindOfClass:[WatermarkView class]]) {
        EXPECT_EQ(watermark_view, nil);
        watermark_view = static_cast<WatermarkView*>(subview);
      }
    }
    return watermark_view;
  }

  // Sets a watermark entry in preferences for the given tab ID.
  void SetWatermarkPref(web::WebStateID tab_id, std::string text) {
    ScopedDictPrefUpdate update(
        profile_->GetTestingPrefService(),
        enterprise_data_protection::kDataProtectionWatermarkedTabs);
    update->Set(base::NumberToString(tab_id.identifier()), std::move(text));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ScopedKeyWindow scoped_key_window_;

  ProfileState* profile_state_;
  FakeSceneState* scene_state_;

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
  profile_->GetPrefs()->ClearPref(data_controls::kDataControlsRulesPref);
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

  // Add an unprotected tab to the incognito profile. Chrome new tab URLs are
  // not protected.
  web::FakeWebState* web_state2 =
      AddActiveWebState(GURL(kChromeNewTabURL), /*incognito=*/true);
  ASSERT_FALSE(DataProtectionTabHelper::FromWebState(web_state2)
                   ->IsScreenshotProtectionEnabled());

  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // Switch to incognito browser, which has an unprotected tab.
  ExpectApplyScreenshotProtection(NO);

  scene_state_.incognitoState.incognitoContentVisible = YES;
  [scene_state_
      setCurrentBrowserProvider:BrowserProviderForMode(
                                    scene_state_, /*is_off_the_record=*/true)];

  EXPECT_OCMOCK_VERIFY(mock_agent_);
}

// Tests that no UAF occurs when the currently observed browser is destroyed.
TEST_F(DataProtectionSceneAgentTransitionTest,
       TabGrid_IncognitoBrowserDestroyed_NoUAF) {
  ASSERT_TRUE(scene_state_.UIEnabled);
  ASSERT_TRUE(scene_state_.activationLevel >=
              SceneActivationLevelForegroundInactive);
  ASSERT_TRUE(scene_state_.profileState.initStage >=
              ProfileInitStage::kProfileLoaded);

  scene_state_.tabGridState.tabGridVisible = YES;

  // Switch to Incognito Tab Grid.
  scene_state_.incognitoState.incognitoContentVisible = YES;
  [scene_state_
      setCurrentBrowserProvider:BrowserProviderForMode(
                                    scene_state_, /*is_off_the_record=*/true)];

  // Destroy the incognito browser.
  [scene_state_ destroyAndRecreateOffTheRecordProfile];

  // Switch back to Regular Tab Grid. This will cause the scene state
  // to update its observers, which could lead to a UAF if the destroyed
  // browser wasn't properly unobserved.
  scene_state_.incognitoState.incognitoContentVisible = NO;
  [scene_state_
      setCurrentBrowserProvider:BrowserProviderForMode(
                                    scene_state_, /*is_off_the_record=*/false)];

  // Expect no protection. No crash means success.
  ExpectNoCallsToApplyScreenshotProtection();
}

// Tests that screenshot protection is applied to all windows in the scene.
TEST_F(DataProtectionSceneAgentTransitionTest, MultiWindowProtection) {
  // Create two windows.
  UIWindow* window1 = [[UIWindow alloc] init];
  UIWindow* window2 = [[UIWindow alloc] init];

  // Mock the scene to return both windows.
  id mock_scene = OCMClassMock([UIWindowScene class]);
  NSArray<UIWindow*>* windows = @[ window1, window2 ];
  OCMStub([mock_scene windows]).andReturn(windows);
  scene_state_.scene = mock_scene;

  SetDataControlsRulesPolicyManaged();

  // Entering the tab grid should protect both windows.
  ExpectApplyScreenshotProtection(YES, window1);
  ExpectApplyScreenshotProtection(YES, window2);

  scene_state_.tabGridState.tabGridVisible = YES;
}

// Tests that new windows added to the scene receive screenshot protection if it
// is enabled.
TEST_F(DataProtectionSceneAgentTransitionTest, NewWindowProtection) {
  SetDataControlsRulesPolicyManaged();

  // Entering the tab grid should protect the current window.
  ExpectApplyScreenshotProtection(YES);
  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_OCMOCK_VERIFY(mock_agent_);

  // Simulate a new window becoming visible in the same scene.
  UIWindow* new_window = [[UIWindow alloc] init];
  id mock_scene = scene_state_.scene;
  id mock_window = OCMPartialMock(new_window);
  OCMStub([mock_window windowScene]).andReturn(mock_scene);

  ExpectApplyScreenshotProtection(YES, new_window);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIWindowDidBecomeVisibleNotification
                    object:new_window];
}

#pragma mark - Watermarking Tests

// Tests that the watermark view is applied to the window when entering the Tab
// Grid if an open tab is watermarked, and is removed when exiting the Tab Grid.
TEST_F(DataProtectionSceneAgentTransitionTest, WatermarkViewAddedAndRemoved) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  WatermarkView* watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Test Watermark");

  scene_state_.tabGridState.tabGridVisible = NO;
  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that the watermark is not displayed on the Incognito Tab Grid even if a
// regular tab is watermarked, and is properly restored when returning to the
// regular Tab Grid.
TEST_F(DataProtectionSceneAgentTransitionTest, WatermarkNotAppliedInIncognito) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  WatermarkView* watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Test Watermark");

  // Switch to incognito. Watermark must be removed immediately.
  scene_state_.incognitoState.incognitoContentVisible = YES;
  EXPECT_EQ(GetWatermarkView(), nil);

  // Switch back to regular tab grid. Watermark should be restored.
  scene_state_.incognitoState.incognitoContentVisible = NO;
  watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Test Watermark");
}

// Tests that closing the only watermarked tab in the Tab Grid dynamically
// removes the watermark view.
TEST_F(DataProtectionSceneAgentTransitionTest, WatermarkRemovedWhenTabClosed) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  ASSERT_NE(GetWatermarkView(), nil);

  // Close the tab from the browser while in the tab grid.
  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  browser->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);

  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that a watermarked tab belonging to another window does not trigger
// watermarking on this window's Tab Grid.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkNotAppliedForOtherWindowTab) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  // Add an unwatermarked tab to this window.
  AddActiveWebState();

  // Add a watermarked tab ID from another window (a valid WebStateID not in
  // this window's WebStateList) into the profile prefs.
  web::WebStateID foreign_tab_id = web::WebStateID::NewUnique();
  SetWatermarkPref(foreign_tab_id, "Foreign Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that inserting a watermarked tab while in the Tab Grid immediately
// applies the watermark view.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkAppliedWhenTabInsertedInGrid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.incognitoState.incognitoContentVisible = NO;
  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_EQ(GetWatermarkView(), nil);

  auto web_state = std::make_unique<web::FakeWebState>();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Inserted Watermark");

  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  browser->GetWebStateList()->InsertWebState(
      std::move(web_state), WebStateList::InsertionParams::AtIndex(0));

  WatermarkView* watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Inserted Watermark");
}

// Tests that closing all tabs in a batch operation while in the Tab Grid
// removes the watermark view.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkRemovedOnBatchCloseInGrid) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  ASSERT_NE(GetWatermarkView(), nil);

  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  CloseAllWebStates(*browser->GetWebStateList(),
                    WebStateList::ClosingReason::kUserAction);

  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that the watermark remains on screen when transitioning to background
// while in the Tab Grid, ensuring the app switcher snapshot is protected.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkPreservedOnBackgroundTransition) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  ASSERT_NE(GetWatermarkView(), nil);

  scene_state_.activationLevel = SceneActivationLevelBackground;
  EXPECT_NE(GetWatermarkView(), nil);
}

// Tests that the watermark is not applied to the Tab Grid if
// kEnableEnterpriseWatermarkingIOS is disabled.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkNotAppliedWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);

  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  std::string tab_id =
      base::NumberToString(web_state->GetUniqueIdentifier().identifier());

  {
    ScopedDictPrefUpdate update(
        profile_->GetTestingPrefService(),
        enterprise_data_protection::kDataProtectionWatermarkedTabs);
    update->Set(tab_id, "Test Watermark");
  }

  scene_state_.tabGridState.tabGridVisible = YES;
  bool found_watermark = false;
  for (UIView* subview in scene_state_.window.subviews) {
    if ([subview isKindOfClass:[WatermarkView class]]) {
      found_watermark = true;
      break;
    }
  }
  EXPECT_FALSE(found_watermark);
}

// Tests that the watermark is not applied to the Tab Grid if enterprise
// lookups are disabled.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkNotAppliedWhenEnterpriseLookupsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  // 0 = REAL_TIME_CHECK_DISABLED
  SetRealTimeUrlCheckModePolicy(0);

  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  web::FakeWebState* web_state = AddActiveWebState();
  SetWatermarkPref(web_state->GetUniqueIdentifier(), "Test Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that when the number of tabs exceeds `kMaxTabsToCheckForWatermark`, the
// Tab Grid is watermarked if there are entries in the watermarked tabs pref.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkAppliedWhenMoreThanMaxTabsToCheck) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  WebStateList* web_state_list = browser->GetWebStateList();
  {
    auto batch = web_state_list->StartBatchOperation();
    for (int i = 0; i < kMaxTabsToCheckForWatermark + 1; ++i) {
      auto web_state = std::make_unique<web::FakeWebState>();
      web_state->SetBrowserState(profile_.get());
      web_state_list->InsertWebState(std::move(web_state),
                                     WebStateList::InsertionParams::AtIndex(i));
    }
  }

  web::WebState* first_web_state = web_state_list->GetWebStateAt(0);
  SetWatermarkPref(first_web_state->GetUniqueIdentifier(),
                   "Over Limit Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  WatermarkView* watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Over Limit Watermark");
}

// Tests that when the number of tabs exceeds `kMaxTabsToCheckForWatermark`, no
// watermark is applied if there are no entries in the watermarked tabs pref.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkNotAppliedWhenMoreThanMaxTabsToCheckAndNoWatermarkedTabs) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  WebStateList* web_state_list = browser->GetWebStateList();
  {
    auto batch = web_state_list->StartBatchOperation();
    for (int i = 0; i < kMaxTabsToCheckForWatermark + 1; ++i) {
      auto web_state = std::make_unique<web::FakeWebState>();
      web_state->SetBrowserState(profile_.get());
      web_state_list->InsertWebState(std::move(web_state),
                                     WebStateList::InsertionParams::AtIndex(i));
    }
  }

  scene_state_.tabGridState.tabGridVisible = YES;
  EXPECT_EQ(GetWatermarkView(), nil);
}

// Tests that when the number of tabs is exactly `kMaxTabsToCheckForWatermark`
// (at the inspection limit), all tabs are inspected and a watermark on the last
// tab is detected.
TEST_F(DataProtectionSceneAgentTransitionTest,
       WatermarkAppliedWhenLastTabAtLimitIsWatermarked) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableEnterpriseWatermarkingIOS);
  SetRealTimeUrlCheckModePolicy(1);
  scene_state_.tabGridState.tabGridVisible = NO;
  scene_state_.incognitoState.incognitoContentVisible = NO;

  Browser* browser = BrowserProviderForMode(scene_state_, false).browser;
  WebStateList* web_state_list = browser->GetWebStateList();
  {
    auto batch = web_state_list->StartBatchOperation();
    for (int i = 0; i < kMaxTabsToCheckForWatermark; ++i) {
      auto web_state = std::make_unique<web::FakeWebState>();
      web_state->SetBrowserState(profile_.get());
      web_state_list->InsertWebState(std::move(web_state),
                                     WebStateList::InsertionParams::AtIndex(i));
    }
  }

  // Watermark only the last tab.
  web::WebState* last_web_state =
      web_state_list->GetWebStateAt(kMaxTabsToCheckForWatermark - 1);
  SetWatermarkPref(last_web_state->GetUniqueIdentifier(), "Last Tab Watermark");

  scene_state_.tabGridState.tabGridVisible = YES;
  WatermarkView* watermark_view = GetWatermarkView();
  ASSERT_NE(watermark_view, nil);
  EXPECT_NSEQ(watermark_view.text, @"Last Tab Watermark");
}
