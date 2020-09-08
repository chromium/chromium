// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {
class ComputedStyle;
class Length;
struct MinMaxSizesInput;
class NGConstraintSpace;
class NGBlockNode;
class NGLayoutInputNode;

// LengthResolvePhase indicates what type of layout pass we are currently in.
// This changes how lengths are resolved. kIntrinsic must be used during the
// intrinsic sizes pass, and kLayout must be used during the layout pass.
enum class LengthResolvePhase { kIntrinsic, kLayout };

inline bool NeedMinMaxSize(const ComputedStyle& style) {
  // This check is technically too broad (fill-available does not need intrinsic
  // size computation) but that's a rare case and only affects performance, not
  // correctness.
  return style.LogicalWidth().IsIntrinsic() ||
         style.LogicalMinWidth().IsIntrinsic() ||
         style.LogicalMaxWidth().IsIntrinsic();
}

LayoutUnit InlineSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                     const LogicalSize& aspect_ratio,
                                     EBoxSizing box_sizing,
                                     LayoutUnit block_size);

LayoutUnit BlockSizeFromAspectRatio(const NGBoxStrut& border_padding,
                                    const LogicalSize& aspect_ratio,
                                    EBoxSizing box_sizing,
                                    LayoutUnit inline_size);

// Returns if the given |Length| is unresolvable, e.g. the length is %-based
// during the intrinsic phase. For block lengths we also consider 'auto',
// 'min-content', 'max-content', 'fit-content' and 'none' (for max-block-size)
// as unresolvable.
CORE_EXPORT bool InlineLengthUnresolvable(const Length&, LengthResolvePhase);
CORE_EXPORT bool BlockLengthUnresolvable(
    const NGConstraintSpace&,
    const Length&,
    LengthResolvePhase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Resolve means translate a Length to a LayoutUnit.
//  - |NGConstraintSpace| the information given by the parent, e.g. the
//    available-size.
//  - |ComputedStyle| the style of the node.
//  - |border_padding| the resolved border, and padding of the node.
//  - |MinMaxSizes| is only used when the length is intrinsic (fit-content).
//  - |Length| is the length to resolve.
CORE_EXPORT LayoutUnit
ResolveInlineLengthInternal(const NGConstraintSpace&,
                            const ComputedStyle&,
                            const NGBoxStrut& border_padding,
                            const base::Optional<MinMaxSizes>&,
                            const Length&);

// Same as ResolveInlineLengthInternal, except here |content_size| roughly plays
// the part of |MinMaxSizes|.
CORE_EXPORT LayoutUnit ResolveBlockLengthInternal(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    const Length&,
    LayoutUnit content_size,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Used for resolving min inline lengths, (|ComputedStyle::MinLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMinInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsAuto() || InlineLengthUnresolvable(length, phase)))
    return border_padding.InlineSum();

  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMinInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsAuto() || InlineLengthUnresolvable(length, phase)))
    return border_padding.InlineSum();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving max inline lengths, (|ComputedStyle::MaxLogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMaxInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsNone() || InlineLengthUnresolvable(length, phase)))
    return LayoutUnit::Max();

  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMaxInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length,
    LengthResolvePhase phase) {
  if (LIKELY(length.IsNone() || InlineLengthUnresolvable(length, phase)))
    return LayoutUnit::Max();

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving main inline lengths, (|ComputedStyle::LogicalWidth|).
template <typename MinMaxSizesFunc>
inline LayoutUnit ResolveMainInlineLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const MinMaxSizesFunc& min_max_sizes_func,
    const Length& length) {
  base::Optional<MinMaxSizes> min_max_sizes;
  if (length.IsIntrinsic()) {
    min_max_sizes =
        min_max_sizes_func(length.IsMinIntrinsic() ? MinMaxSizesType::kIntrinsic
                                                   : MinMaxSizesType::kContent)
            .sizes;
  }

  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

template <>
inline LayoutUnit ResolveMainInlineLength<base::Optional<MinMaxSizes>>(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const base::Optional<MinMaxSizes>& min_max_sizes,
    const Length& length) {
  return ResolveInlineLengthInternal(constraint_space, style, border_padding,
                                     min_max_sizes, length);
}

// Used for resolving min block lengths, (|ComputedStyle::MinLogicalHeight|).
inline LayoutUnit ResolveMinBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(BlockLengthUnresolvable(
          constraint_space, length, phase,
          opt_percentage_resolution_block_size_for_min_max)))
    return border_padding.BlockSum();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving max block lengths, (|ComputedStyle::MaxLogicalHeight|).
