// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_RESPOND_WITH_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_RESPOND_WITH_OBSERVER_H_

#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ScriptValue;
class WaitUntilObserver;

// This is a base class to implement respondWith. The respondWith has the three
// types of results: fulfilled, rejected and not called. Derived classes for
// each event should implement the procedure of the three behaviors by
// overriding onResponseFulfilled, onResponseRejected and onNoResponse.
class MODULES_EXPORT RespondWithObserver
    : public GarbageCollected<RespondWithObserver>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(RespondWithObserver);

 public:
  virtual ~RespondWithObserver() = default;

  void WillDispatchEvent();
  void DidDispatchEvent(DispatchEventResult dispatch_result);

  // Observes the given promise and calls OnResponseRejected() or
  // OnResponseFulfilled() when it settles. It also keeps the event alive by
  // telling the event's WaitUntilObserver to observe the promise. The result of
  // RespondWith() is therefore reported back before the event finishes.
  void RespondWith(ScriptState*, ScriptPromise, ExceptionState&);

  // Called when the respondWith() promise was rejected.
  virtual void OnResponseRejected(mojom::ServiceWorkerResponseError) = 0;

  // Called when the respondWith() promise was fulfilled.
  virtual void OnResponseFulfilled(ScriptState*,
                                   const ScriptValue&,
                                   ExceptionState::ContextType,
                                   const char* interface_name,
                                   const char* property_name) = 0;

  // Called when the event handler finished without calling respondWith().
  virtual void OnNoResponse() = 0;

  void Trace(blink::Visitor*) override;

 protected:
  RespondWithObserver(ExecutionContext*, int event_id, WaitUntilObserver*);
  const int event_id_;
  base::TimeTicks event_dispatch_time_;

 private:
  class ThenFunction;

  void ResponseWasRejected(mojom::ServiceWorkerResponseError,
                           const ScriptValue&);
  void ResponseWasFulfilled(ScriptState* state,
                            ExceptionState::ContextType,
                            const char* interface_name,
                            const char* property_name,
                            const ScriptValue&);

  enum State { kInitial, kPending, kDone };
  State state_;

  // RespondWith should ensure the ExtendableEvent is alive until the promise
  // passed to RespondWith is resolved. The lifecycle of the ExtendableEvent
  // is controlled by WaitUntilObserver, so not only
  // WaitUntilObserver::ThenFunction but RespondWith needs to have a strong
  // reference to the WaitUntilObserver.
  Member<WaitUntilObserver> observer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERVICE_WORKER_RESPOND_WITH_OBSERVER_H_
