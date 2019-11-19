// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"

namespace blink {

class PaymentRequestDelegate;
class ScriptState;
class ScriptValue;

class UpdatePaymentDetailsFunction : public ScriptFunction {
 public:
  enum class ResolveType {
    kFulfill,
    kReject,
  };

  static v8::Local<v8::Function> CreateFunction(ScriptState*,
                                                PaymentRequestDelegate*,
                                                ResolveType);

  UpdatePaymentDetailsFunction(ScriptState*,
                               PaymentRequestDelegate*,
                               ResolveType);
  void Trace(blink::Visitor*) override;
  ScriptValue Call(ScriptValue) override;

 private:
  Member<PaymentRequestDelegate> delegate_;
  ResolveType resolve_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_UPDATE_PAYMENT_DETAILS_FUNCTION_H_
