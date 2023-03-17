// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_H_

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_constraint_space_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class LayoutBlock;
class NGConstraintSpaceBuilder;

enum NGFragmentationType {
  kFragmentNone,
  kFragmentPage,
  kFragmentColumn,
  kFragmentRegion
};

// "adjoining" objects (either floats or inline-level OOF-positioned nodes) are
// used to indicate that a particular node might need a relayout once its BFC
// block-offset is resolved. E.g. their position depends on the final BFC
// block-offset being known.
enum NGAdjoiningObjectTypeValue {
  kAdjoiningNone = 0b000,
  kAdjoiningFloatLeft = 0b001,
  kAdjoiningFloatRight = 0b010,
  kAdjoiningFloatBoth = 0b011,
  kAdjoiningInlineOutOfFlow = 0b100
};
typedef int NGAdjoiningObjectTypes;

// The last baseline algorithm for an inline-blocks are complex. Depending on
// the layout algorithm type it'll select the first (table, flex, grid) or last
// (block-like) as the last baseline.
enum class NGBaselineAlgorithmType {
  // Compute the baselines normally.
  kDefault,
  // Compute the baseline(s) for when we are within an inline-block context.
  // This will select the first/last baseline as the "last" baseline depending
  // on the layout algorithm.
  kInlineBlock
};

// The behavior of the 'auto' keyword when used with a main-size.
enum class NGAutoBehavior : uint8_t {
  // We should shrink-to-fit within the available space.
  kFitContent,
  // We should stretch to the available space, but if there is an aspect-ratio
  // with a definite size in the opposite axis, we should transfer the definite
  // size through the aspect-ratio, and be the resulting size. This is a "weak"
  // stretch constraint.
  kStretchImplicit,
  // We should *always* stretch to the available space, even if we have an
  // aspect-ratio. This is a "strong" stretch constraint.
  kStretchExplicit
};

// Some layout algorithms have multiple layout passes. Between passes they
// typically have different results which we need to cache separately for
// performance reasons.
//
// This enum gives the caching logic a hint into which cache "slot" it should
// store a result in.
enum class NGCacheSlot { kLayout, kMeasure };

// The NGConstraintSpace represents a set of constraints and available space
// which a layout algorithm may produce a NGFragment within.
class CORE_EXPORT NGConstraintSpace final {
  // Though some STACK_ALLOCATED classes, |NGContainerFragmentBuilder| and
  // |NGLineBreaker|, have reference to it, DISALLOW_NEW is applied here for
  // performance reason.
  DISALLOW_NEW();

 public:
  // Percentages are frequently the same as the available-size, zero, or
  // indefinite (thanks non-quirks mode)! This enum encodes this information.
  enum NGPercentageStorage {
    kSameAsAvailable,
    kZero,
    kIndefinite,
    kRareDataPercentage
  };

  NGConstraintSpace()
      : NGConstraintSpace({WritingMode::kHorizontalTb, TextDirection::kLtr}) {}

  NGConstraintSpace(const NGConstraintSpace& other)
      : available_size_(other.available_size_),
        exclusion_space_(other.exclusion_space_),
        bitfields_(other.bitfields_) {
    if (HasRareData())
      rare_data_ = new RareData(*other.rare_data_);
    else
      bfc_offset_ = other.bfc_offset_;
  }
  NGConstraintSpace(NGConstraintSpace&& other)
      : available_size_(other.available_size_),
        exclusion_space_(std::move(other.exclusion_space_)),
        bitfields_(other.bitfields_) {
    if (HasRareData()) {
      rare_data_ = other.rare_data_;
      other.rare_data_ = nullptr;
    } else {
      bfc_offset_ = other.bfc_offset_;
    }
  }

  NGConstraintSpace& operator=(const NGConstraintSpace& other) {
    available_size_ = other.available_size_;
    if (HasRareData())
      delete rare_data_;
    if (other.HasRareData())
      rare_data_ = new RareData(*other.rare_data_);
    else
      bfc_offset_ = other.bfc_offset_;
    exclusion_space_ = other.exclusion_space_;
    bitfields_ = other.bitfields_;
    return *this;
  }
  NGConstraintSpace& operator=(NGConstraintSpace&& other) {
    available_size_ = other.available_size_;
    if (HasRareData())
      delete rare_data_;
    if (other.HasRareData()) {
      rare_data_ = other.rare_data_;
      other.rare_data_ = nullptr;
    } else {
      bfc_offset_ = other.bfc_offset_;
    }
    exclusion_space_ = std::move(other.exclusion_space_);
    bitfields_ = other.bitfields_;
    return *this;
  }

  NGConstraintSpace CloneWithoutFragmentation() const {
    DCHECK(HasBlockFragmentation());
    NGConstraintSpace copy = *this;
    copy.DisableFurtherFragmentation();
    return copy;
  }

  ~NGConstraintSpace() {
    if (HasRareData())
      delete rare_data_;
  }

  // Creates NGConstraintSpace representing LayoutObject's containing block.
  // This should live on NGBlockNode or another layout bridge and probably take
  // a root NGConstraintSpace.
  static NGConstraintSpace CreateFromLayoutObject(const LayoutBlock&);

  const NGExclusionSpace& ExclusionSpace() const { return exclusion_space_; }

  TextDirection Direction() const {
    return static_cast<TextDirection>(bitfields_.direction);
  }

  WritingMode GetWritingMode() const {
    return static_cast<WritingMode>(bitfields_.writing_mode);
  }

  WritingDirectionMode GetWritingDirection() const {
    return {GetWritingMode(), Direction()};
  }

  bool IsOrthogonalWritingModeRoot() const {
    return bitfields_.is_orthogonal_writing_mode_root;
  }

  // The available space size.
  // See: https://drafts.csswg.org/css-sizing/#available
  LogicalSize AvailableSize() const { return available_size_; }

  // The size to use for percentage resolution.
  // See: https://drafts.csswg.org/css-sizing/#percentage-sizing
  LayoutUnit PercentageResolutionInlineSize() const {
    switch (static_cast<NGPercentageStorage>(
        bitfields_.percentage_inline_storage)) {
      default:
        NOTREACHED();
        [[fallthrough]];
      case kSameAsAvailable:
        return available_size_.inline_size;
      case kZero:
        return LayoutUnit();
      case kIndefinite:
        return kIndefiniteSize;
      case kRareDataPercentage:
        DCHECK(HasRareData());
        return rare_data_->percentage_resolution_size.inline_size;
    }
  }

  LayoutUnit PercentageResolutionBlockSize() const {
    switch (
        static_cast<NGPercentageStorage>(bitfields_.percentage_block_storage)) {
      default:
        NOTREACHED();
        [[fallthrough]];
      case kSameAsAvailable:
        return available_size_.block_size;
      case kZero:
        return LayoutUnit();
      case kIndefinite:
        return kIndefiniteSize;
      case kRareDataPercentage:
        DCHECK(HasRareData());
        return rare_data_->percentage_resolution_size.block_size;
    }
  }

  LogicalSize PercentageResolutionSize() const {
    return {PercentageResolutionInlineSize(), PercentageResolutionBlockSize()};
  }

  LayoutUnit ReplacedPercentageResolutionInlineSize() const {
    return PercentageResolutionInlineSize();
  }

  LayoutUnit ReplacedPercentageResolutionBlockSize() const {
    switch (static_cast<NGPercentageStorage>(
        bitfields_.replaced_percentage_block_storage)) {
      case kSameAsAvailable:
        return available_size_.block_size;
      case kZero:
        return LayoutUnit();
      case kIndefinite:
        return kIndefiniteSize;
      case kRareDataPercentage:
        DCHECK(HasRareData());
        return rare_data_->replaced_percentage_resolution_block_size;
      default:
        NOTREACHED();
    }

    return available_size_.block_size;
  }

