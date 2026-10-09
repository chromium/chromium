// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/test/fake_actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_data_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// The serialized identifier of the fixture's fake WebState.
constexpr int32_t kFakeWebStateID = 123;

// Message carried by the confirmation yield actions under test.
NSString* const kConfirmationMessage = @"Please confirm this purchase.";

// Mirrors `kConfirmationUserResponse` in `gemini_actuation_handler.mm`.
// TODO(crbug.com/571050191): Use the localized resource if the string gets
// localized.
NSString* const kExpectedUserResponse =
    @"I agree with what you wanted me to confirm. Please proceed.";

class GeminiActuationHandlerTest : public PlatformTest {
 public:
  GeminiActuationHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    ActorBrowserAgent::CreateForBrowser(browser_.get());

    auto fake_web_state = std::make_unique<web::FakeWebState>(
        web::WebStateID::FromSerializedValue(kFakeWebStateID));
    fake_web_state->SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    fake_web_state->SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    fake_web_state_ = fake_web_state.get();

    browser_->GetWebStateList()->InsertWebState(
        std::move(fake_web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

 protected:
  GeminiActuationHandler* CreateHandler() {
    return [[GeminiActuationHandler alloc]
        initWithActorService:actor_service_
                webStateList:browser_->GetWebStateList()
                   browserId:ActorBrowserAgent::FromBrowser(browser_.get())
                                 ->browser_id()];
  }

  void (^GetCompletionBlock(base::test::TestFuture<GeminiActuationResponse*>&
                                future))(GeminiActuationResponse*) {
    base::test::TestFuture<GeminiActuationResponse*>* future_ptr = &future;
    return ^(GeminiActuationResponse* response) {
      future_ptr->SetValue(response);
    };
  }

  void TestInterruptReason(GeminiYieldReason reason) {
    GeminiActuationHandler* handler = CreateHandler();
    actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

    GeminiYieldAction* yield_action =
        [[GeminiYieldAction alloc] initWithReason:reason
                                    messageToUser:@"User intervention needed."];
    GeminiActuationRequest* request =
        [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                  taskUpdate:nil
                                                 yieldAction:yield_action];

    base::test::TestFuture<GeminiActuationResponse*> future;
    [handler dispatchActuationRequest:request
                            forTaskID:task_id
                      completionBlock:GetCompletionBlock(future)];

    GeminiActuationResponse* response = future.Get();
    ASSERT_NE(nil, response);
    EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);
  }

  // Returns a `kConfirmation` yield request carrying `message`.
  GeminiActuationRequest* CreateConfirmationRequest(NSString* message) {
    GeminiYieldAction* yield_action = [[GeminiYieldAction alloc]
        initWithReason:GeminiYieldReason::kConfirmation
         messageToUser:message];
    return [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                     taskUpdate:nil
                                                    yieldAction:yield_action];
  }

  // Registers a fake intervention delegate for `task_id` and dispatches a
  // `kConfirmation` yield request carrying `message`. The request stays pending
  // until the returned delegate's completion handler runs.
  FakeActorTaskInterventionDelegate* DispatchConfirmationRequest(
      GeminiActuationHandler* handler,
      actor::ActorTaskId task_id,
      NSString* message,
      base::test::TestFuture<GeminiActuationResponse*>& future) {
    FakeActorTaskInterventionDelegate* delegate =
        [[FakeActorTaskInterventionDelegate alloc] init];
    actor_service_->SetTaskInterventionDelegate(task_id, delegate);
    [handler dispatchActuationRequest:CreateConfirmationRequest(message)
                            forTaskID:task_id
                      completionBlock:GetCompletionBlock(future)];
    // `ActorTask` posts the prompt; wait until the prompt is delivered or the
    // request is answered.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return delegate.requestConfirmationCalled || future.IsReady();
    }));
    return delegate;
  }

  // Verifies that `response` reports an accepted confirmation along with an
  // observation of the fixture's WebState.
  void ExpectConfirmedResponse(GeminiActuationResponse* response) {
    ASSERT_NE(nil, response);
    EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);
    EXPECT_NSEQ(kExpectedUserResponse, response.userResponse);
    ASSERT_NE(nil, response.serializedActionsResult);
    optimization_guide::proto::ActionsResult actions_result;
    ASSERT_TRUE(actions_result.ParseFromArray(
        [response.serializedActionsResult bytes],
        [response.serializedActionsResult length]));
    EXPECT_EQ(static_cast<int32_t>(actor::mojom::ActionResultCode::kOk),
              actions_result.action_result());
    ASSERT_EQ(1, actions_result.tabs_size());
    EXPECT_EQ(kFakeWebStateID, actions_result.tabs(0).id());
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> fake_web_state_;
};