inline LayoutUnit ResolveMaxBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (LIKELY(BlockLengthUnresolvable(
          constraint_space, length, phase,
          opt_percentage_resolution_block_size_for_min_max)))
    return LayoutUnit::Max();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, kIndefiniteSize,
      opt_percentage_resolution_block_size_for_min_max);
}

// Used for resolving main block lengths, (|ComputedStyle::LogicalHeight|).
inline LayoutUnit ResolveMainBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    LayoutUnit content_size,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(
                   constraint_space, length, phase,
                   opt_percentage_resolution_block_size_for_min_max)))
    return content_size;

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, content_size,
      opt_percentage_resolution_block_size_for_min_max);
}

template <typename IntrinsicBlockSizeFunc>
inline LayoutUnit ResolveMainBlockLength(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style,
    const NGBoxStrut& border_padding,
    const Length& length,
    const IntrinsicBlockSizeFunc& intrinsic_block_size_func,
    LengthResolvePhase phase,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr) {
  if (UNLIKELY((length.IsPercentOrCalc() || length.IsFillAvailable()) &&
               BlockLengthUnresolvable(
                   constraint_space, length, phase,
                   opt_percentage_resolution_block_size_for_min_max)))
    return intrinsic_block_size_func();

  LayoutUnit intrinsic_block_size = kIndefiniteSize;
  if (length.IsIntrinsicOrAuto())
    intrinsic_block_size = intrinsic_block_size_func();

  return ResolveBlockLengthInternal(
      constraint_space, style, border_padding, length, intrinsic_block_size,
      opt_percentage_resolution_block_size_for_min_max);
}

// For the given style and min/max content sizes, computes the min and max
// content contribution (https://drafts.csswg.org/css-sizing/#contributions).
// This is similar to ComputeInlineSizeForFragment except that it does not
// require a constraint space (percentage sizes as well as auto margins compute
// to zero) and that an auto inline size resolves to the respective min/max
// content size.
// Also, the min/max contribution does include the inline margins as well.
// Because content contributions are commonly needed by a block's parent,
// we also take a writing mode here so we can compute this in the parent's
// coordinate system.
CORE_EXPORT MinMaxSizes
ComputeMinAndMaxContentContributionForTest(WritingMode writing_mode,
                                           const NGBlockNode&,
                                           const MinMaxSizes&);

// A version of ComputeMinAndMaxContentContribution that does not require you
// to compute the min/max content size of the child. Instead, this function
// will compute it if necessary.
// |child| is the node of which to compute the min/max content contribution.
// Note that if the writing mode of the child is orthogonal to that of the
// parent, we'll still return the inline min/max contribution in the writing
// mode of the parent (i.e. typically something based on the preferred *block*
// size of the child).
MinMaxSizesResult ComputeMinAndMaxContentContribution(
    const ComputedStyle& parent_style,
    const NGBlockNode& child,
    const MinMaxSizesInput&);

// Computes the min-block-size and max-block-size values for a node.
MinMaxSizes ComputeMinMaxBlockSize(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    LayoutUnit content_size,
    const LayoutUnit* opt_percentage_resolution_block_size_for_min_max =
        nullptr);

// Computes the transferred min/max inline sizes from the min/max block
// sizes and the aspect ratio.
MinMaxSizes ComputeMinMaxInlineSizesFromAspectRatio(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    LengthResolvePhase);

// Tries to compute the inline size of a node from its block size and
// aspect ratio. If there is no aspect ratio or the block size is indefinite,
// returns kIndefiniteSize.
// block_size can be specified to base the calculation off of that size
// instead of calculating it.
LayoutUnit ComputeInlineSizeFromAspectRatio(
    const NGConstraintSpace&,
    const ComputedStyle&,
    const NGBoxStrut& border_padding,
    LayoutUnit block_size = kIndefiniteSize);

