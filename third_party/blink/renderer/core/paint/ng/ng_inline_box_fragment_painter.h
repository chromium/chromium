// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGInlinePaintContext;
struct PaintInfo;
struct PhysicalRect;

// Common base class for NGInlineBoxFragmentPainter and
// NGLineBoxFragmentPainter.
class NGInlineBoxFragmentPainterBase : public InlineBoxPainterBase {
  STACK_ALLOCATED();

 public:
  void ComputeFragmentOffsetOnLine(TextDirection,
                                   LayoutUnit* offset_on_line,
                                   LayoutUnit* total_width) const;

 protected:
  NGInlineBoxFragmentPainterBase(const NGPhysicalFragment& inline_box_fragment,
                                 const NGInlineCursor* inline_box_cursor,
                                 const NGFragmentItem& inline_box_item,
                                 const LayoutObject& layout_object,
                                 const ComputedStyle& style,
                                 const ComputedStyle& line_style,
                                 NGInlinePaintContext* inline_context)
      : InlineBoxPainterBase(layout_object,
                             &layout_object.GetDocument(),
                             layout_object.GeneratingNode(),
                             style,
                             line_style),
        inline_box_fragment_(inline_box_fragment),
        inline_box_item_(inline_box_item),
        inline_box_cursor_(inline_box_cursor),
        inline_context_(inline_context) {
#if DCHECK_IS_ON()
    if (inline_box_cursor)
      DCHECK_EQ(inline_box_cursor->Current().Item(), &inline_box_item);
    if (inline_box_item.BoxFragment())
      DCHECK_EQ(inline_box_item.BoxFragment(), &inline_box_fragment);
    else
      DCHECK_EQ(inline_box_item.LineBoxFragment(), &inline_box_fragment);
#endif
  }

  // Constructor for |NGFragmentItem|.
  NGInlineBoxFragmentPainterBase(
      const NGInlineCursor& inline_box_cursor,
      const NGFragmentItem& inline_box_item,
      const NGPhysicalBoxFragment& inline_box_fragment,
      const LayoutObject& layout_object,
      const ComputedStyle& style,
      const ComputedStyle& line_style,
      NGInlinePaintContext* inline_context)
      : NGInlineBoxFragmentPainterBase(inline_box_fragment,
                                       &inline_box_cursor,
                                       inline_box_item,
                                       layout_object,
                                       style,
                                       line_style,
                                       inline_context) {}

  const DisplayItemClient& GetDisplayItemClient() const {
    DCHECK(inline_box_item_.GetDisplayItemClient());
    return *inline_box_item_.GetDisplayItemClient();
  }

  virtual PhysicalBoxSides SidesToInclude() const = 0;

  PhysicalRect PaintRectForImageStrip(const PhysicalRect&,
                                      TextDirection direction) const override;

  BorderPaintingType GetBorderPaintType(
      const PhysicalRect& adjusted_frame_rect,
      gfx::Rect& adjusted_clip_rect,
      bool object_has_multiple_boxes) const override;
  void PaintNormalBoxShadow(const PaintInfo&,
                            const ComputedStyle&,
                            const PhysicalRect& paint_rect) override;
  void PaintInsetBoxShadow(const PaintInfo&,
                           const ComputedStyle&,
                           const PhysicalRect& paint_rect) override;

  void PaintBackgroundBorderShadow(const PaintInfo&,
                                   const PhysicalOffset& paint_offset);

  gfx::Rect VisualRect(const PhysicalOffset& paint_offset);

  const NGPhysicalFragment& inline_box_fragment_;
  const NGFragmentItem& inline_box_item_;
  const NGInlineCursor* inline_box_cursor_ = nullptr;
  NGInlinePaintContext* inline_context_ = nullptr;
};

// Painter for LayoutNG inline box fragments. Delegates to NGBoxFragmentPainter
// for all box painting logic that isn't specific to inline boxes.
class NGInlineBoxFragmentPainter : public NGInlineBoxFragmentPainterBase {
  STACK_ALLOCATED();

