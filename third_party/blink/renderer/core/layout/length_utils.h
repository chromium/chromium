// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LENGTH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LENGTH_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/table/table_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class ComputedStyle;
class ConstraintSpace;
class Length;

inline bool NeedMinMaxSize(const ComputedStyle& style) {
  return style.LogicalWidth().IsContentOrIntrinsic() ||
         style.LogicalMinWidth().IsContentOrIntrinsic() ||
         style.LogicalMaxWidth().IsContentOrIntrinsic();
}

CORE_EXPORT LayoutUnit
InlineSizeFromAspectRatio(const BoxStrut& border_padding,
                          const LogicalSize& aspect_ratio,
                          EBoxSizing box_sizing,
                          LayoutUnit block_size);
LayoutUnit BlockSizeFromAspectRatio(const BoxStrut& border_padding,
                                    const LogicalSize& aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size);

// Returns if the given |Length| is unresolvable, e.g. the length is %-based
// and resolving against an indefinite size. For block lengths we also consider
// 'auto', 'min-content', 'max-content', 'fit-content' and 'none' (for
// max-block-size) as unresolvable.
CORE_EXPORT bool InlineLengthUnresolvable(const ConstraintSpace&,
                                          const Length&);
CORE_EXPORT bool BlockLengthUnresolvable(
    const ConstraintSpace&,
    const Length&,
    const LayoutUnit* override_percentage_resolution_size = nullptr);

// Resolve means translate a Length to a LayoutUnit.
//  - |ConstraintSpace| the information given by the parent, e.g. the
//    available-size.
//  - |ComputedStyle| the style of the node.
//  - |border_padding| the resolved border, and padding of the node.
//  - |MinMaxSizes| is only used when the length is intrinsic (fit-content).
//  - |Length| is the length to resolve.
//  - |override_available_size| overrides the available-size. This is used when
//    computing the size of an OOF-positioned element, accounting for insets
//    and the static position.
CORE_EXPORT LayoutUnit ResolveInlineLengthInternal(
    const ConstraintSpace&,
    const ComputedStyle&,
    const BoxStrut& border_padding,
    const absl::optional<MinMaxSizes>&,
    const Length&,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr);

// Same as ResolveInlineLengthInternal, except here |intrinsic_size| roughly
// plays the part of |MinMaxSizes|.
CORE_EXPORT LayoutUnit ResolveBlockLengthInternal(
    const ConstraintSpace&,
    const ComputedStyle&,
    const BoxStrut& border_padding,
    const Length&,
    LayoutUnit intrinsic_size,
    LayoutUnit override_available_size = kIndefiniteSize,
    const LayoutUnit* override_percentage_resolution_size = nullptr,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr);

// In this file the template parameter MinMaxSizesFunc should have the
// following form:
//
// auto MinMaxSizesFunc = [](MinMaxSizesType) -> MinMaxSizesResult { };
//
// This is used for computing the min/max content or intrinsic sizes on-demand
// rather than determining if a length resolving function will require these
// sizes ahead of time.

// Used for resolving min inline lengths, (|ComputedStyle::MinLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMinInlineLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  if (LIKELY(length.IsAuto() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return border_padding.InlineSum();

  absl::optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length,
                                     override_available_size, anchor_evaluator);
}

// Used for resolving max inline lengths, (|ComputedStyle::MaxLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMaxInlineLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  if (LIKELY(length.IsNone() ||
             InlineLengthUnresolvable(constraint_space, length)))
    return LayoutUnit::Max();

  absl::optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length,
                                     override_available_size, anchor_evaluator);
}

// Used for resolving main inline lengths, (|ComputedStyle::LogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMainInlineLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  DCHECK(!length.IsAuto());
  absl::optional<MinMaxSizes> min_max_sizes;
  if (length.IsContentOrIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length,
                                     override_available_size, anchor_evaluator);
}

