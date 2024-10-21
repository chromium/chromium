// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/scroll_state_query_snapshot.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

ScrollStateQuerySnapshot::ScrollStateQuerySnapshot(Element& container)
    : ScrollSnapshotClient(container.GetDocument().GetFrame()),
      container_(container) {}

bool ScrollStateQuerySnapshot::UpdateScrollState() {
  ContainerStuckPhysical stuck_horizontal = ContainerStuckPhysical::kNo;
  ContainerStuckPhysical stuck_vertical = ContainerStuckPhysical::kNo;
  ContainerOverflowingFlags overflowing_horizontal =
      static_cast<ContainerOverflowingFlags>(ContainerOverflowing::kNone);
  ContainerOverflowingFlags overflowing_vertical =
      static_cast<ContainerOverflowingFlags>(ContainerOverflowing::kNone);

  LayoutBoxModelObject* layout_object =
      DynamicTo<LayoutBoxModelObject>(container_->GetLayoutObject());
  if (layout_object) {
    if (layout_object->IsStickyPositioned()) {
      PhysicalOffset sticky_offset = layout_object->StickyPositionOffset();
      if (sticky_offset.left > 0) {
        stuck_horizontal = ContainerStuckPhysical::kLeft;
      } else if (sticky_offset.left < 0) {
        stuck_horizontal = ContainerStuckPhysical::kRight;
      }
      if (sticky_offset.top > 0) {
        stuck_vertical = ContainerStuckPhysical::kTop;
      } else if (sticky_offset.top < 0) {
        stuck_vertical = ContainerStuckPhysical::kBottom;
      }
    }
    if (PaintLayerScrollableArea* scrollable_area =
            layout_object->GetScrollableArea()) {
      ScrollOffset max_offset = scrollable_area->MaximumScrollOffset();
      ScrollOffset min_offset = scrollable_area->MinimumScrollOffset();
      ScrollOffset offset = scrollable_area->GetScrollOffset();
      if (offset.x() > min_offset.x()) {
        overflowing_horizontal |= static_cast<ContainerOverflowingFlags>(
            ContainerOverflowing::kStart);
      }
      if (offset.x() < max_offset.x()) {
        overflowing_horizontal |=
            static_cast<ContainerOverflowingFlags>(ContainerOverflowing::kEnd);
      }
      if (offset.y() > min_offset.y()) {
        overflowing_vertical |= static_cast<ContainerOverflowingFlags>(
            ContainerOverflowing::kStart);
      }
      if (offset.y() < max_offset.y()) {
        overflowing_vertical |=
            static_cast<ContainerOverflowingFlags>(ContainerOverflowing::kEnd);
      }
    }
  }
  std::swap(stuck_horizontal_, stuck_horizontal);
  std::swap(stuck_vertical_, stuck_vertical);
  std::swap(overflowing_horizontal_, overflowing_horizontal);
  std::swap(overflowing_vertical_, overflowing_vertical);

  if (stuck_horizontal_ != stuck_horizontal ||
      stuck_vertical_ != stuck_vertical ||
      overflowing_horizontal_ != overflowing_horizontal ||
      overflowing_vertical_ != overflowing_vertical) {
    // TODO(crbug.com/40268059): The kLocalStyleChange is not necessary for the
    // container itself, but it is a way to reach reach ApplyScrollState() in
    // Element::RecalcOwnStyle() for the next lifecycle update.
    container_->SetNeedsStyleRecalc(kLocalStyleChange,
                                    StyleChangeReasonForTracing::Create(
                                        style_change_reason::kScrollTimeline));
    return true;
  }
  return false;
}

void ScrollStateQuerySnapshot::UpdateSnapshot() {
  UpdateScrollState();
}

bool ScrollStateQuerySnapshot::ValidateSnapshot() {
  if (UpdateScrollState()) {
    return false;
  }
  return true;
}

bool ScrollStateQuerySnapshot::ShouldScheduleNextService() {
  return false;
}

void ScrollStateQuerySnapshot::Trace(Visitor* visitor) const {
  visitor->Trace(container_);
  ScrollSnapshotClient::Trace(visitor);
}

}  // namespace blink
