// Copyright 2016 The Chromium Authors.All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"

#include <algorithm>
#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

enum class EBlockAlignment { kStart, kCenter, kEnd };

inline EBlockAlignment BlockAlignment(const ComputedStyle& style,
                                      const ComputedStyle& container_style) {
  if (style.MayHaveMargin()) {
    bool start_auto = style.MarginStartUsing(container_style).IsAuto();
    bool end_auto = style.MarginEndUsing(container_style).IsAuto();
    if (start_auto || end_auto) {
      if (start_auto)
        return end_auto ? EBlockAlignment::kCenter : EBlockAlignment::kEnd;
      return EBlockAlignment::kStart;
    }
  }

  // If none of the inline margins are auto, look for -webkit- text-align
  // values (which are really about block alignment). These are typically
  // mapped from the legacy "align" HTML attribute.
  switch (container_style.GetTextAlign()) {
    case ETextAlign::kWebkitLeft:
      if (container_style.IsLeftToRightDirection())
        return EBlockAlignment::kStart;
      return EBlockAlignment::kEnd;
    case ETextAlign::kWebkitRight:
      if (container_style.IsLeftToRightDirection())
        return EBlockAlignment::kEnd;
      return EBlockAlignment::kStart;
    case ETextAlign::kWebkitCenter:
      return EBlockAlignment::kCenter;
    default:
      return EBlockAlignment::kStart;
  }
}

}  // anonymous namespace

// Check if we shouldn't resolve a percentage/calc()/-webkit-fill-available
// if we are in the intrinsic sizes phase.
bool InlineLengthUnresolvable(const NGConstraintSpace& constraint_space,
                              const Length& length) {
  if (length.IsPercentOrCalc())
    return constraint_space.PercentageResolutionInlineSize() == kIndefiniteSize;

  if (length.IsFillAvailable())
    return constraint_space.AvailableSize().inline_size == kIndefiniteSize;

  return false;
}

// When the containing block size to resolve against is indefinite, we
// cannot resolve percentages / calc() / -webkit-fill-available.
bool BlockLengthUnresolvable(
    const NGConstraintSpace& constraint_space,
    const Length& length,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max) {
  if (length.IsAuto() || length.IsMinContent() || length.IsMaxContent() ||
      length.IsMinIntrinsic() || length.IsFitContent() || length.IsNone())
    return true;
  if (length.IsPercentOrCalc()) {
    LayoutUnit percentage_resolution_block_size =
        opt_percentage_resolution_block_size_for_min_max
            ? *opt_percentage_resolution_block_size_for_min_max
            : constraint_space.PercentageResolutionBlockSize();
    return percentage_resolution_block_size == kIndefiniteSize;
  }

  if (length.IsFillAvailable())
    return constraint_space.AvailableSize().block_size == kIndefiniteSize;

  return false;
}

LayoutUnit ResolveInlineLengthInternal(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length) {
  DCHECK_EQ(constraint_space.GetWritingMode(), style.GetWritingMode());

  switch (length.GetType()) {
    case Length::kFillAvailable: {
      DCHECK_GE(constraint_space.AvailableSize().inline_size, LayoutUnit());
      LayoutUnit content_size = constraint_space.AvailableSize().inline_size;
      NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
      return std::max(border_padding.InlineSum(),
                      content_size - margins.InlineSum());
    }
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated: {
      LayoutUnit percentage_resolution_size =
          constraint_space.PercentageResolutionInlineSize();
      DCHECK(length.IsFixed() || percentage_resolution_size != kIndefiniteSize);
      LayoutUnit value =
          MinimumValueForLength(length, percentage_resolution_size);

      if (style.BoxSizing() == EBoxSizing::kBorderBox)
        value = std::max(border_padding.InlineSum(), value);
      else
        value += border_padding.InlineSum();
      return value;
    }
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kMinIntrinsic:
    case Length::kFitContent: {
      DCHECK(min_max_sizes.has_value());
      LayoutUnit available_size = constraint_space.AvailableSize().inline_size;
      LayoutUnit value;
      // TODO(ikilpatrick): The |IsFitContent()| might not be correct for a
      // max-size, e.g. "max-width: fit-content".
      if (length.IsMinContent() || length.IsMinIntrinsic() ||
          (length.IsFitContent() && available_size == kIndefiniteSize)) {
        value = min_max_sizes->min_size;
      } else if (length.IsMaxContent()) {
        value = min_max_sizes->max_size;
      } else {
        DCHECK_GE(available_size, LayoutUnit());
        NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
        LayoutUnit fill_available =
            std::max(LayoutUnit(), available_size - margins.InlineSum());
        value = min_max_sizes->ShrinkToFit(fill_available);
      }
      return value;
    }
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
      FALLTHROUGH;
    case Length::kAuto:
    case Length::kNone:
    default:
      NOTREACHED();
      return border_padding.InlineSum();
  }
}

LayoutUnit ResolveBlockLengthInternal(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit intrinsic_size,
    LayoutUnit available_block_size_adjustment,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max) {
  DCHECK_EQ(constraint_space.GetWritingMode(), style.GetWritingMode());

  switch (length.GetType()) {
    case Length::kFillAvailable: {
      DCHECK_GE(constraint_space.AvailableSize().block_size, LayoutUnit());
      LayoutUnit available_size = (constraint_space.AvailableSize().block_size -
                                   available_block_size_adjustment)
                                      .ClampNegativeToZero();
      NGBoxStrut margins = ComputeMarginsForSelf(constraint_space, style);
      return std::max(border_padding.BlockSum(),
                      available_size - margins.BlockSum());
    }
    case Length::kPercent:
    case Length::kFixed:
    case Length::kCalculated: {
      LayoutUnit percentage_resolution_size =
          opt_percentage_resolution_block_size_for_min_max
              ? *opt_percentage_resolution_block_size_for_min_max
              : constraint_space.PercentageResolutionBlockSize();
      DCHECK(length.IsFixed() || percentage_resolution_size != kIndefiniteSize);
      LayoutUnit value =
          MinimumValueForLength(length, percentage_resolution_size);

      if (style.BoxSizing() == EBoxSizing::kBorderBox)
        value = std::max(border_padding.BlockSum(), value);
      else
        value += border_padding.BlockSum();
      return value;
    }
    case Length::kMinContent:
    case Length::kMaxContent:
    case Length::kMinIntrinsic:
    case Length::kFitContent:
#if DCHECK_IS_ON()
      // Due to how intrinsic_size is calculated, it should always include
      // border and padding. We cannot check for this if we are
      // block-fragmented, though, because then the block-start border/padding
      // may be in a different fragmentainer than the block-end border/padding.
      if (intrinsic_size != kIndefiniteSize &&
          !constraint_space.HasBlockFragmentation())
        DCHECK_GE(intrinsic_size, border_padding.BlockSum());
#endif  // DCHECK_IS_ON()
      return intrinsic_size;
    case Length::kDeviceWidth:
    case Length::kDeviceHeight:
    case Length::kExtendToZoom:
      NOTREACHED() << "These should only be used for viewport definitions";
      FALLTHROUGH;
    case Length::kAuto:
    case Length::kNone:
    default:
      NOTREACHED();
      return border_padding.BlockSum();
  }
}

