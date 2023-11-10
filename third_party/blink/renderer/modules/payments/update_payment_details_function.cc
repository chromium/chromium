// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/update_payment_details_function.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"

namespace blink {

UpdatePaymentDetailsFunction::UpdatePaymentDetailsFunction(
    PaymentRequestDelegate* delegate,
    ResolveType resolve_type)
    : delegate_(delegate), resolve_type_(resolve_type) {
  DCHECK(delegate_);
}

void UpdatePaymentDetailsFunction::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  ScriptFunction::Callable::Trace(visitor);
}

ScriptValue UpdatePaymentDetailsFunction::Call(ScriptState* script_state,
                                               ScriptValue value) {
  if (!delegate_)
    return ScriptValue();

  switch (resolve_type_) {
    case ResolveType::kFulfill:
      delegate_->OnUpdatePaymentDetails(value);
      break;
    case ResolveType::kReject:
      delegate_->OnUpdatePaymentDetailsFailure(ToCoreString(
          script_state->GetIsolate(), value.V8Value()
                                          ->ToString(script_state->GetContext())
                                          .ToLocalChecked()));
      break;
  }
  delegate_ = nullptr;
  return ScriptValue();
}

}  // namespace blink
