// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/background_bleed_avoidance.h"
#include "third_party/blink/renderer/core/layout/hit_test_phase.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/box_painter_base.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
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
class CORE_EXPORT NGBoxFragmentPainter : public BoxPainterBase {
  STACK_ALLOCATED();

 public:
  NGBoxFragmentPainter(const NGPhysicalBoxFragment&);
  // Construct for an inline box.
  NGBoxFragmentPainter(const NGInlineCursor& inline_box_cursor,
                       const NGFragmentItem& item,
                       const NGPhysicalBoxFragment& fragment,
                       NGInlinePaintContext* inline_context);

  void Paint(const PaintInfo&);
  // Routes single PaintPhase to actual painters, and traverses children.
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
                   HitTestPhase);
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& hit_test_location,
                   const PhysicalOffset& physical_offset,
                   const PhysicalOffset& inline_root_offset,
                   HitTestPhase);

  bool HitTestAllPhases(HitTestResult&,
                        const HitTestLocation&,
                        const PhysicalOffset& accumulated_offset);

  void PaintBoxDecorationBackgroundWithRectImpl(const PaintInfo&,
                                                const PhysicalRect&,
                                                const BoxDecorationData&);

  gfx::Rect VisualRect(const PhysicalOffset& paint_offset);

 protected:
  NGPhysicalBoxStrut ComputeBorders() const override;
  NGPhysicalBoxStrut ComputePadding() const override;
  BoxPainterBase::FillLayerInfo GetFillLayerInfo(
      const Color&,
      const FillLayer&,
      BackgroundBleedAvoidance,
      bool is_painting_background_in_contents_space) const override;

  void PaintTextClipMask(const PaintInfo&,
                         const gfx::Rect& mask_rect,
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
                       const NGInlineCursor* inline_box_cursor = nullptr,
                       const NGFragmentItem* box_item = nullptr,
                       NGInlinePaintContext* inline_context = nullptr);

  enum MoveTo { kDontSkipChildren, kSkipChildren };
  bool ShouldPaint(const ScopedPaintState&) const;

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset,
                                    bool suppress_box_decoration_background);

  // |visual_rect| is for the drawing display item, covering overflowing box
  // shadows and border image outsets. |paint_rect| is the border box rect in
  // paint coordinates.
  void PaintBoxDecorationBackgroundWithRect(const PaintInfo&,
                                            const gfx::Rect& visual_rect,
                                            const PhysicalRect& paint_rect,
                                            const DisplayItemClient&);
  void PaintCompositeBackgroundAttachmentFixed(const PaintInfo&,
                                               const DisplayItemClient&,
                                               const BoxDecorationData&);
  void PaintBoxDecorationBackgroundWithDecorationData(
      const PaintInfo&,
      const gfx::Rect& visual_rect,
      const PhysicalRect& paint_rect,
      const DisplayItemClient&,
      DisplayItem::Type,
      const BoxDecorationData&);

  void PaintBoxDecorationBackgroundForBlockInInline(
      NGInlineCursor* children,
      const PaintInfo&,
      const PhysicalOffset& paint_offset);

  void PaintColumnRules(const PaintInfo&, const PhysicalOffset& paint_offset);

  void PaintInternal(const PaintInfo&);
  void PaintAllPhasesAtomically(const PaintInfo&);
  void PaintBlockChildren(const PaintInfo&, PhysicalOffset);
  void PaintBlockChild(const NGLink& child,
                       const PaintInfo& paint_info,
                       const PaintInfo& paint_info_for_descendants,
                       PhysicalOffset paint_offset);
  void PaintInlineItems(const PaintInfo&,
                        const PhysicalOffset& paint_offset,
                        const PhysicalOffset& parent_offset,
                        NGInlineCursor* cursor);
  void PaintLineBoxes(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintLineBoxChildItems(NGInlineCursor* children,
                              const PaintInfo&,
                              const PhysicalOffset& paint_offset);
  void PaintLineBox(const NGPhysicalFragment& line_box_fragment,
                    const DisplayItemClient& display_item_client,
                    const NGFragmentItem& line_box_item,
                    const PaintInfo&,
                    const PhysicalOffset& paint_offset);
  void PaintBackplate(NGInlineCursor* descendants,
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
  void PaintFloatingItems(const PaintInfo& paint_info, NGInlineCursor* cursor);
  void PaintFloatingChildren(const NGPhysicalFragment&,
                             const PaintInfo& paint_info);
  void PaintFloats(const PaintInfo&);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);
  void PaintBackground(const PaintInfo&,
                       const PhysicalRect&,
                       const Color& background_color,
                       BackgroundBleedAvoidance = kBackgroundBleedNone);
  void PaintCaretsIfNeeded(const ScopedPaintState&,
                           const PaintInfo&,
                           const PhysicalOffset& paint_offset);
  bool PaintOverflowControls(const PaintInfo&,
                             const PhysicalOffset& paint_offset);

  NGInlinePaintContext& EnsureInlineContext();

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
    // Add |node| to |HitTestResult|. Returns true if the hit-testing should
    // stop.
    // T is PhysicalRect or gfx::QuadF.
    template <typename T>
    bool AddNodeToResult(Node* node,
                         const NGPhysicalBoxFragment* box_fragment,
                         const T& bounds_rect,
                         const PhysicalOffset& offset) const;
    // Same as |AddNodeToResult|, except that |offset| is in the content
    // coordinate system rather than the container coordinate system. They
    // differ when |container| is a scroll container.
    // T is PhysicalRect or gfx::QuadF.
    template <typename T>
    bool AddNodeToResultWithContentOffset(
        Node* node,
        const NGPhysicalBoxFragment& container,
        const T& bounds_rect,
        PhysicalOffset offset) const;

    HitTestPhase phase;
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
                            HitTestPhase);
  bool HitTestItemsChildren(const HitTestContext& hit_test,
                            const NGPhysicalBoxFragment& container,
                            const NGInlineCursor& children);
  bool HitTestFloatingChildren(const HitTestContext& hit_test,
                               const NGPhysicalFragment& container,
                               const PhysicalOffset& accumulated_offset);
  bool HitTestFloatingChildItems(const HitTestContext& hit_test,
                                 const NGInlineCursor& children,
                                 const PhysicalOffset& accumulated_offset);

  // Hit tests a child inline box fragment.
  // @param physical_offset Physical offset of the given box fragment in the
  // paint layer.
  bool HitTestInlineChildBoxFragment(const HitTestContext& hit_test,
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
                       const NGFragmentItem& text_item,
                       const NGInlineBackwardCursor& cursor);

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

  bool UpdateHitTestResultForView(const PhysicalRect& bounds_rect,
                                  const HitTestContext& hit_test) const;

  const NGPhysicalBoxFragment& PhysicalFragment() const {
    return box_fragment_;
  }
  const DisplayItemClient& GetDisplayItemClient() const {
    return display_item_client_;
  }
  PhysicalRect InkOverflowIncludingFilters() const;

  static bool ShouldHitTestCulledInlineAncestors(const HitTestContext& hit_test,
                                                 const NGFragmentItem& item);

  const NGPhysicalBoxFragment& box_fragment_;
  const DisplayItemClient& display_item_client_;
  const NGFragmentItems* items_ = nullptr;
  const NGFragmentItem* box_item_ = nullptr;
  const NGInlineCursor* inline_box_cursor_ = nullptr;
  NGInlinePaintContext* inline_context_ = nullptr;
  absl::optional<NGInlinePaintContext> inline_context_storage_;
};

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPhysicalBoxFragment& box,
    const DisplayItemClient& display_item_client,
    const NGInlineCursor* inline_box_cursor,
    const NGFragmentItem* box_item,
    NGInlinePaintContext* inline_context)
    : BoxPainterBase(&box.GetDocument(), box.Style(), box.GetNode()),
      box_fragment_(box),
      display_item_client_(display_item_client),
      items_(box.Items()),
      box_item_(box_item),
      inline_box_cursor_(inline_box_cursor),
      inline_context_(inline_context) {
  DCHECK(box.IsBox() || box.IsRenderedLegend());
  DCHECK_EQ(box.PostLayout(), &box);
#if DCHECK_IS_ON()
  if (inline_box_cursor_)
    DCHECK_EQ(inline_box_cursor_->Current().Item(), box_item_);
  if (box_item_)
    DCHECK_EQ(box_item_->BoxFragment(), &box);
#endif
}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGPhysicalBoxFragment& fragment)
    : NGBoxFragmentPainter(fragment,
                           *fragment.GetLayoutObject(),
                           /* inline_box_cursor */ nullptr,
                           /* box_item */ nullptr,
                           /* inline_context */ nullptr) {}

inline NGBoxFragmentPainter::NGBoxFragmentPainter(
    const NGInlineCursor& inline_box_cursor,
    const NGFragmentItem& item,
    const NGPhysicalBoxFragment& fragment,
    NGInlinePaintContext* inline_context)
    : NGBoxFragmentPainter(fragment,
                           *item.GetDisplayItemClient(),
                           &inline_box_cursor,
                           &item,
                           inline_context) {
  DCHECK_EQ(item.BoxFragment(), &fragment);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_BOX_FRAGMENT_PAINTER_H_
