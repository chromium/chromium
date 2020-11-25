// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
class NGBoxFragment;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  NGFlexLayoutAlgorithm(const NGLayoutAlgorithmParams& params);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesInput&) const override;
  scoped_refptr<const NGLayoutResult> Layout() override;

 private:
  scoped_refptr<const NGLayoutResult> RelayoutIgnoringChildScrollbarChanges();
  scoped_refptr<const NGLayoutResult> LayoutInternal();

  bool DoesItemCrossSizeComputeToAuto(const NGBlockNode& child) const;
  bool IsItemFlexBasisDefinite(const NGBlockNode& child) const;
  bool IsItemMainSizeDefinite(const NGBlockNode& child) const;
  bool IsItemCrossAxisLengthDefinite(const NGBlockNode& child,
                                     const Length& length) const;
  bool ShouldItemShrinkToFit(const NGBlockNode& child) const;
  double GetMainOverCrossAspectRatio(const NGBlockNode& child) const;
  bool DoesItemStretch(const NGBlockNode& child) const;
  // This implements the first of the additional scenarios where a flex item
  // has definite sizes when it would not if it weren't a flex item.
  // https://drafts.csswg.org/css-flexbox/#definite-sizes
  bool WillChildCrossSizeBeContainerCrossSize(const NGBlockNode& child) const;
  LayoutUnit AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
      const NGBlockNode& child,
      LayoutUnit content_suggestion,
      LayoutUnit cross_min,
      LayoutUnit cross_max,
      LayoutUnit main_axis_border_padding,
      LayoutUnit cross_axis_border_padding);

  bool IsColumnContainerMainSizeDefinite() const;
  bool IsContainerCrossSizeDefinite() const;

  LayoutUnit CalculateFixedCrossSize(const MinMaxSizes& cross_axis_min_max,
                                     const NGBoxStrut& margins) const;

  NGConstraintSpace BuildSpaceForFlexBasis(const NGBlockNode& flex_item) const;
  NGConstraintSpace BuildSpaceForIntrinsicBlockSize(
      const NGBlockNode& flex_item,
      const NGPhysicalBoxStrut& physical_margins,
      const MinMaxSizes& cross_axis) const;
  void ConstructAndAppendFlexItems();
  void ApplyStretchAlignmentToChild(FlexItem& flex_item);
  bool GiveLinesAndItemsFinalPositionAndSize();
  void LayoutColumnReverse(LayoutUnit main_axis_content_size);

  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(const NGBlockNode& child) const;
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size) const;

  void HandleOutOfFlowPositioned(NGBlockNode child);

  void AdjustButtonBaseline(LayoutUnit final_content_cross_size);

  // Propagates the baseline from the given flex-item if needed.
  void PropagateBaselineFromChild(
      const FlexItem&,
      const NGBoxFragment&,
      LayoutUnit block_offset,
      base::Optional<LayoutUnit>* fallback_baseline);

  const bool is_column_;
  const bool is_horizontal_flow_;
  const bool is_cross_size_definite_;
  bool ignore_child_scrollbar_changes_ = false;
  LogicalSize border_box_size_;
  LogicalSize child_percentage_size_;
  base::Optional<FlexLayoutAlgorithm> algorithm_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