// Used for resolving min block lengths, (|ComputedStyle::MinLogicalHeight|).
inline LayoutUnit ResolveMinBlockLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const Length& length,
    LayoutUnit override_available_size = kIndefiniteSize,
    const LayoutUnit* override_percentage_resolution_size = nullptr,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  if (LIKELY(BlockLengthUnresolvable(constraint_space, length,
                                     override_percentage_resolution_size)))
    return border_padding.BlockSum();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      override_available_size, override_percentage_resolution_size,
      anchor_evaluator);
}

// Used for resolving max block lengths, (|ComputedStyle::MaxLogicalHeight|).
inline LayoutUnit ResolveMaxBlockLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const Length& length,
    LayoutUnit override_available_size = kIndefiniteSize,
    const LayoutUnit* override_percentage_resolution_size = nullptr,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  if (LIKELY(BlockLengthUnresolvable(constraint_space, length,
                                     override_percentage_resolution_size)))
    return LayoutUnit::Max();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      override_available_size, override_percentage_resolution_size,
      anchor_evaluator);
}

// Used for resolving main block lengths, (|ComputedStyle::LogicalHeight|).
inline LayoutUnit ResolveMainBlockLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const Length& length,
    LayoutUnit intrinsic_size,
    LayoutUnit override_available_size = kIndefiniteSize,
    const LayoutUnit* override_percentage_resolution_size = nullptr,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  DCHECK(!length.IsAuto());
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(constraint_space, length,
                                       override_percentage_resolution_size)))
    return intrinsic_size;

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, intrinsic_size,
      override_available_size, override_percentage_resolution_size,
      anchor_evaluator);
}

template <typename IntrinsicBlockSizeFunc>
inline LayoutUnit ResolveMainBlockLength(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const BoxStrut& border_padding,
    const Length& length,
    const IntrinsicBlockSizeFunc& intrinsic_block_size_func,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  DCHECK(!length.IsAuto());
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(constraint_space, length)))
    return intrinsic_block_size_func();

  LayoutUnit intrinsic_block_size = kIndefiniteSize;
  if (length.IsContentOrIntrinsic())
    intrinsic_block_size = intrinsic_block_size_func();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, intrinsic_block_size,
      override_available_size,
      /* override_percentage_resolution_size */ nullptr, anchor_evaluator);
}

// For the given |child|, computes the min and max content contribution
// (https://drafts.csswg.org/css-sizing/#contributions).
//
// This is similar to ComputeInlineSizeForFragment except that it does not
// require a constraint space (percentage sizes as well as auto margins compute
// to zero) and an auto inline-size resolves to the respective min/max content
// size.
//
// Additoinally, the min/max contribution includes the inline margins. Because
// content contributions are commonly needed by a block's parent, we also take
// a writing-mode here so we can compute this in the parent's coordinate system.
//
// Note that if the writing mode of the child is orthogonal to that of the
// parent, we'll still return the inline min/max contribution in the writing
// mode of the parent (i.e. typically something based on the preferred *block*
// size of the child).
MinMaxSizesResult ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    const BlockNode& child,
    const ConstraintSpace& space,
    const MinMaxSizesFloatInput float_input = MinMaxSizesFloatInput());

// Similar to |ComputeMinAndMaxContentContribution| but ignores the parent
// writing-mode, and instead computes the contribution relative to |child|'s
// own writing-mode.
MinMaxSizesResult ComputeMinAndMaxContentContributionForSelf(
    const BlockNode& child,
    const ConstraintSpace& space);

// Used for unit-tests.
CORE_EXPORT MinMaxSizes
ComputeMinAndMaxContentContributionForTest(WritingMode writing_mode,
                                           const BlockNode&,
                                           const ConstraintSpace&,
                                           const MinMaxSizes&);

// Computes the min-block-size and max-block-size values for a node.
MinMaxSizes ComputeMinMaxBlockSizes(
    const ConstraintSpace&,
    const ComputedStyle&,
    const BoxStrut& border_padding,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr);

MinMaxSizes ComputeTransferredMinMaxInlineSizes(
    const LogicalSize& ratio,
    const MinMaxSizes& block_min_max,
    const BoxStrut& border_padding,
    const EBoxSizing sizing);
