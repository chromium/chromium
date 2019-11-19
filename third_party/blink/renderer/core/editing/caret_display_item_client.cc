/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_and_visual_rect_needing_update.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

CaretDisplayItemClient::CaretDisplayItemClient() = default;
CaretDisplayItemClient::~CaretDisplayItemClient() = default;

namespace {

inline bool CaretRendersInsideNode(const Node* node) {
  return node && !IsDisplayInsideTable(node) && !EditingIgnoresContent(*node);
}

LayoutBlock* CaretLayoutBlock(const Node* node,
                              const LayoutObject* layout_object) {
  if (!node)
    return nullptr;

  if (!layout_object)
    return nullptr;

  auto* caret_layout_object = DynamicTo<LayoutBlock>(layout_object);
  // if caretNode is a block and caret is inside it then caret should be painted
  // by that block
  bool painted_by_block = caret_layout_object && CaretRendersInsideNode(node);
  return painted_by_block ? const_cast<LayoutBlock*>(caret_layout_object)
                          : layout_object->ContainingBlock();
}

PhysicalRect MapCaretRectToCaretPainter(const LayoutBlock* caret_block,
                                        const LocalCaretRect& caret_rect) {
  // FIXME: This shouldn't be called on un-rooted subtrees.
  // FIXME: This should probably just use mapLocalToAncestor.
  // Compute an offset between the caretLayoutItem and the caretPainterItem.

  LayoutObject* caret_layout_object =
      const_cast<LayoutObject*>(caret_rect.layout_object);
  DCHECK(caret_layout_object->IsDescendantOf(caret_block));

  PhysicalRect result_rect = caret_rect.rect;
  while (caret_layout_object != caret_block) {
    LayoutObject* container_object = caret_layout_object->Container();
    if (!container_object)
      return PhysicalRect();
    result_rect.Move(
        caret_layout_object->OffsetFromContainer(container_object));
    caret_layout_object = container_object;
  }
  return result_rect;
}

}  // namespace

CaretDisplayItemClient::CaretRectAndPainterBlock
CaretDisplayItemClient::ComputeCaretRectAndPainterBlock(
    const PositionWithAffinity& caret_position) {
  if (caret_position.IsNull())
    return {};

  if (!caret_position.AnchorNode()->GetLayoutObject())
    return {};

  // First compute a rect local to the layoutObject at the selection start.
  const LocalCaretRect& caret_rect = LocalCaretRectOfPosition(caret_position);
  if (!caret_rect.layout_object)
    return {};

  // Get the layoutObject that will be responsible for painting the caret
  // (which is either the layoutObject we just found, or one of its containers).
  LayoutBlock* caret_block =
      CaretLayoutBlock(caret_position.AnchorNode(), caret_rect.layout_object);
  return {MapCaretRectToCaretPainter(caret_block, caret_rect), caret_block};
}

void CaretDisplayItemClient::ClearPreviousVisualRect(const LayoutBlock& block) {
  if (block == layout_block_)
    visual_rect_ = IntRect();
  if (block == previous_layout_block_)
    visual_rect_in_previous_layout_block_ = IntRect();
}

void CaretDisplayItemClient::LayoutBlockWillBeDestroyed(
    const LayoutBlock& block) {
  if (block == layout_block_)
    layout_block_ = nullptr;
  if (block == previous_layout_block_)
    previous_layout_block_ = nullptr;
}

