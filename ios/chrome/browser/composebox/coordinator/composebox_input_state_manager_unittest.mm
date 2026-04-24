// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_input_state_manager.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/contextual_search/contextual_search_metrics_recorder.h"
#import "components/contextual_search/contextual_search_service.h"
#import "components/contextual_search/contextual_search_session_handle.h"
#import "components/contextual_search/mock_contextual_search_session_handle.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item_collection.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface ComposeboxInputStateManager (Testing)
- (BOOL)imageToolDisabled;
- (BOOL)tabAttachmentDisabled;
@end

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
    contextual_search::ContextualSearchService::RegisterProfilePrefs(
        pref_service_.registry());
    scoped_feature_list_.InitAndEnableFeature(kComposeboxServerSideState);

    session_handle_ = std::make_unique<
        contextual_search::MockContextualSearchSessionHandle>();
    mode_holder_ = [[ComposeboxModeHolder alloc] init];
    mock_aim_service_ = std::make_unique<MockAimEligibilityService>(
        pref_service_, nullptr, nullptr, identity_test_env_.identity_manager());
    manager_ = [[ComposeboxInputStateManager alloc]
         initWithWebStateList:&web_state_list_
                   modeHolder:mode_holder_
                  prefService:&pref_service_
        aimEligibilityService:mock_aim_service_.get()
              identityManager:identity_test_env_.identity_manager()
           templateURLService:nullptr
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

  std::unique_ptr<MockAimEligibilityService> mock_aim_service_;
  ComposeboxModeHolder* mode_holder_;
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

#pragma mark - Tool Availability and Disabled State

// Tests that a tool is allowed when it is included in the allowed tools list
// from the server-side configuration.
TEST_F(ComposeboxInputStateManagerTest, ToolAllowed_ServerSideEnabled) {
  // Server side state is enabled by default in fixture.

  omnibox::SearchboxConfig config;
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);

  // Setting searchbox config should trigger the initial state update.
  [manager_ setSearchboxConfig:config];

  const auto& state = manager_.inputState;
  EXPECT_THAT(state.allowed_tools,
              testing::Contains(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));
}

// Tests that a tool is disabled when it is restricted by the active model's
// rules in the server-side configuration.
TEST_F(ComposeboxInputStateManagerTest, ToolDisabled_ServerSideEnabled) {
  omnibox::SearchboxConfig config;

  // Add a tool that we want to test as disabled.
  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);

  // Add a model and make it active with a rule that does not allow all tools.
  omnibox::ModelConfig* model_config = config.add_model_configs();
  model_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  omnibox::ModelRule* model_rule = model_config->mutable_rule();
  model_rule->set_allow_all_tools(false);
  // Do NOT add IMAGE_GEN to allowed_tools.

  config.set_initial_model_mode(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  [manager_ setSearchboxConfig:config];

  const auto& state = manager_.inputState;
  EXPECT_THAT(state.disabled_tools,
              testing::Contains(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN));
}

// Tests that the image tool is allowed in local fallback mode when the
// server-side state is disabled and the user is eligible according to the AIM
// eligibility service.
TEST_F(ComposeboxInputStateManagerTest, ImageToolAllowed_ServerSideDisabled) {
  // Disable server-side state for this test.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(kComposeboxServerSideState);

  // Set up mock expectation for AIM eligibility service.
  EXPECT_CALL(*mock_aim_service_, IsCreateImagesEligible())
      .WillOnce(testing::Return(true));

  EXPECT_TRUE([manager_ imageToolAllowed]);
}

// Tests that the image tool is disabled in local fallback mode when there are
// already tab or file attachments.
TEST_F(ComposeboxInputStateManagerTest, ImageToolDisabled_HasTabOrFile) {
  // Disable server-side state for this test.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(kComposeboxServerSideState);

  // Add a tab attachment to the collection.
  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  ComposeboxInputItem* item = [[ComposeboxInputItem alloc]
      initWithComposeboxInputItemType:ComposeboxInputItemType::
                                          kComposeboxInputItemTypeTab];
  [collection addItem:item];

  manager_.items = collection;

  EXPECT_TRUE([manager_ imageToolDisabled]);
}

// Tests that tab attachments are disabled in local fallback mode when the
// active mode is image generation.
TEST_F(ComposeboxInputStateManagerTest, TabAttachmentDisabled_ImageGenMode) {
  // Disable server-side state for this test.
  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndDisableFeature(kComposeboxServerSideState);

  // Set the active mode to image generation.
  mode_holder_.mode = ComposeboxMode::kImageGeneration;

  EXPECT_TRUE([manager_ tabAttachmentDisabled]);
}

#pragma mark - Attachment Capacity

// Tests that the remaining attachment capacity defaults to the limit set by the
// server-side configuration when no items are attached.
TEST_F(ComposeboxInputStateManagerTest, RemainingAttachmentCapacity_Default) {
  // Set a max total inputs limit in the config.
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  EXPECT_EQ([manager_ remainingAttachmentCapacity], 5u);
}

