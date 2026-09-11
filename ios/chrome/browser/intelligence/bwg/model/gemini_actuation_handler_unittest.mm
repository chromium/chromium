// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_handler.h"

#import "base/functional/callback_helpers.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
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
        web::WebStateID::FromSerializedValue(123));
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

// Tests that performActions returns a failure result when passed an invalid
// serialized proto.
TEST_F(GeminiActuationHandlerTest, PerformActions_InvalidProto) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  NSData* invalid_data = [@"invalid" dataUsingEncoding:NSUTF8StringEncoding];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    EXPECT_NE(nil, serializedActionsResult);
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(actions_result.action_result(),
                static_cast<int32_t>(
                    actor::mojom::ActionResultCode::kArgumentsInvalid));
      EXPECT_EQ(actions_result.error_message(), "Failed to parse action proto");
    }
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ invalid_data ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions completes with success and extracts tab observation
// when passed an empty list of protos.
TEST_F(GeminiActuationHandlerTest, PerformActions_EmptyProtos) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(actions_result.action_result(),
                static_cast<int32_t>(actor::mojom::ActionResultCode::kOk));
      EXPECT_EQ(actions_result.tabs_size(), 1);
      EXPECT_EQ(actions_result.tabs(0).id(), 123);
    }
    quit_closure.Run();
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[]
                    completionBlock:completionBlock];
  run_loop.Run();
  EXPECT_TRUE(callback_called);
}

// Tests that performActions can parse multiple valid protos.
TEST_F(GeminiActuationHandlerTest, PerformActions_MultipleProtos) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  optimization_guide::proto::Action action1;
  std::string serialized1;
  action1.SerializeToString(&serialized1);
  NSData* data1 = [NSData dataWithBytes:serialized1.data()
                                 length:serialized1.size()];

  optimization_guide::proto::Action action2;
  std::string serialized2;
  action2.SerializeToString(&serialized2);
  NSData* data2 = [NSData dataWithBytes:serialized2.data()
                                 length:serialized2.size()];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(
          actions_result.action_result(),
          static_cast<int32_t>(actor::mojom::ActionResultCode::kToolUnknown));
    }
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data1, data2 ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that pauseTaskWithID does not crash.
TEST_F(GeminiActuationHandlerTest, PauseTask) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  [handler pauseTaskWithID:task_id];
}

// Tests that stopTaskWithID does not crash.
TEST_F(GeminiActuationHandlerTest, StopTask) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  [handler stopTaskWithID:task_id
                   reason:actor::ActorTaskStoppedReason::kTaskComplete];
}

