/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc.
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
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

struct PaintInfo;
class LineLayoutBox;
class NGBlockNode;
class WordMeasurement;

typedef HeapLinkedHashSet<Member<LayoutBox>> TrackedLayoutBoxLinkedHashSet;
typedef HeapHashMap<WeakMember<const LayoutBlock>,
                    Member<TrackedLayoutBoxLinkedHashSet>>
    TrackedDescendantsMap;
typedef HeapHashMap<WeakMember<const LayoutBox>, Member<LayoutBlock>>
    TrackedContainerMap;
typedef Vector<WordMeasurement, 64> WordMeasurements;

enum ContainingBlockState { kNewContainingBlock, kSameContainingBlock };

// LayoutBlock is the class that is used by any LayoutObject
// that is a containing block.
// http://www.w3.org/TR/CSS2/visuren.html#containing-block
// See also LayoutObject::ContainingBlock() that is the function
// used to get the containing block of a LayoutObject.
//
// CSS is inconsistent and allows inline elements (LayoutInline) to be
// containing blocks, even though they are not blocks. Our
// implementation is as confused with inlines. See e.g.
// LayoutObject::ContainingBlock() vs LayoutObject::Container().
//
// Containing blocks are a central concept for layout, in
// particular to the layout of out-of-flow positioned
// elements. They are used to determine the sizing as well
// as the positioning of the LayoutObjects.
//
// LayoutBlock is the class that handles out-of-flow positioned elements in
// Blink, in particular for layout (see LayoutPositionedObjects()). That's why
// LayoutBlock keeps track of them through |GetPositionedDescendantsMap()| (see
// layout_block.cc).
// Note that this is a design decision made in Blink that doesn't reflect CSS:
// CSS allows relatively positioned inlines (LayoutInline) to be containing
// blocks, but they don't have the logic to handle out-of-flow positioned
// objects. This induces some complexity around choosing an enclosing
// LayoutBlock (for inserting out-of-flow objects during layout) vs the CSS
// containing block (for sizing, invalidation).
//
//
// ***** WHO LAYS OUT OUT-OF-FLOW POSITIONED OBJECTS? *****
// A positioned object gets inserted into an enclosing LayoutBlock's positioned
// map. This is determined by LayoutObject::ContainingBlock().
//
//
// ***** HANDLING OUT-OF-FLOW POSITIONED OBJECTS *****
// Care should be taken to handle out-of-flow positioned objects during
// certain tree walks (e.g. Layout()). The rule is that anything that
// cares about containing blocks should skip the out-of-flow elements
// in the normal tree walk and do an optional follow-up pass for them
// using LayoutBlock::PositionedObjects().
// Not doing so will result in passing the wrong containing
// block as tree walks will always pass the parent as the
// containing block.
//
// Sample code of how to handle positioned objects in LayoutBlock:
//
// for (LayoutObject* child = FirstChild(); child; child = child->NextSibling())
// {
//     if (child->IsOutOfFlowPositioned())
//         continue;
//
//     // Handle normal flow children.
//     ...
// }
// for (LayoutBox* positioned_object : PositionedObjects()) {
//     // Handle out-of-flow positioned objects.
//     ...
// }
class CORE_EXPORT LayoutBlock : public LayoutBox {
 protected:
  explicit LayoutBlock(ContainerNode*);

 public:
  void Trace(Visitor*) const override;