// logical_aspect_ratio is inline_size / block_size.
LayoutUnit InlineSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                     double logical_aspect_ratio,
                                     EBoxSizing box_sizing,
                                     LayoutUnit block_size) {
  // TODO(dgrogan/ikilpatrick): These calculations might need to be done in
  // integer space, in a potential BoundedMultiplyAndDivide(LayoutUnit,
  // LayoutUnit, LayoutUnit) function.
  if (box_sizing == EBoxSizing::kBorderBox) {
    return LayoutUnit::FromDoubleRound(block_size * logical_aspect_ratio);
  }

  return LayoutUnit::FromDoubleRound((block_size - border_padding.BlockSum()) *
                                     logical_aspect_ratio) +
         border_padding.InlineSum();
}

LayoutUnit InlineSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                     const LogicalSize& aspect_ratio,
                                     EBoxSizing box_sizing,
                                     LayoutUnit block_size) {
  return InlineSizeFromAspectRatio(
      border_padding,
      aspect_ratio.inline_size.ToDouble() / aspect_ratio.block_size.ToDouble(),
      box_sizing, block_size);
}

// logical_aspect_ratio is block_size / inline_size.
LayoutUnit BlockSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                    double logical_aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size) {
  if (box_sizing == EBoxSizing::kBorderBox) {
    return LayoutUnit::FromDoubleRound(inline_size * logical_aspect_ratio);
  }

  return LayoutUnit::FromDoubleRound(
             (inline_size - border_padding.InlineSum()) *
             logical_aspect_ratio) +
         border_padding.BlockSum();
}

LayoutUnit BlockSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                    const LogicalSize& aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size) {
  return BlockSizeFromAspectRatio(
      border_padding,
      aspect_ratio.block_size.ToDouble() / aspect_ratio.inline_size.ToDouble(),
      box_sizing, inline_size);
}

namespace {

template <typename MinMaxSizesFunc>
MinMaxSizesResult ComputeMinAndMaxContentContributionInternal(
    WritingMode parent_writing_mode,
    const NGBlockNode& child,
    const NGConstraintSpace& space,
    const MinMaxSizesFunc& min_max_sizes_func) {
  const ComputedStyle& style = child.Style();
  const WritingMode child_writing_mode = style.GetWritingMode();
  const NGBoxStrut border_padding =
      ComputeBorders(space, child) + ComputePadding(space, style);

  MinMaxSizesResult result;

  const Length& inline_size = parent_writing_mode == WritingMode::kHorizontalTb
                                  ? style.Width()
                                  : style.Height();
  if (inline_size.IsAuto() || inline_size.IsPercentOrCalc() ||
      inline_size.IsFillAvailable() || inline_size.IsFitContent()) {
    result = min_max_sizes_func(MinMaxSizesType::kContent);
  } else {
    if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
      MinMaxSizes sizes;
      sizes = ResolveMainInlineLength(space, style, border_padding,
                                      min_max_sizes_func, inline_size);
      result = MinMaxSizesResult(sizes,
                                 /* depends_on_block_constraints */ false);
    } else {
      auto IntrinsicBlockSizeFunc = [&]() -> LayoutUnit {
        return min_max_sizes_func(inline_size.IsMinIntrinsic()
                                      ? MinMaxSizesType::kIntrinsic
                                      : MinMaxSizesType::kContent)
            .sizes.max_size;
      };
      MinMaxSizes sizes;
      sizes = ResolveMainBlockLength(space, style, border_padding, inline_size,
                                     IntrinsicBlockSizeFunc);
      result = MinMaxSizesResult(sizes,
                                 /* depends_on_block_constraints */ false);
    }
  }

  const Length& max_length = parent_writing_mode == WritingMode::kHorizontalTb
                                 ? style.MaxWidth()
                                 : style.MaxHeight();
  LayoutUnit max;
  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    max = ResolveMaxInlineLength(space, style, border_padding,
                                 min_max_sizes_func, max_length);
  } else {
    max = ResolveMaxBlockLength(space, style, border_padding, max_length);
  }
  result.sizes.Constrain(max);

  const Length& min_length = parent_writing_mode == WritingMode::kHorizontalTb
                                 ? style.MinWidth()
                                 : style.MinHeight();
  LayoutUnit min;
  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    min = ResolveMinInlineLength(space, style, border_padding,
                                 min_max_sizes_func, min_length);
  } else {
    min = ResolveMinBlockLength(space, style, border_padding, min_length);
  }
  result.sizes.Encompass(min);

  // Tables need to apply one final constraint. They are never allowed to go
  // below their min-intrinsic size (even if they have an inline-size, etc).
  if (child.IsNGTable()) {
    result.sizes.Encompass(
        min_max_sizes_func(MinMaxSizesType::kIntrinsic).sizes.min_size);
  }

  return result;
}

// Currently this simply sets the correct override sizes for the replaced
// element, and lets legacy layout do the result.
MinMaxSizesResult ComputeMinAndMaxContentContributionForReplaced(
    const NGBlockNode& child,
    const NGConstraintSpace& space) {
  const ComputedStyle& child_style = child.Style();
  LayoutBox* box = child.GetLayoutBox();
  bool needs_size_reset = false;
  if (!box->HasOverrideContainingBlockContentLogicalHeight()) {
    box->SetOverrideContainingBlockContentLogicalHeight(
        space.ReplacedPercentageResolutionBlockSize());
    needs_size_reset = true;
  }

  MinMaxSizes result = box->PreferredLogicalWidths();

  if (needs_size_reset)
    box->ClearOverrideContainingBlockContentSize();

  // Replaced elements which have a percentage block-size always depend on
  // their block constraints (as they have an aspect-ratio which changes their
  // min/max content size).
  bool depends_on_block_constraints =
      child_style.LogicalMinHeight().IsPercentOrCalc() ||
      child_style.LogicalHeight().IsPercentOrCalc() ||
      child_style.LogicalMaxHeight().IsPercentOrCalc();
  return MinMaxSizesResult(result, depends_on_block_constraints);
}

}  // namespace

MinMaxSizesResult ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    const NGBlockNode& child,
    const NGConstraintSpace& space,
    const MinMaxSizesFloatInput float_input) {
  const auto& child_style = child.Style();
  const auto parent_writing_mode = parent_style.GetWritingMode();
  const auto child_writing_mode = child_style.GetWritingMode();

  if (IsParallelWritingMode(parent_writing_mode, child_writing_mode)) {
    // Legacy tables are special - always let the legacy table code handle this.
    if (child.IsTable() && !child.IsNGTable()) {
      return child.ComputeMinMaxSizes(
          parent_writing_mode, MinMaxSizesType::kContent, space, float_input);
    }

    if (child.IsReplaced())
      return ComputeMinAndMaxContentContributionForReplaced(child, space);
  }

  auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
    return child.ComputeMinMaxSizes(parent_writing_mode, type, space,
                                    float_input);
  };

  return ComputeMinAndMaxContentContributionInternal(parent_writing_mode, child,
                                                     space, MinMaxSizesFunc);
}

MinMaxSizesResult ComputeMinAndMaxContentContributionForSelf(
    const NGBlockNode& child,
    const NGConstraintSpace& space) {
  DCHECK(child.CreatesNewFormattingContext());

  const ComputedStyle& child_style = child.Style();
  WritingMode writing_mode = child_style.GetWritingMode();

  // Legacy tables are special - always let the legacy table code handle this.
  if (child.IsTable() && !child.IsNGTable()) {
    return child.ComputeMinMaxSizes(writing_mode, MinMaxSizesType::kContent,
                                    space);
  }

  if (child.IsReplaced())
    return ComputeMinAndMaxContentContributionForReplaced(child, space);

  auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
    return child.ComputeMinMaxSizes(writing_mode, type, space);
  };

  return ComputeMinAndMaxContentContributionInternal(writing_mode, child, space,
                                                     MinMaxSizesFunc);
}