// Tests that the remaining attachment capacity correctly accounts for currently
// attached items.
TEST_F(ComposeboxInputStateManagerTest, RemainingAttachmentCapacity_WithItems) {
  // Set a max total inputs limit in the config.
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  // Add some items to the collection.
  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeTab]];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeImage]];

  manager_.items = collection;

  // Verify that the remaining capacity is reduced by the number of items.
  EXPECT_EQ([manager_ remainingAttachmentCapacity], 3u);
}

// Tests that the remaining attachment capacity is reduced to 0 in image
// generation mode if an image is already attached.
TEST_F(ComposeboxInputStateManagerTest,
       RemainingAttachmentCapacity_ImageGenerationMode) {
  // Set a max total inputs limit in the config.
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  // Set active mode to image generation.
  mode_holder_.mode = ComposeboxMode::kImageGeneration;

  // Add an image to the collection.
  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeImage]];

  manager_.items = collection;

  // Verify that capacity is 0 because an image is already present.
  EXPECT_EQ([manager_ remainingAttachmentCapacity], 0u);
}

#pragma mark - Image Limits

// Tests that the remaining number of images allowed defaults to the total
// capacity when no specific type limits are set.
TEST_F(ComposeboxInputStateManagerTest,
       RemainingNumberOfImagesAllowed_Default) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  EXPECT_EQ([manager_ remainingNumberOfImagesAllowed], 5u);
}

// Tests that the remaining number of images allowed respects the specific type
// limit set by the server-side configuration.
TEST_F(ComposeboxInputStateManagerTest,
       RemainingNumberOfImagesAllowed_ServerSideLimit) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);

  // Add an input type rule to set max_instance for images.
  omnibox::InputTypeRule* rule =
      config.mutable_rule_set()->add_input_type_rules();
  rule->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  rule->set_max_instance(2);

  [manager_ setSearchboxConfig:config];

  EXPECT_EQ([manager_ remainingNumberOfImagesAllowed], 2u);
}

// Tests that the remaining number of images allowed accounts for existing
// images when a specific type limit is set.
TEST_F(ComposeboxInputStateManagerTest,
       RemainingNumberOfImagesAllowed_WithItems) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);

  // Add an input type rule to set max_instance for images.
  omnibox::InputTypeRule* rule =
      config.mutable_rule_set()->add_input_type_rules();
  rule->set_input_type(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  rule->set_max_instance(2);

  [manager_ setSearchboxConfig:config];

  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeImage]];

  manager_.items = collection;

  EXPECT_EQ([manager_ remainingNumberOfImagesAllowed], 1u);
}

#pragma mark - Tab Limits

// Tests that the maximum tab attachment count defaults to the total capacity
// when no specific type limits are set.
TEST_F(ComposeboxInputStateManagerTest, MaxTabAttachmentCount_Default) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  EXPECT_EQ([manager_ maxTabAttachmentCount], 5u);
}

// Tests that the maximum tab attachment count respects the specific type limit
// set by the server-side configuration.
TEST_F(ComposeboxInputStateManagerTest, MaxTabAttachmentCount_ServerSideLimit) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);

  // Add an input type rule to set max_instance for tabs.
  omnibox::InputTypeRule* rule =
      config.mutable_rule_set()->add_input_type_rules();
  rule->set_input_type(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  rule->set_max_instance(3);

  [manager_ setSearchboxConfig:config];

  EXPECT_EQ([manager_ maxTabAttachmentCount], 3u);
}

// Tests that the maximum tab attachment count accounts for existing tabs when a
// specific type limit is set.
TEST_F(ComposeboxInputStateManagerTest, MaxTabAttachmentCount_WithItems) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);

  // Add an input type rule to set max_instance for tabs.
  omnibox::InputTypeRule* rule =
      config.mutable_rule_set()->add_input_type_rules();
  rule->set_input_type(omnibox::InputType::INPUT_TYPE_BROWSER_TAB);
  rule->set_max_instance(3);

  [manager_ setSearchboxConfig:config];

  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeTab]];

  manager_.items = collection;

  EXPECT_EQ([manager_ maxTabAttachmentCount], 3u);
}

