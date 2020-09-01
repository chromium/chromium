// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_BUILDER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGExclusionSpace;

class CORE_EXPORT NGConstraintSpaceBuilder final {
  STACK_ALLOCATED();

 public:
  // The setters on this builder are in the writing mode of parent_space.
  NGConstraintSpaceBuilder(const NGConstraintSpace& parent_space,
                           WritingMode out_writing_mode,
                           bool is_new_fc)
      : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                                 out_writing_mode,
                                 is_new_fc) {
    if (parent_space.IsInsideBalancedColumns())
      space_.EnsureRareData()->is_inside_balanced_columns = true;
  }

  // The setters on this builder are in the writing mode of parent_writing_mode.
  //
  // forced_orthogonal_writing_mode_root is set for constraint spaces created
  // directly from a LayoutObject. In this case parent_writing_mode isn't
  // actually the parent's, it's the same as out_writing_mode.
  // When this occurs we would miss setting the kOrthogonalWritingModeRoot flag
  // unless we force it.
  NGConstraintSpaceBuilder(WritingMode parent_writing_mode,
                           WritingMode out_writing_mode,
                           bool is_new_fc,
                           bool force_orthogonal_writing_mode_root = false)
      : space_(out_writing_mode),
        is_in_parallel_flow_(
            IsParallelWritingMode(parent_writing_mode, out_writing_mode)),
        is_new_fc_(is_new_fc),
        force_orthogonal_writing_mode_root_(
            force_orthogonal_writing_mode_root) {
    space_.bitfields_.is_new_formatting_context = is_new_fc_;
    space_.bitfields_.is_orthogonal_writing_mode_root =
        !is_in_parallel_flow_ || force_orthogonal_writing_mode_root_;
  }

  // If inline size is indefinite, use the fallback size for available inline
  // size for orthogonal flow roots. See:
  // https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
  void AdjustInlineSizeIfNeeded(LayoutUnit* inline_size) const {
    DCHECK(!is_in_parallel_flow_);
    if (*inline_size != kIndefiniteSize)
      return;
    DCHECK_NE(orthogonal_fallback_inline_size_, kIndefiniteSize);
    *inline_size = orthogonal_fallback_inline_size_;
  }

  void SetAvailableSize(LogicalSize available_size) {
#if DCHECK_IS_ON()
    is_available_size_set_ = true;
#endif
    space_.available_size_ = available_size;

    if (UNLIKELY(!is_in_parallel_flow_)) {
      space_.available_size_.Transpose();
      AdjustInlineSizeIfNeeded(&space_.available_size_.inline_size);
    }
  }

  // Set percentage resolution size. Prior to calling this method,
  // SetAvailableSize() must have been called, since we'll compare the input
  // against the available size set, because if they are equal in either
  // dimension, we won't have to store the values separately.
  void SetPercentageResolutionSize(LogicalSize percentage_resolution_size);

  // Set percentage resolution size for replaced content (a special quirk inside
  // tables). Only honored if the writing modes (container vs. child) are
  // parallel. In orthogonal writing modes, we'll use whatever regular
  // percentage resolution size is already set. Prior to calling this method,
  // SetAvailableSize() must have been called, since we'll compare the input
  // against the available size set, because if they are equal in either
  // dimension, we won't have to store the values separately. Additionally,
  // SetPercentageResolutionSize() must have been called, since we'll override
  // with that value on orthogonal writing mode roots.
  void SetReplacedPercentageResolutionSize(
      LogicalSize replaced_percentage_resolution_size);

  // Set the fallback available inline-size for an orthogonal child. The size is
  // the inline size in the writing mode of the orthogonal child.
  void SetOrthogonalFallbackInlineSize(LayoutUnit size) {
    orthogonal_fallback_inline_size_ = size;
  }

  void SetFragmentainerBlockSize(LayoutUnit size) {
#if DCHECK_IS_ON()
    DCHECK(!is_fragmentainer_block_size_set_);
    is_fragmentainer_block_size_set_ = true;
#endif
    if (size != kIndefiniteSize)
      space_.EnsureRareData()->fragmentainer_block_size = size;
  }

  void SetFragmentainerOffsetAtBfc(LayoutUnit offset) {
#if DCHECK_IS_ON()
    DCHECK(!is_fragmentainer_offset_at_bfc_set_);
    is_fragmentainer_offset_at_bfc_set_ = true;
#endif
    if (offset != LayoutUnit())
      space_.EnsureRareData()->fragmentainer_offset_at_bfc = offset;
  }

  void SetTextDirection(TextDirection direction) {
    space_.bitfields_.direction = static_cast<unsigned>(direction);
  }

  void SetIsFixedInlineSize(bool b) {
    if (LIKELY(is_in_parallel_flow_))
      space_.bitfields_.is_fixed_inline_size = b;
    else
      space_.bitfields_.is_fixed_block_size = b;
  }

  void SetIsFixedBlockSize(bool b) {
    if (LIKELY(is_in_parallel_flow_))
      space_.bitfields_.is_fixed_block_size = b;
    else
      space_.bitfields_.is_fixed_inline_size = b;
  }

  void SetIsFixedBlockSizeIndefinite(bool b) {
    if (LIKELY(is_in_parallel_flow_ || !force_orthogonal_writing_mode_root_))
      space_.bitfields_.is_fixed_block_size_indefinite = b;
  }

  void SetIsShrinkToFit(bool b) { space_.bitfields_.is_shrink_to_fit = b; }

  void SetIsPaintedAtomically(bool b) {
    space_.bitfields_.is_painted_atomically = b;
  }

  void SetFragmentationType(NGFragmentationType fragmentation_type) {
#if DCHECK_IS_ON()
    DCHECK(!is_block_direction_fragmentation_type_set_);
    is_block_direction_fragmentation_type_set_ = true;
#endif
    if (fragmentation_type != NGFragmentationType::kFragmentNone) {
      space_.EnsureRareData()->block_direction_fragmentation_type =
          fragmentation_type;
    }
  }

  void SetIsInsideBalancedColumns() {
    space_.EnsureRareData()->is_inside_balanced_columns = true;
  }

  void SetIsInColumnBfc() { space_.EnsureRareData()->is_in_column_bfc = true; }

  void SetEarlyBreakAppeal(NGBreakAppeal appeal) {
    if (appeal == kBreakAppealLastResort && !space_.rare_data_)
      return;
    space_.EnsureRareData()->early_break_appeal = appeal;
  }

  // is_legacy_table_cell must always be assigned if is_table_cell is true.
  void SetIsTableCell(bool is_table_cell, bool is_legacy_table_cell) {
    space_.bitfields_.is_table_cell = is_table_cell;
    space_.bitfields_.is_legacy_table_cell = is_legacy_table_cell;
  }

  void SetIsRestrictedBlockSizeTableCell(bool b) {
    DCHECK(space_.bitfields_.is_table_cell);
    if (!b && !space_.rare_data_)
      return;
    space_.EnsureRareData()->is_restricted_block_size_table_cell = b;
  }

  void SetHideTableCellIfEmpty(bool b) {
    if (!b && !space_.rare_data_)
      return;
    space_.EnsureRareData()->hide_table_cell_if_empty = b;
  }

  void SetIsAnonymous(bool b) { space_.bitfields_.is_anonymous = b; }

  void SetUseFirstLineStyle(bool b) {
    space_.bitfields_.use_first_line_style = b;
  }

  void SetAdjoiningObjectTypes(NGAdjoiningObjectTypes adjoining_object_types) {
    if (!is_new_fc_) {
      space_.bitfields_.adjoining_object_types =
          static_cast<unsigned>(adjoining_object_types);
    }
  }

  void SetAncestorHasClearancePastAdjoiningFloats() {
    space_.bitfields_.ancestor_has_clearance_past_adjoining_floats = true;
  }

  void SetNeedsBaseline(bool b) { space_.bitfields_.needs_baseline = b; }

  void SetBaselineAlgorithmType(NGBaselineAlgorithmType type) {
    space_.bitfields_.baseline_algorithm_type = static_cast<unsigned>(type);
  }

  void SetCacheSlot(NGCacheSlot slot) {
    space_.bitfields_.cache_slot = static_cast<unsigned>(slot);
  }

  void SetBlockStartAnnotationSpace(LayoutUnit space) {
    if (space)
      space_.EnsureRareData()->SetBlockStartAnnotationSpace(space);
  }

  void SetMarginStrut(const NGMarginStrut& margin_strut) {
#if DCHECK_IS_ON()
    DCHECK(!is_margin_strut_set_);
    is_margin_strut_set_ = true;
#endif
    if (!is_new_fc_ && margin_strut != NGMarginStrut())
      space_.EnsureRareData()->SetMarginStrut(margin_strut);
  }

  // Set up a margin strut that discards all adjoining margins. This is used to
  // discard block-start margins after fragmentainer breaks.
  void SetDiscardingMarginStrut() {
#if DCHECK_IS_ON()
    DCHECK(!is_margin_strut_set_);
    is_margin_strut_set_ = true;
#endif
    NGMarginStrut discarding_margin_strut;
    discarding_margin_strut.discard_margins = true;
    space_.EnsureRareData()->SetMarginStrut(discarding_margin_strut);
  }

  void SetBfcOffset(const NGBfcOffset& bfc_offset) {
    if (!is_new_fc_) {
      if (space_.HasRareData())
        space_.rare_data_->bfc_offset = bfc_offset;
      else
        space_.bfc_offset_ = bfc_offset;
    }
  }

  void SetOptimisticBfcBlockOffset(LayoutUnit optimistic_bfc_block_offset) {
#if DCHECK_IS_ON()
    DCHECK(!is_optimistic_bfc_block_offset_set_);
    is_optimistic_bfc_block_offset_set_ = true;
#endif
    if (LIKELY(!is_new_fc_)) {
      space_.EnsureRareData()->SetOptimisticBfcBlockOffset(
          optimistic_bfc_block_offset);
    }
  }

  void SetForcedBfcBlockOffset(LayoutUnit forced_bfc_block_offset) {
#if DCHECK_IS_ON()
    DCHECK(!is_forced_bfc_block_offset_set_);
    is_forced_bfc_block_offset_set_ = true;
#endif
    DCHECK(!is_new_fc_);
    space_.EnsureRareData()->SetForcedBfcBlockOffset(forced_bfc_block_offset);
  }

  void SetClearanceOffset(LayoutUnit clearance_offset) {
#if DCHECK_IS_ON()
    DCHECK(!is_clearance_offset_set_);
    is_clearance_offset_set_ = true;
#endif
    if (!is_new_fc_ && clearance_offset != LayoutUnit::Min())
      space_.EnsureRareData()->SetClearanceOffset(clearance_offset);
  }

  void SetTableCellBorders(const NGBoxStrut& table_cell_borders) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_cell_borders_set_);
    is_table_cell_borders_set_ = true;
