// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool.h"

#import "base/test/test_future.h"
#import "base/values.h"
#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/form_util/child_frame_registrar.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/action_target_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/attempt_form_filling_tool_java_script_feature.h"
#import "ios/chrome/browser/intelligence/actor/tools/public/actor_tool_types.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

// Unit test fixture for `AttemptFormFillingTool`.
class AttemptFormFillingToolTest : public PlatformTest {
 public:
  AttemptFormFillingToolTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;

  std::unique_ptr<AttemptFormFillingTool> CreateTool(
      const optimization_guide::proto::AttemptFormFillingAction& action,
      web::WebState* web_state) {
    return AttemptFormFillingTool::Create(web_state->GetWeakPtr(), action,
                                          /*tool_delegate=*/nullptr);
  }
};

// Test that validation fails if the action contains no form filling requests.
TEST_F(AttemptFormFillingToolTest, Validate_EmptyRequestsFails) {
  optimization_guide::proto::AttemptFormFillingAction action;
  auto web_state = std::make_unique<web::FakeWebState>();

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());
  ToolExecutionResult result = future.Get();

  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that validation fails if one of the requests has no trigger fields.
TEST_F(AttemptFormFillingToolTest, Validate_OneRequestEmptyTriggerFieldsFails) {
  auto web_state = std::make_unique<web::FakeWebState>();

  optimization_guide::proto::AttemptFormFillingAction action;
  // Request 0: has 1 trigger field.
  auto* request0 = action.add_form_filling_requests();
  request0->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request0->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  // Request 1: has 0 trigger fields.
  auto* request1 = action.add_form_filling_requests();
  request1->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that validation fails if one of the requests has an invalid trigger
// field.
TEST_F(AttemptFormFillingToolTest,
       Validate_OneRequestInvalidTriggerFieldFails) {
  auto web_state = std::make_unique<web::FakeWebState>();

  optimization_guide::proto::AttemptFormFillingAction action;
  // Request 0: has 1 trigger field.
  auto* request0 = action.add_form_filling_requests();
  request0->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field0 = request0->add_trigger_fields();
  field0->mutable_coordinate()->set_x(10);
  field0->mutable_coordinate()->set_y(20);

  // Request 1: has 1 invalid trigger field (neither node ID nor coordinate is
  // set).
  auto* request1 = action.add_form_filling_requests();
  request1->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_CREDIT_CARD);
  request1->add_trigger_fields();

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Validate(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kArgumentsInvalid);
}

// Test that execution fails if the WebState is destroyed.
TEST_F(AttemptFormFillingToolTest, Execute_WebStateDestroyedFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  int web_state_index = browser_->GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::AtIndex(0).Activate());
  web::WebState* inserted_web_state =
      browser_->GetWebStateList()->GetWebStateAt(web_state_index);

  optimization_guide::proto::AttemptFormFillingAction action;
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, inserted_web_state);
  ASSERT_TRUE(tool);

  browser_->GetWebStateList()->CloseWebStateAt(
      web_state_index, WebStateList::ClosingReason::kDefault);
  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kTabWentAway);
}

// Test that execution fails if the WebFramesManager is null.
TEST_F(AttemptFormFillingToolTest, Execute_NoWebFramesManagerFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  ASSERT_EQ(web_state->GetWebFramesManager(
                ActionTargetJavaScriptFeature::GetInstance()
                    ->GetSupportedContentWorld()),
            nullptr);

  optimization_guide::proto::AttemptFormFillingAction action;
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Test that execution fails if there is no main web frame.
TEST_F(AttemptFormFillingToolTest, Execute_NoMainFrameFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetWebFramesManager(
      ActionTargetJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::make_unique<web::FakeWebFramesManager>());
  ASSERT_EQ(
      web_state
          ->GetWebFramesManager(ActionTargetJavaScriptFeature::GetInstance()
                                    ->GetSupportedContentWorld())
          ->GetMainWebFrame(),
      nullptr);

  optimization_guide::proto::AttemptFormFillingAction action;
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.code(), mojom::ActionResultCode::kFrameWentAway);
}

// Test that if target frame resolution returns an invalid result, the tool
// execution fails.
TEST_F(AttemptFormFillingToolTest,
       Execute_ResolveTargetFrameInvalidResultFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::test::OverrideJavaScriptFeatures(
      profile_.get(), {
                          ActionTargetJavaScriptFeature::GetInstance(),
                      });
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
  main_frame->set_browser_state(profile_.get());

  // Set up invalid JS result for resolved target frame.
  base::Value js_result(12345);  // non-dictionary is invalid
  main_frame->AddJsResultForFunctionCall(&js_result,
                                         "action_target.resolveTargetIframe");

  frames_manager->AddWebFrame(std::move(main_frame));
  web_state->SetWebFramesManager(
      ActionTargetJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
      std::move(frames_manager));

  optimization_guide::proto::AttemptFormFillingAction action;
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

// Test that if resolving the Autofill renderer IDs returns an invalid result,
// the tool execution fails.
TEST_F(AttemptFormFillingToolTest,
       Execute_GetAutofillRendererIdsInvalidResultFails) {
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web::test::OverrideJavaScriptFeatures(
      profile_.get(),
      {
          ActionTargetJavaScriptFeature::GetInstance(),
          AttemptFormFillingToolJavaScriptFeature::GetInstance(),
      });
  auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
  auto main_frame = web::FakeWebFrame::CreateMainWebFrame();
  main_frame->set_browser_state(profile_.get());

  // Set up success for target frame resolution (returns none/null).
  base::Value resolve_iframe_result;  // default constructor is NONE
  main_frame->AddJsResultForFunctionCall(&resolve_iframe_result,
                                         "action_target.resolveTargetIframe");

  // Set up invalid result for getAutofillRendererIds (returns a number instead
  // of a list).
  base::Value renderer_id_result(12345);
  main_frame->AddJsResultForFunctionCall(
      &renderer_id_result, "attempt_form_filling.getAutofillRendererIds");

  frames_manager->AddWebFrame(std::move(main_frame));
  web_state->SetWebFramesManager(
      AttemptFormFillingToolJavaScriptFeature::GetInstance()
          ->GetSupportedContentWorld(),
      std::move(frames_manager));

  optimization_guide::proto::AttemptFormFillingAction action;
  auto* request = action.add_form_filling_requests();
  request->set_requested_data(
      optimization_guide::proto::FormFillingRequest_RequestedData_ADDRESS);
  auto* field = request->add_trigger_fields();
  field->mutable_coordinate()->set_x(10);
  field->mutable_coordinate()->set_y(20);

  std::unique_ptr<AttemptFormFillingTool> tool =
      CreateTool(action, web_state.get());
  ASSERT_TRUE(tool);

  base::test::TestFuture<ToolExecutionResult> future;
  tool->Execute(future.GetCallback());

  ToolExecutionResult result = future.Get();
  EXPECT_FALSE(result.IsOk());
  EXPECT_EQ(result.internal_code().value(),
            InternalToolErrorCode::kJavascriptFeatureGotInvalidResult);
}

}  // namespace actor