  // The size to use for percentage resolution of replaced elements.
  LogicalSize ReplacedPercentageResolutionSize() const {
    return {ReplacedPercentageResolutionInlineSize(),
            ReplacedPercentageResolutionBlockSize()};
  }

  // The size to use for percentage resolution for margin/border/padding.
  // They are always get computed relative to the inline size, in the parent
  // writing mode.
  LayoutUnit PercentageResolutionInlineSizeForParentWritingMode() const {
    if (!IsOrthogonalWritingModeRoot())
      return PercentageResolutionInlineSize();
    if (PercentageResolutionBlockSize() != kIndefiniteSize)
      return PercentageResolutionBlockSize();
    // TODO(mstensho): Figure out why we get here. It seems wrong, but we do get
    // here in some grid layout situations.
    return LayoutUnit();
  }

  absl::optional<MinMaxSizes> OverrideMinMaxBlockSizes() const {
    return HasRareData() ? rare_data_->OverrideMinMaxBlockSizes()
                         : absl::nullopt;
  }

  // Inline/block target stretch size constraints.
  // See:
  // https://w3c.github.io/mathml-core/#dfn-inline-stretch-size-constraint
  LayoutUnit TargetStretchInlineSize() const {
    return HasRareData() ? rare_data_->TargetStretchInlineSize()
                         : kIndefiniteSize;
  }

  bool HasTargetStretchInlineSize() const {
    return TargetStretchInlineSize() != kIndefiniteSize;
  }

  struct MathTargetStretchBlockSizes {
    LayoutUnit ascent;
    LayoutUnit descent;
  };

  absl::optional<MathTargetStretchBlockSizes> TargetStretchBlockSizes() const {
    return HasRareData() ? rare_data_->TargetStretchBlockSizes()
                         : absl::nullopt;
  }

  // Return the borders which should be used for a table-cell.
  NGBoxStrut TableCellBorders() const {
    return HasRareData() ? rare_data_->TableCellBorders() : NGBoxStrut();
  }

  wtf_size_t TableCellColumnIndex() const {
    return HasRareData() ? rare_data_->TableCellColumnIndex() : 0;
  }

  // Return the baseline offset which the table-cell children should align
  // their baseline to.
  absl::optional<LayoutUnit> TableCellAlignmentBaseline() const {
    return HasRareData() ? rare_data_->TableCellAlignmentBaseline()
                         : absl::nullopt;
  }

  bool IsTableCellHiddenForPaint() const {
    return HasRareData() ? rare_data_->IsTableCellHiddenForPaint() : false;
  }

  bool IsTableCellWithCollapsedBorders() const {
    return HasRareData() ? rare_data_->IsTableCellWithCollapsedBorders()
                         : false;
  }

  const NGTableConstraintSpaceData* TableData() const {
    return HasRareData() ? rare_data_->TableData() : nullptr;
  }

  wtf_size_t TableRowIndex() const {
    return HasRareData() ? rare_data_->TableRowIndex() : kNotFound;
  }

  wtf_size_t TableSectionIndex() const {
    return HasRareData() ? rare_data_->TableSectionIndex() : kNotFound;
  }

  // Return any current page name, specified on an ancestor, or here.
  const AtomicString PageName() const {
    return HasRareData() ? rare_data_->page_name : AtomicString();
  }

  // If we're block-fragmented AND the fragmentainer block-size is known, return
  // the total block-size of the fragmentainer that is to be created. This value
  // is inherited by descendant constraint spaces, as long as we don't enter
  // anything monolithic, or establish a nested fragmentation context. Note that
  // the value returned here is the actual size that will be set on the physical
  // fragment representing the fragmentainer, and 0 is an allowed value, even if
  // the fragmentation spec requires us to fit at least 1px of content in each
  // fragmentainer. See the utility function FragmentainerCapacity() for more
  // details.
  LayoutUnit FragmentainerBlockSize() const {
    return HasRareData() ? rare_data_->fragmentainer_block_size
                         : kIndefiniteSize;
  }

  // Return true if we're column-balancing, and are in the initial pass where
  // we're calculating the initial minimal column block-size.
  bool IsInitialColumnBalancingPass() const {
    return BlockFragmentationType() == kFragmentColumn &&
           FragmentainerBlockSize() == kIndefiniteSize;
  }

  // Return true if we're block-fragmented and know our fragmentainer
  // block-size.
  bool HasKnownFragmentainerBlockSize() const {
    if (!HasBlockFragmentation() || IsInitialColumnBalancingPass())
      return false;
    // The only case where we allow an unknown fragmentainer block-size is if
    // we're in the initial column balancing pass.
    DCHECK(FragmentainerBlockSize() != kIndefiniteSize);
    return true;
  }

  // Return the border edge block-offset from the block-start of the
  // fragmentainer relative to the block-start of the current block in the
  // current fragmentainer. Note that if the current block starts in a previous
  // fragmentainer, we'll return the block-offset relative to the current
  // fragmentainer.
  LayoutUnit FragmentainerOffset() const {
    DCHECK(HasBlockFragmentation());
    if (HasRareData())
      return rare_data_->fragmentainer_offset;
    return LayoutUnit();
  }

  // Return true if we're at the start of the fragmentainer. In most cases this
  // will be equal to "FragmentainerOffset() <= LayoutUnit()", but not
  // necessarily for floats, since float margins are unbreakable. If a node is
  // at the start of the fragmentainer, and the node has an untruncated positive
  // block-start margin, FragmentainerOffset() will be greater than zero. This
  // normally means that the node *isn't* at the start of the fragmentainer, but
  // for floats, this should still be considered to be at the start.
  bool IsAtFragmentainerStart() const {
    return HasRareData() && rare_data_->is_at_fragmentainer_start;
  }

  // Return true if the content will be repeated in the next fragmentainer.
  // This is the case when an element is fixed positioned (printing only), or a
  // repeatable table header / footer. Will return false even for repeatable
  // content, if we can tell for sure that this is the last time that the node
  // will repeat.
  bool ShouldRepeat() const {
    return HasRareData() && rare_data_->should_repeat;
  }

  // Return true if we're inside repeatable content inside block fragmentation,
  // which is the case when an element is fixed positioned (printing only), or a
  // repeatable table header / footer.
  bool IsInsideRepeatableContent() const {
    return HasRareData() && rare_data_->is_inside_repeatable_content;
  }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const {
    return bitfields_.is_new_formatting_context;
  }

  // Whether the current node is a table-cell.
  bool IsTableCell() const { return bitfields_.is_table_cell; }

  // Whether the table-cell fragment should be hidden (not painted) if it has
  // no children.
  bool HideTableCellIfEmpty() const {
    return HasRareData() && rare_data_->hide_table_cell_if_empty;
  }

  // Whether the fragment produced from layout should be anonymous, (e.g. it
  // may be a column in a multi-column layout). In such cases it shouldn't have
  // any borders or padding.
  bool IsAnonymous() const { return bitfields_.is_anonymous; }

  // Whether to use the ':first-line' style or not.
  // Note, this is not about the first line of the content to layout, but
  // whether the constraint space itself is on the first line, such as when it's
  // an inline block.
  // Also note this is true only when the document has ':first-line' rules.
  bool UseFirstLineStyle() const { return bitfields_.use_first_line_style; }

  // Returns true if an ancestor had clearance past adjoining floats.
  //
  // Typically this can be detected by seeing if a |ForcedBfcBlockOffset| is
  // set. However new formatting contexts may require additional passes (if
  // margins are adjoining or not), and without this extra bit of information
  // can get into a bad state.
  bool AncestorHasClearancePastAdjoiningFloats() const {
    return bitfields_.ancestor_has_clearance_past_adjoining_floats;
  }

