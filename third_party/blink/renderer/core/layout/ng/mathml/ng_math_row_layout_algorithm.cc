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

static void DetermineOperatorSpacing(const NGBlockNode& node,
                                     LayoutUnit* lspace,
                                     LayoutUnit* rspace) {
  if (auto properties = GetMathMLEmbellishedOperatorProperties(node)) {
    *lspace = properties->lspace;
    *rspace = properties->rspace;
  }
}

static bool IsStretchyOperatorWithBlockStretchAxis(const NGBlockNode& node) {
  if (auto* core_operator =
          DynamicTo<MathMLOperatorElement>(node.GetDOMNode())) {
    // TODO(crbug.com/1124298): Implement embellished operators.
    return core_operator->HasBooleanProperty(
               MathMLOperatorElement::kStretchy) &&
           core_operator->GetOperatorContent().is_vertical;
  }
  return false;
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

  // https://w3c.github.io/mathml-core/#dfn-algorithm-for-stretching-operators-along-the-block-axis
  NGConstraintSpace::MathTargetStretchBlockSizes stretch_sizes;
  auto UpdateBlockStretchSizes =
      [&](const scoped_refptr<const NGLayoutResult>& result) {
        NGBoxFragment fragment(
            ConstraintSpace().GetWritingDirection(),
            To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
        LayoutUnit ascent = fragment.BaselineOrSynthesize();
        stretch_sizes.ascent = std::max(stretch_sizes.ascent, ascent),
        stretch_sizes.descent =
            std::max(stretch_sizes.descent, fragment.BlockSize() - ascent);
      };

  // "Perform layout without any stretch size constraint on all the items of
  // LNotToStretch."
  bool should_layout_remaining_items_with_zero_block_stretch_size = true;
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned() ||
        IsStretchyOperatorWithBlockStretchAxis(To<NGBlockNode>(child)))
      continue;
    const auto child_constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), child,
        NGCacheSlot::kMeasure);
    const auto child_layout_result = To<NGBlockNode>(child).Layout(
        child_constraint_space, nullptr /* break_token */);
    UpdateBlockStretchSizes(child_layout_result);
    should_layout_remaining_items_with_zero_block_stretch_size = false;
  }

  if (UNLIKELY(should_layout_remaining_items_with_zero_block_stretch_size)) {
    // "If LNotToStretch is empty, perform layout with stretch size constraint
    // 0 on all the items of LToStretch."
    for (NGLayoutInputNode child = Node().FirstChild(); child;
         child = child.NextSibling()) {
      if (child.IsOutOfFlowPositioned())
        continue;
      DCHECK(IsStretchyOperatorWithBlockStretchAxis(To<NGBlockNode>(child)));
      NGConstraintSpace::MathTargetStretchBlockSizes zero_stretch_sizes;
      const auto child_constraint_space = CreateConstraintSpaceForMathChild(
          Node(), ChildAvailableSize(), ConstraintSpace(), child,
          NGCacheSlot::kMeasure, zero_stretch_sizes);
      const auto child_layout_result = To<NGBlockNode>(child).Layout(
          child_constraint_space, nullptr /* break_token */);
      UpdateBlockStretchSizes(child_layout_result);
    }
  }

  // Layout in-flow children in a row.
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
    // TODO(crbug.com/1124298): If there is already a stretch constraint, use
    // for the child_constraint_space.
    const auto child_constraint_space =
        IsStretchyOperatorWithBlockStretchAxis(To<NGBlockNode>(child))
            ? CreateConstraintSpaceForMathChild(
                  Node(), ChildAvailableSize(), ConstraintSpace(), child,
                  NGCacheSlot::kLayout, stretch_sizes)
            : CreateConstraintSpaceForMathChild(Node(), ChildAvailableSize(),
                                                ConstraintSpace(), child);
    const auto child_layout_result = To<NGBlockNode>(child).Layout(
        child_constraint_space, nullptr /* break_token */);
    LayoutUnit lspace, rspace;
    DetermineOperatorSpacing(To<NGBlockNode>(child), &lspace, &rspace);
    const NGPhysicalFragment& physical_fragment =
        child_layout_result->PhysicalFragment();
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           To<NGPhysicalBoxFragment>(physical_fragment));

    NGBoxStrut margins = ComputeMarginsFor(child_constraint_space,
                                           child.Style(), ConstraintSpace());
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
    container_builder_.AddChild(*child_data.fragment, child_data.offset);
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
    const MinMaxSizesFloatInput&) const {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    if (child.IsOutOfFlowPositioned())
      continue;
    const auto child_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), ConstraintSpace(), To<NGBlockNode>(child),
        ChildAvailableSize().block_size);
    sizes += child_result.sizes;

    LayoutUnit lspace, rspace;
    DetermineOperatorSpacing(To<NGBlockNode>(child), &lspace, &rspace);
    sizes += lspace + rspace;
    depends_on_block_constraints |= child_result.depends_on_block_constraints;

    // TODO(crbug.com/1125136): take into account italic correction.
  }

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  sizes.Encompass(LayoutUnit());

  DCHECK_LE(sizes.min_size, sizes.max_size);
  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
