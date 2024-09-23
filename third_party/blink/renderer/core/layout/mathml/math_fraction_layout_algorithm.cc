// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_fraction_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

namespace blink {
namespace {

// Describes the amount to shift the numerator/denominator of the fraction when
// a fraction bar is present. Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://w3c.github.io/mathml-core/#fraction-with-nonzero-line-thickness
struct FractionParameters {
  LayoutUnit numerator_gap_min;
  LayoutUnit denominator_gap_min;
  LayoutUnit numerator_min_shift_up;
  LayoutUnit denominator_min_shift_down;
};

FractionParameters GetFractionParameters(const ComputedStyle& style) {
  FractionParameters parameters;

  bool has_display_style = HasDisplayStyle(style);

  // We try and read constants to draw the fraction from the OpenType MATH and
  // use fallback values suggested in the MathML Core specification otherwise.
  parameters.numerator_gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionNumDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kFractionNumeratorGapMin)
          .value_or((has_display_style ? 3 : 1) *
                    RuleThicknessFallback(style)));
  parameters.denominator_gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionDenomDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kFractionDenominatorGapMin)
          .value_or(parameters.numerator_gap_min));

  parameters.numerator_min_shift_up = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kFractionNumeratorDisplayStyleShiftUp
              : OpenTypeMathSupport::MathConstants::kFractionNumeratorShiftUp)
          .value_or(0));
  parameters.denominator_min_shift_down = LayoutUnit(
      MathConstant(style, has_display_style
                              ? OpenTypeMathSupport::MathConstants::
                                    kFractionDenominatorDisplayStyleShiftDown
                              : OpenTypeMathSupport::MathConstants::
                                    kFractionDenominatorShiftDown)
          .value_or(0));

  return parameters;
}

// Describes the amount to shift the numerator/denominator of the fraction when
// a fraction bar is not present. Data is populated from the OpenType MATH
// table. If the OpenType MATH table is not present fallback values are used.
// https://w3c.github.io/mathml-core/#fraction-with-zero-line-thickness
struct FractionStackParameters {
  LayoutUnit gap_min;
  LayoutUnit top_shift_up;
  LayoutUnit bottom_shift_down;
};

FractionStackParameters GetFractionStackParameters(const ComputedStyle& style) {
  FractionStackParameters parameters;

  bool has_display_style = HasDisplayStyle(style);

  // We try and read constants to draw the stack from the OpenType MATH and use
  // fallback values otherwise.
  // We use the fallback values suggested in the MATH table specification.
  parameters.gap_min = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::kStackDisplayStyleGapMin
              : OpenTypeMathSupport::MathConstants::kStackGapMin)
          .value_or((has_display_style ? 7 : 3) *
                    RuleThicknessFallback(style)));
  // The MATH table specification does not suggest any values for shifts, so
  // we leave them at zero.
  parameters.top_shift_up = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::kStackTopDisplayStyleShiftUp
              : OpenTypeMathSupport::MathConstants::kStackTopShiftUp)
          .value_or(0));
  parameters.bottom_shift_down = LayoutUnit(
      MathConstant(
          style,
          has_display_style
              ? OpenTypeMathSupport::MathConstants::
                    kStackBottomDisplayStyleShiftDown
              : OpenTypeMathSupport::MathConstants::kStackBottomShiftDown)
          .value_or(0));

  return parameters;
}

}  // namespace

MathFractionLayoutAlgorithm::MathFractionLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsMathMLFraction();
}

void MathFractionLayoutAlgorithm::GatherChildren(BlockNode* numerator,
                                                 BlockNode* denominator) {
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    BlockNode block_child = To<BlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      container_builder_.AddOutOfFlowChildCandidate(
          block_child, BorderScrollbarPadding().StartOffset());
      continue;
    }
    if (!*numerator) {
      *numerator = block_child;
      continue;
    }
    if (!*denominator) {
      *denominator = block_child;
      continue;
    }

    NOTREACHED_IN_MIGRATION();
  }

  DCHECK(*numerator);
  DCHECK(*denominator);
}