  LayoutObject* FirstChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->FirstChild();
  }
  LayoutObject* LastChild() const {
    NOT_DESTROYED();
    DCHECK_EQ(Children(), VirtualChildren());
    return Children()->LastChild();
  }

  // If you have a LayoutBlock, use FirstChild or LastChild instead.
  void SlowFirstChild() const = delete;
  void SlowLastChild() const = delete;

  const LayoutObjectChildList* Children() const {
    NOT_DESTROYED();
    return &children_;
  }
  LayoutObjectChildList* Children() {
    NOT_DESTROYED();
    return &children_;
  }

  // These two functions are overridden for inline-block.
  LayoutUnit LineHeight(
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;
  bool UseLogicalBottomMarginEdgeForInlineBlockBaseline() const;

  LayoutUnit MinLineHeightForReplacedObject(bool is_first_line,
                                            LayoutUnit replaced_height) const;

  const char* GetName() const override;

 protected:
  // Insert a child correctly into the tree when |before_descendant| isn't a
  // direct child of |this|. This happens e.g. when there's an anonymous block
  // child of |this| and |before_descendant| has been reparented into that one.
  // Such things are invisible to the DOM, and addChild() is typically called
  // with the DOM tree (and not the layout tree) in mind.
  void AddChildBeforeDescendant(LayoutObject* new_child,
                                LayoutObject* before_descendant);

 public:
  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;

  virtual void UpdateBlockLayout(bool relayout_children);

  void InsertPositionedObject(LayoutBox*);
  static void RemovePositionedObject(LayoutBox*);
  void RemovePositionedObjects(LayoutObject*,
                               ContainingBlockState = kSameContainingBlock);

  TrackedLayoutBoxLinkedHashSet* PositionedObjects() const {
    NOT_DESTROYED();
    return UNLIKELY(HasPositionedObjects()) ? PositionedObjectsInternal()
                                            : nullptr;
  }
  bool HasPositionedObjects() const {
    NOT_DESTROYED();
    DCHECK(has_positioned_objects_ ? (PositionedObjectsInternal() &&
                                      !PositionedObjectsInternal()->empty())
                                   : !PositionedObjectsInternal());
    return has_positioned_objects_;
  }

  void AddPercentHeightDescendant(LayoutBox*);
  void RemovePercentHeightDescendant(LayoutBox*);
  bool HasPercentHeightDescendant(LayoutBox* o) const {
    NOT_DESTROYED();
    return HasPercentHeightDescendants() &&
           PercentHeightDescendantsInternal()->Contains(o);
  }

  TrackedLayoutBoxLinkedHashSet* PercentHeightDescendants() const {
    NOT_DESTROYED();
    return HasPercentHeightDescendants() ? PercentHeightDescendantsInternal()
                                         : nullptr;
  }
  bool HasPercentHeightDescendants() const {
    NOT_DESTROYED();
    DCHECK(has_percent_height_descendants_
               ? (PercentHeightDescendantsInternal() &&
                  !PercentHeightDescendantsInternal()->empty())
               : !PercentHeightDescendantsInternal());
    return has_percent_height_descendants_;
  }

  void AddSvgTextDescendant(LayoutBox& svg_text);
  void RemoveSvgTextDescendant(LayoutBox& svg_text);

  void NotifyScrollbarThicknessChanged() {
    NOT_DESTROYED();
    width_available_to_children_changed_ = true;
  }

  // Return true if this is the anonymous child wrapper of an NG fieldset
  // container. Such a wrapper holds all the fieldset contents. Only the
  // rendered legend is laid out on the outside, although the layout object
  // itself for the legend is still a child of this object.
  bool IsAnonymousNGFieldsetContentWrapper() const;

  void SetHasMarkupTruncation(bool b) {
    NOT_DESTROYED();
    has_markup_truncation_ = b;
  }
  bool HasMarkupTruncation() const {
    NOT_DESTROYED();
    return has_markup_truncation_;
  }

  void SetHasMarginBeforeQuirk(bool b) {
    NOT_DESTROYED();
    has_margin_before_quirk_ = b;
  }
  void SetHasMarginAfterQuirk(bool b) {
    NOT_DESTROYED();
    has_margin_after_quirk_ = b;
  }

  bool HasMarginBeforeQuirk() const {
    NOT_DESTROYED();
    return has_margin_before_quirk_;
  }
  bool HasMarginAfterQuirk() const {
    NOT_DESTROYED();
    return has_margin_after_quirk_;
  }

  bool HasMarginBeforeQuirk(const LayoutBox* child) const;
  bool HasMarginAfterQuirk(const LayoutBox* child) const;

  void MarkPositionedObjectsForLayout();

  LayoutUnit TextIndentOffset() const;

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  static LayoutBlock* CreateAnonymousWithParentAndDisplay(
      const LayoutObject*,
      EDisplay = EDisplay::kBlock);
  LayoutBlock* CreateAnonymousBlock(EDisplay display = EDisplay::kBlock) const {
    NOT_DESTROYED();
    return CreateAnonymousWithParentAndDisplay(this, display);
  }

  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override;

  // Accessors for logical width/height and margins in the containing block's
  // block-flow direction.
  LayoutUnit LogicalWidthForChild(const LayoutBox& child) const {
    NOT_DESTROYED();
    return LogicalWidthForChildSize(child.Size());
  }
  LayoutUnit LogicalWidthForChildSize(LayoutSize child_size) const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? child_size.Width() : child_size.Height();
  }
  LayoutUnit LogicalHeightForChild(const LayoutBox& child) const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? child.Size().Height()
                                     : child.Size().Width();
  }
  LayoutSize LogicalSizeForChild(const LayoutBox& child) const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? child.Size()
                                     : child.Size().TransposedSize();
  }
  LayoutUnit LogicalTopForChild(const LayoutBox& child) const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? child.Location().Y()
                                     : child.Location().X();
  }
  DISABLE_CFI_PERF LayoutUnit
  MarginBeforeForChild(const LayoutBoxModelObject& child) const {
    NOT_DESTROYED();
    return child.MarginBefore(Style());
  }
  DISABLE_CFI_PERF LayoutUnit
  MarginAfterForChild(const LayoutBoxModelObject& child) const {
    NOT_DESTROYED();
    return child.MarginAfter(Style());
  }
  DISABLE_CFI_PERF LayoutUnit
  MarginStartForChild(const LayoutBoxModelObject& child) const {
    NOT_DESTROYED();
    return child.MarginStart(Style());
  }
  LayoutUnit MarginEndForChild(const LayoutBoxModelObject& child) const {
    NOT_DESTROYED();
    return child.MarginEnd(Style());
  }
  void SetMarginStartForChild(LayoutBox& child, LayoutUnit value) const {
    NOT_DESTROYED();
    child.SetMarginStart(value, Style());
  }
  void SetMarginEndForChild(LayoutBox& child, LayoutUnit value) const {
    NOT_DESTROYED();
    child.SetMarginEnd(value, Style());
  }
  void SetMarginBeforeForChild(LayoutBox& child, LayoutUnit value) const {
    NOT_DESTROYED();
    child.SetMarginBefore(value, Style());
  }
  void SetMarginAfterForChild(LayoutBox& child, LayoutUnit value) const {
    NOT_DESTROYED();
    child.SetMarginAfter(value, Style());
  }
  LayoutUnit CollapsedMarginBeforeForChild(const LayoutBox& child) const;
  LayoutUnit CollapsedMarginAfterForChild(const LayoutBox& child) const;

  enum ScrollbarChangeContext { kStyleChange, kLayout };
  virtual void ScrollbarsChanged(bool horizontal_scrollbar_changed,
                                 bool vertical_scrollbar_changed,
                                 ScrollbarChangeContext = kLayout);

  LayoutUnit AvailableLogicalWidthForContent() const {
    NOT_DESTROYED();
    return (LogicalRightOffsetForContent() - LogicalLeftOffsetForContent())
        .ClampNegativeToZero();
  }
  DISABLE_CFI_PERF LayoutUnit LogicalLeftOffsetForContent() const {
    NOT_DESTROYED();
    return IsHorizontalWritingMode() ? ContentLeft() : ContentTop();
  }
  LayoutUnit LogicalRightOffsetForContent() const {
    NOT_DESTROYED();
    return LogicalLeftOffsetForContent() + AvailableLogicalWidth();
  }
  LayoutUnit StartOffsetForContent() const {
    NOT_DESTROYED();
    return StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForContent()
               : LogicalWidth() - LogicalRightOffsetForContent();
  }
  LayoutUnit EndOffsetForContent() const {
    NOT_DESTROYED();
    return !StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForContent()
               : LogicalWidth() - LogicalRightOffsetForContent();
  }

