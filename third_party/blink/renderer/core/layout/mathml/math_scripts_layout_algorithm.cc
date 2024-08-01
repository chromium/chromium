// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/mathml/math_scripts_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {
namespace {

using MathConstants = OpenTypeMathSupport::MathConstants;

static bool IsPrescriptDelimiter(const BlockNode& blockNode) {
  auto* node = blockNode.GetDOMNode();
  return node && IsA<MathMLElement>(node) &&
         node->HasTagName(mathml_names::kMprescriptsTag);
}

LayoutUnit GetSpaceAfterScript(const ComputedStyle& style) {
  return LayoutUnit(MathConstant(style, MathConstants::kSpaceAfterScript)
                        .value_or(style.FontSize() / 5));
}

// Describes the amount of shift to apply to the sub/sup boxes.
// Data is populated from the OpenType MATH table.
// If the OpenType MATH table is not present fallback values are used.
// https://w3c.github.io/mathml-core/#base-with-subscript
// https://w3c.github.io/mathml-core/#base-with-superscript
// https://w3c.github.io/mathml-core/#base-with-subscript-and-superscript
struct ScriptsVerticalParameters {
  STACK_ALLOCATED();

 public:
  LayoutUnit subscript_shift_down;
  LayoutUnit superscript_shift_up;
  LayoutUnit superscript_shift_up_cramped;
  LayoutUnit subscript_baseline_drop_min;
  LayoutUnit superscript_baseline_drop_max;
  LayoutUnit sub_superscript_gap_min;
  LayoutUnit superscript_bottom_min;
  LayoutUnit subscript_top_max;
  LayoutUnit superscript_bottom_max_with_subscript;
};

ScriptsVerticalParameters GetScriptsVerticalParameters(
    const ComputedStyle& style) {
  ScriptsVerticalParameters parameters;
  const SimpleFontData* font_data = style.GetFont().PrimaryFont();
  if (!font_data)
    return parameters;
  auto x_height = font_data->GetFontMetrics().XHeight();
  parameters.subscript_shift_down =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptShiftDown)
                     .value_or(x_height / 3));
  parameters.superscript_shift_up =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptShiftUp)
                     .value_or(x_height));
  parameters.superscript_shift_up_cramped =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptShiftUpCramped)
                     .value_or(x_height));
  parameters.subscript_baseline_drop_min =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptBaselineDropMin)
                     .value_or(x_height / 2));
  parameters.superscript_baseline_drop_max =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptBaselineDropMax)
                     .value_or(x_height / 2));
  parameters.sub_superscript_gap_min =
      LayoutUnit(MathConstant(style, MathConstants::kSubSuperscriptGapMin)
                     .value_or(style.FontSize() / 5));
  parameters.superscript_bottom_min =
      LayoutUnit(MathConstant(style, MathConstants::kSuperscriptBottomMin)
                     .value_or(x_height / 4));
  parameters.subscript_top_max =
      LayoutUnit(MathConstant(style, MathConstants::kSubscriptTopMax)
                     .value_or(4 * x_height / 5));
  parameters.superscript_bottom_max_with_subscript = LayoutUnit(
      MathConstant(style, MathConstants::kSuperscriptBottomMaxWithSubscript)
          .value_or(4 * x_height / 5));
  return parameters;
}

}  // namespace

MathScriptsLayoutAlgorithm::MathScriptsLayoutAlgorithm(
    const LayoutAlgorithmParams& params)
    : LayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());
}