// Tests that a GeminiActuationHandler can be initialized.
TEST_F(GeminiActuationHandlerTest, Initialization) {
  ASSERT_NE(nullptr, actor_service_);
  GeminiActuationHandler* handler = CreateHandler();
  EXPECT_NE(nil, handler);
}

// Tests that createTaskWithTitle returns a valid task ID.
TEST_F(GeminiActuationHandlerTest, CreateTask) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];
  EXPECT_FALSE(task_id.is_null());
}

// Tests that `dispatchActuationRequest` correctly injects the active tab ID
// into an action proto lacking `tab_id`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_InjectsTabId) {
  GeminiActuationHandler* handler = CreateHandler();

  // Activate the fake web state (index 0).
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  // Create a ClickAction proto without a tab_id.
  optimization_guide::proto::Action action;
  auto* click = action.mutable_click();
  click->mutable_target()->mutable_coordinate()->set_x(50);
  click->mutable_target()->mutable_coordinate()->set_y(50);
  click->set_click_type(optimization_guide::proto::ClickAction::LEFT);
  click->set_click_count(optimization_guide::proto::ClickAction::SINGLE);

  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data ]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));

  // If the tab ID injection succeeded, the Actor Tool Factory will
  // successfully resolve the tab (finding our active fake WebState with ID
  // 123), and not failing with an invalid arguments error due to a missing
  // tab ID.
  std::string invalid_args_error =
      actor::GetToolExecutionResultMessage(actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kArgumentsInvalid));
  EXPECT_NE(actions_result.error_message(), invalid_args_error);
}

// Tests that `dispatchActuationRequest` correctly sets the error code returned
// by the tool execution.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_FailureCodePropagated) {
  UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
  UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
  // Mark the tab as unrealized so that NavigateTool execution fails.
  fake_web_state_->SetIsRealized(false);

  GeminiActuationHandler* handler = CreateHandler();

  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action;
  auto* navigate = action.mutable_navigate();
  navigate->set_tab_id(123);
  navigate->set_url("https://example.com");

  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data ]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kNavigateFailedToStart,
            response.resultCode);

  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));
  EXPECT_EQ(actions_result.action_result(),
            static_cast<int32_t>(
                actor::mojom::ActionResultCode::kNavigateFailedToStart));
  EXPECT_EQ(actions_result.index_of_failed_action(), 0);
}

// Tests that `dispatchActuationRequest` correctly injects the browser's window
// ID into a `CreateTab` action.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_InjectsWindowId) {
  GeminiActuationHandler* handler = CreateHandler();

  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  // Create a CreateTab proto without a window_id.
  optimization_guide::proto::Action action;
  action.mutable_create_tab();

  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data ]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kNewTabCreationFailed,
            response.resultCode);

  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));

  // If window_id was injected, it should pass validation and attempt creation,
  // failing with kNewTabCreationFailed because TabInsertionBrowserAgent is
  // not present on our TestBrowser. If window_id is not injected, it will
  // fail validation with kWindowWentAway.
  std::string creation_failed_error =
      actor::GetToolExecutionResultMessage(actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kNewTabCreationFailed));
  EXPECT_EQ(actions_result.error_message(), creation_failed_error);
}

