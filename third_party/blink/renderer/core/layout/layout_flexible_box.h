/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FLEXIBLE_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FLEXIBLE_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/order_iterator.h"

namespace blink {

class FlexItem;
class FlexItemVectorView;
class FlexLayoutAlgorithm;
class FlexLine;
struct MinMaxSizes;

class CORE_EXPORT LayoutFlexibleBox : public LayoutBlock {
 public:
  explicit LayoutFlexibleBox(Element*);
  ~LayoutFlexibleBox() override;

  const char* GetName() const override { return "LayoutFlexibleBox"; }

  bool IsFlexibleBox() const final { return true; }
  bool IsFlexibleBoxIncludingNG() const final { return true; }
  bool IsFlexibleBoxIncludingDeprecatedAndNG() const final { return true; }
  void UpdateBlockLayout(bool relayout_children) final;

  bool IsChildAllowed(LayoutObject* object,
                      const ComputedStyle& style) const override;
  LayoutUnit BaselinePosition(
      FontBaseline,
      bool first_line,
      LineDirectionMode,
      LinePositionMode = kPositionOnContainingLine) const override;

  static LayoutUnit SynthesizedBaselineFromBorderBox(const LayoutBox&,
                                                     LineDirectionMode);

  LayoutUnit FirstLineBoxBaseline() const override;
  LayoutUnit InlineBlockBaseline(LineDirectionMode) const override;
  bool HasTopOverflow() const override;
  bool HasLeftOverflow() const override;

  void PaintChildren(const PaintInfo&,
                     const PhysicalOffset& paint_offset) const final;

  bool IsHorizontalFlow() const;

  const OrderIterator& GetOrderIterator() const { return order_iterator_; }

  // These three functions are used when resolving percentages against a
  // flex item's logical height. In flexbox, sometimes a logical height
  // should be considered definite even though it normally shouldn't be,
  // and these functions implement that logic.
  bool CrossSizeIsDefiniteForPercentageResolution(const LayoutBox& child) const;
  bool MainSizeIsDefiniteForPercentageResolution(const LayoutBox& child) const;
  bool UseOverrideLogicalHeightForPerentageResolution(
      const LayoutBox& child) const;

  void ClearCachedMainSizeForChild(const LayoutBox& child);

  LayoutUnit StaticMainAxisPositionForPositionedChild(const LayoutBox& child);
  LayoutUnit StaticCrossAxisPositionForPositionedChild(const LayoutBox& child);

  LayoutUnit StaticInlinePositionForPositionedChild(const LayoutBox& child);
  LayoutUnit StaticBlockPositionForPositionedChild(const LayoutBox& child);

  // Returns true if the position changed. In that case, the child will have to
  // be laid out again.
  bool SetStaticPositionForPositionedLayout(LayoutBox& child);
  static bool SetStaticPositionForChildInFlexNGContainer(LayoutBox& child,
                                                         LayoutBlock* parent);
  LayoutUnit CrossAxisContentExtent() const;

 protected:
  MinMaxSizes ComputeIntrinsicLogicalWidths() const override;

  bool HitTestChildren(HitTestResult&,
                       const HitTestLocation&,
                       const PhysicalOffset& accumulated_offset,
                       HitTestAction) override;

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void RemoveChild(LayoutObject*) override;

 private:
  enum ChildLayoutType { kLayoutIfNeeded, kForceLayout, kNeverLayout };

  enum class SizeDefiniteness { kDefinite, kIndefinite, kUnknown };

  bool MainAxisIsInlineAxis(const LayoutBox& child) const;
  bool IsColumnFlow() const;
  bool IsLeftToRightFlow() const;
  bool IsMultiline() const;
  Length FlexBasisForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisExtentForChild(const LayoutBox& child) const;
  LayoutUnit CrossAxisUnstretchedExtentForChild(const LayoutBox& child) const;
  LayoutUnit ChildUnstretchedLogicalHeight(const LayoutBox& child) const;
  LayoutUnit ChildUnstretchedLogicalWidth(const LayoutBox& child) const;
  LayoutUnit MainAxisExtentForChild(const LayoutBox& child) const;
  LayoutUnit MainAxisContentExtentForChild(const LayoutBox& child) const;
  LayoutUnit MainAxisContentExtentForChildIncludingScrollbar(
      const LayoutBox& child) const;
  LayoutUnit CrossAxisExtent() const;
  LayoutUnit MainAxisContentExtent(LayoutUnit content_logical_height);
  LayoutUnit ComputeMainAxisExtentForChild(const LayoutBox& child,
                                           SizeType,
                                           const Length& size,
                                           LayoutUnit border_and_padding) const;

