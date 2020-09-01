// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_CONSTRAINT_SPACE_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_exclusion_space.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_offset.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_margin_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_constraint_space_data.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
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
// block-offset is resvoled. E.g. their position depends on the final BFC
// block-offset being known.
enum NGAdjoiningObjectTypeValue {
  kAdjoiningNone = 0b000,
  kAdjoiningFloatLeft = 0b001,
  kAdjoiningFloatRight = 0b010,
  kAdjoiningFloatBoth = 0b011,
  kAdjoiningInlineOutOfFlow = 0b100
};
typedef int NGAdjoiningObjectTypes;

// Tables have two passes, a "measure" pass (for determining the table row
// height), and a "layout" pass.
// See: https://drafts.csswg.org/css-tables-3/#row-layout
//
// This enum is used for communicating to *direct* children of table cells,
// which layout mode the table cell is in.
enum class NGTableCellChildLayoutMode {
  kNotTableCellChild,  // The node isn't a table cell child.
  kMeasure,            // A table cell child, in the "measure" mode.
  kMeasureRestricted,  // A table cell child, in the "restricted-measure" mode.
  kLayout              // A table cell child, in the "layout" mode.
};

// Percentages are frequently the same as the available-size, zero, or
// indefinite (thanks non-quirks mode)! This enum encodes this information.
enum NGPercentageStorage {
  kSameAsAvailable,
  kZero,
  kIndefinite,
  kRareDataPercentage
};

// Some layout algorithms (flow, tables) calculate their alignment baseline
// differently if they are within an atomic-inline context.
//
// Other more modern layout algorithms (flex, grid) however ignore this flag
// and always calculate the alignment baseline in the same way (returning the
// "first-line").
enum class NGBaselineAlgorithmType {
  // Compute the baseline of the first line box.
  kFirstLine,
  // Compute the baseline(s) for when we are within an inline-block context. If
  // the child is block-flow it will produce both the first, and last baselines.
  kInlineBlock
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
  USING_FAST_MALLOC(NGConstraintSpace);

 public:
  // To ensure that the bfc_offset_, rare_data_ union doesn't get polluted,
  // always initialize the bfc_offset_.
  NGConstraintSpace() : bfc_offset_() {}

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
        U_FALLTHROUGH;
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
        U_FALLTHROUGH;
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

  // Inline/block target stretch size constraints.
  // See:
  // https://mathml-refresh.github.io/mathml-core/#dfn-inline-stretch-size-constraint
  LayoutUnit TargetStretchInlineSize() const {
    return HasRareData() ? rare_data_->TargetStretchInlineSize()
                         : kIndefiniteSize;
  }

  bool HasTargetStretchInlineSize() const {
    return TargetStretchInlineSize() != kIndefiniteSize;
  }

  LayoutUnit TargetStretchAscentSize() const {
    return HasRareData() ? rare_data_->TargetStretchAscentSize()
                         : kIndefiniteSize;
  }

  bool HasTargetStretchAscentSize() const {
    return TargetStretchAscentSize() != kIndefiniteSize;
  }

  LayoutUnit TargetStretchDescentSize() const {
    return HasRareData() ? rare_data_->TargetStretchDescentSize()
                         : kIndefiniteSize;
  }

  bool HasTargetStretchDescentSize() const {
    return TargetStretchDescentSize() != kIndefiniteSize;
  }

  // Return the borders which should be used for a table-cell.
  NGBoxStrut TableCellBorders() const {
    return HasRareData() ? rare_data_->TableCellBorders() : NGBoxStrut();
  }

  wtf_size_t TableCellColumnIndex() const {
    return HasRareData() ? rare_data_->TableCellColumnIndex() : 0;
  }

  // Return the "intrinsic" padding for a table-cell.
  NGBoxStrut TableCellIntrinsicPadding() const {
    return HasRareData() ? rare_data_->TableCellIntrinsicPadding()
                         : NGBoxStrut();
  }

  // Return the baseline offset which the table-cell children should align
  // their baseline to.
  base::Optional<LayoutUnit> TableCellAlignmentBaseline() const {
    return HasRareData() ? rare_data_->TableCellAlignmentBaseline()
                         : base::nullopt;
  }

