// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"

namespace blink {

Node* LineLayoutItem::GetNodeForOwnerNodeId() const {
  auto* layout_text_fragment =
      DynamicTo<LayoutTextFragment>(layout_object_.Get());
  if (layout_text_fragment)
    return layout_text_fragment->AssociatedTextNode();
  return layout_object_->GetNode();
}

const ComputedStyle* LineLayoutItem::Style(bool first_line) const {
  return layout_object_->Style(first_line);
}

const ComputedStyle& LineLayoutItem::StyleRef(bool first_line) const {
  return layout_object_->StyleRef(first_line);
}

bool LineLayoutItem::IsEmptyText() const {
  return IsText() && To<LayoutText>(layout_object_.Get())->GetText().empty();
}

int LineLayoutItem::CaretMaxOffset() const {
  if (layout_object_->IsAtomicInlineLevel()) {
    if (Node* const node = layout_object_->GetNode())
      return std::max(1u, GetNode()->CountChildren());
    return 1;
  }
  if (layout_object_->IsHR())
    return 1;
  return 0;
}

PositionWithAffinity LineLayoutItem::PositionForPoint(
    const PhysicalOffset& point) {
  return layout_object_->PositionForPoint(point);
}

PositionWithAffinity LineLayoutItem::CreatePositionWithAffinity(
    int offset,
    TextAffinity affinity) const {
  return layout_object_->CreatePositionWithAffinity(offset, affinity);
}

PositionWithAffinity LineLayoutItem::PositionAfterThis() const {
  return layout_object_->PositionAfterThis();
}

PositionWithAffinity LineLayoutItem::PositionBeforeThis() const {
  return layout_object_->PositionBeforeThis();
}

void LineLayoutItem::SlowSetPaintingLayerNeedsRepaint() {
  ObjectPaintInvalidator(*layout_object_).SlowSetPaintingLayerNeedsRepaint();
}

}  // namespace blink