 public:
  // Constructor for |NGFragmentItem|.
  NGInlineBoxFragmentPainter(const NGInlineCursor& inline_box_cursor,
                             const NGFragmentItem& inline_box_item,
                             const NGPhysicalBoxFragment& inline_box_fragment,
                             NGInlinePaintContext* inline_context)
      : NGInlineBoxFragmentPainterBase(inline_box_cursor,
                                       inline_box_item,
                                       inline_box_fragment,
                                       *inline_box_fragment.GetLayoutObject(),
                                       inline_box_fragment.Style(),
                                       inline_box_fragment.Style(),
                                       inline_context) {
    CheckValid();
  }
  NGInlineBoxFragmentPainter(const NGInlineCursor& inline_box_cursor,
                             const NGFragmentItem& inline_box_item,
                             NGInlinePaintContext* inline_context)
      : NGInlineBoxFragmentPainter(inline_box_cursor,
                                   inline_box_item,
                                   *inline_box_item.BoxFragment(),
                                   inline_context) {
    DCHECK(inline_box_item.BoxFragment());
  }

  void Paint(const PaintInfo&, const PhysicalOffset& paint_offset);

  static void PaintAllFragments(const LayoutInline& layout_inline,
                                const PaintInfo&,
                                const PhysicalOffset& paint_offset);

 private:
  const NGPhysicalBoxFragment& PhysicalFragment() const {
    return static_cast<const NGPhysicalBoxFragment&>(inline_box_fragment_);
  }

  PhysicalBoxSides SidesToInclude() const final;

#if DCHECK_IS_ON()
  void CheckValid() const;
#else
  void CheckValid() const {}
#endif
};

// Painter for LayoutNG line box fragments. Line boxes don't paint anything,
// except when ::first-line style has background properties specified.
// https://drafts.csswg.org/css-pseudo-4/#first-line-styling
class NGLineBoxFragmentPainter : public NGInlineBoxFragmentPainterBase {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentPainter(const NGPhysicalFragment& line_box_fragment,
                           const NGFragmentItem& line_box_item,
                           const NGPhysicalBoxFragment& block_fragment)
      : NGLineBoxFragmentPainter(line_box_fragment,
                                 line_box_item,
                                 block_fragment,
                                 *block_fragment.GetLayoutObject()) {}

  static bool NeedsPaint(const NGPhysicalFragment& line_fragment) {
    DCHECK_EQ(line_fragment.Type(),
              NGPhysicalFragment::NGFragmentType::kFragmentLineBox);
    return line_fragment.UsesFirstLineStyle();
  }

  // Borders are not part of ::first-line style and therefore not painted, but
  // the function name is kept consistent with other classes.
  void PaintBackgroundBorderShadow(const PaintInfo&,
                                   const PhysicalOffset& paint_offset);

 private:
  NGLineBoxFragmentPainter(const NGPhysicalFragment& line_box_fragment,
                           const NGFragmentItem& line_box_item,
                           const NGPhysicalBoxFragment& block_fragment,
                           const LayoutObject& layout_block_flow)
      : NGInlineBoxFragmentPainterBase(
            line_box_fragment,
            /* inline_box_cursor */ nullptr,
            line_box_item,
            layout_block_flow,
            // Use the style from the containing block. |line_fragment.Style()|
            // is a copy at the time of the last layout to reflect the line
            // direction, and its paint properties may have been changed.
            // TODO(kojii): Reconsider |line_fragment.Style()|.
            layout_block_flow.StyleRef(),
            layout_block_flow.FirstLineStyleRef(),
            /* inline_context */ nullptr),
        block_fragment_(block_fragment) {
    DCHECK_EQ(line_box_fragment.Type(),
              NGPhysicalFragment::NGFragmentType::kFragmentLineBox);
    DCHECK(NeedsPaint(line_box_fragment));
    DCHECK(layout_block_flow.IsLayoutNGObject());
  }

  const NGPhysicalLineBoxFragment& PhysicalFragment() const {
    return static_cast<const NGPhysicalLineBoxFragment&>(inline_box_fragment_);
  }

  PhysicalBoxSides SidesToInclude() const final { return PhysicalBoxSides(); }

  const NGPhysicalBoxFragment& block_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_
