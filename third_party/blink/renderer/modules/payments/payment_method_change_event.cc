// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_method_change_event.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

PaymentMethodChangeEvent::~PaymentMethodChangeEvent() = default;

// static
PaymentMethodChangeEvent* PaymentMethodChangeEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit* init) {
  return MakeGarbageCollected<PaymentMethodChangeEvent>(script_state, type,
                                                        init);
}

const String& PaymentMethodChangeEvent::methodName() const {
  return method_name_;
}

const ScriptObject& PaymentMethodChangeEvent::methodDetails() const {
  return method_details_;
}

void PaymentMethodChangeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(method_details_);
  PaymentRequestUpdateEvent::Trace(visitor);
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit* init)
    : PaymentRequestUpdateEvent(ExecutionContext::From(script_state),
                                type,
                                init),
      method_name_(init->methodName()),
      method_details_(init->methodDetails()) {}

}  // namespace blink
