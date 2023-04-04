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

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/line/line_box_list.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/line/trailing_objects.h"

namespace blink {

template <class Run>
class BidiRunList;
class LineInfo;
class LineLayoutState;
class LineWidth;
class LayoutMultiColumnFlowThread;
class MarginInfo;
class NGOffsetMapping;
class NGPhysicalFragment;

struct NGInlineNodeData;

enum IndentTextOrNot { kDoNotIndentText, kIndentText };

// LayoutBlockFlow is the class that implements a block container in CSS 2.1.
// http://www.w3.org/TR/CSS21/visuren.html#block-boxes
//
// LayoutBlockFlows are the only LayoutObject allowed to own floating objects
// (aka floats): http://www.w3.org/TR/CSS21/visuren.html#floats .
//
// LayoutBlockFlow is also the only LayoutObject to own a line box tree and
// perform inline layout. See layout_block_flow_line.cc for these parts.
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
  void Trace(Visitor*) const override;

  static LayoutBlockFlow* CreateAnonymous(Document*,
                                          scoped_refptr<const ComputedStyle>);

  bool IsLayoutBlockFlow() const final {
    NOT_DESTROYED();
    return true;
  }

  void UpdateBlockLayout(bool relayout_children) override;

  void ComputeVisualOverflow(bool recompute_floats) override;

  void DeleteLineBoxTree();

  bool CanContainFirstFormattedLine() const;

  LayoutUnit AvailableLogicalWidthForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return (LogicalRightOffsetForLine(position, indent_text, logical_height) -
            LogicalLeftOffsetForLine(position, indent_text, logical_height))
        .ClampNegativeToZero();
  }
  LayoutUnit LogicalRightOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return LogicalRightOffsetForLine(position, LogicalRightOffsetForContent(),
                                     indent_text, logical_height);
  }
  LayoutUnit LogicalLeftOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return LogicalLeftOffsetForLine(position, LogicalLeftOffsetForContent(),
                                    indent_text, logical_height);
  }
  LayoutUnit StartOffsetForLine(
      LayoutUnit position,
      IndentTextOrNot indent_text,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForLine(position, indent_text, logical_height)
               : LogicalWidth() - LogicalRightOffsetForLine(
                                      position, indent_text, logical_height);
  }

  LayoutUnit AvailableLogicalWidthForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return (LogicalRightOffsetForAvoidingFloats(position, logical_height) -
            LogicalLeftOffsetForAvoidingFloats(position, logical_height))
        .ClampNegativeToZero();
  }
  LayoutUnit LogicalLeftOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return LogicalLeftOffsetForContent();
  }
  LayoutUnit LogicalRightOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return LogicalRightOffsetForContent();
  }
  LayoutUnit StartOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForAvoidingFloats(position, logical_height)
               : LogicalWidth() - LogicalRightOffsetForAvoidingFloats(
                                      position, logical_height);
  }
  LayoutUnit EndOffsetForAvoidingFloats(
      LayoutUnit position,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return !StyleRef().IsLeftToRightDirection()
               ? LogicalLeftOffsetForAvoidingFloats(position, logical_height)
               : LogicalWidth() - LogicalRightOffsetForAvoidingFloats(
                                      position, logical_height);
  }

  const LineBoxList& LineBoxes() const {
    NOT_DESTROYED();
    return line_boxes_;
  }
  LineBoxList* LineBoxes() {
    NOT_DESTROYED();
    return &line_boxes_;
  }
  InlineFlowBox* FirstLineBox() const {
    NOT_DESTROYED();
    return line_boxes_.First();
  }
  InlineFlowBox* LastLineBox() const {
    NOT_DESTROYED();
    return line_boxes_.Last();
  }
  RootInlineBox* FirstRootBox() const {
    NOT_DESTROYED();
    return static_cast<RootInlineBox*>(FirstLineBox());
  }
  RootInlineBox* LastRootBox() const {
    NOT_DESTROYED();
    return static_cast<RootInlineBox*>(LastLineBox());
  }

  RootInlineBox* CreateAndAppendRootInlineBox();
  RootInlineBox* ConstructLine(BidiRunList<BidiRun>&, const LineInfo&);

  // Return the number of lines in *this* block flow. Does not recurse into
  // block flow children.
  // Will start counting from the first line, and stop counting right after
  // |stop_root_inline_box|, if specified.
  int LineCount(const RootInlineBox* stop_root_inline_box = nullptr) const;

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;

  void MarkAllDescendantsWithFloatsForLayout(
      LayoutBox* float_to_remove = nullptr,
      bool in_layout = true);

  void AddChild(LayoutObject* new_child,
                LayoutObject* before_child = nullptr) override;
  void RemoveChild(LayoutObject*) override;

  bool CreatesAnonymousWrapper() const override;

  void MoveAllChildrenIncludingFloatsTo(LayoutBlock* to_block,
                                        bool full_remove_insert);

  void ChildBecameFloatingOrOutOfFlow(LayoutBox* child);
  void CollapseAnonymousBlockChild(LayoutBlockFlow* child);

  bool GeneratesLineBoxesForInlineChild(LayoutObject*);

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
    NOT_DESTROYED();
    return rare_data_ ? rare_data_->multi_column_flow_thread_ : nullptr;
  }
  void ResetMultiColumnFlowThread() {
    NOT_DESTROYED();
    if (rare_data_)
      rare_data_->multi_column_flow_thread_ = nullptr;
  }

  // Return true if this block establishes a fragmentation context root (e.g. a
  // multicol container).
  //
  // Implementation detail: At some point in the future there should be no flow
  // threads. Callers that only want to know if this is a fragmentation context
  // root (and don't depend on flow threads) should call this method.
  bool IsFragmentationContextRoot() const override {
    NOT_DESTROYED();
    return MultiColumnFlowThread();
  }

  bool IsInitialLetterBox() const override;

  void AddVisualOverflowFromInlineChildren();

  void AddLayoutOverflowFromInlineChildren();

  // FIXME: This should be const to avoid a const_cast, but can modify child
  // dirty bits and LayoutTextCombine.
  void ComputeInlinePreferredLogicalWidths(LayoutUnit& min_logical_width,
                                           LayoutUnit& max_logical_width);

  // Return true if this object is allowed to establish a multicol container.
  virtual bool AllowsColumns() const;

  bool CreatesNewFormattingContext() const override;

  using LayoutBoxModelObject::MoveChildrenTo;
  void MoveChildrenTo(LayoutBoxModelObject* to_box_model_object,
                      LayoutObject* start_child,
                      LayoutObject* end_child,
                      LayoutObject* before_child,
                      bool full_remove_insert = false) override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutBlockFlow";
  }

  void SetShouldDoFullPaintInvalidationForFirstLine();

  void SimplifiedNormalFlowInlineLayout();
  RecalcLayoutOverflowResult RecalcInlineChildrenLayoutOverflow();
  void RecalcInlineChildrenVisualOverflow();

  PositionWithAffinity PositionForPoint(const PhysicalOffset&) const override;

  bool ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom() const;

  // These functions are only public so we can call it from NGBlockNode while
  // we're still working on LayoutNG.
  void AddVisualOverflowFromFloats(const NGPhysicalFragment& fragment);

  virtual NGInlineNodeData* TakeNGInlineNodeData() {
    NOT_DESTROYED();
    return nullptr;
  }
  virtual NGInlineNodeData* GetNGInlineNodeData() const {
    NOT_DESTROYED();
    return nullptr;
  }
  virtual void ResetNGInlineNodeData() { NOT_DESTROYED(); }
  virtual void ClearNGInlineNodeData() { NOT_DESTROYED(); }
  virtual bool HasNGInlineNodeData() const {
    NOT_DESTROYED();
    return false;
  }
  virtual void WillCollectInlines() { NOT_DESTROYED(); }