// Tests that `GeminiYieldAction` stores initialized properties correctly.
TEST_F(GeminiActuationHandlerTest, GeminiYieldAction_Initialization) {
  GeminiYieldAction* action = [[GeminiYieldAction alloc]
      initWithReason:GeminiYieldReason::kClarification
       messageToUser:@"Please confirm"];
  EXPECT_EQ(GeminiYieldReason::kClarification, action.reason);
  EXPECT_NSEQ(@"Please confirm", action.messageToUser);
}

// Tests that `GeminiActuationRequest` stores initialized properties correctly.
TEST_F(GeminiActuationHandlerTest, GeminiActuationRequest_Initialization) {
  NSData* proto = [@"proto" dataUsingEncoding:NSUTF8StringEncoding];
  GeminiActuationRequest* action_request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ proto ]
                                                taskUpdate:@"Task Update"
                                               yieldAction:nil];
  EXPECT_EQ(1u, action_request.actionProtos.count);
  EXPECT_NSEQ(@"Task Update", action_request.taskUpdate);
  EXPECT_EQ(nil, action_request.yieldAction);

  GeminiYieldAction* yield_action =
      [[GeminiYieldAction alloc] initWithReason:GeminiYieldReason::kUserTakeover
                                  messageToUser:@"Please confirm"];
  GeminiActuationRequest* yield_request =
      [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                taskUpdate:@"Yield Update"
                                               yieldAction:yield_action];
  EXPECT_EQ(nil, yield_request.actionProtos);
  EXPECT_NSEQ(@"Yield Update", yield_request.taskUpdate);
  EXPECT_EQ(yield_action, yield_request.yieldAction);
}

// Tests that `GeminiActuationResponse` stores initialized properties correctly.
TEST_F(GeminiActuationHandlerTest, GeminiActuationResponse_Initialization) {
  NSData* result_data = [@"result" dataUsingEncoding:NSUTF8StringEncoding];
  GeminiActuationResponse* response = [[GeminiActuationResponse alloc]
           initWithResultCode:actor::mojom::ActionResultCode::kOk
                 userResponse:@"User confirmed"
      serializedActionsResult:result_data];
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);
  EXPECT_NSEQ(@"User confirmed", response.userResponse);
  EXPECT_NSEQ(result_data, response.serializedActionsResult);

  GeminiActuationResponse* failure_response = [[GeminiActuationResponse alloc]
      initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
            errorMessage:"Test error"];
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            failure_response.resultCode);
  EXPECT_EQ(nil, failure_response.userResponse);
  ASSERT_NE(nil, failure_response.serializedActionsResult);
  optimization_guide::proto::ActionsResult failure_actions_result;
  EXPECT_TRUE(failure_actions_result.ParseFromArray(
      [failure_response.serializedActionsResult bytes],
      [failure_response.serializedActionsResult length]));
  EXPECT_EQ(
      failure_actions_result.action_result(),
      static_cast<int32_t>(actor::mojom::ActionResultCode::kArgumentsInvalid));
  EXPECT_EQ(failure_actions_result.error_message(), "Test error");
}

// Tests that `dispatchActuationRequest` returns `kTaskWentAway` when the
// task does not exist.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_TaskNotFound) {
  GeminiActuationHandler* handler = CreateHandler();
  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[]
                                                taskUpdate:nil
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:actor::ActorTaskId(99999)
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway, response.resultCode);
}

// Tests that `dispatchActuationRequest` returns `kTabWentAway` when the
// actuated tab has been closed.
TEST_F(GeminiActuationHandlerTest, DispatchGeminiActuationRequest_TabWentAway) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  // Close the controlled tab.
  fake_web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTabWentAway, response.resultCode);
}