  bool IsTableCellHiddenForPaint() const {
    return HasRareData() ? rare_data_->IsTableCellHiddenForPaint() : false;
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

  // Return the block-offset from the block-start of the fragmentainer
  // relative to the block-start of the current block formatting context in
  // the current fragmentainer. Note that if the current block formatting
  // context starts in a previous fragmentainer, we'll return the block-offset
  // relative to the current fragmentainer.
  LayoutUnit FragmentainerOffsetAtBfc() const {
    DCHECK(HasBlockFragmentation());
    if (HasRareData())
      return rare_data_->fragmentainer_offset_at_bfc;
    return LayoutUnit();
  }

  // Whether the current constraint space is for the newly established
  // Formatting Context.
  bool IsNewFormattingContext() const {
    return bitfields_.is_new_formatting_context;
  }

  // Whether the current node is a table-cell.
  bool IsTableCell() const { return bitfields_.is_table_cell; }

  // True if node is either LayoutTableCell or LayoutNGTableCellLegacy
  bool IsLegacyTableCell() const { return bitfields_.is_legacy_table_cell; }

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

  // Returns if the parent layout needs the baseline from this layout.
  //
  // This bit is only used for skipping querying baseline information from
  // legacy layout.
  bool NeedsBaseline() const { return bitfields_.needs_baseline; }

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

  // Whether a fixed block-size should be considered indefinite.
  bool IsFixedBlockSizeIndefinite() const {
    return bitfields_.is_fixed_block_size_indefinite;
  }

  // Whether an auto inline-size should be interpreted as shrink-to-fit
  // (ie. fit-content). This is used for inline-block, floats, etc.
  bool IsShrinkToFit() const { return bitfields_.is_shrink_to_fit; }

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

  // Return true if there's an ancestor multicol container with balanced
  // columns.
  bool IsInsideBalancedColumns() const {
    return HasRareData() && rare_data_->is_inside_balanced_columns;
  }

  // Return true if we're participating in the same block formatting context as
  // the one established by the nearest ancestor multicol container.
  bool IsInColumnBfc() const {
    return HasRareData() && rare_data_->is_in_column_bfc;
  }

  // Get the appeal of the best breakpoint found so far. When progressing
  // through layout, we know that we don't need to consider less appealing
  // breakpoints than this.
  NGBreakAppeal EarlyBreakAppeal() const {
    if (!HasRareData())
      return kBreakAppealLastResort;
    return static_cast<NGBreakAppeal>(rare_data_->early_break_appeal);
  }

  // Returns if this node is a table cell child, and which table layout mode
  // is occurring.
  NGTableCellChildLayoutMode TableCellChildLayoutMode() const {
    return static_cast<NGTableCellChildLayoutMode>(
        bitfields_.table_cell_child_layout_mode);
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
  base::Optional<LayoutUnit> ForcedBfcBlockOffset() const {
    return HasRareData() ? rare_data_->ForcedBfcBlockOffset() : base::nullopt;
  }

  // If present, this is a hint as to where place any adjoining objects. This
  // isn't necessarily the final position, just where they ended up in a
  // previous layout pass.
  base::Optional<LayoutUnit> OptimisticBfcBlockOffset() const {
    return HasRareData() ? rare_data_->OptimisticBfcBlockOffset()
                         : base::nullopt;
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

  base::Optional<int> LinesUntilClamp() const {
    return HasRareData() ? rare_data_->LinesUntilClamp() : base::nullopt;
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

  // Returns true if the size constraints (shrink-to-fit, fixed-inline-size)
  // are equal.
  bool AreSizeConstraintsEqual(const NGConstraintSpace& other) const {
    return bitfields_.AreSizeConstraintsEqual(other.bitfields_);
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
    enum DataUnionType {
      kNone,
      kBlockData,         // An inflow block which doesn't establish a new FC.
      kTableCellData,     // A table-cell (display: table-cell).
      kTableRowData,      // A table-row (display: table-row).
      kTableSectionData,  // A table-section (display: table-section).
      kCustomData,        // A custom layout (display: layout(foo)).
      kStretchData        // The target inline/block stretch sizes for MathML.
    };

    explicit RareData(const NGBfcOffset bfc_offset)
        : bfc_offset(bfc_offset),
          data_union_type(static_cast<unsigned>(kNone)),
          is_restricted_block_size_table_cell(false),
          hide_table_cell_if_empty(false),
          block_direction_fragmentation_type(
              static_cast<unsigned>(kFragmentNone)),
          is_inside_balanced_columns(false),
          is_in_column_bfc(false),
          early_break_appeal(kBreakAppealLastResort) {}
    RareData(const RareData& other)
        : percentage_resolution_size(other.percentage_resolution_size),
          replaced_percentage_resolution_block_size(
              other.replaced_percentage_resolution_block_size),
          block_start_annotation_space(other.block_start_annotation_space),
          bfc_offset(other.bfc_offset),
          fragmentainer_block_size(other.fragmentainer_block_size),
          fragmentainer_offset_at_bfc(other.fragmentainer_offset_at_bfc),
          data_union_type(other.data_union_type),
          is_restricted_block_size_table_cell(
              other.is_restricted_block_size_table_cell),
          hide_table_cell_if_empty(other.hide_table_cell_if_empty),
          block_direction_fragmentation_type(
              other.block_direction_fragmentation_type),
          is_inside_balanced_columns(other.is_inside_balanced_columns),
          is_in_column_bfc(other.is_in_column_bfc),
          early_break_appeal(other.early_break_appeal) {
      switch (data_union_type) {
        case kNone:
          break;
        case kBlockData:
          new (&block_data_) BlockData(other.block_data_);
          break;
        case kTableCellData:
          new (&table_cell_data_) TableCellData(other.table_cell_data_);
          break;
        case kTableRowData:
          new (&table_row_data_) TableRowData(other.table_row_data_);
          break;
        case kTableSectionData:
          new (&table_section_data_)
              TableSectionData(other.table_section_data_);
          break;
        case kCustomData:
          new (&custom_data_) CustomData(other.custom_data_);
          break;
        case kStretchData:
          new (&stretch_data_) StretchData(other.stretch_data_);
          break;
        default:
          NOTREACHED();
      }
    }
    ~RareData() {
      switch (data_union_type) {
        case kNone:
          break;
        case kBlockData:
          block_data_.~BlockData();
          break;
        case kTableCellData:
          table_cell_data_.~TableCellData();
          break;
        case kTableRowData:
          table_row_data_.~TableRowData();
          break;
        case kTableSectionData:
          table_section_data_.~TableSectionData();
          break;
        case kCustomData:
          custom_data_.~CustomData();
          break;
        case kStretchData:
          stretch_data_.~StretchData();
          break;
        default:
          NOTREACHED();
      }
    }

    bool MaySkipLayout(const RareData& other) const {
      if (fragmentainer_block_size != other.fragmentainer_block_size ||
          fragmentainer_offset_at_bfc != other.fragmentainer_offset_at_bfc ||
          data_union_type != other.data_union_type ||
          is_restricted_block_size_table_cell !=
              other.is_restricted_block_size_table_cell ||
          hide_table_cell_if_empty != other.hide_table_cell_if_empty ||
          block_direction_fragmentation_type !=
              other.block_direction_fragmentation_type ||
          is_inside_balanced_columns != other.is_inside_balanced_columns ||
          is_in_column_bfc != other.is_in_column_bfc ||
          early_break_appeal != other.early_break_appeal)
        return false;

      switch (data_union_type) {
        case kNone:
          return true;
        case kBlockData:
          return block_data_.MaySkipLayout(other.block_data_);
        case kTableCellData:
          return table_cell_data_.MaySkipLayout(other.table_cell_data_);
        case kTableRowData:
          return table_row_data_.MaySkipLayout(other.table_row_data_);
        case kTableSectionData:
          return table_section_data_.MaySkipLayout(other.table_section_data_);
        case kCustomData:
          return custom_data_.MaySkipLayout(other.custom_data_);
        case kStretchData:
          return stretch_data_.MaySkipLayout(other.stretch_data_);
      }
      NOTREACHED();
      return false;
    }

    // Must be kept in sync with members checked within |MaySkipLayout|.
    bool IsInitialForMaySkipLayout() const {
      if (fragmentainer_block_size != kIndefiniteSize ||
          fragmentainer_offset_at_bfc || is_restricted_block_size_table_cell ||
          hide_table_cell_if_empty ||
          block_direction_fragmentation_type != kFragmentNone ||
          is_inside_balanced_columns || is_in_column_bfc ||
          early_break_appeal != kBreakAppealLastResort)
        return false;

      switch (data_union_type) {
        case kNone:
          return true;
        case kBlockData:
          return block_data_.IsInitialForMaySkipLayout();
        case kTableCellData:
          return table_cell_data_.IsInitialForMaySkipLayout();
        case kTableRowData:
          return table_row_data_.IsInitialForMaySkipLayout();
        case kTableSectionData:
          return table_section_data_.IsInitialForMaySkipLayout();
        case kCustomData:
          return custom_data_.IsInitialForMaySkipLayout();
        case kStretchData:
          return stretch_data_.IsInitialForMaySkipLayout();
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
      return data_union_type == kBlockData ? block_data_.margin_strut
                                           : NGMarginStrut();
    }

    void SetMarginStrut(const NGMarginStrut& margin_strut) {
      EnsureBlockData()->margin_strut = margin_strut;
    }

    base::Optional<LayoutUnit> OptimisticBfcBlockOffset() const {
      return data_union_type == kBlockData
                 ? block_data_.optimistic_bfc_block_offset
                 : base::nullopt;
    }

    void SetOptimisticBfcBlockOffset(LayoutUnit optimistic_bfc_block_offset) {
      EnsureBlockData()->optimistic_bfc_block_offset =
          optimistic_bfc_block_offset;
    }

    base::Optional<LayoutUnit> ForcedBfcBlockOffset() const {
      return data_union_type == kBlockData ? block_data_.forced_bfc_block_offset
                                           : base::nullopt;
    }

    void SetForcedBfcBlockOffset(LayoutUnit forced_bfc_block_offset) {
      EnsureBlockData()->forced_bfc_block_offset = forced_bfc_block_offset;
    }

    LayoutUnit ClearanceOffset() const {
      return data_union_type == kBlockData ? block_data_.clearance_offset
                                           : LayoutUnit::Min();
    }

    void SetClearanceOffset(LayoutUnit clearance_offset) {
      EnsureBlockData()->clearance_offset = clearance_offset;
    }

    base::Optional<int> LinesUntilClamp() const {
      return data_union_type == kBlockData ? block_data_.lines_until_clamp
                                           : base::nullopt;
    }

    void SetLinesUntilClamp(int value) {
      EnsureBlockData()->lines_until_clamp = value;
    }

    NGBoxStrut TableCellBorders() const {
      return data_union_type == kTableCellData
                 ? table_cell_data_.table_cell_borders
                 : NGBoxStrut();
    }

    void SetTableCellBorders(const NGBoxStrut& table_cell_borders) {
      EnsureTableCellData()->table_cell_borders = table_cell_borders;
    }

    NGBoxStrut TableCellIntrinsicPadding() const {
      return data_union_type == kTableCellData
                 ? NGBoxStrut(
                       LayoutUnit(), LayoutUnit(),
                       table_cell_data_
                           .table_cell_intrinsic_padding_block_start,
                       table_cell_data_.table_cell_intrinsic_padding_block_end)
                 : NGBoxStrut();
    }

    void SetTableCellIntrinsicPadding(
        const NGBoxStrut& table_cell_intrinsic_padding) {
      EnsureTableCellData()->table_cell_intrinsic_padding_block_start =
          table_cell_intrinsic_padding.block_start;
      EnsureTableCellData()->table_cell_intrinsic_padding_block_end =
          table_cell_intrinsic_padding.block_end;
    }

    wtf_size_t TableCellColumnIndex() const {
      return data_union_type == kTableCellData
                 ? table_cell_data_.table_cell_column_index
                 : 0;
    }

    void SetTableCellColumnIndex(wtf_size_t table_cell_column_index) {
      EnsureTableCellData()->table_cell_column_index = table_cell_column_index;
    }

    base::Optional<LayoutUnit> TableCellAlignmentBaseline() const {
      return data_union_type == kTableCellData
                 ? table_cell_data_.table_cell_alignment_baseline
                 : base::nullopt;
    }

    void SetTableCellAlignmentBaseline(
        LayoutUnit table_cell_alignment_baseline) {
      EnsureTableCellData()->table_cell_alignment_baseline =
          table_cell_alignment_baseline;
    }

    bool IsTableCellHiddenForPaint() const {
      return data_union_type == kTableCellData &&
             table_cell_data_.is_hidden_for_paint;
    }

    void SetIsTableCellHiddenForPaint(bool is_hidden_for_paint) {
      EnsureTableCellData()->is_hidden_for_paint = is_hidden_for_paint;
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

    const NGTableConstraintSpaceData* TableData() {
      if (data_union_type == kTableRowData)
        return EnsureTableRowData()->table_data.get();
      if (data_union_type == kTableSectionData)
        return EnsureTableSectionData()->table_data.get();
      return nullptr;
    }

    wtf_size_t TableRowIndex() {
      return data_union_type == kTableRowData ? EnsureTableRowData()->row_index
                                              : kNotFound;
    }

    wtf_size_t TableSectionIndex() {
      return data_union_type == kTableSectionData
                 ? EnsureTableSectionData()->section_index
                 : kNotFound;
    }

    SerializedScriptValue* CustomLayoutData() const {
      return data_union_type == kCustomData ? custom_data_.data.get() : nullptr;
    }

    void SetCustomLayoutData(
        scoped_refptr<SerializedScriptValue> custom_layout_data) {
      EnsureCustomData()->data = std::move(custom_layout_data);
    }

    LayoutUnit TargetStretchInlineSize() const {
      return data_union_type == kStretchData
                 ? stretch_data_.target_stretch_inline_size
                 : kIndefiniteSize;
    }

    void SetTargetStretchInlineSize(LayoutUnit target_stretch_inline_size) {
      EnsureStretchData()->target_stretch_inline_size =
          target_stretch_inline_size;
    }

    LayoutUnit TargetStretchAscentSize() const {
      return data_union_type == kStretchData
                 ? stretch_data_.target_stretch_ascent_size
                 : kIndefiniteSize;
    }

    void SetTargetStretchAscentSize(LayoutUnit target_stretch_ascent_size) {
      EnsureStretchData()->target_stretch_ascent_size =
          target_stretch_ascent_size;
    }

    LayoutUnit TargetStretchDescentSize() const {
      return data_union_type == kStretchData
                 ? stretch_data_.target_stretch_descent_size
                 : kIndefiniteSize;
    }

    void SetTargetStretchDescentSize(LayoutUnit target_stretch_descent_size) {
      EnsureStretchData()->target_stretch_descent_size =
          target_stretch_descent_size;
    }

    LogicalSize percentage_resolution_size;
    LayoutUnit replaced_percentage_resolution_block_size;
    LayoutUnit block_start_annotation_space;
    NGBfcOffset bfc_offset;

    LayoutUnit fragmentainer_block_size = kIndefiniteSize;
    LayoutUnit fragmentainer_offset_at_bfc;

    unsigned data_union_type : 3;

    unsigned is_restricted_block_size_table_cell : 1;
    unsigned hide_table_cell_if_empty : 1;

    unsigned block_direction_fragmentation_type : 2;
    unsigned is_inside_balanced_columns : 1;
    unsigned is_in_column_bfc : 1;
    unsigned early_break_appeal : 2;  // NGBreakAppeal
   private:
    struct BlockData {
      bool MaySkipLayout(const BlockData& other) const {
        return lines_until_clamp == other.lines_until_clamp;
      }

      bool IsInitialForMaySkipLayout() const {
        return !lines_until_clamp.has_value();
      }

      NGMarginStrut margin_strut;
      base::Optional<LayoutUnit> optimistic_bfc_block_offset;
      base::Optional<LayoutUnit> forced_bfc_block_offset;
      LayoutUnit clearance_offset = LayoutUnit::Min();
      base::Optional<int> lines_until_clamp;
    };

    struct TableCellData {
      bool MaySkipLayout(const TableCellData& other) const {
        return table_cell_borders == other.table_cell_borders &&
               table_cell_intrinsic_padding_block_start ==
                   other.table_cell_intrinsic_padding_block_start &&
               table_cell_intrinsic_padding_block_end ==
                   other.table_cell_intrinsic_padding_block_end &&
               table_cell_alignment_baseline ==
                   other.table_cell_alignment_baseline &&
               table_cell_column_index == other.table_cell_column_index &&
               is_hidden_for_paint == other.is_hidden_for_paint;
      }

      bool IsInitialForMaySkipLayout() const {
        return table_cell_borders == NGBoxStrut() &&
               table_cell_intrinsic_padding_block_start == LayoutUnit() &&
               table_cell_intrinsic_padding_block_end == LayoutUnit() &&
               table_cell_column_index == kNotFound &&
               table_cell_alignment_baseline == base::nullopt &&
               !is_hidden_for_paint;
      }

      NGBoxStrut table_cell_borders;
      LayoutUnit table_cell_intrinsic_padding_block_start;
      LayoutUnit table_cell_intrinsic_padding_block_end;
      wtf_size_t table_cell_column_index = kNotFound;
      base::Optional<LayoutUnit> table_cell_alignment_baseline;
      bool is_hidden_for_paint = false;
    };

    struct TableRowData {
      bool MaySkipLayout(const TableRowData& other) const {
        return table_data->EqualTableSpecificData(other.table_data.get()) &&
               table_data->MaySkipRowLayout(other.table_data.get(), row_index);
      }
      bool IsInitialForMaySkipLayout() const {
        return !table_data && row_index == kNotFound;
      }

      scoped_refptr<const NGTableConstraintSpaceData> table_data;
      wtf_size_t row_index = kNotFound;
    };

    struct TableSectionData {
      bool MaySkipLayout(const TableSectionData& other) const {
        return table_data->EqualTableSpecificData(other.table_data.get()) &&
               table_data->MaySkipSectionLayout(other.table_data.get(),
                                                section_index);
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
               target_stretch_ascent_size == other.target_stretch_ascent_size &&
               target_stretch_descent_size == other.target_stretch_descent_size;
      }

      bool IsInitialForMaySkipLayout() const {
        return target_stretch_inline_size == kIndefiniteSize &&
               target_stretch_ascent_size == kIndefiniteSize &&
               target_stretch_descent_size == kIndefiniteSize;
      }

      LayoutUnit target_stretch_inline_size = kIndefiniteSize;
      LayoutUnit target_stretch_ascent_size = kIndefiniteSize;
      LayoutUnit target_stretch_descent_size = kIndefiniteSize;
    };

    BlockData* EnsureBlockData() {
      DCHECK(data_union_type == kNone || data_union_type == kBlockData);
      if (data_union_type != kBlockData) {
        data_union_type = kBlockData;
        new (&block_data_) BlockData();
      }
      return &block_data_;
    }

    TableCellData* EnsureTableCellData() {
      DCHECK(data_union_type == kNone || data_union_type == kTableCellData);
      if (data_union_type != kTableCellData) {
        data_union_type = kTableCellData;
        new (&table_cell_data_) TableCellData();
      }
      return &table_cell_data_;
    }

    TableRowData* EnsureTableRowData() {
      DCHECK(data_union_type == kNone || data_union_type == kTableRowData);
      if (data_union_type != kTableRowData) {
        data_union_type = kTableRowData;
        new (&table_row_data_) TableRowData();
      }
      return &table_row_data_;
    }

    TableSectionData* EnsureTableSectionData() {
      DCHECK(data_union_type == kNone || data_union_type == kTableSectionData);
      if (data_union_type != kTableSectionData) {
        data_union_type = kTableSectionData;
        new (&table_section_data_) TableSectionData();
      }
      return &table_section_data_;
    }

    CustomData* EnsureCustomData() {
      DCHECK(data_union_type == kNone || data_union_type == kCustomData);
      if (data_union_type != kCustomData) {
        data_union_type = kCustomData;
        new (&custom_data_) CustomData();
      }
      return &custom_data_;
    }

    StretchData* EnsureStretchData() {
      DCHECK(data_union_type == kNone || data_union_type == kStretchData);
      if (data_union_type != kStretchData) {
        data_union_type = kStretchData;
        new (&stretch_data_) StretchData();
      }
      return &stretch_data_;
    }

    union {
      BlockData block_data_;
      TableCellData table_cell_data_;
      TableRowData table_row_data_;
      TableSectionData table_section_data_;
      CustomData custom_data_;
      StretchData stretch_data_;
    };
  };

  // This struct simply allows us easily copy, compare, and initialize all the
  // bitfields without having to explicitly copy, compare, and initialize each
  // one (see the outer class constructors, and assignment operators).
  struct Bitfields {
    DISALLOW_NEW();

   public:
    Bitfields() : Bitfields(WritingMode::kHorizontalTb) {}

    explicit Bitfields(WritingMode writing_mode)
        : has_rare_data(false),
          adjoining_object_types(static_cast<unsigned>(kAdjoiningNone)),
          writing_mode(static_cast<unsigned>(writing_mode)),
          direction(static_cast<unsigned>(TextDirection::kLtr)),
          is_table_cell(false),
          is_legacy_table_cell(false),
          is_anonymous(false),
          is_new_formatting_context(false),
          is_orthogonal_writing_mode_root(false),
          is_painted_atomically(false),
          use_first_line_style(false),
          ancestor_has_clearance_past_adjoining_floats(false),
          needs_baseline(false),
          baseline_algorithm_type(
              static_cast<unsigned>(NGBaselineAlgorithmType::kFirstLine)),
          cache_slot(static_cast<unsigned>(NGCacheSlot::kLayout)),
          is_shrink_to_fit(false),
          is_fixed_inline_size(false),
          is_fixed_block_size(false),
          is_fixed_block_size_indefinite(false),
          table_cell_child_layout_mode(static_cast<unsigned>(
              NGTableCellChildLayoutMode::kNotTableCellChild)),
          percentage_inline_storage(kSameAsAvailable),
          percentage_block_storage(kSameAsAvailable),
          replaced_percentage_block_storage(kSameAsAvailable) {}

    bool MaySkipLayout(const Bitfields& other) const {
      return adjoining_object_types == other.adjoining_object_types &&
             writing_mode == other.writing_mode &&
             direction == other.direction &&
             is_table_cell == other.is_table_cell &&
             is_legacy_table_cell == other.is_legacy_table_cell &&
             is_anonymous == other.is_anonymous &&
             is_new_formatting_context == other.is_new_formatting_context &&
             is_orthogonal_writing_mode_root ==
                 other.is_orthogonal_writing_mode_root &&
             is_painted_atomically == other.is_painted_atomically &&
             use_first_line_style == other.use_first_line_style &&
             ancestor_has_clearance_past_adjoining_floats ==
                 other.ancestor_has_clearance_past_adjoining_floats &&
             needs_baseline == other.needs_baseline &&
             baseline_algorithm_type == other.baseline_algorithm_type;
    }

    bool AreSizeConstraintsEqual(const Bitfields& other) const {
      return is_shrink_to_fit == other.is_shrink_to_fit &&
             is_fixed_inline_size == other.is_fixed_inline_size &&
             is_fixed_block_size == other.is_fixed_block_size &&
             is_fixed_block_size_indefinite ==
                 other.is_fixed_block_size_indefinite &&
             table_cell_child_layout_mode == other.table_cell_child_layout_mode;
    }

    unsigned has_rare_data : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes
    unsigned writing_mode : 3;
    unsigned direction : 1;

    unsigned is_table_cell : 1;
    unsigned is_legacy_table_cell : 1;

    unsigned is_anonymous : 1;
    unsigned is_new_formatting_context : 1;
    unsigned is_orthogonal_writing_mode_root : 1;

    unsigned is_painted_atomically : 1;
    unsigned use_first_line_style : 1;
    unsigned ancestor_has_clearance_past_adjoining_floats : 1;

    unsigned needs_baseline : 1;
    unsigned baseline_algorithm_type : 1;

    unsigned cache_slot : 1;

    // Size constraints.
    unsigned is_shrink_to_fit : 1;
    unsigned is_fixed_inline_size : 1;
    unsigned is_fixed_block_size : 1;
    unsigned is_fixed_block_size_indefinite : 1;
    unsigned table_cell_child_layout_mode : 2;  // NGTableCellChildLayoutMode

    unsigned percentage_inline_storage : 2;           // NGPercentageStorage
    unsigned percentage_block_storage : 2;            // NGPercentageStorage
    unsigned replaced_percentage_block_storage : 2;   // NGPercentageStorage
  };

  explicit NGConstraintSpace(WritingMode writing_mode)
      : bfc_offset_(), bitfields_(writing_mode) {}

  inline bool HasRareData() const { return bitfields_.has_rare_data; }

  RareData* EnsureRareData() {
    if (!HasRareData()) {
      rare_data_ = new RareData(bfc_offset_);
      bitfields_.has_rare_data = true;
    }

    return rare_data_;
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
