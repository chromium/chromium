// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/update_payment_details_function.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_details_update.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"

namespace blink {

UpdatePaymentDetailsResolve::UpdatePaymentDetailsResolve(
    PaymentRequestDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

void UpdatePaymentDetailsResolve::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  ThenCallable<PaymentDetailsUpdate, UpdatePaymentDetailsResolve>::Trace(
      visitor);
}

void UpdatePaymentDetailsResolve::React(ScriptState*,
                                        PaymentDetailsUpdate* value) {
  if (!delegate_) {
    return;
  }

  delegate_->OnUpdatePaymentDetails(value);
  delegate_ = nullptr;
}

UpdatePaymentDetailsReject::UpdatePaymentDetailsReject(
    PaymentRequestDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

void UpdatePaymentDetailsReject::Trace(Visitor* visitor) const {
  visitor->Trace(delegate_);
  ThenCallable<IDLAny, UpdatePaymentDetailsReject>::Trace(visitor);
}

void UpdatePaymentDetailsReject::React(ScriptState* script_state,
                                       ScriptValue value) {
  if (!delegate_) {
    return;
  }
  delegate_->OnUpdatePaymentDetailsFailure(ToCoreString(
      script_state->GetIsolate(),
      value.V8Value()->ToString(script_state->GetContext()).ToLocalChecked()));
  delegate_ = nullptr;
}

}  // namespace blink
