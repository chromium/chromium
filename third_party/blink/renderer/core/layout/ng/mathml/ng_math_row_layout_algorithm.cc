// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_row_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_child_layout_context.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_operator_element.h"

namespace blink {
namespace {

inline LayoutUnit InlineOffsetForDisplayMathCentering(
    bool is_display_block_math,
    LayoutUnit available_inline_size,
    LayoutUnit max_row_inline_size) {
  if (is_display_block_math)
    return (available_inline_size - max_row_inline_size) / 2;
  return LayoutUnit();
}

static void DetermineOperatorSpacing(const NGLayoutInputNode& node,
                                     LayoutUnit* lspace,
                                     LayoutUnit* rspace) {
  auto* core_operator =
      DynamicTo<MathMLOperatorElement>(node.GetDOMNode());
  if (core_operator) {
    // TODO(crbug.com/1124298): Implement embellished operators.
    LayoutUnit leading_space(core_operator->DefaultLeadingSpace() *
                             node.Style().FontSize());
    *lspace = std::max<LayoutUnit>(
        ValueForLength(node.Style().GetMathLSpace(), leading_space),
        LayoutUnit());
    LayoutUnit trailing_space(core_operator->DefaultTrailingSpace() *
                              node.Style().FontSize());
    *rspace = std::max<LayoutUnit>(
        ValueForLength(node.Style().GetMathRSpace(), trailing_space),
        LayoutUnit());
  }
}

}  // namespace

NGMathRowLayoutAlgorithm::NGMathRowLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
  DCHECK(!ConstraintSpace().HasBlockFragmentation());
}

void NGMathRowLayoutAlgorithm::LayoutRowItems(
    ChildrenVector* children,
    LayoutUnit* max_row_block_baseline,
    LogicalSize* row_total_size) {
  LayoutUnit inline_offset, max_row_ascent, max_row_descent;
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned()) {
      // TODO(rbuis): OOF should be "where child would have been if not
      // absolutely positioned".
      // Issue: https://github.com/mathml-refresh/mathml/issues/16
      container_builder_.AddOutOfFlowChildCandidate(
          To<NGBlockNode>(child), BorderScrollbarPadding().StartOffset());
      continue;
    }
    LayoutUnit lspace, rspace;
    DetermineOperatorSpacing(child, &lspace, &rspace);
    const ComputedStyle& child_style = child.Style();
    NGConstraintSpace child_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), child);
    scoped_refptr<const NGLayoutResult> result =
        To<NGBlockNode>(child).Layout(child_space, nullptr /* break token */);
    const NGPhysicalContainerFragment& physical_fragment =
        result->PhysicalFragment();
    NGBoxFragment fragment(ConstraintSpace().GetWritingMode(),
                           ConstraintSpace().Direction(),
                           To<NGPhysicalBoxFragment>(physical_fragment));

    NGBoxStrut margins =
        ComputeMarginsFor(child_space, child_style, ConstraintSpace());
    inline_offset += margins.inline_start;

    LayoutUnit ascent = margins.block_start + fragment.BaselineOrSynthesize();
    *max_row_block_baseline = std::max(*max_row_block_baseline, ascent);

    // TODO(crbug.com/1125136): take into account italic correction.
    inline_offset += lspace;

    children->emplace_back(
        To<NGBlockNode>(child), margins,
        LogicalOffset{inline_offset, margins.block_start - ascent},
        std::move(&physical_fragment));

    inline_offset += fragment.InlineSize() + margins.inline_end;

    inline_offset += rspace;

    max_row_ascent = std::max(max_row_ascent, ascent + margins.block_start);
    max_row_descent = std::max(
        max_row_descent, fragment.BlockSize() + margins.block_end - ascent);
    row_total_size->inline_size =
        std::max(row_total_size->inline_size, inline_offset);
  }
  row_total_size->block_size = max_row_ascent + max_row_descent;
}

scoped_refptr<const NGLayoutResult> NGMathRowLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  bool is_display_block_math =
      Node().IsMathRoot() && (Style().Display() == EDisplay::kBlockMath);

  LogicalSize max_row_size;
  LayoutUnit max_row_block_baseline;

  const LogicalSize border_box_size = container_builder_.InitialBorderBoxSize();

  ChildrenVector children;
  LayoutRowItems(&children, &max_row_block_baseline, &max_row_size);

  // Add children taking into account centering, baseline and
  // border/scrollbar/padding.
  LayoutUnit center_offset = InlineOffsetForDisplayMathCentering(
      is_display_block_math, container_builder_.InlineSize(),
      max_row_size.inline_size);

  LogicalOffset adjust_offset = BorderScrollbarPadding().StartOffset();
  adjust_offset += LogicalOffset{center_offset, max_row_block_baseline};
  for (auto& child_data : children) {
    child_data.offset += adjust_offset;
    container_builder_.AddChild(
        To<NGPhysicalContainerFragment>(*child_data.fragment),
        child_data.offset);
    child_data.child.StoreMargins(ConstraintSpace(), child_data.margins);
  }

  container_builder_.SetBaseline(adjust_offset.block_offset);

  auto block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(),
      max_row_size.block_size + BorderScrollbarPadding().BlockSum(),
      border_box_size.inline_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathRowLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& child_input) const {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    MinMaxSizesResult child_result = ComputeMinAndMaxContentContribution(
        Style(), To<NGBlockNode>(child), child_input);
    NGBoxStrut child_margins = ComputeMinMaxMargins(Style(), child);
    child_result.sizes += child_margins.InlineSum();

    sizes += child_result.sizes;

    LayoutUnit lspace, rspace;
    DetermineOperatorSpacing(child, &lspace, &rspace);
    sizes += lspace + rspace;
    depends_on_percentage_block_size |=
        child_result.depends_on_percentage_block_size;

    // TODO(crbug.com/1125136): take into account italic correction.
  }

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());

  DCHECK_LE(sizes.min_size, sizes.max_size);
  sizes += BorderScrollbarPadding().InlineSum();

  return {sizes, depends_on_percentage_block_size};
}

}  // namespace blink
