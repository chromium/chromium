// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_line_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_positioned_float.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGLayoutResult : public RefCounted<SameSizeAsNGLayoutResult> {
  const NGConstraintSpace space;
  void* physical_fragment;
  union {
    NGBfcOffset bfc_offset;
    LogicalOffset oof_positioned_offset;
    void* rare_data;
  };
  LayoutUnit intrinsic_block_size;
  unsigned bitfields[1];

#if DCHECK_IS_ON()
  bool has_valid_space;
#endif
};

ASSERT_SIZE(NGLayoutResult, SameSizeAsNGLayoutResult);

}  // namespace

// static
scoped_refptr<const NGLayoutResult>
NGLayoutResult::CloneWithPostLayoutFragments(
    const NGLayoutResult& other,
    const base::Optional<PhysicalRect> updated_layout_overflow) {
  return base::AdoptRef(new NGLayoutResult(
      other, NGPhysicalBoxFragment::CloneWithPostLayoutFragments(
                 To<NGPhysicalBoxFragment>(other.PhysicalFragment()),
                 updated_layout_overflow)));
}

NGLayoutResult::NGLayoutResult(
    NGBoxFragmentBuilderPassKey passkey,
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     static_cast<NGContainerFragmentBuilder*>(builder)) {
  bitfields_.is_initial_block_size_indefinite =
      builder->is_initial_block_size_indefinite_;
  bitfields_.subtree_modified_margin_strut =
      builder->subtree_modified_margin_strut_;
  intrinsic_block_size_ = builder->intrinsic_block_size_;
  if (builder->overflow_block_size_ != kIndefiniteSize &&
      builder->overflow_block_size_ != intrinsic_block_size_) {
    EnsureRareData()->overflow_block_size = builder->overflow_block_size_;
  }
  if (builder->custom_layout_data_) {
    EnsureRareData()->custom_layout_data =
        std::move(builder->custom_layout_data_);
  }
  if (builder->lines_until_clamp_)
    EnsureRareData()->lines_until_clamp = *builder->lines_until_clamp_;
  if (builder->annotation_overflow_)
    EnsureRareData()->annotation_overflow = builder->annotation_overflow_;
  if (builder->block_end_annotation_space_) {
    EnsureRareData()->block_end_annotation_space =
        builder->block_end_annotation_space_;
  }

  if (builder->has_block_fragmentation_) {
    RareData* rare_data = EnsureRareData();

    // We don't support fragment caching when block-fragmenting, so mark the
    // result as non-reusable.
    rare_data->is_single_use = true;

    if (builder->tallest_unbreakable_block_size_ >= LayoutUnit()) {
      rare_data->tallest_unbreakable_block_size =
          builder->tallest_unbreakable_block_size_;

      // This field shares storage with "minimal space shortage", so both
      // cannot be set at the same time.
      DCHECK_EQ(builder->minimal_space_shortage_, LayoutUnit::Max());
    } else if (builder->minimal_space_shortage_ != LayoutUnit::Max()) {
      rare_data->minimal_space_shortage = builder->minimal_space_shortage_;
    }

    if (builder->column_spanner_)
      rare_data->column_spanner = builder->column_spanner_;

    bitfields_.initial_break_before =
        static_cast<unsigned>(builder->initial_break_before_);
    bitfields_.final_break_after =
        static_cast<unsigned>(builder->previous_break_after_);
    bitfields_.has_forced_break = builder->has_forced_break_;
  }
  if (builder->table_column_count_)
    EnsureRareData()->table_column_count_ = *builder->table_column_count_;
}

NGLayoutResult::NGLayoutResult(
    NGLineBoxFragmentBuilderPassKey passkey,
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGLineBoxFragmentBuilder* builder)
    : NGLayoutResult(std::move(physical_fragment),
                     static_cast<NGContainerFragmentBuilder*>(builder)) {}

NGLayoutResult::NGLayoutResult(NGBoxFragmentBuilderPassKey key,
                               EStatus status,
                               NGBoxFragmentBuilder* builder)
    : NGLayoutResult(/* physical_fragment */ nullptr,
                     static_cast<NGContainerFragmentBuilder*>(builder)) {
  bitfields_.status = status;
  if (builder->lines_until_clamp_)
    EnsureRareData()->lines_until_clamp = *builder->lines_until_clamp_;
  DCHECK_NE(status, kSuccess)
      << "Use the other constructor for successful layout";
}

