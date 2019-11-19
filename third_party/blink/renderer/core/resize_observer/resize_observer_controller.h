// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_RESIZE_OBSERVER_RESIZE_OBSERVER_CONTROLLER_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ResizeObserver;

// ResizeObserverController keeps track of all ResizeObservers
// in a single Document.
//
// The observation API is used to integrate ResizeObserver
// and the event loop. It delivers notification in a loop.
// In each iteration, only notifications deeper than the
// shallowest notification from previous iteration are delivered.
class ResizeObserverController final
    : public GarbageCollected<ResizeObserverController> {
 public:
  static const size_t kDepthBottom = 4096;

  ResizeObserverController();

  void AddObserver(ResizeObserver&);

  // observation API
  // Returns depth of shallowest observed node, kDepthLimit if none.
  size_t GatherObservations(size_t deeper_than);
  // Returns true if gatherObservations has skipped observations
  // because they were too shallow.
  bool SkippedObservations();
  void DeliverObservations();
  void ClearObservations();
  void ObserverChanged() { observers_changed_ = true; }

  void SetNeedsForcedResizeObservations();

  void Trace(blink::Visitor*);

  // For testing only.
  const HeapHashSet<WeakMember<ResizeObserver>>& Observers() {
    return observers_;
  }

 private:
  // Active observers
  HeapHashSet<WeakMember<ResizeObserver>> observers_;
  // True if any observers were changed since last notification.
  bool observers_changed_;
};

}  // namespace blink

#endif
