/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2013 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_FLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_FLOW_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/floating_objects.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/line/line_box_list.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/line/trailing_objects.h"

namespace blink {

template <class Run>
class BidiRunList;
class BlockChildrenLayoutInfo;
class LayoutInline;
class LineInfo;
class LineLayoutState;
class LineWidth;
class LayoutMultiColumnFlowThread;
class LayoutMultiColumnSpannerPlaceholder;
class LayoutRubyRun;
class MarginInfo;
class NGBlockBreakToken;
class NGFragmentItems;
class NGOffsetMapping;
class NGPhysicalContainerFragment;
class NGPhysicalFragment;

struct NGInlineNodeData;

enum IndentTextOrNot { kDoNotIndentText, kIndentText };

// LayoutBlockFlow is the class that implements a block container in CSS 2.1.
// http://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// LayoutBlockFlows are the only LayoutObject allowed to own floating objects
// (aka floats): http://www.w3.org/TR/CSS21/visuren.html#floats .
//
// Floats are inserted into |m_floatingObjects| (see FloatingObjects for more
// information on how floats are modelled) during layout. This happens either as
// part of laying out blocks (layoutBlockChildren) or line layout (LineBreaker
// class). This is because floats can be part of an inline or a block context.
//
// An interesting feature of floats is that they can intrude into the next
// block(s). This means that |m_floatingObjects| can potentially contain
// pointers to a previous sibling LayoutBlockFlow's float.
//
// LayoutBlockFlow is also the only LayoutObject to own a line box tree and
// perform inline layout. See LayoutBlockFlowLine.cpp for these parts.
//
// TODO(jchaffraix): We need some float and line box expert to expand on this.
//
// LayoutBlockFlow enforces the following invariant:
//
// All in-flow children (ie excluding floating and out-of-flow positioned) are
// either all blocks or all inline boxes.
//
// This is suggested by CSS to correctly the layout mixed inlines and blocks
// lines (http://www.w3.org/TR/CSS21/visuren.html#anonymous-block-level). See
// LayoutBlock::addChild about how the invariant is enforced.
class CORE_EXPORT LayoutBlockFlow : public LayoutBlock {
 public:
  explicit LayoutBlockFlow(ContainerNode*);
  ~LayoutBlockFlow() override;

  static LayoutBlockFlow* CreateAnonymous(Document*,
                                          scoped_refptr<ComputedStyle>,
                                          LegacyLayout);

  bool IsLayoutBlockFlow() const final { return true; }

  void UpdateBlockLayout(bool relayout_children) override;

  void ComputeVisualOverflow(bool recompute_floats) override;
  void ComputeLayoutOverflow(LayoutUnit old_client_after_edge,
                             bool recompute_floats = false) override;

  void DeleteLineBoxTree();

  bool CanContainFirstFormattedLine() const;

