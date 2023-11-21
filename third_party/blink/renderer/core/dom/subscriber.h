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
class Observable;
class Observer;
class ScriptState;
class SubscribeOptions;
class V8ObserverCallback;
class V8ObserverCompleteCallback;
class V8VoidFunction;

class CORE_EXPORT Subscriber final : public ScriptWrappable,
                                     public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Subscriber(base::PassKey<Observable>,
             ScriptState*,
             Observer*,
             SubscribeOptions*);

  // API methods.
  void next(ScriptValue);
  void complete(ScriptState*);
  void error(ScriptState*, ScriptValue);
  void addTeardown(V8VoidFunction*);

  // API attributes.
  bool active() { return active_; }
  AbortSignal* signal() { return signal_.Get(); }

  void Trace(Visitor*) const override;

 private:
  class CloseSubscriptionAlgorithm;

  // This method may be called more than once. See the documentation in the
  // constructor implementation.
  void CloseSubscription();

  // Any of these may be null, since they are derived from non-required
  // dictionary members passed into `Observable::subscribe()` used to construct
  // `this`.
  Member<V8ObserverCallback> next_;
  Member<V8ObserverCompleteCallback> complete_;
  Member<V8ObserverCallback> error_;

  // This starts out true, and becomes false only once `Subscriber::{complete(),
  // error()}` are called (just before the corresponding `Observer` callbacks
  // are invoked) or once the subscriber unsubscribes by aborting the
  // `AbortSignal` that it passed into `Observable::subscribe()`.
  bool active_ = true;

  // `complete_or_error_controller_` is aborted in response to `complete()` or
  // `error()` methods being called on `this`. Specifically, the signal is
  // aborted *after* the associated `Observer` callback is invoked. This
  // controller's signal is one of the parent signals for `signal_` below.
  Member<AbortController> complete_or_error_controller_;

  // Never null. It is exposed via the `signal` WebIDL attribute, and represents
  // whether or not the current subscription has been aborted or not. This
  // signal is a dependent signal, constructed from two signals:
  //  - The input `Observer#signal`, if present
  //  - The signal associated with `complete_or_error_controller_` above
  Member<AbortSignal> signal_;

  // Non-null before `CloseSubscription()` is called.
  Member<AbortSignal::AlgorithmHandle> close_subscription_algorithm_handle_;

  HeapVector<Member<V8VoidFunction>> teardown_callbacks_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SUBSCRIBER_H_
