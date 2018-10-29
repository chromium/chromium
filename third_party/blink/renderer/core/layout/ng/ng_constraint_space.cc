// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

namespace blink {

NGConstraintSpace::NGConstraintSpace(WritingMode out_writing_mode,
                                     bool is_new_fc,
                                     NGConstraintSpaceBuilder& builder)
    : available_size_(builder.available_size_),
      percentage_resolution_size_(builder.percentage_resolution_size_),
      replaced_percentage_resolution_size_(
          builder.replaced_percentage_resolution_size_),
      initial_containing_block_size_(builder.initial_containing_block_size_),
      fragmentainer_block_size_(builder.fragmentainer_block_size_),
      fragmentainer_space_at_bfc_start_(
          builder.fragmentainer_space_at_bfc_start_),
      block_direction_fragmentation_type_(builder.fragmentation_type_),
      table_cell_child_layout_phase_(builder.table_cell_child_layout_phase_),
      adjoining_floats_(builder.adjoining_floats_),
      writing_mode_(static_cast<unsigned>(out_writing_mode)),
      direction_(static_cast<unsigned>(builder.text_direction_)),
      flags_(builder.flags_),
      margin_strut_(is_new_fc ? NGMarginStrut() : builder.margin_strut_),
      bfc_offset_(is_new_fc ? NGBfcOffset() : builder.bfc_offset_),
      floats_bfc_block_offset_(is_new_fc ? base::nullopt
                                         : builder.floats_bfc_block_offset_),
      clearance_offset_(is_new_fc ? LayoutUnit::Min()
                                  : builder.clearance_offset_),
      baseline_requests_(std::move(builder.baseline_requests_)) {
  bool is_in_parallel_flow =
      IsParallelWritingMode(builder.parent_writing_mode_, out_writing_mode);

  DCHECK(!is_new_fc || !adjoining_floats_);

  auto SetResolvedFlag = [this](unsigned mask, bool value) {
    flags_ = (flags_ & ~static_cast<unsigned>(mask)) |
             (-(int32_t)value & static_cast<unsigned>(mask));
  };
  if (!is_in_parallel_flow) {
    available_size_.Flip();
    percentage_resolution_size_.Flip();
    replaced_percentage_resolution_size_.Flip();
    // Swap the fixed size block/inline flags
    bool fixed_size_block = flags_ & kFixedSizeBlock;
    bool fixed_size_inline = flags_ & kFixedSizeInline;
    SetResolvedFlag(kFixedSizeInline, fixed_size_block);
    SetResolvedFlag(kFixedSizeBlock, fixed_size_inline);
    SetResolvedFlag(kFixedSizeBlockIsDefinite, true);
    SetResolvedFlag(kOrthogonalWritingModeRoot, true);
  }
  DCHECK_EQ(flags_ & kOrthogonalWritingModeRoot, !is_in_parallel_flow);

  // For ConstraintSpace instances created from layout objects,
  // parent_writing_mode_ isn't actually the parent's, it's the same as the out
  // writing mode. So we miss setting kOrthogonalWritingModeRoot on such
  // constraint spaces unless it is forced.
  if (builder.force_orthogonal_writing_mode_root_) {
    DCHECK(is_in_parallel_flow)
        << "Forced and inferred ortho writing mode shouldn't happen "
           "simultaneously. Inferred means the constraints are in parent "
           "writing mode, forced means they are in child writing mode. "
           "parent_writing_mode_ = "
        << static_cast<int>(builder.parent_writing_mode_)
        << ", requested writing mode = " << static_cast<int>(out_writing_mode);
    SetResolvedFlag(kOrthogonalWritingModeRoot, true);
    SetResolvedFlag(kFixedSizeBlockIsDefinite, true);
  }

  // If inline size is indefinite, use size of initial containing block.
  // https://www.w3.org/TR/css-writing-modes-3/#orthogonal-auto
  if (available_size_.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      available_size_.inline_size = initial_containing_block_size_.width;
    } else {
      available_size_.inline_size = initial_containing_block_size_.height;
    }
  }
  if (percentage_resolution_size_.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      percentage_resolution_size_.inline_size =
          initial_containing_block_size_.width;
    } else {
      percentage_resolution_size_.inline_size =
          initial_containing_block_size_.height;
    }
  }
  if (replaced_percentage_resolution_size_.inline_size == NGSizeIndefinite) {
    DCHECK(!is_in_parallel_flow);
    if (out_writing_mode == WritingMode::kHorizontalTb) {
      replaced_percentage_resolution_size_.inline_size =
          initial_containing_block_size_.width;
    } else {
      replaced_percentage_resolution_size_.inline_size =
          initial_containing_block_size_.height;
    }
  }

  if (!is_new_fc && builder.exclusion_space_)
    exclusion_space_ = *builder.exclusion_space_;
}

