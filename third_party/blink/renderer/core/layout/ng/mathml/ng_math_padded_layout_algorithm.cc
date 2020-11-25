// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_padded_layout_algorithm.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/mathml_names.h"

namespace blink {

NGMathPaddedLayoutAlgorithm::NGMathPaddedLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {}

LayoutUnit NGMathPaddedLayoutAlgorithm::RequestedLSpace() const {
  return std::max(LayoutUnit(),
                  ValueForLength(Style().GetMathLSpace(), LayoutUnit()));
}

LayoutUnit NGMathPaddedLayoutAlgorithm::RequestedVOffset() const {
  return ValueForLength(Style().GetMathPaddedVOffset(), LayoutUnit());
}

base::Optional<LayoutUnit> NGMathPaddedLayoutAlgorithm::RequestedAscent(
    LayoutUnit content_ascent) const {
  if (Style().GetMathBaseline().IsAuto())
    return base::nullopt;
  return std::max(LayoutUnit(),
                  ValueForLength(Style().GetMathBaseline(), content_ascent));
}

base::Optional<LayoutUnit> NGMathPaddedLayoutAlgorithm::RequestedDescent(
    LayoutUnit content_descent) const {
  if (Style().GetMathPaddedDepth().IsAuto())
    return base::nullopt;
  return std::max(LayoutUnit(), ValueForLength(Style().GetMathPaddedDepth(),
                                               content_descent));
}

void NGMathPaddedLayoutAlgorithm::GatherChildren(
    NGBlockNode* content,
    NGBoxFragmentBuilder* container_builder) const {
  for (NGLayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    NGBlockNode block_child = To<NGBlockNode>(child);
    if (child.IsOutOfFlowPositioned()) {
      if (container_builder) {
        container_builder->AddOutOfFlowChildCandidate(
            block_child, BorderScrollbarPadding().StartOffset());
      }
      continue;
    }
    if (!*content) {
      *content = block_child;
      continue;
    }

    NOTREACHED();
  }
}

scoped_refptr<const NGLayoutResult> NGMathPaddedLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());

  NGBlockNode content = nullptr;
  GatherChildren(&content, &container_builder_);
  LayoutUnit content_ascent, content_descent;
  NGBoxStrut content_margins;
  scoped_refptr<const NGPhysicalBoxFragment> content_fragment;
  if (content) {
    NGConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), content);
    scoped_refptr<const NGLayoutResult> content_layout_result =
        content.Layout(constraint_space);
    content_fragment =
        &To<NGPhysicalBoxFragment>(content_layout_result->PhysicalFragment());
    content_margins =
        ComputeMarginsFor(constraint_space, content.Style(), ConstraintSpace());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           *content_fragment);
    content_ascent = content_margins.block_start +
                     fragment.Baseline().value_or(fragment.BlockSize());
    content_descent =
        fragment.BlockSize() + content_margins.BlockSum() - content_ascent;
  }
  // width/height/depth attributes can override width/ascent/descent.
  LayoutUnit ascent = BorderScrollbarPadding().block_start +
                      RequestedAscent(content_ascent).value_or(content_ascent);
  LayoutUnit descent =
      RequestedDescent(content_descent).value_or(content_descent);
  if (content_fragment) {
    // Need to take into account border/padding, lspace and voffset.
    LogicalOffset content_offset = {
        BorderScrollbarPadding().inline_start + RequestedLSpace(),
        (ascent - content_ascent) - RequestedVOffset()};
    container_builder_.AddChild(*content_fragment, content_offset);
    content.StoreMargins(ConstraintSpace(), content_margins);
  }

  auto total_block_size = ascent + descent + BorderScrollbarPadding().block_end;
  container_builder_.SetIntrinsicBlockSize(total_block_size);
  container_builder_.SetFragmentsTotalBlockSize(total_block_size);

  container_builder_.SetBaseline(ascent);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathPaddedLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) const {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;
  sizes += BorderScrollbarPadding().InlineSum();

  NGBlockNode content = nullptr;
  GatherChildren(&content);

  MinMaxSizesResult content_result =
      ComputeMinAndMaxContentContribution(Style(), content, input);
  NGBoxStrut content_margins = ComputeMinMaxMargins(Style(), content);
  content_result.sizes += content_margins.InlineSum();
  depends_on_percentage_block_size |=
      content_result.depends_on_percentage_block_size;
  sizes += content_result.sizes;

  return {sizes, depends_on_percentage_block_size};
}

}  // namespace blink