#endif
    if (table_cell_borders != NGBoxStrut())
      space_.EnsureRareData()->SetTableCellBorders(table_cell_borders);
  }

  void SetTableCellIntrinsicPadding(
      const NGBoxStrut& table_cell_intrinsic_padding) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_cell_intrinsic_padding_set_);
    is_table_cell_intrinsic_padding_set_ = true;
#endif
    if (table_cell_intrinsic_padding != NGBoxStrut()) {
      space_.EnsureRareData()->SetTableCellIntrinsicPadding(
          table_cell_intrinsic_padding);
    }
  }

  void SetTableCellAlignmentBaseline(
      const base::Optional<LayoutUnit>& table_cell_alignment_baseline) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_cell_alignment_baseline_set_);
    is_table_cell_alignment_baseline_set_ = true;
#endif
    if (is_in_parallel_flow_ && table_cell_alignment_baseline) {
      space_.EnsureRareData()->SetTableCellAlignmentBaseline(
          *table_cell_alignment_baseline);
    }
  }

  void SetTableCellColumnIndex(wtf_size_t column_index) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_cell_column_index_set_);
    is_table_cell_column_index_set_ = true;
#endif
    space_.EnsureRareData()->SetTableCellColumnIndex(column_index);
  }

  void SetIsTableCellHiddenForPaint(bool is_hidden_for_paint) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_cell_hidden_for_paint_set_);
    is_table_cell_hidden_for_paint_set_ = true;
