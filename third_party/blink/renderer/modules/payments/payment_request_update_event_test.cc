// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_update_event.h"

#include <memory>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/modules/payments/payment_request.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"
#include "third_party/blink/renderer/modules/payments/payment_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {
namespace {

class MockPaymentRequest : public GarbageCollected<MockPaymentRequest>,
                           public PaymentRequestDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(MockPaymentRequest);

 public:
  MockPaymentRequest() = default;
  ~MockPaymentRequest() override = default;

  MOCK_METHOD1(OnUpdatePaymentDetails,
               void(const ScriptValue& detailsScriptValue));
  MOCK_METHOD1(OnUpdatePaymentDetailsFailure, void(const String& error));
  bool IsInteractive() const override { return true; }

  void Trace(blink::Visitor* visitor) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPaymentRequest);
};

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsCalled) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::kCapturingPhase);
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*request, OnUpdatePaymentDetails(testing::_));
  EXPECT_CALL(*request, OnUpdatePaymentDetailsFailure(testing::_)).Times(0);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, OnUpdatePaymentDetailsFailureCalled) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::kCapturingPhase);
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  EXPECT_CALL(*request, OnUpdatePaymentDetails(testing::_)).Times(0);
  EXPECT_CALL(*request, OnUpdatePaymentDetailsFailure(testing::_));

  payment_details->Reject("oops");
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateWithoutDispatching) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetPaymentRequest((MakeGarbageCollected<MockPaymentRequest>()));

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, CannotUpdateTwice) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  MockPaymentRequest* request = MakeGarbageCollected<MockPaymentRequest>();
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::kCapturingPhase);
  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, UpdaterNotRequired) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(true);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(PaymentRequestUpdateEventTest, AddressChangeUpdateWithTimeout) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetPaymentRequest(request);
  event->SetTrusted(true);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetailsTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      error_message);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("PaymentRequest is no longer interactive",
            scope.GetExceptionState().Message());
}

TEST(PaymentRequestUpdateEventTest, OptionChangeUpdateWithTimeout) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));

  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  request->OnUpdatePaymentDetailsTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      error_message);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
  EXPECT_EQ("PaymentRequest is no longer interactive",
            scope.GetExceptionState().Message());
}

TEST(PaymentRequestUpdateEventTest, AddressChangePromiseTimeout) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::kCapturingPhase);
  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  request->OnUpdatePaymentDetailsTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      error_message);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, OptionChangePromiseTimeout) {
  PaymentRequestV8TestingScope scope;
  PaymentRequestMockFunctionScope funcs(scope.GetScriptState());
  PaymentRequest* request = PaymentRequest::Create(
      scope.GetExecutionContext(), BuildPaymentMethodDataForTest(),
      BuildPaymentDetailsInitForTest(), scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingoptionchange);
  event->SetTrusted(true);
  event->SetPaymentRequest(request);
  event->SetEventPhase(Event::kCapturingPhase);
  String error_message;
  request->show(scope.GetScriptState())
      .Then(funcs.ExpectNoCall(), funcs.ExpectCall(&error_message));
  static_cast<payments::mojom::blink::PaymentRequestClient*>(request)
      ->OnShippingAddressChange(BuildPaymentAddressForTest());
  auto* payment_details =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  event->updateWith(scope.GetScriptState(), payment_details->Promise(),
                    scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  request->OnUpdatePaymentDetailsTimeoutForTesting();

  v8::MicrotasksScope::PerformCheckpoint(scope.GetScriptState()->GetIsolate());
  EXPECT_EQ(
      "AbortError: Timed out waiting for a "
      "PaymentRequestUpdateEvent.updateWith(promise) to resolve.",
      error_message);

  payment_details->Resolve("foo");
}

TEST(PaymentRequestUpdateEventTest, NotAllowUntrustedEvent) {
  V8TestingScope scope;
  PaymentRequestUpdateEvent* event = PaymentRequestUpdateEvent::Create(
      scope.GetExecutionContext(), event_type_names::kShippingaddresschange);
  event->SetTrusted(false);

  event->updateWith(
      scope.GetScriptState(),
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState())
          ->Promise(),
      scope.GetExceptionState());

  EXPECT_TRUE(scope.GetExceptionState().HadException());
}

}  // namespace
}  // namespace blink
