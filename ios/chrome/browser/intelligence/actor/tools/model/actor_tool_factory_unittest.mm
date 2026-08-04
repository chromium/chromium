// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"

#import "base/notreached.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

// Test fixture for ActorToolFactory.
class ActorToolFactoryTest : public PlatformTest {
 protected:
  ActorToolFactoryTest() { feature_list_.InitAndEnableFeature(kActorTools); }

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    factory_ = std::make_unique<ActorToolFactory>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<ActorToolFactory> factory_;
};

// Tests that GetSupportedCapabilities returns the expected list of tools when
// all tools are enabled.
TEST_F(ActorToolFactoryTest, GetSupportedCapabilities) {
  std::vector<optimization_guide::proto::Action::ActionCase> capabilities =
      factory_->GetSupportedCapabilities();

  EXPECT_THAT(capabilities,
              testing::UnorderedElementsAre(
                  optimization_guide::proto::Action::kNavigate,
                  optimization_guide::proto::Action::kClick,
                  optimization_guide::proto::Action::kBack,
                  optimization_guide::proto::Action::kForward,
                  optimization_guide::proto::Action::kType,
                  optimization_guide::proto::Action::kWait,
                  optimization_guide::proto::Action::kScroll,
                  optimization_guide::proto::Action::kScrollTo,
                  optimization_guide::proto::Action::kSelect,
                  optimization_guide::proto::Action::kAttemptLogin,
                  optimization_guide::proto::Action::kAttemptFormFilling,
                  optimization_guide::proto::Action::kCloseTab,
                  optimization_guide::proto::Action::kCreateTab));
}

// Tests that GetSupportedCapabilities filters out tools that are disabled via
// feature parameters.
TEST_F(ActorToolFactoryTest, GetSupportedCapabilitiesWithDisabledTools) {
  base::test::ScopedFeatureList feature_list;
  // Disable ClickTool via feature parameters.
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTools, {{"DisabledTools", "ClickTool"}});

  std::vector<optimization_guide::proto::Action::ActionCase> capabilities =
      factory_->GetSupportedCapabilities();

  // Verify that the disabled tool is not included in the supported
  // capabilities.
  EXPECT_THAT(capabilities, testing::Not(testing::Contains(
                                optimization_guide::proto::Action::kClick)));

  // Verify that other tools (which are not disabled) are still included.
  EXPECT_THAT(capabilities,
              testing::Contains(optimization_guide::proto::Action::kNavigate));
}

TEST_F(ActorToolFactoryTest, CreateTool_ToolsFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kActorTools);

  optimization_guide::proto::Action action;
  action.mutable_click();

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> result =
      factory_->CreateTool(ActorToolRequest(action), /*tool_delegate=*/nullptr);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(InternalToolErrorCode::kToolDisabledByFeature,
            result.error().internal_code().value());
}

// Verifies that you can create WaitTool without a tab_id.
TEST_F(ActorToolFactoryTest, CreateWaitTool) {
  optimization_guide::proto::Action action;
  action.mutable_wait();

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> result =
      factory_->CreateTool(ActorToolRequest(action), /*tool_delegate=*/nullptr);

  EXPECT_TRUE(result.has_value());
}

// Verifies that if WaitAction has a tab_id, it must be valid and for a real
// WebState.
TEST_F(ActorToolFactoryTest, CreateWaitTool_TabIdMustBeValid) {
  // Create a WaitTool action with an invalid WebStateID. It should fail.
  optimization_guide::proto::Action action;
  action.mutable_wait()->set_observe_tab_id(999);

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> result =
      factory_->CreateTool(ActorToolRequest(action), /*tool_delegate=*/nullptr);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), mojom::ActionResultCode::kTabWentAway);

  // Create a real WebState, insert it, and use its ID.
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  action.mutable_wait()->set_observe_tab_id(tab_id);

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> result2 =
      factory_->CreateTool(ActorToolRequest(action), /*tool_delegate=*/nullptr);

  EXPECT_TRUE(result2.has_value());
}

struct TabIdRequiredTestParam {
  std::string name;
  void (*init_action_func)(optimization_guide::proto::Action&);
};

