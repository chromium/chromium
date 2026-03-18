// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_event.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_handler_response.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using payments::mojom::blink::PaymentEventResponseType;

class TestPaymentRequestRespondWithObserver
    : public PaymentRequestRespondWithObserver {
 public:
  TestPaymentRequestRespondWithObserver(ExecutionContext* context,
                                        WaitUntilObserver* observer)
      : PaymentRequestRespondWithObserver(context, 0, observer) {}

  void OnResponseRejected(mojom::blink::ServiceWorkerResponseError error,
                          PaymentEventResponseType response_type) override {
    response_type_ = response_type;
  }

  PaymentEventResponseType response_type() const {
    return response_type_.value();
  }

 private:
  std::optional<PaymentEventResponseType> response_type_;
};

PaymentEventResponseType CreateAndRespondToPaymentRequestEvent(
    V8TestingScope& scope,
    DOMException* exception) {
  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      scope.GetExecutionContext(), WaitUntilObserver::kPaymentRequest,
      /*event_id=*/0);
  wait_until_observer->WillDispatchEvent();
  auto* observer = MakeGarbageCollected<TestPaymentRequestRespondWithObserver>(
      scope.GetExecutionContext(), wait_until_observer);

  auto* event = PaymentRequestEvent::Create(
      event_type_names::kPaymentrequest, PaymentRequestEventInit::Create(),
      mojo::NullRemote(), observer, wait_until_observer,
      scope.GetExecutionContext());
  event->SetTrusted(true);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PaymentHandlerResponse>>(
          scope.GetScriptState());

  event->respondWith(scope.GetScriptState(), resolver->Promise(),
                     ASSERT_NO_EXCEPTION);

  resolver->Reject(exception);
  scope.PerformMicrotaskCheckpoint();

  return observer->response_type();
}

TEST(PaymentRequestEventTest, RespondWithOperationError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_EQ(PaymentEventResponseType::PAYMENT_EVENT_INTERNAL_ERROR,
            CreateAndRespondToPaymentRequestEvent(
                scope, MakeGarbageCollected<DOMException>(
                           DOMExceptionCode::kOperationError, "test error")));
}

TEST(PaymentRequestEventTest, RespondWithOtherError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_EQ(PaymentEventResponseType::PAYMENT_EVENT_REJECT,
            CreateAndRespondToPaymentRequestEvent(
                scope, MakeGarbageCollected<DOMException>(
                           DOMExceptionCode::kAbortError, "test error")));
}

}  // namespace
}  // namespace blink