// Returns inline size of the node's border box by resolving the computed value
// in style.logicalWidth (Length) to a layout unit, adding border and padding,
// then constraining the result by the resolved min logical width and max
// logical width from the ComputedStyle object. Calls Node::ComputeMinMaxSize
// if needed.
// |override_min_max_sizes_for_test| is provided *solely* for use by unit tests.
CORE_EXPORT LayoutUnit ComputeInlineSizeForFragment(
    const NGConstraintSpace&,
    NGLayoutInputNode,
    const NGBoxStrut& border_padding,
    const MinMaxSizes* override_min_max_sizes_for_test = nullptr);

// Same as ComputeInlineSizeForFragment, but uses height instead of width.
// |inline_size| is necessary to compute the block size when an aspect ratio
// is in use.
CORE_EXPORT LayoutUnit
ComputeBlockSizeForFragment(const NGConstraintSpace&,
                            const ComputedStyle&,
                            const NGBoxStrut& border_padding,
                            LayoutUnit content_size,
                            base::Optional<LayoutUnit> inline_size);

// Intrinsic size for replaced elements is computed as:
// - |out_replaced_size| intrinsic size of the element. It might have no value.
// - |out_aspect_ratio| only set if out_replaced_size is empty.
//   If out_replaced_size is not empty, that is the aspect ratio.
// This routine will return one of the following:
// - out_replaced_size, and no out_aspect_ratio
// - out_aspect_ratio, and no out_replaced_size
// - neither out_aspect_ratio, nor out_replaced_size
// SVG elements can return any of the three options above.
CORE_EXPORT void ComputeReplacedSize(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const base::Optional<MinMaxSizes>&,
    base::Optional<LogicalSize>* out_replaced_size,
    base::Optional<LogicalSize>* out_aspect_ratio);

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

// Compute physical margins.
CORE_EXPORT NGPhysicalBoxStrut
ComputePhysicalMargins(const ComputedStyle&,
                       LayoutUnit percentage_resolution_size);

inline NGPhysicalBoxStrut ComputePhysicalMargins(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size);
}

// Compute margins for the specified NGConstraintSpace.
CORE_EXPORT NGBoxStrut ComputeMarginsFor(const NGConstraintSpace&,
                                         const ComputedStyle&,
                                         const NGConstraintSpace& compute_for);

inline NGBoxStrut ComputeMarginsFor(const ComputedStyle& child_style,
                                    LayoutUnit percentage_resolution_size,
                                    WritingMode container_writing_mode,
                                    TextDirection container_direction) {
  return ComputePhysicalMargins(child_style, percentage_resolution_size)
      .ConvertToLogical(container_writing_mode, container_direction);
}

// Compute margins for the style owner.
inline NGBoxStrut ComputeMarginsForSelf(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLogical(style.GetWritingMode(), style.Direction());
}

// Compute line logical margins for the style owner.
//
// The "line" versions compute line-relative logical values. See NGLineBoxStrut
// for more details.
inline NGLineBoxStrut ComputeLineMarginsForSelf(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGLineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(style.GetWritingMode(), style.Direction());
}

// Compute line logical margins for the constraint space, in the visual order
// (always assumes LTR, ignoring the direction) for inline layout algorithm.
inline NGLineBoxStrut ComputeLineMarginsForVisualContainer(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  if (!style.MayHaveMargin() || constraint_space.IsAnonymous())
    return NGLineBoxStrut();
  LayoutUnit percentage_resolution_size =
      constraint_space.PercentageResolutionInlineSizeForParentWritingMode();
  return ComputePhysicalMargins(style, percentage_resolution_size)
      .ConvertToLineLogical(constraint_space.GetWritingMode(),
                            TextDirection::kLtr);
}

// Compute margins for a child during the min-max size calculation.
CORE_EXPORT NGBoxStrut ComputeMinMaxMargins(const ComputedStyle& parent_style,
                                            NGLayoutInputNode child);

CORE_EXPORT NGBoxStrut ComputeBorders(const NGConstraintSpace&,
                                      const NGBlockNode&);

CORE_EXPORT NGBoxStrut ComputeBordersForInline(const ComputedStyle& style);

inline NGLineBoxStrut ComputeLineBorders(
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputeBordersForInline(style),
                        style.IsFlippedLinesWritingMode());
}

CORE_EXPORT NGBoxStrut ComputeBordersForTest(const ComputedStyle& style);

CORE_EXPORT NGBoxStrut ComputeIntrinsicPadding(const NGConstraintSpace&,
                                               const ComputedStyle&,
                                               const NGBoxStrut& scrollbar);

