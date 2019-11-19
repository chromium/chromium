// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_border_edges.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/paint/inline_box_painter_base.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGPaintFragment;
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
  NGInlineBoxFragmentPainterBase(
      const NGPhysicalFragment& inline_box_fragment,
      const NGPaintFragment* inline_box_paint_fragment,
      const NGFragmentItem* inline_box_item,
      const LayoutObject& layout_object,
      const ComputedStyle& style,
      const ComputedStyle& line_style)
      : InlineBoxPainterBase(layout_object,
                             &layout_object.GetDocument(),
                             layout_object.GeneratingNode(),
                             style,
                             line_style),
        inline_box_fragment_(inline_box_fragment),
        inline_box_paint_fragment_(inline_box_paint_fragment),
        inline_box_item_(inline_box_item) {
#if DCHECK_IS_ON()
    if (inline_box_paint_fragment) {
      DCHECK_EQ(&inline_box_paint_fragment->PhysicalFragment(),
                &inline_box_fragment);
      DCHECK(!inline_box_item);
    } else if (inline_box_item) {
      if (inline_box_item->BoxFragment())
        DCHECK_EQ(inline_box_item->BoxFragment(), &inline_box_fragment);
      else
        DCHECK_EQ(inline_box_item->LineBoxFragment(), &inline_box_fragment);
    } else {
      NOTREACHED();
    }
#endif
  }

  // Constructor for |NGPaintFragment|.
  NGInlineBoxFragmentPainterBase(const NGPaintFragment& inline_box_fragment,
                                 const LayoutObject& layout_object,
                                 const ComputedStyle& style,
                                 const ComputedStyle& line_style)
      : NGInlineBoxFragmentPainterBase(inline_box_fragment.PhysicalFragment(),
                                       &inline_box_fragment,
                                       /* inline_box_item */ nullptr,
                                       layout_object,
                                       style,
                                       line_style) {}

  // Constructor for |NGFragmentItem|.
  NGInlineBoxFragmentPainterBase(
      const NGFragmentItem& inline_box_item,
      const NGPhysicalBoxFragment& inline_box_fragment,
      const LayoutObject& layout_object,
      const ComputedStyle& style,
      const ComputedStyle& line_style)
      : NGInlineBoxFragmentPainterBase(inline_box_fragment,
                                       /* inline_box_paint_fragment */ nullptr,
                                       &inline_box_item,
                                       layout_object,
                                       style,
                                       line_style) {}

  const DisplayItemClient& GetDisplayItemClient() const {
    if (inline_box_paint_fragment_)
      return *inline_box_paint_fragment_;
    DCHECK(inline_box_item_);
    return *inline_box_item_;
  }

  const virtual NGBorderEdges BorderEdges() const = 0;

  PhysicalRect PaintRectForImageStrip(const PhysicalRect&,
                                      TextDirection direction) const override;

  BorderPaintingType GetBorderPaintType(
      const PhysicalRect& adjusted_frame_rect,
      IntRect& adjusted_clip_rect,
      bool object_has_multiple_boxes) const override;
  void PaintNormalBoxShadow(const PaintInfo&,
                            const ComputedStyle&,
                            const PhysicalRect& paint_rect) override;
  void PaintInsetBoxShadow(const PaintInfo&,
                           const ComputedStyle&,
                           const PhysicalRect& paint_rect) override;

  void PaintBackgroundBorderShadow(const PaintInfo&,
                                   const PhysicalOffset& paint_offset);

  const NGPhysicalFragment& inline_box_fragment_;
  const NGPaintFragment* inline_box_paint_fragment_ = nullptr;
  const NGFragmentItem* inline_box_item_ = nullptr;
};

// Painter for LayoutNG inline box fragments. Delegates to NGBoxFragmentPainter
// for all box painting logic that isn't specific to inline boxes.
class NGInlineBoxFragmentPainter : public NGInlineBoxFragmentPainterBase {
  STACK_ALLOCATED();

