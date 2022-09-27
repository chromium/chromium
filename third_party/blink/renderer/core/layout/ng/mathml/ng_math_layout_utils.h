// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

namespace blink {

struct LogicalSize;
struct MinMaxSizes;
struct MinMaxSizesResult;
class NGBlockNode;
class NGConstraintSpace;
class NGLayoutInputNode;
class SimpleFontData;

// Creates a new constraint space for the current child.
NGConstraintSpace CreateConstraintSpaceForMathChild(
    const NGBlockNode& parent_node,
    const LogicalSize& child_available_size,
    const NGConstraintSpace& parent_constraint_space,
    const NGLayoutInputNode&,
    const NGCacheSlot = NGCacheSlot::kLayout,
    const absl::optional<NGConstraintSpace::MathTargetStretchBlockSizes>
        target_stretch_block_sizes = absl::nullopt,
    const absl::optional<LayoutUnit> target_stretch_inline_size =
        absl::nullopt);

MinMaxSizesResult ComputeMinAndMaxContentContributionForMathChild(
    const ComputedStyle& parent_style,
    const NGConstraintSpace& parent_constraint_space,
    const NGBlockNode& child,
    LayoutUnit child_available_block_size);

NGLayoutInputNode FirstChildInFlow(const NGBlockNode&);
NGLayoutInputNode NextSiblingInFlow(const NGBlockNode&);

bool IsValidMathMLFraction(const NGBlockNode&);
bool IsValidMathMLScript(const NGBlockNode&);
bool IsValidMathMLRadical(const NGBlockNode&);

// https://w3c.github.io/mathml-core/#dfn-default-rule-thickness
inline float RuleThicknessFallback(const ComputedStyle& style) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  if (!font_data)
    return 0;
  return font_data->GetFontMetrics().UnderlineThickness().value_or(0);
}

LayoutUnit MathAxisHeight(const ComputedStyle& style);

inline absl::optional<float> MathConstant(
    const ComputedStyle& style,
    OpenTypeMathSupport::MathConstants constant) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  return font_data ? OpenTypeMathSupport::MathConstant(
                         font_data->PlatformData().GetHarfBuzzFace(), constant)
                   : constant;
}

LayoutUnit FractionLineThickness(const ComputedStyle&);

inline bool HasDisplayStyle(const ComputedStyle& style) {
  return style.MathStyle() == EMathStyle::kNormal;
}

// Get parameters for horizontal positioning of mroot.
// The parameters are defined here:
// https://w3c.github.io/mathml-core/#layout-constants-mathconstants
struct RadicalHorizontalParameters {
  LayoutUnit kern_before_degree;
  LayoutUnit kern_after_degree;
};
RadicalHorizontalParameters GetRadicalHorizontalParameters(
    const ComputedStyle&);

// Get parameters for vertical positioning of msqrt/mroot.
// The parameters are defined here:
// https://w3c.github.io/mathml-core/#layout-constants-mathconstants
struct RadicalVerticalParameters {
  LayoutUnit vertical_gap;
  LayoutUnit rule_thickness;
  LayoutUnit extra_ascender;
  float degree_bottom_raise_percent;
};
RadicalVerticalParameters GetRadicalVerticalParameters(const ComputedStyle&,
                                                       bool has_index);

// https://w3c.github.io/mathml-core/#dfn-preferred-inline-size-of-a-glyph-stretched-along-the-block-axis
MinMaxSizes GetMinMaxSizesForVerticalStretchyOperator(const ComputedStyle&,
                                                      UChar character);

bool IsUnderOverLaidOutAsSubSup(const NGBlockNode& node);
bool IsTextOnlyToken(const NGBlockNode& node);
bool IsOperatorWithSpecialShaping(const NGBlockNode& node);

LayoutUnit MathTableBaseline(const ComputedStyle&, LayoutUnit block_offset);

// For nodes corresponding to embellished operators, this function returns the
// properties of its core operator. Otherwise, it returns a null optional.
// See https://mathml-refresh.github.io/mathml-core/#embellished-operators
struct MathMLEmbellishedOperatorProperties {
  bool has_movablelimits;
  bool is_large_op;
  bool is_stretchy;
  bool is_vertical;
  LayoutUnit lspace;
  LayoutUnit rspace;
};
absl::optional<MathMLEmbellishedOperatorProperties>
GetMathMLEmbellishedOperatorProperties(const NGBlockNode&);

bool IsStretchyOperator(const NGBlockNode& node, bool stretch_axis_is_vertical);
inline bool IsBlockAxisStretchyOperator(const NGBlockNode& node) {
  return IsStretchyOperator(node, true);
}
inline bool IsInlineAxisStretchyOperator(const NGBlockNode& node) {
  return IsStretchyOperator(node, false);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_MATHML_NG_MATH_LAYOUT_UTILS_H_
