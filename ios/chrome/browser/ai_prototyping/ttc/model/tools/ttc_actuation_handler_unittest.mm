// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler.h"

#import <memory>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/actor/public/mojom/actor_types.mojom.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_handler+Testing.h"
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
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

base::DictValue CreateUrlArgs(const std::string& url) {
  return base::DictValue().Set("url", url);
}

class TTCActuationHandlerTest : public PlatformTest {
 public:
  TTCActuationHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kActorTools);
    profile_ = TestProfileIOS::Builder().Build();
    actor::ActorServiceFactory::GetForProfile(profile_.get());

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

  TTCActuationHandler* CreateHandler() {
    SessionID browserID =
        ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
    return
        [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                        webStateList:browser_->GetWebStateList()
                                           browserID:browserID];
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests initialization.
TEST_F(TTCActuationHandlerTest, TestInitialization) {
  TTCActuationHandler* handler = CreateHandler();
  EXPECT_NE(handler, nil);
  [handler disconnect];

  SessionID browserID =
      ActorBrowserAgent::FromBrowser(browser_.get())->browser_id();
  TTCActuationHandler* nilProfileHandler =
      [[TTCActuationHandler alloc] initWithProfile:nullptr
                                      webStateList:browser_->GetWebStateList()
                                         browserID:browserID];
  EXPECT_EQ(nilProfileHandler, nil);

  TTCActuationHandler* nilWebStateListHandler =
      [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                      webStateList:nullptr
                                         browserID:browserID];
  EXPECT_EQ(nilWebStateListHandler, nil);

  TTCActuationHandler* invalidBrowserIDHandler =
      [[TTCActuationHandler alloc] initWithProfile:profile_.get()
                                      webStateList:browser_->GetWebStateList()
                                         browserID:SessionID::InvalidValue()];
  EXPECT_EQ(invalidBrowserIDHandler, nil);
}

// Tests setting actor service for testing.
TEST_F(TTCActuationHandlerTest, TestSetActorServiceForTesting) {
  actor::ActorService* actor_service =
      actor::ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(actor_service);

  TTCActuationHandler* handler = CreateHandler();
  [handler setActorServiceForTesting:actor_service];
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Test Task"];
  EXPECT_FALSE(task_id.is_null());
  [handler disconnect];
}

// Tests designated initializer with Profile, WebStateList, and Browser ID.
TEST_F(TTCActuationHandlerTest, TestDesignatedInitializer) {
  TTCActuationHandler* handler = [[TTCActuationHandler alloc]
      initWithProfile:profile_.get()
         webStateList:browser_->GetWebStateList()
            browserID:SessionID::FromSerializedValue(42)];
  EXPECT_NE(handler, nil);
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Init Task"];
  EXPECT_FALSE(task_id.is_null());
  [handler disconnect];
}

// Tests task creation with an active WebState.
TEST_F(TTCActuationHandlerTest, TestCreateTask) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  EXPECT_FALSE(task_id.is_null());
  [handler disconnect];
}

// Tests task creation when no active WebState exists.
TEST_F(TTCActuationHandlerTest, TestCreateTaskWithNoActiveWebState) {
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  EXPECT_TRUE(task_id.is_null());
  [handler disconnect];
}

// Tests that task creation returns a null ActorTaskId if the active WebState is
// off-the-record (incognito).
TEST_F(TTCActuationHandlerTest, TestCreateTaskWithIncognitoWebState) {
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebState(456, profile_->GetOffTheRecordProfile()),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"Incognito Task"];
  EXPECT_TRUE(task_id.is_null());
  [handler disconnect];
}

// Tests dispatching an actuation request when the task has gone away.
TEST_F(TTCActuationHandlerTest, TestDispatchActuationRequestTaskWentAway) {
  TTCActuationHandler* handler = CreateHandler();
  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"), "call_123");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:actor::ActorTaskId(99999)
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_123");
  [handler disconnect];
}

// Tests dispatching an actuation request with empty action protos.
TEST_F(TTCActuationHandlerTest, TestDispatchActuationRequestEmptyActionProtos) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  TtcActuationRequest request({}, "Empty actions", "call_empty");

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kEmptyActionSequence,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_empty");
  [handler disconnect];
}

// Tests that concurrent actuation requests on the same task are rejected.
TEST_F(TTCActuationHandlerTest,
       TestDispatchActuationRequestConcurrentRejection) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  auto request_result_1 = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"), "call_1");
  auto request_result_2 = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"), "call_2");
  ASSERT_TRUE(request_result_1.has_value());
  ASSERT_TRUE(request_result_2.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future1;
  [handler
      dispatchActuationRequest:request_result_1.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future1.GetCallback())];

  base::test::TestFuture<const TtcActuationResponse&> future2;
  [handler
      dispatchActuationRequest:request_result_2.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future2.GetCallback())];

  const TtcActuationResponse& response2 = future2.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutionEngineExistingAction,
            response2.result_code);
  EXPECT_EQ(response2.call_id, "call_2");

  const TtcActuationResponse& response1 = future1.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response1.result_code);
  EXPECT_EQ(response1.call_id, "call_1");
  [handler disconnect];
}

// Tests dispatching an actuation request containing a malformed proto.
TEST_F(TTCActuationHandlerTest, TestDispatchActuationRequestMalformedProto) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  TtcActuationRequest request({"corrupt_proto_data"}, "Executing tool",
                              "call_malformed");

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kArgumentsInvalid,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_malformed");
  [handler disconnect];
}

// Tests successful execution of a navigation action request.
TEST_F(TTCActuationHandlerTest, TestDispatchActuationRequestSuccess) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"), "call_ok");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.result_code);
  EXPECT_EQ(response.call_id, "call_ok");
  [handler disconnect];
}