class ActorToolFactoryTabIdRequiredTest
    : public ActorToolFactoryTest,
      public ::testing::WithParamInterface<TabIdRequiredTestParam> {
 protected:
  void SetUp() override {
    ActorToolFactoryTest::SetUp();

    // Create a real WebState, insert it, and store its ID.
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    tab_id_ = web_state->GetUniqueIdentifier().identifier();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

  // Returns an Action configured with the optional tab ID.
  optimization_guide::proto::Action GetAction(
      const TabIdRequiredTestParam& param,
      std::optional<int> tab_id = std::nullopt) {
    optimization_guide::proto::Action action;
    param.init_action_func(action);
    if (tab_id.has_value()) {
      SetTabId(action, *tab_id);
    }
    return action;
  }

  int tab_id_ = 0;

 private:
  void SetTabId(optimization_guide::proto::Action& action, int tab_id) {
    switch (action.action_case()) {
      case optimization_guide::proto::Action::kNavigate:
        action.mutable_navigate()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kClick:
        action.mutable_click()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kBack:
        action.mutable_back()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kForward:
        action.mutable_forward()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kSelect:
        action.mutable_select()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kType:
        action.mutable_type()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kScroll:
        action.mutable_scroll()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kScrollTo:
        action.mutable_scroll_to()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kAttemptLogin:
        action.mutable_attempt_login()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kAttemptFormFilling:
        action.mutable_attempt_form_filling()->set_tab_id(tab_id);
        break;
      case optimization_guide::proto::Action::kCloseTab:
        action.mutable_close_tab()->set_tab_id(tab_id);
        break;
      default:
        NOTREACHED();
    }
  }
};

// Verifies that for all tools requiring a tab ID, the factory rejects requests
// with missing or invalid tab IDs, and successfully creates tools when a valid
// tab ID is provided.
TEST_P(ActorToolFactoryTabIdRequiredTest, CreateTool_TabIdRequired) {
  const TabIdRequiredTestParam& param = GetParam();

  // 1. Without tab ID, creation must fail with kArgumentsInvalid.
  optimization_guide::proto::Action missing_tab_action =
      GetAction(param, /*tab_id=*/std::nullopt);

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> missing_tab =
      factory_->CreateTool(ActorToolRequest(missing_tab_action),
                           /*tool_delegate=*/nullptr);
  EXPECT_FALSE(missing_tab.has_value());
  EXPECT_EQ(missing_tab.error().code(),
            mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_FALSE(missing_tab.error().internal_code().has_value());

  // 2. With invalid tab ID (999), creation must fail with kTabWentAway.
  optimization_guide::proto::Action invalid_tab_action = GetAction(param, 999);

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> invalid_tab =
      factory_->CreateTool(ActorToolRequest(invalid_tab_action),
                           /*tool_delegate=*/nullptr);
  EXPECT_FALSE(invalid_tab.has_value());
  EXPECT_EQ(invalid_tab.error().code(), mojom::ActionResultCode::kTabWentAway);

  // 3. With valid tab ID, creation must succeed.
  optimization_guide::proto::Action valid_tab_action =
      GetAction(param, tab_id_);

  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> valid_tab =
      factory_->CreateTool(ActorToolRequest(valid_tab_action),
                           /*tool_delegate=*/nullptr);
  EXPECT_TRUE(valid_tab.has_value())
      << "Failed with: " << GetToolExecutionResultMessage(valid_tab.error());
}

INSTANTIATE_TEST_SUITE_P(
    AllCapabilities,
    ActorToolFactoryTabIdRequiredTest,
    ::testing::Values(
        TabIdRequiredTestParam{"Navigate",
                               [](optimization_guide::proto::Action& a) {
                                 a.mutable_navigate()->set_url(
                                     "https://example.com");
                               }},
        TabIdRequiredTestParam{
            "Click",
            [](optimization_guide::proto::Action& a) {
              a.mutable_click()->mutable_target()->mutable_coordinate()->set_x(
                  0);
            }},
        TabIdRequiredTestParam{
            "Back",
            [](optimization_guide::proto::Action& a) { a.mutable_back(); }},
        TabIdRequiredTestParam{
            "Forward",
            [](optimization_guide::proto::Action& a) { a.mutable_forward(); }},
        TabIdRequiredTestParam{
            "Select",
            [](optimization_guide::proto::Action& a) {
              a.mutable_select()->set_value("dummy");
              a.mutable_select()->mutable_target()->mutable_coordinate()->set_x(
                  0);
            }},
        TabIdRequiredTestParam{
            "Type",
            [](optimization_guide::proto::Action& a) {
              a.mutable_type()->set_text("test");
              a.mutable_type()->mutable_target()->mutable_coordinate()->set_x(
                  0);
              a.mutable_type()->set_mode(
                  optimization_guide::proto::TypeAction::DELETE_EXISTING);
            }},
        TabIdRequiredTestParam{
            "Scroll",
            [](optimization_guide::proto::Action& a) {
              a.mutable_scroll()->set_direction(
                  optimization_guide::proto::ScrollAction::DOWN);
              a.mutable_scroll()->set_distance(1.0f);
            }},
        TabIdRequiredTestParam{"ScrollTo",
                               [](optimization_guide::proto::Action& a) {
                                 a.mutable_scroll_to()
                                     ->mutable_target()
                                     ->mutable_coordinate()
                                     ->set_x(0);
                               }},
        TabIdRequiredTestParam{"AttemptLogin",
                               [](optimization_guide::proto::Action& a) {
                                 a.mutable_attempt_login();
                               }},
        TabIdRequiredTestParam{"AttemptFormFilling",
                               [](optimization_guide::proto::Action& a) {
                                 a.mutable_attempt_form_filling();
                               }},
        TabIdRequiredTestParam{"CloseTab",
                               [](optimization_guide::proto::Action& a) {
                                 a.mutable_close_tab();
                               }}),
    [](const ::testing::TestParamInfo<TabIdRequiredTestParam>& info) {
      return info.param.name;
    });

}  // namespace actor