#endif
    space_.EnsureRareData()->SetIsTableCellHiddenForPaint(is_hidden_for_paint);
  }

  void SetTableCellChildLayoutMode(
      NGTableCellChildLayoutMode table_cell_child_layout_mode) {
    space_.bitfields_.table_cell_child_layout_mode =
        static_cast<unsigned>(table_cell_child_layout_mode);
  }

  void SetExclusionSpace(const NGExclusionSpace& exclusion_space) {
    if (!is_new_fc_)
      space_.exclusion_space_ = exclusion_space;
  }

  void SetCustomLayoutData(
      scoped_refptr<SerializedScriptValue> custom_layout_data) {
#if DCHECK_IS_ON()
    DCHECK(!is_custom_layout_data_set_);
    is_custom_layout_data_set_ = true;
#endif
    if (custom_layout_data) {
      space_.EnsureRareData()->SetCustomLayoutData(
          std::move(custom_layout_data));
    }
  }

  void SetTableRowData(const NGTableConstraintSpaceData* table_data,
                       wtf_size_t row_index) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_row_data_set_);
    is_table_row_data_set_ = true;
#endif
    space_.EnsureRareData()->SetTableRowData(std::move(table_data), row_index);
  }

  void SetTableSectionData(
      scoped_refptr<const NGTableConstraintSpaceData> table_data,
      wtf_size_t section_index) {
#if DCHECK_IS_ON()
    DCHECK(!is_table_section_data_set_);
    is_table_section_data_set_ = true;
#endif
    space_.EnsureRareData()->SetTableSectionData(std::move(table_data),
                                                 section_index);
  }

  void SetLinesUntilClamp(const base::Optional<int>& clamp) {
#if DCHECK_IS_ON()
    DCHECK(!is_lines_until_clamp_set_);
    is_lines_until_clamp_set_ = true;
#endif
    DCHECK(!is_new_fc_);
    if (clamp)
      space_.EnsureRareData()->SetLinesUntilClamp(*clamp);
  }

  void SetTargetStretchInlineSize(LayoutUnit target_stretch_inline_size) {
    DCHECK_GE(target_stretch_inline_size, LayoutUnit());
    space_.EnsureRareData()->SetTargetStretchInlineSize(
        target_stretch_inline_size);
  }

  void SetTargetStretchAscentSize(LayoutUnit target_stretch_ascent_size) {
    DCHECK_GE(target_stretch_ascent_size, LayoutUnit());
    space_.EnsureRareData()->SetTargetStretchAscentSize(
        target_stretch_ascent_size);
  }

  void SetTargetStretchDescentSize(LayoutUnit target_stretch_descent_size) {
    DCHECK_GE(target_stretch_descent_size, LayoutUnit());
    space_.EnsureRareData()->SetTargetStretchDescentSize(
        target_stretch_descent_size);
  }

  // Creates a new constraint space.
  const NGConstraintSpace ToConstraintSpace() {
#if DCHECK_IS_ON()
    DCHECK(!to_constraint_space_called_)
        << "ToConstraintSpace should only be called once.";
    to_constraint_space_called_ = true;
#endif

    DCHECK(!is_new_fc_ || !space_.bitfields_.adjoining_object_types);
    DCHECK_EQ(space_.bitfields_.is_orthogonal_writing_mode_root,
              !is_in_parallel_flow_ || force_orthogonal_writing_mode_root_);

    DCHECK(!force_orthogonal_writing_mode_root_ || is_in_parallel_flow_)
        << "Forced and inferred orthogonal writing mode shouldn't happen "
           "simultaneously. Inferred means the constraints are in parent "
           "writing mode, forced means they are in child writing mode.";

    return std::move(space_);
  }

 private:
  NGConstraintSpace space_;

  // Orthogonal writing mode roots may need a fallback, to prevent available
  // inline size from being indefinite, which isn't allowed. This is the
  // available inline size in the writing mode of the orthogonal child.
  LayoutUnit orthogonal_fallback_inline_size_ = kIndefiniteSize;

  bool is_in_parallel_flow_;
  bool is_new_fc_;
  bool force_orthogonal_writing_mode_root_;

#if DCHECK_IS_ON()
  bool is_available_size_set_ = false;
  bool is_percentage_resolution_size_set_ = false;
  bool is_fragmentainer_block_size_set_ = false;
  bool is_fragmentainer_offset_at_bfc_set_ = false;
  bool is_block_direction_fragmentation_type_set_ = false;
  bool is_margin_strut_set_ = false;
  bool is_optimistic_bfc_block_offset_set_ = false;
  bool is_forced_bfc_block_offset_set_ = false;
  bool is_clearance_offset_set_ = false;
  bool is_table_cell_borders_set_ = false;
  bool is_table_cell_intrinsic_padding_set_ = false;
  bool is_table_cell_alignment_baseline_set_ = false;
  bool is_table_cell_column_index_set_ = false;
  bool is_table_cell_hidden_for_paint_set_ = false;
  bool is_custom_layout_data_set_ = false;
  bool is_lines_until_clamp_set_ = false;
  bool is_table_row_data_set_ = false;
  bool is_table_section_data_set_ = false;

  bool to_constraint_space_called_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_BUILDER_H_
