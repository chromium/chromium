// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_H_

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class Element;
class ResizeObserverController;
class ResizeObserverEntry;
class ResizeObservation;
class V8ResizeObserverCallback;

// ResizeObserver represents ResizeObserver javascript api:
// https://github.com/WICG/ResizeObserver/
class CORE_EXPORT ResizeObserver final
    : public ScriptWrappable,
      public ActiveScriptWrappable<ResizeObserver>,
      public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(ResizeObserver);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This delegate is an internal (non-web-exposed) version of ResizeCallback.
  class Delegate : public GarbageCollected<Delegate> {
   public:
    virtual ~Delegate() = default;
    virtual void OnResize(
        const HeapVector<Member<ResizeObserverEntry>>& entries) = 0;
    virtual void Trace(blink::Visitor* visitor) {}
  };

  static ResizeObserver* Create(Document&, V8ResizeObserverCallback*);
  static ResizeObserver* Create(Document&, Delegate*);

  ResizeObserver(V8ResizeObserverCallback*, Document&);
  ResizeObserver(Delegate*, Document&);
  ~ResizeObserver() override = default;

  // API methods
  void observe(Element*);
  void unobserve(Element*);
  void disconnect();

  // Returns depth of shallowest observed node, kDepthLimit if none.
  size_t GatherObservations(size_t deeper_than);
  bool SkippedObservations() { return skipped_observations_; }
  void DeliverObservations();
  void ClearObservations();
  void ElementSizeChanged();
  bool HasElementSizeChanged() { return element_size_changed_; }

  // ScriptWrappable override:
  bool HasPendingActivity() const override;

  void Trace(blink::Visitor*) override;

 private:
  using ObservationList = HeapLinkedHashSet<WeakMember<ResizeObservation>>;

  // Either of |callback_| and |delegate_| should be non-null.
  const Member<V8ResizeObserverCallback> callback_;
  const Member<Delegate> delegate_;

  // List of Elements we are observing. These Elements make the ResizeObserver
  // and most-importantly |callback_| alive. If |observations_| is empty, no one
  // is performing wrapper-tracing and |callback_| might already be gone.
  ObservationList observations_;
  // List of elements that have changes
  HeapVector<Member<ResizeObservation>> active_observations_;
  // True if observations were skipped gatherObservations
  bool skipped_observations_;
  // True if any ResizeObservation reported size change
  bool element_size_changed_;
  WeakMember<ResizeObserverController> controller_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_H_