 public:
  // Constructor for |NGPaintFragment|.
  NGInlineBoxFragmentPainter(const NGPaintFragment& inline_box_fragment)
      : NGInlineBoxFragmentPainterBase(inline_box_fragment,
                                       *inline_box_fragment.GetLayoutObject(),
                                       inline_box_fragment.Style(),
                                       inline_box_fragment.Style()) {
    DCHECK_EQ(inline_box_fragment.PhysicalFragment().Type(),
              NGPhysicalFragment::NGFragmentType::kFragmentBox);
    DCHECK_EQ(inline_box_fragment.PhysicalFragment().BoxType(),
              NGPhysicalFragment::NGBoxType::kInlineBox);
  }

  // Constructor for |NGFragmentItem|.
  NGInlineBoxFragmentPainter(const NGFragmentItem& inline_box_item,
                             const NGPhysicalBoxFragment& inline_box_fragment,
                             NGInlineCursor* descendants = nullptr)
      : NGInlineBoxFragmentPainterBase(inline_box_item,
                                       inline_box_fragment,
                                       *inline_box_fragment.GetLayoutObject(),
                                       inline_box_fragment.Style(),
                                       inline_box_fragment.Style()),
        descendants_(descendants) {
    DCHECK_EQ(inline_box_fragment.Type(),
              NGPhysicalFragment::NGFragmentType::kFragmentBox);
    DCHECK_EQ(inline_box_fragment.BoxType(),
              NGPhysicalFragment::NGBoxType::kInlineBox);
  }

  void Paint(const PaintInfo&, const PhysicalOffset& paint_offset);

  static void PaintAllFragments(const LayoutInline& layout_inline,
                                const PaintInfo&,
                                const PhysicalOffset& paint_offset);

 private:
  const NGPhysicalBoxFragment& PhysicalFragment() const {
    return static_cast<const NGPhysicalBoxFragment&>(inline_box_fragment_);
  }

  const NGBorderEdges BorderEdges() const final;

  mutable base::Optional<NGBorderEdges> border_edges_;
  NGInlineCursor* descendants_ = nullptr;
};

// Painter for LayoutNG line box fragments. Line boxes don't paint anything,
// except when ::first-line style has background properties specified.
// https://drafts.csswg.org/css-pseudo-4/#first-line-styling
class NGLineBoxFragmentPainter : public NGInlineBoxFragmentPainterBase {
  STACK_ALLOCATED();

 public:
  NGLineBoxFragmentPainter(const NGPhysicalFragment& line_box_fragment,
                           const NGPaintFragment* line_box_paint_fragment,
                           const NGFragmentItem* line_box_item,
                           const NGPhysicalBoxFragment& block_fragment,
                           const NGPaintFragment* block_paint_fragment)
      : NGLineBoxFragmentPainter(line_box_fragment,
                                 line_box_paint_fragment,
                                 line_box_item,
                                 block_fragment,
                                 block_paint_fragment,
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
                           const NGPaintFragment* line_box_paint_fragment,
                           const NGFragmentItem* line_box_item,
                           const NGPhysicalBoxFragment& block_fragment,
                           const NGPaintFragment* block_paint_fragment,
                           const LayoutObject& layout_block_flow)
      : NGInlineBoxFragmentPainterBase(
            line_box_fragment,
            line_box_paint_fragment,
            line_box_item,
            layout_block_flow,
            // Use the style from the containing block. |line_fragment.Style()|
            // is a copy at the time of the last layout to reflect the line
            // direction, and its paint properties may have been changed.
            // TODO(kojii): Reconsider |line_fragment.Style()|.
            layout_block_flow.StyleRef(),
            layout_block_flow.FirstLineStyleRef()),
        block_fragment_(block_fragment),
        block_paint_fragment_(block_paint_fragment) {
    DCHECK_EQ(line_box_fragment.Type(),
              NGPhysicalFragment::NGFragmentType::kFragmentLineBox);
    DCHECK(NeedsPaint(line_box_fragment));
    DCHECK(layout_block_flow.IsLayoutNGMixin());
  }

  const NGPhysicalLineBoxFragment& PhysicalFragment() const {
    return static_cast<const NGPhysicalLineBoxFragment&>(inline_box_fragment_);
  }

  const NGBorderEdges BorderEdges() const final { return NGBorderEdges(); }

  const NGPhysicalBoxFragment& block_fragment_;
  const NGPaintFragment* block_paint_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_BOX_FRAGMENT_PAINTER_H_
