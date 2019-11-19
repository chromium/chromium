// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_WAIT_UNTIL_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_WAIT_UNTIL_OBSERVER_H_

#include "base/callback.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class ScriptValue;

// Created for each ExtendableEvent instance.
class MODULES_EXPORT WaitUntilObserver final
    : public GarbageCollected<WaitUntilObserver>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(WaitUntilObserver);

 public:
  using PromiseSettledCallback =
      base::RepeatingCallback<void(const ScriptValue&)>;

  enum EventType {
    kAbortPayment,
    kActivate,
    kCanMakePayment,
    kCookieChange,
    kFetch,
    kInstall,
    kMessage,
    kNotificationClick,
    kNotificationClose,
    kPaymentRequest,
    kPush,
    kPushSubscriptionChange,
    kSync,
    kPeriodicSync,
    kBackgroundFetchAbort,
    kBackgroundFetchClick,
    kBackgroundFetchFail,
    kBackgroundFetchSuccess,
    kContentDelete,
  };

  static WaitUntilObserver* Create(ExecutionContext*, EventType, int event_id);

  WaitUntilObserver(ExecutionContext*, EventType, int event_id);

  // Must be called before dispatching the event.
  void WillDispatchEvent();
  // Must be called after dispatching the event. If |event_dispatch_failed| is
  // true, then DidDispatchEvent() immediately reports to
  // ServiceWorkerGlobalScope that the event finished, without waiting for
  // all waitUntil promises to settle.
  void DidDispatchEvent(bool event_dispatch_failed);

  // Observes the promise and delays reporting to ServiceWorkerGlobalScope
  // that the event completed until the promise is resolved or rejected.
  //
  // WaitUntil may be called multiple times. The event is extended until all
  // promises have settled.
  //
  // If provided, |on_promise_fulfilled| or |on_promise_rejected| is invoked
  // once |script_promise| fulfills or rejects. This enables the caller to do
  // custom handling.
  //
  // If the event is not active, throws a DOMException and returns false. In
  // this case the promise is ignored, and |on_promise_fulfilled| and
  // |on_promise_rejected| will not be called.
  bool WaitUntil(
      ScriptState*,
      ScriptPromise /* script_promise */,
      ExceptionState&,
      PromiseSettledCallback on_promise_fulfilled = PromiseSettledCallback(),
      PromiseSettledCallback on_promise_rejected = PromiseSettledCallback());

  // Whether the associated event is active.
  // https://w3c.github.io/ServiceWorker/#extendableevent-active.
  bool IsEventActive() const;

  // Whether the event is being dispatched, i.e., the event handler
  // is being run.
  // https://dom.spec.whatwg.org/#dispatch-flag
  // TODO(falken): Can this just use Event::IsBeingDispatched?
  bool IsDispatchingEvent() const;

  void Trace(blink::Visitor*) override;

 private:
  friend class InternalsServiceWorker;
  class ThenFunction;

  enum class EventDispatchState {
    // Event dispatch has not yet started.
    kInitial,
    // Event dispatch has started but not yet finished.
    kDispatching,
    // Event dispatch completed. There may still be outstanding waitUntil
    // promises that must settle before notifying ServiceWorkerGlobalScope
    // that the event finished.
    kDispatched,
    // Event dispatch failed. Any outstanding waitUntil promises are ignored.
    kFailed
  };

  void IncrementPendingPromiseCount();
  void DecrementPendingPromiseCount();

  // Enqueued as a microtask when a promise passed to a waitUntil() call that is
  // associated with this observer was fulfilled.
  void OnPromiseFulfilled();
  // Enqueued as a microtask when a promise passed to a waitUntil() call that is
  // associated with this observer was rejected.
  void OnPromiseRejected();

  void ConsumeWindowInteraction(TimerBase*);

  void MaybeCompleteEvent();

  EventType type_;
  int event_id_;
  int pending_promises_ = 0;
  EventDispatchState event_dispatch_state_ = EventDispatchState::kInitial;
  bool has_rejected_promise_ = false;
  TaskRunnerTimer<WaitUntilObserver> consume_window_interaction_timer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_WAIT_UNTIL_OBSERVER_H_
