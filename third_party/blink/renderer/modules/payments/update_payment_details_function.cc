// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/update_payment_details_function.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"

namespace blink {

// static
v8::Local<v8::Function> UpdatePaymentDetailsFunction::CreateFunction(
    ScriptState* script_state,
    PaymentRequestDelegate* delegate,
    ResolveType resolve_type) {
  UpdatePaymentDetailsFunction* self =
      MakeGarbageCollected<UpdatePaymentDetailsFunction>(script_state, delegate,
                                                         resolve_type);
  return self->BindToV8Function();
}

UpdatePaymentDetailsFunction::UpdatePaymentDetailsFunction(
    ScriptState* script_state,
    PaymentRequestDelegate* delegate,
    ResolveType resolve_type)
    : ScriptFunction(script_state),
      delegate_(delegate),
      resolve_type_(resolve_type) {
  DCHECK(delegate_);
}

void UpdatePaymentDetailsFunction::Trace(blink::Visitor* visitor) {
  visitor->Trace(delegate_);
  ScriptFunction::Trace(visitor);
}

ScriptValue UpdatePaymentDetailsFunction::Call(ScriptValue value) {
  if (!delegate_)
    return ScriptValue();

  switch (resolve_type_) {
    case ResolveType::kFulfill:
      delegate_->OnUpdatePaymentDetails(value);
      break;
    case ResolveType::kReject:
      delegate_->OnUpdatePaymentDetailsFailure(
          ToCoreString(value.V8Value()
                           ->ToString(GetScriptState()->GetContext())
                           .ToLocalChecked()));
      break;
  }
  delegate_ = nullptr;
  return ScriptValue();
}

}  // namespace blink