#if DCHECK_IS_ON()
  void ShowLineTreeAndMark(const InlineBox* = nullptr,
                           const char* = nullptr,
                           const InlineBox* = nullptr,
                           const char* = nullptr,
                           const LayoutObject* = nullptr) const;
#endif

 protected:
  void LayoutInlineChildren(bool relayout_children, LayoutUnit after_edge);

  void WillBeDestroyed() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  void UpdateBlockChildDirtyBitsBeforeLayout(bool relayout_children,
                                             LayoutBox&);

  LayoutUnit LogicalRightOffsetForLine(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      IndentTextOrNot apply_text_indent,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return AdjustLogicalRightOffsetForLine(fixed_offset, apply_text_indent);
  }
  LayoutUnit LogicalLeftOffsetForLine(
      LayoutUnit logical_top,
      LayoutUnit fixed_offset,
      IndentTextOrNot apply_text_indent,
      LayoutUnit logical_height = LayoutUnit()) const {
    NOT_DESTROYED();
    return AdjustLogicalLeftOffsetForLine(fixed_offset, apply_text_indent);
  }

  void SetLogicalLeftForChild(LayoutBox& child, LayoutUnit logical_left);
  void SetLogicalTopForChild(LayoutBox& child, LayoutUnit logical_top);
  void DetermineLogicalLeftPositionForChild(LayoutBox& child);

  void AddOutlineRects(Vector<PhysicalRect>&,
                       OutlineInfo*,
                       const PhysicalOffset& additional_offset,
                       NGOutlineType) const override;

  void InvalidateDisplayItemClients(PaintInvalidationReason) const override;

  Node* NodeForHitTest() const final;
  bool HitTestChildren(HitTestResult&,
                       const HitTestLocation&,
                       const PhysicalOffset& accumulated_offset,
                       HitTestPhase) override;

 private:
  void ResetLayout();
  void LayoutChildren(bool relayout_children, SubtreeLayoutScope&);
  void LayoutBlockChildren(bool relayout_children,
                           SubtreeLayoutScope&,
                           LayoutUnit before_edge,
                           LayoutUnit after_edge);

  bool PositionAndLayoutOnceIfNeeded(LayoutBox& child,
                                     LayoutUnit new_logical_top);

  void LayoutBlockChild(LayoutBox& child);
  void AdjustPositionedBlock(LayoutBox& child);

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
    NOT_DESTROYED();
    line_boxes_.DirtyLinesFromChangedChild(
        LineLayoutItem(this), LineLayoutItem(child),
        marking_behaviour == kMarkContainerChain);
  }

  void CreateOrDestroyMultiColumnFlowThreadIfNeeded(
      const ComputedStyle* old_style);

  // Merge children of |sibling_that_may_be_deleted| into this object if
  // possible, and delete |sibling_that_may_be_deleted|. Returns true if we
  // were able to merge. In that case, |sibling_that_may_be_deleted| will be
  // dead. We'll only be able to merge if both blocks are anonymous.
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

 public:
  struct FloatWithRect {
    DISALLOW_NEW();
    explicit FloatWithRect(LayoutBox* f)
        : object(f), rect(f->FrameRect()), ever_had_layout(f->EverHadLayout()) {
      rect.Expand(f->MarginBoxOutsets());
    }

    void Trace(Visitor* visitor) const { visitor->Trace(object); }

    Member<LayoutBox> object;
    LayoutRect rect;
    bool ever_had_layout;
  };

  // Allocated only when some of these fields have non-default values
  struct LayoutBlockFlowRareData final
      : public GarbageCollected<LayoutBlockFlowRareData> {
   public:
    explicit LayoutBlockFlowRareData(const LayoutBlockFlow* block);
    LayoutBlockFlowRareData(const LayoutBlockFlowRareData&) = delete;
    LayoutBlockFlowRareData& operator=(const LayoutBlockFlowRareData&) = delete;
    ~LayoutBlockFlowRareData();

    void Trace(Visitor*) const;

    Member<LayoutMultiColumnFlowThread> multi_column_flow_thread_;

    // |offset_mapping_| is used only for legacy layout tree for caching offset
    // mapping for |NGInlineNode::GetOffsetMapping()|.
    // TODO(yosin): Once we have no legacy support, we should get rid of
    // |offset_mapping_| here.
    Member<NGOffsetMapping> offset_mapping_;
  };

  void ClearOffsetMappingIfNeeded();
  const NGOffsetMapping* GetOffsetMapping() const;
  void SetOffsetMapping(NGOffsetMapping*);

  bool ShouldTruncateOverflowingText() const;

 protected:
  virtual ETextAlign TextAlignmentForLine(bool ends_with_soft_break) const;

 private:
  static void RecalcFloatingDescendantsVisualOverflow(
      const NGPhysicalFragment& fragment);

  LineBoxList line_boxes_;  // All of the root line boxes created for this block
                            // flow.  For example, <div>Hello<br>world.</div>
                            // will have two total lines for the <div>.

  LayoutBlockFlowRareData& EnsureRareData();

 protected:
  Member<LayoutBlockFlowRareData> rare_data_;

  friend class MarginInfo;
  friend class LineWidth;  // needs to know FloatingObject

  // LayoutNGRubyBase objects need to be able to split and merge, moving their
  // children around (calling MakeChildrenNonInline).
  friend class LayoutNGRubyBase;

  // FIXME-BLOCKFLOW: These methods have implementations in
  // LayoutBlockFlowLine. They should be moved to the proper header once the
  // line layout code is separated from LayoutBlock and LayoutBlockFlow.
  // START METHODS DEFINED IN LayoutBlockFlowLine
 private:
  InlineFlowBox* CreateLineBoxes(LineLayoutItem,
                                 const LineInfo&,
                                 InlineBox* child_box);
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
  // Helper function for LayoutInlineChildren()
  RootInlineBox* CreateLineBoxesFromBidiRuns(unsigned bidi_level,
                                             BidiRunList<BidiRun>&,
                                             const InlineIterator& end,
                                             LineInfo&,
                                             VerticalPositionCache&,
                                             BidiRun* trailing_space_run,
                                             const WordMeasurements&);
  void LayoutRunsAndFloats(LineLayoutState&);
  void LayoutRunsAndFloatsInRange(LineLayoutState&,
                                  InlineBidiResolver&,
                                  const InlineIterator& clean_line_start,
                                  const BidiStatus& clean_line_bidi_status);
  void LinkToEndLineIfNeeded(LineLayoutState&);
  RootInlineBox* DetermineStartPosition(LineLayoutState&, InlineBidiResolver&);
  void DetermineEndPosition(LineLayoutState&,
                            RootInlineBox* start_box,
                            InlineIterator& clean_line_start,
                            BidiStatus& clean_line_bidi_status);
  bool LineBoxHasBRWithClearance(RootInlineBox*);
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

  // END METHODS DEFINED IN LayoutBlockFlowLine
};

template <>
struct DowncastTraits<LayoutBlockFlow> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutBlockFlow();
  }
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::LayoutBlockFlow::FloatWithRect)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_BLOCK_FLOW_H_
