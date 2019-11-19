// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/payments/payment_currency_amount.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PaymentAddress;
class PaymentStateResolver;
class PaymentValidationErrors;
class ScriptState;

class MODULES_EXPORT PaymentResponse final
    : public EventTargetWithInlineData,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable<PaymentResponse> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentResponse);

 public:
  PaymentResponse(ScriptState* script_state,
                  payments::mojom::blink::PaymentResponsePtr response,
                  PaymentAddress* shipping_address,
                  PaymentStateResolver* payment_state_resolver,
                  const String& request_id);
  ~PaymentResponse() override;

  void Update(ScriptState* script_state,
              payments::mojom::blink::PaymentResponsePtr response,
              PaymentAddress* shipping_address);
  void UpdatePayerDetail(payments::mojom::blink::PayerDetailPtr);
  void UpdateDetailsFromJSON(ScriptState* script_state, const String& json);

  ScriptValue toJSONForBinding(ScriptState*) const;

  const String& requestId() const { return request_id_; }
  const String& methodName() const { return method_name_; }
  ScriptValue details(ScriptState* script_state) const;
  PaymentAddress* shippingAddress() const { return shipping_address_.Get(); }
  const String& shippingOption() const { return shipping_option_; }
  const String& payerName() const { return payer_name_; }
  const String& payerEmail() const { return payer_email_; }
  const String& payerPhone() const { return payer_phone_; }

  ScriptPromise complete(ScriptState*, const String& result = "");
  ScriptPromise retry(ScriptState*, const PaymentValidationErrors*);

  bool HasPendingActivity() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(payerdetailchange, kPayerdetailchange)

  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(blink::Visitor*) override;

 private:
  String request_id_;
  String method_name_;
  WorldSafeV8Reference<v8::Value> details_;
  Member<PaymentAddress> shipping_address_;
  String shipping_option_;
  String payer_name_;
  String payer_email_;
  String payer_phone_;
  Member<PaymentStateResolver> payment_state_resolver_;

  DISALLOW_COPY_AND_ASSIGN(PaymentResponse);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_RESPONSE_H_
