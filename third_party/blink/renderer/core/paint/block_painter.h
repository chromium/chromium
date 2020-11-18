// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InlineBox;
class LayoutBlock;
class LayoutBox;
class ScopedPaintState;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

class BlockPainter {
  STACK_ALLOCATED();

 public:
  BlockPainter(const LayoutBlock& block) : layout_block_(block) {}

  void Paint(const PaintInfo&);
  void PaintObject(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintContents(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintChildren(const PaintInfo&);
  void PaintChild(const LayoutBox&, const PaintInfo&);

  // See ObjectPainter::PaintAllPhasesAtomically().
  void PaintAllChildPhasesAtomically(const LayoutBox&, const PaintInfo&);
  void PaintChildrenAtomically(const OrderIterator&, const PaintInfo&);
  static void PaintInlineBox(const InlineBox&, const PaintInfo&);

 private:
  void PaintBlockFlowContents(const PaintInfo&, const PhysicalOffset&);
  void PaintCarets(const PaintInfo&, const PhysicalOffset& paint_offset);

  bool ShouldPaint(const ScopedPaintState&) const;

  CORE_EXPORT PhysicalRect
  OverflowRectForCullRectTesting(bool is_printing) const;

  FRIEND_TEST_ALL_PREFIXES(BlockPainterTest, OverflowRectForCullRectTesting);
  FRIEND_TEST_ALL_PREFIXES(BlockPainterTest,
                           OverflowRectCompositedScrollingForCullRectTesting);
  const LayoutBlock& layout_block_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_PAINTER_H_
