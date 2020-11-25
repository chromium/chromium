// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_operator_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"
#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

namespace blink {

namespace {

inline LayoutUnit DisplayOperatorMinHeight(const ComputedStyle& style) {
  return LayoutUnit(
      MathConstant(
          style, OpenTypeMathSupport::MathConstants::kDisplayOperatorMinHeight)
          .value_or(0));
}

}  // namespace

NGMathOperatorLayoutAlgorithm::NGMathOperatorLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsInlineFormattingContext(
      Node().IsInlineFormattingContextRoot());
}

scoped_refptr<const NGLayoutResult> NGMathOperatorLayoutAlgorithm::Layout() {
  // This algorithm can only be used for operators with a single text node,
  // which itself must contain only one glyph. We ensure that the subtree is
  // properly laid out but the glyph will actually be used to determine a
  // "large" or "stretched" version, from which we perform measurement and
  // painting.
  // See https://mathml-refresh.github.io/mathml-core/#layout-of-operators
  NGLayoutInputNode child = Node().FirstChild();
  DCHECK(child && child.IsInline());
  DCHECK(!child.NextSibling());
  DCHECK(!child.IsOutOfFlowPositioned());

  NGInlineChildLayoutContext context;
  NGFragmentItemsBuilder items_builder(
      To<NGInlineNode>(child), container_builder_.GetWritingDirection());
  container_builder_.SetItemsBuilder(&items_builder);
  context.SetItemsBuilder(&items_builder);
  scoped_refptr<const NGLayoutResult> child_layout_result =
      To<NGInlineNode>(child).Layout(ConstraintSpace(), nullptr, &context);
  container_builder_.AddResult(*child_layout_result, {});

  // TODO(http://crbug.com/1124301) Implement stretchy operators.

  float operator_target_size = DisplayOperatorMinHeight(Style());
  StretchyOperatorShaper shaper(GetBaseCodePoint(),
                                OpenTypeMathStretchData::StretchAxis::Vertical);
  StretchyOperatorShaper::Metrics metrics;
  scoped_refptr<ShapeResult> shape_result =
      shaper.Shape(&Style().GetFont(), operator_target_size, &metrics);
  scoped_refptr<const ShapeResultView> shape_result_view =
      ShapeResultView::Create(shape_result.get());

  if (metrics.italic_correction) {
    container_builder_.SetMathItalicCorrection(
        LayoutUnit(metrics.italic_correction));
  }

  LayoutUnit operator_ascent = LayoutUnit::FromFloatFloor(metrics.ascent);
  LayoutUnit operator_descent = LayoutUnit::FromFloatFloor(metrics.descent);

  container_builder_.SetMathMLPaintInfo(
      GetBaseCodePoint(), std::move(shape_result_view),
      LayoutUnit(metrics.advance), operator_ascent, operator_descent);

  LayoutUnit ascent = BorderScrollbarPadding().block_start + operator_ascent;
  LayoutUnit descent = operator_descent + BorderScrollbarPadding().block_end;
  LayoutUnit intrinsic_block_size = ascent + descent;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);
  container_builder_.SetBaseline(ascent);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);
  container_builder_.SetIsMathMLOperator();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathOperatorLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  // TODO(http://crbug.com/1124301) Implement stretchy operators.

  MinMaxSizes sizes;
  StretchyOperatorShaper shaper(GetBaseCodePoint(),
                                OpenTypeMathStretchData::Vertical);
  StretchyOperatorShaper::Metrics metrics;
  float operator_target_size = DisplayOperatorMinHeight(Style());
  shaper.Shape(&Style().GetFont(), operator_target_size, &metrics);
  sizes.Encompass(LayoutUnit(metrics.advance));
  sizes += BorderScrollbarPadding().InlineSum();

  return {sizes, /* depends_on_percentage_block_size */ false};
}

UChar32 NGMathOperatorLayoutAlgorithm::GetBaseCodePoint() const {
  return DynamicTo<MathMLOperatorElement>(Node().GetDOMNode())
      ->GetOperatorContent()
      .code_point;
}

}  // namespace blink
