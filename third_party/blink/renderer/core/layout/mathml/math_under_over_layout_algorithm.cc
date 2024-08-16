// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_under_over_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {
namespace {

// Describes the amount to shift to apply to the under/over boxes.
// Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://w3c.github.io/mathml-core/#base-with-underscript
// https://w3c.github.io/mathml-core/#base-with-overscript
struct UnderOverVerticalParameters {
  bool use_under_over_bar_fallback;
  LayoutUnit under_gap_min;
  LayoutUnit over_gap_min;
  LayoutUnit under_shift_min;
  LayoutUnit over_shift_min;
  LayoutUnit under_extra_descender;
  LayoutUnit over_extra_ascender;
  LayoutUnit accent_base_height;
};

UnderOverVerticalParameters GetUnderOverVerticalParameters(
    const ComputedStyle& style,
    bool is_base_large_operator,
    bool is_base_stretchy_in_inline_axis) {
  UnderOverVerticalParameters parameters;
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  if (!font_data)
    return parameters;

  // https://w3c.github.io/mathml-core/#dfn-default-fallback-constant
  const float default_fallback_constant = 0;

  if (is_base_large_operator) {
    parameters.under_gap_min = LayoutUnit(
        MathConstant(style,
                     OpenTypeMathSupport::MathConstants::kLowerLimitGapMin)
            .value_or(default_fallback_constant));
    parameters.over_gap_min = LayoutUnit(
        MathConstant(style,
                     OpenTypeMathSupport::MathConstants::kUpperLimitGapMin)
            .value_or(default_fallback_constant));
    parameters.under_shift_min = LayoutUnit(
        MathConstant(
            style,
            OpenTypeMathSupport::MathConstants::kLowerLimitBaselineDropMin)
            .value_or(default_fallback_constant));
    parameters.over_shift_min = LayoutUnit(
        MathConstant(
            style,
            OpenTypeMathSupport::MathConstants::kUpperLimitBaselineRiseMin)
            .value_or(default_fallback_constant));
    parameters.under_extra_descender = LayoutUnit();
    parameters.over_extra_ascender = LayoutUnit();
    parameters.accent_base_height = LayoutUnit();
    parameters.use_under_over_bar_fallback = false;
    return parameters;
  }

  if (is_base_stretchy_in_inline_axis) {
    parameters.under_gap_min = LayoutUnit(
        MathConstant(
            style, OpenTypeMathSupport::MathConstants::kStretchStackGapBelowMin)
            .value_or(default_fallback_constant));
    parameters.over_gap_min = LayoutUnit(
        MathConstant(
            style, OpenTypeMathSupport::MathConstants::kStretchStackGapAboveMin)
            .value_or(default_fallback_constant));
    parameters.under_shift_min = LayoutUnit(
        MathConstant(
            style,
            OpenTypeMathSupport::MathConstants::kStretchStackBottomShiftDown)
            .value_or(default_fallback_constant));
    parameters.over_shift_min = LayoutUnit(
        MathConstant(
            style, OpenTypeMathSupport::MathConstants::kStretchStackTopShiftUp)
            .value_or(default_fallback_constant));
    parameters.under_extra_descender = LayoutUnit();
    parameters.over_extra_ascender = LayoutUnit();
    parameters.accent_base_height = LayoutUnit();
    parameters.use_under_over_bar_fallback = false;
    return parameters;
  }

  const float default_rule_thickness = RuleThicknessFallback(style);
  parameters.under_gap_min = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kUnderbarVerticalGap)
          .value_or(3 * default_rule_thickness));
  parameters.over_gap_min = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kOverbarVerticalGap)
          .value_or(3 * default_rule_thickness));
  parameters.under_shift_min = LayoutUnit();
  parameters.over_shift_min = LayoutUnit();
  parameters.under_extra_descender = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kUnderbarExtraDescender)
          .value_or(default_rule_thickness));
  parameters.over_extra_ascender = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kOverbarExtraAscender)
          .value_or(default_rule_thickness));
  parameters.accent_base_height = LayoutUnit(
      MathConstant(style, OpenTypeMathSupport::MathConstants::kAccentBaseHeight)
          .value_or(font_data->GetFontMetrics().XHeight() / 2));
  parameters.use_under_over_bar_fallback = true;
  return parameters;
}

// https://w3c.github.io/mathml-core/#underscripts-and-overscripts-munder-mover-munderover
bool HasAccent(const BlockNode& node, bool accent_under) {
  DCHECK(node);
  auto* underover = To<MathMLUnderOverElement>(node.GetDOMNode());
  auto script_type = underover->GetScriptType();
  DCHECK(script_type == MathScriptType::kUnderOver ||
         (accent_under && script_type == MathScriptType::kUnder) ||
         (!accent_under && script_type == MathScriptType::kOver));

  std::optional<bool> attribute_value =
      accent_under ? underover->AccentUnder() : underover->Accent();
  return attribute_value && *attribute_value;
}

}  // namespace

MathUnderOverLayoutAlgorithm::MathUnderOverLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

