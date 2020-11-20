// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_radical_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

namespace blink {

namespace {

bool HasBaseGlyphForRadical(const ComputedStyle& style) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  return font_data && font_data->GlyphForCharacter(kSquareRootCharacter);
}

}  // namespace

NGMathRadicalLayoutAlgorithm::NGMathRadicalLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

void NGMathRadicalLayoutAlgorithm::GatherChildren(
    NGBlockNode* base,
    NGBlockNode* index,
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
    if (!*base) {
      *base = block_child;
      continue;
    }
    if (!*index) {
      *index = block_child;
      continue;
    }

    NOTREACHED();
  }

  if (Node().HasIndex()) {
    DCHECK(*base);
    DCHECK(*index);
  }
}

scoped_refptr<const NGLayoutResult> NGMathRadicalLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  DCHECK(IsValidMathMLRadical(Node()));

  auto vertical = GetRadicalVerticalParameters(Style(), Node().HasIndex());
  scoped_refptr<const NGPhysicalBoxFragment> index_fragment, base_fragment;
  LayoutUnit index_inline_size, index_ascent, index_descent, base_ascent,
      base_descent;
  RadicalHorizontalParameters horizontal;
  NGBoxStrut index_margins, base_margins;
  NGBlockNode base = nullptr;
  NGBlockNode index = nullptr;
  GatherChildren(&base, &index, &container_builder_);

  if (base) {
    // Handle layout of base child. For <msqrt> the base is anonymous and uses
    // the row layout algorithm.
    NGConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), base);
    scoped_refptr<const NGLayoutResult> base_layout_result =
        base.Layout(constraint_space);
    base_fragment =
        &To<NGPhysicalBoxFragment>(base_layout_result->PhysicalFragment());
    base_margins =
        ComputeMarginsFor(constraint_space, base.Style(), ConstraintSpace());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           *base_fragment);
    base_ascent = base_margins.block_start + fragment.BaselineOrSynthesize();
    base_descent = fragment.BlockSize() + base_margins.BlockSum() - base_ascent;
  }
  if (index) {
    // Handle layout of index child.
    // (https://mathml-refresh.github.io/mathml-core/#root-with-index).
    NGConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), index);
    scoped_refptr<const NGLayoutResult> index_layout_result =
        index.Layout(constraint_space);
    index_fragment =
        &To<NGPhysicalBoxFragment>(index_layout_result->PhysicalFragment());
    index_margins =
        ComputeMarginsFor(constraint_space, index.Style(), ConstraintSpace());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           *index_fragment);
    index_inline_size = fragment.InlineSize() + index_margins.InlineSum();
    index_ascent = index_margins.block_start + fragment.BaselineOrSynthesize();
    index_descent =
        fragment.BlockSize() + index_margins.BlockSum() - index_ascent;
    horizontal = GetRadicalHorizontalParameters(Style());
    horizontal.kern_before_degree =
        std::max(horizontal.kern_before_degree, LayoutUnit());
    horizontal.kern_after_degree =
        std::max(horizontal.kern_after_degree, -index_inline_size);
  }

  StretchyOperatorShaper::Metrics surd_metrics;
  if (HasBaseGlyphForRadical(Style())) {
    // Stretch the radical operator to cover the base height.
    StretchyOperatorShaper shaper(kSquareRootCharacter,
                                  OpenTypeMathStretchData::Vertical);
    float target_size = base_ascent + base_descent + vertical.vertical_gap +
                        vertical.rule_thickness;
    scoped_refptr<ShapeResult> shape_result =
        shaper.Shape(&Style().GetFont(), target_size, &surd_metrics);
    scoped_refptr<ShapeResultView> shape_result_view =
        ShapeResultView::Create(shape_result.get());
    LayoutUnit operator_inline_offset = index_inline_size +
                                        horizontal.kern_before_degree +
                                        horizontal.kern_after_degree;
    container_builder_.SetMathMLPaintInfo(
        std::move(shape_result_view), LayoutUnit(surd_metrics.advance),
        LayoutUnit(surd_metrics.ascent), LayoutUnit(surd_metrics.descent),
        operator_inline_offset, base_margins);
  }

  // Determine the metrics of the radical operator + the base.
  LayoutUnit radical_operator_block_size =
      LayoutUnit(surd_metrics.ascent + surd_metrics.descent);

  LayoutUnit index_bottom_raise =
      LayoutUnit(vertical.degree_bottom_raise_percent) *
      radical_operator_block_size;
  LayoutUnit radical_ascent = base_ascent + vertical.vertical_gap +
                              vertical.rule_thickness + vertical.extra_ascender;
  LayoutUnit ascent = radical_ascent;
  LayoutUnit descent =
      std::max(base_descent,
               radical_operator_block_size + vertical.extra_ascender - ascent);
  if (index) {
    ascent = std::max(
        ascent, -descent + index_bottom_raise + index_descent + index_ascent);
    descent = std::max(
        descent, descent - index_bottom_raise + index_descent + index_ascent);
  }
  ascent += BorderScrollbarPadding().block_start;

  if (base) {
    LogicalOffset base_offset = {
        BorderScrollbarPadding().inline_start +
            LayoutUnit(surd_metrics.advance) + index_inline_size +
            horizontal.kern_before_degree + horizontal.kern_after_degree +
            base_margins.inline_start,
        base_margins.block_start - base_ascent + ascent};
    container_builder_.AddChild(To<NGPhysicalContainerFragment>(*base_fragment),
                                base_offset);
    base.StoreMargins(ConstraintSpace(), base_margins);
  }
  if (index) {
    LogicalOffset index_offset = {
        BorderScrollbarPadding().inline_start + index_margins.inline_start +
            horizontal.kern_before_degree,
        index_margins.block_start + ascent + descent - index_bottom_raise -
            index_descent - index_ascent};
    container_builder_.AddChild(
        To<NGPhysicalContainerFragment>(*index_fragment), index_offset);
    index.StoreMargins(ConstraintSpace(), index_margins);
  }

  container_builder_.SetBaseline(ascent);

  auto total_block_size = ascent + descent + BorderScrollbarPadding().block_end;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      ConstraintSpace(), Style(), BorderPadding(), total_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(total_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  NGOutOfFlowLayoutPart(Node(), ConstraintSpace(), &container_builder_).Run();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGMathRadicalLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesInput& input) {
  DCHECK(IsValidMathMLRadical(Node()));

  NGBlockNode base = nullptr;
  NGBlockNode index = nullptr;
  GatherChildren(&base, &index);

  MinMaxSizes sizes;
  bool depends_on_percentage_block_size = false;
  if (index) {
    auto horizontal = GetRadicalHorizontalParameters(Style());
    sizes += horizontal.kern_before_degree.ClampNegativeToZero();
    MinMaxSizesResult index_result =
        ComputeMinAndMaxContentContribution(Style(), index, input);
    NGBoxStrut index_margins = ComputeMinMaxMargins(Style(), index);
    index_result.sizes += index_margins.InlineSum();
    depends_on_percentage_block_size |=
        index_result.depends_on_percentage_block_size;
    sizes += index_result.sizes;
    // kern_after_degree decreases the inline size, but is capped by the index
    // content inline size.
    sizes.min_size +=
        std::max(-index_result.sizes.min_size, horizontal.kern_after_degree);
    sizes.max_size +=
        std::max(index_result.sizes.max_size, horizontal.kern_after_degree);
  }
  if (base) {
    if (HasBaseGlyphForRadical(Style())) {
      sizes += GetMinMaxSizesForVerticalStretchyOperator(Style(),
                                                         kSquareRootCharacter);
    }
    MinMaxSizesResult base_result =
        ComputeMinAndMaxContentContribution(Style(), base, input);
    NGBoxStrut base_margins = ComputeMinMaxMargins(Style(), base);
    base_result.sizes += base_margins.InlineSum();
    depends_on_percentage_block_size |=
        base_result.depends_on_percentage_block_size;
    sizes += base_result.sizes;
  }
  sizes += BorderScrollbarPadding().InlineSum();

  return {sizes, depends_on_percentage_block_size};
}

}  // namespace blink
