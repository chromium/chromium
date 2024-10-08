// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/column_spanner_path.h"
#include "third_party/blink/renderer/core/layout/exclusions/exclusion_space.h"
#include "third_party/blink/renderer/core/layout/inline/line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/positioned_float.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsLayoutResult
    : public GarbageCollected<SameSizeAsLayoutResult> {
  const ConstraintSpace space;
  Member<void*> physical_fragment;
  Member<void*> rare_data_;
  union {
    BfcOffset bfc_offset;
    BoxStrut oof_insets_for_get_computed_style;
  };
  LayoutUnit intrinsic_block_size;
  unsigned bitfields[1];
};

ASSERT_SIZE(LayoutResult, SameSizeAsLayoutResult);

}  // namespace

// static
const LayoutResult* LayoutResult::Clone(const LayoutResult& other) {
  return MakeGarbageCollected<LayoutResult>(
      other, PhysicalBoxFragment::Clone(
                 To<PhysicalBoxFragment>(other.GetPhysicalFragment())));
}

// static
const LayoutResult* LayoutResult::CloneWithPostLayoutFragments(
    const LayoutResult& other) {
  return MakeGarbageCollected<LayoutResult>(
      other, PhysicalBoxFragment::CloneWithPostLayoutFragments(
                 To<PhysicalBoxFragment>(other.GetPhysicalFragment())));
}

LayoutResult::LayoutResult(BoxFragmentBuilderPassKey passkey,
                           const PhysicalFragment* physical_fragment,
                           BoxFragmentBuilder* builder)
    : LayoutResult(std::move(physical_fragment),
                   static_cast<FragmentBuilder*>(builder)) {
  bitfields_.is_initial_block_size_indefinite =
      builder->is_initial_block_size_indefinite_;
  intrinsic_block_size_ = builder->intrinsic_block_size_;
  if (builder->custom_layout_data_) {
    EnsureRareData()->custom_layout_data =
        std::move(builder->custom_layout_data_);
  }
  if (builder->annotation_overflow_)
    EnsureRareData()->annotation_overflow = builder->annotation_overflow_;
  if (builder->block_end_annotation_space_) {
    EnsureRareData()->block_end_annotation_space =
        builder->block_end_annotation_space_;
  }

  if (builder->has_block_fragmentation_) {
    RareData* rare_data = EnsureRareData();

    rare_data->block_size_for_fragmentation =
        builder->block_size_for_fragmentation_;

    bitfields_.is_block_size_for_fragmentation_clamped =
        builder->is_block_size_for_fragmentation_clamped_;

    bitfields_.has_forced_break = builder->has_forced_break_;
  }
  bitfields_.is_truncated_by_fragmentation_line =
      builder->is_truncated_by_fragmentation_line;

  if (builder->GetConstraintSpace().ShouldPropagateChildBreakValues() &&
      !builder->layout_object_->ShouldApplyLayoutContainment()) {
    bitfields_.initial_break_before = static_cast<unsigned>(
        builder->initial_break_before_.value_or(EBreakBetween::kAuto));
    bitfields_.final_break_after =
        static_cast<unsigned>(builder->previous_break_after_);
  }

  if (builder->table_column_count_) {
    EnsureRareData()->EnsureTableData()->table_column_count =
        *builder->table_column_count_;
  }
  if (builder->math_italic_correction_) {
    EnsureRareData()->EnsureMathData()->italic_correction =
        builder->math_italic_correction_;
  }
  if (builder->grid_layout_data_) {
    EnsureRareData()->EnsureGridData()->grid_layout_data =
        std::move(builder->grid_layout_data_);
  }
  if (builder->flex_layout_data_) {
    EnsureRareData()->EnsureFlexData()->flex_layout_data =
        std::move(builder->flex_layout_data_);
  }
}

