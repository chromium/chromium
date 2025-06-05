// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/outline_rect_collector.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void UnionOutlineRectCollector::AddRect(const PhysicalRect& r) {
  if (rect_) {
    rect_->UniteEvenIfEmpty(r);
  } else {
    rect_ = r;
  }
}

void UnionOutlineRectCollector::Combine(OutlineRectCollector* collector,
                                        const LayoutObject& descendant,
                                        const LayoutBoxModelObject* ancestor,
                                        const PhysicalOffset& post_offset) {
  CHECK_EQ(collector->GetType(), Type::kUnion);
  if (collector->IsEmpty()) {
    return;
  }
  PhysicalRect rect = descendant.LocalToAncestorRect(
      static_cast<UnionOutlineRectCollector*>(collector)->Rect(), ancestor);
  rect.offset += post_offset;
  AddRect(rect);
}

void UnionOutlineRectCollector::Combine(
    OutlineRectCollector* collector,
    const PhysicalOffset& additional_offset) {
  CHECK_EQ(collector->GetType(), Type::kUnion);
  if (collector->IsEmpty()) {
    return;
  }
  auto rect = static_cast<UnionOutlineRectCollector*>(collector)->Rect();
  rect.offset += additional_offset;
  AddRect(rect);
}

void VectorOutlineRectCollector::Combine(OutlineRectCollector* collector,
                                         const LayoutObject& descendant,
                                         const LayoutBoxModelObject* ancestor,
                                         const PhysicalOffset& post_offset) {
  CHECK_EQ(collector->GetType(), Type::kVector);
  VectorOf<PhysicalRect> rects =
      static_cast<VectorOutlineRectCollector*>(collector)->TakeRects();
  for (const auto& r : rects) {
    PhysicalRect rect = descendant.LocalToAncestorRect(r, ancestor);
    rect.offset += post_offset;
    rects_.push_back(rect);
  }
}

void VectorOutlineRectCollector::Combine(
    OutlineRectCollector* collector,
    const PhysicalOffset& additional_offset) {
  CHECK_EQ(collector->GetType(), Type::kVector);
  if (!additional_offset.IsZero()) {
    for (PhysicalRect& rect :
         static_cast<VectorOutlineRectCollector*>(collector)->TakeRects()) {
      rect.offset += additional_offset;
      rects_.push_back(rect);
    }
  } else {
    rects_.AppendVector(
        static_cast<VectorOutlineRectCollector*>(collector)->TakeRects());
  }
}

}  // namespace blink