// Tests that `dispatchActuationRequest` fails with `kArgumentsInvalid`
// when an action proto cannot be parsed.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_InvalidProto) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  NSData* invalid_data = [@"invalid" dataUsingEncoding:NSUTF8StringEncoding];
  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ invalid_data ]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.resultCode);

  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));
  EXPECT_EQ(
      actions_result.action_result(),
      static_cast<int32_t>(actor::mojom::ActionResultCode::kArgumentsInvalid));
  EXPECT_EQ(actions_result.error_message(), "Failed to parse action proto");
}

// Tests that `dispatchActuationRequest` fails with `kArgumentsInvalid`
// when both actionProtos and yieldAction are provided.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_BothProtosAndYield_Fails) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiYieldAction* yield_action =
      [[GeminiYieldAction alloc] initWithReason:GeminiYieldReason::kConfirmation
                                  messageToUser:@"Confirm"];

  GeminiActuationRequest* both_request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[]
                                                taskUpdate:@"Update"
                                               yieldAction:yield_action];

  base::test::TestFuture<GeminiActuationResponse*> both_future;
  [handler dispatchActuationRequest:both_request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(both_future)];

  GeminiActuationResponse* both_response = both_future.Get();
  ASSERT_NE(nil, both_response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            both_response.resultCode);
}

// Tests that `dispatchActuationRequest` fails with `kArgumentsInvalid`
// when neither actionProtos nor yieldAction are provided.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_NeitherProtosNorYield_Fails) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiActuationRequest* neither_request =
      [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                taskUpdate:nil
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> neither_future;
  [handler dispatchActuationRequest:neither_request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(neither_future)];

  GeminiActuationResponse* neither_response = neither_future.Get();
  ASSERT_NE(nil, neither_response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            neither_response.resultCode);
}

// Tests that `dispatchActuationRequest` succeeds and captures tab
// observations when given empty protos.
TEST_F(GeminiActuationHandlerTest, DispatchGeminiActuationRequest_EmptyProtos) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);

  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));
  EXPECT_EQ(actions_result.action_result(),
            static_cast<int32_t>(actor::mojom::ActionResultCode::kOk));
  EXPECT_EQ(actions_result.tabs_size(), 1);
  EXPECT_EQ(actions_result.tabs(0).id(), 123);
}

// Tests that `dispatchActuationRequest` parses multiple action protos and
// propagates the tool factory failure for unset actions.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_MultipleProtos) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action;
  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data, data ]
                                                taskUpdate:@"Update"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kToolUnknown, response.resultCode);

  ASSERT_NE(nil, response.serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(
      actions_result.ParseFromArray([response.serializedActionsResult bytes],
                                    [response.serializedActionsResult length]));
  EXPECT_EQ(actions_result.action_result(),
            static_cast<int32_t>(actor::mojom::ActionResultCode::kToolUnknown));
  EXPECT_EQ(actions_result.index_of_failed_action(), 0);
}

// Tests that a `kConfirmation` yield prompts the user through the intervention
// delegate and resolves with the user's acceptance and a tab observation.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  FakeActorTaskInterventionDelegate* delegate = DispatchConfirmationRequest(
      handler, task_id, kConfirmationMessage, future);

  EXPECT_TRUE(delegate.requestConfirmationCalled);
  EXPECT_NSEQ(kConfirmationMessage, delegate.confirmationTitle);
  // The request stays pending until the user answers.
  EXPECT_FALSE(future.IsReady());
  ASSERT_TRUE(delegate.hasPendingUserIntervention);

  [delegate runUserInterventionCompletion];

  ExpectConfirmedResponse(future.Get());
}

// Tests that a `kConfirmation` yield with no message is rejected with
// `kArgumentsInvalid` without prompting, leaving the task live.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_NoMessage) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  FakeActorTaskInterventionDelegate* delegate =
      DispatchConfirmationRequest(handler, task_id, /*message=*/nil, future);

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.resultCode);
  EXPECT_FALSE(delegate.requestConfirmationCalled);
  EXPECT_NE(std::nullopt, actor_service_->GetActiveTaskState());
}