NGConstraintSpace NGConstraintSpace::CreateFromLayoutObject(
    const LayoutBox& box) {
  auto writing_mode = box.StyleRef().GetWritingMode();
  bool parallel_containing_block = IsParallelWritingMode(
      box.ContainingBlock()->StyleRef().GetWritingMode(), writing_mode);
  bool fixed_inline = false, fixed_block = false;
  bool fixed_block_is_definite = true;

  LayoutUnit available_logical_width;
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalWidth()) {
    // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
    available_logical_width = box.OverrideContainingBlockContentLogicalWidth();
  } else if (!parallel_containing_block &&
             box.HasOverrideContainingBlockContentLogicalHeight()) {
    available_logical_width = box.OverrideContainingBlockContentLogicalHeight();
  } else {
    if (parallel_containing_block)
      available_logical_width = box.ContainingBlockLogicalWidthForContent();
    else
      available_logical_width = box.PerpendicularContainingBlockLogicalHeight();
  }
  available_logical_width = std::max(LayoutUnit(), available_logical_width);

  LayoutUnit available_logical_height;
  if (parallel_containing_block &&
      box.HasOverrideContainingBlockContentLogicalHeight()) {
    // Grid layout sets OverrideContainingBlockContentLogicalWidth|Height
    available_logical_height =
        box.OverrideContainingBlockContentLogicalHeight();
  } else if (!parallel_containing_block &&
             box.HasOverrideContainingBlockContentLogicalWidth()) {
    available_logical_height = box.OverrideContainingBlockContentLogicalWidth();
  } else {
    if (!box.Parent()) {
      available_logical_height = box.View()->ViewLogicalHeightForPercentages();
    } else if (box.ContainingBlock()) {
      if (parallel_containing_block) {
        available_logical_height =
            box.ContainingBlockLogicalHeightForPercentageResolution();
      } else {
        available_logical_height = box.ContainingBlockLogicalWidthForContent();
      }
    }
  }
  NGLogicalSize percentage_size = {available_logical_width,
                                   available_logical_height};
  NGLogicalSize available_size = percentage_size;
  if (box.HasOverrideLogicalWidth()) {
    available_size.inline_size = box.OverrideLogicalWidth();
    fixed_inline = true;
  }
  if (box.HasOverrideLogicalHeight()) {
    available_size.block_size = box.OverrideLogicalHeight();
    fixed_block = true;
  }
  if (box.IsFlexItem() && fixed_block) {
    fixed_block_is_definite =
        ToLayoutFlexibleBox(box.Parent())
            ->UseOverrideLogicalHeightForPerentageResolution(box);
  }

  bool is_new_fc = true;
  // TODO(ikilpatrick): This DCHECK needs to be enabled once we've switched
  // LayoutTableCell, etc over to LayoutNG.
  //
  // We currently need to "force" LayoutNG roots to be formatting contexts so
  // that floats have layout performed on them.
  //
  // DCHECK(is_new_fc,
  //  box.IsLayoutBlock() && ToLayoutBlock(box).CreatesNewFormattingContext());

  IntSize icb_size = box.View()->GetLayoutSize(kExcludeScrollbars);
  NGPhysicalSize initial_containing_block_size{LayoutUnit(icb_size.Width()),
                                               LayoutUnit(icb_size.Height())};

  // ICB cannot be indefinite by the spec.
  DCHECK_GE(initial_containing_block_size.width, LayoutUnit());
  DCHECK_GE(initial_containing_block_size.height, LayoutUnit());

  NGConstraintSpaceBuilder builder(writing_mode, initial_containing_block_size);

  if (!box.IsWritingModeRoot() || box.IsGridItem()) {
    // Add all types because we don't know which baselines will be requested.
    FontBaseline baseline_type = box.StyleRef().GetFontBaseline();
    bool synthesize_inline_block_baseline =
        box.IsLayoutBlock() &&
        ToLayoutBlock(box).UseLogicalBottomMarginEdgeForInlineBlockBaseline();
    if (!synthesize_inline_block_baseline) {
      builder.AddBaselineRequest(
          {NGBaselineAlgorithmType::kAtomicInline, baseline_type});
    }
    builder.AddBaselineRequest(
        {NGBaselineAlgorithmType::kFirstLine, baseline_type});
  }

  return builder.SetAvailableSize(available_size)
      .SetPercentageResolutionSize(percentage_size)
      .SetIsFixedSizeInline(fixed_inline)
      .SetIsFixedSizeBlock(fixed_block)
      .SetFixedSizeBlockIsDefinite(fixed_block_is_definite)
      .SetIsShrinkToFit(
          box.SizesLogicalWidthToFitContent(box.StyleRef().LogicalWidth()))
      .SetIsNewFormattingContext(is_new_fc)
      .SetTextDirection(box.StyleRef().Direction())
      .SetIsOrthogonalWritingModeRoot(!parallel_containing_block)
      .ToConstraintSpace(writing_mode);
}