MinMaxSizes ComputeMinAndMaxContentContributionForTest(
    WritingMode parent_writing_mode,
    const NGBlockNode& child,
    const NGConstraintSpace& space,
    const MinMaxSizes& min_max_sizes) {
  auto MinMaxSizesFunc = [&](MinMaxSizesType) -> MinMaxSizesResult {
    return MinMaxSizesResult(min_max_sizes,
                             /* depends_on_block_constraints */ false);
  };
  return ComputeMinAndMaxContentContributionInternal(parent_writing_mode, child,
                                                     space, MinMaxSizesFunc)
      .sizes;
}

namespace {

LayoutUnit ComputeInlineSizeFromAspectRatio(const NGConstraintSpace& space,
                                            const ComputedStyle& style,
                                            const NGBoxStrut& border_padding) {
  DCHECK(!style.AspectRatio().IsAuto());

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      space, style, border_padding,
      /* intrinsic_size */ kIndefiniteSize, /* inline_size */ base::nullopt);
  if (block_size == kIndefiniteSize)
    return kIndefiniteSize;

  // Check if we can get an inline size using the aspect ratio.
  return InlineSizeFromAspectRatio(border_padding, style.LogicalAspectRatio(),
                                   style.BoxSizingForAspectRatio(), block_size);
}

LayoutUnit ComputeInlineSizeForFragmentInternal(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_padding,
    const MinMaxSizes* override_min_max_sizes) {
  auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
    if (override_min_max_sizes) {
      return MinMaxSizesResult(*override_min_max_sizes,
                               /* depends_on_block_constraints */ false);
    }

    return node.ComputeMinMaxSizes(space.GetWritingMode(), type, space);
  };

  const ComputedStyle& style = node.Style();
  Length logical_width = style.LogicalWidth();
  Length min_length = style.LogicalMinWidth();

  // TODO(ikilpatrick): If we are stretching in both the inline-axis, and
  // block-axis, we shouldn't apply the aspect-ratio.
  // TODO(cbiesinger): Should the if also check !node.IsReplaced()?
  LayoutUnit extent = kIndefiniteSize;
  if (!style.AspectRatio().IsAuto()) {
    if (logical_width.IsAuto() || logical_width.IsMinContent() ||
        logical_width.IsMaxContent()) {
      extent = ComputeInlineSizeFromAspectRatio(space, style, border_padding);
    }
    if (UNLIKELY(extent != kIndefiniteSize)) {
      // This means we successfully applied aspect-ratio and now need to check
      // if we need to apply the implied minimum size:
      // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
      if (style.OverflowInlineDirection() == EOverflow::kVisible &&
          min_length.IsAuto())
        min_length = Length::MinIntrinsic();
    }
  }

  if (LIKELY(extent == kIndefiniteSize)) {
    if (logical_width.IsAuto()) {
      logical_width = space.StretchInlineSizeIfAuto() ? Length::FillAvailable()
                                                      : Length::FitContent();
    }
    extent = ResolveMainInlineLength(space, style, border_padding,
                                     MinMaxSizesFunc, logical_width);
  }

  const MinMaxSizes min_max_sizes = ComputeMinMaxInlineSizes(
      space, node, border_padding, MinMaxSizesFunc, &min_length);
  return min_max_sizes.ClampSizeToMinAndMax(extent);
}

}  // namespace

LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_padding,
    const MinMaxSizes* override_min_max_sizes_for_test) {
  if (space.IsFixedInlineSize() || space.IsAnonymous())
    return space.AvailableSize().inline_size;

  if (node.IsNGTable())
    return To<NGTableNode>(node).ComputeTableInlineSize(space, border_padding);

  return ComputeInlineSizeForFragmentInternal(space, node, border_padding,
                                              override_min_max_sizes_for_test);
}

LayoutUnit ComputeUsedInlineSizeForTableFragment(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_padding,
    const MinMaxSizes& table_grid_min_max_sizes) {
  DCHECK(!space.IsFixedInlineSize());
  return ComputeInlineSizeForFragmentInternal(space, node, border_padding,
                                              &table_grid_min_max_sizes);
}

MinMaxSizes ComputeMinMaxBlockSizes(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit available_block_size_adjustment,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max) {
  MinMaxSizes sizes = {
      ResolveMinBlockLength(constraint_space, style, border_padding,
                            style.LogicalMinHeight(),
                            available_block_size_adjustment,
                            opt_percentage_resolution_block_size_for_min_max),
      ResolveMaxBlockLength(constraint_space, style, border_padding,
                            style.LogicalMaxHeight(),
                            available_block_size_adjustment,
                            opt_percentage_resolution_block_size_for_min_max)};
  sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  return sizes;
}

MinMaxSizes ComputeTransferredMinMaxInlineSizes(
    const LogicalSize& ratio,
    const MinMaxSizes& block_min_max,
    const NGBoxStrut& border_padding,
    const EBoxSizing sizing) {
  MinMaxSizes transferred_min_max = {LayoutUnit(), LayoutUnit::Max()};
  if (block_min_max.min_size > LayoutUnit()) {
    transferred_min_max.min_size = InlineSizeFromAspectRatio(
        border_padding, ratio, sizing, block_min_max.min_size);
  }
  if (block_min_max.max_size != LayoutUnit::Max()) {
    transferred_min_max.max_size = InlineSizeFromAspectRatio(
        border_padding, ratio, sizing, block_min_max.max_size);
  }
  // Minimum size wins over maximum size.
  transferred_min_max.max_size =
      std::max(transferred_min_max.max_size, transferred_min_max.min_size);
  return transferred_min_max;
}

MinMaxSizes ComputeMinMaxInlineSizesFromAspectRatio(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding) {
  DCHECK(!style.AspectRatio().IsAuto());

  // The spec requires us to clamp these by the specified size (it calls it the
  // preferred size). However, we actually don't need to worry about that,
  // because we only use this if the width is indefinite.

  // We do not need to compute the min/max inline sizes; as long as we always
  // apply the transferred min/max size before the explicit min/max size, the
  // result will be identical.

  LogicalSize ratio = style.LogicalAspectRatio();
  MinMaxSizes block_min_max =
      ComputeMinMaxBlockSizes(constraint_space, style, border_padding,
                              /* intrinsic_size */ kIndefiniteSize);
  return ComputeTransferredMinMaxInlineSizes(
      ratio, block_min_max, border_padding, style.BoxSizingForAspectRatio());
}

namespace {

// Computes the block-size for a fragment, ignoring the fixed block-size if set.
LayoutUnit ComputeBlockSizeForFragmentInternal(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    base::Optional<LayoutUnit> inline_size,
    LayoutUnit available_block_size_adjustment = LayoutUnit(),
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  MinMaxSizes min_max = ComputeMinMaxBlockSizes(
      space, style, border_padding, available_block_size_adjustment,
      opt_percentage_resolution_block_size_for_min_max);

  // Scrollable percentage-sized children of table cells, in the table
  // "measure" phase contribute nothing to the row height measurement.
  // See: https://drafts.csswg.org/css-tables-3/#row-layout
  if (space.IsMeasuringRestrictedBlockSizeTableCellChild())
    return min_max.min_size;

  Length logical_height = style.LogicalHeight();
  if (logical_height.IsAuto()) {
    logical_height = space.StretchBlockSizeIfAuto() ? Length::FillAvailable()
                                                    : Length::FitContent();
  }
  // TODO(cbiesinger): Audit callers of ResolveMainBlockLength to see whether
  // they need to respect aspect ratio.
  LayoutUnit extent =
      ResolveMainBlockLength(space, style, border_padding, logical_height,
                             intrinsic_size, available_block_size_adjustment,
                             opt_percentage_resolution_block_size_for_min_max);
  if (UNLIKELY(!style.AspectRatio().IsAuto() && inline_size &&
               BlockLengthUnresolvable(
                   space, logical_height,
                   opt_percentage_resolution_block_size_for_min_max))) {
    extent =
        BlockSizeFromAspectRatio(border_padding, style.LogicalAspectRatio(),
                                 style.BoxSizingForAspectRatio(), *inline_size);
    // Apply the automatic minimum size for aspect ratio:
    // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
    // We also check for LayoutUnit::Max() because flexbox uses that as a
    // "placeholder" to compute the flex line length while still respecting
    // max-block-size.
    if (style.LogicalMinHeight().IsAuto() &&
        style.OverflowBlockDirection() == EOverflow::kVisible &&
        intrinsic_size != kIndefiniteSize &&
        intrinsic_size != LayoutUnit::Max()) {
      min_max.min_size = intrinsic_size;
    }
  } else if (extent == kIndefiniteSize) {
    DCHECK_EQ(intrinsic_size, kIndefiniteSize);
    return extent;
  }

  return min_max.ClampSizeToMinAndMax(extent);
}

}  // namespace