CORE_EXPORT NGBoxStrut ComputePadding(const NGConstraintSpace&,
                                      const ComputedStyle&);

inline NGLineBoxStrut ComputeLinePadding(
    const NGConstraintSpace& constraint_space,
    const ComputedStyle& style) {
  return NGLineBoxStrut(ComputePadding(constraint_space, style),
                        style.IsFlippedLinesWritingMode());
}

// Compute the scrollbars and scrollbar gutters.
CORE_EXPORT NGBoxStrut ComputeScrollbarsForNonAnonymous(const NGBlockNode&);

inline NGBoxStrut ComputeScrollbars(const NGConstraintSpace& space,
                                    const NGBlockNode& node) {
  if (space.IsAnonymous())
    return NGBoxStrut();

  return ComputeScrollbarsForNonAnonymous(node);
}

// Return true if we need to know the inline size of the fragment in order to
// calculate its line-left offset. This is the case when we have auto margins,
// or when block alignment isn't line-left (e.g. with align!=left, and always in
// RTL mode).
bool NeedsInlineSizeToResolveLineLeft(
    const ComputedStyle& style,
    const ComputedStyle& containing_block_style);

// Convert inline margins from computed to used values. This will resolve 'auto'
// values and over-constrainedness. This uses the available size from the
// constraint space and inline size to compute the margins that are auto, if
// any, and adjusts the given NGBoxStrut accordingly.
// available_inline_size, inline_size, and margins are all in the
// containing_block's writing mode.
CORE_EXPORT void ResolveInlineMargins(
    const ComputedStyle& child_style,
    const ComputedStyle& containing_block_style,
    LayoutUnit available_inline_size,
    LayoutUnit inline_size,
    NGBoxStrut* margins);

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

// Calculates the initial (pre-layout) fragment geometry given a node, and a
// constraint space.
// The "pre-layout" block-size may be indefinite, as we'll only have enough
// information to determine this post-layout.
CORE_EXPORT NGFragmentGeometry
CalculateInitialFragmentGeometry(const NGConstraintSpace&, const NGBlockNode&);

// Similar to |CalculateInitialFragmentGeometry| however will only calculate
// the border, scrollbar, and padding (resolving percentages to zero).
CORE_EXPORT NGFragmentGeometry
CalculateInitialMinMaxFragmentGeometry(const NGConstraintSpace&,
                                       const NGBlockNode&);

// Shrinks the logical |size| by |insets|.
LogicalSize ShrinkLogicalSize(LogicalSize size, const NGBoxStrut& insets);

// Calculates the available size that children of the node should use.
LogicalSize CalculateChildAvailableSize(
    const NGConstraintSpace&,
    const NGBlockNode& node,
    const LogicalSize border_box_size,
    const NGBoxStrut& border_scrollbar_padding);

// Calculates the percentage resolution size that children of the node should
// use.
LogicalSize CalculateChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    const LogicalSize child_available_size);

// Calculates the percentage resolution size that replaced children of the node
// should use.
LogicalSize CalculateReplacedChildPercentageSize(
    const NGConstraintSpace&,
    const NGBlockNode node,
    const LogicalSize child_available_size,
    const NGBoxStrut& border_scrollbar_padding,
    const NGBoxStrut& border_padding);

LayoutUnit CalculateChildPercentageBlockSizeForMinMax(
    const NGConstraintSpace& constraint_space,
    const NGBlockNode node,
    const NGBoxStrut& border_padding,
    LayoutUnit input_percentage_block_size,
    bool* uses_input_percentage_block_size);

// The following function clamps the calculated size based on the node
// requirements. Specifically, this adjusts the size based on size containment
// and display locking status.
LayoutUnit ClampIntrinsicBlockSize(
    const NGConstraintSpace&,
    const NGBlockNode&,
    const NGBoxStrut& border_scrollbar_padding,
    LayoutUnit current_intrinsic_block_size,
    base::Optional<LayoutUnit> body_margin_block_sum = base::nullopt);

// This function checks if the inline size of this node has to be calculated
// without considering children. If so, it returns the calculated size.
// Otherwise, it returns base::nullopt and the caller has to compute the size
// itself.
base::Optional<MinMaxSizesResult> CalculateMinMaxSizesIgnoringChildren(
    const NGBlockNode&,
    const NGBoxStrut& border_scrollbar_padding);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_LENGTH_UTILS_H_