  // How the baseline for the fragment should be calculated, see documentation
  // for |NGBaselineAlgorithmType|.
  NGBaselineAlgorithmType BaselineAlgorithmType() const {
    return static_cast<NGBaselineAlgorithmType>(
        bitfields_.baseline_algorithm_type);
  }

  // Which cache slot the output layout result should be stored in.
  NGCacheSlot CacheSlot() const {
    return static_cast<NGCacheSlot>(bitfields_.cache_slot);
  }

  // Some layout modes “stretch” their children to a fixed size (e.g. flex,
  // grid). These flags represented whether a layout needs to produce a
  // fragment that satisfies a fixed constraint in the inline and block
  // direction respectively.
  //
  // If these flags are true, the AvailableSize() is interpreted as the fixed
  // border-box size of this box in the respective dimension.
  bool IsFixedInlineSize() const { return bitfields_.is_fixed_inline_size; }

  bool IsFixedBlockSize() const { return bitfields_.is_fixed_block_size; }

  // The constraint space can have any of the combinations:
  // (1) !IsFixedBlockSize && !IsInitialBlockSizeIndefinite -- default
  // (2) !IsFixedBlockSize && IsInitialBlockSizeIndefinite -- Treat your height
  //     as indefinite when calculating your intrinsic block size.
  // (3) IsFixedBlockSize && !IsInitialBlockSizeIndefinite -- You must be this
  //     size and your children can resolve % block size against it.
  // (4) IsFixedBlockSize && IsInitialBlockSizeIndefinite -- You must be this
  //     size but your children *cannot* resolve % block size against it.
  //
  // The layout machinery (CalculateChildPercentageSize,
  // CalculateInitialFragmentGeometry, etc) handles all this, so individual
  // layout implementations don't need to do anything special UNLESS they let
  // specified block sizes influence the value passed to
  // SetIntrinsicBlock(intrinsic_block_size). If that happens, they need to
  // explicitly handle case 2 above.
  bool IsInitialBlockSizeIndefinite() const {
    return bitfields_.is_initial_block_size_indefinite;
  }

  // Returns the behavior of an 'auto' inline/block main-size.
  NGAutoBehavior InlineAutoBehavior() const {
    return static_cast<NGAutoBehavior>(bitfields_.inline_auto_behavior);
  }
  NGAutoBehavior BlockAutoBehavior() const {
    return static_cast<NGAutoBehavior>(bitfields_.block_auto_behavior);
  }
  bool IsInlineAutoBehaviorStretch() const {
    return InlineAutoBehavior() != NGAutoBehavior::kFitContent;
  }
  bool IsBlockAutoBehaviorStretch() const {
    return BlockAutoBehavior() != NGAutoBehavior::kFitContent;
  }

  // If this is a child of a table-cell.
  bool IsTableCellChild() const { return bitfields_.is_table_cell_child; }

  // If we should apply the restricted block-size behavior. See where this is
  // set within |NGBlockLayoutAlgorithm| for the conditions when this applies.
  bool IsRestrictedBlockSizeTableCellChild() const {
    return bitfields_.is_restricted_block_size_table_cell_child;
  }

  bool IsPaintedAtomically() const { return bitfields_.is_painted_atomically; }

  // If specified a layout should produce a Fragment which fragments at the
  // blockSize if possible.
  NGFragmentationType BlockFragmentationType() const {
    return HasRareData() ? static_cast<NGFragmentationType>(
                               rare_data_->block_direction_fragmentation_type)
                         : kFragmentNone;
  }

  // Return true if this constraint space participates in a fragmentation
  // context.
  bool HasBlockFragmentation() const {
    return BlockFragmentationType() != kFragmentNone;
  }

  // Return true if the node actually participates in block fragmentation, that
  // was disabled due to clipped overflow.
  bool IsBlockFragmentationForcedOff() const {
    return HasRareData() && rare_data_->is_block_fragmentation_forced_off;
  }

  // Return true if the document is paginated (for printing).
  bool IsPaginated() const {
    // TODO(layout-dev): This will not work correctly if establishing a nested
    // fragmentation context (e.g. multicol) when paginated.
    return BlockFragmentationType() == kFragmentPage;
  }

  // Return true if we're not allowed to break until we have placed some
  // content. This will prevent last-resort breaks when there's no container
  // separation, and we'll instead overflow the fragmentainer.
  bool RequiresContentBeforeBreaking() const {
    return HasRareData() && rare_data_->requires_content_before_breaking;
  }

  // Return true if there's an ancestor multicol container with balanced
  // columns that we might affect.
  bool IsInsideBalancedColumns() const {
    return HasRareData() && rare_data_->is_inside_balanced_columns;
  }

  // Return true if forced breaks inside should be ignored. This is needed by
  // out-of-flow positioned elements during column balancing.
  bool ShouldIgnoreForcedBreaks() const {
    return HasRareData() && rare_data_->should_ignore_forced_breaks;
  }

  // Return true if we're participating in the same block formatting context as
  // the one established by the nearest ancestor multicol container.
  bool IsInColumnBfc() const {
    return HasRareData() && rare_data_->is_in_column_bfc;
  }

  // True if there's a preceding break in the current fragmentainer (typically a
  // break in a parallel flow, or we wouldn't attempt to keep laying out).
  bool IsPastBreak() const {
    return HasRareData() && rare_data_->is_past_break;
  }

  // Return true if we would be at least our intrinsic block-size.
  //
  // During fragmentation we may have a stretch block-size (or similar) set,
  // which is determined without considering fragmentation. Without this flag
  // we may have content overflow which doesn't match web developers
  // expectations.
  // Grid (for example) will set this flag, and expand the row with this item in
  // order to accommodate the overflow.
  bool MinBlockSizeShouldEncompassIntrinsicSize() const {
    return HasRareData() &&
           rare_data_->min_block_size_should_encompass_intrinsic_size;
  }

  // Return the minimum break appeal allowed. This is used by multicol nested
  // inside another fragmentation context, if we're at a column row when there's
  // already content progress in the outer fragmentainer. The idea is that we
  // might avoid imperfect breaks, if we push content to the next column row in
  // the next outer fragmentainer (where there might be more space). In this
  // mode we'll set a high break appeal before the first child inside a resumed
  // container, so that any subsequent imperfect break will be weighed against
  // this. When a minimum is set, the code needs to guarantee that there will be
  // a column further ahead (in the next outer fragmentainer) where any break
  // appeal will be allowed (as usual), or we might get stuck in an infinite
  // loop, pushing the same content ahead of us, while creating columns with
  // nothing in them.
  NGBreakAppeal MinBreakAppeal() const {
    if (!HasRareData())
      return kBreakAppealLastResort;
    return static_cast<NGBreakAppeal>(rare_data_->min_break_appeal);
  }

  // In some cases, we may want to calculate the intial-break-before and
  // final-break-after values for a node outside of the normal fragmentation
  // pass. For example, the break values of flex/grid items in a row are
  // propagated to the row itself. Calculating the intial-break-before and
  // final-break-after for these items can be used to determine the break
  // appeal of a row before the full fragmentation layout pass is performed.
  bool ShouldPropagateChildBreakValues() const {
    return HasRareData() && rare_data_->propagate_child_break_values;
  }

  // Return true if the block size of the table-cell should be considered
  // restricted (e.g. height of the cell or its table is non-auto).
  bool IsRestrictedBlockSizeTableCell() const {
    return HasRareData() && rare_data_->is_restricted_block_size_table_cell;
  }

