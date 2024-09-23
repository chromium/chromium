// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SUBSCRIBER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SUBSCRIBER_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/forward.h"

namespace blink {

class AbortController;
class ObservableInternalObserver;
class Observable;
class ScriptState;
class SubscribeOptions;
class V8VoidFunction;

class CORE_EXPORT Subscriber final : public ScriptWrappable,
                                     public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Subscriber(base::PassKey<Observable>,
             ScriptState*,
             ObservableInternalObserver*,
             SubscribeOptions*);

  // API methods.
  void next(ScriptValue);
  void complete(ScriptState*);
  void error(ScriptState*, ScriptValue);
  void addTeardown(V8VoidFunction*);

  // API attributes.
  bool active() const { return active_; }
  AbortSignal* signal() const;

  void Trace(Visitor*) const override;

 private:
  class CloseSubscriptionAlgorithm;

  // This method is idempotent; it may be called more than once, re-entrantly,
  // which is safe, because it is guarded by an `active_` check. See the
  // implementation's documentation.
  //
  // The `abort_reason` parameter is an error value that serves as the abort
  // reason for when this method aborts `subscription_controller_`. It is
  // populated in two cases:
  //   1. Consumer-initiated unsubscription: when `CloseSubscription()` is
  //      called as a result of the downstream `AbortSignal` (passed in via
  //      `SubscribeOptions` in the constructor) gets aborted.
  //   2. Producer-initiated unsubscription: when `Subscriber::error()` is
  //      called, `abort_reason` takes on the provided error value, so that the
  //      producer error is communicated through to `this`'s signal and any
  //      upstream signals.
  void CloseSubscription(ScriptState* script_state,
                         std::optional<ScriptValue> abort_reason);

  // The `ObservableInternalObserver` class encapsulates algorithms to call when
  // `this` produces values or actions that need to be pushed to the subscriber
  // handlers.
  //
  // https://wicg.github.io/observable/#subscriber-next-algorithm:
  // "Each Subscriber has a next algorithm, which is a next steps-or-null."
  //
  // https://wicg.github.io/observable/#subscriber-error-algorithm:
  // "Each Subscriber has a error algorithm, which is an error steps-or-null."

  // https://wicg.github.io/observable/#subscriber-complete-algorithm:
  // "Each Subscriber has a complete algorithm, which is a complete
  // steps-or-null."
  Member<ObservableInternalObserver> internal_observer_;

  // This starts out true, and becomes false only once `Subscriber::{complete(),
  // error()}` are called (just before the corresponding `Observer` callbacks
  // are invoked) or once the subscriber unsubscribes by aborting the
  // `AbortSignal` that it passed into `Observable::subscribe()`.
  bool active_ = true;

  // `subscription_controller_` is aborted in two cases:
  //   1. Producer-initiated unsubscription: when `error()`/`complete()` are
  //      called, they invoke `CloseSubscription()` directly, which aborts this
  //      controller.
  //   2. Consumer-initiated unsubscription: when the downstream `AbortSignal`
  //      is aborted, the `CloseSubscriptionAlgorithm` runs, invoking
  //      `CloseSubscription()`, which aborts this controller.
  //
  // This controller's signal is what `this` exposes as the `signal` WebIDL
  // attribute.
  Member<AbortController> subscription_controller_;

  // Non-null before `CloseSubscription()` is called.
  Member<AbortSignal::AlgorithmHandle> close_subscription_algorithm_handle_;

  HeapVector<Member<V8VoidFunction>> teardown_callbacks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SUBSCRIBER_H_