void MathScriptsLayoutAlgorithm::GatherChildren(
    BlockNode* base,
    HeapVector<SubSupPair>* sub_sup_pairs,
    BlockNode* prescripts,
    unsigned* first_prescript_index,
    BoxFragmentBuilder* container_builder) const {
  auto script_type = Node().ScriptType();
  bool number_of_scripts_is_even = true;
  sub_sup_pairs->resize(1);
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
      // All scripted elements must have at least one child.
      // The first child is the base.
      *base = block_child;
      continue;
    }
    switch (script_type) {
      case MathScriptType::kSub:
      case MathScriptType::kUnder:
        // These elements must have exactly two children.
        // The second child is a postscript and there are no prescripts.
        // <msub> base subscript </msub>
        // <msup> base superscript </msup>
        DCHECK(!sub_sup_pairs->at(0).sub);
        sub_sup_pairs->at(0).sub = block_child;
        continue;
      case MathScriptType::kSuper:
      case MathScriptType::kOver:
        DCHECK(!sub_sup_pairs->at(0).sup);
        sub_sup_pairs->at(0).sup = block_child;
        continue;
      case MathScriptType::kUnderOver:
      case MathScriptType::kSubSup:
        // These elements must have exactly three children.
        // The second and third children are postscripts and there are no
        // prescripts. <msubsup> base subscript superscript </msubsup>
        if (!sub_sup_pairs->at(0).sub) {
          sub_sup_pairs->at(0).sub = block_child;
        } else {
          DCHECK(!sub_sup_pairs->at(0).sup);
          sub_sup_pairs->at(0).sup = block_child;
        }
        continue;
      case MathScriptType::kMultiscripts: {
        // The structure of mmultiscripts is specified here:
        // https://w3c.github.io/mathml-core/#prescripts-and-tensor-indices-mmultiscripts
        if (IsPrescriptDelimiter(block_child)) {
          if (!number_of_scripts_is_even || *prescripts) {
            NOTREACHED_IN_MIGRATION();
            return;
          }
          *first_prescript_index = sub_sup_pairs->size() - 1;
          *prescripts = block_child;
          continue;
        }
        if (!sub_sup_pairs->back().sub) {
          sub_sup_pairs->back().sub = block_child;
        } else {
          DCHECK(!sub_sup_pairs->back().sup);
          sub_sup_pairs->back().sup = block_child;
        }
        number_of_scripts_is_even = !number_of_scripts_is_even;
        if (number_of_scripts_is_even)
          sub_sup_pairs->resize(sub_sup_pairs->size() + 1);
        continue;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
  DCHECK(number_of_scripts_is_even);
}

// Determines ascent/descent and shift metrics depending on script type.
MathScriptsLayoutAlgorithm::VerticalMetrics
MathScriptsLayoutAlgorithm::GetVerticalMetrics(
    const ChildAndMetrics& base_metrics,
    const ChildrenAndMetrics& sub_metrics,
    const ChildrenAndMetrics& sup_metrics) const {
  ScriptsVerticalParameters parameters = GetScriptsVerticalParameters(Style());
  VerticalMetrics metrics;

  MathScriptType type = Node().ScriptType();
  if (type == MathScriptType::kSub || type == MathScriptType::kSubSup ||
      type == MathScriptType::kMultiscripts || type == MathScriptType::kUnder ||
      type == MathScriptType::kMultiscripts) {
    metrics.sub_shift =
        std::max(parameters.subscript_shift_down,
                 base_metrics.descent + parameters.subscript_baseline_drop_min);
  }
  LayoutUnit shift_up = parameters.superscript_shift_up;
  if (type == MathScriptType::kSuper || type == MathScriptType::kSubSup ||
      type == MathScriptType::kMultiscripts || type == MathScriptType::kOver ||
      type == MathScriptType::kMultiscripts) {
    if (Style().MathShift() == EMathShift::kCompact)
      shift_up = parameters.superscript_shift_up_cramped;
    metrics.sup_shift =
        std::max(shift_up, base_metrics.ascent -
                               parameters.superscript_baseline_drop_max);
  }

  switch (type) {
    case MathScriptType::kSub:
    case MathScriptType::kUnder: {
      metrics.descent = sub_metrics[0].descent;
      metrics.sub_shift =
          std::max(metrics.sub_shift,
                   sub_metrics[0].ascent - parameters.subscript_top_max);
    } break;
    case MathScriptType::kSuper:
    case MathScriptType::kOver: {
      metrics.ascent = sup_metrics[0].ascent;
      metrics.sup_shift =
          std::max(metrics.sup_shift,
                   parameters.superscript_bottom_min + sup_metrics[0].descent);
    } break;
    case MathScriptType::kMultiscripts:
    case MathScriptType::kUnderOver:
    case MathScriptType::kSubSup: {
      for (wtf_size_t idx = 0; idx < sub_metrics.size(); ++idx) {
        metrics.ascent = std::max(metrics.ascent, sup_metrics[idx].ascent);
        metrics.descent = std::max(metrics.descent, sub_metrics[idx].descent);
        LayoutUnit sub_script_shift = std::max(
            parameters.subscript_shift_down,
            base_metrics.descent + parameters.subscript_baseline_drop_min);
        sub_script_shift =
            std::max(sub_script_shift,
                     sub_metrics[idx].ascent - parameters.subscript_top_max);
        LayoutUnit sup_script_shift =
            std::max(shift_up, base_metrics.ascent -
                                   parameters.superscript_baseline_drop_max);
        sup_script_shift =
            std::max(sup_script_shift, parameters.superscript_bottom_min +
                                           sup_metrics[idx].descent);

        LayoutUnit sub_super_script_gap =
            (sub_script_shift - sub_metrics[idx].ascent) +
            (sup_script_shift - sup_metrics[idx].descent);
        if (sub_super_script_gap < parameters.sub_superscript_gap_min) {
          // First, we try and push the superscript up.
          LayoutUnit delta = parameters.superscript_bottom_max_with_subscript -
                             (sup_script_shift - sup_metrics[idx].descent);
          if (delta > 0) {
            delta = std::min(delta, parameters.sub_superscript_gap_min -
                                        sub_super_script_gap);
            sup_script_shift += delta;
            sub_super_script_gap += delta;
          }
          // If that is not enough, we push the subscript down.
          if (sub_super_script_gap < parameters.sub_superscript_gap_min) {
            sub_script_shift +=
                parameters.sub_superscript_gap_min - sub_super_script_gap;
          }
        }

        metrics.sub_shift = std::max(metrics.sub_shift, sub_script_shift);
        metrics.sup_shift = std::max(metrics.sup_shift, sup_script_shift);
      }
    } break;
  }

  return metrics;
}

MathScriptsLayoutAlgorithm::ChildAndMetrics
MathScriptsLayoutAlgorithm::LayoutAndGetMetrics(BlockNode child) const {
  ChildAndMetrics child_and_metrics;
  auto constraint_space = CreateConstraintSpaceForMathChild(
      Node(), ChildAvailableSize(), GetConstraintSpace(), child);
  child_and_metrics.result =
      child.Layout(constraint_space, nullptr /*break_token*/);
  LogicalBoxFragment fragment(
      GetConstraintSpace().GetWritingDirection(),
      To<PhysicalBoxFragment>(child_and_metrics.result->GetPhysicalFragment()));
  child_and_metrics.inline_size = fragment.InlineSize();
  child_and_metrics.margins =
      ComputeMarginsFor(constraint_space, child.Style(), GetConstraintSpace());
  child_and_metrics.ascent =
      fragment.FirstBaselineOrSynthesize(Style().GetFontBaseline());
  child_and_metrics.descent = fragment.BlockSize() - child_and_metrics.ascent +
                              child_and_metrics.margins.block_end;
  child_and_metrics.ascent += child_and_metrics.margins.block_start;
  child_and_metrics.node = child;
  return child_and_metrics;
}

const LayoutResult* MathScriptsLayoutAlgorithm::Layout() {
  DCHECK(!GetBreakToken());

  BlockNode base = nullptr;
  BlockNode prescripts = nullptr;
  wtf_size_t first_prescript_index = 0;

  HeapVector<SubSupPair> sub_sup_pairs;
  ClearCollectionScope<HeapVector<SubSupPair>> scope(&sub_sup_pairs);

  GatherChildren(&base, &sub_sup_pairs, &prescripts, &first_prescript_index,
                 &container_builder_);
  ChildrenAndMetrics sub_metrics, sup_metrics;
  ChildAndMetrics prescripts_metrics;
  if (prescripts)
    prescripts_metrics = LayoutAndGetMetrics(prescripts);
  for (auto sub_sup_pair : sub_sup_pairs) {
    if (sub_sup_pair.sub)
      sub_metrics.emplace_back(LayoutAndGetMetrics(sub_sup_pair.sub));
    if (sub_sup_pair.sup)
      sup_metrics.emplace_back(LayoutAndGetMetrics(sub_sup_pair.sup));
  }

  ChildAndMetrics base_metrics = LayoutAndGetMetrics(base);
  VerticalMetrics metrics =
      GetVerticalMetrics(base_metrics, sub_metrics, sup_metrics);

  const LayoutUnit ascent =
      std::max(base_metrics.ascent, metrics.ascent + metrics.sup_shift)
          .ClampNegativeToZero() +
      BorderScrollbarPadding().block_start;
  const LayoutUnit descent =
      std::max(base_metrics.descent, metrics.descent + metrics.sub_shift)
          .ClampNegativeToZero() +
      BorderScrollbarPadding().block_end;

  LayoutUnit base_italic_correction = std::min(
      base_metrics.inline_size, base_metrics.result->MathItalicCorrection());
  LayoutUnit inline_offset = BorderScrollbarPadding().inline_start;

  LayoutUnit space = GetSpaceAfterScript(Style());
  // Position pre scripts if needed.
  if (prescripts) {
    for (wtf_size_t idx = first_prescript_index; idx < sub_metrics.size();
         ++idx) {
      auto& sub_metric = sub_metrics[idx];
      auto& sup_metric = sup_metrics[idx];
      LayoutUnit sub_sup_pair_inline_size =
          std::max(sub_metric.inline_size, sup_metric.inline_size);
      inline_offset += space + sub_sup_pair_inline_size;
      LogicalOffset sub_offset(inline_offset - sub_metric.inline_size +
                                   sub_metric.margins.inline_start,
                               ascent + metrics.sub_shift - sub_metric.ascent +
                                   sub_metric.margins.block_start);
      container_builder_.AddResult(*sub_metric.result, sub_offset,
                                   sub_metric.margins);
      LogicalOffset sup_offset(inline_offset - sup_metric.inline_size +
                                   sup_metric.margins.inline_start,
                               ascent - metrics.sup_shift - sup_metric.ascent +
                                   sup_metric.margins.block_start);
      container_builder_.AddResult(*sup_metric.result, sup_offset,
                                   sup_metric.margins);
    }
  } else {
    first_prescript_index = std::max(sub_metrics.size(), sup_metrics.size());
  }
  inline_offset += base_metrics.margins.inline_start;
  LogicalOffset base_offset(
      inline_offset,
      ascent - base_metrics.ascent + base_metrics.margins.block_start);
  container_builder_.AddResult(*base_metrics.result, base_offset,
                               base_metrics.margins);
  if (prescripts) {
    LogicalOffset prescripts_offset(inline_offset,
                                    ascent - prescripts_metrics.ascent +
                                        prescripts_metrics.margins.block_start);
    container_builder_.AddResult(*prescripts_metrics.result, prescripts_offset,
                                 prescripts_metrics.margins);
  }
  inline_offset += base_metrics.inline_size + base_metrics.margins.inline_end;

  // Position post scripts if needed.
  for (unsigned idx = 0; idx < first_prescript_index; ++idx) {
    ChildAndMetrics sub_metric, sup_metric;
    if (idx < sub_metrics.size())
      sub_metric = sub_metrics[idx];
    if (idx < sup_metrics.size())
      sup_metric = sup_metrics[idx];

    if (sub_metric.node) {
      LogicalOffset sub_offset(
          LayoutUnit(inline_offset + sub_metric.margins.inline_start -
                     base_italic_correction)
              .ClampNegativeToZero(),
          ascent + metrics.sub_shift - sub_metric.ascent +
              sub_metric.margins.block_start);
      container_builder_.AddResult(*sub_metric.result, sub_offset,
                                   sub_metric.margins);
    }
    if (sup_metric.node) {
      LogicalOffset sup_offset(inline_offset + sup_metric.margins.inline_start,
                               ascent - metrics.sup_shift - sup_metric.ascent +
                                   sup_metric.margins.block_start);
      container_builder_.AddResult(*sup_metric.result, sup_offset,
                                   sup_metric.margins);
    }
    LayoutUnit sub_sup_pair_inline_size =
        std::max(sub_metric.inline_size, sup_metric.inline_size);
    inline_offset += space + sub_sup_pair_inline_size;
  }

  container_builder_.SetBaselines(ascent);

  LayoutUnit intrinsic_block_size = ascent + descent;

  LayoutUnit block_size = ComputeBlockSizeForFragment(
      GetConstraintSpace(), Node(), BorderPadding(), intrinsic_block_size,
      container_builder_.InitialBorderBoxSize().inline_size);

  container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  container_builder_.HandleOofsAndSpecialDescendants();

  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult MathScriptsLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  if (auto result = CalculateMinMaxSizesIgnoringChildren(
          Node(), BorderScrollbarPadding()))
    return *result;

  BlockNode base = nullptr;
  BlockNode prescripts = nullptr;
  unsigned first_prescript_index = 0;

  HeapVector<SubSupPair> sub_sup_pairs;
  ClearCollectionScope<HeapVector<SubSupPair>> scope(&sub_sup_pairs);

  GatherChildren(&base, &sub_sup_pairs, &prescripts, &first_prescript_index);
  DCHECK_GE(sub_sup_pairs.size(), 1ul);

  MinMaxSizes sizes;
  bool depends_on_block_constraints = false;

  // TODO(layout-dev): Determine the italic-correction without calling layout
  // within ComputeMinMaxSizes, (or setup in an interoperable constraint-space).
  LayoutUnit base_italic_correction;
  const auto base_result = ComputeMinAndMaxContentContributionForMathChild(
      Style(), GetConstraintSpace(), base, ChildAvailableSize().block_size);

  sizes = base_result.sizes;
  depends_on_block_constraints |= base_result.depends_on_block_constraints;

  LayoutUnit space = GetSpaceAfterScript(Style());
  switch (Node().ScriptType()) {
    case MathScriptType::kSub:
    case MathScriptType::kUnder:
    case MathScriptType::kOver:
    case MathScriptType::kSuper: {
      const BlockNode sub = sub_sup_pairs[0].sub;
      const BlockNode sup = sub_sup_pairs[0].sup;
      const auto first_post_script = sub ? sub : sup;
      const auto first_post_script_result =
          ComputeMinAndMaxContentContributionForMathChild(
              Style(), GetConstraintSpace(), first_post_script,
              ChildAvailableSize().block_size);

      sizes += first_post_script_result.sizes;
      if (sub)
        sizes -= base_italic_correction;
      sizes += space;
      depends_on_block_constraints |=
          first_post_script_result.depends_on_block_constraints;
      break;
    }
    case MathScriptType::kSubSup:
    case MathScriptType::kUnderOver:
    case MathScriptType::kMultiscripts: {
      MinMaxSizes sub_sup_pair_size;
      unsigned index = 0;
      do {
        const auto sub = sub_sup_pairs[index].sub;
        if (!sub)
          continue;
        auto sub_result = ComputeMinAndMaxContentContributionForMathChild(
            Style(), GetConstraintSpace(), sub,
            ChildAvailableSize().block_size);
        sub_result.sizes -= base_italic_correction;
        sub_sup_pair_size.Encompass(sub_result.sizes);

        const auto sup = sub_sup_pairs[index].sup;
        if (!sup)
          continue;
        const auto sup_result = ComputeMinAndMaxContentContributionForMathChild(
            Style(), GetConstraintSpace(), sup,
            ChildAvailableSize().block_size);
        sub_sup_pair_size.Encompass(sup_result.sizes);

        sizes += sub_sup_pair_size;
        sizes += space;
        depends_on_block_constraints |= sub_result.depends_on_block_constraints;
        depends_on_block_constraints |= sup_result.depends_on_block_constraints;
      } while (++index < sub_sup_pairs.size());
      break;
    }
  }

  sizes += BorderScrollbarPadding().InlineSum();
  return MinMaxSizesResult(sizes, depends_on_block_constraints);
}

}  // namespace blink
