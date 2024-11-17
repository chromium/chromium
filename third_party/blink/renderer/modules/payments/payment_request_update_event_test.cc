// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_update_event.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"
#include "third_party/blink/renderer/modules/payments/payment_response.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class MockPaymentRequest : public GarbageCollected<MockPaymentRequest>,
                           public PaymentRequestDelegate {
 public:
  MockPaymentRequest() = default;

  MockPaymentRequest(const MockPaymentRequest&) = delete;
  MockPaymentRequest& operator=(const MockPaymentRequest&) = delete;

  ~MockPaymentRequest() override = default;

  MOCK_METHOD1(OnUpdatePaymentDetails, void(PaymentDetailsUpdate* details));
  MOCK_METHOD1(OnUpdatePaymentDetailsFailure, void(const String& error));
  bool IsInteractive() const override { return true; }

  void Trace(Visitor* visitor) const override {}
};

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsCalled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::PhaseType::kCapturingPhase);
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*request, OnUpdatePaymentDetails(testing::_));
  EXPECT_CALL(*request, OnUpdatePaymentDetailsFailure(testing::_)).Times(0);

  payment_details->Resolve();
}

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsFailureCalled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::PhaseType::kCapturingPhase);
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*request, OnUpdatePaymentDetails(testing::_)).Times(0);
  EXPECT_CALL(*request, OnUpdatePaymentDetailsFailure(testing::_));

  payment_details->Reject("oops");
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateWithoutDispatching) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetPaymentRequest((MakeGarbageCollected<MockPaymentRequest>()));

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateTwice) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::PhaseType::kCapturingPhase);
  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, UpdaterNotRequired) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(true);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, AddressChangeUpdateWithTimeout) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetPaymentRequest(request);
  event->SetTrusted(true);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), scope.GetExceptionState()));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetailsTimeoutForTesting();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      promise_tester.ValueAsString());

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("PaymentRequest is no longer interactive",
            scope.GetExceptionState().Message());
}

TEST(PaymentRequestUpdateEventTest, OptionChangeUpdateWithTimeout) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), scope.GetExceptionState()));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetailsTimeoutForTesting();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      promise_tester.ValueAsString());

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("PaymentRequest is no longer interactive",
            scope.GetExceptionState().Message());
}

TEST(PaymentRequestUpdateEventTest, AddressChangePromiseTimeout) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::PhaseType::kCapturingPhase);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), scope.GetExceptionState()));
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  request->OnUpdatePaymentDetailsTimeoutForTesting();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      promise_tester.ValueAsString());

  payment_details->Resolve();
}

TEST(PaymentRequestUpdateEventTest, OptionChangePromiseTimeout) {
  test::TaskEnvironment task_environment;
  PaymentRequestV8TestingScope scope;
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::PhaseType::kCapturingPhase);

  LocalFrame::NotifyUserActivation(
      &scope.GetFrame(), mojom::UserActivationNotificationType::kTest);
  ScriptPromiseTester promise_tester(
      scope.GetScriptState(),
      request->show(scope.GetScriptState(), scope.GetExceptionState()));
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  request->OnUpdatePaymentDetailsTimeoutForTesting();

  scope.PerformMicrotaskCheckpoint();
  EXPECT_TRUE(promise_tester.IsRejected());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      promise_tester.ValueAsString());

  payment_details->Resolve();
}

TEST(PaymentRequestUpdateEventTest, NotAllowUntrustedEvent) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(false);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver<PaymentDetailsUpdate>>(
          scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

}  // namespace
}  // namespace blink
