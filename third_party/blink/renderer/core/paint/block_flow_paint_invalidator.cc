// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_flow_paint_invalidator.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"

namespace blink {

void BlockFlowPaintInvalidator::InvalidatePaintForOverhangingFloatsInternal(
    InvalidateDescendantMode invalidate_descendants) {
  // Invalidate paint of any overhanging floats (if we know we're the one to
  // paint them).  Otherwise, bail out.
  if (!block_flow_.HasOverhangingFloats())
    return;

  for (const auto& floating_object : block_flow_.GetFloatingObjects()->Set()) {
    // Only issue paint invalidations for the object if it is overhanging, is
    // not in its own layer, and is our responsibility to paint (m_shouldPaint
    // is set). When paintAllDescendants is true, the latter condition is
    // replaced with being a descendant of us.
    if (block_flow_.IsOverhangingFloat(*floating_object) &&
        !floating_object->GetLayoutObject()->HasSelfPaintingLayer() &&
        (floating_object->ShouldPaint() ||
         (invalidate_descendants == kInvalidateDescendants &&
          floating_object->GetLayoutObject()->IsDescendantOf(&block_flow_)))) {
      LayoutBox* floating_box = floating_object->GetLayoutObject();
      floating_box->SetShouldDoFullPaintInvalidation();
      auto* floating_block_flow = DynamicTo<LayoutBlockFlow>(floating_box);
      if (floating_block_flow)
        BlockFlowPaintInvalidator(*floating_block_flow)
            .InvalidatePaintForOverhangingFloatsInternal(
                kDontInvalidateDescendants);
    }
  }
}

void BlockFlowPaintInvalidator::InvalidateDisplayItemClients(
    PaintInvalidationReason reason) {
  ObjectPaintInvalidator object_paint_invalidator(block_flow_);
  object_paint_invalidator.InvalidateDisplayItemClient(block_flow_, reason);

  const NGPaintFragment* paint_fragment = block_flow_.PaintFragment();
  if (paint_fragment) {
    object_paint_invalidator.InvalidateDisplayItemClient(*paint_fragment,
                                                         reason);
  }

  // PaintInvalidationRectangle happens when we invalidate the caret.
  // The later conditions don't apply when we invalidate the caret or the
  // selection.
  if (reason == PaintInvalidationReason::kRectangle ||
      reason == PaintInvalidationReason::kSelection)
    return;

  // It's the RootInlineBox that paints the ::first-line background. Note that
  // since it may be expensive to figure out if the first line is affected by
  // any ::first-line selectors at all, we just invalidate it unconditionally
  // which is typically cheaper.
  if (RootInlineBox* line = block_flow_.FirstRootBox()) {
    if (line->IsFirstLineStyle()) {
      object_paint_invalidator.InvalidateDisplayItemClient(*line, reason);
    }
  } else if (paint_fragment) {
    NGPaintFragment* line = paint_fragment->FirstLineBox();
    if (line && line->PhysicalFragment().UsesFirstLineStyle()) {
      object_paint_invalidator.InvalidateDisplayItemClient(*line, reason);
    }
  }

  if (block_flow_.MultiColumnFlowThread()) {
    // Invalidate child LayoutMultiColumnSets which may need to repaint column
    // rules after m_blockFlow's column rule style and/or layout changed.
    for (LayoutObject* child = block_flow_.FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsLayoutMultiColumnSet() &&
          !child->ShouldDoFullPaintInvalidation())
        object_paint_invalidator.InvalidateDisplayItemClient(*child, reason);
    }
  }
}

}  // namespace blink
