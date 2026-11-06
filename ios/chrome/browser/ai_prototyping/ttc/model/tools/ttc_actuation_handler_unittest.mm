// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>

#import "base/strings/string_number_conversions.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/core/task_source_info.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
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
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
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
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());

    browser_->GetWebStateList()->InsertWebState(
        CreateFakeWebState(123, profile_.get()),
        WebStateList::InsertionParams::AtIndex(0).Activate());
  }

  ~TtcActuationHandlerTest() override {
    if (browser_ && profile_) {
      CloseAllWebStates(*browser_->GetWebStateList(),
                        WebStateList::ClosingReason::kDefault);
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

  web::WebTaskEnvironment task_environment_;
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
      request_result.value(), GetActiveWebState(), browser_.get(),
      future.GetCallback());

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
                                    browser_.get(), future.GetCallback());

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

  handler->DispatchActuationRequest(request_result.value(),
                                    incognito_web_state.get(), browser_.get(),
                                    future.GetCallback());

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
                                    browser_.get(), future.GetCallback());

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
                                    GetActiveWebState(), browser_.get(),
                                    future1.GetCallback());

  base::test::TestFuture<ttc::ToolResponse> future2;
  handler->DispatchActuationRequest(request_result_2.value(),
                                    GetActiveWebState(), browser_.get(),
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
  TtcActuationRequest request({"corrupt_proto_data"}, "Executing tool");

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request, GetActiveWebState(),
                                    browser_.get(), future.GetCallback());

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
                                    GetActiveWebState(), browser_.get(),
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
                                    GetActiveWebState(), browser_.get(),
                                    second_future.GetCallback());
  EXPECT_TRUE(second_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  handler->Disconnect();
}

// Tests that if the session task is stopped externally in `ActorService` while
// an unrelated task remains active, a subsequent actuation request replaces the
// stopped task with a new one.
TEST_F(TtcActuationHandlerTest,
       TestDispatchActuationRequestReplacesExternallyStoppedTask) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  base::expected<TtcActuationRequest, std::string> request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/1")});
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<ttc::ToolResponse> first_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    browser_.get(), first_future.GetCallback());
  EXPECT_TRUE(first_future.Get().Ok());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  // Stops the handler's task externally and creates an unrelated active task so
  // `GetActiveTaskState()` remains non-null.
  actor_service->StopAllTasks();
  actor::ActorTaskId other_task_id = actor_service->CreateTask(
      "Other Task",
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                            /*id=*/std::nullopt),
      /*allow_incognito_web_states=*/false);
  ASSERT_FALSE(other_task_id.is_null());
  actor::FlushCurrentSequence();

  // The next actuation request should create a replacement task.
  base::test::TestFuture<ttc::ToolResponse> second_future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    browser_.get(),
                                    second_future.GetCallback());
  EXPECT_TRUE(second_future.Get().Ok());

  handler->Disconnect();
  actor_service->StopAllTasks();
}

// Tests that stopping the task externally in `ActorService` while an actuation
// request is in flight fails the pending callback with `kTaskWentAway`.
TEST_F(TtcActuationHandlerTest, TestExternalStopCancelsActiveActuation) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  base::expected<TtcActuationRequest, std::string> request_result =
      TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
          ttc::kToolOpenUrl, CreateUrlArgs("https://example.com")});
  ASSERT_TRUE(request_result.has_value());
  base::test::TestFuture<ttc::ToolResponse> future;
  handler->DispatchActuationRequest(request_result.value(), GetActiveWebState(),
                                    browser_.get(), future.GetCallback());

  actor_service->StopAllTasks();

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.error().code);
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
                                    browser_.get(), future.GetCallback());
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
                                    browser_.get(), future.GetCallback());

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
                                    browser_.get(), first_future.GetCallback());
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
                                    browser_.get(),
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
                                    browser_.get(), future.GetCallback());

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
      GetActiveWebState(), browser_.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  EXPECT_TRUE(response.Ok());
  handler->Disconnect();
}