  // The amount of available space for block-start side annotation.
  // For the first box, this is the padding-block-start value of the container.
  // Otherwise, this comes from NGLayoutResult::BlockEndAnnotationSpace().
  // If the value is negative, it's block-end annotation overflow of the
  // previous box.
  LayoutUnit BlockStartAnnotationSpace() const {
    return HasRareData() ? rare_data_->BlockStartAnnotationSpace()
                         : LayoutUnit();
  }

  NGMarginStrut MarginStrut() const {
    return HasRareData() ? rare_data_->MarginStrut() : NGMarginStrut();
  }

  // The BfcOffset is where the MarginStrut is placed within the block
  // formatting context.
  //
  // The current layout or a descendant layout may "resolve" the BFC offset,
  // i.e. decide where the current fragment should be placed within the BFC.
  //
  // This is done by:
  //   bfc_block_offset =
  //     space.BfcOffset().block_offset + space.MarginStrut().Sum();
  //
  // The BFC offset can get "resolved" in many circumstances (including, but
  // not limited to):
  //   - block_start border or padding in the current layout.
  //   - Text content, atomic inlines, (see NGLineBreaker).
  //   - The current layout having a block_size.
  //   - Clearance before a child.
  NGBfcOffset BfcOffset() const {
    return HasRareData() ? rare_data_->bfc_offset : bfc_offset_;
  }

  // If present, and the current layout hasn't resolved its BFC block-offset
  // yet (see BfcOffset), the layout should position all of its floats at this
  // offset.
  //
  // This value is present if:
  //   - An ancestor had clearance past adjoining floats. In this case this
  //     value is calculated ahead of time.
  //   - A second layout pass is required as there were adjoining-floats
  //     within the tree, and an arbitrary sibling determined their BFC
  //     block-offset.
  //
  // This value should be propagated to child layouts if the current layout
  // hasn't resolved its BFC offset yet.
  absl::optional<LayoutUnit> ForcedBfcBlockOffset() const {
    return HasRareData() ? rare_data_->ForcedBfcBlockOffset() : absl::nullopt;
  }

  // If present, this is a hint as to where place any adjoining objects. This
  // isn't necessarily the final position, just where they ended up in a
  // previous layout pass.
  absl::optional<LayoutUnit> OptimisticBfcBlockOffset() const {
    return HasRareData() ? rare_data_->OptimisticBfcBlockOffset()
                         : absl::nullopt;
  }

  // The "expected" BFC block-offset is:
  //  - The |ForcedBfcBlockOffset| if set.
  //  - The |OptimisticBfcBlockOffset| if set.
  //  - Otherwise the |BfcOffset|.
  //
  // This represents where any adjoining-objects should be placed (potentially
  // optimistically)
  LayoutUnit ExpectedBfcBlockOffset() const {
    // A short-circuit optimization (must equivalent to below).
    if (!HasRareData()) {
      DCHECK(!ForcedBfcBlockOffset());
      DCHECK(!OptimisticBfcBlockOffset());
      return bfc_offset_.block_offset;
    }

    return ForcedBfcBlockOffset().value_or(
        OptimisticBfcBlockOffset().value_or(BfcOffset().block_offset));
  }

  SerializedScriptValue* CustomLayoutData() const {
    return HasRareData() ? rare_data_->CustomLayoutData() : nullptr;
  }

  // Returns the types of preceding adjoining objects.
  // See |NGAdjoiningObjectTypes|.
  //
  // Adjoining floats are positioned at their correct position if the
  // |ForcedBfcBlockOffset()| is known.
  //
  // Adjoining floats should be treated differently when calculating clearance
  // on a block with adjoining block-start margin (in such cases we will know
  // up front that the block will need clearance, since, if it doesn't, the
  // float will be pulled along with the block, and the block will fail to
  // clear).
  NGAdjoiningObjectTypes AdjoiningObjectTypes() const {
    return bitfields_.adjoining_object_types;
  }

  // Return true if there were any earlier floats that may affect the current
  // layout.
  bool HasFloats() const { return !ExclusionSpace().IsEmpty(); }

  bool HasClearanceOffset() const {
    return HasRareData() && rare_data_->ClearanceOffset() != LayoutUnit::Min();
  }
  LayoutUnit ClearanceOffset() const {
    return HasRareData() ? rare_data_->ClearanceOffset() : LayoutUnit::Min();
  }

  // Return true if the BFC block-offset has been increased by the presence of
  // floats (e.g. clearance).
  bool IsPushedByFloats() const {
    return HasRareData() && rare_data_->is_pushed_by_floats;
  }

  // Return true if this is participating within a -webkit-line-clamp context.
  bool IsLineClampContext() const {
    return HasRareData() && rare_data_->is_line_clamp_context;
  }

  absl::optional<int> LinesUntilClamp() const {
    return HasRareData() ? rare_data_->LinesUntilClamp() : absl::nullopt;
  }

  const NGGridLayoutSubtree* GridLayoutSubtree() const {
    return HasRareData() ? rare_data_->GridLayoutSubtree() : nullptr;
  }

  // Return true if the two constraint spaces are similar enough that it *may*
  // be possible to skip re-layout. If true is returned, the caller is expected
  // to verify that any constraint space size (available size, percentage size,
  // and so on) and BFC offset changes won't require re-layout, before skipping.
  bool MaySkipLayout(const NGConstraintSpace& other) const {
    if (!bitfields_.MaySkipLayout(other.bitfields_))
      return false;

    if (!HasRareData() && !other.HasRareData())
      return true;

    if (HasRareData() && other.HasRareData())
      return rare_data_->MaySkipLayout(*other.rare_data_);

    if (HasRareData())
      return rare_data_->IsInitialForMaySkipLayout();

    DCHECK(other.HasRareData());
    return other.rare_data_->IsInitialForMaySkipLayout();
  }

  // Returns true if the size constraints (stretch-block-size,
  // fixed-inline-size) are equal.
  bool AreInlineSizeConstraintsEqual(const NGConstraintSpace& other) const {
    return bitfields_.AreInlineSizeConstraintsEqual(other.bitfields_);
  }
  bool AreBlockSizeConstraintsEqual(const NGConstraintSpace& other) const {
    if (!bitfields_.AreBlockSizeConstraintsEqual(other.bitfields_))
      return false;
    if (!HasRareData() && !other.HasRareData())
      return true;
    return TableCellAlignmentBaseline() == other.TableCellAlignmentBaseline() &&
           MinBlockSizeShouldEncompassIntrinsicSize() ==
               other.MinBlockSizeShouldEncompassIntrinsicSize();
  }

  bool AreSizesEqual(const NGConstraintSpace& other) const {
    if (available_size_ != other.available_size_)
      return false;

    if (bitfields_.percentage_inline_storage !=
        other.bitfields_.percentage_inline_storage)
      return false;

    if (bitfields_.percentage_block_storage !=
        other.bitfields_.percentage_block_storage)
      return false;

    if (bitfields_.replaced_percentage_block_storage !=
        other.bitfields_.replaced_percentage_block_storage)
      return false;

    // The rest of this method just checks the percentage resolution sizes. If
    // neither space has rare data, we know that they must equal now.
    if (!HasRareData() && !other.HasRareData())
      return true;

    if (bitfields_.percentage_inline_storage == kRareDataPercentage &&
        other.bitfields_.percentage_inline_storage == kRareDataPercentage &&
        rare_data_->percentage_resolution_size.inline_size !=
            other.rare_data_->percentage_resolution_size.inline_size)
      return false;

    if (bitfields_.percentage_block_storage == kRareDataPercentage &&
        other.bitfields_.percentage_block_storage == kRareDataPercentage &&
        rare_data_->percentage_resolution_size.block_size !=
            other.rare_data_->percentage_resolution_size.block_size)
      return false;

    if (bitfields_.replaced_percentage_block_storage == kRareDataPercentage &&
        other.bitfields_.replaced_percentage_block_storage ==
            kRareDataPercentage &&
        rare_data_->replaced_percentage_resolution_block_size !=
            other.rare_data_->replaced_percentage_resolution_block_size)
      return false;

    return true;
  }

