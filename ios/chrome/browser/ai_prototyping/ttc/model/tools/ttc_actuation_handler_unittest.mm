// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <memory>
#import <string>

#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
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
    // Eagerly instantiates the `ActorService` keyed service for `profile_`.
    GetActorService();

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

  ~TtcActuationHandlerTest() override {
    if (browser_ && profile_) {
      BrowserListFactory::GetForProfile(profile_.get())
          ->RemoveBrowser(browser_.get());
    }
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

  // Returns the active `WebState` in the test browser.
  web::WebState* GetActiveWebState() {
    return browser_->GetWebStateList()->GetActiveWebState();
  }

  // Returns the `ActorService` for the test profile.
  actor::ActorService* GetActorService() {
    return actor::ActorServiceFactory::GetForProfile(profile_.get());
  }

  // Creates a `TtcActuationHandler` backed by the test `ActorService`.
  std::unique_ptr<TtcActuationHandler> CreateHandler() {
    return std::make_unique<TtcActuationHandler>(GetActorService());
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests dispatching an actuation request when `actor_service` is null.
TEST_F(TtcActuationHandlerTest, TestDispatchWithNullActorService) {
  TtcActuationHandler null_service_handler(nullptr);
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  null_service_handler.DispatchActuationRequest(
      request_result.value(), GetActiveWebState(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
}

// Tests that dispatching fails with `kTabWentAway` when `active_web_state` is
// null.
TEST_F(TtcActuationHandlerTest,
       TestDispatchWithNullActiveWebStateFailsWithTabWentAway) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->DispatchActuationRequest(request_result.value(), nullptr,
                                    future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTabWentAway,
            response.error().code);
  handler->Disconnect();
}

// Tests that dispatching fails with `kTabWentAway` when `active_web_state` is
// off-the-record (incognito).
TEST_F(TtcActuationHandlerTest,
       TestDispatchWithIncognitoWebStateFailsWithTabWentAway) {
  std::unique_ptr<web::FakeWebState> incognito_web_state =
      CreateFakeWebState(456, profile_->GetOffTheRecordProfile());
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->DispatchActuationRequest(
      request_result.value(), incognito_web_state.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTabWentAway,
            response.error().code);
  handler->Disconnect();
}

// Tests dispatching an actuation request with empty action protos.
TEST_F(TtcActuationHandlerTest, TestDispatchActuationRequestEmptyActionProtos) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  TtcActuationRequest request({}, "Empty actions");

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request, GetActiveWebState(),
                                    future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kEmptyActionSequence,
            response.error().code);
  handler->Disconnect();
}

// Tests that concurrent actuation requests on the handler are rejected.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestConcurrentRejection) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  auto request_result_1 =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  auto request_result_2 =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result_1.has_value());
  ASSERT_TRUE(request_result_2.has_value());

  base::test::TestFuture<ttc::ToolResponse> future1;
  handler->DispatchActuationRequest(request_result_1.value(),
                                    GetActiveWebState(), future1.GetCallback());

  base::test::TestFuture<ttc::ToolResponse> future2;
  handler->DispatchActuationRequest(request_result_2.value(),
                                    GetActiveWebState(), future2.GetCallback());

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
  TtcActuationRequest request({"corrupt_proto_data"}, "Executing tool");

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request, GetActiveWebState(),
                                    future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.error().code);
  handler->Disconnect();
}

// Tests that multiple sequential actuation requests reuse the same session task
// and can target newly activated tabs across calls.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestSequentialAndTabSwitch) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  auto first_request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/1")});
  ASSERT_TRUE(first_request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> first_future;
  handler->DispatchActuationRequest(first_request_result.value(),
                                    GetActiveWebState(),
                                    first_future.GetCallback());
  EXPECT_TRUE(first_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  // Inserts and activates a second tab, then dispatches a second actuation on
  // it.
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebState(789, profile_.get()),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  auto second_request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/2")});
  ASSERT_TRUE(second_request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> second_future;
  handler->DispatchActuationRequest(second_request_result.value(),
                                    GetActiveWebState(),
                                    second_future.GetCallback());
  EXPECT_TRUE(second_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->Disconnect();
}

// Tests that if the session task is stopped externally in `ActorService`, a
// subsequent actuation request replaces it with a new task.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestReplacesExternallyStoppedTask) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/1")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> first_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    first_future.GetCallback());
  EXPECT_TRUE(first_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  // Stops the task externally via `ActorService`.
  actor_service->StopAllTasks();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !actor_service->GetActiveTaskState().has_value(); }));

  // The next actuation request should create a replacement task.
  base::test::TestFuture<ttc::ToolResponse> second_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    second_future.GetCallback());
  EXPECT_TRUE(second_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->Disconnect();
}

// Tests that `StopTask` stops the active task in `ActorService`.
TEST_F(TtcActuationHandlerTest, TestStopTaskStopsActiveActorTask) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());
  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    future.GetCallback());
  EXPECT_TRUE(future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->StopTask(actor::ActorTaskStoppedReason::kTaskComplete);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !actor_service->GetActiveTaskState().has_value(); }));
  handler->Disconnect();
}

// Tests that `StopTask` while an actuation request is in-flight fails the
// pending callback with `kTaskWentAway`.
TEST_F(TtcActuationHandlerTest, TestStopTaskCancelsActiveActuation) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());
  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    future.GetCallback());

  handler->StopTask();

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
  handler->Disconnect();
}

// Tests that disconnecting cleans up state safely, stops the active task, and
// fails subsequent dispatch requests.
TEST_F(TtcActuationHandlerTest, TestDisconnectCancelsActiveTask) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);

  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> first_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    first_future.GetCallback());
  EXPECT_TRUE(first_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->Disconnect();

  // Verifies that the task is stopped in `ActorService`.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !actor_service->GetActiveTaskState().has_value(); }));

  // Attempting to dispatch to a disconnected handler fails with
  // `kTaskWentAway`.
  base::test::TestFuture<ttc::ToolResponse> second_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    second_future.GetCallback());

  const ttc::ToolResponse& response = second_future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
}

// Tests that disconnecting while an actuation is in-flight fails the pending
// callback with `kExecutorDestroyed`.
TEST_F(TtcActuationHandlerTest,
       TestDisconnectDuringInFlightActuationFailsCallback) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  auto request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
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

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")},
      GetActiveWebState(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  EXPECT_TRUE(response.Ok());
  handler->Disconnect();
}

// Tests that `ProcessToolCall` returns `kToolUnknown` for an unknown tool name.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallUnknownTool) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(ttc::ToolRequest{"unknown_tool", base::DictValue()},
                           GetActiveWebState(), future.GetCallback());

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

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, base::DictValue()},
      GetActiveWebState(), future.GetCallback());

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
