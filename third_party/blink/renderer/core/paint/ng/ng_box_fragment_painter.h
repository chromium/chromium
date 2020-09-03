// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_

#include "third_party/blink/renderer/core/layout/api/hit_test_action.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class BoxDecorationData;
class FillLayer;
class HitTestLocation;
class HitTestResult;
class NGFragmentItems;
class NGInlineCursor;
class NGInlineBackwardCursor;
class NGInlineBoxFragmentPainter;
class NGPhysicalFragment;
class ScopedPaintState;
struct PaintInfo;

// Painter for LayoutNG box fragments, paints borders and background. Delegates
// to NGTextFragmentPainter to paint line box fragments.
class NGBoxFragmentPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  NGBoxFragmentPainter(const NGPhysicalBoxFragment&);
  // Construct for a box fragment. |NGPaintFragment| for this
  // box if this box has inline formatting context, otherwise |nullptr|.
  NGBoxFragmentPainter(const NGPhysicalBoxFragment&, const NGPaintFragment*);
  // Construct for an inline formatting context.
  NGBoxFragmentPainter(const NGPaintFragment&);
  // Construct for an inline box.
  NGBoxFragmentPainter(const NGInlineCursor& inline_box_cursor,
                       const NGFragmentItem& item,
                       const NGPhysicalBoxFragment& fragment);

  void Paint(const PaintInfo&);
  void PaintObject(const PaintInfo&,
                   const PhysicalOffset&,
                   bool suppress_box_decoration_background = false);

  // Hit tests this box fragment.
  // @param physical_offset Physical offset of this box fragment in the
  // coordinate space of |hit_test_location|.
  // TODO(eae): Change to take a HitTestResult pointer instead as it mutates.
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& physical_offset,
                   HitTestAction);
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& physical_offset,
                   const PhysicalOffset& inline_root_offset,
                   HitTestAction);

  bool HitTestAllPhases(HitTestResult&,
                        const HitTestLocation&,
                        const PhysicalOffset& accumulated_offset,
                        HitTestFilter = kHitTestAll);

  void PaintBoxDecorationBackgroundWithRectImpl(const PaintInfo&,
                                                const PhysicalRect&,
                                                const BoxDecorationData&);

  IntRect VisualRect(const PhysicalOffset& paint_offset);

 protected:
  LayoutRectOutsets ComputeBorders() const override;
  LayoutRectOutsets ComputePadding() const override;
  BoxPainterBase::FillLayerInfo GetFillLayerInfo(
      const Color&,
      const FillLayer&,
      BackgroundBleedAvoidance,
      bool is_painting_scrolling_background) const override;
  bool IsPaintingScrollingBackground(const PaintInfo&) const override;

  void PaintTextClipMask(GraphicsContext&,
                         const IntRect& mask_rect,
                         const PhysicalOffset& paint_offset,
                         bool object_has_multiple_boxes) override;
  void PaintTextClipMask(const PaintInfo& paint_info,
                         PhysicalOffset paint_offset,
                         NGInlineBoxFragmentPainter* inline_box_painter);
  PhysicalRect AdjustRectForScrolledContent(
      const PaintInfo&,
      const BoxPainterBase::FillLayerInfo&,
      const PhysicalRect&) override;

 private:
  NGBoxFragmentPainter(const NGPhysicalBoxFragment&,
                       const DisplayItemClient& display_item_client,
                       const NGPaintFragment* = nullptr,
                       const NGInlineCursor* inline_box_cursor = nullptr,
                       const NGFragmentItem* = nullptr);

  enum MoveTo { kDontSkipChildren, kSkipChildren };
  bool ShouldPaint(const ScopedPaintState&) const;

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset,
                                    bool suppress_box_decoration_background);

  // |visual_rect| is for the drawing display item, covering overflowing box
  // shadows and border image outsets. |paint_rect| is the border box rect in
  // paint coordinates.
  void PaintBoxDecorationBackgroundWithRect(const PaintInfo&,
                                            const IntRect& visual_rect,
                                            const PhysicalRect& paint_rect,
                                            const DisplayItemClient&);

  void PaintColumnRules(const PaintInfo&, const PhysicalOffset& paint_offset);

  void PaintInternal(const PaintInfo&);
  void PaintAllPhasesAtomically(const PaintInfo&);
  void PaintBlockChildren(const PaintInfo&, PhysicalOffset);
  void PaintInlineItems(const PaintInfo&,
                        const PhysicalOffset& paint_offset,
                        const PhysicalOffset& parent_offset,
                        NGInlineCursor* cursor);
  void PaintLineBoxChildren(NGInlineCursor* children,
                            const PaintInfo&,
                            const PhysicalOffset& paint_offset);
  void PaintLineBoxChildItems(NGInlineCursor* children,
                              const PaintInfo&,
                              const PhysicalOffset& paint_offset);
  void PaintLineBox(const NGPhysicalFragment& line_box_fragment,
                    const DisplayItemClient& display_item_client,
                    const NGPaintFragment* line_box_paint_fragment,
                    const NGFragmentItem* line_box_item,
                    wtf_size_t line_fragment_id,
                    const PaintInfo&,
                    const PhysicalOffset& paint_offset);
  void PaintBackplate(NGInlineCursor* descendants,
                      const PaintInfo&,
                      const PhysicalOffset& paint_offset);
  void PaintInlineChildren(NGPaintFragment::ChildList,
                           const PaintInfo&,
                           const PhysicalOffset& paint_offset);
  void PaintInlineChildBoxUsingLegacyFallback(const NGPhysicalFragment&,
                                              const PaintInfo&);
  void PaintBlockFlowContents(const PaintInfo&,
                              const PhysicalOffset& paint_offset);
  void PaintAtomicInlineChild(const NGPaintFragment&, const PaintInfo&);
  void PaintTextChild(const NGPaintFragment&,
                      const PaintInfo&,
                      const PhysicalOffset& paint_offset);
  void PaintTextItem(const NGInlineCursor& cursor,
                     const PaintInfo&,
                     const PhysicalOffset& paint_offset,
                     const PhysicalOffset& parent_offset);
  void PaintBoxItem(const NGFragmentItem& item,
                    const NGPhysicalBoxFragment& child_fragment,
                    const NGInlineCursor& cursor,
                    const PaintInfo& paint_info,
                    const PhysicalOffset& paint_offset);
  void PaintBoxItem(const NGFragmentItem& item,
                    const NGInlineCursor& cursor,
                    const PaintInfo& paint_info,
                    const PhysicalOffset& paint_offset,
                    const PhysicalOffset& parent_offset);
  void PaintFloatingItems(const PaintInfo&, NGInlineCursor* cursor);
  void PaintFloatingChildren(const NGPhysicalContainerFragment&,
                             const PaintInfo& paint_info,
                             const PaintInfo& float_paint_info);
  void PaintFloats(const PaintInfo&);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintBackground(const PaintInfo&,
                       const PhysicalRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);
  void PaintCarets(const PaintInfo&, const PhysicalOffset& paint_offset);

  // This should be called in the background paint phase even if there is no
  // other painted content.
  void RecordScrollHitTestData(const PaintInfo&,
                               const DisplayItemClient& background_client);

  bool ShouldRecordHitTestData(const PaintInfo&);

  // This struct has common data needed while traversing trees for the hit
  // testing.
  struct HitTestContext {
    STACK_ALLOCATED();

   public:
    HitTestContext(HitTestAction action,
                   const HitTestLocation& location,
                   const PhysicalOffset& inline_root_offset,
                   HitTestResult* result)
        : action(action),
          location(location),
          inline_root_offset(inline_root_offset),
          result(result) {}

    // Add |node| to |HitTestResult|. Returns true if the hit-testing should
    // stop.
    bool AddNodeToResult(Node* node,
                         const PhysicalRect& bounds_rect,
                         const PhysicalOffset& offset) const;

    HitTestAction action;
    const HitTestLocation& location;
    // When traversing within an inline formatting context, this member
    // represents the offset of the root of the inline formatting context.
    PhysicalOffset inline_root_offset;
    // The result is set to this member, but its address does not change during
    // the traversal.
    HitTestResult* result;
  };

  // Hit tests the children of a container fragment, which is either
  // |box_fragment_|, or one of its child line box fragments.
  // @param physical_offset Physical offset of the container fragment's content
  // box in paint layer. Note that this includes scrolling offset when the
  // container has 'overflow: scroll'.
  bool NodeAtPoint(const HitTestContext& hit_test,
                   const PhysicalOffset& physical_offset);
  bool HitTestChildren(const HitTestContext& hit_test,
                       const PhysicalOffset& physical_offset);
  bool HitTestChildren(const HitTestContext& hit_test,
                       const NGPhysicalBoxFragment& container,
                       const NGInlineCursor& children,
                       const PhysicalOffset& physical_offset);
  bool HitTestBlockChildren(HitTestResult&,
                            const HitTestLocation&,
                            PhysicalOffset,
                            HitTestAction);
  bool HitTestPaintFragmentChildren(const HitTestContext& hit_test,
                                    const NGInlineCursor& children,
                                    const PhysicalOffset& physical_offset);
  bool HitTestItemsChildren(const HitTestContext& hit_test,
                            const NGPhysicalBoxFragment& container,
                            const NGInlineCursor& children);
  bool HitTestFloatingChildren(const HitTestContext& hit_test,
                               const NGPhysicalContainerFragment& container,
                               const PhysicalOffset& accumulated_offset);
  bool HitTestFloatingChildItems(const HitTestContext& hit_test,
                                 const NGInlineCursor& children,
                                 const PhysicalOffset& accumulated_offset);

  // Hit tests a box fragment, which is a child of either |box_fragment_|, or
  // one of its child line box fragments.
  // @param physical_offset Physical offset of the given box fragment in the
  // paint layer.
  bool HitTestChildBoxFragment(const HitTestContext& hit_test,
                               const NGPhysicalBoxFragment& fragment,
                               const NGInlineBackwardCursor& cursor,
                               const PhysicalOffset& physical_offset);
  bool HitTestChildBoxItem(const HitTestContext& hit_test,
                           const NGPhysicalBoxFragment& container,
                           const NGFragmentItem& item,
                           const NGInlineBackwardCursor& cursor);

  // Hit tests the given text fragment.
  // @param physical_offset Physical offset of the text fragment in paint layer.
  bool HitTestTextFragment(const HitTestContext& hit_test,
                           const NGInlineBackwardCursor& cursor,
                           const PhysicalOffset& physical_offset);
  bool HitTestTextItem(const HitTestContext& hit_test,
                       const NGFragmentItem& text_item);

  // Hit tests the given line box fragment.
  // @param physical_offset Physical offset of the line box fragment in paint
  // layer.
  bool HitTestLineBoxFragment(const HitTestContext& hit_test,
                              const NGPhysicalLineBoxFragment& fragment,
                              const NGInlineBackwardCursor& cursor,
                              const PhysicalOffset& physical_offset);

  // Returns whether the hit test location is completely outside the border box,
  // which possibly has rounded corners.
  bool HitTestClippedOutByBorder(
      const HitTestLocation&,
      const PhysicalOffset& border_box_location) const;

  bool HitTestOverflowControl(const HitTestContext&,
                              PhysicalOffset accumulated_offset);

  const NGPhysicalBoxFragment& PhysicalFragment() const {
    return box_fragment_;
  }
  const DisplayItemClient& GetDisplayItemClient() const {
    return display_item_client_;
  }
  PhysicalRect SelfInkOverflow() const;
  PhysicalRect ContentsInkOverflow() const;

  const NGPhysicalBoxFragment& box_fragment_;
  const DisplayItemClient& display_item_client_;
  // If this box has inline children, either |paint_fragment_| or |items_| is
  // not null, depends on |LayoutNGFragmentItemEnabled|. TODO(kojii): Remove
  // |NGPaintFragment| once the transition is done. crbug.com/982194
  const NGPaintFragment* paint_fragment_;
  const NGFragmentItems* items_;
  const NGFragmentItem* box_item_ = nullptr;
  const NGInlineCursor* inline_box_cursor_ = nullptr;
};

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPhysicalBoxFragment& box,
    const DisplayItemClient& display_item_client,
    const NGPaintFragment* paint_fragment,
    const NGInlineCursor* inline_box_cursor,
    const NGFragmentItem* box_item)
    : BoxPainterBase(&box.GetDocument(), box.Style(), box.GeneratingNode()),
      box_fragment_(box),
      display_item_client_(display_item_client),
      paint_fragment_(paint_fragment),
      items_(box.Items()),
      box_item_(box_item),
      inline_box_cursor_(inline_box_cursor) {
  DCHECK(box.IsBox() || box.IsRenderedLegend());
  DCHECK_EQ(box.PostLayout(), &box);
#if DCHECK_IS_ON()
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    DCHECK(!paint_fragment_);
    if (inline_box_cursor_)
      DCHECK_EQ(inline_box_cursor_->Current().Item(), box_item_);
    if (box_item_)
      DCHECK_EQ(box_item_->BoxFragment(), &box);
    DCHECK_EQ(box.IsInlineBox(), !!inline_box_cursor_);
    DCHECK_EQ(box.IsInlineBox(), !!box_item_);
  } else {
    DCHECK(!inline_box_cursor_);
    DCHECK(!box_item_);
    if (paint_fragment)
      DCHECK_EQ(&paint_fragment->PhysicalFragment(), &box);
    if (box.IsInlineBox()) {
      DCHECK(paint_fragment_);
    } else if (box.IsInlineFormattingContext()) {
      // If no children, there maybe or may not be NGPaintFragment.
      // TODO(kojii): To be investigated if this correct or should be fixed.
      if (!box.Children().empty()) {
        if (!box.GetLayoutObject() ||
            !box.GetLayoutObject()->ChildPaintBlockedByDisplayLock()) {
          DCHECK(paint_fragment);
        }
      }
    } else {
      // We may not have |paint_fragment_| nor |box_item_|.
    }
  }