  LayoutUnit AvailableLogicalWidthForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return (LogicalRightOffsetForLine(position, indent_text, logical_height) -
            LogicalLeftOffsetForLine(position, indent_text, logical_height))
        .ClampNegativeToZero();
  }
  LayoutUnit LogicalRightOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return LogicalRightOffsetForLine(position, LogicalRightOffsetForContent(),
                                     indent_text, logical_height);
  }
  LayoutUnit LogicalLeftOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return LogicalLeftOffsetForLine(position, LogicalLeftOffsetForContent(),
                                    indent_text, logical_height);
  }
  LayoutUnit StartOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    return StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForLine(position, indent_text, logical_height)
               : LogicalWidth() - LogicalRightOffsetForLine(
                                      position, indent_text, logical_height);
  }

  LayoutUnit AvailableLogicalWidthForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    return (LogicalRightOffsetForAvoidingFloats(position, logical_height) -
            LogicalLeftOffsetForAvoidingFloats(position, logical_height))
        .ClampNegativeToZero();
  }
  LayoutUnit LogicalLeftOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    return LogicalLeftFloatOffsetForAvoidingFloats(
        position, LogicalLeftOffsetForContent(), logical_height);
  }
  LayoutUnit LogicalRightOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    return LogicalRightFloatOffsetForAvoidingFloats(
        position, LogicalRightOffsetForContent(), logical_height);
  }
  LayoutUnit StartOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    return StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForAvoidingFloats(position, logical_height)
               : LogicalWidth() - LogicalRightOffsetForAvoidingFloats(
                                      position, logical_height);
  }
  LayoutUnit EndOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    return !StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForAvoidingFloats(position, logical_height)
               : LogicalWidth() - LogicalRightOffsetForAvoidingFloats(
                                      position, logical_height);
  }

  const LineBoxList& LineBoxes() const { return line_boxes_; }
  LineBoxList* LineBoxes() { return &line_boxes_; }
  InlineFlowBox* FirstLineBox() const { return line_boxes_.First(); }
  InlineFlowBox* LastLineBox() const { return line_boxes_.Last(); }
  RootInlineBox* FirstRootBox() const {
    return static_cast<RootInlineBox*>(FirstLineBox());
  }
  RootInlineBox* LastRootBox() const {
    return static_cast<RootInlineBox*>(LastLineBox());
  }

  RootInlineBox* CreateAndAppendRootInlineBox();
  RootInlineBox* ConstructLine(BidiRunList<BidiRun>&, const LineInfo&);

  // Return the number of lines in *this* block flow. Does not recurse into
  // block flow children.
  // Will start counting from the first line, and stop counting right after
  // |stopRootInlineBox|, if specified.
  int LineCount(const RootInlineBox* stop_root_inline_box = nullptr) const;

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  void RemoveFloatingObjectsFromDescendants();
  void MarkAllDescendantsWithFloatsForLayout(
      LayoutBox* float_to_remove = nullptr,
      bool in_layout = true);
  void MarkSiblingsWithFloatsForLayout(LayoutBox* float_to_remove = nullptr);

  bool ContainsFloats() const {
    return floating_objects_ && !floating_objects_->Set().IsEmpty();
  }
  bool ContainsFloat(LayoutBox*) const;

  void RemoveFloatingObjects();

  LayoutBoxModelObject* VirtualContinuation() const final {
    return Continuation();
  }
  bool IsAnonymousBlockContinuation() const {
    return Continuation() && IsAnonymousBlock();
  }

  using LayoutBoxModelObject::Continuation;
  using LayoutBoxModelObject::SetContinuation;

  LayoutInline* InlineElementContinuation() const;

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  void MoveAllChildrenIncludingFloatsTo(LayoutBlock* to_block,
                                        bool full_remove_insert);

  void ChildBecameFloatingOrOutOfFlow(LayoutBox* child);
  void CollapseAnonymousBlockChild(LayoutBlockFlow* child);

  bool GeneratesLineBoxesForInlineChild(LayoutObject*);

  LayoutUnit LogicalTopForFloat(const FloatingObject& floating_object) const {
    return IsHorizontalWritingMode() ? floating_object.Y()
                                     : floating_object.X();
  }
  LayoutUnit LogicalBottomForFloat(
      const FloatingObject& floating_object) const {
    return IsHorizontalWritingMode() ? floating_object.MaxY()
                                     : floating_object.MaxX();
  }
  LayoutUnit LogicalLeftForFloat(const FloatingObject& floating_object) const {
    return IsHorizontalWritingMode() ? floating_object.X()
                                     : floating_object.Y();
  }
  LayoutUnit LogicalRightForFloat(const FloatingObject& floating_object) const {
    return IsHorizontalWritingMode() ? floating_object.MaxX()
                                     : floating_object.MaxY();
  }
  LayoutUnit LogicalWidthForFloat(const FloatingObject& floating_object) const {
    return IsHorizontalWritingMode() ? floating_object.Width()
                                     : floating_object.Height();
  }

  void SetLogicalTopForFloat(FloatingObject& floating_object,
                             LayoutUnit logical_top) {
    if (IsHorizontalWritingMode())
      floating_object.SetY(logical_top);
    else
      floating_object.SetX(logical_top);
  }
  void SetLogicalLeftForFloat(FloatingObject& floating_object,
                              LayoutUnit logical_left) {
    if (IsHorizontalWritingMode())
      floating_object.SetX(logical_left);
    else
      floating_object.SetY(logical_left);
  }
  void SetLogicalHeightForFloat(FloatingObject& floating_object,
                                LayoutUnit logical_height) {
    if (IsHorizontalWritingMode())
      floating_object.SetHeight(logical_height);
    else
      floating_object.SetWidth(logical_height);
  }
  void SetLogicalWidthForFloat(FloatingObject& floating_object,
                               LayoutUnit logical_width) {
    if (IsHorizontalWritingMode())
      floating_object.SetWidth(logical_width);
    else
      floating_object.SetHeight(logical_width);
  }

  LayoutUnit StartAlignedOffsetForLine(LayoutUnit position, IndentTextOrNot);

  void SetStaticInlinePositionForChild(LayoutBox&, LayoutUnit inline_position);
  void UpdateStaticInlinePositionForChild(LayoutBox&,
                                          LayoutUnit logical_top,
                                          IndentTextOrNot = kDoNotIndentText);

  static bool ShouldSkipCreatingRunsForObject(LineLayoutItem obj) {
    return obj.IsFloating() || (obj.IsOutOfFlowPositioned() &&
                                !obj.StyleRef().IsOriginalDisplayInlineType() &&
                                !obj.Container().IsLayoutInline());
  }

  LayoutMultiColumnFlowThread* MultiColumnFlowThread() const {
    return rare_data_ ? rare_data_->multi_column_flow_thread_ : nullptr;
  }
  void ResetMultiColumnFlowThread() {
    if (rare_data_)
      rare_data_->multi_column_flow_thread_ = nullptr;
  }

  void AddVisualOverflowFromInlineChildren();

  void AddLayoutOverflowFromInlineChildren();

  // FIXME: This should be const to avoid a const_cast, but can modify child
  // dirty bits and LayoutTextCombine.
  void ComputeInlinePreferredLogicalWidths(LayoutUnit& min_logical_width,
                                           LayoutUnit& max_logical_width);

  bool AllowsPaginationStrut() const;
  // Pagination strut caused by the first line or child block inside this
  // block-level object.
  //
  // When the first piece of content (first child block or line) inside an
  // object wants to insert a soft page or column break, rather than setting a
  // pagination strut on itself it normally propagates the strut to its
  // containing block (|this|), as long as our implementation can handle it.
  // The idea is that we want to push the entire object to the next page or
  // column along with the child content that caused the break, instead of
  // leaving unusable space at the beginning of the object at the end of one
  // column or page and just push the first line or block to the next column or
  // page. That would waste space in the container for no good reason, and it
  // would also be a spec violation, since there is no break opportunity defined
  // between the content logical top of an object and its first child or line
  // (only *between* blocks or lines).
  LayoutUnit PaginationStrutPropagatedFromChild() const {
    return rare_data_ ? rare_data_->pagination_strut_propagated_from_child_
                      : LayoutUnit();
  }
  void SetPaginationStrutPropagatedFromChild(LayoutUnit);

  LayoutUnit FirstForcedBreakOffset() const {
    if (!rare_data_)
      return LayoutUnit();
    return rare_data_->first_forced_break_offset_;
  }
  void SetFirstForcedBreakOffset(LayoutUnit);

  const AtomicString StartPageName() const final;
  const AtomicString EndPageName() const final;

  void PositionSpannerDescendant(LayoutMultiColumnSpannerPlaceholder& child);

  bool CreatesNewFormattingContext() const override;

  using LayoutBoxModelObject::MoveChildrenTo;
  void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                      LayoutObject* start_child,
                      LayoutObject* end_child,
                      LayoutObject* before_child,
                      bool full_remove_insert = false) override;

  LayoutUnit XPositionForFloatIncludingMargin(
      const FloatingObject& child) const {
    LayoutUnit scrollbar_adjustment(OriginAdjustmentForScrollbars().Width());
    if (IsHorizontalWritingMode()) {
      return child.X() + child.GetLayoutObject()->MarginLeft() +
             scrollbar_adjustment;
    }
    return child.X() + MarginBeforeForChild(*child.GetLayoutObject());
  }

  DISABLE_CFI_PERF
  LayoutUnit YPositionForFloatIncludingMargin(
      const FloatingObject& child) const {
    if (IsHorizontalWritingMode())
      return child.Y() + MarginBeforeForChild(*child.GetLayoutObject());

    return child.Y() + child.GetLayoutObject()->MarginTop();
  }

  LayoutPoint FlipFloatForWritingModeForChild(const FloatingObject&,
                                              const LayoutPoint&) const;

  const char* GetName() const override { return "LayoutBlockFlow"; }

  FloatingObject* InsertFloatingObject(LayoutBox&);

  // Return the last placed float. If |iterator| is non-null, it will be set to
  // the float right after said float.
  FloatingObject* LastPlacedFloat(
      FloatingObjectSetIterator* iterator = nullptr) const;

  // Position and lay out all floats that have not yet been positioned.
  //
  // This will mark them as "placed", which means that they have found their
  // final location in this layout pass.
  //
  // |logicalTopMarginEdge| is the minimum logical top for the floats. The
  // final logical top of the floats will also be affected by clearance and
  // space available after having positioned earlier floats.
  //
  // Returns true if and only if it has placed any floats.
  bool PlaceNewFloats(LayoutUnit logical_top_margin_edge, LineWidth* = nullptr);

  // Position and lay out the float, if it needs layout.
  // |logicalTopMarginEdge| is the minimum logical top offset for the float.
  // The value returned is the minimum logical top offset for subsequent
  // floats.
  LayoutUnit PositionAndLayoutFloat(FloatingObject&,
                                    LayoutUnit logical_top_margin_edge);

  LayoutUnit NextFloatLogicalBottomBelow(LayoutUnit) const;
  LayoutUnit NextFloatLogicalBottomBelowForBlock(LayoutUnit) const;

  FloatingObject* LastFloatFromPreviousLine() const {
    return ContainsFloats() ? floating_objects_->Set().back().get() : nullptr;
  }

  void SetShouldDoFullPaintInvalidationForFirstLine();

  void SimplifiedNormalFlowInlineLayout();
  bool RecalcInlineChildrenLayoutOverflow();
  void RecalcInlineChildrenVisualOverflow();

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;
  PositionWithAffinity PositionForPoint(const LayoutObject& offset_parent,
                                        const PhysicalOffset& offset) const;
  bool ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom() const;

  LayoutUnit LowestFloatLogicalBottom(EClear = EClear::kBoth) const;

  bool HasOverhangingFloats() const {
    return Parent() && ContainsFloats() &&
           LowestFloatLogicalBottom() > LogicalHeight();
  }
  bool IsOverhangingFloat(const FloatingObject& float_object) const {
    return LogicalBottomForFloat(float_object) > LogicalHeight();
  }

  LayoutUnit LogicalHeightWithVisibleOverflow() const final;

  void SetIsSelfCollapsingFromNG(bool is_self_collapsing) {
    is_self_collapsing_ = is_self_collapsing;
  }

  // These functions are only public so we can call it from NGBlockNode while
  // we're still working on LayoutNG.
  void AddVisualOverflowFromFloats();
  void AddVisualOverflowFromFloats(const NGPhysicalContainerFragment& fragment);
  void AddLayoutOverflowFromFloats();

  virtual NGInlineNodeData* TakeNGInlineNodeData() { return nullptr; }
  virtual NGInlineNodeData* GetNGInlineNodeData() const { return nullptr; }
  virtual void ResetNGInlineNodeData() {}
  virtual void ClearNGInlineNodeData() {}
  virtual bool HasNGInlineNodeData() const { return false; }
  virtual void WillCollectInlines() {}
  virtual void SetPaintFragment(const NGBlockBreakToken*,
                                scoped_refptr<const NGPhysicalFragment>);
  const NGFragmentItems* FragmentItems() const;