LayoutResult::LayoutResult(LineBoxFragmentBuilderPassKey passkey,
                           const PhysicalFragment* physical_fragment,
                           LineBoxFragmentBuilder* builder)
    : LayoutResult(std::move(physical_fragment),
                   static_cast<FragmentBuilder*>(builder)) {
  DCHECK_EQ(builder->bfc_block_offset_.has_value(),
            builder->line_box_bfc_block_offset_.has_value());
  if (builder->bfc_block_offset_ != builder->line_box_bfc_block_offset_) {
    EnsureRareData()->SetLineBoxBfcBlockOffset(
        *builder->line_box_bfc_block_offset_);
  }

  // `EnsureLineData()` must be done before `EnsureLineSmallData()`.
  DCHECK(!rare_data_ || !rare_data_->HasData(RareData::kLineSmallData));
  if (builder->annotation_block_offset_adjustment_) {
    EnsureRareData()->EnsureLineData()->annotation_block_offset_adjustment =
        builder->annotation_block_offset_adjustment_;
  }
  if (builder->clearance_after_line_) {
    EnsureRareData()->EnsureLineSmallData()->clearance_after_line =
        *builder->clearance_after_line_;
  }
  if (builder->trim_block_end_by_) {
    EnsureRareData()->EnsureLineSmallData()->trim_block_end_by =
        *builder->trim_block_end_by_;
  }
}

