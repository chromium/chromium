// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/payment_method_change_event.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

PaymentMethodChangeEvent::~PaymentMethodChangeEvent() = default;

// static
PaymentMethodChangeEvent* PaymentMethodChangeEvent::Create(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit& init) {
  return new PaymentMethodChangeEvent(script_state, type, init);
}

const String& PaymentMethodChangeEvent::methodName() const {
  return method_name_;
}

const ScriptValue PaymentMethodChangeEvent::methodDetails(
    ScriptState* script_state) const {
  return ScriptValue(script_state, method_details_.V8ValueFor(script_state));
}

PaymentMethodChangeEvent::PaymentMethodChangeEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const PaymentMethodChangeEventInit& init)
    : PaymentRequestUpdateEvent(ExecutionContext::From(script_state),
                                type,
                                init),
      method_name_(init.methodName()),
      method_details_(init.hasMethodDetails()
                          ? init.methodDetails()
                          : ScriptValue::CreateNull(script_state)) {}

}  // namespace blink
