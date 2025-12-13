// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing.h"

#include <limits>

#include "cc/base/region.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
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
      performance_(DOMWindowPerformance::performance(window)) {}

bool ContainerTiming::CanReportToContainerTiming() const {
  DCHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kContainer) ||
         !performance_->IsContainerTimingBufferFull();
}

// static
Element* ContainerTiming::GetContainerRoot(Element* element) {
  DCHECK(element->SelfOrAncestorHasContainerTiming());
  if (element->FastHasAttribute(html_names::kContainertimingAttr)) {
    return element;
  }
  if (Element* parent = element->parentElement()) {
    return GetContainerRoot(parent);
  }
  return nullptr;
}

// static
Element* ContainerTiming::GetParentContainerRoot(Element* element) {
  Element* parent = element->parentElement();
  if (!parent || !parent->SelfOrAncestorHasContainerTiming()) {
    return nullptr;
  }
  return GetContainerRoot(parent);
}

ContainerTiming::Record::Record(const DOMPaintTimingInfo& paint_timing_info,
                                const AtomicString& identifier)
    : first_paint_timing_info_(paint_timing_info), identifier_(identifier) {}

void ContainerTiming::Record::MaybeUpdateLastNewPaintedArea(
    ContainerTiming* container_timing,
    const DOMPaintTimingInfo& paint_timing_info,
    Element* container_root,
    Element* element,
    const gfx::Rect& enclosing_rect) {
  if (painted_region_.Contains(enclosing_rect)) {
    return;
  }

  painted_region_.Union(enclosing_rect);

  last_new_painted_area_paint_timing_info_ = paint_timing_info;
  last_new_painted_area_element_ = element;

  has_pending_changes_ = true;

  // A container timing root with the ignore attribute will not report to
  // ancestor roots.
  if (container_root->FastGetAttribute(
          html_names::kContainertimingIgnoreAttr)) {
    return;
  }

  Element* parent_container_root = GetParentContainerRoot(container_root);
  if (!parent_container_root) {
    return;
  }

  Record* parent_record = container_timing->GetOrCreateRecord(
      paint_timing_info, parent_container_root);
  parent_record->MaybeUpdateLastNewPaintedArea(
      container_timing, paint_timing_info, parent_container_root, element,
      enclosing_rect);
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
  Record* record;
  auto it = container_root_records_.find(container_root);
  if (it == container_root_records_.end()) {
    record = MakeGarbageCollected<Record>(
        paint_timing_info,
        container_root->FastGetAttribute(html_names::kContainertimingAttr));
    container_root_records_.insert(container_root, record);
  } else {
    record = it->value;
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

  Element* container_root = GetContainerRoot(element);
  if (!container_root) {
    // Detached nodes should not report timing events
    return;
  }
  Record* record = GetOrCreateRecord(paint_timing_info, container_root);

  gfx::Rect enclosing_rect = gfx::ToEnclosingRect(intersection_rect);
  record->MaybeUpdateLastNewPaintedArea(this, paint_timing_info, container_root,
                                        element, enclosing_rect);

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
}

}  // namespace blink
