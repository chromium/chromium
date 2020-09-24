/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
 *               All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INLINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INLINE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/line/inline_flow_box.h"
#include "third_party/blink/renderer/core/layout/line/line_box_list.h"

namespace blink {

class LayoutBlockFlow;
class NGInlineCursor;
class NGPaintFragment;

// LayoutInline is the LayoutObject associated with display: inline.
// This is called an "inline box" in CSS 2.1.
// http://www.w3.org/TR/CSS2/visuren.html#inline-boxes
//
// It is also the base class for content that behaves in similar way (like
// quotes and "display: ruby").
//
// Note that LayoutInline is always 'inline-level' but other LayoutObject
// can be 'inline-level', which is why it's stored as a boolean on LayoutObject
// (see LayoutObject::isInline()).
//
// For performance and memory consumption, this class ignores some inline-boxes
// during line layout because they don't impact layout (they still exist and are
// inserted into the layout tree). An example of this is
//             <span><span>Text</span></span>
// where the 2 spans have the same size as the inner text-node so they can be
// ignored for layout purpose, generating a single inline-box instead of 3.
// One downside of this optimization is that we have extra work to do when
// asking for bounding rects (see generateLineBoxRects).
// This optimization is called "culled inline" in the code.
//
// LayoutInlines are expected to be laid out by their containing
// LayoutBlockFlow. See LayoutBlockFlow::layoutInlineChildren.
//
//
// ***** CONTINUATIONS AND ANONYMOUS LAYOUTBLOCKFLOWS *****
// LayoutInline enforces the following invariant:
// "All in-flow children of an inline box are inline."
// When a non-inline child is inserted, LayoutInline::addChild splits the inline
// and potentially enclosing inlines too. It then wraps layout objects into
// anonymous block-flow containers. This creates complexity in the code as:
// - a DOM node can have several associated LayoutObjects (we don't currently
//   expose this information to the DOM code though).
// - more importantly, nodes that are parent/child in the DOM have no natural
//   relationship anymore (see example below).
// In order to do a correct tree walk over this synthetic tree, a single linked
// list is stored called *continuation*. See splitFlow() about how it is
// populated during LayoutInline split.
//
// Continuations can only be a LayoutInline or an anonymous LayoutBlockFlow.
// That's why continuations are handled by LayoutBoxModelObject (common class
// between the 2). See LayoutBoxModelObject::continuation and setContinuation.
//
// Let's take the following example:
//   <!DOCTYPE html>
//   <b>Bold inline.<div>Bold block.</div>More bold inlines.</b>
//
// The generated layout tree is:
//   LayoutBlockFlow {HTML}
//    LayoutBlockFlow {BODY}
//      LayoutBlockFlow (anonymous)
//        LayoutInline {B}
//          LayoutText {#text}
//            text run: "Bold inline."
//      LayoutBlockFlow (anonymous)
//        LayoutBlockFlow {DIV}
//          LayoutText {#text}
//            text run: "Bold block."
//      LayoutBlockFlow (anonymous)
//        LayoutInline {B}
//          LayoutText {#text}
//            text run: "More bold inlines."
//
// The insertion of the <div> inside the <b> forces the latter to be split
// into 2 LayoutInlines and the insertion of anonymous LayoutBlockFlows. The 2
// LayoutInlines are done so that we can apply the correct (bold) style to both
// sides of the <div>. The continuation chain starts with the first
// LayoutInline {B}, continues to the middle anonymous LayoutBlockFlow and
// finishes with the last LayoutInline {B}.
//
// Note that the middle anonymous LayoutBlockFlow duplicates the content.
// TODO(jchaffraix): Find out why we made the decision to always insert the
//                   anonymous LayoutBlockFlows.
//
// This section was inspired by an older article by Dave Hyatt:
// https://www.webkit.org/blog/115/webcore-rendering-ii-blocks-and-inlines/
class CORE_EXPORT LayoutInline : public LayoutBoxModelObject {
 public:
  explicit LayoutInline(Element*);