// Tests that a `kConfirmation` yield without an intervention delegate answers
// `kTaskWentAway`, since `ActorTask` stops a task it cannot prompt for.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_NoDelegate) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler
      dispatchActuationRequest:CreateConfirmationRequest(kConfirmationMessage)
                     forTaskID:task_id
               completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway, response.resultCode);
  EXPECT_EQ(std::nullopt, actor_service_->GetActiveTaskState());
}

// Tests that resolving a confirmation after the actuated tab closed fails the
// request with `kTabWentAway`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_TabClosed) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  FakeActorTaskInterventionDelegate* delegate = DispatchConfirmationRequest(
      handler, task_id, kConfirmationMessage, future);
  ASSERT_TRUE(delegate.hasPendingUserIntervention);

  // Close the actuated tab while the confirmation is pending.
  fake_web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kUserAction);

  [delegate runUserInterventionCompletion];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTabWentAway, response.resultCode);
}

// Tests that a duplicate confirmation resolution signal completes the request
// only once.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_DuplicateSignal) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  FakeActorTaskInterventionDelegate* delegate = DispatchConfirmationRequest(
      handler, task_id, kConfirmationMessage, future);
  ASSERT_TRUE(delegate.hasPendingUserIntervention);

  [delegate runUserInterventionCompletion];
  ExpectConfirmedResponse(future.Get());

  // A resolution signal with no pending confirmation must be ignored rather
  // than re-running the consumed callback, which would trip `TestFuture`'s
  // single-value expectation.
  [static_cast<id<ActorTaskUpdatesObserver>>(handler)
      actorTaskDidResolveConfirmationInterruptWithID:task_id];
  EXPECT_EQ(actor::ActorTaskState::kReflecting,
            actor_service_->GetActiveTaskState());
}

// Tests that stopping a task while a confirmation is pending aborts it with
// `kTaskWentAway`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_Stopped_Cancels) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  DispatchConfirmationRequest(handler, task_id, kConfirmationMessage, future);

  actor_service_->StopTask(task_id,
                           actor::ActorTaskStoppedReason::kStoppedByUser);

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway, response.resultCode);
}

// Tests that disconnecting while a confirmation is pending aborts it with
// `kExecutorDestroyed`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_Disconnect_Cancels) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future;
  DispatchConfirmationRequest(handler, task_id, kConfirmationMessage, future);

  [handler disconnect];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutorDestroyed,
            response.resultCode);
}

// Tests that a second request dispatched while a confirmation is pending is
// rejected, and that the pending confirmation still resolves normally.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation_ConcurrentRequest) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future1;
  FakeActorTaskInterventionDelegate* delegate = DispatchConfirmationRequest(
      handler, task_id, kConfirmationMessage, future1);

  GeminiActuationRequest* request2 =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[]
                                                taskUpdate:@"Second Request"
                                               yieldAction:nil];
  base::test::TestFuture<GeminiActuationResponse*> future2;
  [handler dispatchActuationRequest:request2
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future2)];

  GeminiActuationResponse* response2 = future2.Get();
  ASSERT_NE(nil, response2);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response2.resultCode);

  EXPECT_FALSE(future1.IsReady());
  ASSERT_TRUE(delegate.hasPendingUserIntervention);
  [delegate runUserInterventionCompletion];

  ExpectConfirmedResponse(future1.Get());
}

// Tests that a second confirmation dispatched while one is pending is rejected
// without dropping the pending one.
TEST_F(
    GeminiActuationHandlerTest,
    DispatchGeminiActuationRequest_Yield_Confirmation_ConcurrentConfirmation) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  base::test::TestFuture<GeminiActuationResponse*> future1;
  FakeActorTaskInterventionDelegate* delegate = DispatchConfirmationRequest(
      handler, task_id, kConfirmationMessage, future1);

  base::test::TestFuture<GeminiActuationResponse*> future2;
  [handler
      dispatchActuationRequest:CreateConfirmationRequest(kConfirmationMessage)
                     forTaskID:task_id
               completionBlock:GetCompletionBlock(future2)];

  GeminiActuationResponse* response2 = future2.Get();
  ASSERT_NE(nil, response2);
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response2.resultCode);

  EXPECT_FALSE(future1.IsReady());
  ASSERT_TRUE(delegate.hasPendingUserIntervention);
  [delegate runUserInterventionCompletion];

  ExpectConfirmedResponse(future1.Get());
}

