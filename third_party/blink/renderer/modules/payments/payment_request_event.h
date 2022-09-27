// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/payments/payment_handler_host.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_address_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_request_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class ExceptionState;
class RespondWithObserver;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;

class MODULES_EXPORT PaymentRequestEvent final : public ExtendableEvent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PaymentRequestEvent* Create(
      const AtomicString& type,
      const PaymentRequestEventInit*,
      mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host =
          mojo::NullRemote(),
      RespondWithObserver* respond_with_observer = nullptr,
      WaitUntilObserver* wait_until_observer = nullptr,
      ExecutionContext* execution_context = nullptr);

  PaymentRequestEvent(
      const AtomicString& type,
      const PaymentRequestEventInit*,
      mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost> host,
      RespondWithObserver*,
      WaitUntilObserver*,
      ExecutionContext* execution_context);

  PaymentRequestEvent(const PaymentRequestEvent&) = delete;
  PaymentRequestEvent& operator=(const PaymentRequestEvent&) = delete;

  ~PaymentRequestEvent() override;

  const AtomicString& InterfaceName() const override;

  const String& topOrigin() const;
  const String& paymentRequestOrigin() const;
  const String& paymentRequestId() const;
  const HeapVector<Member<PaymentMethodData>>& methodData() const;
  const ScriptValue total(ScriptState*) const;
  const HeapVector<Member<PaymentDetailsModifier>>& modifiers() const;
  const String& instrumentKey() const;
  const ScriptValue paymentOptions(ScriptState*) const;
  absl::optional<HeapVector<Member<PaymentShippingOption>>> shippingOptions()
      const;

  ScriptPromise openWindow(ScriptState*, const String& url);
  ScriptPromise changePaymentMethod(ScriptState*,
                                    const String& method_name,
                                    ExceptionState& exception_state);
  ScriptPromise changePaymentMethod(ScriptState*,
                                    const String& method_name,
                                    const ScriptValue& method_details,
                                    ExceptionState& exception_state);
  ScriptPromise changeShippingAddress(ScriptState*,
                                      AddressInit*,
                                      ExceptionState&);
  ScriptPromise changeShippingOption(ScriptState*,
                                     const String& shipping_option_id,
                                     ExceptionState&);
  void respondWith(ScriptState*, ScriptPromise, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void OnChangePaymentRequestDetailsResponse(
      payments::mojom::blink::PaymentRequestDetailsUpdatePtr);
  void OnHostConnectionError();

  String top_origin_;
  String payment_request_origin_;
  String payment_request_id_;
  HeapVector<Member<PaymentMethodData>> method_data_;
  Member<const PaymentCurrencyAmount> total_;
  HeapVector<Member<PaymentDetailsModifier>> modifiers_;
  String instrument_key_;
  Member<const PaymentOptions> payment_options_;
  HeapVector<Member<PaymentShippingOption>> shipping_options_;

  Member<ScriptPromiseResolver> change_payment_request_details_resolver_;
  Member<RespondWithObserver> observer_;
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerHost>
      payment_handler_host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_EVENT_H_
