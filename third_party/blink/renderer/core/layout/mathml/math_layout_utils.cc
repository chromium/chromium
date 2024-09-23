// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"

#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_radical_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"

namespace blink {

ConstraintSpace CreateConstraintSpaceForMathChild(
    const BlockNode& parent_node,
    const LogicalSize& child_available_size,
    const ConstraintSpace& parent_space,
    const LayoutInputNode& child,
    LayoutResultCacheSlot cache_slot,
    const std::optional<ConstraintSpace::MathTargetStretchBlockSizes>
        target_stretch_block_sizes,
    const std::optional<LayoutUnit> target_stretch_inline_size) {
  const ComputedStyle& parent_style = parent_node.Style();
  const ComputedStyle& child_style = child.Style();
  DCHECK(child.CreatesNewFormattingContext());
  ConstraintSpaceBuilder builder(
      parent_space, child_style.GetWritingDirection(), true /* is_new_fc */);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, child, &builder);
  builder.SetAvailableSize(child_available_size);
  builder.SetPercentageResolutionSize(child_available_size);
  builder.SetCacheSlot(cache_slot);
  if (target_stretch_block_sizes)
    builder.SetTargetStretchBlockSizes(*target_stretch_block_sizes);
  if (target_stretch_inline_size)
    builder.SetTargetStretchInlineSize(*target_stretch_inline_size);

  // TODO(crbug.com/1125137): add ink metrics.
  return builder.ToConstraintSpace();
}

MinMaxSizesResult ComputeMinAndMaxContentContributionForMathChild(
    const ComputedStyle& parent_style,
    const ConstraintSpace& parent_space,
    const BlockNode& child,
    LayoutUnit child_available_block_size) {
  DCHECK(child.CreatesNewFormattingContext());
  MinMaxConstraintSpaceBuilder builder(parent_space, parent_style, child,
                                       true /* is_new_fc */);
  builder.SetAvailableBlockSize(child_available_block_size);
  builder.SetPercentageResolutionBlockSize(child_available_block_size);
  const auto space = builder.ToConstraintSpace();

  auto result = ComputeMinAndMaxContentContribution(parent_style, child, space);

  // Add margins directly here.
  result.sizes +=
      ComputeMarginsFor(space, child.Style(), parent_space).InlineSum();

  return result;
}

LayoutInputNode FirstChildInFlow(const BlockNode& node) {
  LayoutInputNode child = node.FirstChild();
  while (child && child.IsOutOfFlowPositioned())
    child = child.NextSibling();
  return child;
}

LayoutInputNode NextSiblingInFlow(const BlockNode& node) {
  LayoutInputNode sibling = node.NextSibling();
  while (sibling && sibling.IsOutOfFlowPositioned())
    sibling = sibling.NextSibling();
  return sibling;
}

inline bool InFlowChildCountIs(const BlockNode& node, unsigned count) {
  DCHECK(count == 2 || count == 3);
  auto child = To<BlockNode>(FirstChildInFlow(node));
  while (count && child) {
    child = To<BlockNode>(NextSiblingInFlow(child));
    count--;
  }
  return !count && !child;
}

bool IsValidMathMLFraction(const BlockNode& node) {
  return InFlowChildCountIs(node, 2);
}

static bool IsPrescriptDelimiter(const BlockNode& block_node) {
  auto* node = block_node.GetDOMNode();
  return node && IsA<MathMLElement>(node) &&
         node->HasTagName(mathml_names::kMprescriptsTag);
}

// Valid according to:
// https://w3c.github.io/mathml-core/#prescripts-and-tensor-indices-mmultiscripts
inline bool IsValidMultiscript(const BlockNode& node) {
  auto child = To<BlockNode>(FirstChildInFlow(node));
  if (!child || IsPrescriptDelimiter(child))
    return false;
  bool number_of_scripts_is_even = true;
  bool prescript_delimiter_found = false;
  while (child) {
    child = To<BlockNode>(NextSiblingInFlow(child));
    if (!child)
      continue;
    if (IsPrescriptDelimiter(child)) {
      if (!number_of_scripts_is_even || prescript_delimiter_found)
        return false;
      prescript_delimiter_found = true;
      continue;
    }
    number_of_scripts_is_even = !number_of_scripts_is_even;
  }
  return number_of_scripts_is_even;
}