// Tests that multiple sequential actuation requests can be dispatched
// on the same task ID without premature task termination.
TEST_F(TTCActuationHandlerTest,
       TestDispatchActuationRequestMultipleSequentialCallsOnSameTask) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  auto first_request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/1"), "call_seq_1");
  ASSERT_TRUE(first_request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> first_future;
  [handler dispatchActuationRequest:first_request_result.value()
                          forTaskID:task_id
                    completionBlock:base::CallbackToBlock(
                                        first_future.GetCallback())];

  const TtcActuationResponse& first_response = first_future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, first_response.result_code);
  EXPECT_EQ(first_response.call_id, "call_seq_1");

  // Dispatch a second actuation using the exact same task ID.
  auto second_request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com/2"), "call_seq_2");
  ASSERT_TRUE(second_request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> second_future;
  [handler dispatchActuationRequest:second_request_result.value()
                          forTaskID:task_id
                    completionBlock:base::CallbackToBlock(
                                        second_future.GetCallback())];

  const TtcActuationResponse& second_response = second_future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, second_response.result_code);
  EXPECT_EQ(second_response.call_id, "call_seq_2");

  [handler disconnect];
}

// Tests that stopTask removes the task and rejects subsequent dispatch requests
// with kTaskWentAway.
TEST_F(TTCActuationHandlerTest, TestStopTask) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  [handler stopTask:task_id
         withReason:actor::ActorTaskStoppedReason::kTaskComplete];

  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"),
      "call_after_stop");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_after_stop");

  [handler disconnect];
}

// Tests that stopTask while an actuation request is in-flight fails the pending
// callback with kTaskWentAway.
TEST_F(TTCActuationHandlerTest, TestStopTaskCancelsActiveActuation) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"),
      "call_inflight_stop");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  [handler stopTask:task_id];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_inflight_stop");

  [handler disconnect];
}

// Tests that an action with an explicitly pre-populated `tab_id` preserves that
// target tab rather than overwriting it with the active tab, allowing actions
// to target background tabs (e.g. closing an inactive tab).
TEST_F(TTCActuationHandlerTest,
       TestDispatchActuationRequestPreservesExplicitTabIdForCloseTab) {
  // Insert a background WebState with ID 456 (active tab remains 123).
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebState(456, profile_.get()),
      WebStateList::InsertionParams::AtIndex(1));
  ASSERT_EQ(2, browser_->GetWebStateList()->count());

  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  // Construct a `kCloseTab` action explicitly targeting background tab 456.
  optimization_guide::proto::Action action;
  action.mutable_close_tab()->set_tab_id(456);
  std::string serialized;
  action.SerializeToString(&serialized);
  TtcActuationRequest request({serialized}, "Closing background tab",
                              "call_close_bg_tab");

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kOk, response.result_code);
  EXPECT_EQ(response.call_id, "call_close_bg_tab");

  // Verify background tab 456 was closed and active tab 123 remains open.
  EXPECT_EQ(1, browser_->GetWebStateList()->count());
  EXPECT_EQ(123, browser_->GetWebStateList()
                     ->GetActiveWebState()
                     ->GetUniqueIdentifier()
                     .identifier());

  [handler disconnect];
}

// Tests that disconnecting cleans up state safely, stops active tasks, and
// fails subsequent dispatch requests.
TEST_F(TTCActuationHandlerTest, TestDisconnectCancelsActiveTasks) {
  actor::ActorService* actor_service =
      actor::ActorServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(actor_service);

  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());
  EXPECT_TRUE(actor_service->GetActiveTaskState().has_value());

  [handler disconnect];

  // Verify task is stopped in ActorService.
  EXPECT_FALSE(actor_service->GetActiveTaskState().has_value());

  // Post-disconnect task creation fails since WebStateList is cleared.
  actor::ActorTaskId post_disconnect_id =
      [handler createTaskWithTitle:@"Post-disconnect Task"];
  EXPECT_TRUE(post_disconnect_id.is_null());

  // Attempting to dispatch to disconnected handler or closed task fails.
  base::DictValue empty_args;
  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolGoBack, empty_args, "call_disc");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kTaskWentAway,
            response.result_code);
}

// Tests that disconnecting while an actuation is in-flight fails the pending
// callback with kExecutorDestroyed and preserves call_id.
TEST_F(TTCActuationHandlerTest,
       TestDisconnectDuringInFlightActuationFailsCallback) {
  TTCActuationHandler* handler = CreateHandler();
  actor::ActorTaskId task_id = [handler createTaskWithTitle:@"TTC Task"];
  ASSERT_FALSE(task_id.is_null());

  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, CreateUrlArgs("https://example.com"),
      "call_inflight_disconnect");
  ASSERT_TRUE(request_result.has_value());

  base::test::TestFuture<const TtcActuationResponse&> future;
  [handler
      dispatchActuationRequest:request_result.value()
                     forTaskID:task_id
               completionBlock:base::CallbackToBlock(future.GetCallback())];

  [handler disconnect];

  const TtcActuationResponse& response = future.Get();
  EXPECT_EQ(actor::mojom::ActionResultCode::kExecutorDestroyed,
            response.result_code);
  EXPECT_EQ(response.call_id, "call_inflight_disconnect");
}

// Tests that multiple disconnect calls are idempotent and safe.
TEST_F(TTCActuationHandlerTest, TestMultipleDisconnectCallsAreIdempotent) {
  TTCActuationHandler* handler = CreateHandler();
  [handler disconnect];
  EXPECT_NO_FATAL_FAILURE([handler disconnect]);
}

}  // namespace
