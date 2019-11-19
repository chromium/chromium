// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INTERSECTION_OBSERVER_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INTERSECTION_OBSERVER_DELEGATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"

namespace blink {

class V8IntersectionObserverCallback;

class V8IntersectionObserverDelegate final
    : public IntersectionObserverDelegate,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(V8IntersectionObserverDelegate);

 public:
  CORE_EXPORT V8IntersectionObserverDelegate(V8IntersectionObserverCallback*,
                                             ScriptState*);
  ~V8IntersectionObserverDelegate() override;

  ExecutionContext* GetExecutionContext() const override;

  void Trace(blink::Visitor*) override;

  IntersectionObserver::DeliveryBehavior GetDeliveryBehavior() const override {
    return IntersectionObserver::kPostTaskToDeliver;
  }

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>&,
               IntersectionObserver&) override;

 private:
  Member<V8IntersectionObserverCallback> callback_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_INTERSECTION_OBSERVER_DELEGATE_H_