MinMaxSizes ComputeTransferredMinMaxBlockSizes(const LogicalSize& ratio,
                                               const MinMaxSizes& block_min_max,
                                               const BoxStrut& border_padding,
                                               const EBoxSizing sizing);

// Computes the transferred min/max inline sizes from the min/max block
// sizes and the aspect ratio.
// This will compute the min/max block sizes for you, but it only works with
// styles that have a LogicalAspectRatio. It doesn't work if the aspect ratio is
// coming from a replaced element.
CORE_EXPORT MinMaxSizes
ComputeMinMaxInlineSizesFromAspectRatio(const ConstraintSpace&,
                                        const ComputedStyle&,
                                        const BoxStrut& border_padding);

template <typename MinMaxSizesFunc>
MinMaxSizes ComputeMinMaxInlineSizes(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length* opt_min_length = nullptr,
    LayoutUnit override_available_size = kIndefiniteSize,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr) {
  const ComputedStyle& style = node.Style();
  const Length& min_length =
      opt_min_length ? *opt_min_length : style.LogicalMinWidth();
  MinMaxSizes sizes = {
      ResolveMinInlineLength(space, style, border_padding, min_max_sizes_func,
                             min_length, override_available_size,
                             anchor_evaluator),
      ResolveMaxInlineLength(space, style, border_padding, min_max_sizes_func,
                             style.LogicalMaxWidth(), override_available_size,
                             anchor_evaluator)};

  // This implements the transferred min/max sizes per:
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-size-transfers
  if (!style.AspectRatio().IsAuto() && style.LogicalWidth().IsAuto() &&
      space.InlineAutoBehavior() != AutoSizeBehavior::kStretchExplicit) {
    MinMaxSizes transferred_sizes =
        ComputeMinMaxInlineSizesFromAspectRatio(space, style, border_padding);
    sizes.min_size = std::max(
        sizes.min_size, std::min(transferred_sizes.min_size, sizes.max_size));
    sizes.max_size = std::min(sizes.max_size, transferred_sizes.max_size);
  }

  if (node.IsTable()) {
    // Tables can't shrink below their inline min-content size.
    sizes.Encompass(
        min_max_sizes_func(MinMaxSizesType::kIntrinsic).sizes.min_size);
  }

  sizes.max_size = std::max(sizes.max_size, sizes.min_size);
  return sizes;
}

// Returns block size of the node's border box by resolving the computed value
// in `style.logicalHeight` to a `LayoutUnit`, adding border and padding, then
// constraining the result by the resolved min and max logical height from the
// `ComputedStyle` object.
//
// `inline_size` is necessary when an aspect ratio is in use.
// `override_available_size` is needed for <table> layout: when a table is under
// an extrinsic constraint (e.g., being stretched by its parent, or forced to a
// fixed block-size), we need to subtract the block size of all the <caption>
// elements from the available block size.
CORE_EXPORT LayoutUnit ComputeBlockSizeForFragment(
    const ConstraintSpace&,
    const ComputedStyle&,
    const BoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    absl::optional<LayoutUnit> inline_size,
    LayoutUnit override_available_size = kIndefiniteSize);

CORE_EXPORT LayoutUnit
ComputeInlineSizeFromAspectRatio(const ConstraintSpace& space,
                                 const ComputedStyle& style,
                                 const BoxStrut& border_padding);

