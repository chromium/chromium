// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpaceBuilder_h
#define NGConstraintSpaceBuilder_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_size.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class NGExclusionSpace;

class CORE_EXPORT NGConstraintSpaceBuilder final {
  STACK_ALLOCATED();

 public:
  // NOTE: This constructor doesn't act like a copy-constructor, it uses the
  // writing_mode and icb_size from the parent constraint space, and passes
  // them to the constructor below.
  NGConstraintSpaceBuilder(const NGConstraintSpace& parent_space)
      : NGConstraintSpaceBuilder(parent_space.GetWritingMode(),
                                 parent_space.InitialContainingBlockSize()) {
    flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
    if (parent_space.IsIntermediateLayout())
      flags_ |= NGConstraintSpace::kIntermediateLayout;
  }

  // writing_mode is the writing mode that the logical sizes passed to the
  // setters are in.
  NGConstraintSpaceBuilder(WritingMode writing_mode, NGPhysicalSize icb_size)
      : initial_containing_block_size_(icb_size),
        parent_writing_mode_(writing_mode) {
    flags_ = NGConstraintSpace::kFixedSizeBlockIsDefinite;
  }

  NGConstraintSpaceBuilder& SetAvailableSize(NGLogicalSize available_size) {
    available_size_ = available_size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetPercentageResolutionSize(
      NGLogicalSize percentage_resolution_size) {
    percentage_resolution_size_ = percentage_resolution_size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetReplacedPercentageResolutionSize(
      NGLogicalSize replaced_percentage_resolution_size) {
    replaced_percentage_resolution_size_ = replaced_percentage_resolution_size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerBlockSize(LayoutUnit size) {
    fragmentainer_block_size_ = size;
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentainerSpaceAtBfcStart(LayoutUnit space) {
    fragmentainer_space_at_bfc_start_ = space;
    return *this;
  }

  NGConstraintSpaceBuilder& SetTextDirection(TextDirection text_direction) {
    text_direction_ = text_direction;
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsFixedSizeInline(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeInline, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsFixedSizeBlock(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeBlock, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetFixedSizeBlockIsDefinite(bool b) {
    SetFlag(NGConstraintSpace::kFixedSizeBlockIsDefinite, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsShrinkToFit(bool b) {
    SetFlag(NGConstraintSpace::kShrinkToFit, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsIntermediateLayout(bool b) {
    SetFlag(NGConstraintSpace::kIntermediateLayout, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetFragmentationType(
      NGFragmentationType fragmentation_type) {
    fragmentation_type_ = fragmentation_type;
    return *this;
  }

  NGConstraintSpaceBuilder& SetSeparateLeadingFragmentainerMargins(bool b) {
    SetFlag(NGConstraintSpace::kSeparateLeadingFragmentainerMargins, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetIsNewFormattingContext(bool b) {
    SetFlag(NGConstraintSpace::kNewFormattingContext, b);
    return *this;
  }
  NGConstraintSpaceBuilder& SetIsAnonymous(bool b) {
    SetFlag(NGConstraintSpace::kAnonymous, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetUseFirstLineStyle(bool b) {
    SetFlag(NGConstraintSpace::kUseFirstLineStyle, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetAdjoiningFloatTypes(NGFloatTypes floats) {
    adjoining_floats_ = floats;
    return *this;
  }

  NGConstraintSpaceBuilder& SetMarginStrut(const NGMarginStrut& margin_strut) {
    margin_strut_ = margin_strut;
    return *this;
  }

  NGConstraintSpaceBuilder& SetBfcOffset(const NGBfcOffset& bfc_offset) {
    bfc_offset_ = bfc_offset;
    return *this;
  }
  NGConstraintSpaceBuilder& SetFloatsBfcBlockOffset(
      const base::Optional<LayoutUnit>& floats_bfc_block_offset) {
    floats_bfc_block_offset_ = floats_bfc_block_offset;
    return *this;
  }

  NGConstraintSpaceBuilder& SetClearanceOffset(LayoutUnit clearance_offset) {
    clearance_offset_ = clearance_offset;
    return *this;
  }

  NGConstraintSpaceBuilder& SetShouldForceClearance(bool b) {
    SetFlag(NGConstraintSpace::kForceClearance, b);
    return *this;
  }

  NGConstraintSpaceBuilder& SetTableCellChildLayoutPhase(
      NGTableCellChildLayoutPhase table_cell_child_layout_phase) {
    table_cell_child_layout_phase_ = table_cell_child_layout_phase;
    return *this;
  }

  // Usually orthogonality is inferred from the WritingMode parameters passed to
  // the constructor and ToConstraintSpace. But if you're passing the same
  // writing mode to those methods but the node targeted by this ConstraintSpace
  // is an orthogonal writing mode root, call this method to have the
  // appropriate flags set on the resulting ConstraintSpace.
  NGConstraintSpaceBuilder& SetIsOrthogonalWritingModeRoot(bool b) {
    force_orthogonal_writing_mode_root_ = b;
    return *this;
  }

  NGConstraintSpaceBuilder& SetExclusionSpace(
      const NGExclusionSpace& exclusion_space) {
    exclusion_space_ = &exclusion_space;
    return *this;
  }

  void AddBaselineRequests(
      const NGConstraintSpace::NGBaselineRequestVector& requests) {
    DCHECK(baseline_requests_.IsEmpty());
    baseline_requests_.AppendVector(requests);
  }
  NGConstraintSpaceBuilder& AddBaselineRequest(const NGBaselineRequest&);

  // Creates a new constraint space. This may be called multiple times, for
  // example the constraint space will be different for a child which:
  //  - Establishes a new formatting context.
  //  - Is within a fragmentation container and needs its fragmentation offset
  //    updated.
  //  - Has its size is determined by its parent layout (flex, abs-pos).
  //
  // WritingMode specifies the writing mode of the generated space.
  const NGConstraintSpace ToConstraintSpace(WritingMode out_writing_mode) {
    return NGConstraintSpace(out_writing_mode,
                             flags_ & NGConstraintSpace::kNewFormattingContext,
                             *this);
  }

 private:
  void SetFlag(NGConstraintSpace::ConstraintSpaceFlags mask, bool value) {
    flags_ = (flags_ & ~static_cast<unsigned>(mask)) |
             (-(int32_t)value & static_cast<unsigned>(mask));
  }

  // NOTE: The below NGLogicalSizes are relative to parent_writing_mode_.
  NGLogicalSize available_size_;
  NGLogicalSize percentage_resolution_size_;
  NGLogicalSize replaced_percentage_resolution_size_;

  NGPhysicalSize initial_containing_block_size_;
  LayoutUnit fragmentainer_block_size_ = NGSizeIndefinite;
  LayoutUnit fragmentainer_space_at_bfc_start_ = NGSizeIndefinite;

  WritingMode parent_writing_mode_;
  NGFragmentationType fragmentation_type_ = kFragmentNone;
  NGTableCellChildLayoutPhase table_cell_child_layout_phase_ =
      kNotTableCellChild;
  NGFloatTypes adjoining_floats_ = kFloatTypeNone;
  TextDirection text_direction_ = TextDirection::kLtr;
  bool force_orthogonal_writing_mode_root_ = false;

  unsigned flags_;

  NGMarginStrut margin_strut_;
  NGBfcOffset bfc_offset_;
  base::Optional<LayoutUnit> floats_bfc_block_offset_;
  const NGExclusionSpace* exclusion_space_ = nullptr;
  LayoutUnit clearance_offset_;
  NGConstraintSpace::NGBaselineRequestVector baseline_requests_;

  friend class NGConstraintSpace;
};

}  // namespace blink

#endif  // NGConstraintSpaceBuilder
