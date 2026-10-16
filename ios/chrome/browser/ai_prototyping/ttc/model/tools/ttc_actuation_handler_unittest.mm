// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <memory>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_browser_agent.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

base::DictValue CreateUrlArgs(const std::string& url) {
  return base::DictValue().Set("url", url);
}

class TtcActuationHandlerTest : public PlatformTest {
 public:
  TtcActuationHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    profile_ = TestProfileIOS::Builder().Build();
    actor_service_ = actor::ActorServiceFactory::GetForProfile(profile_.get());

    browser_ = std::make_unique<TestBrowser>(profile_.get());
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    ActorBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());

    browser_->GetWebStateList()->InsertWebState(
        CreateFakeWebState(123, profile_.get()),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

 protected:
  std::unique_ptr<web::FakeWebState> CreateFakeWebState(
      int web_state_id,
      web::BrowserState* browser_state) {
    auto fake_web_state = std::make_unique<web::FakeWebState>(
        web::WebStateID::FromSerializedValue(web_state_id));
    fake_web_state->SetBrowserState(browser_state);
    fake_web_state->SetNavigationManager(
        std::make_unique<actor::CompletingFakeNavigationManager>(
            fake_web_state.get()));
    return fake_web_state;
  }

  std::unique_ptr<TtcActuationHandler> CreateHandler() {
    SessionID browserID =
        ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
    return std::make_unique<TtcActuationHandler>(
        actor_service_, browser_->GetWebStateList(), browserID);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<actor::ActorService> actor_service_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests initialization.
TEST_F(TtcActuationHandlerTest, TestInitialization) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  EXPECT_TRUE(handler->CreateTask("Valid Task"));
  handler->Disconnect();

  SessionID browserID =
      ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
  TtcActuationHandler null_service_handler(nullptr, browser_->GetWebStateList(),
                                           browserID);
  EXPECT_FALSE(null_service_handler.CreateTask("Null Service"));

  TtcActuationHandler null_web_state_list_handler(actor_service_, nullptr,
                                                  browserID);
  EXPECT_FALSE(null_web_state_list_handler.CreateTask("Null WebStateList"));

  TtcActuationHandler invalid_browser_id_handler(
      actor_service_, browser_->GetWebStateList(), SessionID::InvalidValue());
  EXPECT_FALSE(invalid_browser_id_handler.CreateTask("Invalid Browser ID"));
}

// Tests constructor with ActorService, WebStateList, and Browser ID.
TEST_F(TtcActuationHandlerTest, TestDesignatedInitializer) {
  TtcActuationHandler handler(actor_service_, browser_->GetWebStateList(),
                              SessionID::FromSerializedValue(42));
  actor::ActorTaskId task_id = handler.CreateTask("Init Task");
  EXPECT_TRUE(task_id);
  handler.Disconnect();
}

// Tests task creation when no active WebState exists.
TEST_F(TtcActuationHandlerTest, TestCreateTaskWithNoActiveWebState) {
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  EXPECT_FALSE(task_id);
  handler->Disconnect();
}

// Tests that task creation returns a null ActorTaskId if the active WebState is
// off-the-record (incognito).
TEST_F(TtcActuationHandlerTest, TestCreateTaskWithIncognitoWebState) {
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebState(456, profile_->GetOffTheRecordProfile()),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("Incognito Task");
  EXPECT_FALSE(task_id);
  handler->Disconnect();
}

// Tests dispatching an actuation request when the task has gone away.
TEST_F(TtcActuationHandlerTest, TestDispatchActuationRequestTaskWentAway) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(
      request_result.value(), actor::ActorTaskId(99999), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
  handler->Disconnect();
}

// Tests dispatching an actuation request with empty action protos.
TEST_F(TtcActuationHandlerTest, TestDispatchActuationRequestEmptyActionProtos) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  TtcActuationRequest request({}, "Empty actions");

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request, task_id, future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kEmptyActionSequence,
            response.error().code);
  handler->Disconnect();
}

// Tests that concurrent actuation requests on the same task are rejected.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestConcurrentRejection) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  auto request_result_1 =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  auto request_result_2 =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result_1.has_value());
  ASSERT_TRUE(request_result_2.has_value());

  base::test::TestFuture<ttc::ToolResponse> future1;
  handler->DispatchActuationRequest(request_result_1.value(), task_id,
                                    future1.GetCallback());

  base::test::TestFuture<ttc::ToolResponse> future2;
  handler->DispatchActuationRequest(request_result_2.value(), task_id,
                                    future2.GetCallback());

  const ttc::ToolResponse& response2 = future2.Get();
  ASSERT_FALSE(response2.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutionEngineExistingAction,
            response2.error().code);

  const ttc::ToolResponse& response1 = future1.Get();
  EXPECT_TRUE(response1.Ok());
  handler->Disconnect();
}

