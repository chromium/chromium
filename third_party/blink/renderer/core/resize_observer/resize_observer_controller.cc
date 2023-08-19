// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"

namespace blink {

const char ResizeObserverController::kSupplementName[] =
    "ResizeObserverController";

ResizeObserverController* ResizeObserverController::From(
    LocalDOMWindow& window) {
  auto* controller = FromIfExists(window);
  if (!controller) {
    controller = MakeGarbageCollected<ResizeObserverController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return controller;
}

ResizeObserverController* ResizeObserverController::FromIfExists(
    LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<ResizeObserverController>(window);
}

ResizeObserverController::ResizeObserverController(LocalDOMWindow& window)
    : Supplement(window) {}

void ResizeObserverController::AddObserver(ResizeObserver& observer) {
  switch (observer.Delivery()) {
    case ResizeObserver::DeliveryTime::kInsertionOrder:
      observers_.insert(&observer);
      break;
    case ResizeObserver::DeliveryTime::kBeforeOthers:
      observers_.PrependOrMoveToFirst(&observer);
      break;
  }
}

size_t ResizeObserverController::GatherObservations() {
  size_t shallowest = ResizeObserverController::kDepthBottom;

  for (auto& observer : observers_) {
    size_t depth = observer->GatherObservations(min_depth_);
    if (depth < shallowest)
      shallowest = depth;
  }
  min_depth_ = shallowest;
  return min_depth_;
}

bool ResizeObserverController::SkippedObservations() {
  for (auto& observer : observers_) {
    if (observer->SkippedObservations())
      return true;
  }
  return false;
}

void ResizeObserverController::DeliverObservations() {
  // Copy is needed because m_observers might get modified during
  // deliverObservations.
  HeapVector<Member<ResizeObserver>> observers(observers_);

  for (auto& observer : observers) {
    if (observer) {
      observer->DeliverObservations();
    }
  }
}

void ResizeObserverController::ClearObservations() {
  for (auto& observer : observers_)
    observer->ClearObservations();
}

void ResizeObserverController::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(observers_);
}

}  // namespace blink