const LayoutResult* MathFractionLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());

  BlockNode numerator = nullptr;
  BlockNode denominator = nullptr;
  GatherChildren(&numerator, &denominator);

  const auto numerator_space = CreateConstraintSpaceForMathChild(
      Node(), ChildAvailableSize(), GetConstraintSpace(), numerator);
  const LayoutResult* numerator_layout_result =
      numerator.Layout(numerator_space);
  const auto numerator_margins = ComputeMarginsFor(
      numerator_space, numerator.Style(), GetConstraintSpace());
  const auto denominator_space = CreateConstraintSpaceForMathChild(
      Node(), ChildAvailableSize(), GetConstraintSpace(), denominator);
  const LayoutResult* denominator_layout_result =
      denominator.Layout(denominator_space);
  const auto denominator_margins = ComputeMarginsFor(
      denominator_space, denominator.Style(), GetConstraintSpace());

  const LogicalBoxFragment numerator_fragment(
      GetConstraintSpace().GetWritingDirection(),
      To<PhysicalBoxFragment>(numerator_layout_result->GetPhysicalFragment()));
  const LogicalBoxFragment denominator_fragment(
      GetConstraintSpace().GetWritingDirection(),
      To<PhysicalBoxFragment>(
          denominator_layout_result->GetPhysicalFragment()));
  const auto baseline_type = Style().GetFontBaseline();

  const LayoutUnit numerator_ascent =
      numerator_margins.block_start +
      numerator_fragment.FirstBaselineOrSynthesize(baseline_type);
  const LayoutUnit numerator_descent = numerator_fragment.BlockSize() +
                                       numerator_margins.BlockSum() -
                                       numerator_ascent;
  const LayoutUnit denominator_ascent =
      denominator_margins.block_start +
      denominator_fragment.FirstBaselineOrSynthesize(baseline_type);
  const LayoutUnit denominator_descent = denominator_fragment.BlockSize() +
                                         denominator_margins.BlockSum() -
                                         denominator_ascent;

  LayoutUnit numerator_shift, denominator_shift;
  LayoutUnit thickness = FractionLineThickness(Style());
  if (thickness) {
    LayoutUnit axis_height = MathAxisHeight(Style());
    FractionParameters parameters = GetFractionParameters(Style());
    numerator_shift =
        std::max(parameters.numerator_min_shift_up,
                 axis_height + thickness / 2 + parameters.numerator_gap_min +
                     numerator_descent);
    denominator_shift =
        std::max(parameters.denominator_min_shift_down,
                 thickness / 2 + parameters.denominator_gap_min +
                     denominator_ascent - axis_height);
  } else {
    FractionStackParameters parameters = GetFractionStackParameters(Style());
    numerator_shift = parameters.top_shift_up;
    denominator_shift = parameters.bottom_shift_down;
    LayoutUnit gap = denominator_shift - denominator_ascent + numerator_shift -
                     numerator_descent;
    if (gap < parameters.gap_min) {
      LayoutUnit diff = parameters.gap_min - gap;
      LayoutUnit delta = diff / 2;
      numerator_shift += delta;
      denominator_shift += diff - delta;
    }
  }

  const LayoutUnit fraction_ascent =
      std::max(numerator_shift + numerator_ascent,
               -denominator_shift + denominator_ascent)
          .ClampNegativeToZero() +
      BorderScrollbarPadding().block_start;
  const LayoutUnit fraction_descent =
      std::max(-numerator_shift + numerator_descent,
               denominator_shift + denominator_descent)
          .ClampNegativeToZero() +
      BorderScrollbarPadding().block_end;
  LayoutUnit intrinsic_block_size = fraction_ascent + fraction_descent;

  container_builder_.SetBaselines(fraction_ascent);

  LogicalOffset numerator_offset;
  LogicalOffset denominator_offset;
  numerator_offset.inline_offset =
      BorderScrollbarPadding().inline_start + numerator_margins.inline_start +
      (ChildAvailableSize().inline_size -
       (numerator_fragment.InlineSize() + numerator_margins.InlineSum())) /
          2;
  denominator_offset.inline_offset =
      BorderScrollbarPadding().inline_start + denominator_margins.inline_start +
      (ChildAvailableSize().inline_size -
       (denominator_fragment.InlineSize() + denominator_margins.InlineSum())) /
          2;

  numerator_offset.block_offset = numerator_margins.block_start +
                                  fraction_ascent - numerator_shift -
                                  numerator_ascent;
  denominator_offset.block_offset = denominator_margins.block_start +
                                    fraction_ascent + denominator_shift -
                                    denominator_ascent;

  container_builder_.AddResult(*numerator_layout_result, numerator_offset,
                               numerator_margins);
  container_builder_.AddResult(*denominator_layout_result, denominator_offset,
                               denominator_margins);

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MathFractionLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;

    const auto child_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), GetConstraintSpace(), To<BlockNode>(child),
        ChildAvailableSize().block_size);

    sizes.Encompass(child_result.sizes);
    depends_on_block_constraints |= child_result.depends_on_block_constraints;
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
