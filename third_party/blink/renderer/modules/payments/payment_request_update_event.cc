// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_update_event.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"
#include "third_party/blink/renderer/modules/payments/update_payment_details_function.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() = default;

PaymentRequestUpdateEvent* PaymentRequestUpdateEvent::Create(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit* init) {
  return MakeGarbageCollected<PaymentRequestUpdateEvent>(execution_context,
                                                         type, init);
}

void PaymentRequestUpdateEvent::SetPaymentRequest(
    PaymentRequestDelegate* request) {
  request_ = request;
}

void PaymentRequestUpdateEvent::updateWith(ScriptState* script_state,
                                           ScriptPromiseUntyped promise,
                                           ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot update details when the event is not trusted");
    return;
  }

  if (wait_for_update_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot update details twice");
    return;
  }

  if (!request_)
    return;

  if (!request_->IsInteractive()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "PaymentRequest is no longer interactive");
    return;
  }

  stopPropagation();
  stopImmediatePropagation();
  wait_for_update_ = true;

  promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<UpdatePaymentDetailsFunction>(
              request_, UpdatePaymentDetailsFunction::ResolveType::kFulfill)),
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<UpdatePaymentDetailsFunction>(
              request_, UpdatePaymentDetailsFunction::ResolveType::kReject)));
}

void PaymentRequestUpdateEvent::Trace(Visitor* visitor) const {
  visitor->Trace(request_);
  Event::Trace(visitor);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(
    ExecutionContext* execution_context,
    const AtomicString& type,
    const PaymentRequestUpdateEventInit* init)
    : Event(type, init), wait_for_update_(false) {}

}  // namespace blink
