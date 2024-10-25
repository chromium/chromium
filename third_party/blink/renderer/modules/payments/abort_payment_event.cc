// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/payments/abort_payment_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_event_init.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class AbortPaymentRespondWithFulfill final
    : public ThenCallable<IDLBoolean, AbortPaymentRespondWithFulfill> {
 public:
  explicit AbortPaymentRespondWithFulfill(
      AbortPaymentRespondWithObserver* observer)
      : observer_(observer) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(observer_);
    ThenCallable<IDLBoolean, AbortPaymentRespondWithFulfill>::Trace(visitor);
  }

  void React(ScriptState* script_state, bool response) {
    DCHECK(observer_);
    observer_->OnResponseFulfilled(script_state, response);
  }

 private:
  Member<AbortPaymentRespondWithObserver> observer_;
};

AbortPaymentEvent* AbortPaymentEvent::Create(
    const AtomicString& type,
    const ExtendableEventInit* initializer) {
  return MakeGarbageCollected<AbortPaymentEvent>(type, initializer, nullptr,
                                                 nullptr);
}

AbortPaymentEvent* AbortPaymentEvent::Create(
    const AtomicString& type,
    const ExtendableEventInit* initializer,
    AbortPaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer) {
  return MakeGarbageCollected<AbortPaymentEvent>(
      type, initializer, respond_with_observer, wait_until_observer);
}

AbortPaymentEvent::~AbortPaymentEvent() = default;

const AtomicString& AbortPaymentEvent::InterfaceName() const {
  return event_interface_names::kAbortPaymentEvent;
}

void AbortPaymentEvent::respondWith(ScriptState* script_state,
                                    ScriptPromise<IDLBoolean> script_promise,
                                    ExceptionState& exception_state) {
  stopImmediatePropagation();
  if (observer_) {
    observer_->RespondWith(
        script_state, script_promise,
        MakeGarbageCollected<AbortPaymentRespondWithFulfill>(observer_),
        exception_state);
  }
}

void AbortPaymentEvent::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  ExtendableEvent::Trace(visitor);
}

AbortPaymentEvent::AbortPaymentEvent(
    const AtomicString& type,
    const ExtendableEventInit* initializer,
    AbortPaymentRespondWithObserver* respond_with_observer,
    WaitUntilObserver* wait_until_observer)
    : ExtendableEvent(type, initializer, wait_until_observer),
      observer_(respond_with_observer) {}

}  // namespace blink