// Tests that `ProcessToolCall` returns `kToolUnknown` and populates the
// validation error message for an unknown tool name.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallUnknownTool) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(ttc::ToolRequest{"unknown_tool", base::DictValue()},
                           GetActiveWebState(), browser_.get(),
                           future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kToolUnknown,
            response.error().code);
  EXPECT_EQ(response.error().message, ttc::kErrorMessageUnknownTool);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` returns `kArgumentsInvalid` and populates the
// validation error message for invalid arguments.
TEST_F(TtcActuationHandlerTest, TestProcessToolCallInvalidArguments) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();

  base::test::TestFuture<ttc::ToolResponse> future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, base::DictValue()},
      GetActiveWebState(), browser_.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.error().code);
  EXPECT_EQ(response.error().message, ttc::kErrorMessageInvalidArguments);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` forwards the underlying tool execution error
// message when `ActorService::PerformActions` fails.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallExecutionFailureForwardsErrorMessage) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolGoBack, base::DictValue()},
      GetActiveWebState(), browser_.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kHistoryNoBackEntries);
  EXPECT_EQ(response.error().message, "Cannot go back.");
  handler->Disconnect();
}

// Tests that multiple Disconnect calls are idempotent and safe.
TEST_F(TtcActuationHandlerTest, TestMultipleDisconnectCallsAreIdempotent) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  handler->Disconnect();
  EXPECT_NO_FATAL_FAILURE(handler->Disconnect());
}

// Tests that `ProcessToolCall` for `close_session` replies with success,
// logs `ActuationExecute` to `TTCJournal`, and invokes the registered
// close-session callback without requiring an `ActorService` or active
// `WebState`.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallCloseSessionRunsCallbackAndReturnsSuccess) {
  TtcActuationHandler handler(/*actor_service=*/nullptr);
  TTCJournal journal;
  handler.set_journal(&journal);
  bool close_session_called = false;
  handler.SetCloseSessionCallback(
      base::BindRepeating([](bool* called) { *called = true; },
                          base::Unretained(&close_session_called)));
  base::test::TestFuture<ttc::ToolResponse> future;

  handler.ProcessToolCall(
      ttc::ToolRequest{ttc::kToolCloseSession, base::DictValue()},
      /*target_web_state=*/nullptr, /*browser=*/nullptr, future.GetCallback());

  EXPECT_TRUE(future.Get().Ok());
  EXPECT_TRUE(close_session_called);
  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[0].type, TTCJournalEntryType::kBegin);
  EXPECT_EQ(journal.entries()[0].summary, "Tool started: close_session");
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_EQ(journal.entries()[1].summary, "Tool finished: ok");
  handler.Disconnect();
}

// Tests that `ProcessToolCall` for `close_session` fails with
// `kExecutorDestroyed` and does not invoke the callback after `Disconnect()`
// clears the registered close-session hook.
TEST_F(TtcActuationHandlerTest,
       CloseSessionAfterDisconnectFailsWithExecutorDestroyed) {
  TtcActuationHandler handler(/*actor_service=*/nullptr);
  bool close_session_called = false;
  handler.SetCloseSessionCallback(
      base::BindRepeating([](bool* called) { *called = true; },
                          base::Unretained(&close_session_called)));
  handler.Disconnect();
  base::test::TestFuture<ttc::ToolResponse> future;

  handler.ProcessToolCall(
      ttc::ToolRequest{ttc::kToolCloseSession, base::DictValue()},
      /*target_web_state=*/nullptr, /*browser=*/nullptr, future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kExecutorDestroyed);
  EXPECT_FALSE(close_session_called);
}