bool IsValidMathMLScript(const BlockNode& node) {
  switch (node.ScriptType()) {
    case MathScriptType::kUnder:
    case MathScriptType::kOver:
    case MathScriptType::kSub:
    case MathScriptType::kSuper:
      return InFlowChildCountIs(node, 2);
    case MathScriptType::kSubSup:
    case MathScriptType::kUnderOver:
      return InFlowChildCountIs(node, 3);
    case MathScriptType::kMultiscripts:
      return IsValidMultiscript(node);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool IsValidMathMLRadical(const BlockNode& node) {
  auto* radical =
      DynamicTo<MathMLRadicalElement>(node.GetDOMNode());
  return !radical->HasIndex() || InFlowChildCountIs(node, 2);
}

RadicalHorizontalParameters GetRadicalHorizontalParameters(
    const ComputedStyle& style) {
  RadicalHorizontalParameters parameters;
  parameters.kern_before_degree = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kRadicalKernBeforeDegree)
          .value_or(5 * style.FontSize() * kMathUnitFraction));
  parameters.kern_after_degree = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kRadicalKernAfterDegree)
          .value_or(-10 * style.FontSize() * kMathUnitFraction));
  return parameters;
}

RadicalVerticalParameters GetRadicalVerticalParameters(
    const ComputedStyle& style,
    bool has_index) {
  RadicalVerticalParameters parameters;
  bool has_display = HasDisplayStyle(style);
  float rule_thickness = RuleThicknessFallback(style);
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  float x_height = font_data ? font_data->GetFontMetrics().XHeight() : 0;
  parameters.rule_thickness = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kRadicalRuleThickness)
          .value_or(rule_thickness));
  parameters.vertical_gap = LayoutUnit(
      MathConstant(
          style, has_display
                     ? OpenTypeMathSupport::MathConstants::
                           kRadicalDisplayStyleVerticalGap
                     : OpenTypeMathSupport::MathConstants::kRadicalVerticalGap)
          .value_or(has_display ? rule_thickness + x_height / 4
                                : 5 * rule_thickness / 4));
  parameters.extra_ascender = LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kRadicalExtraAscender)
          .value_or(parameters.rule_thickness));
  if (has_index) {
    parameters.degree_bottom_raise_percent =
        MathConstant(style, OpenTypeMathSupport::MathConstants::
                                kRadicalDegreeBottomRaisePercent)
            .value_or(.6);
  }
  return parameters;
}

MinMaxSizes GetMinMaxSizesForVerticalStretchyOperator(
    const ComputedStyle& style,
    UChar character) {
  // https://w3c.github.io/mathml-core/#dfn-preferred-inline-size-of-a-glyph-stretched-along-the-block-axis
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  MinMaxSizes sizes;
  if (!font_data)
    return sizes;

  if (auto base_glyph = font_data->GlyphForCharacter(character)) {
    sizes.Encompass(LayoutUnit(font_data->WidthForGlyph(base_glyph)));

    const HarfBuzzFace* harfbuzz_face =
        font_data->PlatformData().GetHarfBuzzFace();

    for (auto& variant : OpenTypeMathSupport::GetGlyphVariantRecords(
             harfbuzz_face, base_glyph, OpenTypeMathStretchData::Vertical)) {
      sizes.Encompass(LayoutUnit(font_data->WidthForGlyph(variant)));
    }

    for (auto& part : OpenTypeMathSupport::GetGlyphPartRecords(
             harfbuzz_face, base_glyph,
             OpenTypeMathStretchData::StretchAxis::Vertical)) {
      sizes.Encompass(LayoutUnit(font_data->WidthForGlyph(part.glyph)));
    }
  }

  return sizes;
}