NGLayoutResult::NGLayoutResult(const NGLayoutResult& other,
                               const NGConstraintSpace& new_space,
                               const NGMarginStrut& new_end_margin_strut,
                               LayoutUnit bfc_line_offset,
                               base::Optional<LayoutUnit> bfc_block_offset,
                               LayoutUnit block_offset_delta)
    : space_(new_space),
      physical_fragment_(other.physical_fragment_),
      intrinsic_block_size_(other.intrinsic_block_size_),
      bitfields_(other.bitfields_) {
  if (HasRareData()) {
    rare_data_ = new RareData(*other.rare_data_);
    rare_data_->bfc_line_offset = bfc_line_offset;
    rare_data_->bfc_block_offset = bfc_block_offset;
  } else if (!bitfields_.has_oof_positioned_offset) {
    bfc_offset_.line_offset = bfc_line_offset;
    bfc_offset_.block_offset = bfc_block_offset.value_or(LayoutUnit());
    bitfields_.is_bfc_block_offset_nullopt = !bfc_block_offset.has_value();
  } else {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    DCHECK_EQ(bfc_line_offset, LayoutUnit());
    DCHECK(bfc_block_offset && bfc_block_offset.value() == LayoutUnit());
    oof_positioned_offset_ = LogicalOffset();
  }

  NGExclusionSpace new_exclusion_space = MergeExclusionSpaces(
      other, space_.ExclusionSpace(), bfc_line_offset, block_offset_delta);

  if (new_exclusion_space != space_.ExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(new_exclusion_space);
  } else {
    space_.ExclusionSpace().MoveDerivedGeometry(new_exclusion_space);
  }

  if (new_end_margin_strut != NGMarginStrut() || HasRareData())
    EnsureRareData()->end_margin_strut = new_end_margin_strut;

#if DCHECK_IS_ON()
  has_valid_space_ = other.has_valid_space_;
#endif
}

NGLayoutResult::NGLayoutResult(
    const NGLayoutResult& other,
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment)
    : space_(other.space_),
      physical_fragment_(std::move(physical_fragment)),
      intrinsic_block_size_(other.intrinsic_block_size_),
      bitfields_(other.bitfields_) {
  if (HasRareData()) {
    rare_data_ = new RareData(*other.rare_data_);
  } else if (!bitfields_.has_oof_positioned_offset) {
    bfc_offset_ = other.bfc_offset_;
  } else {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    oof_positioned_offset_ = other.oof_positioned_offset_;
  }

  DCHECK_EQ(physical_fragment_->Size(), other.physical_fragment_->Size());

#if DCHECK_IS_ON()
  has_valid_space_ = other.has_valid_space_;
#endif
}