// Tests dispatching an actuation request containing a malformed proto.
TEST_F(TtcActuationHandlerTest, TestDispatchActuationRequestMalformedProto) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  TtcActuationRequest request({"corrupt_proto_data"}, "Executing tool");

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request, task_id, future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.error().code);
  handler->Disconnect();
}

// Tests that multiple sequential actuation requests can be dispatched
// on the same task ID without premature task termination.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestMultipleSequentialCallsOnSameTask) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  auto first_request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/1")});
  ASSERT_TRUE(first_request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> first_future;
  handler->DispatchActuationRequest(first_request_result.value(), task_id,
                                    first_future.GetCallback());

  const ttc::ToolResponse& first_response = first_future.Get();
  EXPECT_TRUE(first_response.Ok());

  // Dispatch a second actuation using the exact same task ID.
  auto second_request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/2")});
  ASSERT_TRUE(second_request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> second_future;
  handler->DispatchActuationRequest(second_request_result.value(), task_id,
                                    second_future.GetCallback());

  const ttc::ToolResponse& second_response = second_future.Get();
  EXPECT_TRUE(second_response.Ok());

  handler->Disconnect();
}

// Tests that StopTask removes the task and rejects subsequent dispatch requests
// with kTaskWentAway.
TEST_F(TtcActuationHandlerTest, TestStopTask) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  handler->StopTask(task_id, actor::ActorTaskStoppedReason::kTaskComplete);

  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), task_id,
                                    future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);

  handler->Disconnect();
}

// Tests that StopTask while an actuation request is in-flight fails the pending
// callback with kTaskWentAway.
TEST_F(TtcActuationHandlerTest, TestStopTaskCancelsActiveActuation) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), task_id,
                                    future.GetCallback());

  handler->StopTask(task_id);

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);

  handler->Disconnect();
}

// Tests that disconnecting cleans up state safely, stops active tasks, and
// fails subsequent dispatch requests.
TEST_F(TtcActuationHandlerTest, TestDisconnectCancelsActiveTasks) {
  actor::ActorService* actor_service =
      actor::ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(actor_service);

  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->Disconnect();

  // Verify task is stopped in ActorService.
  EXPECT_FALSE(actor_service->GetActiveTaskState().has_value());

  // Post-disconnect task creation fails since WebStateList is cleared.
  actor::ActorTaskId post_disconnect_id =
      handler->CreateTask("Post-disconnect Task");
  EXPECT_FALSE(post_disconnect_id);

  // Attempting to dispatch to disconnected handler or closed task fails.
  base::DictValue empty_args;
  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::ToolRequest{ttc::kToolGoBack, std::move(empty_args)});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), task_id,
                                    future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
}

// Tests that disconnecting while an actuation is in-flight fails the pending
// callback with `kExecutorDestroyed`.
TEST_F(TtcActuationHandlerTest,
       TestDisconnectDuringInFlightActuationFailsCallback) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), task_id,
                                    future.GetCallback());

  handler->Disconnect();

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutorDestroyed,
            response.error().code);
}

// Tests validating and dispatching a valid tool request via `ProcessToolCall`.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallSuccess) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")},
      task_id, future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  EXPECT_TRUE(response.Ok());
  handler->Disconnect();
}

// Tests that `ProcessToolCall` returns `kToolUnknown` for an unknown tool name.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallUnknownTool) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(ttc::ToolRequest{"unknown_tool", base::DictValue()},
                           task_id, future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kToolUnknown,
            response.error().code);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` returns `kArgumentsInvalid` for invalid
// arguments.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallInvalidArguments) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  actor::ActorTaskId task_id = handler->CreateTask("TTC Task");
  ASSERT_TRUE(task_id);

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, base::DictValue()}, task_id,
      future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.error().code);
  handler->Disconnect();
}

// Tests that multiple Disconnect calls are idempotent and safe.
TEST_F(TtcActuationHandlerTest, TestMultipleDisconnectCallsAreIdempotent) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  handler->Disconnect();
  EXPECT_NO_FATAL_FAILURE(handler->Disconnect());
}

}  // namespace
