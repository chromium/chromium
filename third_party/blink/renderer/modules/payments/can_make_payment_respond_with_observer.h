// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_CAN_MAKE_PAYMENT_RESPOND_WITH_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_CAN_MAKE_PAYMENT_RESPOND_WITH_OBSERVER_H_

#include "third_party/blink/public/mojom/payments/payment_app.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"

namespace blink {

class ExecutionContext;
class ScriptValue;
class WaitUntilObserver;

// Implementation for CanMakePaymentEvent.respondWith() and
// CanMakePayment.respondWithMinimalUI(), which are used by the payment handler
// to indicate whether it can respond to a payment request.
class MODULES_EXPORT CanMakePaymentRespondWithObserver final
    : public RespondWithObserver {
 public:
  CanMakePaymentRespondWithObserver(ExecutionContext*,
                                    int event_id,
                                    WaitUntilObserver*);
  ~CanMakePaymentRespondWithObserver() override = default;

  void OnResponseRejected(mojom::blink::ServiceWorkerResponseError) override;
  void OnResponseFulfilled(ScriptState*,
                           const ScriptValue&,
                           const ExceptionContext&) override;
  void OnNoResponse() override;

  void Trace(Visitor*) const override;

  // Observes the given promise and calls OnResponseRejected() or
  // OnResponseFulfilled().
  void ObservePromiseResponse(ScriptState*,
                              ScriptPromise,
                              ExceptionState&,
                              bool is_minimal_ui);

 private:
  void OnResponseFulfilledForMinimalUI(ScriptState*,
                                       const ScriptValue&,
                                       ExceptionState&);

  void ConsoleWarning(const String& message);
  void RespondWithoutMinimalUI(
      payments::mojom::blink::CanMakePaymentEventResponseType response_type,
      bool can_make_payment);
  void RespondInternal(
      payments::mojom::blink::CanMakePaymentEventResponseType response_type,
      bool can_make_payment,
      bool ready_for_minimal_ui,
      const String& account_balance);

  bool is_minimal_ui_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_CAN_MAKE_PAYMENT_RESPOND_WITH_OBSERVER_H_