// Tests that `ProcessToolCall` for `open_url` with `new_tab: true` inserts and
// activates a new foreground `WebState` in the browser, registers it as a
// controlled `WebState` in `ActorService`, and logs `ActuationExecute` to
// `TTCJournal`.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallOpenUrlInNewTabInsertsAndActivatesForegroundTab) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  TTCJournal journal;
  handler->set_journal(&journal);
  ASSERT_EQ(browser_->GetWebStateList()->count(), 1);
  ASSERT_EQ(browser_->GetWebStateList()->active_index(), 0);

  base::DictValue args = CreateUrlArgs("https://example.com/new");
  args.Set("new_tab", true);
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(ttc::ToolRequest{ttc::kToolOpenUrl, std::move(args)},
                           GetActiveWebState(), browser_.get(),
                           future.GetCallback());

  EXPECT_TRUE(future.Get().Ok());
  EXPECT_EQ(browser_->GetWebStateList()->count(), 2);
  EXPECT_EQ(browser_->GetWebStateList()->active_index(), 1);
  web::NavigationItem* pending_item =
      GetActiveWebState()->GetNavigationManager()->GetPendingItem();
  ASSERT_NE(pending_item, nullptr);
  EXPECT_EQ(pending_item->GetURL(), GURL("https://example.com/new"));
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());
  EXPECT_TRUE(actor::HasJournalEntryWithDetail(
      *actor_service->GetJournal(), "ActorTask::AddControlledWebState",
      "web_state_id",
      base::NumberToString(
          GetActiveWebState()->GetUniqueIdentifier().identifier())));
  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[0].type, TTCJournalEntryType::kBegin);
  EXPECT_EQ(journal.entries()[0].summary, "Tool started: open_url");
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_EQ(journal.entries()[1].summary, "Tool finished: ok");
  handler->Disconnect();
}

// Tests that `ProcessToolCall` for `open_url` with `new_tab: true` fails with
// `kWindowWentAway` when both `target_web_state` and `browser` are null.
TEST_F(
    TtcActuationHandlerTest,
    ProcessToolCallOpenUrlInNewTabWithoutRegularBrowserFailsWithWindowWentAway) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  base::DictValue args = CreateUrlArgs("https://example.com/new");
  args.Set("new_tab", true);
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(ttc::ToolRequest{ttc::kToolOpenUrl, std::move(args)},
                           /*target_web_state=*/nullptr, /*browser=*/nullptr,
                           future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kWindowWentAway);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` for `open_url` with `new_tab: true` fails with
// `kTabWentAway` when `active_web_state` is off-the-record.
TEST_F(
    TtcActuationHandlerTest,
    ProcessToolCallOpenUrlInNewTabWithIncognitoWebStateFailsWithTabWentAway) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  std::unique_ptr<web::FakeWebState> incognito_web_state =
      CreateFakeWebState(456, profile_->GetOffTheRecordProfile());
  base::DictValue args = CreateUrlArgs("https://example.com/new");
  args.Set("new_tab", true);
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(ttc::ToolRequest{ttc::kToolOpenUrl, std::move(args)},
                           incognito_web_state.get(), browser_.get(),
                           future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kTabWentAway);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` for `open_url` with `new_tab: true` fails with
