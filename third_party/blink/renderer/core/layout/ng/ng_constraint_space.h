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
#include "third_party/blink/renderer/core/layout/ng/inline/ng_baseline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_appeal.h"
#include "third_party/blink/renderer/core/layout/ng/ng_floats_utils.h"
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
  static NGConstraintSpace CreateFromLayoutObject(const LayoutBlock&,
                                                  bool is_layout_root);

  const NGExclusionSpace& ExclusionSpace() const { return exclusion_space_; }

  TextDirection Direction() const {
    return static_cast<TextDirection>(bitfields_.direction);
  }

  WritingMode GetWritingMode() const {
    return static_cast<WritingMode>(bitfields_.writing_mode);
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

  // Return the borders which should be used for a table-cell.
  NGBoxStrut TableCellBorders() const {
    return HasRareData() ? rare_data_->TableCellBorders() : NGBoxStrut();
  }

  // Return the "intrinsic" padding for a table-cell.
  NGBoxStrut TableCellIntrinsicPadding() const {
    return HasRareData() ? rare_data_->TableCellIntrinsicPadding()
                         : NGBoxStrut();
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

  // Whether this constraint space is used for an intermediate layout in a
  // multi-pass layout. In such a case, we should not copy back the resulting
  // layout data to the legacy tree or create a paint fragment from it.
  bool IsIntermediateLayout() const {
    return bitfields_.is_intermediate_layout;
  }

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
    return bitfields_.is_restricted_block_size_table_cell;
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

  const NGBaselineRequestList BaselineRequests() const {
    return NGBaselineRequestList(bitfields_.baseline_requests);
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

  explicit NGConstraintSpace(WritingMode writing_mode)
      : bfc_offset_(), bitfields_(writing_mode) {}

  // This struct defines all of the inputs to layout which we consider rare.
  // Primarily this is:
  //  - Percentage resolution sizes which differ from the available size or
  //    aren't indefinite.
  //  - The margin strut.
  //  - Anything to do with floats (the exclusion space, clearance offset, etc).
  //  - Anything to do with fragmentation.
  //
  // This information is kept in a separate in this heap-allocated struct to
  // reduce memory usage. Over time this may have to change based on usage data.
  struct RareData {
    USING_FAST_MALLOC(RareData);

   public:
    explicit RareData(const NGBfcOffset bfc_offset)
        : bfc_offset(bfc_offset),
          data_union_type(static_cast<unsigned>(kNone)),
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
          bfc_offset(other.bfc_offset),
          fragmentainer_block_size(other.fragmentainer_block_size),
          fragmentainer_offset_at_bfc(other.fragmentainer_offset_at_bfc),
          data_union_type(other.data_union_type),
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
        case kCustomData:
          new (&custom_data_) CustomData(other.custom_data_);
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
        case kCustomData:
          custom_data_.~CustomData();
          break;
        default:
          NOTREACHED();
      }
    }

    // |RareData| unions different types of data which are mutually exclusive.
    // They fall into the following categories:
    enum DataUnionType {
      kNone,
      kBlockData,      // An inflow block which doesn't establish a new FC.
      kTableCellData,  // A table-cell (display: table-cell).
      kCustomData      // A custom layout (display: layout(foo)).
    };

    LogicalSize percentage_resolution_size;
    LayoutUnit replaced_percentage_resolution_block_size;
    NGBfcOffset bfc_offset;

    LayoutUnit fragmentainer_block_size = kIndefiniteSize;
    LayoutUnit fragmentainer_offset_at_bfc;

    unsigned data_union_type : 2;
    unsigned hide_table_cell_if_empty : 1;
    unsigned block_direction_fragmentation_type : 2;
    unsigned is_inside_balanced_columns : 1;
    unsigned is_in_column_bfc : 1;
    unsigned early_break_appeal : 2;  // NGBreakAppeal

    bool MaySkipLayout(const RareData& other) const {
      if (fragmentainer_block_size != other.fragmentainer_block_size ||
          fragmentainer_offset_at_bfc != other.fragmentainer_offset_at_bfc ||
          data_union_type != other.data_union_type ||
          hide_table_cell_if_empty != other.hide_table_cell_if_empty ||
          block_direction_fragmentation_type !=
              other.block_direction_fragmentation_type ||
          is_inside_balanced_columns != other.is_inside_balanced_columns ||
          is_in_column_bfc != other.is_in_column_bfc ||
          early_break_appeal != other.early_break_appeal)
        return false;

      if (data_union_type == kNone)
        return true;

      if (data_union_type == kBlockData)
        return true;

      if (data_union_type == kTableCellData)
        return table_cell_data_.MaySkipLayout(other.table_cell_data_);

      DCHECK_EQ(data_union_type, kCustomData);
      return custom_data_.MaySkipLayout(other.custom_data_);
    }

    // Must be kept in sync with members checked within |MaySkipLayout|.
    bool IsInitialForMaySkipLayout() const {
      if (fragmentainer_block_size != kIndefiniteSize ||
          fragmentainer_offset_at_bfc || hide_table_cell_if_empty ||
          block_direction_fragmentation_type != kFragmentNone ||
          is_inside_balanced_columns || is_in_column_bfc ||
          early_break_appeal != kBreakAppealLastResort)
        return false;

      if (data_union_type == kNone)
        return true;

      if (data_union_type == kBlockData)
        return true;

      if (data_union_type == kTableCellData)
        return table_cell_data_.IsInitialForMaySkipLayout();

      DCHECK_EQ(data_union_type, kCustomData);
      return custom_data_.IsInitialForMaySkipLayout();
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

    SerializedScriptValue* CustomLayoutData() const {
      return data_union_type == kCustomData ? custom_data_.data.get() : nullptr;
    }

    void SetCustomLayoutData(
        scoped_refptr<SerializedScriptValue> custom_layout_data) {
      EnsureCustomData()->data = std::move(custom_layout_data);
    }

   private:
    struct BlockData {
      NGMarginStrut margin_strut;
      base::Optional<LayoutUnit> optimistic_bfc_block_offset;
      base::Optional<LayoutUnit> forced_bfc_block_offset;
      LayoutUnit clearance_offset = LayoutUnit::Min();
    };

    BlockData* EnsureBlockData() {
      DCHECK(data_union_type == kNone || data_union_type == kBlockData);
      if (data_union_type != kBlockData) {
        data_union_type = kBlockData;
        new (&block_data_) BlockData();
      }
      return &block_data_;
    }

    struct TableCellData {
      NGBoxStrut table_cell_borders;
      LayoutUnit table_cell_intrinsic_padding_block_start;
      LayoutUnit table_cell_intrinsic_padding_block_end;

      bool MaySkipLayout(const TableCellData& other) const {
        return table_cell_borders == other.table_cell_borders &&
               table_cell_intrinsic_padding_block_start ==
                   other.table_cell_intrinsic_padding_block_start &&
               table_cell_intrinsic_padding_block_end ==
                   other.table_cell_intrinsic_padding_block_end;
      }

      bool IsInitialForMaySkipLayout() const {
        return table_cell_borders == NGBoxStrut() &&
               table_cell_intrinsic_padding_block_start == LayoutUnit() &&
               table_cell_intrinsic_padding_block_end == LayoutUnit();
      }
    };

    TableCellData* EnsureTableCellData() {
      DCHECK(data_union_type == kNone || data_union_type == kTableCellData);
      if (data_union_type != kTableCellData) {
        data_union_type = kTableCellData;
        new (&table_cell_data_) TableCellData();
      }
      return &table_cell_data_;
    }

    struct CustomData {
      scoped_refptr<SerializedScriptValue> data;

      bool MaySkipLayout(const CustomData& other) const {
        return data == other.data;
      }

      bool IsInitialForMaySkipLayout() const { return !data; }
    };

    CustomData* EnsureCustomData() {
      DCHECK(data_union_type == kNone || data_union_type == kCustomData);
      if (data_union_type != kCustomData) {
        data_union_type = kCustomData;
        new (&custom_data_) CustomData();
      }
      return &custom_data_;
    }

    union {
      BlockData block_data_;
      TableCellData table_cell_data_;
      CustomData custom_data_;
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
          is_anonymous(false),
          is_new_formatting_context(false),
          is_orthogonal_writing_mode_root(false),
          is_intermediate_layout(false),
          is_fixed_block_size_indefinite(false),
          is_restricted_block_size_table_cell(false),
          use_first_line_style(false),
          ancestor_has_clearance_past_adjoining_floats(false),
          is_shrink_to_fit(false),
          is_fixed_inline_size(false),
          is_fixed_block_size(false),
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
             is_anonymous == other.is_anonymous &&
             is_new_formatting_context == other.is_new_formatting_context &&
             is_orthogonal_writing_mode_root ==
                 other.is_orthogonal_writing_mode_root &&
             is_intermediate_layout == other.is_intermediate_layout &&
             is_fixed_block_size_indefinite ==
                 other.is_fixed_block_size_indefinite &&
             is_restricted_block_size_table_cell ==
                 other.is_restricted_block_size_table_cell &&
             use_first_line_style == other.use_first_line_style &&
             ancestor_has_clearance_past_adjoining_floats ==
                 other.ancestor_has_clearance_past_adjoining_floats &&
             baseline_requests == other.baseline_requests;
    }

    bool AreSizeConstraintsEqual(const Bitfields& other) const {
      return is_shrink_to_fit == other.is_shrink_to_fit &&
             is_fixed_inline_size == other.is_fixed_inline_size &&
             is_fixed_block_size == other.is_fixed_block_size &&
             table_cell_child_layout_mode == other.table_cell_child_layout_mode;
    }

    unsigned has_rare_data : 1;
    unsigned adjoining_object_types : 3;  // NGAdjoiningObjectTypes
    unsigned writing_mode : 3;
    unsigned direction : 1;

    unsigned is_table_cell : 1;
    unsigned is_anonymous : 1;
    unsigned is_new_formatting_context : 1;
    unsigned is_orthogonal_writing_mode_root : 1;
    unsigned is_intermediate_layout : 1;

    unsigned is_fixed_block_size_indefinite : 1;
    unsigned is_restricted_block_size_table_cell : 1;
    unsigned use_first_line_style : 1;
    unsigned ancestor_has_clearance_past_adjoining_floats : 1;

    unsigned baseline_requests : NGBaselineRequestList::kSerializedBits;

    // Size constraints.
    unsigned is_shrink_to_fit : 1;
    unsigned is_fixed_inline_size : 1;
    unsigned is_fixed_block_size : 1;
    unsigned table_cell_child_layout_mode : 2;  // NGTableCellChildLayoutMode

    unsigned percentage_inline_storage : 2;           // NGPercentageStorage
    unsigned percentage_block_storage : 2;            // NGPercentageStorage
    unsigned replaced_percentage_block_storage : 2;   // NGPercentageStorage
  };

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
