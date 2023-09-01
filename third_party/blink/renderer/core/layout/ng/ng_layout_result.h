// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_data.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/grid/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_early_break.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_link.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/non_overflowing_scroll_range.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/bit_field.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGColumnSpannerPath;
class NGExclusionSpace;
class NGFragmentBuilder;
class NGLineBoxFragmentBuilder;

// The NGLayoutResult stores the resulting data from layout. This includes
// geometry information in form of a NGPhysicalFragment, which is kept around
// for painting, hit testing, etc., as well as additional data which is only
// necessary during layout and stored on this object.
// Layout code should access the NGPhysicalFragment through the wrappers in
// NGFragment et al.
class CORE_EXPORT NGLayoutResult final
    : public GarbageCollected<NGLayoutResult> {
 public:
  enum EStatus {
    kSuccess = 0,
    kBfcBlockOffsetResolved = 1,
    kNeedsEarlierBreak = 2,
    kOutOfFragmentainerSpace = 3,
    kNeedsRelayoutWithNoForcedTruncateAtLineClamp = 4,
    kDisableFragmentation = 5,
    kNeedsRelayoutWithNoChildScrollbarChanges = 6,
    kAlgorithmSpecific1 = 7,  // Save bits by using the same value for mutually
                              // exclusive results.
    kNeedsRelayoutWithRowCrossSizeChanges = kAlgorithmSpecific1,
    kNeedsRelayoutAsLastTableBox = kAlgorithmSpecific1,
    // When adding new values, make sure the bit size of |Bitfields::status| is
    // large enough to store.
  };

  // Make a shallow clone of the result. The fragment is cloned. Fragment
  // *items* are also cloned, but child fragments are not. Apart from that it's
  // truly shallow. Pinky promise.
  static const NGLayoutResult* Clone(const NGLayoutResult&);

  // Same as Clone(), but uses the "post-layout" fragments to ensure
  // fragment-tree consistency.
  static const NGLayoutResult* CloneWithPostLayoutFragments(
      const NGLayoutResult& other,
      const absl::optional<PhysicalRect> updated_layout_overflow =
          absl::nullopt);

  // Create a copy of NGLayoutResult with |BfcBlockOffset| replaced by the given
  // parameter. Note, when |bfc_block_offset| is |nullopt|, |BfcBlockOffset| is
  // still replaced with |nullopt|.
  NGLayoutResult(const NGLayoutResult& other,
                 const NGConstraintSpace& new_space,
                 const NGMarginStrut& new_end_margin_strut,
                 LayoutUnit bfc_line_offset,
                 absl::optional<LayoutUnit> bfc_block_offset,
                 LayoutUnit block_offset_delta);

  // Creates a copy of NGLayoutResult with a new (but "identical") fragment.
  NGLayoutResult(const NGLayoutResult& other,
                 const NGPhysicalFragment* physical_fragment);

  // Delegate constructor that sets up what it can, based on the builder.
  NGLayoutResult(const NGPhysicalFragment* physical_fragment,
                 NGFragmentBuilder* builder);

  // We don't need the copy constructor, move constructor, copy
  // assigmnment-operator, or move assignment-operator today.
  // If at some point we do need these constructors particular care will need
  // to be taken with the |rare_data_| field.
  NGLayoutResult(const NGLayoutResult&) = delete;
  NGLayoutResult(NGLayoutResult&&) = delete;
  NGLayoutResult& operator=(const NGLayoutResult& other) = delete;
  NGLayoutResult& operator=(NGLayoutResult&& other) = delete;
  NGLayoutResult() = delete;

  ~NGLayoutResult() = default;

  const NGPhysicalFragment& PhysicalFragment() const {
    DCHECK(physical_fragment_);
    DCHECK_EQ(kSuccess, Status());
    return *physical_fragment_;
  }

  int LinesUntilClamp() const {
    return rare_data_ ? rare_data_->lines_until_clamp : 0;
  }

  // Return true if this is an orthogonal writing-mode root that depends on the
  // size of the initial containing block.
  bool HasOrthogonalFallbackInlineSize() const {
    return space_.UsesOrthogonalFallbackInlineSize();
  }

  // Return true if there's an orthogonal writing-mode root descendant inside
  // that depends on the size of the initial containing block.
  bool HasOrthogonalFallbackSizeDescendant() const {
    return bitfields_.has_orthogonal_fallback_size_descendant;
  }

  // Return the adjustment baked into the fragment's block-offset that's caused
  // by ruby annotations.
  LayoutUnit AnnotationBlockOffsetAdjustment() const {
    if (!rare_data_) {
      return LayoutUnit();
    }
    const RareData::LineData* data = rare_data_->GetLineData();
    return data ? data->annotation_block_offset_adjustment : LayoutUnit();
  }

  // How much an annotation box overflow from this box.
  // This is for LayoutRubyColumn and line boxes.
  // 0 : No overflow
  // -N : Overflowing by N px at block-start side
  //      This happens only for LayoutRubyColumn.
  // N : Overflowing by N px at block-end side
  LayoutUnit AnnotationOverflow() const {
    return rare_data_ ? rare_data_->annotation_overflow : LayoutUnit();
  }

  // The amount of available space for block-start side annotations of the
  // next box.
  // This never be negative.
  LayoutUnit BlockEndAnnotationSpace() const {
    return rare_data_ ? rare_data_->block_end_annotation_space : LayoutUnit();
  }

  LogicalOffset OutOfFlowPositionedOffset() const {
    // The offset is either explicitly stored on the rare data, or impliclty
    // stored as the start offset of |oof_insets_for_get_computed_style_|.
    CHECK(bitfields_.has_oof_insets_for_get_computed_style);
    return rare_data_ && rare_data_->oof_positioned_offset_is_set()
               ? rare_data_->OutOfFlowPositionedOffset()
               : oof_insets_for_get_computed_style_.StartOffset();
  }

  // Returns the absolutized inset property values in the parent's writing mode.
  // Not necessarily the insets of the actual box in the container, but matches
  // the result of the `getComputedStyle()` JavaScript API.
  const NGBoxStrut& OutOfFlowInsetsForGetComputedStyle() const {
    CHECK(bitfields_.has_oof_insets_for_get_computed_style);
    return oof_insets_for_get_computed_style_;
  }

  // Called after subtree layout to make sure the fields for out-of-flow
  // positioned nodes are set.
  void CopyMutableOutOfFlowData(const NGLayoutResult& previous_result) const;

  // Returns if we can use the first-tier OOF-positioned cache.
  bool CanUseOutOfFlowPositionedFirstTierCache() const {
    DCHECK(physical_fragment_->IsOutOfFlowPositioned());
    return bitfields_.can_use_out_of_flow_positioned_first_tier_cache;
  }

  absl::optional<wtf_size_t> PositionFallbackIndex() const {
    return rare_data_ ? rare_data_->PositionFallbackIndex() : absl::nullopt;
  }
  const Vector<NonOverflowingScrollRange>*
  PositionFallbackNonOverflowingRanges() const {
    return rare_data_ ? rare_data_->PositionFallbackNonOverflowingRanges()
                      : nullptr;
  }

  // Get the path to the column spanner (if any) that interrupted column layout.
  const NGColumnSpannerPath* ColumnSpannerPath() const {
    if (rare_data_) {
      if (const RareData::BlockData* data = rare_data_->GetBlockData())
        return data->column_spanner_path;
    }
    return nullptr;
  }

  // True if this result is the parent of a column spanner and is empty (i.e.
  // has no children). This is used to determine whether the column spanner
  // margins should collapse. Note that |is_empty_spanner_parent| may be false
  // even if this column spanner parent is actually empty. This can happen in
  // the case where the spanner parent has no children but has not broken
  // previously - in which case, we shouldn't collapse the spanner margins since
  // we do not want to collapse margins with a column spanner outside of this
  // parent.
  bool IsEmptySpannerParent() const {
    return bitfields_.is_empty_spanner_parent;
  }

  const NGEarlyBreak* GetEarlyBreak() const {
    if (!rare_data_) {
      return nullptr;
    }
    return rare_data_->early_break;
  }

  const NGExclusionSpace& ExclusionSpace() const {
    if (bitfields_.has_rare_data_exclusion_space) {
      DCHECK(rare_data_);
      return rare_data_->exclusion_space;
    }

    return space_.ExclusionSpace();
  }

  EStatus Status() const { return static_cast<EStatus>(bitfields_.status); }

  LayoutUnit BfcLineOffset() const {
    if (bitfields_.has_oof_insets_for_get_computed_style) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    return bfc_offset_.line_offset;
  }

  const absl::optional<LayoutUnit> BfcBlockOffset() const {
    if (bitfields_.has_oof_insets_for_get_computed_style) {
      DCHECK(physical_fragment_->IsOutOfFlowPositioned());
      return LayoutUnit();
    }

    if (bitfields_.is_bfc_block_offset_nullopt)
      return absl::nullopt;

    return bfc_offset_.block_offset;
  }

  // The BFC block-offset where a line-box has been placed. Will be nullopt if
  // it isn't a line-box, or an empty line-box.
  //
  // This can be different (but rarely) to where the |BfcBlockOffset()|
  // resolves to, when floats are present. E.g.
  //
  // <div style="width: 100px; display: flow-root;">
  //   <div style="float: left; width: 200px; height: 20px;"></div>
  //   text
  // </div>
  //
  // In the above example the |BfcBlockOffset()| will be at 0px, where-as the
  // |LineBoxBfcBlockOffset()| will be at 20px.
  absl::optional<LayoutUnit> LineBoxBfcBlockOffset() const {
    if (Status() != kSuccess || !PhysicalFragment().IsLineBox())
      return absl::nullopt;

    if (rare_data_) {
      if (absl::optional<LayoutUnit> offset =
              rare_data_->LineBoxBfcBlockOffset())
        return offset;
    }

    return BfcBlockOffset();
  }

  const NGMarginStrut EndMarginStrut() const {
    return rare_data_ ? rare_data_->end_margin_strut : NGMarginStrut();
  }

  // Get the intrinsic block-size of the fragment. This is the block-size the
  // fragment would get if no block-size constraints were applied and, for
  // non-replaced elements, no inline-size constraints were applied through any
  // aspect-ratio (For replaced elements, inline-size constraints ARE applied
  // through the aspect-ratio).

  // This is not supported (and should not be needed [1]) if the node got split
  // into multiple fragments.
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

  // Return the amount of clearance that we have to add after the fragment. This
  // is used for BR clear elements.
  LayoutUnit ClearanceAfterLine() const {
    if (!rare_data_) {
      return LayoutUnit();
    }
    const RareData::LineData* data = rare_data_->GetLineData();
    return data ? data->clearance_after_line : LayoutUnit();
  }

  absl::optional<LayoutUnit> MinimalSpaceShortage() const {
    if (!rare_data_ || space_.IsInitialColumnBalancingPass() ||
        rare_data_->minimal_space_shortage == kIndefiniteSize) {
      return absl::nullopt;
    }
    return rare_data_->minimal_space_shortage;
  }

  LayoutUnit TallestUnbreakableBlockSize() const {
    if (!rare_data_ || !space_.IsInitialColumnBalancingPass() ||
        rare_data_->tallest_unbreakable_block_size == kIndefiniteSize) {
      return LayoutUnit();
    }
    return rare_data_->tallest_unbreakable_block_size;
  }

  // Return the block-size that this fragment will take up inside a
  // fragmentation context. This will include overflow from descendants (if it
  // is visible and supposed to affect block fragmentation), and also
  // out-of-flow positioned descendants (in the initial balancing pass), but not
  // relative offsets. kIndefiniteSize will be returned if block fragmentation
  // wasn't performed on the node (e.g. monolithic content such as line boxes,
  // or if the node isn't inside a fragmentation context at all).
  LayoutUnit BlockSizeForFragmentation() const {
    if (!rare_data_) {
      return kIndefiniteSize;
    }
    return rare_data_->block_size_for_fragmentation;
  }

  // Return true if the block-size for fragmentation (see
  // BlockSizeForFragmentation()) got clamped. If this is the case, we cannot
  // use BlockSizeForFragmentation() for cache testing.
  bool IsBlockSizeForFragmentationClamped() const {
    return bitfields_.is_block_size_for_fragmentation_clamped;
  }

  // Return true if this generating node must stay within the same fragmentation
  // flow as the parent (and not establish a parallel fragmentation flow), even
  // if it has content that overflows into the next fragmentainer.
  bool ShouldForceSameFragmentationFlow() const {
    return bitfields_.should_force_same_fragmentation_flow;
  }

  // Return the (lowest) appeal among any unforced breaks inside the resulting
  // fragment (or kBreakAppealPerfect if there are no such breaks).
  //
  // A higher value is better. Violating breaking rules decreases appeal. Forced
  // breaks always have perfect appeal.
  //
  // If a node breaks, the resulting fragment usually carries an outgoing break
  // token, but this isn't necessarily the case if the break happened inside an
  // inner fragmentation context. The block-size of an inner multicol is
  // constrained by the available block-size in the outer fragmentation
  // context. This may cause suboptimal column breaks inside. The entire inner
  // multicol container may fit in the outer fragmentation context, but we may
  // also need to consider the inner column breaks (in an inner fragmentation
  // context). If there are any suboptimal breaks, we may want to push the
  // entire multicol container to the next outer fragmentainer, if it's likely
  // that we'll avoid suboptimal column breaks inside that way.
  NGBreakAppeal BreakAppeal() const {
    return static_cast<NGBreakAppeal>(bitfields_.break_appeal);
  }

  SerializedScriptValue* CustomLayoutData() const {
    return rare_data_ ? rare_data_->custom_layout_data.get() : nullptr;
  }

  wtf_size_t TableColumnCount() const {
    if (!rare_data_) {
      return 0;
    }
    const RareData::TableData* data = rare_data_->GetTableData();
    return data ? data->table_column_count : 0;
  }

  const NGGridLayoutData* GridLayoutData() const {
    if (!rare_data_) {
      return nullptr;
    }
    const RareData::GridData* data = rare_data_->GetGridData();
    return data ? data->grid_layout_data.get() : nullptr;
  }

  const DevtoolsFlexInfo* FlexLayoutData() const {
    if (!rare_data_) {
      return nullptr;
    }
    const RareData::FlexData* data = rare_data_->GetFlexData();
    return data ? data->flex_layout_data.get() : nullptr;
  }

  LayoutUnit MathItalicCorrection() const {
    if (!rare_data_) {
      return LayoutUnit();
    }
    const RareData::MathData* data = rare_data_->GetMathData();
    return data ? data->italic_correction : LayoutUnit();
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

  // Returns true if the fragment got truncated because it reached the
  // fragmentation line. This typically means that we cannot re-use (cache-hit)
  // this fragment if the fragmentation line moves.
  bool IsTruncatedByFragmentationLine() const {
    return bitfields_.is_truncated_by_fragmentation_line;
  }

  // Returns the space which generated this object for caching purposes.
  const NGConstraintSpace& GetConstraintSpaceForCaching() const {
    return space_;
  }

  // This exposes a mutable part of the layout result just for the
  // |NGOutOfFlowLayoutPart|.
  class MutableForOutOfFlow final {
    STACK_ALLOCATED();

   protected:
    friend class NGOutOfFlowLayoutPart;

    void SetOutOfFlowInsetsForGetComputedStyle(
        const NGBoxStrut& insets,
        bool can_use_out_of_flow_positioned_first_tier_cache) {
      // OOF-positioned nodes *must* always have an initial BFC-offset.
      DCHECK(layout_result_->physical_fragment_->IsOutOfFlowPositioned());
      DCHECK_EQ(layout_result_->BfcLineOffset(), LayoutUnit());
      DCHECK_EQ(layout_result_->BfcBlockOffset().value_or(LayoutUnit()),
                LayoutUnit());

      layout_result_->bitfields_
          .can_use_out_of_flow_positioned_first_tier_cache =
          can_use_out_of_flow_positioned_first_tier_cache;
      layout_result_->bitfields_.has_oof_insets_for_get_computed_style = true;
      layout_result_->oof_insets_for_get_computed_style_ = insets;
    }

    void SetOutOfFlowPositionedOffset(const LogicalOffset& offset) {
      CHECK(layout_result_->bitfields_.has_oof_insets_for_get_computed_style);
      // To minimize the chance of creating a rare data, we explicitly store
      // |offset| on rare data only if:
      // 1. There's already an offset stored on rare data, in which case we
      //    simply update it regardlessly.
      // 2. It no longer matches the start offset of the stored insets.
      if ((layout_result_->rare_data_ &&
           layout_result_->rare_data_->oof_positioned_offset_is_set()) ||
          offset != layout_result_->oof_insets_for_get_computed_style_
                        .StartOffset()) {
        layout_result_->EnsureRareData()->SetOutOfFlowPositionedOffset(offset);
      }
    }

    void SetPositionFallbackResult(
        wtf_size_t fallback_index,
        const Vector<NonOverflowingScrollRange>& non_overflowing_ranges) {
      layout_result_->EnsureRareData()->SetPositionFallbackResult(
          fallback_index, non_overflowing_ranges);
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

  class MutableForLayoutBoxCachedResults final {
    STACK_ALLOCATED();

   protected:
    friend class LayoutBox;

    void SetFragmentChildrenInvalid() {
      layout_result_->physical_fragment_->SetChildrenInvalid();
    }

   private:
    friend class NGLayoutResult;
    explicit MutableForLayoutBoxCachedResults(
        const NGLayoutResult* layout_result)
        : layout_result_(const_cast<NGLayoutResult*>(layout_result)) {}

    NGLayoutResult* layout_result_;
  };

  MutableForLayoutBoxCachedResults GetMutableForLayoutBoxCachedResults() const {
    return MutableForLayoutBoxCachedResults(this);
  }

#if DCHECK_IS_ON()
  void CheckSameForSimplifiedLayout(const NGLayoutResult&,
                                    bool check_same_block_size = true,
                                    bool check_no_fragmentation = true) const;
#endif

  using NGFragmentBuilderPassKey = base::PassKey<NGFragmentBuilder>;
  // This constructor is for a non-success status.
  NGLayoutResult(NGFragmentBuilderPassKey, EStatus, NGFragmentBuilder*);

  // This constructor requires a non-null fragment and sets a success status.
  using NGBoxFragmentBuilderPassKey = base::PassKey<NGBoxFragmentBuilder>;
  NGLayoutResult(NGBoxFragmentBuilderPassKey,
                 const NGPhysicalFragment* physical_fragment,
                 NGBoxFragmentBuilder*);

  using NGLineBoxFragmentBuilderPassKey =
      base::PassKey<NGLineBoxFragmentBuilder>;
  // This constructor requires a non-null fragment and sets a success status.
  NGLayoutResult(NGLineBoxFragmentBuilderPassKey,
                 const NGPhysicalFragment* physical_fragment,
                 NGLineBoxFragmentBuilder*);

  void Trace(Visitor*) const;

 private:
  friend class MutableForOutOfFlow;

  static NGExclusionSpace MergeExclusionSpaces(
      const NGLayoutResult& other,
      const NGExclusionSpace& new_input_exclusion_space,
      LayoutUnit bfc_line_offset,
      LayoutUnit block_offset_delta);

  struct RareData final : public GarbageCollected<RareData> {
   public:
    // RareData has fields which are mutually exclusive. They are grouped into
    // unions.
    //
    // NOTE: Make sure that data_union_type has enough bits to express all these
    // enum values.
    enum DataUnionType {
      kNone,
      kBlockData,
      kFlexData,
      kGridData,
      kLineData,
      kMathData,
      kTableData,
    };

    using BitField = WTF::ConcurrentlyReadBitField<uint8_t>;
    using LineBoxBfcBlockOffsetIsSetFlag = BitField::DefineFirstValue<bool, 1>;
    using PositionFallbackResultIsSetFlag =
        LineBoxBfcBlockOffsetIsSetFlag::DefineNextValue<bool, 1>;
    using OutOfFlowPositionedOffsetIsSetFlag =
        PositionFallbackResultIsSetFlag::DefineNextValue<bool, 1>;
    using DataUnionTypeValue =
        OutOfFlowPositionedOffsetIsSetFlag::DefineNextValue<uint8_t, 3>;

    struct BlockData {
      GC_PLUGIN_IGNORE("crbug.com/1146383")
      Member<const NGColumnSpannerPath> column_spanner_path;
    };

    struct FlexData {
      FlexData() = default;
      FlexData(const FlexData& other) {
        flex_layout_data =
            std::make_unique<DevtoolsFlexInfo>(*other.flex_layout_data);
      }

      std::unique_ptr<const DevtoolsFlexInfo> flex_layout_data;
    };

    struct GridData {
      GridData() = default;
      GridData(const GridData& other) {
        grid_layout_data =
            std::make_unique<NGGridLayoutData>(*other.grid_layout_data);
      }

      std::unique_ptr<const NGGridLayoutData> grid_layout_data;
    };

    struct LineData {
      LayoutUnit clearance_after_line;
      LayoutUnit annotation_block_offset_adjustment;
    };

    struct MathData {
      // See https://w3c.github.io/mathml-core/#box-model
      LayoutUnit italic_correction;
    };

    struct TableData {
      wtf_size_t table_column_count = 0;
    };

    bool line_box_bfc_block_offset_is_set() const {
      return bit_field.get<LineBoxBfcBlockOffsetIsSetFlag>();
    }

    void set_line_box_bfc_block_offset_is_set(bool flag) {
      return bit_field.set<LineBoxBfcBlockOffsetIsSetFlag>(flag);
    }

    bool position_fallback_result_is_set() const {
      return bit_field.get<PositionFallbackResultIsSetFlag>();
    }

    void set_position_fallback_result_is_set(bool flag) {
      return bit_field.set<PositionFallbackResultIsSetFlag>(flag);
    }

    bool oof_positioned_offset_is_set() const {
      return bit_field.get<OutOfFlowPositionedOffsetIsSetFlag>();
    }

    void set_oof_positioned_offset_is_set(bool flag) {
      return bit_field.set<OutOfFlowPositionedOffsetIsSetFlag>(flag);
    }

    DataUnionType data_union_type() const {
      return static_cast<DataUnionType>(
          bit_field.get_concurrently<DataUnionTypeValue>());
    }

    void set_data_union_type(DataUnionType data_type) {
      return bit_field.set<DataUnionTypeValue>(static_cast<uint8_t>(data_type));
    }

    template <typename DataType>
    DataType* EnsureData(DataType* address, DataUnionType data_type) {
      DataUnionType old_data_type = data_union_type();
      DCHECK(old_data_type == kNone || old_data_type == data_type);
      if (old_data_type != data_type) {
        set_data_union_type(data_type);
        new (address) DataType();
      }
      return address;
    }
    template <typename DataType>
    const DataType* GetData(const DataType* address,
                            DataUnionType data_type) const {
      return data_union_type() == data_type ? address : nullptr;
    }

    BlockData* EnsureBlockData() {
      return EnsureData<BlockData>(&block_data, kBlockData);
    }
    const BlockData* GetBlockData() const {
      return GetData<BlockData>(&block_data, kBlockData);
    }
    FlexData* EnsureFlexData() {
      return EnsureData<FlexData>(&flex_data, kFlexData);
    }
    const FlexData* GetFlexData() const {
      return GetData<FlexData>(&flex_data, kFlexData);
    }
    GridData* EnsureGridData() {
      return EnsureData<GridData>(&grid_data, kGridData);
    }
    const GridData* GetGridData() const {
      return GetData<GridData>(&grid_data, kGridData);
    }
    LineData* EnsureLineData() {
      return EnsureData<LineData>(&line_data, kLineData);
    }
    const LineData* GetLineData() const {
      return GetData<LineData>(&line_data, kLineData);
    }
    MathData* EnsureMathData() {
      return EnsureData<MathData>(&math_data, kMathData);
    }
    const MathData* GetMathData() const {
      return GetData<MathData>(&math_data, kMathData);
    }
    TableData* EnsureTableData() {
      return EnsureData<TableData>(&table_data, kTableData);
    }
    const TableData* GetTableData() const {
      return GetData<TableData>(&table_data, kTableData);
    }

    RareData() : bit_field(DataUnionTypeValue::encode(kNone)) {}

    RareData(const RareData& rare_data)
        : early_break(rare_data.early_break),
          end_margin_strut(rare_data.end_margin_strut),
          // This will initialize "both" members of the union.
          tallest_unbreakable_block_size(
              rare_data.tallest_unbreakable_block_size),
          block_size_for_fragmentation(rare_data.block_size_for_fragmentation),
          exclusion_space(rare_data.exclusion_space),
          custom_layout_data(rare_data.custom_layout_data),
          annotation_overflow(rare_data.annotation_overflow),
          block_end_annotation_space(rare_data.block_end_annotation_space),
          lines_until_clamp(rare_data.lines_until_clamp),
          line_box_bfc_block_offset(rare_data.line_box_bfc_block_offset),
          position_fallback_index(rare_data.position_fallback_index),
          position_fallback_non_overflowing_ranges(
              rare_data.position_fallback_non_overflowing_ranges),
          oof_positioned_offset(rare_data.oof_positioned_offset),
          bit_field(rare_data.bit_field) {
      switch (data_union_type()) {
        case kNone:
          break;
        case kBlockData:
          new (&block_data) BlockData(rare_data.block_data);
          break;
        case kFlexData:
          new (&flex_data) FlexData(rare_data.flex_data);
          break;
        case kGridData:
          new (&grid_data) GridData(rare_data.grid_data);
          break;
        case kLineData:
          new (&line_data) LineData(rare_data.line_data);
          break;
        case kMathData:
          new (&math_data) MathData(rare_data.math_data);
          break;
        case kTableData:
          new (&table_data) TableData(rare_data.table_data);
          break;
        default:
          NOTREACHED();
      }
    }

    ~RareData() {
      switch (data_union_type()) {
        case kNone:
          break;
        case kBlockData:
          block_data.~BlockData();
          break;
        case kFlexData:
          flex_data.~FlexData();
          break;
        case kGridData:
          grid_data.~GridData();
          break;
        case kLineData:
          line_data.~LineData();
          break;
        case kMathData:
          math_data.~MathData();
          break;
        case kTableData:
          table_data.~TableData();
          break;
        default:
          NOTREACHED();
      }
    }

    void SetLineBoxBfcBlockOffset(LayoutUnit offset) {
      line_box_bfc_block_offset = offset;
      set_line_box_bfc_block_offset_is_set(true);
    }
    absl::optional<LayoutUnit> LineBoxBfcBlockOffset() const {
      if (!line_box_bfc_block_offset_is_set())
        return absl::nullopt;
      return line_box_bfc_block_offset;
    }

    void SetPositionFallbackResult(
        wtf_size_t fallback_index,
        const Vector<NonOverflowingScrollRange>& non_overflowing_ranges) {
      position_fallback_index = fallback_index;
      position_fallback_non_overflowing_ranges = non_overflowing_ranges;
      set_position_fallback_result_is_set(true);
    }
    absl::optional<wtf_size_t> PositionFallbackIndex() const {
      if (!position_fallback_result_is_set()) {
        return absl::nullopt;
      }
      return position_fallback_index;
    }
    const Vector<NonOverflowingScrollRange>*
    PositionFallbackNonOverflowingRanges() const {
      if (!position_fallback_result_is_set()) {
        return nullptr;
      }
      return &position_fallback_non_overflowing_ranges;
    }

    void SetOutOfFlowPositionedOffset(const LogicalOffset& offset) {
      oof_positioned_offset = offset;
      set_oof_positioned_offset_is_set(true);
    }
    LogicalOffset OutOfFlowPositionedOffset() const {
      CHECK(oof_positioned_offset_is_set());
      return oof_positioned_offset;
    }

    void Trace(Visitor* visitor) const;

    Member<const NGEarlyBreak> early_break;
    NGMarginStrut end_margin_strut;
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
    LayoutUnit block_size_for_fragmentation = kIndefiniteSize;
    NGExclusionSpace exclusion_space;
    scoped_refptr<SerializedScriptValue> custom_layout_data;

    LayoutUnit annotation_overflow;
    LayoutUnit block_end_annotation_space;
    int lines_until_clamp = 0;

   private:
    // Only valid if line_box_bfc_block_offset_is_set
    LayoutUnit line_box_bfc_block_offset;

    // Only valid if position_fallback_result_is_set
    wtf_size_t position_fallback_index;
    Vector<NonOverflowingScrollRange> position_fallback_non_overflowing_ranges;

    // Only valid if oof_positioned_offset_is_set
    LogicalOffset oof_positioned_offset;

    BitField bit_field;

    union {
      BlockData block_data;
      FlexData flex_data;
      GridData grid_data;
      LineData line_data;
      MathData math_data;
      TableData table_data;
    };
  };
  RareData* EnsureRareData();

#if DCHECK_IS_ON()
  void AssertSoleBoxFragment() const;
#endif

  struct Bitfields {
    DISALLOW_NEW();

   public:
    Bitfields()
        : Bitfields(
              /* is_self_collapsing */ false,
              /* is_pushed_by_floats */ false,
              /* adjoining_object_types */ kAdjoiningNone,
              /* has_descendant_that_depends_on_percentage_block_size */
              false,
              /* subtree_modified_margin_strut */ false) {}
    Bitfields(bool is_self_collapsing,
              bool is_pushed_by_floats,
              NGAdjoiningObjectTypes adjoining_object_types,
              bool has_descendant_that_depends_on_percentage_block_size,
              bool subtree_modified_margin_strut)
        : has_rare_data_exclusion_space(false),
          has_oof_insets_for_get_computed_style(false),
          can_use_out_of_flow_positioned_first_tier_cache(false),
          is_bfc_block_offset_nullopt(false),
          has_forced_break(false),
          break_appeal(kBreakAppealPerfect),
          is_empty_spanner_parent(false),
          is_block_size_for_fragmentation_clamped(false),
          should_force_same_fragmentation_flow(false),
          is_self_collapsing(is_self_collapsing),
          is_pushed_by_floats(is_pushed_by_floats),
          adjoining_object_types(static_cast<unsigned>(adjoining_object_types)),
          is_initial_block_size_indefinite(false),
          has_descendant_that_depends_on_percentage_block_size(
              has_descendant_that_depends_on_percentage_block_size),
          subtree_modified_margin_strut(subtree_modified_margin_strut),
          initial_break_before(static_cast<unsigned>(EBreakBetween::kAuto)),
          final_break_after(static_cast<unsigned>(EBreakBetween::kAuto)),
          status(static_cast<unsigned>(kSuccess)),
          is_truncated_by_fragmentation_line(false),
          has_orthogonal_fallback_size_descendant(false) {}

    unsigned has_rare_data_exclusion_space : 1;
    unsigned has_oof_insets_for_get_computed_style : 1;
    unsigned can_use_out_of_flow_positioned_first_tier_cache : 1;
    unsigned is_bfc_block_offset_nullopt : 1;

    unsigned has_forced_break : 1;
    unsigned break_appeal : kNGBreakAppealBitsNeeded;
    unsigned is_empty_spanner_parent : 1;
    unsigned is_block_size_for_fragmentation_clamped : 1;
    unsigned should_force_same_fragmentation_flow : 1;

    unsigned is_self_collapsing : 1;
    unsigned is_pushed_by_floats : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes

    unsigned is_initial_block_size_indefinite : 1;
    unsigned has_descendant_that_depends_on_percentage_block_size : 1;

    unsigned subtree_modified_margin_strut : 1;

    unsigned initial_break_before : 4;  // EBreakBetween
    unsigned final_break_after : 4;     // EBreakBetween

    unsigned status : 3;  // EStatus
    unsigned is_truncated_by_fragmentation_line : 1;
    unsigned has_orthogonal_fallback_size_descendant : 1;
  };

  // The constraint space which generated this layout result.
  const NGConstraintSpace space_;

  Member<const NGPhysicalFragment> physical_fragment_;

  // |rare_data_| cannot be stored in the union because it is difficult to have
  // a const bitfield for it and it cannot be traced.
  // Note that |bfc_offset_| and |oof_insets_for_get_computed_style_| cannot be
  // both valid at the same time, because an OOF-positioned node's BFC offset is
  // *always* the initial value.
  Member<RareData> rare_data_;
  union {
    NGBfcOffset bfc_offset_;
    // This is the absolutized inset property values of an OOF-positioned object
    // in its parent's writing-mode. This is set by the |NGOutOfFlowLayoutPart|
    // while generating this layout result.
    NGBoxStrut oof_insets_for_get_computed_style_;
  };

  LayoutUnit intrinsic_block_size_;
  Bitfields bitfields_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LAYOUT_RESULT_H_
