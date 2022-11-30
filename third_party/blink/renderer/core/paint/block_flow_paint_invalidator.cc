// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/block_flow_paint_invalidator.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/paint/box_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
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

  NGInlineCursor cursor(block_flow_);
  if (cursor) {
    // Line boxes record hit test data (see NGBoxFragmentPainter::PaintLineBox)
    // and should be invalidated if they change.
    bool invalidate_all_lines = block_flow_.HasEffectiveAllowedTouchAction() ||
                                block_flow_.InsideBlockingWheelEventHandler();

    for (cursor.MoveToFirstLine(); cursor; cursor.MoveToNextLine()) {
      // The first line NGLineBoxFragment paints the ::first-line background.
      // Because it may be expensive to figure out if the first line is affected
      // by any ::first-line selectors at all, we just invalidate
      // unconditionally which is typically cheaper.
      if (invalidate_all_lines || cursor.Current().UsesFirstLineStyle()) {
        DCHECK(cursor.Current().GetDisplayItemClient());
        object_paint_invalidator.InvalidateDisplayItemClient(
            *cursor.Current().GetDisplayItemClient(), reason);
      }
      if (!invalidate_all_lines)
        break;
    }
  } else if (RootInlineBox* line = block_flow_.FirstRootBox()) {
    // It's the RootInlineBox that paints the ::first-line background. Note that
    // since it may be expensive to figure out if the first line is affected by
    // any ::first-line selectors at all, we just invalidate it unconditionally
    // which is typically cheaper.
    if (line->IsFirstLineStyle()) {
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
