// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_break_token_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"

namespace blink {

class NGBlockNode;
class NGBlockBreakToken;
struct DevtoolsFlexInfo;
struct NGFlexItem;

class CORE_EXPORT NGFlexLayoutAlgorithm
    : public NGLayoutAlgorithm<NGBlockNode,
                               NGBoxFragmentBuilder,
                               NGBlockBreakToken> {
 public:
  explicit NGFlexLayoutAlgorithm(
      const NGLayoutAlgorithmParams& params,
      const HashMap<wtf_size_t, LayoutUnit>* cross_size_adjustments = nullptr);

  MinMaxSizesResult ComputeMinMaxSizes(const MinMaxSizesFloatInput&) override;
  const NGLayoutResult* Layout() override;

 private:
  const NGLayoutResult* RelayoutIgnoringChildScrollbarChanges();
  const NGLayoutResult* RelayoutAndBreakEarlierForFlex(
      const NGLayoutResult* previous_result);
  const NGLayoutResult* LayoutInternal();

  void PlaceFlexItems(
      HeapVector<NGFlexLine>* flex_line_outputs,
      HeapVector<Member<LayoutBox>>* oof_children,
      bool is_computing_multiline_column_intrinsic_size = false);

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
  LayoutUnit AdjustMainSizeForAspectRatioCrossAxisMinAndMax(
      const NGBlockNode& child,
      LayoutUnit main_size,
      const MinMaxSizes& cross_min_max,
      const NGBoxStrut& border_padding_in_child_writing_mode);

  bool IsColumnContainerMainSizeDefinite() const;
  bool IsContainerCrossSizeDefinite() const;

  enum class Phase { kLayout, kRowIntrinsicSize, kColumnWrapIntrinsicSize };
  NGConstraintSpace BuildSpaceForIntrinsicInlineSize(
      const NGBlockNode& flex_item) const;
  NGConstraintSpace BuildSpaceForFlexBasis(const NGBlockNode& flex_item) const;
  NGConstraintSpace BuildSpaceForIntrinsicBlockSize(
      const NGBlockNode& flex_item,
      absl::optional<LayoutUnit> override_inline_size,
      Phase phase) const;
  // |line_cross_size_for_stretch| should only be set when running the final
  // layout pass for stretch, when the line cross size is definite.
  // |block_offset_for_fragmentation| should only be set when running the final
  // layout pass for fragmentation. Both may be set at the same time.
  NGConstraintSpace BuildSpaceForLayout(
      const NGBlockNode& flex_item_node,
      LayoutUnit item_main_axis_final_size,
      absl::optional<LayoutUnit> override_inline_size = absl::nullopt,
      absl::optional<LayoutUnit> line_cross_size_for_stretch = absl::nullopt,
      absl::optional<LayoutUnit> block_offset_for_fragmentation = absl::nullopt,
      bool min_block_size_should_encompass_intrinsic_size = false) const;

  void ConstructAndAppendFlexItems(
      Phase phase,
      HeapVector<Member<LayoutBox>>* oof_children = nullptr);
  void ApplyFinalAlignmentAndReversals(
      HeapVector<NGFlexLine>* flex_line_outputs);
  NGLayoutResult::EStatus GiveItemsFinalPositionAndSize(
      HeapVector<NGFlexLine>* flex_line_outputs,
      Vector<EBreakBetween>* row_break_between_outputs);
  NGLayoutResult::EStatus GiveItemsFinalPositionAndSizeForFragmentation(
      HeapVector<NGFlexLine>* flex_line_outputs,
      Vector<EBreakBetween>* row_break_between_outputs,
      NGFlexBreakTokenData::NGFlexBreakBeforeRow* break_before_row);
  NGLayoutResult::EStatus PropagateFlexItemInfo(FlexItem* flex_item,
                                                wtf_size_t flex_line_idx,
                                                LogicalOffset offset,
                                                PhysicalSize fragment_size);
  void LayoutColumnReverse(LayoutUnit main_axis_content_size);

  // This is same method as FlexItem but we need that logic before FlexItem is
  // constructed.
  bool MainAxisIsInlineAxis(const NGBlockNode& child) const;
  LayoutUnit MainAxisContentExtent(LayoutUnit sum_hypothetical_main_size) const;

  void HandleOutOfFlowPositionedItems(
      HeapVector<Member<LayoutBox>>& oof_children);

  void AdjustButtonBaseline(LayoutUnit final_content_cross_size);

  MinMaxSizesResult ComputeMinMaxSizeOfRowContainer();
  MinMaxSizesResult ComputeMinMaxSizeOfMultilineColumnContainer();
  // This implements 9.9.3. Flex Item Intrinsic Size Contributions, from
  // https://drafts.csswg.org/css-flexbox/#intrinsic-item-contributions.
  MinMaxSizesResult ComputeItemContributions(const NGConstraintSpace& space,
                                             const FlexItem& item) const;

  // Return the amount of block space available in the current fragmentainer
  // for the node being laid out by this algorithm.
  LayoutUnit FragmentainerSpaceAvailable(LayoutUnit block_offset) const;

  // Consume all remaining fragmentainer space. This happens when we decide to
  // break before a child.
  //
  // https://www.w3.org/TR/css-break-3/#box-splitting
  void ConsumeRemainingFragmentainerSpace(
      LayoutUnit previously_consumed_block_size,
      NGFlexLine* flex_line,
      const NGFlexColumnBreakInfo* column_break_info = nullptr);

  // Insert a fragmentainer break before a row if necessary. Rows do not produce
  // a layout result, so when breaking before a row, we will insert a
  // fragmentainer break before the first child in a row. |child| should be
  // those associated with the first child in the row. |row|,
  // |row_block_offset|, |row_break_between|, |row_index|,
  // |has_container_separation| and |is_first_for_row| are specific to the row
  // itself. See
  // |::blink::BreakBeforeChildIfNeeded()| for more documentation.
  NGBreakStatus BreakBeforeRowIfNeeded(const NGFlexLine& row,
                                       LayoutUnit row_block_offset,
                                       EBreakBetween row_break_between,
                                       wtf_size_t row_index,
                                       NGLayoutInputNode child,
                                       bool has_container_separation,
                                       bool is_first_for_row);

  // Move past the breakpoint before the row, if possible, and return true. Also
  // update the appeal of breaking before the row (if we're not going
  // to break before it). If false is returned, it means that we need to break
  // before the row (or even earlier). See |::blink::MovePastBreakpoint()| for
  // more documentation.
  bool MovePastRowBreakPoint(NGBreakAppeal appeal_before,
                             LayoutUnit fragmentainer_block_offset,
                             LayoutUnit row_block_size,
                             wtf_size_t row_index,
                             bool has_container_separation,
                             bool breakable_at_start_of_container);

  // Add an early break for the column at the provided |index|.
  void AddColumnEarlyBreak(NGEarlyBreak* breakpoint, wtf_size_t index);

  // Add the amount an item expanded by to the item offset adjustment of the
  // flex line at the index directly after |flex_line_idx|, if there is one.
  void AdjustOffsetForNextLine(HeapVector<NGFlexLine>* flex_line_outputs,
                               wtf_size_t flex_line_idx,
                               LayoutUnit item_expansion) const;

  // If a flex item expands past the row cross-size as a result of
  // fragmentation, we will abort and re-run layout with the appropriate row
  // cross-size adjustments.
  const NGLayoutResult* RelayoutWithNewRowSizes();

  // Used to determine when to allow an item to expand as a result of
  // fragmentation.
  bool MinBlockSizeShouldEncompassIntrinsicSize(const NGFlexItem& item) const;

#if DCHECK_IS_ON()
  void CheckFlexLines(HeapVector<NGFlexLine>& flex_line_outputs) const;
#endif

  // Used when determining the max-content width of a column-wrap flex
  // container.
  LayoutUnit largest_min_content_contribution_;

  const bool is_column_;
  const bool is_horizontal_flow_;
  const bool is_cross_size_definite_;
  const LogicalSize child_percentage_size_;

  bool has_column_percent_flex_basis_ = false;
  bool ignore_child_scrollbar_changes_ = false;

  // This will be set during block fragmentation once we've processed the first
  // flex item in a given line. It is used to check if we're at a valid class A
  // or C breakpoint within a column flex container.
  wtf_size_t last_line_idx_to_process_first_child_ = kNotFound;
  // This will be set during block fragmentation once we've processed the first
  // flex line. It is used to check if we're at a valid class A or C breakpoint
  // within a row flex container.
  bool has_processed_first_line_ = false;

  FlexLayoutAlgorithm algorithm_;
  std::unique_ptr<DevtoolsFlexInfo> layout_info_for_devtools_;

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

  // Only one early break is supported per container. However, we may need to
  // return to an early break within multiple flex columns. This stores the
  // early breaks per column to be used when aborting layout.
  HeapVector<Member<NGEarlyBreak>> column_early_breaks_;

  // If an item expands past the row block-end, we will re-run layout with the
  // new cross size. Keep track of each such flex line index mapped to how much
  // it should expand by in the next layout pass.
  HashMap<wtf_size_t, LayoutUnit> row_cross_size_updates_;

  // If set, that means that we are re-running layout with updated row
  // cross-sizes. See |row_cross_size_updates_| for what this maps to.
  const HashMap<wtf_size_t, LayoutUnit>* cross_size_adjustments_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_FLEX_NG_FLEX_LAYOUT_ALGORITHM_H_