void CaretDisplayItemClient::UpdateStyleAndLayoutIfNeeded(
    const PositionWithAffinity& caret_position) {
  // This method may be called multiple times (e.g. in partial lifecycle
  // updates) before a paint invalidation. We should save previous_layout_block_
  // and visual_rect_in_previous_layout_block only if they have not been saved
  // since the last paint invalidation to ensure the caret painted in the
  // previous paint invalidated block will be invalidated. We don't care about
  // intermediate changes of LayoutBlock because they are not painted.
  if (!previous_layout_block_) {
    previous_layout_block_ = layout_block_;
    visual_rect_in_previous_layout_block_ = visual_rect_;
  }

  CaretRectAndPainterBlock rect_and_block =
      ComputeCaretRectAndPainterBlock(caret_position);
  LayoutBlock* new_layout_block = rect_and_block.painter_block;
  if (new_layout_block != layout_block_) {
    if (layout_block_)
      layout_block_->SetShouldCheckForPaintInvalidation();
    layout_block_ = new_layout_block;
    visual_rect_ = IntRect();
    if (new_layout_block) {
      needs_paint_invalidation_ = true;
      if (new_layout_block == previous_layout_block_) {
        // The caret has disappeared and is reappearing in the same block,
        // since the last paint invalidation. Set visual_rect_ as if the caret
        // has always been there as paint invalidation doesn't care about the
        // intermediate changes.
        visual_rect_ = visual_rect_in_previous_layout_block_;
      }
    }
  }

  if (!new_layout_block) {
    color_ = Color();
    local_rect_ = PhysicalRect();
    return;
  }

  Color new_color;
  if (caret_position.AnchorNode()) {
    new_color = caret_position.AnchorNode()->GetLayoutObject()->ResolveColor(
        GetCSSPropertyCaretColor());
  }
  if (new_color != color_) {
    needs_paint_invalidation_ = true;
    color_ = new_color;
  }

  auto new_local_rect = rect_and_block.caret_rect;
  if (new_local_rect != local_rect_) {
    needs_paint_invalidation_ = true;
    local_rect_ = new_local_rect;
  }

  if (needs_paint_invalidation_)
    new_layout_block->SetShouldCheckForPaintInvalidation();
}

void CaretDisplayItemClient::InvalidatePaint(
    const LayoutBlock& block,
    const PaintInvalidatorContext& context) {
  if (block == layout_block_) {
    InvalidatePaintInCurrentLayoutBlock(context);
    return;
  }

  if (block == previous_layout_block_)
    InvalidatePaintInPreviousLayoutBlock(context);
}

void CaretDisplayItemClient::InvalidatePaintInPreviousLayoutBlock(
    const PaintInvalidatorContext& context) {
  DCHECK(previous_layout_block_);

  ObjectPaintInvalidatorWithContext object_invalidator(*previous_layout_block_,
                                                       context);
  context.painting_layer->SetNeedsRepaint();
  object_invalidator.InvalidateDisplayItemClient(
      *this, PaintInvalidationReason::kCaret);
  previous_layout_block_ = nullptr;
}

void CaretDisplayItemClient::InvalidatePaintInCurrentLayoutBlock(
    const PaintInvalidatorContext& context) {
  DCHECK(layout_block_);

  IntRect new_visual_rect;
#if DCHECK_IS_ON()
  FindVisualRectNeedingUpdateScope finder(*layout_block_, context, visual_rect_,
                                          new_visual_rect);
#endif
  if (context.NeedsVisualRectUpdate(*layout_block_)) {
    if (!local_rect_.IsEmpty()) {
      new_visual_rect =
          context.MapLocalRectToVisualRect(*layout_block_, local_rect_);
    }
  } else {
    new_visual_rect = visual_rect_;
  }

  if (layout_block_ == previous_layout_block_)
    previous_layout_block_ = nullptr;

  ObjectPaintInvalidatorWithContext object_invalidator(*layout_block_, context);
  if (!needs_paint_invalidation_ && new_visual_rect == visual_rect_) {
    // The caret may change paint offset without changing visual rect, and we
    // need to invalidate the display item client if the block is doing full
    // paint invalidation.
    if (layout_block_->ShouldDoFullPaintInvalidation()) {
      object_invalidator.InvalidateDisplayItemClient(
          *this, PaintInvalidationReason::kCaret);
    }
    return;
  }

  needs_paint_invalidation_ = false;

  context.painting_layer->SetNeedsRepaint();
  object_invalidator.InvalidateDisplayItemClient(
      *this, PaintInvalidationReason::kCaret);

  visual_rect_ = new_visual_rect;
}

void CaretDisplayItemClient::PaintCaret(
    GraphicsContext& context,
    const PhysicalOffset& paint_offset,
    DisplayItem::Type display_item_type) const {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, *this,
                                                  display_item_type))
    return;

  PhysicalRect drawing_rect = local_rect_;
  drawing_rect.Move(paint_offset);

  DrawingRecorder recorder(context, *this, display_item_type);
  IntRect paint_rect = PixelSnappedIntRect(drawing_rect);
  context.FillRect(paint_rect, color_, DarkModeFilter::ElementRole::kText);
}

String CaretDisplayItemClient::DebugName() const {
  return "Caret";
}

IntRect CaretDisplayItemClient::VisualRect() const {
  return visual_rect_;
}

}  // namespace blink