#if DCHECK_IS_ON()
  void ShowLineTreeAndMark(const InlineBox* = nullptr,
                           const char* = nullptr,
                           const InlineBox* = nullptr,
                           const char* = nullptr,
                           const LayoutObject* = nullptr) const;
#endif

 protected:
  void RebuildFloatsFromIntruding();
  void LayoutInlineChildren(bool relayout_children, LayoutUnit after_edge);
  void AddLowestFloatFromChildren(LayoutBlockFlow*);

  void CreateFloatingObjects();

  void WillBeDestroyed() override;
  void StyleWillChange(StyleDifference,
                       const ComputedStyle& new_style) override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void UpdateBlockChildDirtyBitsBeforeLayout(bool relayout_children,
                                             LayoutBox&);
  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const override;
  void AbsoluteQuadsForSelf(Vector<FloatQuad>& quads,
                            MapCoordinatesFlags mode = 0) const override;
  LayoutObject* HoverAncestor() const final;

  LayoutUnit LogicalRightOffsetForLine(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      IndentTextOrNot apply_text_indent,
      LayoutUnit logical_height = LayoutUnit()) const {
    return AdjustLogicalRightOffsetForLine(
        LogicalRightFloatOffsetForLine(logical_top, fixed_offset,
                                       logical_height),
        apply_text_indent);
  }
  LayoutUnit LogicalLeftOffsetForLine(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      IndentTextOrNot apply_text_indent,
      LayoutUnit logical_height = LayoutUnit()) const {
    return AdjustLogicalLeftOffsetForLine(
        LogicalLeftFloatOffsetForLine(logical_top, fixed_offset,
                                      logical_height),
        apply_text_indent);
  }

  virtual LayoutObject* LayoutSpecialExcludedChild(bool /*relayoutChildren*/,
                                                   SubtreeLayoutScope&);
  bool UpdateLogicalWidthAndColumnWidth() override;

  void SetLogicalLeftForChild(LayoutBox& child, LayoutUnit logical_left);
  void SetLogicalTopForChild(LayoutBox& child, LayoutUnit logical_top);
  void DetermineLogicalLeftPositionForChild(LayoutBox& child);

  void AddOutlineRects(Vector<PhysicalRect>&,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const override;

  void InvalidateDisplayItemClients(PaintInvalidationReason) const override;

  Node* NodeForHitTest() const final;
  bool HitTestChildren(HitTestResult&,
                       const HitTestLocation&,
                       const PhysicalOffset& accumulated_offset,
                       HitTestAction) override;

  PhysicalOffset AccumulateRelativePositionOffsets() const override;

 private:
  void ResetLayout();
  void LayoutChildren(bool relayout_children, SubtreeLayoutScope&);
  void AddOverhangingFloatsFromChildren(LayoutUnit unconstrained_height);
  void LayoutBlockChildren(bool relayout_children,
                           SubtreeLayoutScope&,
                           LayoutUnit before_edge,
                           LayoutUnit after_edge);

  void MarkDescendantsWithFloatsForLayoutIfNeeded(
      LayoutBlockFlow& child,
      LayoutUnit new_logical_top,
      LayoutUnit previous_float_logical_bottom);
  bool PositionAndLayoutOnceIfNeeded(LayoutBox& child,
                                     LayoutUnit new_logical_top,
                                     BlockChildrenLayoutInfo&);

  // Handle breaking policy before the child, and insert a forced break in front
  // of it if needed.
  void InsertForcedBreakBeforeChildIfNeeded(LayoutBox& child,
                                            BlockChildrenLayoutInfo&);

  void LayoutBlockChild(LayoutBox& child, BlockChildrenLayoutInfo&);
  void AdjustPositionedBlock(LayoutBox& child, const BlockChildrenLayoutInfo&);
  void AdjustFloatingBlock(const MarginInfo&);

  LayoutPoint ComputeLogicalLocationForFloat(
      const FloatingObject&,
      LayoutUnit logical_top_offset) const;

  void RemoveFloatingObject(LayoutBox*);
  void RemoveFloatingObjectsBelow(FloatingObject*, LayoutUnit logical_offset);

  LayoutUnit GetClearDelta(LayoutBox* child, LayoutUnit y_pos);

  bool HasOverhangingFloat(LayoutBox*);
  void AddIntrudingFloats(LayoutBlockFlow* prev,
                          LayoutUnit xoffset,
                          LayoutUnit yoffset);
  void AddOverhangingFloats(LayoutBlockFlow* child,
                            bool make_child_paint_other_floats);

  bool HitTestFloats(HitTestResult&,
                     const HitTestLocation&,
                     const PhysicalOffset& accumulated_offset);

  void ClearFloats(EClear);

  LayoutUnit LogicalRightFloatOffsetForLine(LayoutUnit logical_top,
                                            LayoutUnit fixed_offset,
                                            LayoutUnit logical_height) const;
  LayoutUnit LogicalLeftFloatOffsetForLine(LayoutUnit logical_top,
                                           LayoutUnit fixed_offset,
                                           LayoutUnit logical_height) const;

  LayoutUnit LogicalLeftFloatOffsetForAvoidingFloats(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      LayoutUnit logical_height) const;
  LayoutUnit LogicalRightFloatOffsetForAvoidingFloats(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      LayoutUnit logical_height) const;

  LayoutUnit LogicalRightOffsetForPositioningFloat(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      LayoutUnit* height_remaining) const;
  LayoutUnit LogicalLeftOffsetForPositioningFloat(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      LayoutUnit* height_remaining) const;

  LayoutUnit AdjustLogicalRightOffsetForLine(
      LayoutUnit offset_from_floats,
      IndentTextOrNot apply_text_indent) const;
  LayoutUnit AdjustLogicalLeftOffsetForLine(
      LayoutUnit offset_from_floats,
      IndentTextOrNot apply_text_indent) const;

  virtual RootInlineBox* CreateRootInlineBox();  // Subclassed by SVG

  void DirtyLinesFromChangedChild(
      LayoutObject* child,
      MarkingBehavior marking_behaviour = kMarkContainerChain) override {
    line_boxes_.DirtyLinesFromChangedChild(
        LineLayoutItem(this), LineLayoutItem(child),
        marking_behaviour == kMarkContainerChain);
  }

  void CreateOrDestroyMultiColumnFlowThreadIfNeeded(
      const ComputedStyle* old_style);

  // Merge children of |siblingThatMayBeDeleted| into this object if possible,
  // and delete |siblingThatMayBeDeleted|. Returns true if we were able to
  // merge. In that case, |siblingThatMayBeDeleted| will be dead. We'll only be
  // able to merge if both blocks are anonymous.
  bool MergeSiblingContiguousAnonymousBlock(
      LayoutBlockFlow* sibling_that_may_be_deleted);

  // Reparent subsequent or preceding adjacent floating or out-of-flow siblings
  // into this object.
  void ReparentSubsequentFloatingOrOutOfFlowSiblings();
  void ReparentPrecedingFloatingOrOutOfFlowSiblings();

  void MakeChildrenInlineIfPossible();

  void MakeChildrenNonInline(LayoutObject* insertion_point = nullptr);
  void ChildBecameNonInline(LayoutObject* child) final;

  void UpdateLogicalWidthForAlignment(const ETextAlign&,
                                      const RootInlineBox*,
                                      BidiRun* trailing_space_run,
                                      LayoutUnit& logical_left,
                                      LayoutUnit& total_logical_width,
                                      LayoutUnit& available_logical_width,
                                      unsigned expansion_opportunity_count);

  bool ShouldBreakAtLineToAvoidWidow() const {
    return rare_data_ && rare_data_->line_break_to_avoid_widow_ >= 0;
  }
  void ClearShouldBreakAtLineToAvoidWidow() const;
  int LineBreakToAvoidWidow() const {
    return rare_data_ ? rare_data_->line_break_to_avoid_widow_ : -1;
  }
  void SetBreakAtLineToAvoidWidow(int);
  void ClearDidBreakAtLineToAvoidWidow();
  void SetDidBreakAtLineToAvoidWidow();
  bool DidBreakAtLineToAvoidWidow() const {
    return rare_data_ && rare_data_->did_break_at_line_to_avoid_widow_;
  }

  // Start page name propagated from the first child, if there are children, and
  // the first child has a start page name associated with it.
  const AtomicString PropagatedStartPageName() const;
  void SetPropagatedStartPageName(const AtomicString&);

  // End page name propagated from the last child, if there are children, and
  // the last child has a end page name associated with it.
  const AtomicString PropagatedEndPageName() const;
  void SetPropagatedEndPageName(const AtomicString&);

 public:
  struct FloatWithRect {
    DISALLOW_NEW();
    FloatWithRect(LayoutBox* f)
        : object(f), rect(f->FrameRect()), ever_had_layout(f->EverHadLayout()) {
      rect.Expand(f->MarginBoxOutsets());
    }

    LayoutBox* object;
    LayoutRect rect;
    bool ever_had_layout;
  };

  // MarginValues holds the margins in the block direction
  // used during collapsing margins computation.
  // CSS mandates to keep track of both positive and negative margins:
  // "When two or more margins collapse, the resulting margin width is the
  // maximum of the collapsing margins' widths. In the case of negative
  // margins, the maximum of the absolute values of the negative adjoining
  // margins is deducted from the maximum of the positive adjoining margins.
  // If there are no positive margins, the maximum of the absolute values of
  // the adjoining margins is deducted from zero."
  // https://drafts.csswg.org/css2/box.html#collapsing-margins
  class MarginValues {
    DISALLOW_NEW();

   public:
    MarginValues(LayoutUnit before_pos,
                 LayoutUnit before_neg,
                 LayoutUnit after_pos,
                 LayoutUnit after_neg)
        : positive_margin_before_(before_pos),
          negative_margin_before_(before_neg),
          positive_margin_after_(after_pos),
          negative_margin_after_(after_neg) {}

    LayoutUnit PositiveMarginBefore() const { return positive_margin_before_; }
    LayoutUnit NegativeMarginBefore() const { return negative_margin_before_; }
    LayoutUnit PositiveMarginAfter() const { return positive_margin_after_; }
    LayoutUnit NegativeMarginAfter() const { return negative_margin_after_; }

    void SetPositiveMarginBefore(LayoutUnit pos) {
      positive_margin_before_ = pos;
    }
    void SetNegativeMarginBefore(LayoutUnit neg) {
      negative_margin_before_ = neg;
    }
    void SetPositiveMarginAfter(LayoutUnit pos) {
      positive_margin_after_ = pos;
    }
    void SetNegativeMarginAfter(LayoutUnit neg) {
      negative_margin_after_ = neg;
    }

   private:
    LayoutUnit positive_margin_before_;
    LayoutUnit negative_margin_before_;
    LayoutUnit positive_margin_after_;
    LayoutUnit negative_margin_after_;
  };
  MarginValues MarginValuesForChild(LayoutBox& child) const;

  // Allocated only when some of these fields have non-default values
  struct LayoutBlockFlowRareData final
      : public GarbageCollected<LayoutBlockFlowRareData> {
   public:
    explicit LayoutBlockFlowRareData(const LayoutBlockFlow* block);
    LayoutBlockFlowRareData(const LayoutBlockFlowRareData&) = delete;
    LayoutBlockFlowRareData& operator=(const LayoutBlockFlowRareData&) = delete;
    ~LayoutBlockFlowRareData();

    static LayoutUnit PositiveMarginBeforeDefault(
        const LayoutBlockFlow* block) {
      return block->MarginBefore().ClampNegativeToZero();
    }
    static LayoutUnit NegativeMarginBeforeDefault(
        const LayoutBlockFlow* block) {
      return (-block->MarginBefore()).ClampNegativeToZero();
    }
    static LayoutUnit PositiveMarginAfterDefault(const LayoutBlockFlow* block) {
      return block->MarginAfter().ClampNegativeToZero();
    }
    static LayoutUnit NegativeMarginAfterDefault(const LayoutBlockFlow* block) {
      return (-block->MarginAfter()).ClampNegativeToZero();
    }

    void Trace(Visitor*) const {}

    MarginValues margins_;
    LayoutUnit pagination_strut_propagated_from_child_;

    LayoutUnit first_forced_break_offset_;

    LayoutMultiColumnFlowThread* multi_column_flow_thread_ = nullptr;

    // |offset_mapping_| is used only for legacy layout tree for caching offset
    // mapping for |NGInlineNode::GetOffsetMapping()|.
    // TODO(yosin): Once we have no legacy support, we should get rid of
    // |offset_mapping_| here.
    std::unique_ptr<NGOffsetMapping> offset_mapping_;

    // Name of the start page for this object, if propagated from a descendant;
    // see https://drafts.csswg.org/css-page-3/#start-page-value
    AtomicString propagated_start_page_name_;

    // Name of the end page for this object, if propagated from a descendant;
    // see https://drafts.csswg.org/css-page-3/#end-page-value
    AtomicString propagated_end_page_name_;

    unsigned break_before_ : 4;
    unsigned break_after_ : 4;
    int line_break_to_avoid_widow_;
    bool did_break_at_line_to_avoid_widow_ : 1;
  };

  void ClearOffsetMappingIfNeeded();
  const NGOffsetMapping* GetOffsetMapping() const;
  void SetOffsetMapping(std::unique_ptr<NGOffsetMapping>);

  const FloatingObjects* GetFloatingObjects() const {
    return floating_objects_.get();
  }

  static void UpdateAncestorShouldPaintFloatingObject(
      const LayoutBox& float_box);

  bool ShouldTruncateOverflowingText() const;

  int GetLayoutPassCountForTesting();

  // This is public only for use by LayoutNG, so that NGBlockNode can call it.
  void IncrementLayoutPassCount();

 protected:
  LayoutUnit MaxPositiveMarginBefore() const {
    return rare_data_
               ? rare_data_->margins_.PositiveMarginBefore()
               : LayoutBlockFlowRareData::PositiveMarginBeforeDefault(this);
  }
  LayoutUnit MaxNegativeMarginBefore() const {
    return rare_data_
               ? rare_data_->margins_.NegativeMarginBefore()
               : LayoutBlockFlowRareData::NegativeMarginBeforeDefault(this);
  }
  LayoutUnit MaxPositiveMarginAfter() const {
    return rare_data_
               ? rare_data_->margins_.PositiveMarginAfter()
               : LayoutBlockFlowRareData::PositiveMarginAfterDefault(this);
  }
  LayoutUnit MaxNegativeMarginAfter() const {
    return rare_data_
               ? rare_data_->margins_.NegativeMarginAfter()
               : LayoutBlockFlowRareData::NegativeMarginAfterDefault(this);
  }

  void SetMaxMarginBeforeValues(LayoutUnit pos, LayoutUnit neg);
  void SetMaxMarginAfterValues(LayoutUnit pos, LayoutUnit neg);

  void InitMaxMarginValues() {
    if (rare_data_) {
      rare_data_->margins_ = MarginValues(
          LayoutBlockFlowRareData::PositiveMarginBeforeDefault(this),
          LayoutBlockFlowRareData::NegativeMarginBeforeDefault(this),
          LayoutBlockFlowRareData::PositiveMarginAfterDefault(this),
          LayoutBlockFlowRareData::NegativeMarginAfterDefault(this));
    }
  }

  virtual ETextAlign TextAlignmentForLine(bool ends_with_soft_break) const;

 private:
  LayoutUnit CollapsedMarginBefore() const final {
    return MaxPositiveMarginBefore() - MaxNegativeMarginBefore();
  }
  LayoutUnit CollapsedMarginAfter() const final {
    return MaxPositiveMarginAfter() - MaxNegativeMarginAfter();
  }

  LayoutUnit AdjustedMarginBeforeForPagination(
      const LayoutBox&,
      LayoutUnit logical_top_margin_edge,
      LayoutUnit logical_top_border_edge,
      const BlockChildrenLayoutInfo&) const;

  LayoutUnit CollapseMargins(LayoutBox& child,
                             BlockChildrenLayoutInfo&,
                             bool child_is_self_collapsing);
  LayoutUnit ClearFloatsIfNeeded(LayoutBox& child,
                                 MarginInfo&,
                                 LayoutUnit old_top_pos_margin,
                                 LayoutUnit old_top_neg_margin,
                                 LayoutUnit y_pos,
                                 bool child_is_self_collapsing);
  LayoutUnit EstimateLogicalTopPosition(
      LayoutBox& child,
      const BlockChildrenLayoutInfo&,
      LayoutUnit& estimate_without_pagination);
  void MarginBeforeEstimateForChild(LayoutBox&, LayoutUnit&, LayoutUnit&) const;
  void HandleAfterSideOfBlock(LayoutBox* last_child,
                              LayoutUnit top,
                              LayoutUnit bottom,
                              MarginInfo&);
  void SetCollapsedBottomMargin(const MarginInfo&);

  static void RecalcFloatingDescendantsVisualOverflow(
      const NGPhysicalContainerFragment& fragment);

  // Apply any forced fragmentainer break that's set on the current class A
  // break point.
  LayoutUnit ApplyForcedBreak(LayoutUnit logical_offset, EBreakBetween);

  void SetBreakBefore(EBreakBetween);
  void SetBreakAfter(EBreakBetween);
  EBreakBetween BreakBefore() const override;
  EBreakBetween BreakAfter() const override;

  LayoutUnit AdjustBlockChildForPagination(LayoutUnit logical_top,
                                           LayoutBox& child,
                                           BlockChildrenLayoutInfo&,
                                           bool at_before_side_of_block);

  // If a float cannot fit in the current fragmentainer, return the logical top
  // margin edge that the float needs to have in order to be pushed to the top
  // of the next fragmentainer. Otherwise, just return |logicalTopMarginEdge|.
  LayoutUnit AdjustFloatLogicalTopForPagination(
      LayoutBox&,
      LayoutUnit logical_top_margin_edge);

  // Computes a deltaOffset value that put a line at the top of the next page if
  // it doesn't fit on the current page.
  void AdjustLinePositionForPagination(RootInlineBox&,
                                       LayoutUnit& delta_offset);

  // If the child is unsplittable and can't fit on the current page, return the
  // top of the next page/column.
  LayoutUnit AdjustForUnsplittableChild(LayoutBox&,
                                        LayoutUnit logical_offset) const;

  // Used to store state between styleWillChange and styleDidChange
  static bool can_propagate_float_into_sibling_;

  LineBoxList line_boxes_;  // All of the root line boxes created for this block
                            // flow.  For example, <div>Hello<br>world.</div>
                            // will have two total lines for the <div>.

  LayoutBlockFlowRareData& EnsureRareData();

  bool IsSelfCollapsingBlock() const override;
  bool CheckIfIsSelfCollapsingBlock() const;

 protected:
  Persistent<LayoutBlockFlowRareData> rare_data_;
  std::unique_ptr<FloatingObjects> floating_objects_;

  friend class MarginInfo;
  friend class LineWidth;  // needs to know FloatingObject

  // LayoutRubyBase objects need to be able to split and merge, moving their
  // children around (calling makeChildrenNonInline).
  // TODO(mstensho): Try to get rid of this friendship.
  friend class LayoutRubyBase;

  // FIXME-BLOCKFLOW: These methods have implementations in
  // LayoutBlockFlowLine. They should be moved to the proper header once the
  // line layout code is separated from LayoutBlock and LayoutBlockFlow.
  // START METHODS DEFINED IN LayoutBlockFlowLine
 private:
  InlineFlowBox* CreateLineBoxes(LineLayoutItem,
                                 const LineInfo&,
                                 InlineBox* child_box);
  void SetMarginsForRubyRun(BidiRun*,
                            LayoutRubyRun*,
                            LayoutObject*,
                            const LineInfo&);
  void ComputeInlineDirectionPositionsForLine(RootInlineBox*,
                                              const LineInfo&,
                                              BidiRun* first_run,
                                              BidiRun* trailing_space_run,
                                              bool reached_end,
                                              GlyphOverflowAndFallbackFontsMap&,
                                              VerticalPositionCache&,
                                              const WordMeasurements&);
  BidiRun* ComputeInlineDirectionPositionsForSegment(
      RootInlineBox*,
      const LineInfo&,
      LayoutUnit& logical_left,
      LayoutUnit& available_logical_width,
      BidiRun* first_run,
      BidiRun* trailing_space_run,
      GlyphOverflowAndFallbackFontsMap& text_box_data_map,
      VerticalPositionCache&,
      const WordMeasurements&);
  void ComputeBlockDirectionPositionsForLine(RootInlineBox*,
                                             BidiRun*,
                                             GlyphOverflowAndFallbackFontsMap&,
                                             VerticalPositionCache&);
  void AppendFloatingObjectToLastLine(FloatingObject&);
  void AppendFloatsToLastLine(LineLayoutState&,
                              const InlineIterator& clean_line_start,
                              const InlineBidiResolver&,
                              const BidiStatus& clean_line_bidi_status);
  // Helper function for layoutInlineChildren()
  RootInlineBox* CreateLineBoxesFromBidiRuns(unsigned bidi_level,
                                             BidiRunList<BidiRun>&,
                                             const InlineIterator& end,
                                             LineInfo&,
                                             VerticalPositionCache&,
                                             BidiRun* trailing_space_run,
                                             const WordMeasurements&);
  void LayoutRunsAndFloats(LineLayoutState&);
  const InlineIterator& RestartLayoutRunsAndFloatsInRange(
      LayoutUnit old_logical_height,
      LayoutUnit new_logical_height,
      FloatingObject* last_float_from_previous_line,
      InlineBidiResolver&,
      const InlineIterator&);
  void LayoutRunsAndFloatsInRange(LineLayoutState&,
                                  InlineBidiResolver&,
                                  const InlineIterator& clean_line_start,
                                  const BidiStatus& clean_line_bidi_status);
  void LinkToEndLineIfNeeded(LineLayoutState&);
  void MarkDirtyFloatsForPaintInvalidation(Vector<FloatWithRect>& floats);
  RootInlineBox* DetermineStartPosition(LineLayoutState&, InlineBidiResolver&);
  void DetermineEndPosition(LineLayoutState&,
                            RootInlineBox* start_box,
                            InlineIterator& clean_line_start,
                            BidiStatus& clean_line_bidi_status);
  bool LineBoxHasBRWithClearance(RootInlineBox*);
  bool CheckPaginationAndFloatsAtEndLine(LineLayoutState&);
  bool MatchedEndLine(LineLayoutState&,
                      const InlineBidiResolver&,
                      const InlineIterator& end_line_start,
                      const BidiStatus& end_line_status);
  void DeleteEllipsisLineBoxes();
  void CheckLinesForTextOverflow();
  void TryPlacingEllipsisOnAtomicInlines(RootInlineBox*,
                                         LayoutUnit block_right_edge,
                                         LayoutUnit block_left_edge,
                                         LayoutUnit width,
                                         const AtomicString&,
                                         InlineBox*);
  void ClearTruncationOnAtomicInlines(RootInlineBox*);
  void MarkLinesDirtyInBlockRange(LayoutUnit logical_top,
                                  LayoutUnit logical_bottom,
                                  RootInlineBox* highest = nullptr);
  // Positions new floats and also adjust all floats encountered on the line if
  // any of them have to move to the next page/column.
  void PositionDialog();

  // END METHODS DEFINED IN LayoutBlockFlowLine
};

template <>
struct DowncastTraits<LayoutBlockFlow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutBlockFlow();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_FLOW_H_