  ~LayoutInline() override;

  static LayoutInline* CreateAnonymous(Document*);

  LayoutObject* FirstChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

  // If you have a LayoutInline, use firstChild or lastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  Element* GetNode() const {
    return To<Element>(LayoutBoxModelObject::GetNode());
  }

  LayoutUnit MarginLeft() const final;
  LayoutUnit MarginRight() const final;
  LayoutUnit MarginTop() const final;
  LayoutUnit MarginBottom() const final;

  FloatRect LocalBoundingBoxRectForAccessibility() const final;

  PhysicalRect PhysicalLinesBoundingBox() const;
  PhysicalRect PhysicalVisualOverflowRect() const final;
  PhysicalRect ReferenceBoxForClipPath() const;

  InlineFlowBox* CreateAndAppendInlineFlowBox();

  void DirtyLineBoxes(bool full_layout);

  // LineBoxes() and FirstInlineFragment() are mutually exclusive,
  // depends on IsInLayoutNGInlineFormattingContext().
  const LineBoxList* LineBoxes() const {
    return IsInLayoutNGInlineFormattingContext() ? &LineBoxList::Empty()
                                                 : &line_boxes_;
  }
  LineBoxList* MutableLineBoxes();

  InlineFlowBox* FirstLineBox() const { return LineBoxes()->First(); }
  InlineFlowBox* LastLineBox() const { return LineBoxes()->Last(); }
  InlineBox* FirstLineBoxIncludingCulling() const {
    return AlwaysCreateLineBoxes() ? FirstLineBox()
                                   : CulledInlineFirstLineBox();
  }
  InlineBox* LastLineBoxIncludingCulling() const {
    return AlwaysCreateLineBoxes() ? LastLineBox() : CulledInlineLastLineBox();
  }

  bool HasInlineFragments() const final;
  NGPaintFragment* FirstInlineFragment() const final;
  void SetFirstInlineFragment(NGPaintFragment*) final;
  wtf_size_t FirstInlineFragmentItemIndex() const final;
  void ClearFirstInlineFragmentItemIndex() final;
  void SetFirstInlineFragmentItemIndex(wtf_size_t) final;

  LayoutBoxModelObject* VirtualContinuation() const final {
    return Continuation();
  }
  LayoutInline* InlineElementContinuation() const;

  PhysicalOffset OffsetForInFlowPositionedInline(const LayoutBox& child) const;

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const final;
  // The following methods are called from the container if it has already added
  // outline rects for line boxes and/or children of this LayoutInline.
  void AddOutlineRectsForChildrenAndContinuations(
      Vector<PhysicalRect>&,
      const PhysicalOffset& additional_offset,
      NGOutlineType) const;
  void AddOutlineRectsForContinuations(Vector<PhysicalRect>&,
                                       const PhysicalOffset& additional_offset,
                                       NGOutlineType) const;

  using LayoutBoxModelObject::Continuation;
  using LayoutBoxModelObject::SetContinuation;

  bool AlwaysCreateLineBoxes() const {
    return AlwaysCreateLineBoxesForLayoutInline() &&
           !IsInLayoutNGInlineFormattingContext();
  }
  void SetAlwaysCreateLineBoxes(bool always_create_line_boxes = true) {
    DCHECK(!IsInLayoutNGInlineFormattingContext());
    SetAlwaysCreateLineBoxesForLayoutInline(always_create_line_boxes);
  }
  void UpdateAlwaysCreateLineBoxes(bool full_layout);

  // True if this inline box should force creation of NGPhysicalBoxFragment.
  bool ShouldCreateBoxFragment() const {
    return AlwaysCreateLineBoxesForLayoutInline() &&
           IsInLayoutNGInlineFormattingContext();
  }
  void SetShouldCreateBoxFragment(bool value = true) {
    DCHECK(IsInLayoutNGInlineFormattingContext());
    SetAlwaysCreateLineBoxesForLayoutInline(value);
  }
  void UpdateShouldCreateBoxFragment();