LayoutUnit ComputeBlockSizeForFragment(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    base::Optional<LayoutUnit> inline_size,
    LayoutUnit available_block_size_adjustment) {
  // The |available_block_size_adjustment| should only be used for <table>s.
  DCHECK(available_block_size_adjustment == LayoutUnit() ||
         style.IsDisplayTableBox());

  if (constraint_space.IsLegacyTableCell() && intrinsic_size != kIndefiniteSize)
    return intrinsic_size;

  if (constraint_space.IsFixedBlockSize()) {
    return (constraint_space.AvailableSize().block_size -
            available_block_size_adjustment)
        .ClampNegativeToZero();
  }

  if (constraint_space.IsTableCell() && !constraint_space.IsLegacyTableCell() &&
      intrinsic_size != kIndefiniteSize)
    return intrinsic_size;

  if (constraint_space.IsAnonymous())
    return intrinsic_size;

  return ComputeBlockSizeForFragmentInternal(
      constraint_space, style, border_padding, intrinsic_size, inline_size,
      available_block_size_adjustment);
}

LayoutUnit ComputeInitialBlockSizeForFragment(
    const NGConstraintSpace& space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    base::Optional<LayoutUnit> inline_size,
    LayoutUnit available_block_size_adjustment) {
  if (space.IsFixedBlockSizeIndefinite())
    return intrinsic_size;
  return ComputeBlockSizeForFragment(space, style, border_padding,
                                     intrinsic_size, inline_size,
                                     available_block_size_adjustment);
}

// Computes size for a replaced element.
void ComputeReplacedSize(const NGBlockNode& node,
                         const NGConstraintSpace& space,
                         const base::Optional<MinMaxSizes>& child_min_max_sizes,
                         base::Optional<LogicalSize>* out_replaced_size,
                         base::Optional<LogicalSize>* out_aspect_ratio) {
  DCHECK(node.IsReplaced());
  DCHECK(!out_replaced_size->has_value());
  DCHECK(!out_aspect_ratio->has_value());

  const ComputedStyle& style = node.Style();

  NGBoxStrut border_padding =
      ComputeBorders(space, node) + ComputePadding(space, style);
  LayoutUnit inline_min =
      ResolveMinInlineLength(space, style, border_padding, child_min_max_sizes,
                             style.LogicalMinWidth());
  LayoutUnit inline_max =
      ResolveMaxInlineLength(space, style, border_padding, child_min_max_sizes,
                             style.LogicalMaxWidth());
  LayoutUnit block_min = ResolveMinBlockLength(space, style, border_padding,
                                               style.LogicalMinHeight());
  LayoutUnit block_max = ResolveMaxBlockLength(space, style, border_padding,
                                               style.LogicalMaxHeight());

  const Length& inline_length = style.LogicalWidth();
  const Length& block_length = style.LogicalHeight();
  base::Optional<LayoutUnit> replaced_inline;
  if (space.IsFixedInlineSize()) {
    replaced_inline = space.AvailableSize().inline_size;
    DCHECK_GE(*replaced_inline, 0);
  } else if (!inline_length.IsAuto() || space.StretchInlineSizeIfAuto()) {
    Length inline_length_to_resolve = inline_length;
    if (inline_length_to_resolve.IsAuto()) {
      // TODO(dgrogan): This code block (and its corresponding block version
      // below) didn't make any tests pass when written so it may be unnecessary
      // or untested. Check again when launching ReplacedNG.
      DCHECK(space.StretchInlineSizeIfAuto());
      DCHECK(space.AvailableSize().inline_size != kIndefiniteSize);
      inline_length_to_resolve = Length::FillAvailable();
    }
    replaced_inline =
        ResolveMainInlineLength(space, style, border_padding,
                                child_min_max_sizes, inline_length_to_resolve);
    DCHECK(replaced_inline != kIndefiniteSize);
    replaced_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
  }
  base::Optional<LayoutUnit> replaced_block;
  if (space.IsFixedBlockSize()) {
    replaced_block = space.AvailableSize().block_size;
    DCHECK_GE(*replaced_block, 0);
  } else if (!block_length.IsAuto() || space.StretchBlockSizeIfAuto()) {
    Length block_length_to_resolve = block_length;
    if (block_length_to_resolve.IsAuto()) {
      DCHECK(space.StretchBlockSizeIfAuto());
      DCHECK(space.AvailableSize().block_size != kIndefiniteSize);
      block_length_to_resolve = Length::FillAvailable();
    }
    replaced_block = ResolveMainBlockLength(space, style, border_padding,
                                            block_length_to_resolve,
                                            space.AvailableSize().block_size);
    if (*replaced_block == kIndefiniteSize)
      replaced_block.reset();
    else
      replaced_block = ConstrainByMinMax(*replaced_block, block_min, block_max);
  }
  if (replaced_inline && replaced_block) {
    out_replaced_size->emplace(*replaced_inline, *replaced_block);
    return;
  }

  base::Optional<LayoutUnit> intrinsic_inline;
  base::Optional<LayoutUnit> intrinsic_block;
  node.IntrinsicSize(&intrinsic_inline, &intrinsic_block);

  LogicalSize aspect_ratio = node.GetAspectRatio();

  // Computing intrinsic size is complicated by the fact that
  // intrinsic_inline, intrinsic_block, and aspect_ratio can all
  // be empty independent of each other.
  // https://www.w3.org/TR/CSS22/visudet.html#inline-replaced-width
  if (intrinsic_inline)
    intrinsic_inline = *intrinsic_inline + border_padding.InlineSum();
  else if (aspect_ratio.IsEmpty())
    intrinsic_inline = LayoutUnit(300) + border_padding.InlineSum();
  if (intrinsic_block)
    intrinsic_block = *intrinsic_block + border_padding.BlockSum();
  else if (aspect_ratio.IsEmpty())
    intrinsic_block = LayoutUnit(150) + border_padding.BlockSum();
  if (!intrinsic_inline) {
    if (intrinsic_block) {
      intrinsic_inline =
          InlineSizeFromAspectRatio(border_padding, aspect_ratio,
                                    EBoxSizing::kContentBox, *intrinsic_block);
    } else if (!replaced_inline && !replaced_block) {
      // No sizes available, return only the aspect ratio.
      *out_aspect_ratio = aspect_ratio;
      return;
    }
  }
  if (intrinsic_inline && !intrinsic_block) {
    DCHECK(!aspect_ratio.IsEmpty());
    intrinsic_block =
        BlockSizeFromAspectRatio(border_padding, aspect_ratio,
                                 EBoxSizing::kContentBox, *intrinsic_inline);
  }

  DCHECK(intrinsic_inline || intrinsic_block || replaced_inline ||
         replaced_block);

  // If we only know one length, the other length gets computed wrt one we know.
  auto ComputeBlockFromInline = [&replaced_inline, &aspect_ratio,
                                 &border_padding](LayoutUnit default_block) {
    if (aspect_ratio.IsEmpty()) {
      DCHECK_GE(default_block, border_padding.BlockSum());
      return default_block;
    }
    return BlockSizeFromAspectRatio(border_padding, aspect_ratio,
                                    EBoxSizing::kContentBox, *replaced_inline);
  };

  auto ComputeInlineFromBlock = [&replaced_block, &aspect_ratio,
                                 &border_padding](LayoutUnit default_inline) {
    if (aspect_ratio.IsEmpty()) {
      DCHECK_GE(default_inline, border_padding.InlineSum());
      return default_inline;
    }
    return InlineSizeFromAspectRatio(border_padding, aspect_ratio,
                                     EBoxSizing::kContentBox, *replaced_block);
  };
  if (replaced_inline) {
    DCHECK(!replaced_block);
    DCHECK(intrinsic_block || !aspect_ratio.IsEmpty());
    replaced_block =
        ComputeBlockFromInline(intrinsic_block.value_or(kIndefiniteSize));
    replaced_block = ConstrainByMinMax(*replaced_block, block_min, block_max);
  } else if (replaced_block) {
    DCHECK(!replaced_inline);
    DCHECK(intrinsic_inline || !aspect_ratio.IsEmpty());
    replaced_inline =
        ComputeInlineFromBlock(intrinsic_inline.value_or(kIndefiniteSize));
    replaced_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
  } else {
    // If both lengths are unknown, they get defined by intrinsic values.
    DCHECK(!replaced_inline);
    DCHECK(!replaced_block);
    replaced_inline = *intrinsic_inline;
    replaced_block = *intrinsic_block;
    // If lengths are constrained, keep aspect ratio.
    // The side that shrank the most defines the other side.
    LayoutUnit constrained_inline =
        ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
    LayoutUnit constrained_block =
        ConstrainByMinMax(*replaced_block, block_min, block_max);
    if (constrained_inline != replaced_inline ||
        constrained_block != replaced_block) {
      LayoutUnit inline_ratio =
          (*replaced_inline - border_padding.InlineSum()) == LayoutUnit()
              ? LayoutUnit::Max()
              : (constrained_inline - border_padding.InlineSum()) /
                    (*replaced_inline - border_padding.InlineSum());
      LayoutUnit block_ratio =
          (*replaced_block - border_padding.BlockSum()) == LayoutUnit()
              ? LayoutUnit::Max()
              : (constrained_block - border_padding.BlockSum()) /
                    (*replaced_block - border_padding.BlockSum());

      // The following implements spec table from section 10.4 at
      // https://www.w3.org/TR/CSS22/visudet.html#min-max-widths
      // Translating specs to code:
      // inline_ratio < 1 => w > max_width
      // inline_ratio > 1 => w < min_width
      // block_ratio < 1 => h > max_height
      // block_ratio > 1 => h < min_height
      LayoutUnit one_unit(1);
      if (inline_ratio != one_unit || block_ratio != one_unit) {
        if ((inline_ratio < one_unit && block_ratio > one_unit) ||
            (inline_ratio > one_unit && block_ratio < one_unit)) {
          // Constraints caused us to grow in one dimension and shrink in the
          // other. Use both constrained sizes.
          replaced_inline = constrained_inline;
          replaced_block = constrained_block;
        } else if (block_ratio == one_unit ||
                   (inline_ratio < one_unit && inline_ratio <= block_ratio) ||
                   (inline_ratio > one_unit && inline_ratio >= block_ratio)) {
          // The inline size got constrained more extremely than the block size.
          // Use constrained inline size, re-calculate block size from aspect
          // ratio.
          replaced_inline = constrained_inline;
          replaced_block = ComputeBlockFromInline(constrained_block);
        } else {
          // The block size got constrained more extremely than the inline size.
          // Use constrained block size, re-calculate inline size from aspect
          // ratio.
          replaced_block = constrained_block;
          replaced_inline = ComputeInlineFromBlock(constrained_inline);
        }
      }
    }
  }
  *replaced_inline =
      ConstrainByMinMax(*replaced_inline, inline_min, inline_max);
  *replaced_block = ConstrainByMinMax(*replaced_block, block_min, block_max);
  out_replaced_size->emplace(*replaced_inline, *replaced_block);
}

