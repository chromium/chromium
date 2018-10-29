/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CARET_DISPLAY_ITEM_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CARET_DISPLAY_ITEM_CLIENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"

namespace blink {

class Node;
class GraphicsContext;
class LayoutBlock;
struct PaintInvalidatorContext;

class CORE_EXPORT CaretDisplayItemClient final : public DisplayItemClient {
 public:
  CaretDisplayItemClient();
  ~CaretDisplayItemClient() override;

  // TODO(yosin,wangxianzhu): Make these two static functions private or
  // combine them into updateForPaintInvalidation() when the callsites in
  // FrameCaret are removed.

  // Creating VisiblePosition causes synchronous layout so we should use the
  // PositionWithAffinity version if possible.
  // A position in HTMLTextFromControlElement is a typical example.
  static LayoutRect ComputeCaretRect(
      const PositionWithAffinity& caret_position);
  static LayoutBlock* CaretLayoutBlock(const Node*);

  // Called indirectly from LayoutBlock::clearPreviousVisualRects().
  void ClearPreviousVisualRect(const LayoutBlock&);

  // Called indirectly from LayoutBlock::willBeDestroyed().
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);

  // Called when a FrameView finishes layout. Updates style and geometry of the
  // caret for paint invalidation and painting.
  void UpdateStyleAndLayoutIfNeeded(const PositionWithAffinity& caret_position);

  // Called during LayoutBlock paint invalidation.
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  bool ShouldPaintCaret(const LayoutBlock& block) const {
    return &block == layout_block_;
  }
  void PaintCaret(GraphicsContext&,
                  const LayoutPoint& paint_offset,
                  DisplayItem::Type) const;

  // DisplayItemClient methods.
  LayoutRect VisualRect() const final;
  String DebugName() const final;

 private:
  friend class CaretDisplayItemClientTest;

  void InvalidatePaintInCurrentLayoutBlock(const PaintInvalidatorContext&);
  void InvalidatePaintInPreviousLayoutBlock(const PaintInvalidatorContext&);

  // These are updated by updateStyleAndLayoutIfNeeded().
  Color color_;
  LayoutRect local_rect_;
  LayoutBlock* layout_block_ = nullptr;

  // Visual rect of the caret in layout_block_. This is updated by
  // InvalidatePaintIfNeeded().
  LayoutRect visual_rect_;

  // These are set to the previous value of layout_bloc_k and visual_rect_
  // during UpdateStyleAndLayoutIfNeeded() if they haven't been set since the
  // last paint invalidation. They can only be used in InvalidatePaintIfNeeded()
  // to invalidate the caret in the previous layout block.
  const LayoutBlock* previous_layout_block_ = nullptr;
  LayoutRect visual_rect_in_previous_layout_block_;

  bool needs_paint_invalidation_ = false;

  DISALLOW_COPY_AND_ASSIGN(CaretDisplayItemClient);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_CARET_DISPLAY_ITEM_CLIENT_H_