bool NGConstraintSpace::operator==(const NGConstraintSpace& other) const {
  return available_size_ == other.available_size_ &&
         percentage_resolution_size_ == other.percentage_resolution_size_ &&
         replaced_percentage_resolution_size_ ==
             other.replaced_percentage_resolution_size_ &&
         initial_containing_block_size_ ==
             other.initial_containing_block_size_ &&
         fragmentainer_block_size_ == other.fragmentainer_block_size_ &&
         fragmentainer_space_at_bfc_start_ ==
             other.fragmentainer_space_at_bfc_start_ &&
         block_direction_fragmentation_type_ ==
             other.block_direction_fragmentation_type_ &&
         table_cell_child_layout_phase_ ==
             other.table_cell_child_layout_phase_ &&
         flags_ == other.flags_ &&
         adjoining_floats_ == other.adjoining_floats_ &&
         writing_mode_ == other.writing_mode_ &&
         direction_ == other.direction_ &&
         margin_strut_ == other.margin_strut_ &&
         bfc_offset_ == other.bfc_offset_ &&
         floats_bfc_block_offset_ == other.floats_bfc_block_offset_ &&
         exclusion_space_ == other.exclusion_space_ &&
         clearance_offset_ == other.clearance_offset_ &&
         baseline_requests_ == other.baseline_requests_;
}

String NGConstraintSpace::ToString() const {
  return String::Format("Offset: %s,%s Size: %sx%s Clearance: %s",
                        bfc_offset_.line_offset.ToString().Ascii().data(),
                        bfc_offset_.block_offset.ToString().Ascii().data(),
                        AvailableSize().inline_size.ToString().Ascii().data(),
                        AvailableSize().block_size.ToString().Ascii().data(),
                        HasClearanceOffset()
                            ? ClearanceOffset().ToString().Ascii().data()
                            : "none");
}

}  // namespace blink