LayoutResult::LayoutResult(FragmentBuilderPassKey key,
                           EStatus status,
                           FragmentBuilder* builder)
    : LayoutResult(/* physical_fragment */ nullptr, builder) {
  bitfields_.status = status;
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

LayoutResult::LayoutResult(const LayoutResult& other,
                           const ConstraintSpace& new_space,
                           const MarginStrut& new_end_margin_strut,
                           LayoutUnit bfc_line_offset,
                           std::optional<LayoutUnit> bfc_block_offset,
                           LayoutUnit block_offset_delta)
    : space_(new_space),
      physical_fragment_(other.physical_fragment_),
      rare_data_(other.rare_data_
                     ? MakeGarbageCollected<RareData>(*other.rare_data_)
                     : nullptr),
      intrinsic_block_size_(other.intrinsic_block_size_),
      bitfields_(other.bitfields_) {
  if (!bitfields_.has_oof_insets_for_get_computed_style) {
    bfc_offset_.line_offset = bfc_line_offset;
    bfc_offset_.block_offset = bfc_block_offset.value_or(LayoutUnit());
    bitfields_.is_bfc_block_offset_nullopt = !bfc_block_offset.has_value();
  } else {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    DCHECK_EQ(bfc_line_offset, LayoutUnit());
    DCHECK(bfc_block_offset && bfc_block_offset.value() == LayoutUnit());
    oof_insets_for_get_computed_style_ = BoxStrut();
  }

  ExclusionSpace new_exclusion_space = MergeExclusionSpaces(
      other, space_.GetExclusionSpace(), bfc_line_offset, block_offset_delta);

  if (new_exclusion_space != space_.GetExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(new_exclusion_space);
  } else {
    space_.GetExclusionSpace().MoveDerivedGeometry(new_exclusion_space);
  }

  if (new_end_margin_strut != MarginStrut() || rare_data_) {
    EnsureRareData()->end_margin_strut = new_end_margin_strut;
  }
}

LayoutResult::LayoutResult(const LayoutResult& other,
                           const PhysicalFragment* physical_fragment)
    : space_(other.space_),
      physical_fragment_(std::move(physical_fragment)),
      rare_data_(other.rare_data_
                     ? MakeGarbageCollected<RareData>(*other.rare_data_)
                     : nullptr),
      intrinsic_block_size_(other.intrinsic_block_size_),
      bitfields_(other.bitfields_) {
  if (!bitfields_.has_oof_insets_for_get_computed_style) {
    bfc_offset_ = other.bfc_offset_;
  } else {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    oof_insets_for_get_computed_style_ =
        other.oof_insets_for_get_computed_style_;
  }

  DCHECK_EQ(physical_fragment_->Size(), other.physical_fragment_->Size());
}

LayoutResult::LayoutResult(const PhysicalFragment* physical_fragment,
                           FragmentBuilder* builder)
    : space_(builder->space_),
      physical_fragment_(std::move(physical_fragment)),
      rare_data_(nullptr),
      bitfields_(builder->is_self_collapsing_,
                 builder->is_pushed_by_floats_,
                 builder->adjoining_object_types_,
                 builder->has_descendant_that_depends_on_percentage_block_size_,
                 builder->subtree_modified_margin_strut_) {
#if DCHECK_IS_ON()
  if (bitfields_.is_self_collapsing && physical_fragment_) {
    // A new formatting-context shouldn't be self-collapsing.
    DCHECK(!physical_fragment_->IsFormattingContextRoot());

    // Self-collapsing children must have a block-size of zero.
    LogicalFragment fragment(physical_fragment_->Style().GetWritingDirection(),
                             *physical_fragment_);
    DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
  }
#endif

  if (builder->end_margin_strut_ != MarginStrut()) {
    EnsureRareData()->end_margin_strut = builder->end_margin_strut_;
  }
  if (builder->annotation_overflow_ > LayoutUnit())
    EnsureRareData()->annotation_overflow = builder->annotation_overflow_;
  if (builder->block_end_annotation_space_) {
    EnsureRareData()->block_end_annotation_space =
        builder->block_end_annotation_space_;
  }
  if (builder->exclusion_space_ != space_.GetExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(builder->exclusion_space_);
  } else {
    space_.GetExclusionSpace().MoveDerivedGeometry(builder->exclusion_space_);
  }
  if (builder->lines_until_clamp_) {
    EnsureRareData()->lines_until_clamp = *builder->lines_until_clamp_;
  }
  if (builder->is_block_start_trimmed_) {
    EnsureRareData()->set_is_block_start_trimmed();
  }
  if (builder->is_block_end_trimmed_) {
    EnsureRareData()->set_is_block_end_trimmed();
  }

  if (builder->tallest_unbreakable_block_size_ >= LayoutUnit()) {
    EnsureRareData()->tallest_unbreakable_block_size =
        builder->tallest_unbreakable_block_size_;

    // This field shares storage with "minimal space shortage", so both cannot
    // be set at the same time.
    DCHECK_EQ(builder->minimal_space_shortage_, kIndefiniteSize);
  } else if (builder->minimal_space_shortage_ != kIndefiniteSize) {
    EnsureRareData()->minimal_space_shortage = builder->minimal_space_shortage_;
  }

  // If we produced a fragment that we didn't break inside, provide the best
  // early possible breakpoint that we found inside. This early breakpoint will
  // be propagated to the container for further consideration. If we didn't
  // produce a fragment, on the other hand, it means that we're going to
  // re-layout now, and break at the early breakpoint (i.e. the status is
  // kNeedsEarlierBreak).
  if (builder->early_break_ &&
      (!physical_fragment_ || !physical_fragment_->GetBreakToken())) {
    EnsureRareData()->early_break = builder->early_break_;
  }

  if (builder->column_spanner_path_) {
    EnsureRareData()->column_spanner_path = builder->column_spanner_path_;
    bitfields_.is_empty_spanner_parent = builder->is_empty_spanner_parent_;
  }

  bitfields_.break_appeal = builder->break_appeal_;

  bitfields_.should_force_same_fragmentation_flow =
      builder->should_force_same_fragmentation_flow_;
  bitfields_.has_orthogonal_fallback_size_descendant =
      builder->has_orthogonal_fallback_size_descendant_;

  bfc_offset_.line_offset = builder->bfc_line_offset_;
  bfc_offset_.block_offset = builder->bfc_block_offset_.value_or(LayoutUnit());
  bitfields_.is_bfc_block_offset_nullopt =
      !builder->bfc_block_offset_.has_value();
}

ExclusionSpace LayoutResult::MergeExclusionSpaces(
    const LayoutResult& other,
    const ExclusionSpace& new_input_exclusion_space,
    LayoutUnit bfc_line_offset,
    LayoutUnit block_offset_delta) {
  BfcDelta offset_delta = {bfc_line_offset - other.BfcLineOffset(),
                           block_offset_delta};

  return ExclusionSpace::MergeExclusionSpaces(
      /* old_output */ other.GetExclusionSpace(),
      /* old_input */ other.space_.GetExclusionSpace(),
      /* new_input */ new_input_exclusion_space, offset_delta);
}

LayoutResult::RareData* LayoutResult::EnsureRareData() {
  if (!rare_data_) {
    rare_data_ = MakeGarbageCollected<RareData>();
  }
  return rare_data_.Get();
}

void LayoutResult::CopyMutableOutOfFlowData(const LayoutResult& other) const {
  if (bitfields_.has_oof_insets_for_get_computed_style) {
    return;
  }
  GetMutableForOutOfFlow().SetOutOfFlowInsetsForGetComputedStyle(
      other.OutOfFlowInsetsForGetComputedStyle());
  GetMutableForOutOfFlow().SetOutOfFlowPositionedOffset(
      other.OutOfFlowPositionedOffset());
}

void LayoutResult::MutableForOutOfFlow::SetAccessibilityAnchor(
    Element* anchor) {
  if (layout_result_->rare_data_ || anchor) {
    layout_result_->EnsureRareData()->accessibility_anchor = anchor;
  }
}

void LayoutResult::MutableForOutOfFlow::SetDisplayLocksAffectedByAnchors(
    HeapHashSet<Member<Element>>* display_locks) {
  if (layout_result_->rare_data_ || display_locks) {
    layout_result_->EnsureRareData()->display_locks_affected_by_anchors =
        display_locks;
  }
}

#if DCHECK_IS_ON()
void LayoutResult::CheckSameForSimplifiedLayout(
    const LayoutResult& other,
    bool check_same_block_size,
    bool check_no_fragmentation) const {
  To<PhysicalBoxFragment>(*physical_fragment_)
      .CheckSameForSimplifiedLayout(
          To<PhysicalBoxFragment>(*other.physical_fragment_),
          check_same_block_size, check_no_fragmentation);

  DCHECK(LinesUntilClamp() == other.LinesUntilClamp());
  GetExclusionSpace().CheckSameForSimplifiedLayout(other.GetExclusionSpace());

  // We ignore |BfcBlockOffset|, and |BfcLineOffset| as "simplified" layout
  // will move the layout result if required.

  // We ignore the |intrinsic_block_size_| as if a scrollbar gets added/removed
  // this may change (even if the size of the fragment remains the same).

  DCHECK(EndMarginStrut() == other.EndMarginStrut());
  DCHECK(MinimalSpaceShortage() == other.MinimalSpaceShortage());
  DCHECK_EQ(TableColumnCount(), other.TableColumnCount());

  DCHECK_EQ(bitfields_.has_forced_break, other.bitfields_.has_forced_break);
  DCHECK_EQ(bitfields_.is_self_collapsing, other.bitfields_.is_self_collapsing);
  DCHECK_EQ(bitfields_.is_pushed_by_floats,
            other.bitfields_.is_pushed_by_floats);
  DCHECK_EQ(bitfields_.adjoining_object_types,
            other.bitfields_.adjoining_object_types);

  DCHECK_EQ(bitfields_.subtree_modified_margin_strut,
            other.bitfields_.subtree_modified_margin_strut);

  DCHECK_EQ(CustomLayoutData(), other.CustomLayoutData());

  DCHECK_EQ(bitfields_.initial_break_before,
            other.bitfields_.initial_break_before);
  DCHECK_EQ(bitfields_.final_break_after, other.bitfields_.final_break_after);

  DCHECK_EQ(
      bitfields_.has_descendant_that_depends_on_percentage_block_size,
      other.bitfields_.has_descendant_that_depends_on_percentage_block_size);
  DCHECK_EQ(bitfields_.status, other.bitfields_.status);
}
#endif

#if DCHECK_IS_ON()
void LayoutResult::AssertSoleBoxFragment() const {
  DCHECK(physical_fragment_->IsBox());
  DCHECK(To<PhysicalBoxFragment>(GetPhysicalFragment()).IsFirstForNode());
  DCHECK(!physical_fragment_->GetBreakToken());
}
#endif

void LayoutResult::Trace(Visitor* visitor) const {
  visitor->Trace(physical_fragment_);
  visitor->Trace(rare_data_);
}

void LayoutResult::RareData::Trace(Visitor* visitor) const {
  visitor->Trace(early_break);
  visitor->Trace(non_overflowing_scroll_ranges);
  visitor->Trace(column_spanner_path);
  visitor->Trace(accessibility_anchor);
  visitor->Trace(display_locks_affected_by_anchors);
}

}  // namespace blink
