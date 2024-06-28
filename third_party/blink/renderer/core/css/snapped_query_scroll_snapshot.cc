// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/snapped_query_scroll_snapshot.h"

#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

SnappedQueryScrollSnapshot::SnappedQueryScrollSnapshot(
    PaintLayerScrollableArea& scroller)
    : ScrollSnapshotClient(scroller.GetLayoutBox()->GetDocument().GetFrame()),
      scroller_(&scroller) {}

void SnappedQueryScrollSnapshot::InvalidateSnappedTarget(Element* target) {
  if (target) {
    if (ContainerQueryEvaluator* evaluator =
            target->GetContainerQueryEvaluator()) {
      evaluator->SetPendingSnappedStateFromScrollSnapshot(*this);
    }
  }
}

bool SnappedQueryScrollSnapshot::UpdateSnappedTargets() {
  bool did_change = false;

  Element* snapped_target_x =
      scroller_->GetSnappedQueryTargetAlongAxis(cc::SnapAxis::kX);
  Element* snapped_target_y =
      scroller_->GetSnappedQueryTargetAlongAxis(cc::SnapAxis::kY);

  if (snapped_target_x_ != snapped_target_x) {
    Element* snapped_target_x_old = snapped_target_x_;
    snapped_target_x_ = snapped_target_x;
    InvalidateSnappedTarget(snapped_target_x_old);
    InvalidateSnappedTarget(snapped_target_x);
    did_change = true;
  }
  if (snapped_target_y_ != snapped_target_y) {
    Element* snapped_target_y_old = snapped_target_y_;
    snapped_target_y_ = snapped_target_y;
    InvalidateSnappedTarget(snapped_target_y_old);
    InvalidateSnappedTarget(snapped_target_y);
    did_change = true;
  }
  return did_change;
}

void SnappedQueryScrollSnapshot::UpdateSnapshot() {
  UpdateSnappedTargets();
}

bool SnappedQueryScrollSnapshot::ValidateSnapshot() {
  if (UpdateSnappedTargets()) {
    return false;
  }
  return true;
}

bool SnappedQueryScrollSnapshot::ShouldScheduleNextService() {
  return false;
}

void SnappedQueryScrollSnapshot::Trace(Visitor* visitor) const {
  visitor->Trace(scroller_);
  visitor->Trace(snapped_target_x_);
  visitor->Trace(snapped_target_y_);
  ScrollSnapshotClient::Trace(visitor);
}

}  // namespace blink
