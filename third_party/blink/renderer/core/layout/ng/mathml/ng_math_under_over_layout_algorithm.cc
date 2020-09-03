// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_under_over_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"

namespace blink {
namespace {

// Describes the amount to shift to apply to the under/over boxes.
// Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://mathml-refresh.github.io/mathml-core/#base-with-underscript
// https://mathml-refresh.github.io/mathml-core/#base-with-overscript
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

  // https://mathml-refresh.github.io/mathml-core/#dfn-default-fallback-constant
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

  // TODO(crbug.com/1124285): Handle accent/accentunder attributes.
  // TODO(crbug.com/1124289): Implement AccentBaseHeight.
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

// https://mathml-refresh.github.io/mathml-core/#underscripts-and-overscripts-munder-mover-munderover
bool HasAccent(const NGBlockNode& node, bool accent_under) {
  DCHECK(node);
  auto* underover = To<MathMLUnderOverElement>(node.GetLayoutBox()->GetNode());
  auto script_type = underover->GetScriptType();
  DCHECK(script_type == MathScriptType::kUnderOver ||
         (accent_under && script_type == MathScriptType::kUnder) ||
         (!accent_under && script_type == MathScriptType::kOver));

  base::Optional<bool> attribute_value =
      accent_under ? underover->AccentUnder() : underover->Accent();
  return attribute_value && *attribute_value;
}

}  // namespace

NGMathUnderOverLayoutAlgorithm::NGMathUnderOverLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsNewFormattingContext(
      params.space.IsNewFormattingContext());
}

void NGMathUnderOverLayoutAlgorithm::GatherChildren(NGBlockNode* base,
                                                    NGBlockNode* over,
                                                    NGBlockNode* under) {
  auto script_type = Node().ScriptType();
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
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
        NOTREACHED();
    }
  }
}