#endif
}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPhysicalBoxFragment& fragment)
    : NGBoxFragmentPainter(fragment,
                           *fragment.GetLayoutObject(),
                           /* paint_fragment */ nullptr,
                           /* inline_box_cursor */ nullptr,
                           /* box_item */ nullptr) {}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPhysicalBoxFragment& fragment,
    const NGPaintFragment* paint_fragment)
    : NGBoxFragmentPainter(
          fragment,
          paint_fragment
              ? *static_cast<const DisplayItemClient*>(paint_fragment)
              : *static_cast<const DisplayItemClient*>(
                    fragment.GetLayoutObject()),
          paint_fragment,
          /* inline_box_cursor */ nullptr,
          /* box_item */ nullptr) {}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPaintFragment& paint_fragment)
    : NGBoxFragmentPainter(
          To<NGPhysicalBoxFragment>(paint_fragment.PhysicalFragment()),
          paint_fragment,
          &paint_fragment) {}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGInlineCursor& inline_box_cursor,
    const NGFragmentItem& item,
    const NGPhysicalBoxFragment& fragment)
    : NGBoxFragmentPainter(fragment,
                           *item.GetDisplayItemClient(),
                           /* paint_fragment */ nullptr,
                           &inline_box_cursor,
                           &item) {
  DCHECK_EQ(item.BoxFragment(), &fragment);
  DCHECK(fragment.IsInlineBox());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_
