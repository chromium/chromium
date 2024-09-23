// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_radical_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

namespace blink {

namespace {

bool HasBaseGlyphForRadical(const ComputedStyle& style) {
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  return font_data && font_data->GlyphForCharacter(kSquareRootCharacter);
}

}  // namespace

MathRadicalLayoutAlgorithm::MathRadicalLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

void MathRadicalLayoutAlgorithm::GatherChildren(
    BlockNode* base,
    BlockNode* index,
    BoxFragmentBuilder* container_builder) const {
  for (LayoutInputNode child = Node().FirstChild(); child;
       child = child.NextSibling()) {
    BlockNode block_child = To<BlockNode>(child);
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

    NOTREACHED_IN_MIGRATION();
  }

  if (Node().HasIndex()) {
    DCHECK(*base);
    DCHECK(*index);
  }
}

const LayoutResult* MathRadicalLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());
  DCHECK(IsValidMathMLRadical(Node()));

  const auto baseline_type = Style().GetFontBaseline();
  const auto vertical =
      GetRadicalVerticalParameters(Style(), Node().HasIndex());

  LayoutUnit index_inline_size, index_ascent, index_descent, base_ascent,
      base_descent;
  RadicalHorizontalParameters horizontal;
  BoxStrut index_margins, base_margins;
  BlockNode base = nullptr;
  BlockNode index = nullptr;
  GatherChildren(&base, &index, &container_builder_);

  const LayoutResult* base_layout_result = nullptr;
  const LayoutResult* index_layout_result = nullptr;
  if (base) {
    // Handle layout of base child. For <msqrt> the base is anonymous and uses
    // the row layout algorithm.
    ConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), GetConstraintSpace(), base);
    base_layout_result = base.Layout(constraint_space);
    const auto& base_fragment =
        To<PhysicalBoxFragment>(base_layout_result->GetPhysicalFragment());
    base_margins =
        ComputeMarginsFor(constraint_space, base.Style(), GetConstraintSpace());
    LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
                                base_fragment);
    base_ascent = base_margins.block_start +
                  fragment.FirstBaselineOrSynthesize(baseline_type);
    base_descent = fragment.BlockSize() + base_margins.BlockSum() - base_ascent;
  }
  if (index) {
    // Handle layout of index child.
    // (https://w3c.github.io/mathml-core/#root-with-index).
    ConstraintSpace constraint_space = CreateConstraintSpaceForMathChild(
        Node(), ChildAvailableSize(), GetConstraintSpace(), index);
    index_layout_result = index.Layout(constraint_space);
    const auto& index_fragment =
        To<PhysicalBoxFragment>(index_layout_result->GetPhysicalFragment());
    index_margins = ComputeMarginsFor(constraint_space, index.Style(),
                                      GetConstraintSpace());
    LogicalBoxFragment fragment(GetConstraintSpace().GetWritingDirection(),
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
    const ShapeResult* shape_result =
        shaper.Shape(&Style().GetFont(), target_size, &surd_metrics);
    const ShapeResultView* shape_result_view =
        ShapeResultView::Create(shape_result);
    LayoutUnit operator_inline_offset = index_inline_size +
                                        horizontal.kern_before_degree +
                                        horizontal.kern_after_degree;
    container_builder_.SetMathMLPaintInfo(MakeGarbageCollected<MathMLPaintInfo>(
        kSquareRootCharacter, shape_result_view,
        LayoutUnit(surd_metrics.advance), LayoutUnit(surd_metrics.ascent),
        LayoutUnit(surd_metrics.descent), base_margins,
        operator_inline_offset));
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
    container_builder_.AddResult(*base_layout_result, base_offset,
                                 base_margins);
  }
  if (index) {
    LogicalOffset index_offset = {
        BorderScrollbarPadding().inline_start + index_margins.inline_start +
            horizontal.kern_before_degree,
        index_margins.block_start + ascent + descent - index_bottom_raise -
            index_descent - index_ascent};
    container_builder_.AddResult(*index_layout_result, index_offset,
                                 index_margins);
  }

  container_builder_.SetBaselines(ascent);

  auto total_block_size = ascent + descent + BorderScrollbarPadding().block_end;
  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), total_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(total_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MathRadicalLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  DCHECK(IsValidMathMLRadical(Node()));

  BlockNode base = nullptr;
  BlockNode index = nullptr;
  GatherChildren(&base, &index);

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;
  if (index) {
    const auto horizontal = GetRadicalHorizontalParameters(Style());
    sizes += horizontal.kern_before_degree.ClampNegativeToZero();

    const auto index_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), GetConstraintSpace(), index, ChildAvailableSize().block_size);
    depends_on_block_constraints |= index_result.depends_on_block_constraints;
    sizes += index_result.sizes;

    // kern_after_degree decreases the inline size, but is capped by the index
    // content inline size.
    sizes.min_size +=
        std::max(-index_result.sizes.min_size, horizontal.kern_after_degree);
    sizes.max_size +=
        std::max(-index_result.sizes.max_size, horizontal.kern_after_degree);
  }
  if (HasBaseGlyphForRadical(Style())) {
    sizes += GetMinMaxSizesForVerticalStretchyOperator(Style(),
                                                       kSquareRootCharacter);
  }
  if (base) {
    const auto base_result = ComputeMinAndMaxContentContributionForMathChild(
        Style(), GetConstraintSpace(), base, ChildAvailableSize().block_size);
    depends_on_block_constraints |= base_result.depends_on_block_constraints;
    sizes += base_result.sizes;
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