bool IsUnderOverLaidOutAsSubSup(const BlockNode& node) {
  DCHECK(IsValidMathMLScript(node));
  if (HasDisplayStyle(node.Style()))
    return false;
  if (!node.IsBlock() || !node.IsMathML())
    return false;
  const auto base = To<BlockNode>(FirstChildInFlow(node));
  const auto base_properties = GetMathMLEmbellishedOperatorProperties(base);
  return base_properties && base_properties->has_movablelimits;
}

bool IsTextOnlyToken(const BlockNode& node) {
  if (!node.IsBlock() || !node.IsMathML() || !node.FirstChild().IsInline())
    return false;
  if (auto* element = DynamicTo<MathMLTokenElement>(node.GetDOMNode()))
    return !element->GetTokenContent().characters.IsNull();
  return false;
}

bool IsOperatorWithSpecialShaping(const BlockNode& node) {
  if (!IsTextOnlyToken(node))
    return false;
  // https://w3c.github.io/mathml-core/#layout-of-operators
  if (auto* element = DynamicTo<MathMLOperatorElement>(node.GetDOMNode())) {
    UChar32 base_code_point = element->GetTokenContent().code_point;
    if (base_code_point == kNonCharacter ||
        !node.Style().GetFont().PrimaryFont() ||
        !node.Style().GetFont().PrimaryFont()->GlyphForCharacter(
            base_code_point))
      return false;

    if (element->HasBooleanProperty(MathMLOperatorElement::kStretchy))
      return true;

    if (element->HasBooleanProperty(MathMLOperatorElement::kLargeOp) &&
        HasDisplayStyle(node.Style()))
      return true;
  }
  return false;
}

namespace {

inline LayoutUnit DefaultFractionLineThickness(const ComputedStyle& style) {
  return LayoutUnit(
      MathConstant(style,
                   OpenTypeMathSupport::MathConstants::kFractionRuleThickness)
          .value_or(RuleThicknessFallback(style)));
}

}  // namespace

LayoutUnit MathAxisHeight(const ComputedStyle& style) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  float x_height = font_data ? font_data->GetFontMetrics().XHeight() : 0;
  return LayoutUnit(
      MathConstant(style, OpenTypeMathSupport::MathConstants::kAxisHeight)
          .value_or(x_height / 2));
}

LayoutUnit FractionLineThickness(const ComputedStyle& style) {
  return std::max<LayoutUnit>(
      ValueForLength(style.GetMathFractionBarThickness(),
                     DefaultFractionLineThickness(style)),
      LayoutUnit());
}

LayoutUnit MathTableBaseline(const ComputedStyle& style,
                             LayoutUnit block_offset) {
  // The center of the table is aligned with the math axis.
  // See: https://w3c.github.io/mathml-core/#table-or-matrix-mtable
  return LayoutUnit(block_offset / 2 + MathAxisHeight(style));
}

