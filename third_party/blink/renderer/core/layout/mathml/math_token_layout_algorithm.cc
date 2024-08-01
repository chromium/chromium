// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_token_layout_algorithm.h"

#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"

namespace blink {

MathTokenLayoutAlgorithm::MathTokenLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  container_builder_.SetIsInlineFormattingContext(
      Node().IsInlineFormattingContextRoot());
}

const LayoutResult* MathTokenLayoutAlgorithm::Layout() {
  DCHECK(!IsBreakInside(GetBreakToken()));

  LayoutInputNode child = Node().FirstChild();
  DCHECK(child && child.IsInline());
  DCHECK(!child.NextSibling());
  DCHECK(!child.IsOutOfFlowPositioned());

  TextMetrics* metrics = MakeGarbageCollected<TextMetrics>(
      Style().GetFont(), Style().Direction(), kAlphabeticTextBaseline,
      kStartTextAlign,
      DynamicTo<MathMLTokenElement>(Node().GetDOMNode())
          ->GetTokenContent()
          .characters);
  LayoutUnit ink_ascent(metrics->actualBoundingBoxAscent());
  LayoutUnit ink_descent(metrics->actualBoundingBoxDescent());
  LayoutUnit ascent = BorderScrollbarPadding().block_start + ink_ascent;
  LayoutUnit descent = ink_descent + BorderScrollbarPadding().block_end;

  SimpleInlineChildLayoutContext context(To<InlineNode>(child),
                                         &container_builder_);
  const LayoutResult* child_layout_result = To<InlineNode>(child).Layout(
      GetConstraintSpace(), /* break_token */ nullptr,
      /* column_spanner_path */ nullptr, &context);

  const auto& line_box =
      To<PhysicalLineBoxFragment>(child_layout_result->GetPhysicalFragment());
  const FontHeight line_metrics = line_box.Metrics();
  container_builder_.AddResult(
      *child_layout_result,
      {BorderScrollbarPadding().inline_start, ascent - line_metrics.ascent});

  LayoutUnit intrinsic_block_size = ascent + descent;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);
  container_builder_.SetBaselines(ascent);
  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MathTokenLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput& input) {
  LayoutInputNode child = Node().FirstChild();
  DCHECK(child && child.IsInline());
  DCHECK(!child.NextSibling());
  DCHECK(!child.IsOutOfFlowPositioned());

  MinMaxSizes sizes;
  sizes += BorderScrollbarPadding().InlineSum();

  const auto child_result = To<InlineNode>(child).ComputeMinMaxSizes(
      Style().GetWritingMode(), GetConstraintSpace(), MinMaxSizesFloatInput());
  sizes += child_result.sizes;

  return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
}

}  // namespace blink