int ResolveUsedColumnCount(int computed_count,
                           LayoutUnit computed_size,
                           LayoutUnit used_gap,
                           LayoutUnit available_size) {
  if (computed_size == kIndefiniteSize) {
    DCHECK(computed_count);
    return computed_count;
  }
  DCHECK_GT(computed_size, LayoutUnit());
  int count_from_width =
      ((available_size + used_gap) / (computed_size + used_gap)).ToInt();
  count_from_width = std::max(1, count_from_width);
  if (!computed_count)
    return count_from_width;
  return std::max(1, std::min(computed_count, count_from_width));
}

int ResolveUsedColumnCount(LayoutUnit available_size,
                           const ComputedStyle& style) {
  LayoutUnit computed_column_inline_size =
      style.HasAutoColumnWidth()
          ? kIndefiniteSize
          : std::max(LayoutUnit(1), LayoutUnit(style.ColumnWidth()));
  LayoutUnit gap = ResolveUsedColumnGap(available_size, style);
  int computed_count = style.HasAutoColumnCount() ? 0 : style.ColumnCount();
  return ResolveUsedColumnCount(computed_count, computed_column_inline_size,
                                gap, available_size);
}

LayoutUnit ResolveUsedColumnInlineSize(int computed_count,
                                       LayoutUnit computed_size,
                                       LayoutUnit used_gap,
                                       LayoutUnit available_size) {
  int used_count = ResolveUsedColumnCount(computed_count, computed_size,
                                          used_gap, available_size);
  return std::max(((available_size + used_gap) / used_count) - used_gap,
                  LayoutUnit());
}

LayoutUnit ResolveUsedColumnInlineSize(LayoutUnit available_size,
                                       const ComputedStyle& style) {
  // Should only attempt to resolve this if columns != auto.
  DCHECK(!style.HasAutoColumnCount() || !style.HasAutoColumnWidth());

  LayoutUnit computed_size =
      style.HasAutoColumnWidth()
          ? kIndefiniteSize
          : std::max(LayoutUnit(1), LayoutUnit(style.ColumnWidth()));
  int computed_count = style.HasAutoColumnCount() ? 0 : style.ColumnCount();
  LayoutUnit used_gap = ResolveUsedColumnGap(available_size, style);
  return ResolveUsedColumnInlineSize(computed_count, computed_size, used_gap,
                                     available_size);
}

LayoutUnit ResolveUsedColumnGap(LayoutUnit available_size,
                                const ComputedStyle& style) {
  if (const base::Optional<Length>& column_gap = style.ColumnGap())
    return ValueForLength(*column_gap, available_size);
  return LayoutUnit(style.GetFontDescription().ComputedPixelSize());
}

LayoutUnit ColumnInlineProgression(LayoutUnit available_size,
                                   const ComputedStyle& style) {
  LayoutUnit column_inline_size =
      ResolveUsedColumnInlineSize(available_size, style);
  return column_inline_size + ResolveUsedColumnGap(available_size, style);
}