// `kWindowWentAway` when `active_web_state` is not attached to a `Browser`.
TEST_F(
    TtcActuationHandlerTest,
    ProcessToolCallOpenUrlInNewTabWithUnattachedWebStateFailsWithWindowWentAway) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  std::unique_ptr<web::FakeWebState> unattached_web_state =
      CreateFakeWebState(789, profile_.get());
  base::DictValue args = CreateUrlArgs("https://example.com/new");
  args.Set("new_tab", true);
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(ttc::ToolRequest{ttc::kToolOpenUrl, std::move(args)},
                           unattached_web_state.get(), /*browser=*/nullptr,
                           future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kWindowWentAway);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` for `open_url` with `new_tab: true` and an
// invalid URL fails with `kArgumentsInvalid`.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallOpenUrlInNewTabWithInvalidUrlFailsWithArgumentsInvalid) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  base::DictValue invalid_args = CreateUrlArgs("ftp://example.com");
  invalid_args.Set("new_tab", true);
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(invalid_args)},
      GetActiveWebState(), browser_.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
  handler->Disconnect();
}

// Tests that `ProcessToolCall` executes `create_tab` and `close_current_tab`
// against the browser's `WebStateList`.
TEST_F(TtcActuationHandlerTest, ProcessToolCallCreatesAndClosesBrowserTabs) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(web_state_list->count(), 1);
  web::WebState* initial_tab = GetActiveWebState();

  // `create_tab` creates and activates a new foreground tab.
  base::test::TestFuture<ttc::ToolResponse> create_future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolCreateTab, base::DictValue()},
      GetActiveWebState(), browser_.get(), create_future.GetCallback());
  EXPECT_TRUE(create_future.Get().Ok());
  ASSERT_EQ(web_state_list->count(), 2);
  web::WebState* created_tab = GetActiveWebState();
  EXPECT_NE(created_tab, initial_tab);

  // `close_current_tab` closes the currently active tab.
  base::test::TestFuture<ttc::ToolResponse> close_future;
  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolCloseCurrentTab, base::DictValue()},
      GetActiveWebState(), browser_.get(), close_future.GetCallback());
  EXPECT_TRUE(close_future.Get().Ok());
  EXPECT_EQ(web_state_list->count(), 1);
  EXPECT_EQ(GetActiveWebState(), initial_tab);

  handler->Disconnect();
}

// Tests that `scroll_page` with `"page"` granularity fails validation with
// `kArgumentsInvalid` when the active `WebState`'s view is unattached or has
// zero dimensions.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallScrollPageWithZeroSizedViewportFailsValidation) {
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  base::DictValue args;
  args.Set("granularity", "page");
  args.Set("direction", "down");
  args.Set("node_id",
           base::DictValue().Set("document_id", "").Set("dom_node_id", 0));
  base::test::TestFuture<ttc::ToolResponse> future;

  handler->ProcessToolCall(
      ttc::ToolRequest{ttc::kToolScrollPage, std::move(args)},
      GetActiveWebState(), browser_.get(), future.GetCallback());

  const ttc::ToolResponse& response = future.Get();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
  handler->Disconnect();
}

// Tests that `scroll_page` with `"page"` granularity computes 80% of the active
// `WebState` viewport height (for vertical scrolls) or width (for horizontal
// scrolls), while preserving fixed distance for `"document"` granularity.
TEST_F(TtcActuationHandlerTest,
       ProcessToolCallScrollPageInjectsViewportBasedDistance) {
  actor::ActorService* actor_service = GetActorService();
  ASSERT_TRUE(actor_service);
  std::unique_ptr<TtcActuationHandler> handler = CreateHandler();
  web::FakeWebState* active_fake_web_state =
      static_cast<web::FakeWebState*>(GetActiveWebState());
  UIView* web_view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 400, 800)];
  active_fake_web_state->SetView(web_view);

  struct ScrollTestCase {
    const char* granularity;
    const char* direction;
    const char* expected_tool_debug_string;
  };
  const ScrollTestCase kScrollCases[] = {
      {"page", "up",
       "ScrollTool[target(null);direction(UP);distance(640.000000)]"},
      {"page", "down",
       "ScrollTool[target(null);direction(DOWN);distance(640.000000)]"},
      {"page", "left",
       "ScrollTool[target(null);direction(LEFT);distance(320.000000)]"},
      {"page", "right",
       "ScrollTool[target(null);direction(RIGHT);distance(320.000000)]"},
      {"document", "down",
       "ScrollTool[target(null);direction(DOWN);distance(100000.000000)]"},
  };

  for (const ScrollTestCase& test_case : kScrollCases) {
    SCOPED_TRACE(testing::Message()
                 << test_case.granularity << ":" << test_case.direction);
    base::DictValue args;
    args.Set("granularity", test_case.granularity);
    args.Set("direction", test_case.direction);
    args.Set("node_id",
             base::DictValue().Set("document_id", "").Set("dom_node_id", 0));
    base::test::TestFuture<ttc::ToolResponse> future;

    handler->ProcessToolCall(
        ttc::ToolRequest{ttc::kToolScrollPage, std::move(args)},
        GetActiveWebState(), browser_.get(), future.GetCallback());

    ASSERT_TRUE(future.Wait());
    EXPECT_TRUE(actor::HasJournalEntryWithDetail(
        *actor_service->GetJournal(), "Execute Tool: ScrollTool", "tool",
        test_case.expected_tool_debug_string));
  }

  handler->Disconnect();
}

}  // namespace