// Tests that requestActionablePageContextForWebStateIDs returns
// TAB_OBSERVATION_TAB_WENT_AWAY when tab is not found.
TEST_F(GeminiActuationHandlerTest, RequestActionablePageContext_NotFound) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSArray<NSData*>* serializedTabObservations) {
    callback_called = YES;
    EXPECT_EQ([serializedTabObservations count], 1u);
    if ([serializedTabObservations count] > 0) {
      optimization_guide::proto::TabObservation tabObservation;
      EXPECT_TRUE(
          tabObservation.ParseFromArray([serializedTabObservations[0] bytes],
                                        [serializedTabObservations[0] length]));
      EXPECT_EQ(tabObservation.result(),
                optimization_guide::proto::TabObservation::
                    TAB_OBSERVATION_TAB_WENT_AWAY);
    }
  };

  [handler requestActionablePageContextForWebStateIDs:@[ @999 ]
                                               taskID:task_id
                                      completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that requestActionablePageContextForWebStateIDs returns errors for
// all missing tabs.
TEST_F(GeminiActuationHandlerTest,
       RequestActionablePageContext_MultipleNotFound) {
  GeminiActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSArray<NSData*>* serializedTabObservations) {
    callback_called = YES;
    EXPECT_EQ([serializedTabObservations count], 2u);
    for (NSData* data in serializedTabObservations) {
      optimization_guide::proto::TabObservation tabObservation;
      EXPECT_TRUE(tabObservation.ParseFromArray([data bytes], [data length]));
      EXPECT_EQ(tabObservation.result(),
                optimization_guide::proto::TabObservation::
                    TAB_OBSERVATION_TAB_WENT_AWAY);
    }
  };

  [handler requestActionablePageContextForWebStateIDs:@[ @999, @888 ]
                                               taskID:task_id
                                      completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions correctly injects the active tab ID into an action
// proto lacking tab_id.
TEST_F(GeminiActuationHandlerTest, PerformActions_InjectsTabId) {
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

  base::test::TestFuture<NSData*> future;
  base::test::TestFuture<NSData*>* future_ptr = &future;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    future_ptr->SetValue(serializedActionsResult);
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];

  NSData* serializedActionsResult = future.Get();
  ASSERT_NE(nil, serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(actions_result.ParseFromArray([serializedActionsResult bytes],
                                            [serializedActionsResult length]));

  // If the tab ID injection succeeded, the Actor Tool Factory will
  // successfully resolve the tab (finding our active fake WebState with ID
  // 123), and not failing with an invalid arguments error due to a missing
  // tab ID.
  std::string invalid_args_error =
      actor::GetToolExecutionResultMessage(actor::ToolExecutionResult(
          actor::mojom::ActionResultCode::kArgumentsInvalid));
  EXPECT_NE(actions_result.error_message(), invalid_args_error);
}

// Tests that performActions returns a failure result when passed an unmapped
// task ID.
TEST_F(GeminiActuationHandlerTest, PerformActions_UnmappedTaskId) {
  GeminiActuationHandler* handler = CreateHandler();

  // Generate an arbitrary task ID that is not mapped.
  actor::ActorTaskId unmapped_task_id = actor::ActorTaskId();

  optimization_guide::proto::Action action;
  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    ASSERT_NE(nil, serializedActionsResult);
    optimization_guide::proto::ActionsResult actions_result;
    EXPECT_TRUE(actions_result.ParseFromArray(
        [serializedActionsResult bytes], [serializedActionsResult length]));
    EXPECT_EQ(
        actions_result.action_result(),
        static_cast<int32_t>(actor::mojom::ActionResultCode::kTaskWentAway));
    EXPECT_EQ(actions_result.error_message(),
              "Failed to perform actions: Task ID not found.");
  };

  [handler performActionsWithTaskID:unmapped_task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];
  EXPECT_TRUE(callback_called);
}

// Tests that performActions correctly sets the error code returned by the tool
// execution.
TEST_F(GeminiActuationHandlerTest, PerformActions_FailureCodePropagated) {
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

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  __block BOOL callback_called = NO;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    callback_called = YES;
    if (serializedActionsResult) {
      optimization_guide::proto::ActionsResult actions_result;
      EXPECT_TRUE(actions_result.ParseFromArray(
          [serializedActionsResult bytes], [serializedActionsResult length]));
      EXPECT_EQ(actions_result.action_result(),
                static_cast<int32_t>(
                    actor::mojom::ActionResultCode::kNavigateFailedToStart));
      EXPECT_EQ(actions_result.index_of_failed_action(), 0);
    }
    quit_closure.Run();
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];
  run_loop.Run();
  EXPECT_TRUE(callback_called);

  [handler stopTaskWithID:task_id
                   reason:actor::ActorTaskStoppedReason::kTaskComplete];
}

// Tests that performActions correctly injects the browser's window ID into a
// CreateTab action.
TEST_F(GeminiActuationHandlerTest, PerformActions_InjectsWindowId) {
  GeminiActuationHandler* handler = CreateHandler();

  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];

  // Create a CreateTab proto without a window_id.
  optimization_guide::proto::Action action;
  action.mutable_create_tab();

  std::string serialized;
  action.SerializeToString(&serialized);
  NSData* data = [NSData dataWithBytes:serialized.data()
                                length:serialized.size()];

  base::test::TestFuture<NSData*> future;
  base::test::TestFuture<NSData*>* future_ptr = &future;
  auto completionBlock = ^(NSData* serializedActionsResult) {
    future_ptr->SetValue(serializedActionsResult);
  };

  [handler performActionsWithTaskID:task_id
                         taskUpdate:@"Update"
             serializedActionProtos:@[ data ]
                    completionBlock:completionBlock];

  NSData* serializedActionsResult = future.Get();
  ASSERT_NE(nil, serializedActionsResult);
  optimization_guide::proto::ActionsResult actions_result;
  EXPECT_TRUE(actions_result.ParseFromArray([serializedActionsResult bytes],
                                            [serializedActionsResult length]));

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

// Tests that `dispatchActuationRequest` routes `kConfirmation`
// and responds with `kOk`.
TEST_F(GeminiActuationHandlerTest,
       DispatchGeminiActuationRequest_Yield_Confirmation) {
  TestInterruptReason(GeminiYieldReason::kConfirmation);
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
