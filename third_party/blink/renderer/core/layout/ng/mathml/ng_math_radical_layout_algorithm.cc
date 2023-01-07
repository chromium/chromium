// Copyright 2020 The Chromium Authors
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

const NGLayoutResult* NGMathRadicalLayoutAlgorithm::Layout() {
  DCHECK(!BreakToken());
  DCHECK(IsValidMathMLRadical(Node()));

  const auto baseline_type = Style().GetFontBaseline();
  const auto vertical =
      GetRadicalVerticalParameters(Style(), Node().HasIndex());

  LayoutUnit index_inline_size, index_ascent, index_descent, base_ascent,
      base_descent;
  RadicalHorizontalParameters horizontal;
  NGBoxStrut index_margins, base_margins;
  NGBlockNode base = nullptr;
  NGBlockNode index = nullptr;
  GatherChildren(&base, &index, &container_builder_);

  const NGLayoutResult* base_layout_result = nullptr;
  const NGLayoutResult* index_layout_result = nullptr;
  if (base) {
    // Handle layout of base child. For <msqrt> the base is anonymous and uses
    // the row layout algorithm.
    NGConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), base);
    base_layout_result = base.Layout(constraint_space);
    const auto& base_fragment =
        To<NGPhysicalBoxFragment>(base_layout_result->PhysicalFragment());
    base_margins =
        ComputeMarginsFor(constraint_space, base.Style(), ConstraintSpace());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           base_fragment);
    base_ascent = base_margins.block_start +
                  fragment.FirstBaselineOrSynthesize(baseline_type);
    base_descent = fragment.BlockSize() + base_margins.BlockSum() - base_ascent;
  }
  if (index) {
    // Handle layout of index child.
    // (https://w3c.github.io/mathml-core/#root-with-index).
    NGConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), ConstraintSpace(), index);
    index_layout_result = index.Layout(constraint_space);
    const auto& index_fragment =
        To<NGPhysicalBoxFragment>(index_layout_result->PhysicalFragment());
    index_margins =
        ComputeMarginsFor(constraint_space, index.Style(), ConstraintSpace());
    NGBoxFragment fragment(ConstraintSpace().GetWritingDirection(),
                           index_fragment);
    index_inline_size = fragment.InlineSize() + index_margins.InlineSum();
    index_ascent = index_margins.block_start +
                   fragment.FirstBaselineOrSynthesize(baseline_type);
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
    container_builder_.AddResult(*base_layout_result, base_offset);
    base.StoreMargins(ConstraintSpace(), base_margins);
  }
  if (index) {
    LogicalOffset index_offset = {
        BorderScrollbarPadding().inline_start + index_margins.inline_start +
            horizontal.kern_before_degree,
        index_margins.block_start + ascent + descent - index_bottom_raise -
            index_descent - index_ascent};
    container_builder_.AddResult(*index_layout_result, index_offset);
    index.StoreMargins(ConstraintSpace(), index_margins);
  }

  container_builder_.SetBaselines(ascent);

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
    const MinMaxSizesFloatInput&) {
  DCHECK(IsValidMathMLRadical(Node()));

  NGBlockNode base = nullptr;
  NGBlockNode index = nullptr;
  GatherChildren(&base, &index);

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;
  if (index) {
    const auto horizontal = GetRadicalHorizontalParameters(Style());
    sizes += horizontal.kern_before_degree.ClampNegativeToZero();

    const auto index_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), ConstraintSpace(), index, ChildAvailableSize().block_size);
    depends_on_block_constraints |= index_result.depends_on_block_constraints;
    sizes += index_result.sizes;

    // kern_after_degree decreases the inline size, but is capped by the index
    // content inline size.
    sizes.min_size +=
        std::max(-index_result.sizes.min_size, horizontal.kern_after_degree);
    sizes.max_size +=
        std::max(-index_result.sizes.max_size, horizontal.kern_after_degree);
  }
  if (base) {
    if (HasBaseGlyphForRadical(Style())) {
      sizes += GetMinMaxSizesForVerticalStretchyOperator(Style(),
                                                         kSquareRootCharacter);
    }
    const auto base_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), ConstraintSpace(), base, ChildAvailableSize().block_size);
    depends_on_block_constraints |= base_result.depends_on_block_constraints;
    sizes += base_result.sizes;
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
