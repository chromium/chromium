// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/list/ng_unpositioned_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGContainerFragmentBuilder;
class NGExclusionSpace;
class NGLineBoxFragmentBuilder;

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult : public RefCounted<NGLayoutResult> {
 public:
  enum EStatus {
    kSuccess = 0,
    kBfcBlockOffsetResolved = 1,
    kNeedsEarlierBreak = 2,
    kOutOfFragmentainerSpace = 3,
    kNeedsRelayoutWithNoForcedTruncateAtLineClamp = 4,
    // When adding new values, make sure the bit size of |Bitfields::status| is
    // large enough to store.
  };

  // Create a copy of NGLayoutResult with |BfcBlockOffset| replaced by the given
  // parameter. Note, when |bfc_block_offset| is |nullopt|, |BfcBlockOffset| is
  // still replaced with |nullopt|.
  NGLayoutResult(const NGLayoutResult& other,
                 const NGConstraintSpace& new_space,
                 const NGMarginStrut& new_end_margin_strut,
                 LayoutUnit bfc_line_offset,
                 base::Optional<LayoutUnit> bfc_block_offset,
                 LayoutUnit block_offset_delta);
  ~NGLayoutResult();

  const NGPhysicalContainerFragment& PhysicalFragment() const {
    DCHECK(physical_fragment_);
    DCHECK_EQ(kSuccess, Status());
    return *physical_fragment_;
  }

  int LinesUntilClamp() const {
    return HasRareData() ? rare_data_->lines_until_clamp : 0;
  }

  // How much an annotation box overflow from this box.
  // This is for LayoutNGRubyRun and line boxes.
  // 0 : No overflow
  // -N : Overflowing by N px at block-start side
  //      This happens only for LayoutRubyRun.
  // N : Overflowing by N px at block-end side
  LayoutUnit AnnotationOverflow() const {
    return HasRareData() ? rare_data_->annotation_overflow : LayoutUnit();
  }

  // The amount of available space for block-start side annotations of the
  // next box.
  // This never be negative.
  LayoutUnit BlockEndAnnotationSpace() const {
    return HasRareData() ? rare_data_->block_end_annotation_space
                         : LayoutUnit();
  }

  LogicalOffset OutOfFlowPositionedOffset() const {
    DCHECK(bitfields_.has_oof_positioned_offset);
    return HasRareData() ? rare_data_->oof_positioned_offset
                         : oof_positioned_offset_;
  }

  // Returns if we can use the first-tier OOF-positioned cache.
  bool CanUseOutOfFlowPositionedFirstTierCache() const {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    return bitfields_.can_use_out_of_flow_positioned_first_tier_cache;
  }

  const NGUnpositionedListMarker UnpositionedListMarker() const {
    return HasRareData() ? rare_data_->unpositioned_list_marker
                         : NGUnpositionedListMarker();
  }

  // Get the column spanner (if any) that interrupted column layout.
  NGBlockNode ColumnSpanner() const {
    return HasRareData() ? rare_data_->column_spanner : NGBlockNode(nullptr);
  }

  scoped_refptr<const NGEarlyBreak> GetEarlyBreak() const {
    if (!HasRareData())
      return nullptr;
    return rare_data_->early_break;
  }

  // Return the appeal of the best breakpoint (if any) we found inside the node.
  NGBreakAppeal EarlyBreakAppeal() const {
    if (HasRareData())
      return static_cast<NGBreakAppeal>(rare_data_->early_break_appeal);
    return kBreakAppealLastResort;
  }

  const NGExclusionSpace& ExclusionSpace() const {
    if (bitfields_.has_rare_data_exclusion_space) {
      DCHECK(HasRareData());
      return rare_data_->exclusion_space;
    }

    return space_.ExclusionSpace();
  }

  EStatus Status() const { return static_cast<EStatus>(bitfields_.status); }

  LayoutUnit BfcLineOffset() const {
    if (HasRareData())
      return rare_data_->bfc_line_offset;

    if (bitfields_.has_oof_positioned_offset) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    return bfc_offset_.line_offset;
  }

  const base::Optional<LayoutUnit> BfcBlockOffset() const {
    if (HasRareData())
      return rare_data_->bfc_block_offset;

    if (bitfields_.has_oof_positioned_offset) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    if (bitfields_.is_bfc_block_offset_nullopt)
      return base::nullopt;

    return bfc_offset_.block_offset;
  }

  const NGMarginStrut EndMarginStrut() const {
    return HasRareData() ? rare_data_->end_margin_strut : NGMarginStrut();
  }

  // Get the intrinsic block-size of the fragment (i.e. the block-size the
  // fragment would get if no block-size constraints were applied). This is not
  // supported (and should not be needed [1]) if the node got split into
  // multiple fragments.
  //
  // [1] If a node gets block-fragmented, it means that it has possibly been
  // constrained and/or stretched by something extrinsic (i.e. the
  // fragmentainer), so the value returned here wouldn't be useful.
  const LayoutUnit IntrinsicBlockSize() const {
#if DCHECK_IS_ON()
    AssertSoleBoxFragment();
#endif
    return intrinsic_block_size_;
  }

  LayoutUnit OverflowBlockSize() const {
    return HasRareData() && rare_data_->overflow_block_size != kIndefiniteSize
               ? rare_data_->overflow_block_size
               : intrinsic_block_size_;
  }

  LayoutUnit MinimalSpaceShortage() const {
    if (!HasRareData() || rare_data_->minimal_space_shortage == kIndefiniteSize)
      return LayoutUnit::Max();
    return rare_data_->minimal_space_shortage;
  }

  LayoutUnit TallestUnbreakableBlockSize() const {
    if (!HasRareData() ||
        rare_data_->tallest_unbreakable_block_size == kIndefiniteSize)
      return LayoutUnit();
    return rare_data_->tallest_unbreakable_block_size;
  }

  // Return whether this result is single-use only (true), or if it is allowed
  // to be involved in cache hits in future layout passes (false).
  // For example, this happens when a block is fragmented, since we don't yet
  // support caching of block-fragmented results.
  bool IsSingleUse() const {
    return HasRareData() && rare_data_->is_single_use;
  }

  SerializedScriptValue* CustomLayoutData() const {
    return HasRareData() ? rare_data_->custom_layout_data.get() : nullptr;
  }

  wtf_size_t TableColumnCount() const {
    return HasRareData() ? rare_data_->table_column_count_ : 0;
  }

  // The break-before value on the first child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween InitialBreakBefore() const {
    return static_cast<EBreakBetween>(bitfields_.initial_break_before);
  }

  // The break-after value on the last child needs to be propagated to the
  // container, in search of a valid class A break point.
  EBreakBetween FinalBreakAfter() const {
    return static_cast<EBreakBetween>(bitfields_.final_break_after);
  }

  // Return true if the fragment broke because a forced break before a child.
  bool HasForcedBreak() const { return bitfields_.has_forced_break; }

  // Returns true if the fragment should be considered empty for margin
  // collapsing purposes (e.g. margins "collapse through").
  bool IsSelfCollapsing() const { return bitfields_.is_self_collapsing; }

  // Return true if this fragment got its block offset increased by the presence
  // of floats.
  bool IsPushedByFloats() const { return bitfields_.is_pushed_by_floats; }

  // Returns the types of preceding adjoining objects.
  // See |NGAdjoiningObjectTypes|.
  //
  // Adjoining floats should be treated differently when calculating clearance
  // on a block with adjoining block-start margin (in such cases we will know
  // up front that the block will need clearance, since, if it doesn't, the
  // float will be pulled along with the block, and the block will fail to
  // clear).
  NGAdjoiningObjectTypes AdjoiningObjectTypes() const {
    return bitfields_.adjoining_object_types;
  }

  // Returns true if the initial (pre-layout) block-size of this fragment was
  // indefinite. (e.g. it has "height: auto").
  bool IsInitialBlockSizeIndefinite() const {
    return bitfields_.is_initial_block_size_indefinite;
  }

  // Returns true if there is a descendant that depends on percentage
  // resolution block-size changes.
  // Some layout modes (flex-items, table-cells) have more complex child
  // percentage sizing behaviour (typically when their parent layout forces a
  // block-size on them).
  bool HasDescendantThatDependsOnPercentageBlockSize() const {
    return bitfields_.has_descendant_that_depends_on_percentage_block_size;
  }

  // Returns true if this subtree modified the incoming margin-strut (i.e.
  // appended a non-zero margin).
  bool SubtreeModifiedMarginStrut() const {
    return bitfields_.subtree_modified_margin_strut;
  }

  // Returns the space which generated this object for caching purposes.
  const NGConstraintSpace& GetConstraintSpaceForCaching() const {
#if DCHECK_IS_ON()
    DCHECK(has_valid_space_);
#endif
    return space_;
  }

  // This exposes a mutable part of the layout result just for the
  // |NGOutOfFlowLayoutPart|.
  class MutableForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;

    void SetOutOfFlowPositionedOffset(
        const LogicalOffset& offset,
        bool can_use_out_of_flow_positioned_first_tier_cache) {
      // OOF-positioned nodes *must* always have an initial BFC-offset.
      DCHECK(layout_result_->physical_fragment_->IsOutOfFlowPositioned());
      DCHECK_EQ(layout_result_->BfcLineOffset(), LayoutUnit());
      DCHECK_EQ(layout_result_->BfcBlockOffset().value_or(LayoutUnit()),
                LayoutUnit());

      layout_result_->bitfields_
          .can_use_out_of_flow_positioned_first_tier_cache =
          can_use_out_of_flow_positioned_first_tier_cache;
      layout_result_->bitfields_.has_oof_positioned_offset = true;
      if (layout_result_->HasRareData())
        layout_result_->rare_data_->oof_positioned_offset = offset;
      else
        layout_result_->oof_positioned_offset_ = offset;
    }

   private:
    friend class NGLayoutResult;
    MutableForOutOfFlow(const NGLayoutResult* layout_result)
        : layout_result_(const_cast<NGLayoutResult*>(layout_result)) {}

    NGLayoutResult* layout_result_;
  };

  MutableForOutOfFlow GetMutableForOutOfFlow() const {
    return MutableForOutOfFlow(this);
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGLayoutResult&,
                                    bool check_same_block_size = true) const;
