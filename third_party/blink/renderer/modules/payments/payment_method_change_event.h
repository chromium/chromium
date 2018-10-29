// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_METHOD_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_METHOD_CHANGE_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/payments/payment_method_change_event_init.h"
#include "third_party/blink/renderer/modules/payments/payment_request_update_event.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT PaymentMethodChangeEvent final
    : public PaymentRequestUpdateEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PaymentMethodChangeEvent() override;

  static PaymentMethodChangeEvent* Create(
      ScriptState*,
      const AtomicString& type,
      const PaymentMethodChangeEventInit& = PaymentMethodChangeEventInit());

  const String& methodName() const;
  const ScriptValue methodDetails(ScriptState*) const;

 private:
  PaymentMethodChangeEvent(ScriptState*,
                           const AtomicString& type,
                           const PaymentMethodChangeEventInit&);

  String method_name_;
  ScriptValue method_details_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_METHOD_CHANGE_EVENT_H_
