// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/payments/mojom/payment_request_data.mojom-blink-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/payments/payment_method_data.h"
#include "third_party/blink/renderer/modules/payments/payment_options.h"
#include "third_party/blink/renderer/modules/payments/payment_request_delegate.h"
#include "third_party/blink/renderer/modules/payments/payment_state_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PaymentAddress;
class PaymentDetailsInit;
class PaymentRequestUpdateEvent;
class PaymentResponse;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT PaymentRequest final
    : public EventTargetWithInlineData,
      public payments::mojom::blink::PaymentRequestClient,
      public PaymentStateResolver,
      public PaymentRequestDelegate,
      public ContextLifecycleObserver,
      public ActiveScriptWrappable<PaymentRequest> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentRequest);
  USING_PRE_FINALIZER(PaymentRequest, ClearResolversAndCloseMojoConnection);

 public:
  static PaymentRequest* Create(ExecutionContext*,
                                const HeapVector<Member<PaymentMethodData>>&,
                                const PaymentDetailsInit*,
                                ExceptionState&);
  static PaymentRequest* Create(ExecutionContext*,
                                const HeapVector<Member<PaymentMethodData>>&,
                                const PaymentDetailsInit*,
                                const PaymentOptions*,
                                ExceptionState&);

  PaymentRequest(ExecutionContext*,
                 const HeapVector<Member<PaymentMethodData>>&,
                 const PaymentDetailsInit*,
                 const PaymentOptions*,
                 ExceptionState&);
  ~PaymentRequest() override;

  ScriptPromise show(ScriptState*);
  ScriptPromise show(ScriptState*, ScriptPromise details_promise);
  ScriptPromise abort(ScriptState*);

  const String& id() const { return id_; }
  PaymentAddress* getShippingAddress() const { return shipping_address_.Get(); }
  const String& shippingOption() const { return shipping_option_; }
  const String& shippingType() const { return shipping_type_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingaddresschange, kShippingaddresschange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(shippingoptionchange, kShippingoptionchange)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(paymentmethodchange, kPaymentmethodchange)

  ScriptPromise canMakePayment(ScriptState*);
  ScriptPromise hasEnrolledInstrument(ScriptState*);

  // ScriptWrappable:
  bool HasPendingActivity() const override;

  // EventTargetWithInlineData:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // PaymentStateResolver:
  ScriptPromise Complete(ScriptState*, PaymentComplete result) override;
  ScriptPromise Retry(ScriptState*, const PaymentValidationErrors*) override;

  // PaymentRequestDelegate:
  void OnUpdatePaymentDetails(const ScriptValue& details_script_value) override;
  void OnUpdatePaymentDetailsFailure(const String& error) override;
  bool IsInteractive() const override;

  void Trace(blink::Visitor*) override;

  void OnCompleteTimeoutForTesting();
  void OnUpdatePaymentDetailsTimeoutForTesting();

  enum {
    // Implementation defined constants controlling the allowed list length
    kMaxListSize = 1024,
    // ... and string length.
    kMaxStringLength = 1024,
  };

 private:
  // Called when the renderer loses the IPC connection to the browser.
  void OnConnectionError();

  // LifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

  // payments::mojom::blink::PaymentRequestClient:
  void OnPaymentMethodChange(const String& method_name,
                             const String& stringified_details) override;
  void OnShippingAddressChange(
      payments::mojom::blink::PaymentAddressPtr) override;
  void OnShippingOptionChange(const String& shipping_option_id) override;
  void OnPayerDetailChange(payments::mojom::blink::PayerDetailPtr) override;
  void OnPaymentResponse(payments::mojom::blink::PaymentResponsePtr) override;
  void OnError(payments::mojom::blink::PaymentErrorReason,
               const String& error_message) override;
  void OnComplete() override;
  void OnAbort(bool aborted_successfully) override;
  void OnCanMakePayment(
      payments::mojom::blink::CanMakePaymentQueryResult) override;
  void OnHasEnrolledInstrument(
      payments::mojom::blink::HasEnrolledInstrumentQueryResult) override;
  void WarnNoFavicon() override;

  void OnCompleteTimeout(TimerBase*);
  void OnUpdatePaymentDetailsTimeout(TimerBase*);

  // Clears the promise resolvers and closes the Mojo connection.
  void ClearResolversAndCloseMojoConnection();

  // Returns the resolver for the current pending accept promise that should
  // be resolved if the user accepts or aborts the payment request.
  // The pending promise can be [[acceptPromise]] or [[retryPromise]] in the
  // spec.
  ScriptPromiseResolver* GetPendingAcceptPromiseResolver() const;

  // Implements the PaymentRequest updated algorithm.
  // https://w3c.github.io/payment-request/#paymentrequest-updated-algorithm
  void DispatchPaymentRequestUpdateEvent(EventTarget* event_target,
                                         PaymentRequestUpdateEvent* event);

  Member<const PaymentOptions> options_;
  Member<PaymentAddress> shipping_address_;
  Member<PaymentResponse> payment_response_;
  String id_;
  String shipping_option_;
  String shipping_type_;
  HashSet<String> method_names_;
  Member<ScriptPromiseResolver> accept_resolver_;
  Member<ScriptPromiseResolver> complete_resolver_;
  Member<ScriptPromiseResolver> retry_resolver_;
  Member<ScriptPromiseResolver> abort_resolver_;
  Member<ScriptPromiseResolver> can_make_payment_resolver_;
  Member<ScriptPromiseResolver> has_enrolled_instrument_resolver_;
  mojo::Remote<payments::mojom::blink::PaymentRequest> payment_provider_;
  mojo::Receiver<payments::mojom::blink::PaymentRequestClient> client_receiver_{
      this};
  TaskRunnerTimer<PaymentRequest> complete_timer_;
  TaskRunnerTimer<PaymentRequest> update_payment_details_timer_;
  bool is_waiting_for_show_promise_to_resolve_;
  bool basic_card_has_supported_card_types_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_H_
