// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/container_timing.h"

#include <limits>
#include <utility>

#include "cc/base/region.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing_paint_attribution_tracker.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_container_timing.h"
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
      performance_(DOMWindowPerformance::performance(window)),
      paint_attribution_tracker_(
          MakeGarbageCollected<ContainerTimingPaintAttributionTracker>()) {
  CHECK(RuntimeEnabledFeatures::ContainerTimingEnabled(&window));
}

// static
bool ContainerTiming::ContributesToContainerTiming(Element* element) {
  if (!element || element->IsInShadowTree()) {
    return false;
  }
  // Check the feature before From(), which would create the supplement: this
  // runs for every text record considered for timing.
  LocalDOMWindow* window = element->GetDocument().domWindow();
  if (!window || !RuntimeEnabledFeatures::ContainerTimingEnabled(window)) {
    return false;
  }
  // The paint attribution tracker, populated during the pre-paint walk (which
  // runs before paint), keys images by their generating node and text by its
  // aggregation node, which is exactly `element` either way.
  ContainerTimingPaintAttributionTracker* tracker =
      From(*window).PaintAttributionTracker();
  return tracker->GetContainerRootFor(element) != nullptr;
}

ContainerTiming::Record::Record(Element* container_root,
                                const AtomicString& identifier)
    : container_root_(container_root), identifier_(identifier) {}

bool ContainerTiming::Record::MaybeUpdateLastNewPaintedArea(
    Element* element,
    const gfx::Rect& enclosing_rect) {
  // Nominate the element by its own clipped area, the way LCP selects its
  // candidates, rather than by the area it newly contributed to the container.
  // Selecting by the contributed area would hand the overlap between two
  // elements to whichever happened to paint first, both within a frame and
  // across loads of the same page. Ties keep the earlier element.
  //
  // This runs before the containment check below: content painting entirely
  // inside the region painted so far is still a contentful paint, it just does
  // not grow the container. Nothing re-nominates on a repaint, because each
  // element reaches container timing only once per content it paints.
  const uint64_t own_painted_area = enclosing_rect.size().Area64();
  if (own_painted_area > largest_painted_area_) {
    largest_painted_area_ = own_painted_area;
    largest_painted_area_element_ = element;
  }

  if (painted_region_.Contains(enclosing_rect)) {
    return false;
  }

  painted_region_.Union(enclosing_rect);
  return !std::exchange(has_pending_changes_, true);
}

PerformanceContainerTiming* ContainerTiming::Record::CreatePerformanceEntry(
    DOMWindow* source) {
  CHECK(has_pending_changes_);
  auto* entry = PerformanceContainerTiming::Create(
      painted_region_.bounds(), GetRegionSize(painted_region_), container_root_,
      identifier_, largest_painted_area_element_, source);
  has_pending_changes_ = false;
  // Start the comparison over, so the next entry reports the largest element
  // painted since this one.
  largest_painted_area_ = 0;
  largest_painted_area_element_ = nullptr;
  return entry;
}

void ContainerTiming::Record::SetFirstRenderTimeIfUnset(
    DOMHighResTimeStamp presentation_time) {
  if (first_render_time_ == 0.0) {
    first_render_time_ = presentation_time;
  }
}

void ContainerTiming::Record::Trace(Visitor* visitor) const {
  visitor->Trace(container_root_);
  visitor->Trace(largest_painted_area_element_);
}

ContainerTiming::Record* ContainerTiming::GetOrCreateRecord(
    Element* container_root) {
  Record* record = nullptr;
  auto it = container_root_records_.find(container_root);

  if (it != container_root_records_.end()) {
    record = it->value;
  } else {
    record = MakeGarbageCollected<Record>(
        container_root,
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

void ContainerTiming::OnContainerRootDisconnected(Element* element) {
  container_root_records_.erase(element);
}

void ContainerTiming::OnElementPainted(Element& element,
                                       const gfx::RectF& intersection_rect) {
  // The tracker only maps elements that are actually attributed to a container
  // root. A null result means this paint (e.g. a text record that only exists
  // for element timing) is not under any container root and must be ignored.
  //
  // TODO(crbug.com/559075199): decoupling ContainerTiming from ElementTiming
  // would let this become a CHECK.
  Element* container_root =
      paint_attribution_tracker_->GetContainerRootFor(&element);
  if (!container_root) {
    return;
  }

  const gfx::Rect enclosing_rect = gfx::ToEnclosingRect(intersection_rect);
  // Skip roots that are no longer roots, in case the containertiming attribute
  // was removed, or the root disconnected, since the last pre-paint walk
  // updated the tracker. See StaleTrackerGuard_* tests.
  while (container_root && container_root->isConnected() &&
         container_root->FastHasAttribute(html_names::kContainertimingAttr)) {
    Record* record = GetOrCreateRecord(container_root);
    if (record->MaybeUpdateLastNewPaintedArea(&element, enclosing_rect)) {
      dirty_records_.push_back(record);
    }
    // A container root with containertimingignore does not propagate to
    // ancestor roots.
    if (container_root->FastHasAttribute(
            html_names::kContainertimingignoreAttr)) {
      break;
    }
    container_root =
        paint_attribution_tracker_->GetParentContainerRootFor(container_root);
  }
}

OptionalPaintTimingCallback ContainerTiming::TakePaintTimingCallback() {
  if (dirty_records_.empty()) {
    return std::nullopt;
  }
  auto* entries = MakeGarbageCollected<GCedHeapVector<PendingEntry>>();
  for (Record* record : dirty_records_) {
    entries->emplace_back(record,
                          record->CreatePerformanceEntry(GetSupplementable()));
  }
  dirty_records_.clear();
  return blink::BindOnce(&ContainerTiming::FlushTimingsOnFramePresented,
                         WrapWeakPersistent(this), WrapPersistent(entries));
}

void ContainerTiming::FlushTimingsOnFramePresented(
    GCedHeapVector<PendingEntry>* entries,
    const base::TimeTicks&,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  for (const auto& [record, entry] : *entries) {
    record->SetFirstRenderTimeIfUnset(paint_timing_info.presentation_time);
    entry->FinalizeEntry(paint_timing_info, navigation_id,
                         record->FirstRenderTime());
    performance_->AddContainerTiming(entry);
  }
}

void ContainerTiming::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(performance_);
  visitor->Trace(container_root_records_);
  visitor->Trace(dirty_records_);
  visitor->Trace(paint_attribution_tracker_);
}

}  // namespace blink
