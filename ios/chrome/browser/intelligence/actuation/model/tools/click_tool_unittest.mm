// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actuation/model/actuation_error.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/actuation_tool.h"
#import "ios/chrome/browser/intelligence/actuation/model/tools/click_tool_java_script_feature.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ActuationCallback = ActuationTool::ActuationCallback;
using ActuationResult = ActuationTool::ActuationResult;

class ClickToolTest : public PlatformTest {
 public:
  ClickToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

TEST_F(ClickToolTest, Create_MissingTabId) {
  optimization_guide::proto::Action action;
  action.mutable_click()->mutable_target()->set_content_node_id(123);

  base::expected<std::unique_ptr<ClickTool>, ActuationError> result =
      ClickTool::Create(action.click(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);
}

TEST_F(ClickToolTest, Create_NoWebStateForTabId) {
  optimization_guide::proto::Action action;
  action.mutable_click()->set_tab_id(1);

  base::expected<std::unique_ptr<ClickTool>, ActuationError> result =
      ClickTool::Create(action.click(), profile_.get());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationTargetTabNotFound,
            result.error().code);
}

TEST_F(ClickToolTest, Create_MissingClickCount) {
  optimization_guide::proto::Action action;
  action.mutable_click()->set_tab_id(1);
  action.mutable_click()->set_click_type(
      optimization_guide::proto::ClickAction::LEFT);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  action.mutable_click()->set_tab_id(tab_id);

  base::expected<std::unique_ptr<ClickTool>, ActuationError> result =
      ClickTool::Create(action.click(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);
}

TEST_F(ClickToolTest, Create_MissingClickType) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  action.mutable_click()->set_tab_id(tab_id);
  action.mutable_click()->set_click_count(
      optimization_guide::proto::ClickAction::SINGLE);

  base::expected<std::unique_ptr<ClickTool>, ActuationError> result =
      ClickTool::Create(action.click(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);
}

TEST_F(ClickToolTest, Create_MissingTarget) {
  optimization_guide::proto::Action action;
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  int tab_id = web_state->GetUniqueIdentifier().identifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());

  action.mutable_click()->set_tab_id(tab_id);
  action.mutable_click()->set_click_count(
      optimization_guide::proto::ClickAction::SINGLE);
  action.mutable_click()->set_click_type(
      optimization_guide::proto::ClickAction::LEFT);

  base::expected<std::unique_ptr<ClickTool>, ActuationError> result =
      ClickTool::Create(action.click(), profile_.get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kCreationMissingRequiredFields,
            result.error().code);
}

TEST_F(ClickToolTest, Execute_WebStateDestroyed_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  auto* click_action = action.mutable_click();
  click_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  click_action->mutable_target()->mutable_coordinate()->set_x(50);
  click_action->mutable_target()->mutable_coordinate()->set_y(50);
  click_action->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click_action->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  auto create_result = ClickTool::Create(action.click(), profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<ClickTool> tool = std::move(create_result.value());

  browser_->GetWebStateList()->CloseWebStateAt(
      web_state_index, WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kExecutionMissingDependencies,
            result.error().code);
}

TEST_F(ClickToolTest, Execute_NoWebFramesManager_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(
      web_state->GetWebFramesManager(ClickToolJavaScriptFeature::GetInstance()
                                         ->GetSupportedContentWorld()),
      nullptr);
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  auto* click_action = action.mutable_click();
  click_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  click_action->mutable_target()->mutable_coordinate()->set_x(50);
  click_action->mutable_target()->mutable_coordinate()->set_y(50);
  click_action->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click_action->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  auto create_result = ClickTool::Create(action.click(), profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<ClickTool> tool = std::move(create_result.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kExecutionMissingDependencies,
            result.error().code);
}

TEST_F(ClickToolTest, Execute_NoMainFrame_ReturnsError) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      ClickToolJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_NE(
      web_state->GetWebFramesManager(ClickToolJavaScriptFeature::GetInstance()
                                         ->GetSupportedContentWorld()),
      nullptr);
  ASSERT_EQ(web_state
                ->GetWebFramesManager(ClickToolJavaScriptFeature::GetInstance()
                                          ->GetSupportedContentWorld())
                ->GetMainWebFrame(),
            nullptr);

  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::Action action;
  auto* click_action = action.mutable_click();
  click_action->set_tab_id(
      inserted_web_state->GetUniqueIdentifier().identifier());
  click_action->mutable_target()->mutable_coordinate()->set_x(50);
  click_action->mutable_target()->mutable_coordinate()->set_y(50);
  click_action->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click_action->set_click_count(optimization_guide::proto::ClickAction::SINGLE);
  auto create_result = ClickTool::Create(action.click(), profile_.get());
  ASSERT_TRUE(create_result.has_value());
  std::unique_ptr<ClickTool> tool = std::move(create_result.value());

  base::test::TestFuture<ActuationResult> future;
  tool->Execute(future.GetCallback());

  ActuationResult result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(ActuationErrorCode::kExecutionMissingDependencies,
            result.error().code);
}
