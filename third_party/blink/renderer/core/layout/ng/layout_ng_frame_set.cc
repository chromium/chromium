// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_frame_set.h"

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/platform/cursors.h"
#include "ui/base/cursor/cursor.h"

namespace blink {

LayoutNGFrameSet::LayoutNGFrameSet(Element* element) : LayoutNGBlock(element) {
  DCHECK(IsA<HTMLFrameSetElement>(element));
}

const char* LayoutNGFrameSet::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGFrameSet";
}

bool LayoutNGFrameSet::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGFrameSet || LayoutNGBlock::IsOfType(type);
}

bool LayoutNGFrameSet::IsChildAllowed(LayoutObject* child,
                                      const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsFrame() || child->IsLayoutNGFrameSet();
}

void LayoutNGFrameSet::AddChild(LayoutObject* new_child,
                                LayoutObject* before_child) {
  LayoutNGBlock::AddChild(new_child, before_child);
  To<HTMLFrameSetElement>(GetNode())->DirtyEdgeInfoAndFullPaintInvalidation();
}

void LayoutNGFrameSet::RemoveChild(LayoutObject* child) {
  LayoutNGBlock::RemoveChild(child);
  if (DocumentBeingDestroyed())
    return;
  To<HTMLFrameSetElement>(GetNode())->DirtyEdgeInfoAndFullPaintInvalidation();
}

void LayoutNGFrameSet::UpdateBlockLayout(bool relayout_children) {
  if (IsOutOfFlowPositioned())
    UpdateOutOfFlowBlockLayout();
  else
    UpdateInFlowBlockLayout();
}

CursorDirective LayoutNGFrameSet::GetCursor(const PhysicalOffset& point,
                                            ui::Cursor& cursor) const {
  NOT_DESTROYED();
  const auto& frame_set = *To<HTMLFrameSetElement>(GetNode());
  gfx::Point rounded_point = ToRoundedPoint(point);
  if (frame_set.CanResizeRow(rounded_point)) {
    cursor = RowResizeCursor();
    return kSetCursor;
  }
  if (frame_set.CanResizeColumn(rounded_point)) {
    cursor = ColumnResizeCursor();
    return kSetCursor;
  }
  return LayoutBox::GetCursor(point, cursor);
}

}  // namespace blink