  void ReplaceTableRowData(const NGTableConstraintSpaceData& table_data,
                           const wtf_size_t row_index) {
    DCHECK(HasRareData());
    rare_data_->ReplaceTableRowData(table_data, row_index);
  }

  String ToString() const;

 private:
  friend class NGConstraintSpaceBuilder;

  // This struct defines all of the inputs to layout which we consider rare.
  // Primarily this is:
  //  - Percentage resolution sizes which differ from the available size or
  //    aren't indefinite.
  //  - The margin strut.
  //  - Anything to do with floats (the exclusion space, clearance offset, etc).
  //  - Anything to do with fragmentation.
  //  - Anything to do with stretching of math operators.
  //
  // This information is kept in a separate in this heap-allocated struct to
  // reduce memory usage. Over time this may have to change based on usage data.
  struct RareData {
    USING_FAST_MALLOC(RareData);

   public:
    // |RareData| unions different types of data which are mutually exclusive.
    // They fall into the following categories:
    enum class DataUnionType {
      kNone,
      kBlockData,         // An inflow block which doesn't establish a new FC.
      kTableCellData,     // A table-cell (display: table-cell).
      kTableRowData,      // A table-row (display: table-row).
      kTableSectionData,  // A table-section (display: table-section).
      kCustomData,        // A custom layout (display: layout(foo)).
      kStretchData,       // The target inline/block stretch sizes for MathML.
      kSubgridData        // A nested grid with subgridded columns/rows.
    };

    explicit RareData(const NGBfcOffset bfc_offset)
        : bfc_offset(bfc_offset),
          data_union_type(static_cast<unsigned>(DataUnionType::kNone)),
          is_line_clamp_context(false),
          is_pushed_by_floats(false),
          is_restricted_block_size_table_cell(false),
          hide_table_cell_if_empty(false),
          block_direction_fragmentation_type(
              static_cast<unsigned>(kFragmentNone)),
          is_block_fragmentation_forced_off(false),
          requires_content_before_breaking(false),
          is_inside_balanced_columns(false),
          should_ignore_forced_breaks(false),
          is_in_column_bfc(false),
          is_past_break(false),
          min_block_size_should_encompass_intrinsic_size(false),
          has_override_min_max_block_sizes(false),
          min_break_appeal(kBreakAppealLastResort),
          propagate_child_break_values(false),
          is_at_fragmentainer_start(false),
          should_repeat(false),
          is_inside_repeatable_content(false) {}
    RareData(const RareData& other)
        : percentage_resolution_size(other.percentage_resolution_size),
          replaced_percentage_resolution_block_size(
              other.replaced_percentage_resolution_block_size),
          block_start_annotation_space(other.block_start_annotation_space),
          bfc_offset(other.bfc_offset),
          override_min_max_block_sizes(other.override_min_max_block_sizes),
          page_name(other.page_name),
          fragmentainer_block_size(other.fragmentainer_block_size),
          fragmentainer_offset(other.fragmentainer_offset),
          data_union_type(other.data_union_type),
          is_line_clamp_context(other.is_line_clamp_context),
          is_pushed_by_floats(other.is_pushed_by_floats),
          is_restricted_block_size_table_cell(
              other.is_restricted_block_size_table_cell),
          hide_table_cell_if_empty(other.hide_table_cell_if_empty),
          block_direction_fragmentation_type(
              other.block_direction_fragmentation_type),
          is_block_fragmentation_forced_off(
              other.is_block_fragmentation_forced_off),
          requires_content_before_breaking(
              other.requires_content_before_breaking),
          is_inside_balanced_columns(other.is_inside_balanced_columns),
          should_ignore_forced_breaks(other.should_ignore_forced_breaks),
          is_in_column_bfc(other.is_in_column_bfc),
          is_past_break(other.is_past_break),
          min_block_size_should_encompass_intrinsic_size(
              other.min_block_size_should_encompass_intrinsic_size),
          has_override_min_max_block_sizes(
              other.has_override_min_max_block_sizes),
          min_break_appeal(other.min_break_appeal),
          propagate_child_break_values(other.propagate_child_break_values),
          is_at_fragmentainer_start(other.is_at_fragmentainer_start),
          should_repeat(other.should_repeat),
          is_inside_repeatable_content(other.is_inside_repeatable_content) {
      switch (GetDataUnionType()) {
        case DataUnionType::kNone:
          break;
        case DataUnionType::kBlockData:
          new (&block_data_) BlockData(other.block_data_);
          break;
        case DataUnionType::kTableCellData:
          new (&table_cell_data_) TableCellData(other.table_cell_data_);
          break;
        case DataUnionType::kTableRowData:
          new (&table_row_data_) TableRowData(other.table_row_data_);
          break;
        case DataUnionType::kTableSectionData:
          new (&table_section_data_)
              TableSectionData(other.table_section_data_);
          break;
        case DataUnionType::kCustomData:
          new (&custom_data_) CustomData(other.custom_data_);
          break;
        case DataUnionType::kStretchData:
          new (&stretch_data_) StretchData(other.stretch_data_);
          break;
        case DataUnionType::kSubgridData:
          new (&subgrid_data_) SubgridData(other.subgrid_data_);
          break;
        default:
          NOTREACHED();
      }
    }
    ~RareData() {
      switch (GetDataUnionType()) {
        case DataUnionType::kNone:
          break;
        case DataUnionType::kBlockData:
          block_data_.~BlockData();
          break;
        case DataUnionType::kTableCellData:
          table_cell_data_.~TableCellData();
          break;
        case DataUnionType::kTableRowData:
          table_row_data_.~TableRowData();
          break;
        case DataUnionType::kTableSectionData:
          table_section_data_.~TableSectionData();
          break;
        case DataUnionType::kCustomData:
          custom_data_.~CustomData();
          break;
        case DataUnionType::kStretchData:
          stretch_data_.~StretchData();
          break;
        case DataUnionType::kSubgridData:
          subgrid_data_.~SubgridData();
          break;
        default:
          NOTREACHED();
      }
    }

    bool MaySkipLayout(const RareData& other) const {
      if (data_union_type != other.data_union_type ||
          is_line_clamp_context != other.is_line_clamp_context ||
          is_pushed_by_floats != other.is_pushed_by_floats ||
          is_restricted_block_size_table_cell !=
              other.is_restricted_block_size_table_cell ||
          hide_table_cell_if_empty != other.hide_table_cell_if_empty ||
          block_direction_fragmentation_type !=
              other.block_direction_fragmentation_type ||
          is_block_fragmentation_forced_off !=
              other.is_block_fragmentation_forced_off ||
          requires_content_before_breaking !=
              other.requires_content_before_breaking ||
          is_inside_balanced_columns != other.is_inside_balanced_columns ||
          should_ignore_forced_breaks != other.should_ignore_forced_breaks ||
          is_in_column_bfc != other.is_in_column_bfc ||
          is_past_break != other.is_past_break ||
          min_break_appeal != other.min_break_appeal ||
          propagate_child_break_values != other.propagate_child_break_values ||
          should_repeat != other.should_repeat ||
          is_inside_repeatable_content != other.is_inside_repeatable_content)
        return false;

      switch (GetDataUnionType()) {
        case DataUnionType::kNone:
          return true;
        case DataUnionType::kBlockData:
          return block_data_.MaySkipLayout(other.block_data_);
        case DataUnionType::kTableCellData:
          return table_cell_data_.MaySkipLayout(other.table_cell_data_);
        case DataUnionType::kTableRowData:
          return table_row_data_.MaySkipLayout(other.table_row_data_);
        case DataUnionType::kTableSectionData:
          return table_section_data_.MaySkipLayout(other.table_section_data_);
        case DataUnionType::kCustomData:
          return custom_data_.MaySkipLayout(other.custom_data_);
        case DataUnionType::kStretchData:
          return stretch_data_.MaySkipLayout(other.stretch_data_);
        case DataUnionType::kSubgridData:
          return subgrid_data_.MaySkipLayout(other.subgrid_data_);
      }
      NOTREACHED();
      return false;
    }