template <typename MinMaxSizesFunc>
LayoutUnit ComputeInlineSizeForFragmentInternal(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func) {
  const auto& style = node.Style();

  auto extent = kIndefiniteSize;
  auto logical_width = style.LogicalWidth();
  auto min_length = style.LogicalMinWidth();

  if (!style.AspectRatio().IsAuto() &&
      ((logical_width.IsAuto() &&
        space.InlineAutoBehavior() != AutoSizeBehavior::kStretchExplicit) ||
       logical_width.IsMinContent() || logical_width.IsMaxContent())) {
    extent = ComputeInlineSizeFromAspectRatio(space, style, border_padding);

    if (extent != kIndefiniteSize) {
      // This means we successfully applied aspect-ratio and now need to check
      // if we need to apply the implied minimum size:
      // https://drafts.csswg.org/css-sizing-4/#aspect-ratio-minimum
      if (style.OverflowInlineDirection() == EOverflow::kVisible &&
          min_length.IsAuto()) {
        min_length = Length::MinIntrinsic();
      }
    }
  }

  if (LIKELY(extent == kIndefiniteSize)) {
    if (logical_width.IsAuto()) {
      if (space.AvailableSize().inline_size == kIndefiniteSize) {
        logical_width = Length::MinContent();
      } else if (space.IsInlineAutoBehaviorStretch()) {
        logical_width = Length::FillAvailable();
      } else {
        logical_width = Length::FitContent();
      }
    }
    extent = ResolveMainInlineLength(space, style, border_padding,
                                     min_max_sizes_func, logical_width);
  }

  return ComputeMinMaxInlineSizes(space, node, border_padding,
                                  min_max_sizes_func, &min_length)
      .ClampSizeToMinAndMax(extent);
}

template <typename MinMaxSizesFunc>
LayoutUnit ComputeInlineSizeForFragment(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func) {
  if (space.IsFixedInlineSize() || space.IsAnonymous()) {
    return space.AvailableSize().inline_size;
  }

  if (node.IsTable()) {
    return To<TableNode>(node).ComputeTableInlineSize(space, border_padding);
  }

  return ComputeInlineSizeForFragmentInternal(space, node, border_padding,
                                              min_max_sizes_func);
}

// Returns inline size of the node's border box by resolving the computed value
// in `style.logicalWidth` to a `LayoutUnit`, adding border and padding, then
// constraining the result by the resolved min and max logical width from the
// `ComputedStyle` object. Calls `ComputeMinMaxSizes` if needed.
//
// `override_min_max_sizes_for_test` is provided *solely* for use by unit tests.
inline LayoutUnit ComputeInlineSizeForFragment(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const MinMaxSizes* override_min_max_sizes_for_test = nullptr) {
  auto MinMaxSizesFunc = [&](MinMaxSizesType type) -> MinMaxSizesResult {
    if (UNLIKELY(override_min_max_sizes_for_test)) {
      return MinMaxSizesResult(*override_min_max_sizes_for_test,
                               /* depends_on_block_constraints */ false);
    }
    return node.ComputeMinMaxSizes(space.GetWritingMode(), type, space);
  };

  return ComputeInlineSizeForFragment(space, node, border_padding,
                                      MinMaxSizesFunc);
}

// Similar to |ComputeInlineSizeForFragment| but for determining the "used"
// inline-size for a table fragment. See:
// https://drafts.csswg.org/css-tables-3/#used-width-of-table
CORE_EXPORT LayoutUnit ComputeUsedInlineSizeForTableFragment(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BoxStrut& border_padding,
    const MinMaxSizes& table_grid_min_max_sizes);

LayoutUnit ComputeInitialBlockSizeForFragment(
    const ConstraintSpace&,
    const ComputedStyle&,
    const BoxStrut& border_padding,
    LayoutUnit intrinsic_size,
    absl::optional<LayoutUnit> inline_size,
    LayoutUnit override_available_size = kIndefiniteSize);

// Calculates default content size for html and body elements in quirks mode.
// Returns |kIndefiniteSize| in all other cases.
CORE_EXPORT LayoutUnit
CalculateDefaultBlockSize(const ConstraintSpace& space,
                          const BlockNode& node,
                          const BlockBreakToken* break_token,
                          const BoxStrut& border_scrollbar_padding);

// Flex layout is interested in ignoring lengths in a particular axis. This
// enum is used to control this behaviour.
enum class ReplacedSizeMode {
  kNormal,
  kIgnoreInlineLengths,  // Used for determining the min/max content size.
  kIgnoreBlockLengths    // Used for determining the "intrinsic" block-size.
};