#if DCHECK_IS_ON()
  void CheckPositionedObjectsNeedLayout();
#endif

  // This method returns the size that percentage logical heights should
  // resolve against *if* this LayoutBlock is the containing block for the
  // percentage calculation.
  //
  // A version of this function without the above restriction, (that will walk
  // the ancestor chain in quirks mode), see:
  // LayoutBox::ContainingBlockLogicalHeightForPercentageResolution
  LayoutUnit AvailableLogicalHeightForPercentageComputation() const;
  bool HasDefiniteLogicalHeight() const;

 protected:
  RecalcLayoutOverflowResult RecalcPositionedDescendantsLayoutOverflow();
  bool RecalcSelfLayoutOverflow();
  void RecalcSelfVisualOverflow();

 public:
  virtual RecalcLayoutOverflowResult RecalcChildLayoutOverflow();
  RecalcLayoutOverflowResult RecalcLayoutOverflow() override;
  void RecalcChildVisualOverflow();
  void RecalcVisualOverflow() override;

  // An example explaining layout tree structure about first-line style:
  // <style>
  //   #enclosingFirstLineStyleBlock::first-line { ... }
  // </style>
  // <div id="enclosingFirstLineStyleBlock">
  //   <div>
  //     <div id="nearestInnerBlockWithFirstLine">
  //       [<span>]first line text[</span>]
  //     </div>
  //   </div>
  // </div>

  // Return the parent LayoutObject if it can contribute to our ::first-line
  // style.
  const LayoutBlock* FirstLineStyleParentBlock() const;

  // Returns this block or the nearest inner block containing the actual first
  // line.
  LayoutBlockFlow* NearestInnerBlockWithFirstLine();

 protected:
  void WillBeDestroyed() override;

  void DirtyForLayoutFromPercentageHeightDescendants(SubtreeLayoutScope&);

  void UpdateLayout() override;

  enum PositionedLayoutBehavior {
    kDefaultLayout,
    kLayoutOnlyFixedPositionedObjects,
    kForcedLayoutAfterContainingBlockMoved
  };

  virtual void LayoutPositionedObjects(
      bool relayout_children,
      PositionedLayoutBehavior = kDefaultLayout);
  void LayoutPositionedObject(LayoutBox*,
                              bool relayout_children,
                              PositionedLayoutBehavior info);
  void MarkFixedPositionObjectForLayoutIfNeeded(LayoutObject* child,
                                                SubtreeLayoutScope&);

 public:
  bool IsLegacyInitiatedOutOfFlowLayout() const {
    NOT_DESTROYED();
    return is_legacy_initiated_out_of_flow_layout_;
  }

  void SetIsLegacyInitiatedOutOfFlowLayout(bool b) {
    NOT_DESTROYED();
    is_legacy_initiated_out_of_flow_layout_ = b;
  }

 protected:
  LayoutUnit MarginIntrinsicLogicalWidthForChild(const LayoutBox& child) const;

  LayoutUnit BeforeMarginInLineDirection(LineDirectionMode) const;

 public:
  void Paint(const PaintInfo&) const override;
  virtual void PaintObject(const PaintInfo&,
                           const PhysicalOffset& paint_offset) const;
  virtual void PaintChildren(const PaintInfo&,
                             const PhysicalOffset& paint_offset) const;
  MinMaxSizes PreferredLogicalWidths() const override;

  virtual bool HasLineIfEmpty() const;
  // Returns baseline offset if we can get |SimpleFontData| from primary font.
  // Or returns no value if we can't get font data.
  absl::optional<LayoutUnit> BaselineForEmptyLine(
      LineDirectionMode line_direction) const;

 protected:
  virtual void AdjustInlineDirectionLineBounds(
      unsigned /* expansion_opportunity_count */,
      LayoutUnit& /* logical_left */,
      LayoutUnit& /* logical_width */) const {
    NOT_DESTROYED();
  }

  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;
  void ComputeChildPreferredLogicalWidths(
      LayoutObject& child,
      LayoutUnit& min_preferred_logical_width,
      LayoutUnit& max_preferred_logical_width) const;

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;
  absl::optional<LayoutUnit> FirstLineBoxBaselineOverride() const;
  absl::optional<LayoutUnit> InlineBlockBaselineOverride(
      LineDirectionMode) const;

  bool HitTestChildren(HitTestResult&,
                       const HitTestLocation&,
                       const PhysicalOffset& accumulated_offset,
                       HitTestPhase) override;

  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  bool RespectsCSSOverflow() const override;

  bool SimplifiedLayout();
  virtual void SimplifiedNormalFlowLayout();

 private:
  void AddLayoutOverflowFromPositionedObjects();
  void AddLayoutOverflowFromBlockChildren();

 protected:
  virtual void ComputeVisualOverflow(
      bool recompute_floats);
  virtual void ComputeLayoutOverflow(LayoutUnit old_client_after_edge,
                                     bool recompute_floats = false);

  virtual void AddLayoutOverflowFromChildren();
  void AddVisualOverflowFromChildren();
  virtual void AddVisualOverflowFromBlockChildren();

  void AddOutlineRects(Vector<PhysicalRect>&,
                       OutlineInfo*,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const override;

  void UpdateBlockChildDirtyBitsBeforeLayout(bool relayout_children,
                                             LayoutBox&);

  // TODO(jchaffraix): We should rename this function as inline-flex and
  // inline-grid as also covered.
  // Alternatively it should be removed as we clarify the meaning of
  // IsAtomicInlineLevel to imply isInline.
  bool IsInlineBlockOrInlineTable() const final {
    NOT_DESTROYED();
    return IsInline() && IsAtomicInlineLevel();
  }

  bool NeedsPreferredWidthsRecalculation() const override;

  bool IsInSelfHitTestingPhase(HitTestPhase phase) const final {
    NOT_DESTROYED();
    return phase == HitTestPhase::kSelfBlockBackground;
  }

  // Returns baseline offset of this block if is empty editable or having
  // CSS property "--internal-empty-line-height"fabricated", otherwise
  // returns |LayoutUnit(-1)|.
  LayoutUnit EmptyLineBaseline(LineDirectionMode line_direction) const;

 private:
  LayoutObjectChildList* VirtualChildren() final {
    NOT_DESTROYED();
    return Children();
  }
  const LayoutObjectChildList* VirtualChildren() const final {
    NOT_DESTROYED();
    return Children();
  }

  bool IsLayoutBlock() const final {
    NOT_DESTROYED();
    return true;
  }

  virtual void RemoveLeftoverAnonymousBlock(LayoutBlock* child);

  TrackedLayoutBoxLinkedHashSet* PositionedObjectsInternal() const;
  TrackedLayoutBoxLinkedHashSet* PercentHeightDescendantsInternal() const;

  // Returns true if the positioned movement-only layout succeeded.
  bool TryLayoutDoingPositionedMovementOnly();

  void ComputeBlockPreferredLogicalWidths(LayoutUnit& min_logical_width,
                                          LayoutUnit& max_logical_width) const;

 protected:
  void InvalidatePaint(const PaintInvalidatorContext&) const override;

  void ImageChanged(WrappedImagePtr, CanDeferInvalidation) override;

 private:
  LayoutRect LocalCaretRect(
      const InlineBox*,
      int caret_offset,
      LayoutUnit* extra_width_to_end_of_line = nullptr) const final;
  bool IsInlineBoxWrapperActuallyChild() const;

  Position PositionForBox(InlineBox*, bool start = true) const;

  // End helper functions and structs used by layoutBlockChildren.

  void RemoveFromGlobalMaps();
  bool WidthAvailableToChildrenHasChanged();

 protected:
  // Paginated content inside this block was laid out.
  // |logical_bottom_offset_after_pagination| is the logical bottom offset of
  // the child content after applying any forced or unforced breaks as needed.
  void PaginatedContentWasLaidOut(
      LayoutUnit logical_bottom_offset_after_pagination);

  // Adjust from painting offsets to the local coords of this layoutObject
  void OffsetForContents(LayoutPoint&) const;
  void OffsetForContents(PhysicalOffset&) const;

  PositionWithAffinity PositionForPointRespectingEditingBoundaries(
      LineLayoutBox child,
      const PhysicalOffset& point_in_parent_coordinates) const;
  PositionWithAffinity PositionForPointIfOutsideAtomicInlineLevel(
      const PhysicalOffset&) const;

  virtual bool UpdateLogicalWidthAndColumnWidth();

  LayoutObjectChildList children_;

  // Note these quirk values can't be put in LayoutBlockRareData since they are
  // set too frequently.
  unsigned has_margin_before_quirk_ : 1;
  unsigned has_margin_after_quirk_ : 1;
  unsigned has_markup_truncation_ : 1;
  unsigned width_available_to_children_changed_ : 1;
  unsigned height_available_to_children_changed_ : 1;
  // True if margin-before and margin-after are adjoining.
  unsigned is_self_collapsing_ : 1;
  unsigned descendants_with_floats_marked_for_layout_ : 1;

  unsigned has_positioned_objects_ : 1;
  unsigned has_percent_height_descendants_ : 1;
  unsigned has_svg_text_descendants_ : 1;

  // When an object ceases to establish a fragmentation context (e.g. the
  // LayoutView when we're no longer printing), we need a deep layout
  // afterwards, to clear all pagination struts. Likewise, when an object
  // becomes fragmented, we need to re-lay out the entire subtree. There might
  // be forced breaks somewhere in there that we suddenly have to pay attention
  // to, for all we know.
  unsigned pagination_state_changed_ : 1;

  // LayoutNG-only: This flag is true if an NG out of flow layout was
  // initiated by Legacy positioning code.
  unsigned is_legacy_initiated_out_of_flow_layout_ : 1;

  // FIXME: This is temporary as we move code that accesses block flow
  // member variables out of LayoutBlock and into LayoutBlockFlow.
  friend class LayoutBlockFlow;

  // This is necessary for now for interoperability between the old and new
  // layout code. Primarily for calling layoutPositionedObjects at the moment.
  friend class NGBlockNode;
};

template <>
struct DowncastTraits<LayoutBlock> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutBlock();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_H_
