// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace blink {
namespace {

template <typename IDLType, typename T>
ScriptValue GetScriptValueOrUndefined(ScriptState* script_state,
                                      const T& value) {
  v8::Isolate* isolate = script_state->GetIsolate();

  // Merchant and user identifying fields should return undefined values when
  // ClearIdentityInCanMakePaymentEvent is enabled.
  if (RuntimeEnabledFeatures::ClearIdentityInCanMakePaymentEventEnabled(
          ExecutionContext::From(script_state))) {
    return ScriptValue(isolate, v8::Undefined(isolate));
  }

  return ScriptValue(
      isolate, ToV8Traits<IDLType>::ToV8(script_state, value).ToLocalChecked());
}

}  // namespace

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer) {
  return MakeGarbageCollected<CanMakePaymentEvent>(type, initializer, nullptr,
                                                   nullptr);
}

CanMakePaymentEvent* CanMakePaymentEvent::Create(
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer,
    CanMakePaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return MakeGarbageCollected<CanMakePaymentEvent>(
      type, initializer, respond_with_observer, wait_until_observer);
}

CanMakePaymentEvent::~CanMakePaymentEvent() = default;

const AtomicString& CanMakePaymentEvent::InterfaceName() const {
  return event_interface_names::kCanMakePaymentEvent;
}

ScriptValue CanMakePaymentEvent::topOrigin(ScriptState* script_state) const {
  return GetScriptValueOrUndefined<IDLUSVString>(script_state, top_origin_);
}

ScriptValue CanMakePaymentEvent::paymentRequestOrigin(
    ScriptState* script_state) const {
  return GetScriptValueOrUndefined<IDLUSVString>(script_state,
                                                 payment_request_origin_);
}

ScriptValue CanMakePaymentEvent::methodData(ScriptState* script_state) const {
  return GetScriptValueOrUndefined<IDLArray<PaymentMethodData>>(script_state,
                                                                method_data_);
}

ScriptValue CanMakePaymentEvent::modifiers(ScriptState* script_state) const {
  return GetScriptValueOrUndefined<IDLArray<PaymentDetailsModifier>>(
      script_state, modifiers_);
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
    const AtomicString& type,
    const CanMakePaymentEventInit* initializer,
    CanMakePaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_origin_(initializer->hasTopOrigin() ? initializer->topOrigin()
                                              : String()),
      payment_request_origin_(initializer->hasPaymentRequestOrigin()
                                  ? initializer->paymentRequestOrigin()
                                  : String()),
      method_data_(initializer->hasMethodData()
                       ? initializer->methodData()
                       : HeapVector<Member<PaymentMethodData>>()),
      modifiers_(initializer->hasModifiers()
                     ? initializer->modifiers()
                     : HeapVector<Member<PaymentDetailsModifier>>()),
      observer_(respond_with_observer) {}

}  // namespace blink