NGPhysicalBoxStrut ComputePhysicalMargins(
    const ComputedStyle& style,
    LayoutUnit percentage_resolution_size) {
  if (!style.MayHaveMargin())
    return NGPhysicalBoxStrut();

  // This function may be called for determining intrinsic margins, clamp
  // indefinite %-sizes to zero. See:
  // https://drafts.csswg.org/css-sizing-3/#min-percentage-contribution
  percentage_resolution_size =
      percentage_resolution_size.ClampIndefiniteToZero();

  return {
      MinimumValueForLength(style.MarginTop(), percentage_resolution_size),
      MinimumValueForLength(style.MarginRight(), percentage_resolution_size),
      MinimumValueForLength(style.MarginBottom(), percentage_resolution_size),
      MinimumValueForLength(style.MarginLeft(), percentage_resolution_size)};
}

NGBoxStrut ComputeMarginsFor(const NGConstraintSpace& constraint_space,
                             const ComputedStyle& style,
                             const NGConstraintSpace& compute_for) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(compute_for.GetWritingDirection());
}

NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                NGLayoutInputNode child) {
  // An inline child just produces line-boxes which don't have any margins.
  if (child.IsInline() || !child.Style().MayHaveMargin())
    return NGBoxStrut();

  const Length& inline_start_margin_length =
      child.Style().MarginStartUsing(parent_style);
  const Length& inline_end_margin_length =
      child.Style().MarginEndUsing(parent_style);

  // TODO(ikilpatrick): We may want to re-visit calculated margins at some
  // point. Currently "margin-left: calc(10px + 50%)" will resolve to 0px, but
  // 10px would be more correct, (as percentages resolve to zero).
  NGBoxStrut margins;
  if (inline_start_margin_length.IsFixed())
    margins.inline_start = LayoutUnit(inline_start_margin_length.Value());
  if (inline_end_margin_length.IsFixed())
    margins.inline_end = LayoutUnit(inline_end_margin_length.Value());

  return margins;
}

namespace {

NGBoxStrut ComputeBordersInternal(const ComputedStyle& style) {
  NGBoxStrut borders;
  borders.inline_start = LayoutUnit(style.BorderStartWidth());
  borders.inline_end = LayoutUnit(style.BorderEndWidth());
  borders.block_start = LayoutUnit(style.BorderBeforeWidth());
  borders.block_end = LayoutUnit(style.BorderAfterWidth());
  return borders;
}

}  // namespace

NGBoxStrut ComputeBorders(const NGConstraintSpace& constraint_space,
                          const NGBlockNode& node) {
  // If we are producing an anonymous fragment (e.g. a column), it has no
  // borders, padding or scrollbars. Using the ones from the container can only
  // cause trouble.
  if (constraint_space.IsAnonymous())
    return NGBoxStrut();

  // If we are a table cell we just access the values set by the parent table
  // layout as border may be collapsed etc.
  if (constraint_space.IsTableCell())
    return constraint_space.TableCellBorders();

  if (node.IsNGTable())
    return To<NGTableNode>(node).GetTableBorders()->TableBorder();

  return ComputeBordersInternal(node.Style());
}

NGBoxStrut ComputeBordersForInline(const ComputedStyle& style) {
  return ComputeBordersInternal(style);
}

NGBoxStrut ComputeBordersForTest(const ComputedStyle& style) {
  return ComputeBordersInternal(style);
}

NGBoxStrut ComputeIntrinsicPadding(const NGConstraintSpace& constraint_space,
                                   const ComputedStyle& style,
                                   const NGBoxStrut& scrollbar) {
  DCHECK(constraint_space.IsTableCell());

  // During the "layout" table phase, adjust the given intrinsic-padding to
  // accommodate the scrollbar.
  NGBoxStrut intrinsic_padding = constraint_space.TableCellIntrinsicPadding();
  if (constraint_space.IsFixedBlockSize()) {
    if (style.VerticalAlign() == EVerticalAlign::kMiddle) {
      intrinsic_padding.block_start -= scrollbar.block_end / 2;
      intrinsic_padding.block_end -= scrollbar.block_end / 2;
    } else {
      intrinsic_padding.block_end -= scrollbar.block_end;
    }
  }

  return intrinsic_padding;
}

