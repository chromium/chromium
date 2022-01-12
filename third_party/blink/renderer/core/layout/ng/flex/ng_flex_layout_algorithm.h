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
struct DevtoolsFlexInfo;
struct NGFlexItem;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGFlexLayoutAlgorithm(const NGLayoutAlgorithmParams& params,
                                 DevtoolsFlexInfo* devtools = nullptr);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;
  scoped_refptr<const NGLayoutResult> Layout() override;

 private:
  scoped_refptr<const NGLayoutResult> RelayoutIgnoringChildScrollbarChanges();
  scoped_refptr<const NGLayoutResult> LayoutInternal();

  void PlaceFlexItems(Vector<NGFlexLine>* flex_line_outputs);
  void CalculateTotalIntrinsicBlockSize(bool use_empty_line_block_size);

  Length GetUsedFlexBasis(const NGBlockNode& child) const;
  // This has an optional out parameter so that callers can avoid a subsequent
  // redundant call to GetUsedFlexBasis.
  bool IsUsedFlexBasisDefinite(const NGBlockNode& child,
                               Length* flex_basis) const;
  bool DoesItemCrossSizeComputeToAuto(const NGBlockNode& child) const;
  bool IsItemCrossAxisLengthDefinite(const NGBlockNode& child,
                                     const Length& length) const;
  bool AspectRatioProvidesMainSize(const NGBlockNode& child) const;
  bool DoesItemStretch(const NGBlockNode& child) const;
  // This checks for one of the scenarios where a flex-item box has a definite
  // size that would be indefinite if the box weren't a flex item.
  // See https://drafts.csswg.org/css-flexbox/#definite-sizes
  bool WillChildCrossSizeBeContainerCrossSize(const NGBlockNode& child) const;
  LayoutUnit AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
      const NGBlockNode& child,
      LayoutUnit content_suggestion,
      const MinMaxSizes& cross_min_max,
      const NGBoxStrut& border_padding_in_child_writing_mode);

  bool IsColumnContainerMainSizeDefinite() const;
  bool IsContainerCrossSizeDefinite() const;

  NGConstraintSpace BuildSpaceForFlexBasis(const NGBlockNode& flex_item) const;
  NGConstraintSpace BuildSpaceForIntrinsicBlockSize(
      const NGBlockNode& flex_item) const;
  // |line_cross_size_for_stretch| should only be set when running the final
  // layout pass for stretch, when the line cross size is definite.
  // |block_offset_for_fragmentation| should only be set when running the final
  // layout pass for fragmentation. Both may be set at the same time.
  NGConstraintSpace BuildSpaceForLayout(
      const NGBlockNode& flex_item_node,
      LayoutUnit item_main_axis_final_size,
      absl::optional<LayoutUnit> line_cross_size_for_stretch = absl::nullopt,
      absl::optional<LayoutUnit> block_offset_for_fragmentation = absl::nullopt,
      bool min_block_size_should_encompass_intrinsic_size = false) const;
  void ConstructAndAppendFlexItems();
  void ApplyFinalAlignmentAndReversals(Vector<NGFlexLine>* flex_line_outputs);
  NGLayoutResult::EStatus GiveItemsFinalPositionAndSize(
      Vector<NGFlexLine>* flex_line_outputs);
  NGLayoutResult::EStatus GiveItemsFinalPositionAndSizeForFragmentation(
      Vector<NGFlexLine>* flex_line_outputs);
  NGLayoutResult::EStatus PropagateFlexItemInfo(FlexItem* flex_item,
                                                wtf_size_t flex_line_idx,
                                                LogicalOffset offset,
                                                PhysicalSize fragment_size);
  void LayoutColumnReverse(LayoutUnit main_axis_content_size);

  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(const NGBlockNode& child) const;
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size) const;

  void HandleOutOfFlowPositioned(NGBlockNode child);

  void AdjustButtonBaseline(LayoutUnit final_content_cross_size);

  // Propagates the baseline from the given flex-item if needed.
  void PropagateBaselineFromChild(
      const ComputedStyle&,
      const NGBoxFragment&,
      LayoutUnit block_offset,
      absl::optional<LayoutUnit>* fallback_baseline);

  // Return the amount of block space available in the current fragmentainer
  // for the node being laid out by this algorithm.
  LayoutUnit FragmentainerSpaceAvailable(LayoutUnit block_offset) const;

  // Consume all remaining fragmentainer space. This happens when we decide to
  // break before a child.
  //
  // https://www.w3.org/TR/css-break-3/#box-splitting
  void ConsumeRemainingFragmentainerSpace(LogicalOffset item_offset,
                                          NGFlexItem* flex_item);

#if DCHECK_IS_ON()
  void CheckFlexLines(const Vector<NGFlexLine>& flex_line_outputs) const;
#endif

  const bool is_column_;
  const bool is_horizontal_flow_;
  const bool is_cross_size_definite_;
  const LogicalSize child_percentage_size_;

  bool has_column_percent_flex_basis_ = false;
  bool ignore_child_scrollbar_changes_ = false;
  bool involved_in_block_fragmentation_ = false;

  // This will be set during block fragmentation once we've processed the first
  // flex item in a given line. It is used to check if we're at a valid class A
  // or C breakpoint within a column flex container.
  wtf_size_t last_line_idx_to_process_first_child_ = kNotFound;
  // This will be set during block fragmentation once we've processed the first
  // flex line. It is used to check if we're at a valid class A or C breakpoint
  // within a row flex container.
  bool has_processed_first_line_ = false;

  FlexLayoutAlgorithm algorithm_;
  DevtoolsFlexInfo* layout_info_for_devtools_;

  // The block size of the entire flex container (ignoring any fragmentation).
  LayoutUnit total_block_size_;
  // This will be the intrinsic block size in the current fragmentainer, if
  // inside a fragmentation context. Otherwise, it will represent the intrinsic
  // block size for the entire flex container.
  LayoutUnit intrinsic_block_size_;
  // The intrinsic block size for the entire flex container. When not
  // fragmenting, |total_intrinsic_block_size| and |intrinsic_block_size_| will
  // be equivalent.
  LayoutUnit total_intrinsic_block_size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
