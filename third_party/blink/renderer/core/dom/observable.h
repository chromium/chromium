// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExecutionContext;
class ObservableInternalObserver;
class ScriptState;
class Subscriber;
class SubscribeOptions;
class V8SubscribeCallback;
class V8UnionObserverOrObserverCallback;

// Implementation of the DOM `Observable` API. See
// https://github.com/WICG/observable and
// https://docs.google.com/document/d/1NEobxgiQO-fTSocxJBqcOOOVZRmXcTFg9Iqrhebb7bg/edit.
class CORE_EXPORT Observable final : public ScriptWrappable,
                                     public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This delegate is an internal (non-web-exposed) version of
  // `V8SubscribeCallback` for `Observer`s that are created natively by the web
  // platform. `OnSubscribe()` owns the passed-in `Subscriber` so that the
  // delegate, like an actual JS `V8SubscribeCallback`, can forward events to
  // the underlying observable subscriber.
  class SubscribeDelegate : public GarbageCollected<SubscribeDelegate> {
   public:
    virtual ~SubscribeDelegate() = default;
    virtual void OnSubscribe(Subscriber*, ScriptState*) = 0;
    virtual void Trace(Visitor* visitor) const {}
  };

  // Called by v8 bindings.
  static Observable* Create(ScriptState*, V8SubscribeCallback*);

  Observable(ExecutionContext*, V8SubscribeCallback*);
  Observable(ExecutionContext*, SubscribeDelegate*);

  // API methods:
  void subscribe(ScriptState*,
                 V8UnionObserverOrObserverCallback*,
                 SubscribeOptions*);

  // Promise-returning operators. See
  // https://wicg.github.io/observable/#promise-returning-operators.
  ScriptPromise toArray(ScriptState*, SubscribeOptions*);

  void Trace(Visitor*) const override;

 private:
  void SubscribeInternal(ScriptState*,
                         V8UnionObserverOrObserverCallback*,
                         ObservableInternalObserver*,
                         SubscribeOptions*);

  // Exactly one of `subscribe_callback_` and `subscribe_delegate_` must be
  // non-null. `subscribe_callback_` is non-null when `this` is created from
  // script, and the subscribe callback is a JS-provided callback function,
  // whereas `subscribe_delegate_` is used for `Observable`s created internally
  // in C++, where the subscription steps are native steps.
  //
  // `subscribe_callback_` gets called when the `subscribe` method is invoked.
  // When run, is run, errors are caught and "reported":
  // https://html.spec.whatwg.org/C#report-the-exception.
  const Member<V8SubscribeCallback> subscribe_callback_;
  const Member<SubscribeDelegate> subscribe_delegate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_OBSERVABLE_H_
