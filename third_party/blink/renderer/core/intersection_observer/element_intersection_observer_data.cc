// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"

#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"

namespace blink {

ElementIntersectionObserverData::ElementIntersectionObserverData() = default;

IntersectionObservation* ElementIntersectionObserverData::GetObservationFor(
    IntersectionObserver& observer) {
  auto i = observations_.find(&observer);
  if (i == observations_.end())
    return nullptr;
  return i->value.Get();
}

void ElementIntersectionObserverData::AddObservation(
    IntersectionObservation& observation) {
  DCHECK(observation.Observer());
  observations_.insert(observation.Observer(), &observation);
}

void ElementIntersectionObserverData::AddObserver(
    IntersectionObserver& observer) {
  observers_.insert(&observer);
}

void ElementIntersectionObserverData::RemoveObservation(
    IntersectionObservation& observation) {
  observations_.erase(observation.Observer());
}

void ElementIntersectionObserverData::RemoveObserver(
    IntersectionObserver& observer) {
  observers_.erase(&observer);
}

void ElementIntersectionObserverData::TrackWithController(
    IntersectionObserverController& controller) {
  for (auto& entry : observations_)
    controller.AddTrackedObservation(*entry.value);
  for (auto& observer : observers_)
    controller.AddTrackedObserver(*observer);
}

void ElementIntersectionObserverData::StopTrackingWithController(
    IntersectionObserverController& controller) {
  for (auto& entry : observations_)
    controller.RemoveTrackedObservation(*entry.value);
  for (auto& observer : observers_)
    controller.RemoveTrackedObserver(*observer);
}

void ElementIntersectionObserverData::ComputeIntersectionsForTarget() {
  ComputeIntersectionsContext context;
  for (auto& [observer, observation] : observations_) {
    observation->ComputeIntersectionImmediately(context);
  }
}

bool ElementIntersectionObserverData::NeedsOcclusionTracking() const {
  for (auto& entry : observations_) {
    if (entry.key->trackVisibility())
      return true;
  }
  return false;
}

void ElementIntersectionObserverData::InvalidateCachedRects() {
  if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    for (auto& observer : observers_) {
      observer->InvalidateCachedRects();
    }
  }
  for (auto& entry : observations_) {
    entry.value->InvalidateCachedRects();
  }
}

void ElementIntersectionObserverData::Trace(Visitor* visitor) const {
  visitor->Trace(observations_);
  visitor->Trace(observers_);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
