// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

// Merchant and user identifying fields should return empty values when
// ClearIdentityInCanMakePaymentEvent is enabled.
bool ClearFields(ScriptState* script_state) {
  return RuntimeEnabledFeatures::ClearIdentityInCanMakePaymentEventEnabled(
      ExecutionContext::From(script_state));
}

}  // namespace

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer) {
  return MakeGarbageCollected<CanMakePaymentEvent>(
      script_state, type, initializer, nullptr, nullptr);
}

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer,
    CanMakePaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return MakeGarbageCollected<CanMakePaymentEvent>(
      script_state, type, initializer, respond_with_observer,
      wait_until_observer);
}

CanMakePaymentEvent::~CanMakePaymentEvent() = default;

const AtomicString& CanMakePaymentEvent::InterfaceName() const {
  return event_interface_names::kCanMakePaymentEvent;
}

const String& CanMakePaymentEvent::topOrigin() const {
  return top_origin_;
}

const String& CanMakePaymentEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const HeapVector<Member<PaymentMethodData>>& CanMakePaymentEvent::methodData()
    const {
  return method_data_;
}

const HeapVector<Member<PaymentDetailsModifier>>&
CanMakePaymentEvent::modifiers() const {
  return modifiers_;
}

void CanMakePaymentEvent::respondWith(ScriptState* script_state,
                                      ScriptPromise script_promise,
                                      ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot respond with data when the event is not trusted");
    return;
  }

  stopImmediatePropagation();
  if (observer_) {
    observer_->ObservePromiseResponse(script_state, script_promise,
                                      exception_state);
  }
}

void CanMakePaymentEvent::Trace(Visitor* visitor) const {
  visitor->Trace(method_data_);
  visitor->Trace(modifiers_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

// TODO(crbug.com/1070871): Use fooOr() in members' initializers.
CanMakePaymentEvent::CanMakePaymentEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer,
    CanMakePaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_origin_(initializer->hasTopOrigin() && !ClearFields(script_state)
                      ? initializer->topOrigin()
                      : String()),
      payment_request_origin_(initializer->hasPaymentRequestOrigin() &&
                                      !ClearFields(script_state)
                                  ? initializer->paymentRequestOrigin()
                                  : String()),
      method_data_(initializer->hasMethodData() && !ClearFields(script_state)
                       ? initializer->methodData()
                       : HeapVector<Member<PaymentMethodData>>()),
      modifiers_(initializer->hasModifiers() && !ClearFields(script_state)
                     ? initializer->modifiers()
                     : HeapVector<Member<PaymentDetailsModifier>>()),
      observer_(respond_with_observer) {}

}  // namespace blink
