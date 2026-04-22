// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_search/mock_contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeComposeboxInputStateManagerDelegate
    : NSObject <ComposeboxInputStateManagerDelegate>
@property(nonatomic, assign) BOOL didUpdateInputStateCalled;
@property(nonatomic, assign) contextual_search::InputState lastInputState;
@end

@implementation FakeComposeboxInputStateManagerDelegate

- (void)inputStateManager:(ComposeboxInputStateManager*)manager
      didUpdateInputState:(const contextual_search::InputState&)inputState {
  self.didUpdateInputStateCalled = YES;
  self.lastInputState = inputState;
}

@end

class ComposeboxInputStateManagerTest : public PlatformTest {
 protected:
  ComposeboxInputStateManagerTest()
      : web_state_list_(&web_state_list_delegate_) {}

  void SetUp() override {
    PlatformTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(kComposeboxServerSideState);

    session_handle_ = std::make_unique<
        contextual_search::MockContextualSearchSessionHandle>();
    manager_ = [[ComposeboxInputStateManager alloc]
         initWithWebStateList:&web_state_list_
                  prefService:&pref_service_
        aimEligibilityService:nullptr
              identityManager:identity_test_env_.identity_manager()
                sessionHandle:session_handle_.get()
                   entrypoint:ComposeboxEntrypoint::kOther
                  isIncognito:NO];
  }

  void TearDown() override {
    [manager_ disconnect];
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<contextual_search::MockContextualSearchSessionHandle>
      session_handle_;
  signin::IdentityTestEnvironment identity_test_env_;

  ComposeboxInputStateManager* manager_;
};

// Tests that the manager initializes with the expected default state.
TEST_F(ComposeboxInputStateManagerTest, Initialization) {
  // Expect manager to be created.
  EXPECT_NE(manager_, nil);
  // Expect default active tool to be unspecified.
  EXPECT_EQ(manager_.activeTool, omnibox::TOOL_MODE_UNSPECIFIED);
  // Expect default active model to be none.
  EXPECT_EQ(manager_.activeModel, ComposeboxModelOption::kNone);
}

// Tests that the manager correctly observes state updates from the model
// and notifies its delegate.
TEST_F(ComposeboxInputStateManagerTest, StateObservation) {
  FakeComposeboxInputStateManagerDelegate* delegate =
      [[FakeComposeboxInputStateManagerDelegate alloc] init];
  manager_.delegate = delegate;

  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->add_allowed_tools(
      omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolRule* rule = tool_config->mutable_rule();
  rule->set_allow_all_input_types(true);
  rule->set_allow_all_models(true);

  // Setting searchbox config should trigger the initial state update.
  [manager_ setSearchboxConfig:config];

  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // Changing active tool should trigger another state update.
  [manager_ setActiveTool:omnibox::ToolMode::TOOL_MODE_CANVAS];

  // Verify delegate was notified and received the correct state.
  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
  EXPECT_EQ(delegate.lastInputState.active_tool,
            omnibox::ToolMode::TOOL_MODE_CANVAS);
}

// Tests that the manager correctly notifies its delegate when the searchbox
// configuration changes (reloads).
TEST_F(ComposeboxInputStateManagerTest, StateObservationOnConfigChange) {
  FakeComposeboxInputStateManagerDelegate* delegate =
      [[FakeComposeboxInputStateManagerDelegate alloc] init];
  manager_.delegate = delegate;

  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->add_allowed_tools(
      omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);

  [manager_ setSearchboxConfig:config];
  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // Reload config.
  [manager_ setSearchboxConfig:config];

  // Verify delegate was notified again on config change.
  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
}

// Tests that the manager preserves the user's active tool choice across
// searchbox configuration reloads if the tool is still allowed.
TEST_F(ComposeboxInputStateManagerTest, Preselection) {
  FakeComposeboxInputStateManagerDelegate* delegate =
      [[FakeComposeboxInputStateManagerDelegate alloc] init];
  manager_.delegate = delegate;

  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->add_allowed_tools(
      omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolRule* rule = tool_config->mutable_rule();
  rule->set_allow_all_input_types(true);
  rule->set_allow_all_models(true);

  // Load initial config.
  [manager_ setSearchboxConfig:config];
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // User selects a tool.
  [manager_ setActiveTool:omnibox::ToolMode::TOOL_MODE_CANVAS];
  EXPECT_EQ(manager_.activeTool, omnibox::ToolMode::TOOL_MODE_CANVAS);
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // Reload same config.
  [manager_ setSearchboxConfig:config];

  // Verify that the user's choice is preserved (preselected).
  EXPECT_EQ(manager_.activeTool, omnibox::ToolMode::TOOL_MODE_CANVAS);
  // Verify delegate was notified with the correct state.
  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
  EXPECT_EQ(delegate.lastInputState.active_tool,
            omnibox::ToolMode::TOOL_MODE_CANVAS);
}

// Tests that the manager falls back to default state when a previously
// selected tool becomes disallowed after a configuration reload.
TEST_F(ComposeboxInputStateManagerTest, PreselectionRestricted) {
  FakeComposeboxInputStateManagerDelegate* delegate =
      [[FakeComposeboxInputStateManagerDelegate alloc] init];
  manager_.delegate = delegate;

  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->add_allowed_tools(
      omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  omnibox::ToolRule* rule = tool_config->mutable_rule();
  rule->set_allow_all_input_types(true);
  rule->set_allow_all_models(true);

  // Load initial config where Canvas is allowed.
  [manager_ setSearchboxConfig:config];
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // User selects Canvas.
  [manager_ setActiveTool:omnibox::ToolMode::TOOL_MODE_CANVAS];
  EXPECT_EQ(manager_.activeTool, omnibox::ToolMode::TOOL_MODE_CANVAS);
  delegate.didUpdateInputStateCalled = NO;  // Reset flag

  // Load new config where Canvas is NOT allowed.
  omnibox::SearchboxConfig new_config;
  [manager_ setSearchboxConfig:new_config];

  // Verify that the tool falls back to unspecified because it's no longer
  // allowed.
  EXPECT_EQ(manager_.activeTool, omnibox::TOOL_MODE_UNSPECIFIED);
  // Verify delegate was notified with the correct state.
  EXPECT_TRUE(delegate.didUpdateInputStateCalled);
  EXPECT_EQ(delegate.lastInputState.active_tool,
            omnibox::TOOL_MODE_UNSPECIFIED);
}
