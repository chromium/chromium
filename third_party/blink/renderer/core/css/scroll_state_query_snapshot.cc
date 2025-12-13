// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/scroll_state_query_snapshot.h"

#include "third_party/blink/renderer/core/css/container_state.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ScrollStateQuerySnapshot::ScrollStateQuerySnapshot(Element& container)
    : PostLayoutSnapshotClient(container.GetDocument().GetFrame()),
      container_(container) {}

bool ScrollStateQuerySnapshot::UpdateSnapshot() {
  ContainerStuckPhysical stuck_horizontal = ContainerStuckPhysical::kNo;
  ContainerStuckPhysical stuck_vertical = ContainerStuckPhysical::kNo;
  ContainerScrollableFlags scrollable_horizontal =
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone);
  ContainerScrollableFlags scrollable_vertical =
      static_cast<ContainerScrollableFlags>(ContainerScrollable::kNone);
  ContainerScrolled scrolled_horizontal = scrolled_horizontal_;
  ContainerScrolled scrolled_vertical = scrolled_vertical_;

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
    if (layout_object->IsDocumentElement()) {
      layout_object = layout_object->View();
    }
    if (PaintLayerScrollableArea* scrollable_area =
            layout_object->GetScrollableArea()) {
      ScrollOffset max_offset = scrollable_area->MaximumScrollOffset();
      ScrollOffset min_offset = scrollable_area->MinimumScrollOffset();
      ScrollOffset offset = scrollable_area->GetScrollOffset();
      if (offset.x() > min_offset.x()) {
        scrollable_horizontal |=
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart);
      }
      if (offset.x() < max_offset.x()) {
        scrollable_horizontal |=
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd);
      }
      if (offset.y() > min_offset.y()) {
        scrollable_vertical |=
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kStart);
      }
      if (offset.y() < max_offset.y()) {
        scrollable_vertical |=
            static_cast<ContainerScrollableFlags>(ContainerScrollable::kEnd);
      }
      if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) {
        scrolled_vertical = scrollable_area->LastScrolledVertical();
        scrolled_horizontal = scrollable_area->LastScrolledHorizontal();
      }
    }
  }
  std::swap(stuck_horizontal_, stuck_horizontal);
  std::swap(stuck_vertical_, stuck_vertical);
  std::swap(scrollable_horizontal_, scrollable_horizontal);
  std::swap(scrollable_vertical_, scrollable_vertical);

  if (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled()) {
    std::swap(scrolled_horizontal_, scrolled_horizontal);
    std::swap(scrolled_vertical_, scrolled_vertical);
  }

  if (stuck_horizontal_ != stuck_horizontal ||
      stuck_vertical_ != stuck_vertical ||
      scrollable_horizontal_ != scrollable_horizontal ||
      scrollable_vertical_ != scrollable_vertical ||
      (RuntimeEnabledFeatures::CSSScrolledContainerQueriesEnabled() &&
       (scrolled_horizontal_ != scrolled_horizontal ||
        scrolled_vertical_ != scrolled_vertical))) {
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

bool ScrollStateQuerySnapshot::ShouldScheduleNextService() {
  return false;
}

void ScrollStateQuerySnapshot::Trace(Visitor* visitor) const {
  visitor->Trace(container_);
  PostLayoutSnapshotClient::Trace(visitor);
}

}  // namespace blink