scoped_refptr<const NGLayoutResult> NGMathUnderOverLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  DCHECK(IsValidMathMLScript(Node()));

  NGBlockNode base = nullptr;
  NGBlockNode over = nullptr;
  NGBlockNode under = nullptr;
  GatherChildren(&base, &over, &under);

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();

  const LogicalOffset content_start_offset =
      BorderScrollbarPadding().StartOffset();

  LayoutUnit block_offset = content_start_offset.block_offset;

  bool is_base_large_operator = false;
  bool is_base_stretchy_in_inline_axis = false;
  if (auto* core_operator =
          DynamicTo<MathMLOperatorElement>(base.GetLayoutBox()->GetNode())) {
    // TODO(crbug.com/1124298): Implement embellished operators.
    is_base_large_operator =
        core_operator->HasBooleanProperty(MathMLOperatorElement::kLargeOp);
    is_base_stretchy_in_inline_axis =
        core_operator->HasBooleanProperty(MathMLOperatorElement::kStretchy) &&
        !core_operator->GetOperatorContent().is_vertical;
  }
  UnderOverVerticalParameters parameters = GetUnderOverVerticalParameters(
      Style(), is_base_large_operator, is_base_stretchy_in_inline_axis);
  // TODO(crbug.com/1124301): handle stretchy operators.

  auto base_space = CreateConstraintSpaceForMathChild(
      Node(), ChildAvailableSize(), ConstraintSpace(), base);
  auto base_layout_result = base.Layout(base_space);
  auto base_margins =
      ComputeMarginsFor(base_space, base.Style(), ConstraintSpace());

  NGBoxFragment base_fragment(
      ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
      To<NGPhysicalBoxFragment>(base_layout_result->PhysicalFragment()));
  LayoutUnit base_ascent = base_fragment.BaselineOrSynthesize();

  // All children are positioned centered relative to the container (and
  // therefore centered relative to themselves).
  if (over) {
    auto over_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), over);
    scoped_refptr<const NGLayoutResult> over_layout_result =
        over.Layout(over_space);
    NGBoxStrut over_margins =
        ComputeMarginsFor(over_space, over.Style(), ConstraintSpace());
    NGBoxFragment over_fragment(
        ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
        To<NGPhysicalBoxFragment>(over_layout_result->PhysicalFragment()));
    block_offset += parameters.over_extra_ascender + over_margins.block_start;
    LogicalOffset over_offset = {
        content_start_offset.inline_offset + over_margins.inline_start +
            (ChildAvailableSize().inline_size -
             (over_fragment.InlineSize() + over_margins.InlineSum())) /
                2,
        block_offset};
    container_builder_.AddChild(over_layout_result->PhysicalFragment(),
                                over_offset);
    over.StoreMargins(ConstraintSpace(), over_margins);
    if (parameters.use_under_over_bar_fallback) {
      block_offset += over_fragment.BlockSize();
      if (HasAccent(Node(), false)) {
        if (base_ascent < parameters.accent_base_height)
          block_offset += parameters.accent_base_height - base_ascent;
      } else {
        block_offset += parameters.over_gap_min;
      }
    } else {
      LayoutUnit over_ascent = over_fragment.BaselineOrSynthesize();
      block_offset +=
          std::max(over_fragment.BlockSize() + parameters.over_gap_min,
                   over_ascent + parameters.over_shift_min);
    }
    block_offset += over_margins.block_end;
  }

  block_offset += base_margins.block_start;
  LogicalOffset base_offset = {
      content_start_offset.inline_offset + base_margins.inline_start +
          (ChildAvailableSize().inline_size -
           (base_fragment.InlineSize() + base_margins.InlineSum())) /
              2,
      block_offset};
  container_builder_.AddChild(base_layout_result->PhysicalFragment(),
                              base_offset);
  base.StoreMargins(ConstraintSpace(), base_margins);
  block_offset += base_fragment.BlockSize() + base_margins.block_end;

  if (under) {
    auto under_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), under);
    scoped_refptr<const NGLayoutResult> under_layout_result =
        under.Layout(under_space);
    NGBoxStrut under_margins =
        ComputeMarginsFor(under_space, under.Style(), ConstraintSpace());
    NGBoxFragment under_fragment(
        ConstraintSpace().GetWritingMode(), ConstraintSpace().Direction(),
        To<NGPhysicalBoxFragment>(under_layout_result->PhysicalFragment()));
    block_offset += under_margins.block_start;
    if (parameters.use_under_over_bar_fallback) {
      if (!HasAccent(Node(), true))
        block_offset += parameters.under_gap_min;
    } else {
      LayoutUnit under_ascent = under_fragment.BaselineOrSynthesize();
      block_offset += std::max(parameters.under_gap_min,
                               parameters.under_shift_min - under_ascent);
    }
    LogicalOffset under_offset = {
        content_start_offset.inline_offset + under_margins.inline_start +
            (ChildAvailableSize().inline_size -
             (under_fragment.InlineSize() + under_margins.InlineSum())) /
                2,
        block_offset};
    block_offset += under_fragment.BlockSize();
    block_offset += parameters.under_extra_descender;
    container_builder_.AddChild(under_layout_result->PhysicalFragment(),
                                under_offset);
    under.StoreMargins(ConstraintSpace(), under_margins);
    block_offset += under_margins.block_end;
  }

  container_builder_.SetBaseline(base_offset.block_offset + base_ascent);

  block_offset += BorderScrollbarPadding().block_end;

  LayoutUnit block_size =
      ComputeBlockSizeForFragment(ConstraintSpace(), Style(), BorderPadding(),
                                  block_offset, border_box_size.inline_size);

  container_builder_.SetIntrinsicBlockSize(block_offset);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathUnderOverLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& child_input) const {
  DCHECK(IsValidMathMLScript(Node()));

  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    auto child_result = ComputeMinAndMaxContentContribution(
        Style(), To<NGBlockNode>(child), child_input);
    NGBoxStrut margins = ComputeMinMaxMargins(Style(), child);
    child_result.sizes += margins.InlineSum();

    sizes.Encompass(child_result.sizes);
    depends_on_percentage_block_size |=
        child_result.depends_on_percentage_block_size;
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return {sizes, depends_on_percentage_block_size};
}

}  // namespace blink