NGLayoutResult::NGLayoutResult(
    scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
    NGContainerFragmentBuilder* builder)
    : space_(builder->space_ ? NGConstraintSpace(*builder->space_)
                             : NGConstraintSpace()),
      physical_fragment_(std::move(physical_fragment)),
      bitfields_(
          /* is_self_collapsing */ builder->is_self_collapsing_,
          /* is_pushed_by_floats */ builder->is_pushed_by_floats_,
          /* adjoining_object_types */ builder->adjoining_object_types_,
          /* has_descendant_that_depends_on_percentage_block_size */
          builder->has_descendant_that_depends_on_percentage_block_size_) {
#if DCHECK_IS_ON()
  if (bitfields_.is_self_collapsing && physical_fragment_) {
    // A new formatting-context shouldn't be self-collapsing.
    DCHECK(!physical_fragment_->IsFormattingContextRoot());

    // Self-collapsing children must have a block-size of zero.
    NGFragment fragment(physical_fragment_->Style().GetWritingDirection(),
                        *physical_fragment_);
    DCHECK_EQ(LayoutUnit(), fragment.BlockSize());
  }
#endif

  if (builder->end_margin_strut_ != NGMarginStrut())
    EnsureRareData()->end_margin_strut = builder->end_margin_strut_;
  if (builder->annotation_overflow_ > LayoutUnit())
    EnsureRareData()->annotation_overflow = builder->annotation_overflow_;
  if (builder->block_end_annotation_space_) {
    EnsureRareData()->block_end_annotation_space =
        builder->block_end_annotation_space_;
  }
  if (builder->unpositioned_list_marker_) {
    EnsureRareData()->unpositioned_list_marker =
        builder->unpositioned_list_marker_;
  }
  if (builder->exclusion_space_ != space_.ExclusionSpace()) {
    bitfields_.has_rare_data_exclusion_space = true;
    EnsureRareData()->exclusion_space = std::move(builder->exclusion_space_);
  } else {
    space_.ExclusionSpace().MoveDerivedGeometry(builder->exclusion_space_);
  }

  // If we produced a fragment that we didn't break inside, provide the best
  // early possible breakpoint that we found inside. This early breakpoint will
  // be propagated to the container for further consideration. If we didn't
  // produce a fragment, on the other hand, it means that we're going to
  // re-layout now, and break at the early breakpoint (i.e. the status is
  // kNeedsEarlierBreak).
  if (builder->early_break_ &&
      (!physical_fragment_ || !physical_fragment_->BreakToken())) {
    auto* rare_data = EnsureRareData();
    rare_data->early_break = builder->early_break_;
    rare_data->early_break_appeal = builder->break_appeal_;
  }

  if (HasRareData()) {
    rare_data_->bfc_line_offset = builder->bfc_line_offset_;
    rare_data_->bfc_block_offset = builder->bfc_block_offset_;
  } else {
    bfc_offset_.line_offset = builder->bfc_line_offset_;
    bfc_offset_.block_offset =
        builder->bfc_block_offset_.value_or(LayoutUnit());
    bitfields_.is_bfc_block_offset_nullopt =
        !builder->bfc_block_offset_.has_value();
  }

#if DCHECK_IS_ON()
  has_valid_space_ = builder->space_;
#endif
}

NGLayoutResult::~NGLayoutResult() {
  if (HasRareData())
    delete rare_data_;
}

NGExclusionSpace NGLayoutResult::MergeExclusionSpaces(
    const NGLayoutResult& other,
    const NGExclusionSpace& new_input_exclusion_space,
    LayoutUnit bfc_line_offset,
    LayoutUnit block_offset_delta) {
  NGBfcDelta offset_delta = {bfc_line_offset - other.BfcLineOffset(),
                             block_offset_delta};

  return NGExclusionSpace::MergeExclusionSpaces(
      /* old_output */ other.ExclusionSpace(),
      /* old_input */ other.space_.ExclusionSpace(),
      /* new_input */ new_input_exclusion_space, offset_delta);
}

NGLayoutResult::RareData* NGLayoutResult::EnsureRareData() {
  if (!HasRareData()) {
    base::Optional<LayoutUnit> bfc_block_offset;
    if (!bitfields_.is_bfc_block_offset_nullopt)
      bfc_block_offset = bfc_offset_.block_offset;
    rare_data_ = new RareData(bfc_offset_.line_offset, bfc_block_offset);
    bitfields_.has_rare_data = true;
  }

  return rare_data_;
}

#if DCHECK_IS_ON()
void NGLayoutResult::CheckSameForSimplifiedLayout(
    const NGLayoutResult& other,
    bool check_same_block_size) const {
  To<NGPhysicalBoxFragment>(*physical_fragment_)
      .CheckSameForSimplifiedLayout(
          To<NGPhysicalBoxFragment>(*other.physical_fragment_),
          check_same_block_size);

  DCHECK(LinesUntilClamp() == other.LinesUntilClamp());
  DCHECK(UnpositionedListMarker() == other.UnpositionedListMarker());
  ExclusionSpace().CheckSameForSimplifiedLayout(other.ExclusionSpace());

  // We ignore |BfcBlockOffset|, and |BfcLineOffset| as "simplified" layout
  // will move the layout result if required.

  // We ignore the |intrinsic_block_size_| as if a scrollbar gets added/removed
  // this may change (even if the size of the fragment remains the same).

  DCHECK(EndMarginStrut() == other.EndMarginStrut());
  DCHECK_EQ(MinimalSpaceShortage(), other.MinimalSpaceShortage());

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
void NGLayoutResult::AssertSoleBoxFragment() const {
  DCHECK(physical_fragment_->IsBox());
  DCHECK(To<NGPhysicalBoxFragment>(PhysicalFragment()).IsFirstForNode());
  DCHECK(!physical_fragment_->BreakToken());
}
#endif

}  // namespace blink