// Computes the size for a replaced element. See:
// https://www.w3.org/TR/CSS2/visudet.html#inline-replaced-width
// https://www.w3.org/TR/CSS2/visudet.html#inline-replaced-height
// https://www.w3.org/TR/CSS22/visudet.html#min-max-widths
// https://drafts.csswg.org/css-sizing-3/#intrinsic-sizes
//
// This will handle both intrinsic, and layout calculations depending on the
// space provided. (E.g. if the available inline-size is indefinite it will
// return the intrinsic size).
CORE_EXPORT LogicalSize ComputeReplacedSize(
    const BlockNode&,
    const ConstraintSpace&,
    const BoxStrut& border_padding,
    absl::optional<LogicalSize> override_available_size = absl::nullopt,
    ReplacedSizeMode = ReplacedSizeMode::kNormal,
    const Length::AnchorEvaluator* anchor_evaluator = nullptr);

// Based on available inline size, CSS computed column-width, CSS computed
// column-count and CSS used column-gap, return CSS used column-count.
// If computed column-count is auto, pass 0 as |computed_count|.
CORE_EXPORT int ResolveUsedColumnCount(int computed_count,
                                       LayoutUnit computed_size,
                                       LayoutUnit used_gap,
                                       LayoutUnit available_size);
CORE_EXPORT int ResolveUsedColumnCount(LayoutUnit available_size,
                                       const ComputedStyle&);

// Based on available inline size, CSS computed column-width, CSS computed
// column-count and CSS used column-gap, return CSS used column-width.
CORE_EXPORT LayoutUnit ResolveUsedColumnInlineSize(int computed_count,
                                                   LayoutUnit computed_size,
                                                   LayoutUnit used_gap,
                                                   LayoutUnit available_size);
CORE_EXPORT LayoutUnit ResolveUsedColumnInlineSize(LayoutUnit available_size,
                                                   const ComputedStyle&);

CORE_EXPORT LayoutUnit ResolveUsedColumnGap(LayoutUnit available_size,
                                            const ComputedStyle&);

CORE_EXPORT LayoutUnit ColumnInlineProgression(LayoutUnit available_size,
                                               const ComputedStyle&);
// Compute physical margins.
CORE_EXPORT PhysicalBoxStrut
ComputePhysicalMargins(const ComputedStyle&,
                       LayoutUnit percentage_resolution_size);

inline PhysicalBoxStrut ComputePhysicalMargins(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size);
}

// Compute margins for the specified ConstraintSpace.
CORE_EXPORT BoxStrut ComputeMarginsFor(const ConstraintSpace&,
                                       const ComputedStyle&,
                                       const ConstraintSpace& compute_for);

inline BoxStrut ComputeMarginsFor(
    const ComputedStyle& style,
    LayoutUnit percentage_resolution_size,
    WritingDirectionMode container_writing_direction) {
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(container_writing_direction);
}

inline BoxStrut ComputeMarginsFor(
    const ConstraintSpace& space,
    const ComputedStyle& style,
    WritingDirectionMode container_writing_direction) {
  return ComputePhysicalMargins(space, style)
      .ConvertToLogical(container_writing_direction);
}

// Compute margins for the style owner.
inline BoxStrut ComputeMarginsForSelf(const ConstraintSpace& constraint_space,
                                      const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return BoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(style.GetWritingDirection());
}

// Compute line logical margins for the style owner.
//
// The "line" versions compute line-relative logical values. See LineBoxStrut
// for more details.
inline LineBoxStrut ComputeLineMarginsForSelf(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return LineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(style.GetWritingDirection());
}

// Compute line logical margins for the constraint space, in the visual order
// (always assumes LTR, ignoring the direction) for inline layout algorithm.
inline LineBoxStrut ComputeLineMarginsForVisualContainer(
    const ConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return LineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(
          {constraint_space.GetWritingMode(), TextDirection::kLtr});
}

CORE_EXPORT BoxStrut ComputeBorders(const ConstraintSpace&, const BlockNode&);

CORE_EXPORT BoxStrut ComputeBordersForInline(const ComputedStyle&);

