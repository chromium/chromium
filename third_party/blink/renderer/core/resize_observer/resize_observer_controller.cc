// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"

#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

namespace blink {

ResizeObserverController::ResizeObserverController()
    : observers_changed_(false) {}

void ResizeObserverController::AddObserver(ResizeObserver& observer) {
  observers_.insert(&observer);
}

size_t ResizeObserverController::GatherObservations(size_t deeper_than) {
  size_t shallowest = ResizeObserverController::kDepthBottom;
  if (!observers_changed_)
    return shallowest;
  for (auto& observer : observers_) {
    size_t depth = observer->GatherObservations(deeper_than);
    if (depth < shallowest)
      shallowest = depth;
  }
  return shallowest;
}

void ResizeObserverController::SetNeedsForcedResizeObservations() {
  for (auto& observer : observers_) {
    // Set ElementSizeChanged as a way of forcing the observer to check all
    // observations.
    observer->ElementSizeChanged();
  }
}

bool ResizeObserverController::SkippedObservations() {
  for (auto& observer : observers_) {
    if (observer->SkippedObservations())
      return true;
  }
  return false;
}

void ResizeObserverController::DeliverObservations() {
  observers_changed_ = false;
  // Copy is needed because m_observers might get modified during
  // deliverObservations.
  HeapVector<Member<ResizeObserver>> observers;
  CopyToVector(observers_, observers);

  for (auto& observer : observers) {
    if (observer) {
      observer->DeliverObservations();
      observers_changed_ =
          observers_changed_ || observer->HasElementSizeChanged();
    }
  }
}

void ResizeObserverController::ClearObservations() {
  for (auto& observer : observers_)
    observer->ClearObservations();
}

void ResizeObserverController::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
}

}  // namespace blink