    // Must be kept in sync with members checked within |MaySkipLayout|.
    bool IsInitialForMaySkipLayout() const {
      if (page_name || fragmentainer_block_size != kIndefiniteSize ||
          fragmentainer_offset || is_line_clamp_context ||
          is_pushed_by_floats || is_restricted_block_size_table_cell ||
          hide_table_cell_if_empty ||
          block_direction_fragmentation_type != kFragmentNone ||
          is_block_fragmentation_forced_off ||
          requires_content_before_breaking || is_inside_balanced_columns ||
          should_ignore_forced_breaks || is_in_column_bfc || is_past_break ||
          min_break_appeal != kBreakAppealLastResort ||
          propagate_child_break_values || is_at_fragmentainer_start ||
          should_repeat || is_inside_repeatable_content)
        return false;

      switch (GetDataUnionType()) {
        case DataUnionType::kNone:
          return true;
        case DataUnionType::kBlockData:
          return block_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kTableCellData:
          return table_cell_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kTableRowData:
          return table_row_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kTableSectionData:
          return table_section_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kCustomData:
          return custom_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kStretchData:
          return stretch_data_.IsInitialForMaySkipLayout();
        case DataUnionType::kSubgridData:
          return subgrid_data_.IsInitialForMaySkipLayout();
      }
      NOTREACHED();
      return false;
    }

    LayoutUnit BlockStartAnnotationSpace() const {
      return block_start_annotation_space;
    }

    void SetBlockStartAnnotationSpace(LayoutUnit space) {
      block_start_annotation_space = space;
    }

    NGMarginStrut MarginStrut() const {
      return GetDataUnionType() == DataUnionType::kBlockData
                 ? block_data_.margin_strut
                 : NGMarginStrut();
    }

    void SetMarginStrut(const NGMarginStrut& margin_strut) {
      EnsureBlockData()->margin_strut = margin_strut;
    }

    absl::optional<LayoutUnit> OptimisticBfcBlockOffset() const {
      return GetDataUnionType() == DataUnionType::kBlockData
                 ? block_data_.optimistic_bfc_block_offset
                 : absl::nullopt;
    }

    void SetOptimisticBfcBlockOffset(LayoutUnit optimistic_bfc_block_offset) {
      EnsureBlockData()->optimistic_bfc_block_offset =
          optimistic_bfc_block_offset;
    }

    absl::optional<LayoutUnit> ForcedBfcBlockOffset() const {
      return GetDataUnionType() == DataUnionType::kBlockData
                 ? block_data_.forced_bfc_block_offset
                 : absl::nullopt;
    }

    void SetForcedBfcBlockOffset(LayoutUnit forced_bfc_block_offset) {
      EnsureBlockData()->forced_bfc_block_offset = forced_bfc_block_offset;
    }

    LayoutUnit ClearanceOffset() const {
      return GetDataUnionType() == DataUnionType::kBlockData
                 ? block_data_.clearance_offset
                 : LayoutUnit::Min();
    }

    absl::optional<MinMaxSizes> OverrideMinMaxBlockSizes() const {
      if (has_override_min_max_block_sizes)
        return override_min_max_block_sizes;
      return absl::nullopt;
    }

    void SetOverrideMinMaxBlockSizes(const MinMaxSizes& min_max_sizes) {
      if (min_max_sizes.IsEmpty()) {
        has_override_min_max_block_sizes = false;
        return;
      }
      DCHECK_GE(min_max_sizes.max_size, min_max_sizes.min_size);
      has_override_min_max_block_sizes = true;
      override_min_max_block_sizes = min_max_sizes;
    }

    void SetClearanceOffset(LayoutUnit clearance_offset) {
      EnsureBlockData()->clearance_offset = clearance_offset;
    }

    absl::optional<int> LinesUntilClamp() const {
      return GetDataUnionType() == DataUnionType::kBlockData
                 ? block_data_.lines_until_clamp
                 : absl::nullopt;
    }

    void SetLinesUntilClamp(int value) {
      EnsureBlockData()->lines_until_clamp = value;
    }

    NGBoxStrut TableCellBorders() const {
      return GetDataUnionType() == DataUnionType::kTableCellData
                 ? table_cell_data_.table_cell_borders
                 : NGBoxStrut();
    }

    void SetTableCellBorders(const NGBoxStrut& table_cell_borders) {
      EnsureTableCellData()->table_cell_borders = table_cell_borders;
    }

    wtf_size_t TableCellColumnIndex() const {
      return GetDataUnionType() == DataUnionType::kTableCellData
                 ? table_cell_data_.table_cell_column_index
                 : 0;
    }

    void SetTableCellColumnIndex(wtf_size_t table_cell_column_index) {
      EnsureTableCellData()->table_cell_column_index = table_cell_column_index;
    }

    absl::optional<LayoutUnit> TableCellAlignmentBaseline() const {
      return GetDataUnionType() == DataUnionType::kTableCellData
                 ? table_cell_data_.table_cell_alignment_baseline
                 : absl::nullopt;
    }

    void SetTableCellAlignmentBaseline(
        LayoutUnit table_cell_alignment_baseline) {
      EnsureTableCellData()->table_cell_alignment_baseline =
          table_cell_alignment_baseline;
    }

    bool IsTableCellHiddenForPaint() const {
      return GetDataUnionType() == DataUnionType::kTableCellData &&
             table_cell_data_.is_hidden_for_paint;
    }

    void SetIsTableCellHiddenForPaint(bool is_hidden_for_paint) {
      EnsureTableCellData()->is_hidden_for_paint = is_hidden_for_paint;
    }

    bool IsTableCellWithCollapsedBorders() const {
      return GetDataUnionType() == DataUnionType::kTableCellData &&
             table_cell_data_.has_collapsed_borders;
    }

    void SetIsTableCellWithCollapsedBorders(bool has_collapsed_borders) {
      EnsureTableCellData()->has_collapsed_borders = has_collapsed_borders;
    }

    void SetTableRowData(
        scoped_refptr<const NGTableConstraintSpaceData> table_data,
        wtf_size_t row_index) {
      EnsureTableRowData()->table_data = std::move(table_data);
      EnsureTableRowData()->row_index = row_index;
    }

    void SetTableSectionData(
        scoped_refptr<const NGTableConstraintSpaceData> table_data,
        wtf_size_t section_index) {
      EnsureTableSectionData()->table_data = std::move(table_data);
      EnsureTableSectionData()->section_index = section_index;
    }

    void ReplaceTableRowData(const NGTableConstraintSpaceData& table_data,
                             wtf_size_t row_index) {
      DCHECK_EQ(GetDataUnionType(), DataUnionType::kTableRowData);
      DCHECK(
          table_data.IsTableSpecificDataEqual(*(table_row_data_.table_data)));
      DCHECK(table_data.MaySkipRowLayout(*table_row_data_.table_data, row_index,
                                         table_row_data_.row_index));
      table_row_data_.table_data = &table_data;
      table_row_data_.row_index = row_index;
    }

