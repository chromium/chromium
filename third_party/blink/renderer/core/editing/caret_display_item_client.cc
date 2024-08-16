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

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

CaretDisplayItemClient::CaretDisplayItemClient() = default;
CaretDisplayItemClient::~CaretDisplayItemClient() = default;
void CaretDisplayItemClient::Trace(Visitor* visitor) const {
  visitor->Trace(layout_block_);
  visitor->Trace(previous_layout_block_);
  visitor->Trace(box_fragment_);
  DisplayItemClient::Trace(visitor);
}

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
  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPosition(caret_position, kCannotCrossEditingBoundary);
  if (!caret_rect.layout_object)
    return {};

  // Get the layoutObject that will be responsible for painting the caret
  // (which is either the layoutObject we just found, or one of its containers).
  LayoutBlock* caret_block;
  if (caret_rect.root_box_fragment) {
    caret_block =
        To<LayoutBlock>(caret_rect.root_box_fragment->GetMutableLayoutObject());
    // The root box fragment's layout object should always match the one we'd
    // get from CaretLayoutBlock, except for atomic inline-level LayoutBlocks
    // (i.e. display: inline-block). In those cases, the layout object should be
    // either the caret rect's layout block, or its containing block.
    if (!(caret_rect.layout_object->IsLayoutBlock() &&
          caret_rect.layout_object->IsAtomicInlineLevel())) {
      DCHECK_EQ(caret_block, CaretLayoutBlock(caret_position.AnchorNode(),
                                              caret_rect.layout_object));
    } else if (caret_block != caret_rect.layout_object) {
      DCHECK_EQ(caret_block, caret_rect.layout_object->ContainingBlock());
    }
  } else {
    caret_block =
        CaretLayoutBlock(caret_position.AnchorNode(), caret_rect.layout_object);
  }
  return {MapCaretRectToCaretPainter(caret_block, caret_rect), caret_block,
          caret_rect.root_box_fragment};
}

void CaretDisplayItemClient::LayoutBlockWillBeDestroyed(
    const LayoutBlock& block) {
  if (block == layout_block_)
    layout_block_ = nullptr;
  if (block == previous_layout_block_)
    previous_layout_block_ = nullptr;
}

bool CaretDisplayItemClient::ShouldPaintCaret(
    const PhysicalBoxFragment& box_fragment) const {
  const auto* const block =
      DynamicTo<LayoutBlock>(box_fragment.GetLayoutObject());
  if (!block)
    return false;
  if (!ShouldPaintCaret(*block))
    return false;
  return !box_fragment_ || &box_fragment == box_fragment_;
}

void CaretDisplayItemClient::UpdateStyleAndLayoutIfNeeded(
    const PositionWithAffinity& caret_position) {
  // This method may be called multiple times (e.g. in partial lifecycle
  // updates) before a paint invalidation. We should save previous_layout_block_
  // if it has not been saved since the last paint invalidation to ensure the
  // caret painted in the previous paint invalidated block will be invalidated.
  // We don't care about intermediate changes of LayoutBlock because they are
  // not painted.
  if (!previous_layout_block_)
    previous_layout_block_ = layout_block_.Get();

  CaretRectAndPainterBlock rect_and_block =
      ComputeCaretRectAndPainterBlock(caret_position);
  LayoutBlock* new_layout_block = rect_and_block.painter_block;
  if (new_layout_block != layout_block_) {
    if (layout_block_)
      layout_block_->SetShouldCheckForPaintInvalidation();
    layout_block_ = new_layout_block;

    if (new_layout_block) {
      needs_paint_invalidation_ = true;
      // The caret property tree space may have changed.
      layout_block_->GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
    }
  }

  if (!new_layout_block) {
    color_ = Color();
    local_rect_ = PhysicalRect();
    return;
  }

  const PhysicalBoxFragment* const new_box_fragment =
      rect_and_block.box_fragment;
  if (new_box_fragment != box_fragment_) {
    // The caret property tree space may have changed.
    layout_block_->GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();

    if (new_box_fragment)
      needs_paint_invalidation_ = true;
    box_fragment_ = new_box_fragment;
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
  // TODO(crbug.com/1123630): Avoid paint invalidation on caret movement.
  if (new_local_rect != local_rect_) {
    needs_paint_invalidation_ = true;
    local_rect_ = new_local_rect;
  }

  if (needs_paint_invalidation_)
    new_layout_block->SetShouldCheckForPaintInvalidation();
}

void CaretDisplayItemClient::SetActive(bool active) {
  if (active == is_active_)
    return;
  is_active_ = active;
  needs_paint_invalidation_ = true;
}

void CaretDisplayItemClient::EnsureInvalidationOfPreviousLayoutBlock() {
  if (!previous_layout_block_ || previous_layout_block_ == layout_block_) {
    return;
  }

  PaintInvalidatorContext context;
  context.painting_layer = previous_layout_block_->PaintingLayer();
  InvalidatePaintInPreviousLayoutBlock(context);
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

  if (layout_block_ == previous_layout_block_)
    previous_layout_block_ = nullptr;

  needs_paint_invalidation_ |= layout_block_->ShouldDoFullPaintInvalidation();
  needs_paint_invalidation_ |=
      context.fragment_data->PaintOffset() != context.old_paint_offset;

  if (!needs_paint_invalidation_)
    return;

  needs_paint_invalidation_ = false;
  context.painting_layer->SetNeedsRepaint();
  ObjectPaintInvalidatorWithContext(*layout_block_, context)
      .InvalidateDisplayItemClient(*this, PaintInvalidationReason::kCaret);
}

void CaretDisplayItemClient::PaintCaret(
    GraphicsContext& context,
    const PhysicalOffset& paint_offset,
    DisplayItem::Type display_item_type) const {
  PhysicalRect drawing_rect = local_rect_;
  drawing_rect.Move(paint_offset);

  // When caret is in text-combine box with scaling, |context| is already
  // associated to drawing record to apply affine transform.
  std::optional<DrawingRecorder> recorder;
  if (!context.InDrawingRecorder()) [[likely]] {
    if (DrawingRecorder::UseCachedDrawingIfPossible(context, *this,
                                                    display_item_type))
      return;
    recorder.emplace(context, *this, display_item_type,
                     ToPixelSnappedRect(drawing_rect));
  }

  gfx::Rect paint_rect = ToPixelSnappedRect(drawing_rect);
  context.FillRect(paint_rect, color_,
                   PaintAutoDarkMode(layout_block_->StyleRef(),
                                     DarkModeFilter::ElementRole::kForeground));
}

void CaretDisplayItemClient::RecordSelection(GraphicsContext& context,
                                             const PhysicalOffset& paint_offset,
                                             gfx::SelectionBound::Type type) {
  PhysicalRect drawing_rect = local_rect_;
  drawing_rect.Move(paint_offset);
  gfx::Rect paint_rect = ToPixelSnappedRect(drawing_rect);

  // For the caret, the start and end selection bounds are recorded as
  // the same edges, with the type marked as CENTER or HIDDEN.
  PaintedSelectionBound start = {type, paint_rect.origin(),
                                 paint_rect.bottom_left(), false};
  PaintedSelectionBound end = start;

  // Get real world data to help debug crbug.com/1441243.
#if DCHECK_IS_ON()
  String debug_info = drawing_rect.ToString();
#else
  String debug_info = "";
#endif

  context.GetPaintController().RecordSelection(start, end, debug_info);
}

String CaretDisplayItemClient::DebugName() const {
  return "Caret";
}

}  // namespace blink