void MathUnderOverLayoutAlgorithm::GatherChildren(BlockNode* base,
                                                  BlockNode* over,
                                                  BlockNode* under) {
  auto script_type = Node().ScriptType();
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    BlockNode block_child = To<BlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      container_builder_.AddOutOfFlowChildCandidate(
          block_child, BorderScrollbarPadding().StartOffset());
      continue;
    }
    if (!*base) {
      *base = block_child;
      continue;
    }
    switch (script_type) {
      case MathScriptType::kUnder:
        DCHECK(!*under);
        *under = block_child;
        break;
      case MathScriptType::kOver:
        DCHECK(!*over);
        *over = block_child;
        break;
      case MathScriptType::kUnderOver:
        if (!*under) {
          *under = block_child;
          continue;
        }
        DCHECK(!*over);
        *over = block_child;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

const LayoutResult* MathUnderOverLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  DCHECK(IsValidMathMLScript(Node()));

  BlockNode base = nullptr;
  BlockNode over = nullptr;
  BlockNode under = nullptr;
  GatherChildren(&base, &over, &under);

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();

  const LogicalOffset content_start_offset =
      BorderScrollbarPadding().StartOffset();

  LayoutUnit ascent;
  LayoutUnit descent;

  const auto base_properties = GetMathMLEmbellishedOperatorProperties(base);
  const bool is_base_large_operator =
      base_properties && base_properties->is_large_op;
  const bool is_base_stretchy_in_inline_axis = base_properties &&
                                               base_properties->is_stretchy &&
                                               !base_properties->is_vertical;
  const auto& constraint_space = GetConstraintSpace();
  const bool base_inherits_block_stretch_size_constraint =
      constraint_space.TargetStretchBlockSizes().has_value();
  const bool base_inherits_inline_stretch_size_constraint =
      !base_inherits_block_stretch_size_constraint &&
      constraint_space.HasTargetStretchInlineSize();
  UnderOverVerticalParameters parameters = GetUnderOverVerticalParameters(
      Style(), is_base_large_operator, is_base_stretchy_in_inline_axis);

  // https://w3c.github.io/mathml-core/#dfn-algorithm-for-stretching-operators-along-the-inline-axis
  LayoutUnit inline_stretch_size;
  auto UpdateInlineStretchSize = [&](const LayoutResult* result) {
    LogicalFragment fragment(
        constraint_space.GetWritingDirection(),
        To<PhysicalBoxFragment>(result->GetPhysicalFragment()));
    inline_stretch_size = std::max(inline_stretch_size, fragment.InlineSize());
  };

  // "Perform layout without any stretch size constraint on all the items of
  // LNotToStretch"
  bool layout_remaining_items_with_zero_inline_stretch_size = true;
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned() ||
        IsInlineAxisStretchyOperator(To<BlockNode>(child))) {
      continue;
    }
    const auto child_constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), constraint_space, child,
        LayoutResultCacheSlot::kMeasure);
    const auto* child_layout_result = To<BlockNode>(child).Layout(
        child_constraint_space, nullptr /* break_token */);
    UpdateInlineStretchSize(child_layout_result);
    layout_remaining_items_with_zero_inline_stretch_size = false;
  }

  if (layout_remaining_items_with_zero_inline_stretch_size) [[unlikely]] {
    // "If LNotToStretch is empty, perform layout with stretch size constraint 0
    // on all the items of LToStretch.
    for (LayoutInputNode child = Node().FirstChild(); child;
         child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned())
        continue;
      DCHECK(IsInlineAxisStretchyOperator(To<BlockNode>(child)));
      if (child == base && (base_inherits_block_stretch_size_constraint ||
                            base_inherits_inline_stretch_size_constraint))
        continue;
      LayoutUnit zero_stretch_size;
      const auto child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), constraint_space, child,
          LayoutResultCacheSlot::kMeasure, std::nullopt, zero_stretch_size);
      const auto* child_layout_result = To<BlockNode>(child).Layout(
          child_constraint_space, nullptr /* break_token */);
      UpdateInlineStretchSize(child_layout_result);
    }
  }

  auto CreateConstraintSpaceForUnderOverChild = [&](const BlockNode child) {
    if (child == base && base_inherits_block_stretch_size_constraint &&
        IsBlockAxisStretchyOperator(To<BlockNode>(child))) {
      return CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), constraint_space, child,
          LayoutResultCacheSlot::kLayout,
          *constraint_space.TargetStretchBlockSizes());
    }
    if (child == base && base_inherits_inline_stretch_size_constraint &&
        IsInlineAxisStretchyOperator(To<BlockNode>(child))) {
      return CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), constraint_space, child,
          LayoutResultCacheSlot::kLayout, std::nullopt,
          constraint_space.TargetStretchInlineSize());
    }
    if ((child != base || (!base_inherits_block_stretch_size_constraint &&
                           !base_inherits_inline_stretch_size_constraint)) &&
        IsInlineAxisStretchyOperator(To<BlockNode>(child))) {
      return CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), constraint_space, child,
          LayoutResultCacheSlot::kLayout, std::nullopt, inline_stretch_size);
    }
    return CreateConstraintSpaceForMathChild(Node(), ChildAvailableSize(),
                                             constraint_space, child,
                                             LayoutResultCacheSlot::kLayout);
  };

  // TODO(crbug.com/1125136): take into account italic correction.

  const auto baseline_type = Style().GetFontBaseline();
  const auto base_space = CreateConstraintSpaceForUnderOverChild(base);
  auto* base_layout_result = base.Layout(base_space);
  auto base_margins =
      ComputeMarginsFor(base_space, base.Style(), constraint_space);

  LogicalBoxFragment base_fragment(
      constraint_space.GetWritingDirection(),
      To<PhysicalBoxFragment>(base_layout_result->GetPhysicalFragment()));
  LayoutUnit base_ascent =
      base_fragment.FirstBaselineOrSynthesize(baseline_type);

  // All children are positioned centered relative to the container (and
  // therefore centered relative to themselves).
  if (over) {
    const auto over_space = CreateConstraintSpaceForUnderOverChild(over);
    const LayoutResult* over_layout_result = over.Layout(over_space);
    BoxStrut over_margins =
        ComputeMarginsFor(over_space, over.Style(), constraint_space);
    LogicalBoxFragment over_fragment(
        constraint_space.GetWritingDirection(),
        To<PhysicalBoxFragment>(over_layout_result->GetPhysicalFragment()));
    ascent += parameters.over_extra_ascender + over_margins.block_start;
    LogicalOffset over_offset = {
        content_start_offset.inline_offset + over_margins.inline_start +
            (ChildAvailableSize().inline_size -
             (over_fragment.InlineSize() + over_margins.InlineSum())) /
                2,
        BorderScrollbarPadding().block_start + ascent};
    container_builder_.AddResult(*over_layout_result, over_offset,
                                 over_margins);
    if (parameters.use_under_over_bar_fallback) {
      ascent += over_fragment.BlockSize();
      if (HasAccent(Node(), false)) {
        if (base_ascent < parameters.accent_base_height)
          ascent += parameters.accent_base_height - base_ascent;
      } else {
        ascent += parameters.over_gap_min;
      }
    } else {
      LayoutUnit over_ascent =
          over_fragment.FirstBaselineOrSynthesize(baseline_type);
      ascent += std::max(over_fragment.BlockSize() + parameters.over_gap_min,
                         over_ascent + parameters.over_shift_min);
    }
    ascent += over_margins.block_end;
  }

  ascent += base_margins.block_start;
  LogicalOffset base_offset = {
      content_start_offset.inline_offset + base_margins.inline_start +
          (ChildAvailableSize().inline_size -
           (base_fragment.InlineSize() + base_margins.InlineSum())) /
              2,
      BorderScrollbarPadding().block_start + ascent};
  container_builder_.AddResult(*base_layout_result, base_offset, base_margins);
  ascent += base_ascent;
  ascent = ascent.ClampNegativeToZero();
  ascent += BorderScrollbarPadding().block_start;
  descent = base_fragment.BlockSize() - base_ascent + base_margins.block_end;

  if (under) {
    const auto under_space = CreateConstraintSpaceForUnderOverChild(under);
    const LayoutResult* under_layout_result = under.Layout(under_space);
    BoxStrut under_margins =
        ComputeMarginsFor(under_space, under.Style(), constraint_space);
    LogicalBoxFragment under_fragment(
        constraint_space.GetWritingDirection(),
        To<PhysicalBoxFragment>(under_layout_result->GetPhysicalFragment()));
    descent += under_margins.block_start;
    if (parameters.use_under_over_bar_fallback) {
      if (!HasAccent(Node(), true))
        descent += parameters.under_gap_min;
    } else {
      LayoutUnit under_ascent =
          under_fragment.FirstBaselineOrSynthesize(baseline_type);
      descent += std::max(parameters.under_gap_min,
                          parameters.under_shift_min - under_ascent);
    }
    LogicalOffset under_offset = {
        content_start_offset.inline_offset + under_margins.inline_start +
            (ChildAvailableSize().inline_size -
             (under_fragment.InlineSize() + under_margins.InlineSum())) /
                2,
        ascent + descent};
    descent += under_fragment.BlockSize();
    descent += parameters.under_extra_descender;
    container_builder_.AddResult(*under_layout_result, under_offset,
                                 under_margins);
    descent += under_margins.block_end;
  }

  container_builder_.SetBaselines(ascent);
  descent = descent.ClampNegativeToZero();
  descent += BorderScrollbarPadding().block_end;

  LayoutUnit intrinsic_block_size = ascent + descent;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      constraint_space, Node(), BorderPadding(), intrinsic_block_size,
      border_box_size.inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MathUnderOverLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  DCHECK(IsValidMathMLScript(Node()));

  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    // TODO(crbug.com/1125136): take into account italic correction.
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