    const NGTableConstraintSpaceData* TableData() {
      if (GetDataUnionType() == DataUnionType::kTableRowData)
        return table_row_data_.table_data.get();
      if (GetDataUnionType() == DataUnionType::kTableSectionData)
        return table_section_data_.table_data.get();
      return nullptr;
    }

    wtf_size_t TableRowIndex() const {
      return GetDataUnionType() == DataUnionType::kTableRowData
                 ? table_row_data_.row_index
                 : kNotFound;
    }

    wtf_size_t TableSectionIndex() const {
      return GetDataUnionType() == DataUnionType::kTableSectionData
                 ? table_section_data_.section_index
                 : kNotFound;
    }

    SerializedScriptValue* CustomLayoutData() const {
      return GetDataUnionType() == DataUnionType::kCustomData
                 ? custom_data_.data.get()
                 : nullptr;
    }

    void SetCustomLayoutData(
        scoped_refptr<SerializedScriptValue> custom_layout_data) {
      EnsureCustomData()->data = std::move(custom_layout_data);
    }

    LayoutUnit TargetStretchInlineSize() const {
      return GetDataUnionType() == DataUnionType::kStretchData
                 ? stretch_data_.target_stretch_inline_size
                 : kIndefiniteSize;
    }

    void SetTargetStretchInlineSize(LayoutUnit target_stretch_inline_size) {
      EnsureStretchData()->target_stretch_inline_size =
          target_stretch_inline_size;
    }

    absl::optional<MathTargetStretchBlockSizes> TargetStretchBlockSizes()
        const {
      return GetDataUnionType() == DataUnionType::kStretchData
                 ? stretch_data_.target_stretch_block_sizes
                 : absl::nullopt;
    }

    void SetTargetStretchBlockSizes(
        MathTargetStretchBlockSizes target_stretch_block_sizes) {
      EnsureStretchData()->target_stretch_block_sizes =
          target_stretch_block_sizes;
    }

    const NGGridLayoutSubtree* GridLayoutSubtree() const {
      return GetDataUnionType() == DataUnionType::kSubgridData
                 ? &subgrid_data_.layout_subtree
                 : nullptr;
    }

    void SetGridLayoutSubtree(NGGridLayoutSubtree&& grid_layout_subtree) {
      EnsureSubgridData()->layout_subtree = std::move(grid_layout_subtree);
    }

    DataUnionType GetDataUnionType() const {
      return static_cast<DataUnionType>(data_union_type);
    }

    LogicalSize percentage_resolution_size;
    LayoutUnit replaced_percentage_resolution_block_size;
    LayoutUnit block_start_annotation_space;
    NGBfcOffset bfc_offset;
    MinMaxSizes override_min_max_block_sizes;

    AtomicString page_name;
    LayoutUnit fragmentainer_block_size = kIndefiniteSize;
    LayoutUnit fragmentainer_offset;

    unsigned data_union_type : 3;

    unsigned is_line_clamp_context : 1;
    unsigned is_pushed_by_floats : 1;

    unsigned is_restricted_block_size_table_cell : 1;
    unsigned hide_table_cell_if_empty : 1;

    unsigned block_direction_fragmentation_type : 2;
    unsigned is_block_fragmentation_forced_off : 1;
    unsigned requires_content_before_breaking : 1;
    unsigned is_inside_balanced_columns : 1;
    unsigned should_ignore_forced_breaks : 1;
    unsigned is_in_column_bfc : 1;
    unsigned is_past_break : 1;
    unsigned min_block_size_should_encompass_intrinsic_size : 1;
    unsigned has_override_min_max_block_sizes : 1;
    unsigned min_break_appeal : kNGBreakAppealBitsNeeded;
    unsigned propagate_child_break_values : 1;
    unsigned is_at_fragmentainer_start : 1;
    unsigned should_repeat : 1;
    unsigned is_inside_repeatable_content : 1;

   private:
    struct BlockData {
      bool MaySkipLayout(const BlockData& other) const {
        return lines_until_clamp == other.lines_until_clamp;
      }

      bool IsInitialForMaySkipLayout() const {
        return !lines_until_clamp.has_value();
      }

      NGMarginStrut margin_strut;
      absl::optional<LayoutUnit> optimistic_bfc_block_offset;
      absl::optional<LayoutUnit> forced_bfc_block_offset;
      LayoutUnit clearance_offset = LayoutUnit::Min();
      absl::optional<int> lines_until_clamp;
    };

    struct TableCellData {
      bool MaySkipLayout(const TableCellData& other) const {
        // NOTE: We don't compare |table_cell_alignment_baseline| as it is
        // still possible to hit the cache if this differs.
        return table_cell_borders == other.table_cell_borders &&
               table_cell_column_index == other.table_cell_column_index &&
               is_hidden_for_paint == other.is_hidden_for_paint &&
               has_collapsed_borders == other.has_collapsed_borders;
      }

      bool IsInitialForMaySkipLayout() const {
        return table_cell_borders == NGBoxStrut() &&
               table_cell_column_index == kNotFound && !is_hidden_for_paint &&
               !has_collapsed_borders;
      }

      NGBoxStrut table_cell_borders;
      wtf_size_t table_cell_column_index = kNotFound;
      absl::optional<LayoutUnit> table_cell_alignment_baseline;
      bool is_hidden_for_paint = false;
      bool has_collapsed_borders = false;
    };

    struct TableRowData {
      bool MaySkipLayout(const TableRowData& other) const {
        return table_data->IsTableSpecificDataEqual(*other.table_data) &&
               table_data->MaySkipRowLayout(*other.table_data, row_index,
                                            other.row_index);
      }
      bool IsInitialForMaySkipLayout() const {
        return !table_data && row_index == kNotFound;
      }

      scoped_refptr<const NGTableConstraintSpaceData> table_data;
      wtf_size_t row_index = kNotFound;
    };

    struct TableSectionData {
      bool MaySkipLayout(const TableSectionData& other) const {
        return table_data->IsTableSpecificDataEqual(*other.table_data) &&
               table_data->MaySkipSectionLayout(
                   *other.table_data, section_index, other.section_index);
      }
      bool IsInitialForMaySkipLayout() const {
        return !table_data && section_index == kNotFound;
      }

      scoped_refptr<const NGTableConstraintSpaceData> table_data;
      wtf_size_t section_index = kNotFound;
    };

    struct CustomData {
      scoped_refptr<SerializedScriptValue> data;

      bool MaySkipLayout(const CustomData& other) const {
        return data == other.data;
      }

      bool IsInitialForMaySkipLayout() const { return !data; }
    };

    struct StretchData {
      bool MaySkipLayout(const StretchData& other) const {
        return target_stretch_inline_size == other.target_stretch_inline_size &&
               target_stretch_block_sizes.has_value() ==
                   other.target_stretch_block_sizes.has_value() &&
               (!target_stretch_block_sizes ||
                (target_stretch_block_sizes->ascent ==
                     other.target_stretch_block_sizes->ascent &&
                 target_stretch_block_sizes->descent ==
                     other.target_stretch_block_sizes->descent));
      }

      bool IsInitialForMaySkipLayout() const {
        return target_stretch_inline_size == kIndefiniteSize &&
               !target_stretch_block_sizes;
      }

      LayoutUnit target_stretch_inline_size = kIndefiniteSize;
      absl::optional<MathTargetStretchBlockSizes> target_stretch_block_sizes;
    };

    struct SubgridData {
      bool MaySkipLayout(const SubgridData& other) const {
        return layout_subtree == other.layout_subtree;
      }

      bool IsInitialForMaySkipLayout() const { return !layout_subtree; }

      NGGridLayoutSubtree layout_subtree;
    };