#endif

  using NGBoxFragmentBuilderPassKey = util::PassKey<NGBoxFragmentBuilder>;
  // This constructor is for a non-success status.
  NGLayoutResult(NGBoxFragmentBuilderPassKey, EStatus, NGBoxFragmentBuilder*);
  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(
      NGBoxFragmentBuilderPassKey,
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGBoxFragmentBuilder*);
  using NGLineBoxFragmentBuilderPassKey =
      util::PassKey<NGLineBoxFragmentBuilder>;
  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(
      NGLineBoxFragmentBuilderPassKey,
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGLineBoxFragmentBuilder*);

 private:
  friend class MutableForOutOfFlow;

  // We don't need the copy constructor, move constructor, copy
  // assigmnment-operator, or move assignment-operator today.
  // Delete these to clarify that they will not work because a |RefCounted|
  // object can't be copied directly.
  //
  // If at some point we do need these constructors particular care will need
  // to be taken with the |rare_data_| field which is manually memory managed.
  NGLayoutResult(const NGLayoutResult&) = delete;
  NGLayoutResult(NGLayoutResult&&) = delete;
  NGLayoutResult& operator=(const NGLayoutResult& other) = delete;
  NGLayoutResult& operator=(NGLayoutResult&& other) = delete;
  NGLayoutResult() = delete;

  // Delegate constructor that sets up what it can, based on the builder.
  NGLayoutResult(
      scoped_refptr<const NGPhysicalContainerFragment> physical_fragment,
      NGContainerFragmentBuilder* builder);

  static NGExclusionSpace MergeExclusionSpaces(
      const NGLayoutResult& other,
      const NGExclusionSpace& new_input_exclusion_space,
      LayoutUnit bfc_line_offset,
      LayoutUnit block_offset_delta);

  struct RareData {
    USING_FAST_MALLOC(RareData);

   public:
    RareData(LayoutUnit bfc_line_offset,
             base::Optional<LayoutUnit> bfc_block_offset)
        : bfc_line_offset(bfc_line_offset),
          bfc_block_offset(bfc_block_offset) {}

    LayoutUnit bfc_line_offset;
    base::Optional<LayoutUnit> bfc_block_offset;

    scoped_refptr<const NGEarlyBreak> early_break;
    NGBreakAppeal early_break_appeal = kBreakAppealLastResort;
    LogicalOffset oof_positioned_offset;
    NGMarginStrut end_margin_strut;
    NGUnpositionedListMarker unpositioned_list_marker;
    NGBlockNode column_spanner = nullptr;
    union {
      // Only set in the initial column balancing layout pass, when we have no
      // clue what the column block-size is going to be.
      LayoutUnit tallest_unbreakable_block_size;

      // Only set in subsequent column balancing passes, when we have set a
      // tentative column block-size. At every column boundary we'll record
      // space shortage, and store the smallest one here. If the columns
      // couldn't fit all the content, and we're allowed to stretch columns
      // further, we'll perform another pass with the column block-size
      // increased by this amount.
      LayoutUnit minimal_space_shortage = kIndefiniteSize;
    };
    NGExclusionSpace exclusion_space;
    scoped_refptr<SerializedScriptValue> custom_layout_data;

    LayoutUnit overflow_block_size = kIndefiniteSize;
    LayoutUnit annotation_overflow;
    LayoutUnit block_end_annotation_space;
    bool is_single_use = false;
    int lines_until_clamp = 0;
    wtf_size_t table_column_count_ = 0;
  };

  bool HasRareData() const { return bitfields_.has_rare_data; }
  RareData* EnsureRareData();