namespace {

// This function has bad theoretical worst-case complexity. However, real-life
// MathML formulas don't use deeply nested space-like expressions so it should
// be fine in in practice. See https://github.com/w3c/mathml/issues/115
static bool IsSpaceLike(const BlockNode& node) {
  DCHECK(node);
  if (!node.IsMathML())
    return false;
  // See https://w3c.github.io/mathml-core/#dfn-space-like
  const auto* element = DynamicTo<MathMLElement>(node.GetDOMNode());
  // 1. An <mtext> or <mspace>;
  if (element && (element->HasTagName(mathml_names::kMtextTag) ||
                  element->HasTagName(mathml_names::kMspaceTag)))
    return true;
  // 2. Or a grouping element or <mpadded> all of whose in-flow children are
  // space-like.
  // Note: This also handles the case of anonymous <mrow>'s generated by
  // <msqrt> and <mpadded> elements.
  if ((element && (element->IsGroupingElement() ||
                   element->HasTagName(mathml_names::kMpaddedTag))) ||
      node.IsAnonymous()) {
    for (auto child = To<BlockNode>(FirstChildInFlow(node)); child;
         child = To<BlockNode>(NextSiblingInFlow(child))) {
      if (!IsSpaceLike(child))
        return false;
    }
    return true;
  }
  return false;
}

// This function has bad theoretical worst-case complexity. However, real-life
// MathML formulas don't use deeply nested expressions that are embellished
// operators or that are essentially made of space-like descendants, so it
// should be fine in in practice. See https://github.com/w3c/mathml/issues/115
MathMLOperatorElement* GetCoreOperator(const BlockNode& node) {
  if (!node || !node.IsMathML())
    return nullptr;

  // See https://w3c.github.io/mathml-core/#embellished-operators
  auto* element = DynamicTo<MathMLElement>(node.GetDOMNode());
  if (element && element->HasTagName(mathml_names::kMoTag)) {
    // 1. An <mo> element;
    return To<MathMLOperatorElement>(element);
  }
  if (element && (IsA<MathMLScriptsElement>(element) ||
                  element->HasTagName(mathml_names::kMfracTag))) {
    // 2. A scripted element or an <mfrac>, whose first in-flow child exists
    // and is an embellished operator;
    auto first_child = FirstChildInFlow(node);
    return IsA<BlockNode>(first_child)
               ? GetCoreOperator(To<BlockNode>(first_child))
               : nullptr;
  }
  if ((element && (element->IsGroupingElement() ||
                   element->HasTagName(mathml_names::kMpaddedTag))) ||
      node.IsAnonymous()) {
    // 3. A grouping element or <mpadded>, whose in-flow children consist (in
    // any order) of one embellished operator and zero or more space-like
    // elements.
    // Note: This also handles the case of anonymous <mrow>'s generated by
    // <msqrt> and <mpadded> elements.
    MathMLOperatorElement* core_operator = nullptr;
    for (auto child = To<BlockNode>(FirstChildInFlow(node)); child;
         child = To<BlockNode>(NextSiblingInFlow(child))) {
      // Skip space-like children as they don't affect whether the parent is an
      // embellished operator.
      if (IsSpaceLike(child))
        continue;

      // The parent is not an embellished operator if it contains two children
      // that are not space-like.
      if (core_operator)
        return nullptr;
      core_operator = GetCoreOperator(child);

      // The parent is not an embellished operator if it contains a child that
      // is neither space-like nor an embellished operator.
      if (!core_operator)
        return nullptr;
    }
    return core_operator;
  }
  return nullptr;
}

}  // namespace

std::optional<MathMLEmbellishedOperatorProperties>
GetMathMLEmbellishedOperatorProperties(const BlockNode& node) {
  auto* core_operator = GetCoreOperator(node);
  if (!core_operator)
    return std::nullopt;
  DCHECK(core_operator->GetLayoutObject());
  const auto& core_operator_style =
      core_operator->GetLayoutObject()->StyleRef();

  MathMLEmbellishedOperatorProperties properties;

  properties.has_movablelimits =
      core_operator->HasBooleanProperty(MathMLOperatorElement::kMovableLimits);

  properties.is_stretchy =
      core_operator->HasBooleanProperty(MathMLOperatorElement::kStretchy);

  properties.is_large_op =
      core_operator->HasBooleanProperty(MathMLOperatorElement::kLargeOp);

  properties.is_vertical = core_operator->IsVertical();

  LayoutUnit leading_space(core_operator->DefaultLeadingSpace() *
                           core_operator_style.FontSize());
  properties.lspace =
      ValueForLength(core_operator_style.GetMathLSpace(), leading_space)
          .ClampNegativeToZero();

  LayoutUnit trailing_space(core_operator->DefaultTrailingSpace() *
                            core_operator_style.FontSize());
  properties.rspace =
      ValueForLength(core_operator_style.GetMathRSpace(), trailing_space)
          .ClampNegativeToZero();

  return properties;
}

bool IsStretchyOperator(const BlockNode& node, bool stretch_axis_is_vertical) {
  const auto properties = GetMathMLEmbellishedOperatorProperties(node);
  return properties && properties->is_stretchy &&
         properties->is_vertical == stretch_axis_is_vertical;
}

}  // namespace blink