CORE_EXPORT BoxStrut ComputeNonCollapsedTableBorders(const ComputedStyle&);

inline LineBoxStrut ComputeLineBorders(const ComputedStyle& style) {
  return LineBoxStrut(ComputeBordersForInline(style),
                      style.IsFlippedLinesWritingMode());
}

CORE_EXPORT BoxStrut ComputeBordersForTest(const ComputedStyle& style);

CORE_EXPORT BoxStrut ComputePadding(const ConstraintSpace&,
                                    const ComputedStyle&);

inline LineBoxStrut ComputeLinePadding(const ConstraintSpace& constraint_space,
                                       const ComputedStyle& style) {
  return LineBoxStrut(ComputePadding(constraint_space, style),
                      style.IsFlippedLinesWritingMode());
}

// Compute the scrollbars and scrollbar gutters.
CORE_EXPORT BoxStrut ComputeScrollbarsForNonAnonymous(const BlockNode&);

inline BoxStrut ComputeScrollbars(const ConstraintSpace& space,
                                  const BlockNode& node) {
  if (space.IsAnonymous())
    return BoxStrut();

  return ComputeScrollbarsForNonAnonymous(node);
}

// Resolves any 'auto' margins in the inline dimension. All arguments are in
// the containing-block's writing-mode.
CORE_EXPORT void ResolveInlineAutoMargins(
    const ComputedStyle& child_style,
    const ComputedStyle& containing_block_style,
    LayoutUnit available_inline_size,
    LayoutUnit inline_size,
    BoxStrut* margins);

// Calculate the adjustment needed for the line's left position, based on
// text-align, direction and amount of unused space.
CORE_EXPORT LayoutUnit LineOffsetForTextAlign(ETextAlign,
                                              TextDirection,
                                              LayoutUnit space_left);

inline LayoutUnit ConstrainByMinMax(LayoutUnit length,
                                    LayoutUnit min,
                                    LayoutUnit max) {
  return std::max(min, std::min(length, max));
}

template <typename MinMaxSizesFunc>
FragmentGeometry CalculateInitialFragmentGeometry(
    const ConstraintSpace& space,
    const BlockNode& node,
    const BlockBreakToken* break_token,
    const MinMaxSizesFunc& min_max_sizes_func,
    bool is_intrinsic = false) {
  const auto& style = node.Style();

  if (node.IsFrameSet()) {
    if (node.IsParentNGFrameSet()) {
      const auto size = space.AvailableSize();
      DCHECK_NE(size.inline_size, kIndefiniteSize);
      DCHECK_NE(size.block_size, kIndefiniteSize);
      DCHECK(space.IsFixedInlineSize());
      DCHECK(space.IsFixedBlockSize());
      return {size, {}, {}, {}};
    }

    const auto size = node.InitialContainingBlockSize();
    return {size.ConvertToLogical(style.GetWritingMode()), {}, {}, {}};
  }

  const auto border = ComputeBorders(space, node);
  const auto padding = ComputePadding(space, style);
  auto scrollbar = ComputeScrollbars(space, node);

  const auto border_padding = border + padding;
  const auto border_scrollbar_padding = border_padding + scrollbar;

  if (node.IsReplaced()) {
    const auto border_box_size =
        ComputeReplacedSize(node, space, border_padding);
    return {border_box_size, border, scrollbar, padding};
  }

  absl::optional<LayoutUnit> inline_size;
  const auto default_block_size = CalculateDefaultBlockSize(
      space, node, break_token, border_scrollbar_padding);

  if (!is_intrinsic &&
      (space.IsFixedInlineSize() ||
       !InlineLengthUnresolvable(space, style.LogicalWidth()))) {
    inline_size = ComputeInlineSizeForFragment(space, node, border_padding,
                                               min_max_sizes_func);

    if (UNLIKELY(*inline_size < border_scrollbar_padding.InlineSum() &&
                 scrollbar.InlineSum() && !space.IsAnonymous())) {
      // Clamp the inline size of the scrollbar, unless it's larger than the
      // inline size of the content box, in which case we'll return that
      // instead. Scrollbar handling is quite bad in such situations, and this
      // method here is just to make sure that left-hand scrollbars don't mess
      // up scrollWidth. For the full story, visit http://crbug.com/724255.
      const auto content_box_inline_size =
          *inline_size - border_padding.InlineSum();

      if (scrollbar.InlineSum() > content_box_inline_size) {
        if (scrollbar.inline_end) {
          DCHECK(!scrollbar.inline_start);
          scrollbar.inline_end = content_box_inline_size;
        } else {
          DCHECK(scrollbar.inline_start);
          scrollbar.inline_start = content_box_inline_size;
        }
      }
    }
  }

  const auto block_size = ComputeInitialBlockSizeForFragment(
      space, style, border_padding, default_block_size, inline_size);

  return {LogicalSize(inline_size.value_or(kIndefiniteSize), block_size),
          border, scrollbar, padding};
}