NGBoxStrut ComputePadding(const NGConstraintSpace& constraint_space,
                          const ComputedStyle& style) {
  // If we are producing an anonymous fragment (e.g. a column) we shouldn't
  // have any padding.
  if (!style.MayHavePadding() || constraint_space.IsAnonymous())
    return NGBoxStrut();

  // Tables with collapsed borders don't have any padding.
  if (style.IsDisplayTableBox() &&
      style.BorderCollapse() == EBorderCollapse::kCollapse) {
    return NGBoxStrut();
  }

  // This function may be called for determining intrinsic padding, clamp
  // indefinite %-sizes to zero. See:
  // https://drafts.csswg.org/css-sizing-3/#min-percentage-contribution
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode()
          .ClampIndefiniteToZero();
  NGBoxStrut padding = {
      MinimumValueForLength(style.PaddingStart(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingEnd(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingBefore(), percentage_resolution_size),
      MinimumValueForLength(style.PaddingAfter(), percentage_resolution_size)};

  if (!RuntimeEnabledFeatures::LayoutNGTableEnabled() &&
      style.Display() == EDisplay::kTableCell) {
    // Compatibility hack to mach legacy layout. Legacy layout floors padding on
    // the block sides, but not on the inline sides. o.O
    padding.block_start = LayoutUnit(padding.block_start.Floor());
    padding.block_end = LayoutUnit(padding.block_end.Floor());
  }

  return padding;
}

NGBoxStrut ComputeScrollbarsForNonAnonymous(const NGBlockNode& node) {
  const ComputedStyle& style = node.Style();
  if (!style.IsScrollContainer() && style.IsScrollbarGutterAuto())
    return NGBoxStrut();
  const LayoutBox* layout_box = node.GetLayoutBox();
  return layout_box->ComputeLogicalScrollbars();
}

bool NeedsInlineSizeToResolveLineLeft(const ComputedStyle& style,
                                      const ComputedStyle& container_style) {
  // In RTL, there's no block alignment where we can guarantee that line-left
  // doesn't depend on the inline size of a fragment.
  if (IsRtl(container_style.Direction()))
    return true;

  return BlockAlignment(style, container_style) != EBlockAlignment::kStart;
}

void ResolveInlineMargins(const ComputedStyle& style,
                          const ComputedStyle& container_style,
                          LayoutUnit available_inline_size,
                          LayoutUnit inline_size,
                          NGBoxStrut* margins) {
  DCHECK(margins) << "Margins cannot be NULL here";
  const LayoutUnit used_space = inline_size + margins->InlineSum();
  const LayoutUnit available_space = available_inline_size - used_space;
  if (available_space > LayoutUnit()) {
    EBlockAlignment alignment = BlockAlignment(style, container_style);
    if (alignment == EBlockAlignment::kCenter)
      margins->inline_start += available_space / 2;
    else if (alignment == EBlockAlignment::kEnd)
      margins->inline_start += available_space;
  }
  margins->inline_end =
      available_inline_size - inline_size - margins->inline_start;
}

LayoutUnit LineOffsetForTextAlign(ETextAlign text_align,
                                  TextDirection direction,
                                  LayoutUnit space_left) {
  bool is_ltr = IsLtr(direction);
  if (text_align == ETextAlign::kStart || text_align == ETextAlign::kJustify)
    text_align = is_ltr ? ETextAlign::kLeft : ETextAlign::kRight;
  else if (text_align == ETextAlign::kEnd)
    text_align = is_ltr ? ETextAlign::kRight : ETextAlign::kLeft;

  switch (text_align) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft: {
      // The direction of the block should determine what happens with wide
      // lines. In particular with RTL blocks, wide lines should still spill
      // out to the left.
      if (is_ltr)
        return LayoutUnit();
      return space_left.ClampPositiveToZero();
    }
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight: {
      // In RTL, trailing spaces appear on the left of the line.
      if (UNLIKELY(!is_ltr))
        return space_left;
      // Wide lines spill out of the block based off direction.
      // So even if text-align is right, if direction is LTR, wide lines
      // should overflow out of the right side of the block.
      if (space_left > LayoutUnit())
        return space_left;
      return LayoutUnit();
    }
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter: {
      if (is_ltr)
        return (space_left / 2).ClampNegativeToZero();
      // In RTL, trailing spaces appear on the left of the line.
      if (space_left > LayoutUnit())
        return (space_left / 2).ClampNegativeToZero();
      // In RTL, wide lines should spill out to the left, same as kRight.
      return space_left;
    }
    default:
      NOTREACHED();
      return LayoutUnit();
  }
}

// Calculates default content size for html and body elements in quirks mode.
// Returns |kIndefiniteSize| in all other cases.
LayoutUnit CalculateDefaultBlockSize(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_scrollbar_padding) {
  // In quirks mode, html and body elements will completely fill the ICB, block
  // percentages should resolve against this size.
  if (node.IsQuirkyAndFillsViewport()) {
    LayoutUnit block_size = space.AvailableSize().block_size;
    block_size -= ComputeMarginsForSelf(space, node.Style()).BlockSum();
    return std::max(block_size.ClampNegativeToZero(),
                    border_scrollbar_padding.BlockSum());
  }
  return kIndefiniteSize;
}

namespace {

// Clamp the inline size of the scrollbar, unless it's larger than the inline
// size of the content box, in which case we'll return that instead. Scrollbar
// handling is quite bad in such situations, and this method here is just to
// make sure that left-hand scrollbars don't mess up scrollWidth. For the full
// story, visit http://crbug.com/724255.
bool ClampScrollbarToContentBox(NGBoxStrut* scrollbars,
                                LayoutUnit content_box_inline_size) {
  DCHECK(scrollbars->InlineSum());
  if (scrollbars->InlineSum() <= content_box_inline_size)
    return false;
  if (scrollbars->inline_end) {
    DCHECK(!scrollbars->inline_start);
    scrollbars->inline_end = content_box_inline_size;
  } else {
    DCHECK(scrollbars->inline_start);
    scrollbars->inline_start = content_box_inline_size;
  }
  return true;
}

}  // namespace

NGFragmentGeometry CalculateInitialFragmentGeometry(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode& node,
    bool is_intrinsic) {
  DCHECK(is_intrinsic || node.CanUseNewLayout());
  const ComputedStyle& style = node.Style();
  NGBoxStrut border = ComputeBorders(constraint_space, node);
  NGBoxStrut padding = ComputePadding(constraint_space, style);
  NGBoxStrut scrollbar = ComputeScrollbars(constraint_space, node);
  NGBoxStrut border_padding = border + padding;
  NGBoxStrut border_scrollbar_padding = border_padding + scrollbar;

  // If we have a percentage size, we need to set the
  // HasPercentHeightDescendants flag correctly so that flexbox knows it may
  // need to redo layout and can also do some performance optimizations.
  if (style.LogicalHeight().IsPercentOrCalc() ||
      style.LogicalMinHeight().IsPercentOrCalc() ||
      style.LogicalMaxHeight().IsPercentOrCalc() ||
      style.LogicalTop().IsPercentOrCalc() ||
      style.LogicalBottom().IsPercentOrCalc() ||
      (node.IsFlexItem() && style.FlexBasis().IsPercentOrCalc())) {
    // This call has the side-effect of setting HasPercentHeightDescendants
    // correctly.
    node.GetLayoutBox()->ComputePercentageLogicalHeight(Length::Percent(0));
  }

  // TODO(ikilpatrick): This check shouldn't be needed. |ComputeReplacedSize|
  // should be able to determine the correct size without the
  // |NGBlockSize::ComputeMinMaxSizes| call.
  if (!is_intrinsic && node.IsReplaced()) {
    LogicalSize border_box_size;
    MinMaxSizes intrinsic_min_max_sizes =
        node.ComputeMinMaxSizes(constraint_space.GetWritingMode(),
                                MinMaxSizesType::kIntrinsic, constraint_space)
            .sizes;
    base::Optional<LogicalSize> replaced_size;
    base::Optional<LogicalSize> aspect_ratio;
    ComputeReplacedSize(node, constraint_space, intrinsic_min_max_sizes,
                        &replaced_size, &aspect_ratio);
    bool has_aspect_ratio_without_intrinsic_size =
        !replaced_size && aspect_ratio && !aspect_ratio->IsEmpty();
    if (has_aspect_ratio_without_intrinsic_size) {
      // TODO(dgrogan): This doesn't obey min/max-width or transferred
      // min/max-height or margins. Maybe use ResolveInlineLength with
      // fill-available, then clamp with ComputeMinMaxInlineSizes after adapting
      // it for Replaced elements. Or use ComputeInlineSizeForFragment with a
      // ConstraintSpace with StretchInlineSizeIfAuto.
      DCHECK_GE(constraint_space.AvailableSize().inline_size, LayoutUnit());
      border_box_size.inline_size =
          std::max(constraint_space.AvailableSize().inline_size,
                   border_padding.InlineSum());
      border_box_size.block_size = BlockSizeFromAspectRatio(
          border_padding, *aspect_ratio, EBoxSizing::kContentBox,
          border_box_size.inline_size);
    } else {
      DCHECK(replaced_size.has_value())
          << "Images should have at least one of aspect_ratio or replaced_size";
      border_box_size = *replaced_size;
    }
    return {border_box_size, border, scrollbar, padding};
  }

  LayoutUnit default_block_size = CalculateDefaultBlockSize(
      constraint_space, node, border_scrollbar_padding);
  base::Optional<LayoutUnit> inline_size;
  if (!is_intrinsic) {
    inline_size =
        ComputeInlineSizeForFragment(constraint_space, node, border_padding);

    if (UNLIKELY(*inline_size < border_scrollbar_padding.InlineSum() &&
                 scrollbar.InlineSum() && !constraint_space.IsAnonymous())) {
      ClampScrollbarToContentBox(&scrollbar,
                                 *inline_size - border_padding.InlineSum());
    }
  }

  LayoutUnit block_size = ComputeInitialBlockSizeForFragment(
      constraint_space, style, border_padding, default_block_size, inline_size);

  return {LogicalSize(inline_size.value_or(kIndefiniteSize), block_size),
          border, scrollbar, padding};
}

LogicalSize ShrinkLogicalSize(LogicalSize size, const NGBoxStrut& insets) {
  if (size.inline_size != kIndefiniteSize) {
    size.inline_size =
        (size.inline_size - insets.InlineSum()).ClampNegativeToZero();
  }
  if (size.block_size != kIndefiniteSize) {
    size.block_size =
        (size.block_size - insets.BlockSum()).ClampNegativeToZero();
  }

  return size;
}

LogicalSize CalculateChildAvailableSize(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const LogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding) {
  LogicalSize child_available_size =
      ShrinkLogicalSize(border_box_size, border_scrollbar_padding);

  if (space.IsAnonymous() || node.IsAnonymousBlock())
    child_available_size.block_size = space.AvailableSize().block_size;

  return child_available_size;
}

namespace {

// Implements the common part of the child percentage size calculation. Deals
// with how percentages are propagated from parent to child in quirks mode.
LogicalSize AdjustChildPercentageSize(const NGConstraintSpace& space,
                                      const NGBlockNode node,
                                      LogicalSize child_percentage_size,
                                      LayoutUnit parent_percentage_block_size) {
  // Flex items may have a fixed block-size, but children shouldn't resolve
  // their percentages against this.
  if (space.IsFixedBlockSizeIndefinite()) {
    DCHECK(node.IsFlexItem() || space.IsTableCell());
    child_percentage_size.block_size = kIndefiniteSize;
    return child_percentage_size;
  }

  bool is_table_cell_in_measure_phase =
      space.IsTableCell() && !space.IsFixedBlockSize();
  // A table-cell during the "measure" phase forces its descendants to have an
  // indefinite percentage resolution size.
  // NOTE: If the Layout and ComputeMinMaxSizes ever get merged, this can be
  // removed (as we'll need to allow for indefinite %-inline-sizes).
  if (is_table_cell_in_measure_phase) {
    // Orthogonal cells need to call layout on the cell to determine
    // size of the table. Because table's inline size is unknown, percentages
    // are resolved against 0.
    if (space.IsOrthogonalWritingModeRoot())
      child_percentage_size.block_size = LayoutUnit();
    else
      child_percentage_size.block_size = kIndefiniteSize;
    return child_percentage_size;
  }

  // In quirks mode the percentage resolution height is passed from parent to
  // child.
  // https://quirks.spec.whatwg.org/#the-percentage-height-calculation-quirk
  if (child_percentage_size.block_size == kIndefiniteSize &&
      node.UseParentPercentageResolutionBlockSizeForChildren())
    child_percentage_size.block_size = parent_percentage_block_size;

  return child_percentage_size;
}

}  // namespace

LogicalSize CalculateChildPercentageSize(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    const LogicalSize child_available_size) {
  // Anonymous block or spaces should pass the percent size straight through.
  if (space.IsAnonymous() || node.IsAnonymousBlock())
    return space.PercentageResolutionSize();

  // Table cell children don't apply the "percentage-quirk". I.e. if their
  // percentage resolution block-size is indefinite, they don't pass through
  // their parent's percentage resolution block-size.
  if (space.IsTableCellChild())
    return child_available_size;

  return AdjustChildPercentageSize(space, node, child_available_size,
                                   space.PercentageResolutionBlockSize());
}

LogicalSize CalculateReplacedChildPercentageSize(
    const NGConstraintSpace& space,
    const NGBlockNode node,
    const LogicalSize child_available_size,
    const NGBoxStrut& border_scrollbar_padding,
    const NGBoxStrut& border_padding) {
  // Anonymous block or spaces should pass the percent size straight through.
  if (space.IsAnonymous() || node.IsAnonymousBlock())
    return space.ReplacedPercentageResolutionSize();

  // Replaced descendants of a table-cell which has a definite block-size,
  // always resolve their percentages against this size (even during the
  // "layout" pass where the fixed block-size may be different).
  //
  // This ensures that between the table-cell "measure" and "layout" passes
  // the replaced descendants remain the same size.
  const ComputedStyle& style = node.Style();
  if (space.IsTableCell() && style.LogicalHeight().IsFixed()) {
    LayoutUnit block_size = ComputeBlockSizeForFragmentInternal(
        space, style, border_padding, kIndefiniteSize /* intrinsic_size */,
        base::nullopt /* inline_size */);
    DCHECK_NE(block_size, kIndefiniteSize);
    return {child_available_size.inline_size,
            (block_size - border_scrollbar_padding.BlockSum())
                .ClampNegativeToZero()};
  }

  return AdjustChildPercentageSize(
      space, node, child_available_size,
      space.ReplacedPercentageResolutionBlockSize());
}

LayoutUnit ClampIntrinsicBlockSize(
    const NGConstraintSpace& space,
    const NGBlockNode& node,
    const NGBoxStrut& border_scrollbar_padding,
    LayoutUnit current_intrinsic_block_size,
    base::Optional<LayoutUnit> body_margin_block_sum) {
  // Tables don't respect size containment, or apply the "fill viewport" quirk.
  DCHECK(!node.IsTable());
  const ComputedStyle& style = node.Style();

  // Apply the "fills viewport" quirk if needed.
  LayoutUnit available_block_size = space.AvailableSize().block_size;
  if (node.IsQuirkyAndFillsViewport() && style.LogicalHeight().IsAuto() &&
      available_block_size != kIndefiniteSize) {
    DCHECK_EQ(node.IsBody() && !node.CreatesNewFormattingContext(),
              body_margin_block_sum.has_value());
    LayoutUnit margin_sum = body_margin_block_sum.value_or(
        ComputeMarginsForSelf(space, style).BlockSum());
    current_intrinsic_block_size = std::max(
        current_intrinsic_block_size,
        (space.AvailableSize().block_size - margin_sum).ClampNegativeToZero());
  }

  // If the intrinsic size was overridden, then use that.
  LayoutUnit intrinsic_size_override = node.OverrideIntrinsicContentBlockSize();
  if (intrinsic_size_override != kIndefiniteSize) {
    return intrinsic_size_override + border_scrollbar_padding.BlockSum();
  } else {
    LayoutUnit default_intrinsic_size = node.DefaultIntrinsicContentBlockSize();
    if (default_intrinsic_size != kIndefiniteSize) {
      // <textarea>'s intrinsic size should ignore scrollbar existence.
      if (node.IsTextArea()) {
        return default_intrinsic_size + border_scrollbar_padding.BlockSum() -
               ComputeScrollbars(space, node).BlockSum();
      }
      return default_intrinsic_size + border_scrollbar_padding.BlockSum();
    }
  }

  // If we have size containment, we ignore child contributions to intrinsic
  // sizing.
  if (node.ShouldApplyBlockSizeContainment())
    return border_scrollbar_padding.BlockSum();
  return current_intrinsic_block_size;
}

base::Optional<MinMaxSizesResult> CalculateMinMaxSizesIgnoringChildren(
    const NGBlockNode& node,
    const NGBoxStrut& border_scrollbar_padding) {
  MinMaxSizes sizes;
  sizes += border_scrollbar_padding.InlineSum();

  // If intrinsic size was overridden, then use that.
  const LayoutUnit intrinsic_size_override =
      node.OverrideIntrinsicContentInlineSize();
  if (intrinsic_size_override != kIndefiniteSize) {
    sizes += intrinsic_size_override;
    return MinMaxSizesResult{sizes,
                             /* depends_on_block_constraints */ false};
  } else {
    LayoutUnit default_inline_size = node.DefaultIntrinsicContentInlineSize();
    if (default_inline_size != kIndefiniteSize) {
      sizes += default_inline_size;
      // <textarea>'s intrinsic size should ignore scrollbar existence.
      if (node.IsTextArea())
        sizes -= ComputeScrollbarsForNonAnonymous(node).InlineSum();
      return MinMaxSizesResult{sizes,
                               /* depends_on_block_constraints */ false};
    }
  }

  // Size contained elements don't consider children for intrinsic sizing.
  // Also, if we don't have children, we can determine the size immediately.
  if (node.ShouldApplyInlineSizeContainment() || !node.FirstChild()) {
    return MinMaxSizesResult{sizes,
                             /* depends_on_block_constraints */ false};
  }

  return base::nullopt;
}

}  // namespace blink
