// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/stuck_query_scroll_snapshot.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"

namespace blink {

StuckQueryScrollSnapshot::StuckQueryScrollSnapshot(Element& container)
    : ScrollSnapshotClient(container.GetDocument().GetFrame()),
      container_(container) {}

bool StuckQueryScrollSnapshot::UpdateStuckState() {
  ContainerStuckPhysical stuck_horizontal = ContainerStuckPhysical::kNo;
  ContainerStuckPhysical stuck_vertical = ContainerStuckPhysical::kNo;
  LayoutBoxModelObject* layout_object =
      DynamicTo<LayoutBoxModelObject>(container_->GetLayoutObject());
  if (layout_object && layout_object->IsStickyPositioned()) {
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
  std::swap(stuck_horizontal_, stuck_horizontal);
  std::swap(stuck_vertical_, stuck_vertical);

  if (stuck_horizontal_ != stuck_horizontal ||
      stuck_vertical_ != stuck_vertical) {
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

void StuckQueryScrollSnapshot::UpdateSnapshot() {
  UpdateStuckState();
}

bool StuckQueryScrollSnapshot::ValidateSnapshot() {
  if (UpdateStuckState()) {
    return false;
  }
  return true;
}

bool StuckQueryScrollSnapshot::ShouldScheduleNextService() {
  return false;
}

void StuckQueryScrollSnapshot::Trace(Visitor* visitor) const {
  visitor->Trace(container_);
  ScrollSnapshotClient::Trace(visitor);
}

}  // namespace blink