  LayoutRect LocalCaretRect(const InlineBox*,
                            int,
                            LayoutUnit* extra_width_to_end_of_line) const final;

  // When this LayoutInline doesn't generate line boxes of its own, regenerate
  // the rects of the line boxes and hit test the rects.
  // In LayoutNG, |parent_fragment| is non-null, and limits the regenerated
  // rects to be from descendant fragments of |parent_fragment|.
  // In legacy, |parent_fragment| is always null, and all rects are regenerated.
  bool HitTestCulledInline(HitTestResult&,
                           const HitTestLocation&,
                           const PhysicalOffset& accumulated_offset,
                           const NGInlineCursor* parent_cursor = nullptr);

  PhysicalOffset FirstLineBoxTopLeft() const {
    return FirstLineBoxTopLeftInternal().value_or(PhysicalOffset());
  }

  void MapLocalToAncestor(const LayoutBoxModelObject* ancestor,
                          TransformState&,
                          MapCoordinatesFlags mode) const override;

  PhysicalRect AbsoluteBoundingBoxRectHandlingEmptyInline() const final;

  PhysicalRect VisualRectInDocument(
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  const char* GetName() const override { return "LayoutInline"; }

  PhysicalRect DebugRect() const override;

 protected:
  void WillBeDestroyed() override;

  void InLayoutNGInlineFormattingContextWillChange(bool) final;

  void DeleteLineBoxes();

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void InvalidateDisplayItemClients(PaintInvalidationReason) const override;

  void AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                            MapCoordinatesFlags mode = 0) const override;

  PhysicalOffset OffsetFromContainerInternal(
      const LayoutObject*,
      bool ignore_scroll_offset) const final;

 private:
  LayoutObjectChildList* VirtualChildren() final { return Children(); }
  const LayoutObjectChildList* VirtualChildren() const final {
    return Children();
  }
  const LayoutObjectChildList* Children() const { return &children_; }
  LayoutObjectChildList* Children() { return &children_; }

  bool IsLayoutInline() const final { return true; }

  // Compute the initial value of |ShouldCreateBoxFragment()| for this
  // LayoutInline. It maybe flipped to true later for other conditions.
  bool ComputeInitialShouldCreateBoxFragment() const;
  bool ComputeInitialShouldCreateBoxFragment(const ComputedStyle& style) const;

  PhysicalRect CulledInlineVisualOverflowBoundingBox() const;
  InlineBox* CulledInlineFirstLineBox() const;
  InlineBox* CulledInlineLastLineBox() const;

  // For PhysicalVisualOverflowRect() only, to get bounding box of visual
  // overflow of line boxes.
  PhysicalRect LinesVisualOverflowBoundingBox() const;

  // PhysicalRectCollector should be like a function:
  // void (const PhysicalRect&).
  template <typename PhysicalRectCollector>
  void CollectLineBoxRects(const PhysicalRectCollector&) const;
  template <typename PhysicalRectCollector>
  void CollectCulledLineBoxRects(const PhysicalRectCollector&) const;

  // FlippedRectCollector should be like a function:
  // void (const LayoutRect&);
  template <typename FlippedRectCollector>
  void CollectCulledLineBoxRectsInFlippedBlocksDirection(
      const FlippedRectCollector&,
      const LayoutInline* container) const;

  void AddChildToContinuation(LayoutObject* new_child,
                              LayoutObject* before_child);
  void AddChildIgnoringContinuation(LayoutObject* new_child,
                                    LayoutObject* before_child = nullptr) final;

  void MoveChildrenToIgnoringContinuation(LayoutInline* to,
                                          LayoutObject* start_child);

  void SplitInlines(LayoutBlockFlow* from_block,
                    LayoutBlockFlow* to_block,
                    LayoutBlockFlow* middle_block,
                    LayoutObject* before_child,
                    LayoutBoxModelObject* old_cont);
  void SplitFlow(LayoutObject* before_child,
                 LayoutBlockFlow* new_block_box,
                 LayoutObject* new_child,
                 LayoutBoxModelObject* old_cont);