    BlockData* EnsureBlockData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kBlockData);
      if (GetDataUnionType() != DataUnionType::kBlockData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kBlockData);
        new (&block_data_) BlockData();
      }
      return &block_data_;
    }

    TableCellData* EnsureTableCellData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kTableCellData);
      if (GetDataUnionType() != DataUnionType::kTableCellData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kTableCellData);
        new (&table_cell_data_) TableCellData();
      }
      return &table_cell_data_;
    }

    TableRowData* EnsureTableRowData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kTableRowData);
      if (GetDataUnionType() != DataUnionType::kTableRowData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kTableRowData);
        new (&table_row_data_) TableRowData();
      }
      return &table_row_data_;
    }

    TableSectionData* EnsureTableSectionData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kTableSectionData);
      if (GetDataUnionType() != DataUnionType::kTableSectionData) {
        data_union_type =
            static_cast<unsigned>(DataUnionType::kTableSectionData);
        new (&table_section_data_) TableSectionData();
      }
      return &table_section_data_;
    }

    CustomData* EnsureCustomData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kCustomData);
      if (GetDataUnionType() != DataUnionType::kCustomData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kCustomData);
        new (&custom_data_) CustomData();
      }
      return &custom_data_;
    }

    StretchData* EnsureStretchData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kStretchData);
      if (GetDataUnionType() != DataUnionType::kStretchData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kStretchData);
        new (&stretch_data_) StretchData();
      }
      return &stretch_data_;
    }

    SubgridData* EnsureSubgridData() {
      DCHECK(GetDataUnionType() == DataUnionType::kNone ||
             GetDataUnionType() == DataUnionType::kSubgridData);
      if (GetDataUnionType() != DataUnionType::kSubgridData) {
        data_union_type = static_cast<unsigned>(DataUnionType::kSubgridData);
        new (&subgrid_data_) SubgridData();
      }
      return &subgrid_data_;
    }

    union {
      BlockData block_data_;
      TableCellData table_cell_data_;
      TableRowData table_row_data_;
      TableSectionData table_section_data_;
      CustomData custom_data_;
      StretchData stretch_data_;
      SubgridData subgrid_data_;
    };
  };

  // This struct simply allows us easily copy, compare, and initialize all the
  // bitfields without having to explicitly copy, compare, and initialize each
  // one (see the outer class constructors, and assignment operators).
  struct Bitfields {
    DISALLOW_NEW();

   public:
    Bitfields()
        : Bitfields({WritingMode::kHorizontalTb, TextDirection::kLtr}) {}

    explicit Bitfields(WritingDirectionMode writing_direction)
        : has_rare_data(false),
          adjoining_object_types(static_cast<unsigned>(kAdjoiningNone)),
          writing_mode(
              static_cast<unsigned>(writing_direction.GetWritingMode())),
          direction(static_cast<unsigned>(writing_direction.Direction())),
          is_table_cell(false),
          is_anonymous(false),
          is_new_formatting_context(false),
          is_orthogonal_writing_mode_root(false),
          is_painted_atomically(false),
          use_first_line_style(false),
          ancestor_has_clearance_past_adjoining_floats(false),
          baseline_algorithm_type(
              static_cast<unsigned>(NGBaselineAlgorithmType::kDefault)),
          cache_slot(static_cast<unsigned>(NGCacheSlot::kLayout)),
          inline_auto_behavior(
              static_cast<unsigned>(NGAutoBehavior::kFitContent)),
          block_auto_behavior(
              static_cast<unsigned>(NGAutoBehavior::kFitContent)),
          is_fixed_inline_size(false),
          is_fixed_block_size(false),
          is_initial_block_size_indefinite(false),
          is_table_cell_child(false),
          is_restricted_block_size_table_cell_child(false),
          percentage_inline_storage(kSameAsAvailable),
          percentage_block_storage(kSameAsAvailable),
          replaced_percentage_block_storage(kSameAsAvailable) {}

    bool MaySkipLayout(const Bitfields& other) const {
      return adjoining_object_types == other.adjoining_object_types &&
             writing_mode == other.writing_mode &&
             direction == other.direction &&
             is_table_cell == other.is_table_cell &&
             is_anonymous == other.is_anonymous &&
             is_new_formatting_context == other.is_new_formatting_context &&
             is_orthogonal_writing_mode_root ==
                 other.is_orthogonal_writing_mode_root &&
             is_painted_atomically == other.is_painted_atomically &&
             use_first_line_style == other.use_first_line_style &&
             ancestor_has_clearance_past_adjoining_floats ==
                 other.ancestor_has_clearance_past_adjoining_floats &&
             baseline_algorithm_type == other.baseline_algorithm_type;
    }

    bool AreInlineSizeConstraintsEqual(const Bitfields& other) const {
      return inline_auto_behavior == other.inline_auto_behavior &&
             is_fixed_inline_size == other.is_fixed_inline_size;
    }
    bool AreBlockSizeConstraintsEqual(const Bitfields& other) const {
      return block_auto_behavior == other.block_auto_behavior &&
             is_fixed_block_size == other.is_fixed_block_size &&
             is_initial_block_size_indefinite ==
                 other.is_initial_block_size_indefinite &&
             is_table_cell_child == other.is_table_cell_child &&
             is_restricted_block_size_table_cell_child ==
                 other.is_restricted_block_size_table_cell_child;
    }

    unsigned has_rare_data : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes
    unsigned writing_mode : 3;
    unsigned direction : 1;

    unsigned is_table_cell : 1;

    unsigned is_anonymous : 1;
    unsigned is_new_formatting_context : 1;
    unsigned is_orthogonal_writing_mode_root : 1;

    unsigned is_painted_atomically : 1;
    unsigned use_first_line_style : 1;
    unsigned ancestor_has_clearance_past_adjoining_floats : 1;

    unsigned baseline_algorithm_type : 1;

    unsigned cache_slot : 1;

    // Size constraints.
    unsigned inline_auto_behavior : 2;  // NGAutoBehavior
    unsigned block_auto_behavior : 2;   // NGAutoBehavior
    unsigned is_fixed_inline_size : 1;
    unsigned is_fixed_block_size : 1;
    unsigned is_initial_block_size_indefinite : 1;
    unsigned is_table_cell_child : 1;
    unsigned is_restricted_block_size_table_cell_child : 1;

    unsigned percentage_inline_storage : 2;           // NGPercentageStorage
    unsigned percentage_block_storage : 2;            // NGPercentageStorage
    unsigned replaced_percentage_block_storage : 2;   // NGPercentageStorage
  };

  // To ensure that the bfc_offset_, rare_data_ union doesn't get polluted,
  // always initialize the bfc_offset_.
  explicit NGConstraintSpace(WritingDirectionMode writing_direction)
      : available_size_(kIndefiniteSize, kIndefiniteSize),
        bfc_offset_(),
        bitfields_(writing_direction) {}

  inline bool HasRareData() const { return bitfields_.has_rare_data; }

  RareData* EnsureRareData() {
    if (!HasRareData()) {
      rare_data_ = new RareData(bfc_offset_);
      bitfields_.has_rare_data = true;
    }

    return rare_data_;
  }

  void DisableFurtherFragmentation() {
    if (!HasBlockFragmentation()) {
      return;
    }
    DCHECK(rare_data_);
    rare_data_->block_direction_fragmentation_type = kFragmentNone;
    rare_data_->is_block_fragmentation_forced_off = true;
  }

  LogicalSize available_size_;

  // To save a little space, we union these two fields. rare_data_ is valid if
  // the |has_rare_data| bit is set, otherwise bfc_offset_ is valid.
  union {
    NGBfcOffset bfc_offset_;
    RareData* rare_data_;
  };

  NGExclusionSpace exclusion_space_;
  Bitfields bitfields_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const NGConstraintSpace& value) {
  return stream << value.ToString();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_H_
