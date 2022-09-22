// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/merchant_validation_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/modules/payments/payments_validators.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

MerchantValidationEvent* MerchantValidationEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const MerchantValidationEventInit* initializer,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<MerchantValidationEvent>(
      script_state, type, initializer, exception_state);
}

MerchantValidationEvent::~MerchantValidationEvent() = default;

const AtomicString& MerchantValidationEvent::InterfaceName() const {
  return event_interface_names::kMerchantValidationEvent;
}

const String& MerchantValidationEvent::methodName() const {
  return method_name_;
}

const KURL& MerchantValidationEvent::validationURL() const {
  return validation_url_;
}

MerchantValidationEvent::MerchantValidationEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const MerchantValidationEventInit* initializer,
    ExceptionState& exception_state)
    : Event(type, initializer),
      method_name_(initializer->methodName()),
      wait_for_update_(false) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  validation_url_ =
      KURL(execution_context->BaseURL(), initializer->validationURL());
  if (!validation_url_.IsValid()) {
    exception_state.ThrowTypeError("Invalid validation URL");
    return;
  }
  if (!method_name_.IsEmpty() &&
      !PaymentsValidators::IsValidMethodFormat(method_name_)) {
    exception_state.ThrowRangeError("Invalid payment method identifier.");
    return;
  }
}

void MerchantValidationEvent::complete(ScriptState* script_state,
                                       ScriptPromise merchantSessionPromise,
                                       ExceptionState& exception_state) {
  if (!isTrusted()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Validation event not dispatched by the user agent.");
    return;
  }
  if (!wait_for_update_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Validation event already triggered once.");
    return;
  }

  // TODO(https://crbug.com/867904): Finish implementing the rest of this
  // algorithm, starting from step 4 of
  // https://w3c.github.io/payment-request/#complete-method-0.
}

}  // namespace blink