  // Create an anoymous block for block children of this inline.
  LayoutBlockFlow* CreateAnonymousContainerForBlockChildren();

  void UpdateLayout() final { NOTREACHED(); }  // Do nothing for layout()

  void Paint(const PaintInfo&) const final;

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset& accumulated_offset,
                   HitTestAction) final;

  PaintLayerType LayerTypeRequired() const override;

  LayoutUnit OffsetLeft(const Element*) const final;
  LayoutUnit OffsetTop(const Element*) const final;
  LayoutUnit OffsetWidth() const final {
    return PhysicalLinesBoundingBox().Width();
  }
  LayoutUnit OffsetHeight() const final {
    return PhysicalLinesBoundingBox().Height();
  }

  // This method differs from VisualOverflowRect() in that
  // 1. it doesn't include the rects for culled inline boxes, which aren't
  //    necessary for paint invalidation;
  // 2. it is in physical coordinates.
  PhysicalRect LocalVisualRectIgnoringVisibility() const override;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const final;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  IntRect BorderBoundingBox() const final {
    IntRect bounding_box = EnclosingIntRect(PhysicalLinesBoundingBox());
    return IntRect(0, 0, bounding_box.Width(), bounding_box.Height());
  }

  virtual InlineFlowBox* CreateInlineFlowBox();  // Subclassed by SVG and Ruby

  void DirtyLinesFromChangedChild(LayoutObject*, MarkingBehavior) final;

  // TODO(leviw): This should probably be an int. We don't snap equivalent lines
  // to different heights.
  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const final;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const final;

  void ChildBecameNonInline(LayoutObject* child) final;

  void UpdateHitTestResult(HitTestResult&, const PhysicalOffset&) const final;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) final;

  void AddAnnotatedRegions(Vector<AnnotatedRegionValue>&) final;

  void UpdateFromStyle() final;
  bool AnonymousHasStylePropagationOverride() final { return true; }

  LayoutInline* Clone() const;

  LayoutBoxModelObject* ContinuationBefore(LayoutObject* before_child);

  base::Optional<PhysicalOffset> FirstLineBoxTopLeftInternal() const;
  PhysicalOffset AnchorPhysicalLocation() const;

  LayoutObjectChildList children_;

  union {
    // All of the line boxes created for this inline flow. For example,
    // <i>Hello<br>world.</i> will have two <i> line boxes.
    // Valid only when !IsInLayoutNGInlineFormattingContext().
    LineBoxList line_boxes_;
    // The first fragment of inline boxes associated with this object.
    // Valid only when IsInLayoutNGInlineFormattingContext().
    NGPaintFragment* first_paint_fragment_;
    // The index of the first fragment item associated with this object in
    // |NGFragmentItems::Items()|. Zero means there are no such item.
    // Valid only when IsInLayoutNGInlineFormattingContext().
    wtf_size_t first_fragment_item_index_;
  };
};

inline LineBoxList* LayoutInline::MutableLineBoxes() {
  CHECK(!IsInLayoutNGInlineFormattingContext());
  return &line_boxes_;
}

inline NGPaintFragment* LayoutInline::FirstInlineFragment() const {
  if (!IsInLayoutNGInlineFormattingContext())
    return nullptr;
  // TODO(yosin): Once we replace all usage of |FirstInlineFragment()| to
  // |NGInlineCursor|, we should change this to |DCHECK()|.
  DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  if (RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
    return nullptr;
  return first_paint_fragment_;
}

inline wtf_size_t LayoutInline::FirstInlineFragmentItemIndex() const {
  if (!IsInLayoutNGInlineFormattingContext())
    return 0u;
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  return first_fragment_item_index_;
}

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutInline, IsLayoutInline());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_INLINE_H_