#pragma mark - UI State Computation

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` correctly
// populates the UI state object with current values.
TEST_F(ComposeboxInputStateManagerTest, ComputeUIInputState) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);
  [manager_ setSearchboxConfig:config];

  UIImage* favicon = [[UIImage alloc] init];
  std::set<web::WebStateID> attached_ids;

  ComposeboxUIInputState* state =
      [manager_ computeUIInputStateWithFavicon:favicon
                           attachedWebStateIDs:attached_ids];

  EXPECT_NE(state, nil);
  EXPECT_EQ(state.currentTabFavicon, favicon);
  EXPECT_EQ(state.remainingAttachmentCapacity, 5u);
}

// Tests that setting a mode results in that mode being active in the UI
// state.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_ActiveModePropagation) {
  mode_holder_.mode = ComposeboxMode::kCanvas;

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_EQ(state.activeTool, ComposeboxMode::kCanvas);
}

// Tests that setting a model results in that model being active in the UI
// state.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_ActiveModelPropagation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kComposeboxAdditionalAdvancedTools);

  omnibox::SearchboxConfig config;
  omnibox::ModelConfig* model_config = config.add_model_configs();
  model_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  omnibox::ModelConfig* regular_config = config.add_model_configs();
  regular_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  [manager_ setSearchboxConfig:config];

  mode_holder_.mode = ComposeboxMode::kAIM;
  [manager_ setActiveModel:ComposeboxModelOption::kThinking
        explicitUserAction:YES];

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_EQ(state.activeModel, ComposeboxModelOption::kThinking);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` correctly
// populates fields based on the provided configuration.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_PopulatesFieldsFromConfig) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kComposeboxAdditionalAdvancedTools);

  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(5);

  omnibox::ToolConfig* tool_config = config.add_tool_configs();
  tool_config->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  omnibox::ToolRule* rule = tool_config->mutable_rule();
  rule->set_tool(omnibox::ToolMode::TOOL_MODE_IMAGE_GEN);
  rule->set_allow_all_input_types(true);

  omnibox::ModelConfig* model_config = config.add_model_configs();
  model_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  config.mutable_rule_set()->add_allowed_models(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  [manager_ setSearchboxConfig:config];

  UIImage* favicon = [[UIImage alloc] init];
  std::set<web::WebStateID> attached_ids;

  ComposeboxUIInputState* state =
      [manager_ computeUIInputStateWithFavicon:favicon
                           attachedWebStateIDs:attached_ids];

  EXPECT_NE(state, nil);
  EXPECT_EQ(state.currentTabFavicon, favicon);
  EXPECT_EQ(state.remainingAttachmentCapacity, 5u);
  EXPECT_TRUE(state.allowModelPicker);

  EXPECT_TRUE([state isToolAvailable:ComposeboxMode::kImageGeneration]);
  EXPECT_TRUE([state isModelAvailable:ComposeboxModelOption::kThinking]);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` disables
// model picker when the feature flag is off.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_ModelPickerDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kComposeboxAdditionalAdvancedTools);

  omnibox::SearchboxConfig config;
  [manager_ setSearchboxConfig:config];

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_FALSE(state.allowModelPicker);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` disables
// current tab attachment on NTP.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_AttachmentEligibility_NTP) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("chrome://newtab/"));
  web_state_list_.InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_FALSE(
      [state isAttachmentAvailable:ComposeboxAttachmentOption::kCurrentTab]);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` disables
// current tab attachment if already attached.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_AttachmentEligibility_AlreadyAttached) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetVisibleURL(GURL("https://example.com"));
  web_state_list_.InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  web::WebStateID web_state_id =
      web_state_list_.GetActiveWebState()->GetUniqueIdentifier();

  ComposeboxUIInputState* state =
      [manager_ computeUIInputStateWithFavicon:nil
                           attachedWebStateIDs:{web_state_id}];

  EXPECT_NE(state, nil);
  EXPECT_FALSE(
      [state isAttachmentAvailable:ComposeboxAttachmentOption::kCurrentTab]);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` disables
// attachments when capacity is full.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_AttachmentEligibility_FullCapacity) {
  omnibox::SearchboxConfig config;
  config.mutable_rule_set()->set_max_total_inputs(1);
  [manager_ setSearchboxConfig:config];

  // Add an item to fill capacity.
  ComposeboxInputItemCollection* collection =
      [[ComposeboxInputItemCollection alloc] init];
  [collection
      addItem:[[ComposeboxInputItem alloc]
                  initWithComposeboxInputItemType:
                      ComposeboxInputItemType::kComposeboxInputItemTypeTab]];
  manager_.items = collection;

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_EQ(state.remainingAttachmentCapacity, 0u);
  // Tab attachment should be disabled.
  EXPECT_TRUE([state isAttachmentDisabled:ComposeboxAttachmentOption::kTab]);
}

// Tests that `computeUIInputStateWithFavicon:attachedWebStateIDs:` disables
// AIM tool for Cobrowse entrypoint.
TEST_F(ComposeboxInputStateManagerTest,
       ComputeUIInputState_ToolEligibility_Cobrowse) {
  [manager_ disconnect];
  manager_ = [[ComposeboxInputStateManager alloc]
       initWithWebStateList:&web_state_list_
                 modeHolder:mode_holder_
                prefService:&pref_service_
      aimEligibilityService:mock_aim_service_.get()
            identityManager:identity_test_env_.identity_manager()
         templateURLService:nullptr
              sessionHandle:session_handle_.get()
                 entrypoint:ComposeboxEntrypoint::kCobrowse
                isIncognito:NO];

  ComposeboxUIInputState* state = [manager_ computeUIInputStateWithFavicon:nil
                                                       attachedWebStateIDs:{}];

  EXPECT_NE(state, nil);
  EXPECT_TRUE([state isToolHidden:ComposeboxMode::kAIM]);
}