  LayoutUnit ContentInsetBottom() const;
  LayoutUnit ContentInsetRight() const;
  LayoutUnit FlowAwareContentInsetStart() const;
  LayoutUnit FlowAwareContentInsetEnd() const;
  LayoutUnit FlowAwareContentInsetBefore() const;
  LayoutUnit FlowAwareContentInsetAfter() const;

  LayoutUnit CrossAxisScrollbarExtent() const;
  LayoutUnit CrossAxisScrollbarExtentForChild(const LayoutBox& child) const;
  LayoutPoint FlowAwareLocationForChild(const LayoutBox& child) const;
  bool UseChildAspectRatio(const LayoutBox& child) const;
  LayoutUnit ComputeMainSizeFromAspectRatioUsing(
      const LayoutBox& child,
      const Length& cross_size_length) const;
  void SetFlowAwareLocationForChild(LayoutBox& child, const LayoutPoint&);
  LayoutUnit ComputeInnerFlexBaseSizeForChild(
      LayoutBox& child,
      LayoutUnit main_axis_border_and_padding,
      ChildLayoutType = kLayoutIfNeeded);
  void ResetAlignmentForChild(LayoutBox& child, LayoutUnit);
  bool MainAxisLengthIsDefinite(const LayoutBox& child,
                                const Length& flex_basis,
                                bool add_to_cb = true) const;
  bool CrossAxisLengthIsDefinite(const LayoutBox& child,
                                 const Length& flex_basis) const;
  bool NeedToStretchChildLogicalHeight(const LayoutBox& child) const;
  bool ChildHasIntrinsicMainAxisSize(const FlexLayoutAlgorithm&,
                                     const LayoutBox& child) const;
  EOverflow MainAxisOverflowForChild(const LayoutBox& child) const;
  EOverflow CrossAxisOverflowForChild(const LayoutBox& child) const;
  void CacheChildMainSize(const LayoutBox& child);
  bool CanAvoidLayoutForNGChild(const LayoutBox& child) const;

  void LayoutFlexItems(bool relayout_children, SubtreeLayoutScope&);
  bool HasAutoMarginsInCrossAxis(const LayoutBox& child) const;
  void RepositionLogicalHeightDependentFlexItems(
      FlexLayoutAlgorithm& algorithm);
  LayoutUnit ClientLogicalBottomAfterRepositioning();

  LayoutUnit ComputeChildMarginValue(const Length& margin);
  void PrepareOrderIteratorAndMargins();
  MinMaxSizes ComputeMinAndMaxSizesForChild(
      const FlexLayoutAlgorithm& algorithm,
      const LayoutBox& child,
      LayoutUnit border_and_padding) const;
  LayoutUnit AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
      const LayoutBox& child,
      LayoutUnit child_size) const;
  void ConstructAndAppendFlexItem(FlexLayoutAlgorithm* algorithm,
                                  LayoutBox& child,
                                  ChildLayoutType);

  bool ResolveFlexibleLengths(FlexLine*,
                              LayoutUnit initial_free_space,
                              LayoutUnit& remaining_free_space);

  void ResetAutoMarginsAndLogicalTopInCrossAxis(LayoutBox& child);
  void SetOverrideMainAxisContentSizeForChild(FlexItem&);
  void PrepareChildForPositionedLayout(LayoutBox& child);
  void LayoutLineItems(FlexLine*, bool relayout_children, SubtreeLayoutScope&);
  void ApplyLineItemsPosition(FlexLine*);
  void LayoutColumnReverse(FlexItemVectorView&,
                           LayoutUnit cross_axis_offset,
                           LayoutUnit available_free_space);
  void AlignFlexLines(FlexLayoutAlgorithm&);
  void AlignChildren(FlexLayoutAlgorithm&);
  void ApplyStretchAlignmentToChild(FlexItem& child);
  void FlipForRightToLeftColumn(const Vector<FlexLine>& line_contexts);

  float CountIntrinsicSizeForAlgorithmChange(
      LayoutUnit max_preferred_width,
      LayoutBox* child,
      float previous_max_content_flex_fraction) const;

  void MergeAnonymousFlexItems(LayoutObject* remove_child);

  // This is used to cache the preferred size for orthogonal flow children so we
  // don't have to relayout to get it
  HashMap<const LayoutObject*, LayoutUnit> intrinsic_size_along_main_axis_;

  // This set is used to keep track of which children we laid out in this
  // current layout iteration. We need it because the ones in this set may
  // need an additional layout pass for correct stretch alignment handling, as
  // the first layout likely did not use the correct value for percentage
  // sizing of children.
  HashSet<const LayoutObject*> relaid_out_children_;

  mutable OrderIterator order_iterator_;
  int number_of_in_flow_children_on_first_line_;

  // This is SizeIsUnknown outside of layoutBlock()
  mutable SizeDefiniteness has_definite_height_;
  bool in_layout_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutFlexibleBox, IsFlexibleBox());

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_FLEXIBLE_BOX_H_
