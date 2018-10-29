// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_UPDATE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_UPDATE_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/payments/payment_request_update_event_init.h"
#include "third_party/blink/renderer/modules/payments/payment_updater.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ScriptState;

class MODULES_EXPORT PaymentRequestUpdateEvent : public Event,
                                                 public PaymentUpdater {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaymentRequestUpdateEvent)

 public:
  ~PaymentRequestUpdateEvent() override;

  static PaymentRequestUpdateEvent* Create(
      ExecutionContext*,
      const AtomicString& type,
      const PaymentRequestUpdateEventInit& = PaymentRequestUpdateEventInit());

  void SetPaymentDetailsUpdater(PaymentUpdater*);

  void updateWith(ScriptState*, ScriptPromise, ExceptionState&);

  bool is_waiting_for_update() const { return wait_for_update_; }

  // PaymentUpdater:
  void OnUpdatePaymentDetails(const ScriptValue& details_script_value) override;
  void OnUpdatePaymentDetailsFailure(const String& error) override;

  void Trace(blink::Visitor*) override;

  void OnUpdateEventTimeoutForTesting();

 protected:
  PaymentRequestUpdateEvent(ExecutionContext*,
                            const AtomicString& type,
                            const PaymentRequestUpdateEventInit&);

 private:
  void OnUpdateEventTimeout(TimerBase*);

  // True after event.updateWith() was called.
  bool wait_for_update_;

  Member<PaymentUpdater> updater_;
  TaskRunnerTimer<PaymentRequestUpdateEvent> abort_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_PAYMENT_REQUEST_UPDATE_EVENT_H_