// Calculates the initial (pre-layout) fragment geometry given a node, and a
// constraint space.
// The "pre-layout" block-size may be indefinite, as we'll only have enough
// information to determine this post-layout.
// Setting |is_intrinsic| to true will avoid calculating the inline-size, and
// is typically used within the |BlockNode::ComputeMinMaxSizes| pass (as to
// determine the inline-size, we'd need to compute the min/max sizes, which in
// turn would call this function).
CORE_EXPORT FragmentGeometry
CalculateInitialFragmentGeometry(const ConstraintSpace&,
                                 const BlockNode&,
                                 const BlockBreakToken*,
                                 bool is_intrinsic = false);

// Shrinks the logical |size| by |insets|.
LogicalSize ShrinkLogicalSize(LogicalSize size, const BoxStrut& insets);

// Calculates the available size that children of the node should use.
LogicalSize CalculateChildAvailableSize(
    const ConstraintSpace&,
    const BlockNode& node,
    const LogicalSize border_box_size,
    const BoxStrut& border_scrollbar_padding);

// Calculates the percentage resolution size that children of the node should
// use.
LogicalSize CalculateChildPercentageSize(
    const ConstraintSpace&,
    const BlockNode node,
    const LogicalSize child_available_size);

// Calculates the percentage resolution size that replaced children of the node
// should use.
LogicalSize CalculateReplacedChildPercentageSize(
    const ConstraintSpace&,
    const BlockNode node,
    const LogicalSize child_available_size,
    const BoxStrut& border_scrollbar_padding,
    const BoxStrut& border_padding);

// The following function clamps the calculated size based on the node
// requirements. Specifically, this adjusts the size based on size containment
// and display locking status.
LayoutUnit ClampIntrinsicBlockSize(
    const ConstraintSpace&,
    const BlockNode&,
    const BlockBreakToken* break_token,
    const BoxStrut& border_scrollbar_padding,
    LayoutUnit current_intrinsic_block_size,
    absl::optional<LayoutUnit> body_margin_block_sum = absl::nullopt);

// This function checks if the inline size of this node has to be calculated
// without considering children. If so, it returns the calculated size.
// Otherwise, it returns absl::nullopt and the caller has to compute the size
// itself.
absl::optional<MinMaxSizesResult> CalculateMinMaxSizesIgnoringChildren(
    const BlockNode&,
    const BoxStrut& border_scrollbar_padding);

// Determine which scrollbars to freeze in the next layout pass. Scrollbars that
// appear will be frozen (while scrollbars that disappear will not). Input is
// the scrollbar situation before and after the previous layout pass, and the
// current freeze state (|freeze_horizontal|, |freeze_vertical|). Output is the
// new freeze state (|freeze_horizontal|, |freeze_vertical|). A scrollbar that
// was previously frozen will not become unfrozen.
void AddScrollbarFreeze(const BoxStrut& scrollbars_before,
                        const BoxStrut& scrollbars_after,
                        WritingDirectionMode,
                        bool* freeze_horizontal,
                        bool* freeze_vertical);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LENGTH_UTILS_H_
