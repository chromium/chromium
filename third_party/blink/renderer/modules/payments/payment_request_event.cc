// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_request_event.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit& initializer) {
  return new PaymentRequestEvent(type, initializer, nullptr, nullptr);
}

PaymentRequestEvent* PaymentRequestEvent::Create(
    const AtomicString& type,
    const PaymentRequestEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return new PaymentRequestEvent(type, initializer, respond_with_observer,
                                 wait_until_observer);
}

PaymentRequestEvent::~PaymentRequestEvent() = default;

const AtomicString& PaymentRequestEvent::InterfaceName() const {
  return EventNames::PaymentRequestEvent;
}

const String& PaymentRequestEvent::topOrigin() const {
  return top_origin_;
}

const String& PaymentRequestEvent::paymentRequestOrigin() const {
  return payment_request_origin_;
}

const String& PaymentRequestEvent::paymentRequestId() const {
  return payment_request_id_;
}

const HeapVector<PaymentMethodData>& PaymentRequestEvent::methodData() const {
  return method_data_;
}

const ScriptValue PaymentRequestEvent::total(ScriptState* script_state) const {
  return ScriptValue::From(script_state, total_);
}

const HeapVector<PaymentDetailsModifier>& PaymentRequestEvent::modifiers()
    const {
  return modifiers_;
}

const String& PaymentRequestEvent::instrumentKey() const {
  return instrument_key_;
}

ScriptPromise PaymentRequestEvent::openWindow(ScriptState* script_state,
                                              const String& url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!isTrusted()) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "Cannot open a window when the event is not trusted"));
    return promise;
  }

  KURL parsed_url_to_open = context->CompleteURL(url);
  if (!parsed_url_to_open.IsValid()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "'" + url + "' is not a valid URL."));
    return promise;
  }

  if (!context->GetSecurityOrigin()->IsSameSchemeHostPort(
          SecurityOrigin::Create(parsed_url_to_open).get())) {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return promise;
  }

  if (!context->IsWindowInteractionAllowed()) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kNotAllowedError,
        "Not allowed to open a window without user activation"));
    return promise;
  }
  context->ConsumeWindowInteraction();

  ServiceWorkerGlobalScopeClient::From(context)->OpenWindowForPaymentHandler(
      parsed_url_to_open, resolver);
  return promise;
}

void PaymentRequestEvent::respondWith(ScriptState* script_state,
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
    observer_->RespondWith(script_state, script_promise, exception_state);
  }
}

void PaymentRequestEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(method_data_);
  visitor->Trace(modifiers_);
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

PaymentRequestEvent::PaymentRequestEvent(
    const AtomicString& type,
    const PaymentRequestEventInit& initializer,
    RespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      top_origin_(initializer.topOrigin()),
      payment_request_origin_(initializer.paymentRequestOrigin()),
      payment_request_id_(initializer.paymentRequestId()),
      method_data_(initializer.hasMethodData()
                       ? initializer.methodData()
                       : HeapVector<PaymentMethodData>()),
      total_(initializer.hasTotal() ? initializer.total()
                                    : PaymentCurrencyAmount()),
      modifiers_(initializer.hasModifiers()
                     ? initializer.modifiers()
                     : HeapVector<PaymentDetailsModifier>()),
      instrument_key_(initializer.instrumentKey()),
      observer_(respond_with_observer) {}

}  // namespace blink