#if DCHECK_IS_ON()
  void AssertSoleBoxFragment() const;
#endif

  struct Bitfields {
    DISALLOW_NEW();

   public:
    // We define the default constructor so that the |has_rare_data| bit is
    // never uninitialized (potentially allowing a dangling pointer).
    Bitfields()
        : Bitfields(
              /* is_self_collapsing */ false,
              /* is_pushed_by_floats */ false,
              /* adjoining_object_types */ kAdjoiningNone,
              /* has_descendant_that_depends_on_percentage_block_size */
              false) {}
    Bitfields(bool is_self_collapsing,
              bool is_pushed_by_floats,
              NGAdjoiningObjectTypes adjoining_object_types,
              bool has_descendant_that_depends_on_percentage_block_size)
        : has_rare_data(false),
          has_rare_data_exclusion_space(false),
          has_oof_positioned_offset(false),
          can_use_out_of_flow_positioned_first_tier_cache(false),
          is_bfc_block_offset_nullopt(false),
          has_forced_break(false),
          is_self_collapsing(is_self_collapsing),
          is_pushed_by_floats(is_pushed_by_floats),
          adjoining_object_types(static_cast<unsigned>(adjoining_object_types)),
          is_initial_block_size_indefinite(false),
          has_descendant_that_depends_on_percentage_block_size(
              has_descendant_that_depends_on_percentage_block_size),
          subtree_modified_margin_strut(false),
          initial_break_before(static_cast<unsigned>(EBreakBetween::kAuto)),
          final_break_after(static_cast<unsigned>(EBreakBetween::kAuto)),
          status(static_cast<unsigned>(kSuccess)) {}

    unsigned has_rare_data : 1;
    unsigned has_rare_data_exclusion_space : 1;
    unsigned has_oof_positioned_offset : 1;
    unsigned can_use_out_of_flow_positioned_first_tier_cache : 1;
    unsigned is_bfc_block_offset_nullopt : 1;

    unsigned has_forced_break : 1;

    unsigned is_self_collapsing : 1;
    unsigned is_pushed_by_floats : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes

    unsigned is_initial_block_size_indefinite : 1;
    unsigned has_descendant_that_depends_on_percentage_block_size : 1;

    unsigned subtree_modified_margin_strut : 1;

    unsigned initial_break_before : 4;  // EBreakBetween
    unsigned final_break_after : 4;     // EBreakBetween

    unsigned status : 3;  // EStatus
  };

  // The constraint space which generated this layout result, may not be valid
  // as indicated by |has_valid_space_|.
  const NGConstraintSpace space_;

  scoped_refptr<const NGPhysicalContainerFragment> physical_fragment_;

  // To save space, we union these fields.
  //  - |rare_data_| is valid if the |Bitfields::has_rare_data| bit is set.
  //    |bfc_offset_| and |oof_positioned_offset_| are stored within the
  //    |RareData| object for this case.
  //  - |oof_positioned_offset_| is valid if the
  //    |Bitfields::has_oof_positioned_offset| bit is set. As the node is
  //    OOF-positioned the |bfc_offset_| is *always* the initial value.
  //  - Otherwise |bfc_offset_| is valid.
  union {
    NGBfcOffset bfc_offset_;
    // This is the final position of an OOF-positioned object in its parent's
    // writing-mode. This is set by the |NGOutOfFlowLayoutPart| while
    // generating this layout result.
    LogicalOffset oof_positioned_offset_;
    RareData* rare_data_;
  };

  LayoutUnit intrinsic_block_size_;
  Bitfields bitfields_;

#if DCHECK_IS_ON()
  bool has_valid_space_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