// Tests that `dispatchActuationRequest` routes `kClarification`
// and responds with `kOk`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Clarification) {
  TestInterruptReason(GeminiYieldReason::kClarification);
}

// Tests that `dispatchActuationRequest` routes `kUserTakeover`
// and responds with `kOk`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_UserTakeover) {
  TestInterruptReason(GeminiYieldReason::kUserTakeover);
}

// Tests that `dispatchActuationRequest` with `kTaskComplete` stops the
// task and returns `kOk`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Stop_TaskComplete) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiYieldAction* yield_action =
      [[GeminiYieldAction alloc] initWithReason:GeminiYieldReason::kTaskComplete
                                  messageToUser:nil];
  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                taskUpdate:nil
                                               yieldAction:yield_action];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);

  // Subsequent request should fail because task was stopped.
  base::test::TestFuture<GeminiActuationResponse*> subsequent_future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(subsequent_future)];
  GeminiActuationResponse* subsequent_response = subsequent_future.Get();
  ASSERT_NE(nil, subsequent_response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            subsequent_response.resultCode);
}

// Tests that `dispatchActuationRequest` with `kIrrelevantUserInput` stops the
// task and responds with `kOk`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Stop_IrrelevantUserInput) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiYieldAction* yield_action = [[GeminiYieldAction alloc]
      initWithReason:GeminiYieldReason::kIrrelevantUserInput
       messageToUser:nil];
  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                taskUpdate:nil
                                               yieldAction:yield_action];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);

  // Subsequent request should fail because task was stopped.
  base::test::TestFuture<GeminiActuationResponse*> subsequent_future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(subsequent_future)];
  GeminiActuationResponse* subsequent_response = subsequent_future.Get();
  ASSERT_NE(nil, subsequent_response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            subsequent_response.resultCode);
}

// Tests that `dispatchActuationRequest` with `kUnknownReason` pauses the
// task.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Pause_UnknownReason) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  GeminiYieldAction* yield_action = [[GeminiYieldAction alloc]
      initWithReason:GeminiYieldReason::kUnknownReason
       messageToUser:nil];
  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:nil
                                                taskUpdate:nil
                                               yieldAction:yield_action];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.resultCode);
}

// Tests that `disconnect` cancels in-flight requests with `kExecutorDestroyed`.
TEST_F(GeminiActuationHandlerTest, Disconnect_CancelsInFlightRequest) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action;
  action.mutable_create_tab();
  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data ]
                                                taskUpdate:@"Action"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  EXPECT_TRUE(actor_service_->GetActiveTaskState().has_value());

  [handler disconnect];

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutorDestroyed,
            response.resultCode);
  EXPECT_FALSE(actor_service_->GetActiveTaskState().has_value());
}

// Tests that an external task stop cancels in-flight requests with
// `kTaskWentAway`.
TEST_F(GeminiActuationHandlerTest, ExternalStop_CancelsInFlightRequest) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action;
  action.mutable_create_tab();
  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  GeminiActuationRequest* request =
      [[GeminiActuationRequest alloc] initWithActionProtos:@[ data ]
                                                taskUpdate:@"Action"
                                               yieldAction:nil];

  base::test::TestFuture<GeminiActuationResponse*> future;
  [handler dispatchActuationRequest:request
                          forTaskID:task_id
                    completionBlock:GetCompletionBlock(future)];

  actor_service_->StopTask(task_id,
                           actor::ActorTaskStoppedReason::kStoppedByUser);

  GeminiActuationResponse* response = future.Get();
  ASSERT_NE(nil, response);
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway, response.resultCode);
}

}  // namespace
