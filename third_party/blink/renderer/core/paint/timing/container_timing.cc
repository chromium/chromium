// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing.h"

#include <limits>

#include "cc/base/region.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

uint64_t GetRegionSize(const cc::Region& region) {
  uint64_t size = 0;
  for (gfx::Rect rect : region) {
    size += rect.size().Area64();
  }
  return size;
}

// static
Element* ContainerRootFallback(Element* element) {
  // Check if the element itself is a container root (e.g., <img
  // containertiming> where the painted element and the root are the same
  // element).
  if (element->FastHasAttribute(html_names::kContainertimingAttr)) {
    return element;
  }
  // Walk up the DOM looking for a container root. Stops at
  // containertimingignore (unless that element also has containertiming).
  while ((element = element->parentElement())) {
    if (element->FastHasAttribute(html_names::kContainertimingAttr)) {
      return element;
    }
    if (element->HasContainerTimingIgnoreAttribute()) {
      return nullptr;
    }
  }
  return nullptr;
}

// static
Element* ParentContainerRootFallback(Element* element) {
  auto* parent = element->parentElement();
  if (!parent || !parent->SelfOrAncestorHasContainerTiming()) {
    return nullptr;
  }

  return ContainerRootFallback(parent);
}

}  // namespace

// static
ContainerTiming& ContainerTiming::From(LocalDOMWindow& window) {
  ContainerTiming* timing =
      Supplement<LocalDOMWindow>::From<ContainerTiming>(window);
  if (!timing) {
    timing = MakeGarbageCollected<ContainerTiming>(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

ContainerTiming::ContainerTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      performance_(DOMWindowPerformance::performance(window)) {
  if (RuntimeEnabledFeatures::ContainerTimingPrepaintTraversalEnabled(
          &window)) {
    paint_attribution_tracker_ =
        MakeGarbageCollected<ContainerTimingPaintAttributionTracker>();
  }
}

bool ContainerTiming::CanReportToContainerTiming() const {
  DCHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kContainer) ||
         !performance_->IsContainerTimingBufferFull();
}

ContainerTiming::Record::Record(const DOMPaintTimingInfo& paint_timing_info,
                                const AtomicString& identifier)
    : first_paint_timing_info_(paint_timing_info), identifier_(identifier) {}

void ContainerTiming::Record::MaybeUpdateLastNewPaintedArea(
    const DOMPaintTimingInfo& paint_timing_info,
    Element* element,
    const gfx::Rect& enclosing_rect) {
  if (painted_region_.Contains(enclosing_rect)) {
    return;
  }

  painted_region_.Union(enclosing_rect);

  last_new_painted_area_paint_timing_info_ = paint_timing_info;
  last_new_painted_area_element_ = element;

  has_pending_changes_ = true;
}

void ContainerTiming::Record::MaybeEmitPerformanceEntry(
    WindowPerformance* performance,
    Element* container_root) {
  if (!has_pending_changes_) {
    return;
  }
  performance->AddContainerTiming(
      last_new_painted_area_paint_timing_info_, painted_region_.bounds(),
      GetRegionSize(painted_region_), container_root, identifier_,
      last_new_painted_area_element_, first_paint_timing_info_);
  has_pending_changes_ = false;
}

void ContainerTiming::Record::Trace(Visitor* visitor) const {
  visitor->Trace(last_new_painted_area_element_);
}

ContainerTiming::Record* ContainerTiming::GetOrCreateRecord(
    const DOMPaintTimingInfo& paint_timing_info,
    Element* container_root) {
  Record* record = nullptr;
  auto it = container_root_records_.find(container_root);

  if (it != container_root_records_.end()) {
    record = it->value;
  } else {
    record = MakeGarbageCollected<Record>(
        paint_timing_info,
        container_root->FastGetAttribute(html_names::kContainertimingAttr));
    container_root_records_.insert(container_root, record);
  }

  return record;
}

void ContainerTiming::MaybeUpdateContainerRootIdentifier(
    Element* element,
    const AtomicString& new_value) {
  auto it = container_root_records_.find(element);
  if (it != container_root_records_.end()) {
    Record* record = it->value;

    if (new_value.IsNull() || record->identifier() != new_value) {
      // If containertiming is unset, drop record.
      // Also, once the identifier changes, the old values should not be used
      // for the new events.
      container_root_records_.erase(it);
    }
  }
}

void ContainerTiming::OnElementPainted(
    const DOMPaintTimingInfo& paint_timing_info,
    Element* element,
    const gfx::RectF& intersection_rect) {
  if (!ContributesToContainerTiming(element)) {
    return;
  }

  // This runs at presentation time, not during paint, so we deliberately don't
  // assert that ContainerTimingChanged() is clean: an attribute change between
  // paint and now marks the layout object for re-attribution on a future
  // pre-paint, which doesn't affect this callback. We report the paint that
  // already happened using the attribution captured at the last pre-paint. The
  // fallback is only for the feature-off case (no tracker).
  Element* container_root =
      paint_attribution_tracker_
          ? paint_attribution_tracker_->GetContainerRootFor(element)
          : ContainerRootFallback(element);
  if (!container_root) {
    // This runs at presentation time, so the element may have been detached
    // since paint, leaving no reachable container root. Don't report it.
    // TODO(crbug.com/535107494): attributing at paint time would let this
    // become a CHECK.
    return;
  }

  const gfx::Rect enclosing_rect = gfx::ToEnclosingRect(intersection_rect);
  // The attribute re-check is not redundant: this runs at presentation time, so
  // the tracker (or fallback chain) can be stale relative to the live DOM if
  // the containertiming attribute was removed since the last pre-paint. Skip
  // roots that are no longer roots. See StaleTrackerGuard_* tests.
  while (container_root &&
         container_root->FastHasAttribute(html_names::kContainertimingAttr)) {
    Record* record = GetOrCreateRecord(paint_timing_info, container_root);
    record->MaybeUpdateLastNewPaintedArea(paint_timing_info, element,
                                          enclosing_rect);
    // A container root with containertimingignore does not propagate to
    // ancestor roots.
    if (container_root->HasContainerTimingIgnoreAttribute()) {
      break;
    }
    container_root =
        paint_attribution_tracker_
            ? paint_attribution_tracker_->GetParentContainerRootFor(
                  container_root)
            : ParentContainerRootFallback(container_root);
  }
  performance_->SetHasContainerTimingChanges();
}

void ContainerTiming::EmitPerformanceEntries() {
  const bool can_report = CanReportToContainerTiming();
  for (const auto& pair : container_root_records_) {
    Record* record = pair.value;

    if (can_report) {
      record->MaybeEmitPerformanceEntry(performance_.Get(), pair.key.Get());
    }
  }
}

void ContainerTiming::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(performance_);
  visitor->Trace(container_root_records_);
  visitor->Trace(paint_attribution_tracker_);
}

}  // namespace blink
