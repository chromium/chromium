// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_marquee_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/box_layout_extra_input.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_table.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/custom/layout_ng_custom.h"
#include "third_party/blink/renderer/core/layout/ng/custom/ng_custom_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_fraction_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_operator_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_padded_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_radical_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_row_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_scripts_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_space_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_under_over_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_simplified_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_borders.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_row_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_section_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_radical_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

namespace {

inline bool HasInlineChildren(LayoutBlockFlow* block_flow) {
  auto* child = GetLayoutObjectForFirstChildNode(block_flow);
  return child && AreNGBlockFlowChildrenInline(block_flow);
}

inline LayoutMultiColumnFlowThread* GetFlowThread(
    const LayoutBlockFlow* block_flow) {
  if (!block_flow)
    return nullptr;
  return block_flow->MultiColumnFlowThread();
}

inline LayoutMultiColumnFlowThread* GetFlowThread(const LayoutBox& box) {
  return GetFlowThread(DynamicTo<LayoutBlockFlow>(box));
}

inline wtf_size_t FragmentIndex(const NGBlockBreakToken* incoming_break_token) {
  if (incoming_break_token && !incoming_break_token->IsBreakBefore())
    return incoming_break_token->SequenceNumber() + 1;
  return 0;
}

// The entire purpose of this function is to avoid allocating space on the stack
// for all layout algorithms for each node we lay out. Therefore it must not be
// inline.
template <typename Algorithm, typename Callback>
NOINLINE void CreateAlgorithmAndRun(const NGLayoutAlgorithmParams& params,
                                    const Callback& callback) {
  Algorithm algorithm(params);
  callback(&algorithm);
}

template <typename Callback>
NOINLINE void DetermineMathMLAlgorithmAndRun(
    const LayoutBox& box,
    const NGLayoutAlgorithmParams& params,
    const Callback& callback) {
  DCHECK(box.IsMathML());
  // Currently math layout algorithms can only apply to MathML elements.
  auto* element = box.GetNode();
  if (element) {
    if (IsA<MathMLSpaceElement>(element)) {
      CreateAlgorithmAndRun<NGMathSpaceLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLFractionElement>(element) &&
               IsValidMathMLFraction(params.node)) {
      CreateAlgorithmAndRun<NGMathFractionLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLRadicalElement>(element) &&
               IsValidMathMLRadical(params.node)) {
      CreateAlgorithmAndRun<NGMathRadicalLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLPaddedElement>(element)) {
      CreateAlgorithmAndRun<NGMathPaddedLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLElement>(element) &&
               To<MathMLElement>(*element).IsTokenElement()) {
      if (IsOperatorWithSpecialShaping(params.node))
        CreateAlgorithmAndRun<NGMathOperatorLayoutAlgorithm>(params, callback);
      else
        CreateAlgorithmAndRun<NGBlockLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLScriptsElement>(element) &&
               IsValidMathMLScript(params.node)) {
      if (IsA<MathMLUnderOverElement>(element) &&
          !IsUnderOverLaidOutAsSubSup(params.node)) {
        CreateAlgorithmAndRun<NGMathUnderOverLayoutAlgorithm>(params, callback);
      } else {
        CreateAlgorithmAndRun<NGMathScriptsLayoutAlgorithm>(params, callback);
      }
      return;
    }
  }
  CreateAlgorithmAndRun<NGMathRowLayoutAlgorithm>(params, callback);
}

template <typename Callback>
NOINLINE void DetermineAlgorithmAndRun(const NGLayoutAlgorithmParams& params,
                                       const Callback& callback) {
  const ComputedStyle& style = params.node.Style();
  const LayoutBox& box = *params.node.GetLayoutBox();
  if (box.IsLayoutNGFlexibleBox()) {
    CreateAlgorithmAndRun<NGFlexLayoutAlgorithm>(params, callback);
  } else if (box.IsTable()) {
    CreateAlgorithmAndRun<NGTableLayoutAlgorithm>(params, callback);
  } else if (box.IsTableRow()) {
    CreateAlgorithmAndRun<NGTableRowLayoutAlgorithm>(params, callback);
  } else if (box.IsTableSection()) {
    CreateAlgorithmAndRun<NGTableSectionLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutNGCustom()) {
    CreateAlgorithmAndRun<NGCustomLayoutAlgorithm>(params, callback);
  } else if (box.IsMathML()) {
    DetermineMathMLAlgorithmAndRun(box, params, callback);
  } else if (box.IsLayoutNGGrid() &&
             RuntimeEnabledFeatures::LayoutNGGridEnabled()) {
    CreateAlgorithmAndRun<NGGridLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutNGFieldset()) {
    CreateAlgorithmAndRun<NGFieldsetLayoutAlgorithm>(params, callback);
    // If there's a legacy layout box, we can only do block fragmentation if
    // we would have done block fragmentation with the legacy engine.
    // Otherwise writing data back into the legacy tree will fail. Look for
    // the flow thread.
  } else if (GetFlowThread(box)) {
    if (style.SpecifiesColumns())
      CreateAlgorithmAndRun<NGColumnLayoutAlgorithm>(params, callback);
    else
      CreateAlgorithmAndRun<NGPageLayoutAlgorithm>(params, callback);
  } else {
    CreateAlgorithmAndRun<NGBlockLayoutAlgorithm>(params, callback);
  }
}

inline scoped_refptr<const NGLayoutResult> LayoutWithAlgorithm(
    const NGLayoutAlgorithmParams& params) {
  scoped_refptr<const NGLayoutResult> result;
  DetermineAlgorithmAndRun(params,
                           [&result](NGLayoutAlgorithmOperations* algorithm) {
                             result = algorithm->Layout();
                           });
  return result;
}

inline MinMaxSizesResult ComputeMinMaxSizesWithAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const MinMaxSizesInput& input) {
  MinMaxSizesResult result;
  DetermineAlgorithmAndRun(
      params, [&result, &input](NGLayoutAlgorithmOperations* algorithm) {
        result = algorithm->ComputeMinMaxSizes(input);
      });
  return result;
}

NGConstraintSpace CreateConstraintSpaceForMinMax(
    const NGBlockNode& node,
    const MinMaxSizesInput& input) {
  NGConstraintSpaceBuilder builder(node.Style().GetWritingMode(),
                                   node.Style().GetWritingMode(),
                                   node.CreatesNewFormattingContext());
  builder.SetTextDirection(node.Style().Direction());
  builder.SetAvailableSize(LogicalSize());
  builder.SetPercentageResolutionSize(
      {LayoutUnit(), input.percentage_resolution_block_size});
  return builder.ToConstraintSpace();
}

LayoutUnit CalculateAvailableInlineSizeForLegacy(
    const LayoutBox& box,
    const NGConstraintSpace& space) {
  if (box.ShouldComputeSizeAsReplaced())
    return space.ReplacedPercentageResolutionInlineSize();

  return space.PercentageResolutionInlineSize();
}

LayoutUnit CalculateAvailableBlockSizeForLegacy(
    const LayoutBox& box,
    const NGConstraintSpace& space) {
  if (box.ShouldComputeSizeAsReplaced())
    return space.ReplacedPercentageResolutionBlockSize();

  return space.PercentageResolutionBlockSize();
}

void SetupBoxLayoutExtraInput(const NGConstraintSpace& space,
                              const LayoutBox& box,
                              BoxLayoutExtraInput* input) {
  input->containing_block_content_inline_size =
      CalculateAvailableInlineSizeForLegacy(box, space);
  input->containing_block_content_block_size =
      CalculateAvailableBlockSizeForLegacy(box, space);

  WritingMode writing_mode = box.StyleRef().GetWritingMode();
  if (LayoutObject* containing_block = box.ContainingBlock()) {
    if (!IsParallelWritingMode(containing_block->StyleRef().GetWritingMode(),
                               writing_mode)) {
      // The sizes should be in the containing block writing mode.
      std::swap(input->containing_block_content_block_size,
                input->containing_block_content_inline_size);

      // We cannot lay out without a definite containing block inline-size. We
      // end up here if we're performing a measure pass (as part of resolving
      // the intrinsic min/max inline-size of some ancestor, for instance).
      // Legacy layout has a tendency of clamping negative sizes to 0 anyway,
      // but this is missing when it comes to resolving percentage-based
      // padding, for instance.
      if (input->containing_block_content_inline_size == kIndefiniteSize)
        input->containing_block_content_inline_size = LayoutUnit();
    }
  }

  // We need a definite containing block inline-size, or we'd be unable to
  // resolve percentages.
  DCHECK_GE(input->containing_block_content_inline_size, LayoutUnit());

  input->available_inline_size = space.AvailableSize().inline_size;

  if (space.IsFixedInlineSize())
    input->override_inline_size = space.AvailableSize().inline_size;
  if (space.IsFixedBlockSize()) {
    input->override_block_size = space.AvailableSize().block_size;
    input->is_override_block_size_definite =
        !space.IsFixedBlockSizeIndefinite();
  }
}

bool CanUseCachedIntrinsicInlineSizes(const MinMaxSizesInput& input,
                                      const NGConstraintSpace& constraint_space,
                                      const NGBlockNode& node) {
  // Obviously can't use the cache if our intrinsic logical widths are dirty.
  if (node.GetLayoutBox()->IntrinsicLogicalWidthsDirty())
    return false;

  // We don't store the float inline sizes for comparison, always skip the
  // cache in this case.
  if (input.float_left_inline_size || input.float_right_inline_size)
    return false;

  // Check if we have any percentage inline padding.
  const auto& style = node.Style();
  if (style.MayHavePadding() && (style.PaddingStart().IsPercentOrCalc() ||
                                 style.PaddingEnd().IsPercentOrCalc()))
    return false;

  if (!style.AspectRatio().IsAuto() &&
      (style.LogicalMinHeight().IsPercentOrCalc() ||
       style.LogicalMaxHeight().IsPercentOrCalc()) &&
      input.percentage_resolution_block_size !=
          node.GetLayoutBox()
              ->IntrinsicLogicalWidthsPercentageResolutionBlockSize())
    return false;

  if (node.IsNGTableCell() && To<LayoutNGTableCell>(node.GetLayoutBox())
                                      ->IntrinsicLogicalWidthsBorderSizes() !=
                                  constraint_space.TableCellBorders())
    return false;

  return true;
}

bool IsContentMinimumInlineSizeZero(const NGBlockNode& block_node) {
  const auto* node = block_node.GetDOMNode();
  const auto* marquee_element = DynamicTo<HTMLMarqueeElement>(node);
  if (marquee_element && marquee_element->IsHorizontal())
    return true;
  if (!block_node.Style().LogicalWidth().IsPercentOrCalc())
    return false;
  if (block_node.IsTextControl())
    return true;
  if (IsA<HTMLSelectElement>(node))
    return true;
  if (const auto* input_element = DynamicTo<HTMLInputElement>(node)) {
    const AtomicString& type = input_element->type();
    if (type == input_type_names::kFile)
      return true;
    if (type == input_type_names::kRange)
      return true;
  }
  return false;
}

// Convert a physical offset for an NG fragment to a physical legacy
// LayoutPoint, to be used in LayoutBox. There are special considerations for
// vertical-rl writing-mode, and also for block fragmentation (the block-offset
// should include consumed space in previous fragments).
inline LayoutPoint ToLayoutPoint(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const NGPhysicalBoxFragment& container_fragment,
    const NGBlockBreakToken* previous_container_break_token) {
  if (UNLIKELY(container_fragment.Style().IsFlippedBlocksWritingMode())) {
    // Move the physical offset to the right side of the child fragment,
    // relative to the right edge of the container fragment. This is the
    // block-start offset in vertical-rl, and the legacy engine expects always
    // expects the block offset to be relative to block-start.
    offset.left = container_fragment.Size().width - offset.left -
                  child_fragment.Size().width;
  }

  if (UNLIKELY(previous_container_break_token)) {
    // Add the amount of block-size previously (in previous fragmentainers)
    // consumed by the container fragment. This will map the child's offset
    // nicely into the flow thread coordinate system used by the legacy engine.
    LayoutUnit consumed = previous_container_break_token->ConsumedBlockSize();
    if (container_fragment.Style().IsHorizontalWritingMode())
      offset.top += consumed;
    else
      offset.left += consumed;
  }

  return offset.ToLayoutPoint();
}

}  // namespace

scoped_refptr<const NGLayoutResult> NGBlockNode::Layout(
    const NGConstraintSpace& constraint_space,
    const NGBlockBreakToken* break_token,
    const NGEarlyBreak* early_break) const {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout())
    return RunLegacyLayout(constraint_space);

  // The exclusion space internally is a pointer to a shared vector, and
  // equality of exclusion spaces is performed using pointer comparison on this
  // internal shared vector.
  // In order for the caching logic to work correctly we need to set the
  // pointer to the value previous shared vector.
  if (const NGLayoutResult* previous_result = box_->GetCachedLayoutResult()) {
    constraint_space.ExclusionSpace().PreInitialize(
        previous_result->GetConstraintSpaceForCaching().ExclusionSpace());
  }

  NGLayoutCacheStatus cache_status;
  base::Optional<NGFragmentGeometry> fragment_geometry;
  scoped_refptr<const NGLayoutResult> layout_result =
      box_->CachedLayoutResult(constraint_space, break_token, early_break,
                               &fragment_geometry, &cache_status);
  if (cache_status == NGLayoutCacheStatus::kHit) {
    DCHECK(layout_result);

    // We may have to update the margins on box_; we reuse the layout result
    // even if a percentage margin may have changed.
    if (UNLIKELY(Style().MayHaveMargin() && !constraint_space.IsTableCell()))
      box_->SetMargin(ComputePhysicalMargins(constraint_space, Style()));

    UpdateShapeOutsideInfoIfNeeded(
        *layout_result, constraint_space.PercentageResolutionInlineSize());

    // Even if we can reuse the result, we may still need to recalculate our
    // overflow. TODO(crbug.com/919415): Explain why.
    if (box_->NeedsLayoutOverflowRecalc()) {
      if (RuntimeEnabledFeatures::LayoutNGLayoutOverflowEnabled())
        box_->SetLayoutOverflowFromLayoutResults();
      else
        box_->RecalcLayoutOverflow();
    }

    // Return the cached result unless we're marked for layout. We may have
    // added or removed scrollbars during overflow recalculation, which may have
    // marked us for layout. In that case the cached result is unusable, and we
    // need to re-lay out now.
    if (!box_->NeedsLayout())
      return layout_result;
  }

  if (!fragment_geometry) {
    fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this);
  }

  TextAutosizer::NGLayoutScope text_autosizer_layout_scope(
      box_, fragment_geometry->border_box_size.inline_size);

  PrepareForLayout();

  NGLayoutAlgorithmParams params(*this, *fragment_geometry, constraint_space,
                                 break_token, early_break);

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_);

  // Try to perform "simplified" layout.
  if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout &&
      !GetFlowThread(block_flow)) {
    DCHECK(layout_result);
#if DCHECK_IS_ON()
    scoped_refptr<const NGLayoutResult> previous_result = layout_result;
#endif

    // A child may have changed size while performing "simplified" layout (it
    // may have gained or removed scrollbars, changing its size). In these
    // cases "simplified" layout will return a null layout-result, indicating
    // we need to perform a full layout.
    layout_result = RunSimplifiedLayout(params, *layout_result);

#if DCHECK_IS_ON()
    if (layout_result) {
      layout_result->CheckSameForSimplifiedLayout(
          *previous_result, /* check_same_block_size */ !block_flow);
    }
#endif
  } else if (cache_status == NGLayoutCacheStatus::kCanReuseLines) {
    params.previous_result = layout_result.get();
    layout_result = nullptr;
  } else {
    layout_result = nullptr;
  }

  // Fragment geometry scrollbars are potentially size constrained, and cannot
  // be used for comparison with their after layout size.
  NGBoxStrut before_layout_scrollbars =
      ComputeScrollbars(constraint_space, *this);
  bool before_layout_intrinsic_logical_widths_dirty =
      box_->IntrinsicLogicalWidthsDirty();

  if (!layout_result)
    layout_result = LayoutWithAlgorithm(params);

  FinishLayout(block_flow, constraint_space, break_token, layout_result);

  // We may need to relayout if:
  // - Our scrollbars have changed causing our size to change (shrink-to-fit)
  //   or the available space to our children changing.
  // - A child changed scrollbars causing our size to change (shrink-to-fit).
  //
  // This mirrors legacy code in PaintLayerScrollableArea::UpdateAfterLayout.
  if ((before_layout_scrollbars !=
       ComputeScrollbars(constraint_space, *this)) ||
      (!before_layout_intrinsic_logical_widths_dirty &&
       box_->IntrinsicLogicalWidthsDirty())) {
    PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars;

    // We need to clear any previous results when scrollbars change. For
    // example - we may have stored a "measure" layout result which will be
    // incorrect if we try and reuse it.
    params.previous_result = nullptr;
    box_->ClearLayoutResults();

#if DCHECK_IS_ON()
    // Ensure turning on/off scrollbars only once at most, when we call
    // |LayoutWithAlgorithm| recursively.
    DEFINE_STATIC_LOCAL(HashSet<LayoutBox*>, scrollbar_changed, ());
    DCHECK(scrollbar_changed.insert(box_).is_new_entry);
#endif

    // Scrollbar changes are hard to detect. Make sure everyone gets the
    // message.
    box_->SetNeedsLayout(layout_invalidation_reason::kScrollbarChanged,
                         kMarkOnlyThis);

    fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this);
    layout_result = LayoutWithAlgorithm(params);
    FinishLayout(block_flow, constraint_space, break_token, layout_result);

#if DCHECK_IS_ON()
    scrollbar_changed.erase(box_);
#endif
  }

  // We always need to update the ShapeOutsideInfo even if the layout is
  // intermediate (e.g. called during a min/max pass).
  //
  // If a shape-outside float is present in an orthogonal flow, when
  // calculating the min/max-size (by performing an intermediate layout), we
  // might calculate this incorrectly, as the layout won't take into account the
  // shape-outside area.
  //
  // TODO(ikilpatrick): This should be fixed by moving the shape-outside data
  // to the NGLayoutResult, removing this "side" data-structure.
  UpdateShapeOutsideInfoIfNeeded(
      *layout_result, constraint_space.PercentageResolutionInlineSize());

#if DCHECK_IS_ON()
  // DCHECK can only be turned on for LayoutNGTable because it
  // fails with Legacy table implementation.
  if (RuntimeEnabledFeatures::LayoutNGTableEnabled() &&
      layout_result->Status() == NGLayoutResult::kSuccess) {
    LogicalSize size =
        layout_result->PhysicalFragment().Size().ConvertToLogical(
            constraint_space.GetWritingMode());
    if (constraint_space.IsFixedInlineSize())
      DCHECK_EQ(size.inline_size, constraint_space.AvailableSize().inline_size);
    if (constraint_space.IsFixedBlockSize())
      DCHECK_EQ(size.block_size, constraint_space.AvailableSize().block_size);
  }
#endif
  return layout_result;
}

scoped_refptr<const NGLayoutResult> NGBlockNode::SimplifiedLayout(
    const NGPhysicalFragment& previous_fragment) const {
  scoped_refptr<const NGLayoutResult> previous_result =
      box_->GetCachedLayoutResult();
  DCHECK(previous_result);

  // We might be be trying to perform simplfied layout on a fragment in the
  // "measure" cache slot, abort if this is the case.
  if (&previous_result->PhysicalFragment() != &previous_fragment)
    return nullptr;

  if (!box_->NeedsLayout())
    return previous_result;

  DCHECK(box_->NeedsSimplifiedLayoutOnly() ||
         box_->ChildLayoutBlockedByDisplayLock());

  // Perform layout on ourselves using the previous constraint space.
  const NGConstraintSpace space(
      previous_result->GetConstraintSpaceForCaching());
  scoped_refptr<const NGLayoutResult> result =
      Layout(space, /* break_token */ nullptr);

  // If we changed size from performing "simplified" layout, we have
  // added/removed scrollbars. Return null indicating to our parent that it
  // needs to perform a full layout.
  if (previous_result->PhysicalFragment().Size() !=
      result->PhysicalFragment().Size())
    return nullptr;

#if DCHECK_IS_ON()
  result->CheckSameForSimplifiedLayout(*previous_result);
#endif

  return result;
}

scoped_refptr<const NGLayoutResult>
NGBlockNode::CachedLayoutResultForOutOfFlowPositioned(
    LogicalSize container_content_size) const {
  DCHECK(IsOutOfFlowPositioned());

  if (box_->NeedsLayout())
    return nullptr;

  const NGLayoutResult* cached_layout_result = box_->GetCachedLayoutResult();
  if (!cached_layout_result)
    return nullptr;

  // The containing-block may have borders/scrollbars which might change
  // between passes affecting the final position.
  if (!cached_layout_result->CanUseOutOfFlowPositionedFirstTierCache())
    return nullptr;

  // TODO(layout-dev): There are potentially more cases where we can reuse this
  // layout result.
  // E.g. when we have a fixed-length top position constraint (top: 5px), we
  // are in the correct writing mode (htb-ltr), and we have a fixed width.
  const NGConstraintSpace& space =
      cached_layout_result->GetConstraintSpaceForCaching();
  if (space.PercentageResolutionSize() != container_content_size)
    return nullptr;

  // We currently don't keep the static-position around to determine if it is
  // the same as the previous layout pass. As such, only reuse the result when
  // we know it doesn't depend on the static-position.
  //
  // TODO(layout-dev): We might be able to determine what the previous
  // static-position was based on |NGLayoutResult::OutOfFlowPositionedOffset|.
  bool depends_on_static_position =
      (Style().Left().IsAuto() && Style().Right().IsAuto()) ||
      (Style().Top().IsAuto() && Style().Bottom().IsAuto());

  if (depends_on_static_position)
    return nullptr;

  return cached_layout_result;
}

void NGBlockNode::PrepareForLayout() const {
  auto* block = DynamicTo<LayoutBlock>(box_);
  if (block && block->IsScrollContainer()) {
    DCHECK(block->GetScrollableArea());
    if (block->GetScrollableArea()->ShouldPerformScrollAnchoring())
      block->GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();
  }

  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    ToLayoutNGListItem(box_)->UpdateMarkerTextIfNeeded();
}

void NGBlockNode::FinishLayout(
    LayoutBlockFlow* block_flow,
    const NGConstraintSpace& constraint_space,
    const NGBlockBreakToken* break_token,
    scoped_refptr<const NGLayoutResult> layout_result) const {
  // If we abort layout and don't clear the cached layout-result, we can end
  // up in a state where the layout-object tree doesn't match fragment tree
  // referenced by this layout-result.
  if (layout_result->Status() != NGLayoutResult::kSuccess) {
    box_->ClearLayoutResults();
    return;
  }

  // Add all layout results (and fragments) generated from a node to a list in
  // the layout object. Some extra care is required to correctly overwrite
  // intermediate layout results: The sequence number of an incoming break token
  // corresponds with the fragment index in the layout object (off by 1,
  // though). When writing back a layout result, we remove any fragments in the
  // layout box at higher indices than that of the one we're writing back.
  const auto& physical_fragment =
      To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

  if (layout_result->IsSingleUse())
    box_->AddLayoutResult(layout_result, FragmentIndex(break_token));
  else
    box_->SetCachedLayoutResult(layout_result);

  if (block_flow) {
    const NGFragmentItems* items = physical_fragment.Items();
    bool has_inline_children = items || HasInlineChildren(block_flow);

    // Don't consider display-locked objects as having any children.
    if (has_inline_children && box_->ChildLayoutBlockedByDisplayLock()) {
      has_inline_children = false;
      // It could be the case that our children are already clean at the time
      // the lock was acquired. This means that |box_| self dirty bits might be
      // set, and child dirty bits might not be. We clear the self bits since we
      // want to treat the |box_| as layout clean, even when locked. However,
      // here we also skip appending paint fragments for inline children. This
      // means that we potentially can end up in a situation where |box_| is
      // completely layout clean, but its inline children didn't append the
      // paint fragments to it, which causes problems. In order to solve this,
      // we set a child dirty bit on |box_| ensuring that when the lock
      // is removed, or update is forced, we will visit this box again and
      // properly create the paint fragments. See https://crbug.com/962614.
      box_->SetChildNeedsLayout(kMarkOnlyThis);
    }

    if (has_inline_children) {
      if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            physical_fragment, physical_fragment.Size().width,
            Style().IsFlippedBlocksWritingMode());
        block_flow->SetPaintFragment(break_token, &physical_fragment);
      } else if (items) {
        CopyFragmentItemsToLayoutBox(physical_fragment, *items, break_token);
      }
    } else {
      // We still need to clear paint fragments in case it had inline children,
      // and thus had NGPaintFragment.
      block_flow->ClearNGInlineNodeData();
      if (!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled())
        block_flow->SetPaintFragment(break_token, nullptr);
    }
  } else {
    DCHECK(!physical_fragment.HasItems());
  }

  CopyFragmentDataToLayoutBox(constraint_space, *layout_result, break_token);
}

MinMaxSizesResult NGBlockNode::ComputeMinMaxSizes(
    WritingMode container_writing_mode,
    const MinMaxSizesInput& input,
    const NGConstraintSpace* constraint_space) const {
  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    ToLayoutNGListItem(box_)->UpdateMarkerTextIfNeeded();

  bool is_orthogonal_flow_root =
      !IsParallelWritingMode(container_writing_mode, Style().GetWritingMode());

  // If we're orthogonal, run layout to compute the sizes.
  if (is_orthogonal_flow_root) {
    // If we have an aspect ratio, we may be able to avoid laying out the
    // child as an optimization, if performance testing shows this to be
    // important.

    MinMaxSizes sizes;
    // Some other areas of the code can query the intrinsic-sizes while outside
    // of the layout phase.
    // TODO(ikilpatrick): Remove this check.
    if (!box_->GetFrameView()->IsInPerformLayout()) {
      sizes = ComputeMinMaxSizesFromLegacy(input);
      return {sizes, /* depends_on_percentage_block_size */ false};
    }

    DCHECK(constraint_space);
    scoped_refptr<const NGLayoutResult> layout_result =
        Layout(*constraint_space);
    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
    sizes = NGFragment({container_writing_mode, TextDirection::kLtr},
                       layout_result->PhysicalFragment())
                .InlineSize();
    return {sizes, /* depends_on_percentage_block_size */ false};
  }

  // Synthesize a zero space if not provided.
  auto zero_constraint_space = CreateConstraintSpaceForMinMax(*this, input);
  if (!constraint_space)
    constraint_space = &zero_constraint_space;

  if (!Style().AspectRatio().IsAuto() && !IsReplaced() &&
      input.type == MinMaxSizesType::kContent) {
    LayoutUnit block_size(kIndefiniteSize);
    if (IsOutOfFlowPositioned()) {
      // For out-of-flow, the input percentage block size is actually our
      // block size. We should use that for aspect-ratio purposes if known.
      block_size = input.percentage_resolution_block_size;
    }

    NGFragmentGeometry fragment_geometry =
        CalculateInitialMinMaxFragmentGeometry(*constraint_space, *this);
    NGBoxStrut border_padding =
        fragment_geometry.border + fragment_geometry.padding;
    LayoutUnit size_from_ar = ComputeInlineSizeFromAspectRatio(
        *constraint_space, Style(), border_padding, block_size);
    if (size_from_ar != kIndefiniteSize) {
      return {{size_from_ar, size_from_ar},
              Style().LogicalHeight().IsPercentOrCalc()};
    }
  }

  bool can_use_cached_intrinsic_inline_sizes =
      CanUseCachedIntrinsicInlineSizes(input, *constraint_space, *this);

  // Use our cached sizes if either:
  //  - The %-block-sizes match.
  //  - We don't have a descendant which depends on the %-block-size.
  if (can_use_cached_intrinsic_inline_sizes &&
      (input.percentage_resolution_block_size ==
           box_->IntrinsicLogicalWidthsPercentageResolutionBlockSize() ||
       !box_->IntrinsicLogicalWidthsChildDependsOnPercentageBlockSize())) {
    MinMaxSizes sizes = box_->IsTable() && !box_->IsLayoutNGMixin()
                            ? box_->PreferredLogicalWidths()
                            : box_->IntrinsicLogicalWidths(input.type);
    bool depends_on_percentage_block_size =
        box_->IntrinsicLogicalWidthsDependsOnPercentageBlockSize();
    return {sizes, depends_on_percentage_block_size};
  }

  NGFragmentGeometry fragment_geometry =
      CalculateInitialMinMaxFragmentGeometry(*constraint_space, *this);

  // Calculate the %-block-size for our children up front. This allows us to
  // determine if |input|'s %-block-size is used.
  const NGBoxStrut border_padding =
      fragment_geometry.border + fragment_geometry.padding;
  bool uses_input_percentage_block_size = false;
  LayoutUnit child_percentage_resolution_block_size =
      CalculateChildPercentageBlockSizeForMinMax(
          *constraint_space, *this, border_padding,
          input.percentage_resolution_block_size,
          &uses_input_percentage_block_size);

  bool cache_depends_on_percentage_block_size =
      uses_input_percentage_block_size &&
      box_->IntrinsicLogicalWidthsChildDependsOnPercentageBlockSize();

  // We might still be able to use the cached values if our children don't
  // depend on the *input* %-block-size.
  if (can_use_cached_intrinsic_inline_sizes &&
      !cache_depends_on_percentage_block_size) {
    MinMaxSizes sizes = box_->IsTable() && !box_->IsLayoutNGMixin()
                            ? box_->PreferredLogicalWidths()
                            : box_->IntrinsicLogicalWidths(input.type);
    return {sizes, cache_depends_on_percentage_block_size};
  }

  box_->SetIntrinsicLogicalWidthsDirty();

  if (!CanUseNewLayout()) {
    MinMaxSizes sizes = ComputeMinMaxSizesFromLegacy(input);

    // Update the cache bits for this legacy root (but not the intrinsic
    // inline-sizes themselves).
    box_->SetIntrinsicLogicalWidthsFromNG(
        input.percentage_resolution_block_size,
        /* depends_on_percentage_block_size */ uses_input_percentage_block_size,
        /* child_depends_on_percentage_block_size */ true,
        /* sizes */ nullptr);

    return {sizes, uses_input_percentage_block_size};
  }

  // Copy the input, and set the new %-block-size.
  MinMaxSizesInput adjusted_input = input;
  adjusted_input.percentage_resolution_block_size =
      child_percentage_resolution_block_size;

  MinMaxSizesResult result = ComputeMinMaxSizesWithAlgorithm(
      NGLayoutAlgorithmParams(*this, fragment_geometry, *constraint_space),
      adjusted_input);

  if (UNLIKELY(IsContentMinimumInlineSizeZero(*this)))
    result.sizes.min_size = border_padding.InlineSum();

  bool depends_on_percentage_block_size =
      uses_input_percentage_block_size &&
      result.depends_on_percentage_block_size;

  if (!Style().AspectRatio().IsAuto() &&
      BlockLengthUnresolvable(*constraint_space, Style().LogicalHeight(),
                              LengthResolvePhase::kLayout)) {
    // If the block size will be computed from the aspect ratio, we need
    // to take the max-block-size into account.
    // https://drafts.csswg.org/css-sizing-4/#aspect-ratio
    MinMaxSizes min_max = ComputeMinMaxInlineSizesFromAspectRatio(
        *constraint_space, Style(), border_padding,
        LengthResolvePhase::kIntrinsic);
    result.sizes.min_size = min_max.ClampSizeToMinAndMax(result.sizes.min_size);
    result.sizes.max_size = min_max.ClampSizeToMinAndMax(result.sizes.max_size);
    depends_on_percentage_block_size =
        depends_on_percentage_block_size ||
        Style().LogicalMinHeight().IsPercentOrCalc() ||
        Style().LogicalMaxHeight().IsPercentOrCalc();
  }

  box_->SetIntrinsicLogicalWidthsFromNG(
      input.percentage_resolution_block_size, depends_on_percentage_block_size,
      /* child_depends_on_percentage_block_size */
      result.depends_on_percentage_block_size, &result.sizes);

  if (IsNGTableCell()) {
    To<LayoutNGTableCell>(box_)->SetIntrinsicLogicalWidthsBorderSizes(
        constraint_space->TableCellBorders());
  }

  // We report to our parent if we depend on the %-block-size if we used the
  // input %-block-size, or one of children said it depended on this.
  result.depends_on_percentage_block_size = depends_on_percentage_block_size;
  return result;
}

MinMaxSizes NGBlockNode::ComputeMinMaxSizesFromLegacy(
    const MinMaxSizesInput& input) const {
  bool needs_size_reset = false;
  if (!box_->HasOverrideContainingBlockContentLogicalHeight()) {
    box_->SetOverrideContainingBlockContentLogicalHeight(
        input.percentage_resolution_block_size);
    needs_size_reset = true;
  }

  // Tables don't calculate their min/max content contribution the same way as
  // other layout nodes. This is because width/min-width/etc have a different
  // meaning for tables.
  //
  // Due to this the min/max content contribution is their min/max content size.
  MinMaxSizes sizes = box_->IsTable()
                          ? box_->PreferredLogicalWidths()
                          : box_->IntrinsicLogicalWidths(input.type);

  if (needs_size_reset)
    box_->ClearOverrideContainingBlockContentSize();

  return sizes;
}

NGLayoutInputNode NGBlockNode::NextSibling() const {
  LayoutObject* next_sibling = GetLayoutObjectForNextSiblingNode(box_);

  // We may have some LayoutInline(s) still within the tree (due to treating
  // inline-level floats and/or OOF-positioned nodes as block-level), we need
  // to skip them and clear layout.
  while (next_sibling && next_sibling->IsInline()) {
    // TODO(layout-dev): Clearing needs-layout within this accessor is an
    // unexpected side-effect. There may be additional invalidations that need
    // to be performed.
    DCHECK(next_sibling->IsText());
    next_sibling->ClearNeedsLayout();
    next_sibling = next_sibling->NextSibling();
  }

  if (!next_sibling)
    return nullptr;

  return NGBlockNode(ToLayoutBox(next_sibling));
}

NGLayoutInputNode NGBlockNode::FirstChild() const {
  auto* block = DynamicTo<LayoutBlock>(box_);
  if (UNLIKELY(!block))
    return NGBlockNode(box_->FirstChildBox());
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (!AreNGBlockFlowChildrenInline(block))
    return NGBlockNode(ToLayoutBox(child));

  NGInlineNode inline_node(To<LayoutBlockFlow>(block));
  if (!inline_node.IsBlockLevel())
    return inline_node;

  // At this point we have a node which is empty or only has floats and
  // OOF-positioned nodes. We treat all children as block-level, even though
  // they are within a inline-level LayoutBlockFlow.

  // We may have some LayoutInline(s) still within the tree (due to treating
  // inline-level floats and/or OOF-positioned nodes as block-level), we need
  // to skip them and clear layout.
  while (child && child->IsInline()) {
    // TODO(layout-dev): Clearing needs-layout within this accessor is an
    // unexpected side-effect. There may be additional invalidations that need
    // to be performed.
    DCHECK(child->IsText());
    child->ClearNeedsLayout();
    child = child->NextSibling();
  }

  if (!child)
    return nullptr;

  DCHECK(child->IsFloatingOrOutOfFlowPositioned());
  return NGBlockNode(ToLayoutBox(child));
}

NGBlockNode NGBlockNode::GetRenderedLegend() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return NGBlockNode(LayoutFieldset::FindInFlowLegend(*To<LayoutBlock>(box_)));
}

NGBlockNode NGBlockNode::GetFieldsetContent() const {
  if (!IsFieldsetContainer())
    return nullptr;
  auto* child = GetLayoutObjectForFirstChildNode(To<LayoutBlock>(box_));
  if (!child)
    return nullptr;
  return NGBlockNode(ToLayoutBox(child));
}

const NGBoxStrut& NGBlockNode::GetTableBordersStrut() const {
  return GetTableBorders()->TableBorder();
}

scoped_refptr<const NGTableBorders> NGBlockNode::GetTableBorders() const {
  DCHECK(IsTable());
  DCHECK(box_->IsLayoutNGMixin());
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_);
  scoped_refptr<const NGTableBorders> table_borders =
      layout_table->GetCachedTableBorders();
  if (!table_borders) {
    table_borders = NGTableBorders::ComputeTableBorders(*this);
    layout_table->SetCachedTableBorders(table_borders.get());
  }
  return table_borders;
}

scoped_refptr<const NGTableTypes::Columns> NGBlockNode::GetColumnConstraints(
    const NGTableGroupedChildren& grouped_children,
    const NGBoxStrut& border_padding) const {
  DCHECK(IsTable());
  LayoutNGTable* layout_table = To<LayoutNGTable>(box_);
  scoped_refptr<const NGTableTypes::Columns> column_constraints =
      layout_table->GetCachedTableColumnConstraints();
  if (!column_constraints) {
    column_constraints = NGTableAlgorithmUtils::ComputeColumnConstraints(
        *this, grouped_children, *GetTableBorders().get(), border_padding);
    layout_table->SetCachedTableColumnConstraints(column_constraints.get());
  }
  return column_constraints;
}

LayoutUnit NGBlockNode::ComputeTableInlineSize(
    const NGConstraintSpace& space,
    const NGBoxStrut& border_padding) const {
  DCHECK(IsNGTable());
  return NGTableLayoutAlgorithm::ComputeTableInlineSize(*this, space,
                                                        border_padding);
}

bool NGBlockNode::CanUseNewLayout(const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
  if (box.ForceLegacyLayout())
    return false;
  return box.IsLayoutNGMixin();
}

bool NGBlockNode::CanUseNewLayout() const {
  return CanUseNewLayout(*box_);
}

String NGBlockNode::ToString() const {
  return String::Format("NGBlockNode: '%s'",
                        GetLayoutBox()->DebugName().Ascii().c_str());
}

void NGBlockNode::CopyFragmentDataToLayoutBox(
    const NGConstraintSpace& constraint_space,
    const NGLayoutResult& layout_result,
    const NGBlockBreakToken* previous_break_token) const {
  const auto& physical_fragment =
      To<NGPhysicalBoxFragment>(layout_result.PhysicalFragment());

  NGBoxFragment fragment(constraint_space.GetWritingDirection(),
                         physical_fragment);
  LogicalSize fragment_logical_size = fragment.Size();
  NGBoxStrut borders = fragment.Borders();
  NGBoxStrut scrollbars = ComputeScrollbars(constraint_space, *this);
  NGBoxStrut padding = fragment.Padding();
  NGBoxStrut border_scrollbar_padding = borders + scrollbars + padding;
  bool is_last_fragment = !physical_fragment.BreakToken();

  // For each fragment we process, we'll accumulate the logical height. We reset
  // it at the first fragment, and accumulate at each method call for fragments
  // belonging to the same layout object. Logical width will only be set at the
  // first fragment and is expected to remain the same throughout all subsequent
  // fragments, since legacy layout doesn't support non-uniform fragmentainer
  // widths.
  if (LIKELY(physical_fragment.IsFirstForNode())) {
    box_->SetSize(LayoutSize(physical_fragment.Size().width,
                             physical_fragment.Size().height));
    // If this is a fragment from a node that didn't break into multiple
    // fragments, write back the intrinsic size. We skip this if the node has
    // fragmented, since intrinsic block-size is rather meaningless in that
    // case, because the block-size may have been affected by something on the
    // outside (i.e. the fragmentainer).
    //
    // If we had a fixed block size, our children will have sized themselves
    // relative to the fixed size, which would make our intrinsic size incorrect
    // (too big). So skip the write-back in that case, too.
    if (LIKELY(is_last_fragment && !constraint_space.IsFixedBlockSize())) {
      box_->SetIntrinsicContentLogicalHeight(
          layout_result.IntrinsicBlockSize() -
          border_scrollbar_padding.BlockSum());
    }
  } else {
    DCHECK_EQ(box_->LogicalWidth(), fragment_logical_size.inline_size)
        << "Variable fragment inline size not supported";
    // Update logical height, unless this fragment is past the block-end of the
    // generating node (happens with overflow).
    if (previous_break_token && !previous_break_token->IsAtBlockEnd()) {
      box_->SetLogicalHeight(fragment_logical_size.block_size +
                             previous_break_token->ConsumedBlockSize());
    } else {
      DCHECK_EQ(fragment_logical_size.block_size, LayoutUnit());
    }
  }

  // TODO(mstensho): This should always be done by the parent algorithm, since
  // we may have auto margins, which only the parent is able to resolve. Remove
  // the following line when all layout modes do this properly.
  if (UNLIKELY(box_->IsTableCell())) {
    // Table-cell margins compute to zero.
    box_->SetMargin(NGPhysicalBoxStrut());
  } else {
    box_->SetMargin(ComputePhysicalMargins(constraint_space, Style()));
  }

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_);
  LayoutMultiColumnFlowThread* flow_thread = GetFlowThread(block_flow);

  // Position the children inside the box. We skip this if display-lock prevents
  // child layout.
  if (!ChildLayoutBlockedByDisplayLock()) {
    if (UNLIKELY(flow_thread)) {
      PlaceChildrenInFlowThread(flow_thread, constraint_space,
                                physical_fragment, previous_break_token);
    } else {
      PlaceChildrenInLayoutBox(physical_fragment, previous_break_token);
    }
  }

  if (UNLIKELY(!is_last_fragment))
    return;

  LayoutBlock* block = DynamicTo<LayoutBlock>(box_);
  bool needs_full_invalidation = false;
  if (LIKELY(block)) {
#if DCHECK_IS_ON()
    block->CheckPositionedObjectsNeedLayout();
#endif

    if (UNLIKELY(flow_thread && Style().HasColumnRule())) {
      // Issue full invalidation, in case the number of column rules have
      // changed.
      needs_full_invalidation = true;
    }

    block->SetNeedsOverflowRecalc(
        LayoutObject::OverflowRecalcType::kOnlyVisualOverflowRecalc);

    if (RuntimeEnabledFeatures::LayoutNGLayoutOverflowEnabled()) {
      block->SetLayoutOverflowFromLayoutResults();
    } else {
      BoxLayoutExtraInput input(*block);
      SetupBoxLayoutExtraInput(constraint_space, *block, &input);

      LayoutUnit overflow_block_size = layout_result.OverflowBlockSize();
      if (UNLIKELY(previous_break_token))
        overflow_block_size += previous_break_token->ConsumedBlockSize();

      block->ComputeLayoutOverflow(overflow_block_size - borders.block_end -
                                   scrollbars.block_end);
    }
  }

  box_->UpdateAfterLayout();

  if (needs_full_invalidation)
    box_->ClearNeedsLayoutWithFullPaintInvalidation();
  else
    box_->ClearNeedsLayout();

  // Overflow computation depends on this being set.
  if (LIKELY(block_flow))
    block_flow->SetIsSelfCollapsingFromNG(layout_result.IsSelfCollapsing());

  // We should notify the display lock that we've done layout on self, and if
  // it's not blocked, on children.
  if (auto* context = box_->GetDisplayLockContext()) {
    if (!ChildLayoutBlockedByDisplayLock())
      context->DidLayoutChildren();
  }
}

void NGBlockNode::PlaceChildrenInLayoutBox(
    const NGPhysicalBoxFragment& physical_fragment,
    const NGBlockBreakToken* previous_break_token) const {
  LayoutBox* rendered_legend = nullptr;
  for (const auto& child_fragment : physical_fragment.Children()) {
    // Skip any line-boxes we have as children, this is handled within
    // NGInlineNode at the moment.
    if (!child_fragment->IsBox())
      continue;

    const auto& box_fragment = *To<NGPhysicalBoxFragment>(child_fragment.get());
    if (box_fragment.IsFirstForNode()) {
      if (box_fragment.IsRenderedLegend())
        rendered_legend = ToLayoutBox(box_fragment.GetMutableLayoutObject());
      CopyChildFragmentPosition(box_fragment, child_fragment.offset,
                                physical_fragment, previous_break_token);
    }
  }
}

void NGBlockNode::PlaceChildrenInFlowThread(
    LayoutMultiColumnFlowThread* flow_thread,
    const NGConstraintSpace& space,
    const NGPhysicalBoxFragment& physical_fragment,
    const NGBlockBreakToken* previous_container_break_token) const {
  // Stitch the contents of the columns together in the legacy flow thread, and
  // update the position and size of column sets, spanners and spanner
  // placeholders. Create fragmentainer groups as needed. When in a nested
  // fragmentation context, we need one fragmentainer group for each outer
  // fragmentainer in which the column contents occur. All this ensures that the
  // legacy layout tree is sufficiently set up, so that DOM position/size
  // querying APIs (such as offsetTop and offsetLeft) work correctly. We still
  // rely on the legacy engine for this.
  //
  // This rather complex piece of machinery is described to some extent in the
  // design document for legacy multicol:
  // https://www.chromium.org/developers/design-documents/multi-column-layout

  NGBoxStrut border_scrollbar_padding = ComputeBorders(space, *this) +
                                        ComputeScrollbars(space, *this) +
                                        ComputePadding(space, Style());
  WritingModeConverter converter(space.GetWritingDirection(),
                                 physical_fragment.Size());
  LayoutUnit column_row_inline_size =
      converter.ToLogical(physical_fragment.Size()).inline_size -
      border_scrollbar_padding.InlineSum();

  const NGBlockBreakToken* previous_column_break_token = nullptr;
  LayoutMultiColumnSet* pending_column_set = nullptr;
  LayoutUnit flow_thread_offset;
  bool has_processed_first_column_in_flow_thread = false;
  bool should_append_fragmentainer_group = false;

  if (IsResumingLayout(previous_container_break_token)) {
    // This multicol container is nested inside another fragmentation context,
    // and this isn't its first fragment. Locate the break token for the
    // previous inner column contents, so that we include the correct amount of
    // consumed block-size in the child offsets. If there's a break token for
    // column contents, we'll find it at the back.
    const auto& child_break_tokens =
        previous_container_break_token->ChildBreakTokens();
    if (!child_break_tokens.empty()) {
      const auto* token = To<NGBlockBreakToken>(child_break_tokens.back());
      // We also create break tokens for spanners, so we need to check.
      if (token->InputNode() == *this) {
        previous_column_break_token = token;
        flow_thread_offset = previous_column_break_token->ConsumedBlockSize();

        // We're usually resuming layout into a column set that has already been
        // started in an earlier fragment, but in some cases the column set
        // starts exactly at the outer fragmentainer boundary (right after a
        // spanner that took up all remaining space in the earlier fragment),
        // and when this happens, we need to initialize the set now.
        pending_column_set = flow_thread->PendingColumnSetForNG();

        // If we resume with column content (without being interrupted by a
        // spanner) in this multicol fragment, we need to add another
        // fragmentainer group to the column set that we're resuming.
        should_append_fragmentainer_group = true;
      }
    }
  } else {
    // This is the first fragment generated for the multicol container (there
    // may be multiple fragments if we're nested inside another fragmentation
    // context).
    int column_count =
        ResolveUsedColumnCount(space.AvailableSize().inline_size, Style());
    flow_thread->StartLayoutFromNG(column_count);
    pending_column_set =
        ToLayoutMultiColumnSetOrNull(flow_thread->FirstMultiColumnBox());
  }

  for (const auto& child : physical_fragment.Children()) {
    const auto& child_fragment = To<NGPhysicalBoxFragment>(*child);
    const LayoutBox* child_box = ToLayoutBox(child_fragment.GetLayoutObject());
    if (child_box && child_box != box_) {
      DCHECK(child_box->IsColumnSpanAll());
      CopyChildFragmentPosition(child_fragment, child.offset,
                                physical_fragment);
      LayoutBox* placeholder = child_box->SpannerPlaceholder();
      if (!child_fragment.BreakToken()) {
        // Last fragment for this spanner. Update its placeholder.
        placeholder->SetLocation(child_box->Location());
        placeholder->SetSize(child_box->Size());
      }

      flow_thread->SkipColumnSpanner(child_box, flow_thread_offset);

      if (LayoutMultiColumnSet* previous_column_set =
              ToLayoutMultiColumnSetOrNull(
                  placeholder->PreviousSiblingMultiColumnBox()))
        previous_column_set->FinishLayoutFromNG();

      pending_column_set = ToLayoutMultiColumnSetOrNull(
          placeholder->NextSiblingMultiColumnBox());

      // If this multicol container was nested inside another fragmentation
      // context, and we're resuming at a subsequent fragment, we'll normally
      // append another fragmentainer group for column contents. But since we
      // found a spanner first, we won't do that, since we'll move to another
      // column set (if there's more column content at all).
      should_append_fragmentainer_group = false;
      continue;
    }

    DCHECK(!child_box);

    LogicalSize logical_size = converter.ToLogical(child_fragment.Size());

    if (has_processed_first_column_in_flow_thread) {
      // Non-uniform fragmentainer widths not supported by legacy layout.
      DCHECK_EQ(flow_thread->LogicalWidth(), logical_size.inline_size);
    } else {
      // The offset of the flow thread is the same as that of the first column.
      LayoutPoint point =
          ToLayoutPoint(child_fragment, child.offset, physical_fragment,
                        previous_container_break_token);
      flow_thread->SetLocationAndUpdateOverflowControlsIfNeeded(point);
      flow_thread->SetLogicalWidth(logical_size.inline_size);
      has_processed_first_column_in_flow_thread = true;
    }

    if (pending_column_set) {
      // We're visiting this column set for the first time in this layout pass.
      // Set up what we can set up. That's everything except for the block-size.
      // Set the inline-size to that of the content-box of the multicol
      // container. The inline-offset will be the content-box edge of the
      // multicol container, and the block-offset will be the block-offset of
      // the column itself. It doesn't matter which column from the same row we
      // use, since all columns have the same block-offset and block-size (so
      // just use the first one).
      LogicalOffset logical_offset(
          border_scrollbar_padding.inline_start,
          converter.ToLogical(child.offset, child_fragment.Size())
              .block_offset);
      PhysicalOffset physical_offset =
          converter.ToPhysical(logical_offset, child_fragment.Size());
      // We have calculated the physical offset relative to the border edge of
      // this multicol container fragment. We'll now convert it to a legacy
      // engine LayoutPoint, which will also take care of converting it into the
      // flow thread coordinate space, if we happen to be nested inside another
      // fragmentation context.
      LayoutPoint point =
          ToLayoutPoint(child_fragment, physical_offset, physical_fragment,
                        previous_container_break_token);

      pending_column_set->SetLocation(point);
      pending_column_set->SetLogicalWidth(column_row_inline_size);
      pending_column_set->ResetColumnHeight();
      pending_column_set = nullptr;
    } else if (should_append_fragmentainer_group) {
      // Resuming column layout from the previous outer fragmentainer into the
      // same column set as we used there.
      flow_thread->AppendNewFragmentainerGroupFromNG();
      should_append_fragmentainer_group = false;
    }

    flow_thread->SetCurrentColumnBlockSizeFromNG(logical_size.block_size);

    // Each anonymous child of a multicol container constitutes one column.
    // Position each child fragment in the first column that they occur,
    // relatively to the block-start of the flow thread.
    PlaceChildrenInLayoutBox(child_fragment, previous_column_break_token);

    // If the multicol container has inline children, there may still be floats
    // there, but they aren't stored as child fragments of |column| in that case
    // (but rather inside fragment items). Make sure that they get positioned,
    // too.
    if (const NGFragmentItems* items = child_fragment.Items()) {
      CopyFragmentItemsToLayoutBox(child_fragment, *items,
                                   previous_column_break_token);
    }

    flow_thread_offset += logical_size.block_size;
    previous_column_break_token =
        To<NGBlockBreakToken>(child_fragment.BreakToken());
  }

  if (!physical_fragment.BreakToken())
    flow_thread->FinishLayoutFromNG(flow_thread_offset);
}

// Copies data back to the legacy layout tree for a given child fragment.
void NGBlockNode::CopyChildFragmentPosition(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const NGPhysicalBoxFragment& container_fragment,
    const NGBlockBreakToken* previous_container_break_token) const {
  LayoutBox* layout_box = ToLayoutBox(child_fragment.GetMutableLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  LayoutPoint point = ToLayoutPoint(child_fragment, offset, container_fragment,
                                    previous_container_break_token);
  layout_box->SetLocationAndUpdateOverflowControlsIfNeeded(point);
}

// For inline children, NG painters handles fragments directly, but there are
// some cases where we need to copy data to the LayoutObject tree. This function
// handles such cases.
void NGBlockNode::CopyFragmentDataToLayoutBoxForInlineChildren(
    const NGPhysicalContainerFragment& container,
    LayoutUnit initial_container_width,
    bool initial_container_is_flipped,
    PhysicalOffset offset) const {
  DCHECK(!RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());
  for (const auto& child : container.Children()) {
    if (child->IsContainer()) {
      PhysicalOffset child_offset = offset + child.Offset();

      // Replaced elements and inline blocks need Location() set relative to
      // their block container.
      LayoutObject* layout_object = child->GetMutableLayoutObject();
      if (layout_object && layout_object->IsBox()) {
        LayoutBox& layout_box = ToLayoutBox(*layout_object);
        PhysicalOffset maybe_flipped_offset = child_offset;
        if (initial_container_is_flipped) {
          maybe_flipped_offset.left = initial_container_width -
                                      child->Size().width -
                                      maybe_flipped_offset.left;
        }
        layout_box.SetLocationAndUpdateOverflowControlsIfNeeded(
            maybe_flipped_offset.ToLayoutPoint());
      }

      // Legacy compatibility. This flag is used in paint layer for
      // invalidation.
      if (layout_object && layout_object->IsLayoutInline() &&
          layout_object->StyleRef().HasOutline() &&
          !layout_object->IsElementContinuation() &&
          ToLayoutInline(layout_object)->Continuation()) {
        box_->SetContainsInlineWithOutlineAndContinuation(true);
      }

      // The Location() of inline LayoutObject is relative to the
      // LayoutBlockFlow. If |child| establishes a new block formatting context,
      // it also creates another inline formatting context. Do not copy to its
      // descendants in this case.
      if (!child->IsFormattingContextRoot()) {
        CopyFragmentDataToLayoutBoxForInlineChildren(
            To<NGPhysicalContainerFragment>(*child), initial_container_width,
            initial_container_is_flipped, child_offset);
      }
    }
  }
}

void NGBlockNode::CopyFragmentItemsToLayoutBox(
    const NGPhysicalBoxFragment& container,
    const NGFragmentItems& items,
    const NGBlockBreakToken* previous_break_token) const {
  DCHECK(RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled());

  LayoutUnit previously_consumed_block_size;
  if (previous_break_token)
    previously_consumed_block_size = previous_break_token->ConsumedBlockSize();
  bool initial_container_is_flipped = Style().IsFlippedBlocksWritingMode();
  for (NGInlineCursor cursor(items); cursor; cursor.MoveToNext()) {
    if (const NGPhysicalBoxFragment* child = cursor.Current().BoxFragment()) {
      // Replaced elements and inline blocks need Location() set relative to
      // their block container.
      LayoutObject* layout_object = child->GetMutableLayoutObject();
      if (!layout_object)
        continue;
      if (LayoutBox* layout_box = ToLayoutBoxOrNull(layout_object)) {
        PhysicalOffset maybe_flipped_offset =
            cursor.Current().OffsetInContainerBlock();
        if (initial_container_is_flipped) {
          maybe_flipped_offset.left = container.Size().width -
                                      child->Size().width -
                                      maybe_flipped_offset.left;
        }
        if (container.Style().IsHorizontalWritingMode())
          maybe_flipped_offset.top += previously_consumed_block_size;
        else
          maybe_flipped_offset.left += previously_consumed_block_size;
        layout_box->SetLocationAndUpdateOverflowControlsIfNeeded(
            maybe_flipped_offset.ToLayoutPoint());
        if (UNLIKELY(layout_box->HasSelfPaintingLayer()))
          layout_box->Layer()->SetNeedsVisualOverflowRecalc();
        continue;
      }

      // Legacy compatibility. This flag is used in paint layer for
      // invalidation.
      if (LayoutInline* layout_inline = ToLayoutInlineOrNull(layout_object)) {
        if (layout_inline->StyleRef().HasOutline() &&
            !layout_inline->IsElementContinuation() &&
            layout_inline->Continuation()) {
          box_->SetContainsInlineWithOutlineAndContinuation(true);
        }
        if (UNLIKELY(layout_inline->HasSelfPaintingLayer()))
          layout_inline->Layer()->SetNeedsVisualOverflowRecalc();
      }
    }
  }
}

bool NGBlockNode::IsInlineFormattingContextRoot(
    NGLayoutInputNode* first_child_out) const {
  if (const auto* block = DynamicTo<LayoutBlockFlow>(box_)) {
    if (!AreNGBlockFlowChildrenInline(block))
      return false;
    NGLayoutInputNode first_child = FirstChild();
    if (first_child.IsInline()) {
      if (first_child_out)
        *first_child_out = first_child;
      return true;
    }
  }
  return false;
}

bool NGBlockNode::IsInlineLevel() const {
  return GetLayoutBox()->IsInline();
}

bool NGBlockNode::IsAtomicInlineLevel() const {
  // LayoutObject::IsAtomicInlineLevel() returns true for e.g., <img
  // style="display: block">. Check IsInline() as well.
  return GetLayoutBox()->IsAtomicInlineLevel() && GetLayoutBox()->IsInline();
}

bool NGBlockNode::HasAspectRatio() const {
  if (!Style().AspectRatio().IsAuto())
    return true;
  LayoutBox* layout_object = GetLayoutBox();
  if (!layout_object->IsImage() && !IsA<LayoutVideo>(layout_object) &&
      !layout_object->IsCanvas() && !layout_object->IsSVGRoot()) {
    return false;
  }

  // Retrieving this and throwing it away is wasteful. We could make this method
  // return Optional<LogicalSize> that returns the aspect_ratio if there is one.
  return !GetAspectRatio().IsEmpty();
}

LogicalSize NGBlockNode::GetAspectRatio() const {
  // The CSS parser will ensure that this will only be set if the feature
  // is enabled.
  const StyleAspectRatio& ratio = Style().AspectRatio();
  if (ratio.GetType() == EAspectRatioType::kRatio)
    return Style().LogicalAspectRatio();

  base::Optional<LayoutUnit> computed_inline_size;
  base::Optional<LayoutUnit> computed_block_size;
  GetOverrideIntrinsicSize(&computed_inline_size, &computed_block_size);
  if (computed_inline_size && computed_block_size)
    return LogicalSize(*computed_inline_size, *computed_block_size);

  IntrinsicSizingInfo legacy_sizing_info;
  ToLayoutReplaced(box_)->ComputeIntrinsicSizingInfo(legacy_sizing_info);
  LogicalSize intrinsic_ar{
      LayoutUnit(legacy_sizing_info.aspect_ratio.Width()),
      LayoutUnit(legacy_sizing_info.aspect_ratio.Height())};
  if (!intrinsic_ar.IsEmpty())
    return intrinsic_ar;
  if (ratio.GetType() == EAspectRatioType::kAutoAndRatio)
    return Style().LogicalAspectRatio();
  return LogicalSize();
}

base::Optional<TransformationMatrix> NGBlockNode::GetTransformForChildFragment(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalSize size) const {
  const auto* child_layout_object = child_fragment.GetLayoutObject();
  DCHECK(child_layout_object);

  if (!child_layout_object->ShouldUseTransformFromContainer(box_))
    return base::nullopt;

  TransformationMatrix transform;
  child_layout_object->GetTransformFromContainer(box_, PhysicalOffset(),
                                                 transform, &size);

  return transform;
}

bool NGBlockNode::IsCustomLayoutLoaded() const {
  DCHECK(box_->IsLayoutNGCustom());
  return To<LayoutNGCustom>(box_)->IsLoaded();
}

MathScriptType NGBlockNode::ScriptType() const {
  DCHECK(IsA<MathMLScriptsElement>(GetDOMNode()));
  return To<MathMLScriptsElement>(GetDOMNode())->GetScriptType();
}

bool NGBlockNode::HasIndex() const {
  DCHECK(IsA<MathMLRadicalElement>(GetDOMNode()));
  return To<MathMLRadicalElement>(GetDOMNode())->HasIndex();
}

scoped_refptr<const NGLayoutResult> NGBlockNode::LayoutAtomicInline(
    const NGConstraintSpace& parent_constraint_space,
    const ComputedStyle& parent_style,
    bool use_first_line_style,
    NGBaselineAlgorithmType baseline_algorithm_type) {
  NGConstraintSpaceBuilder builder(
      parent_constraint_space, Style().GetWritingMode(), /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, *this, &builder);

  builder.SetIsPaintedAtomically(true);
  builder.SetUseFirstLineStyle(use_first_line_style);

  builder.SetNeedsBaseline(true);
  builder.SetBaselineAlgorithmType(baseline_algorithm_type);

  builder.SetIsShrinkToFit(Style().LogicalWidth().IsAuto());
  builder.SetAvailableSize(parent_constraint_space.AvailableSize());
  builder.SetPercentageResolutionSize(
      parent_constraint_space.PercentageResolutionSize());
  builder.SetReplacedPercentageResolutionSize(
      parent_constraint_space.ReplacedPercentageResolutionSize());
  builder.SetTextDirection(Style().Direction());
  NGConstraintSpace constraint_space = builder.ToConstraintSpace();
  scoped_refptr<const NGLayoutResult> result = Layout(constraint_space);
  // TODO(kojii): Investigate why ClearNeedsLayout() isn't called automatically
  // when it's being laid out.
  GetLayoutBox()->ClearNeedsLayout();
  return result;
}

scoped_refptr<const NGLayoutResult> NGBlockNode::RunLegacyLayout(
    const NGConstraintSpace& constraint_space) const {
  // This is an exit-point from LayoutNG to the legacy engine. This means that
  // we need to be at a formatting context boundary, since NG and legacy don't
  // cooperate on e.g. margin collapsing.
  DCHECK(!box_->IsLayoutBlock() ||
         To<LayoutBlock>(box_)->CreatesNewFormattingContext());

  // We cannot enter legacy layout for something fragmentable if we're inside an
  // NG block fragmentation context. LayoutNG and legacy block fragmentation
  // cannot cooperate within the same fragmentation context.
  DCHECK(!constraint_space.HasBlockFragmentation() ||
         box_->GetNGPaginationBreakability() == LayoutBox::kForbidBreaks);

  scoped_refptr<const NGLayoutResult> layout_result =
      box_->GetCachedLayoutResult();

  // We need to force a layout on the child if the constraint space given will
  // change the layout.
  bool needs_force_relayout =
      layout_result &&
      !MaySkipLegacyLayout(*this, *layout_result, constraint_space);

  if (box_->NeedsLayout() || !layout_result || needs_force_relayout) {
    BoxLayoutExtraInput input(*box_);
    WritingMode writing_mode = Style().GetWritingMode();

    SetupBoxLayoutExtraInput(constraint_space, *box_, &input);
    box_->ComputeAndSetBlockDirectionMargins(box_->ContainingBlock());

    // Using |LayoutObject::LayoutIfNeeded| save us a little bit of overhead,
    // compared to |LayoutObject::ForceLayout|.
    DCHECK(!box_->IsLayoutNGMixin());
    bool needed_layout = box_->NeedsLayout();
    if (box_->NeedsLayout() && !needs_force_relayout)
      box_->LayoutIfNeeded();
    else
      box_->ForceLayout();

    // Synthesize a new layout result.
    NGFragmentGeometry fragment_geometry;
    fragment_geometry.border_box_size = {box_->LogicalWidth(),
                                         box_->LogicalHeight()};
    fragment_geometry.border = {box_->BorderStart(), box_->BorderEnd(),
                                box_->BorderBefore(), box_->BorderAfter()};
    fragment_geometry.scrollbar = ComputeScrollbars(constraint_space, *this);
    fragment_geometry.padding = {box_->PaddingStart(), box_->PaddingEnd(),
                                 box_->PaddingBefore(), box_->PaddingAfter()};

    // TODO(kojii): Implement use_first_line_style.
    NGBoxFragmentBuilder builder(*this, box_->Style(), &constraint_space,
                                 {writing_mode, box_->StyleRef().Direction()});
    builder.SetIsNewFormattingContext(
        constraint_space.IsNewFormattingContext());
    builder.SetInitialFragmentGeometry(fragment_geometry);
    builder.SetIsLegacyLayoutRoot();
    if (box_->ShouldComputeSizeAsReplaced()) {
      builder.SetIntrinsicBlockSize(box_->LogicalHeight());
    } else {
      builder.SetIntrinsicBlockSize(
          box_->IntrinsicContentLogicalHeight() +
          box_->BorderAndPaddingLogicalHeight() +
          box_->ComputeLogicalScrollbars().BlockSum());
    }

    // If we're block-fragmented, we can only handle monolithic content, since
    // the two block fragmentation machineries (NG and legacy) cannot cooperate.
    DCHECK(!constraint_space.HasBlockFragmentation() || IsMonolithic());

    if (constraint_space.IsInitialColumnBalancingPass()) {
      // In the initial column balancing pass we need to provide the tallest
      // unbreakable block-size. However, since the content is monolithic,
      // that's already handled by the parent algorithm (so we don't need to
      // propagate anything here). We still have to tell the builder that we're
      // in this layout pass, though, so that the layout result is set up
      // correctly.
      builder.SetIsInitialColumnBalancingPass();
    }

    CopyBaselinesFromLegacyLayout(constraint_space, &builder);
    layout_result = builder.ToBoxFragment();

    box_->SetCachedLayoutResult(layout_result);

    // If |SetCachedLayoutResult| did not update cached |LayoutResult|,
    // |NeedsLayout()| flag should not be cleared.
    if (needed_layout) {
      if (layout_result != box_->GetCachedLayoutResult()) {
        // TODO(kojii): If we failed to update CachedLayoutResult for other
        // reasons, we'd like to review it.
        NOTREACHED();
        box_->SetNeedsLayout(layout_invalidation_reason::kUnknown);
      }
    }
  } else if (layout_result) {
    // OOF-positioned nodes have a two-tier cache, and their layout results
    // must always contain the correct percentage resolution size.
    // See |NGBlockNode::CachedLayoutResultForOutOfFlowPositioned|.
    const NGConstraintSpace& old_space =
        layout_result->GetConstraintSpaceForCaching();
    bool needs_cached_result_update =
        IsOutOfFlowPositioned() &&
        constraint_space.PercentageResolutionSize() !=
            old_space.PercentageResolutionSize();
    if (needs_cached_result_update) {
      layout_result = base::AdoptRef(new NGLayoutResult(
          *layout_result, constraint_space, layout_result->EndMarginStrut(),
          layout_result->BfcLineOffset(), layout_result->BfcBlockOffset(),
          LayoutUnit() /* block_offset_delta */));
      box_->SetCachedLayoutResult(layout_result);
    }
  }

  UpdateShapeOutsideInfoIfNeeded(
      *layout_result, constraint_space.PercentageResolutionInlineSize());

  return layout_result;
}

scoped_refptr<const NGLayoutResult> NGBlockNode::RunSimplifiedLayout(
    const NGLayoutAlgorithmParams& params,
    const NGLayoutResult& previous_result) const {
  NGSimplifiedLayoutAlgorithm algorithm(params, previous_result);
  if (const auto* previous_box_fragment = DynamicTo<NGPhysicalBoxFragment>(
          &previous_result.PhysicalFragment())) {
    if (previous_box_fragment->HasItems())
      return algorithm.LayoutWithItemsBuilder();
  }
  return algorithm.Layout();
}

void NGBlockNode::CopyBaselinesFromLegacyLayout(
    const NGConstraintSpace& constraint_space,
    NGBoxFragmentBuilder* builder) const {
  // As the calls to query baselines from legacy layout are potentially
  // expensive we only ask for them if needed.
  // TODO(layout-dev): Once we have flexbox, and editing switched over to
  // LayoutNG we should be able to safely remove this flag without a
  // performance penalty.
  if (!constraint_space.NeedsBaseline())
    return;

  switch (constraint_space.BaselineAlgorithmType()) {
    case NGBaselineAlgorithmType::kFirstLine: {
      LayoutUnit position = box_->FirstLineBoxBaseline();
      if (position != -1)
        builder->SetBaseline(position);
      break;
    }
    case NGBaselineAlgorithmType::kInlineBlock: {
      LayoutUnit position =
          AtomicInlineBaselineFromLegacyLayout(constraint_space);
      if (position != -1)
        builder->SetBaseline(position);
      break;
    }
  }
}

LayoutUnit NGBlockNode::AtomicInlineBaselineFromLegacyLayout(
    const NGConstraintSpace& constraint_space) const {
  LineDirectionMode line_direction = box_->IsHorizontalWritingMode()
                                         ? LineDirectionMode::kHorizontalLine
                                         : LineDirectionMode::kVerticalLine;

  // If this is an inline box, use |BaselinePosition()|. Some LayoutObject
  // classes override it assuming inline layout calls |BaselinePosition()|.
  if (box_->IsInline()) {
    LayoutUnit position = LayoutUnit(box_->BaselinePosition(
        box_->Style()->GetFontBaseline(), constraint_space.UseFirstLineStyle(),
        line_direction, kPositionOnContainingLine));

    // BaselinePosition() uses margin edge for atomic inlines. Subtract
    // margin-over so that the position is relative to the border box.
    if (box_->IsAtomicInlineLevel())
      position -= box_->MarginOver();

    if (IsFlippedLinesWritingMode(constraint_space.GetWritingMode()))
      return box_->Size().Width() - position;

    return position;
  }

  // If this is a block box, use |InlineBlockBaseline()|. When an inline block
  // has block children, their inline block baselines need to be propagated.
  return box_->InlineBlockBaseline(line_direction);
}

// Floats can optionally have a shape area, specifed by "shape-outside". The
// current shape machinery requires setting the size of the float after layout
// in the parents writing mode.
void NGBlockNode::UpdateShapeOutsideInfoIfNeeded(
    const NGLayoutResult& layout_result,
    LayoutUnit percentage_resolution_inline_size) const {
  if (!box_->IsFloating() || !box_->GetShapeOutsideInfo())
    return;

  // The box_ may not have a valid size yet (due to an intermediate layout),
  // use the fragment's size instead.
  LayoutSize box_size = layout_result.PhysicalFragment().Size().ToLayoutSize();

  // TODO(ikilpatrick): Ideally this should be moved to a NGLayoutResult
  // computing the shape area. There may be an issue with the new fragmentation
  // model and computing the correct sizes of shapes.
  ShapeOutsideInfo* shape_outside = box_->GetShapeOutsideInfo();
  LayoutBlock* containing_block = box_->ContainingBlock();
  shape_outside->SetReferenceBoxLogicalSize(
      containing_block->IsHorizontalWritingMode() ? box_size
                                                  : box_size.TransposedSize());
  shape_outside->SetPercentageResolutionInlineSize(
      percentage_resolution_inline_size);
}

void NGBlockNode::UseLegacyOutOfFlowPositioning() const {
  DCHECK(box_->IsOutOfFlowPositioned());
  box_->ContainingBlock()->InsertPositionedObject(box_);
}

void NGBlockNode::StoreMargins(const NGConstraintSpace& constraint_space,
                               const NGBoxStrut& margins) {
  NGPhysicalBoxStrut physical_margins =
      margins.ConvertToPhysical(constraint_space.GetWritingDirection());
  box_->SetMargin(physical_margins);
}

void NGBlockNode::StoreMargins(const NGPhysicalBoxStrut& physical_margins) {
  box_->SetMargin(physical_margins);
}

void NGBlockNode::AddColumnResult(
    scoped_refptr<const NGLayoutResult> result,
    const NGBlockBreakToken* incoming_break_token) const {
  wtf_size_t index = FragmentIndex(incoming_break_token);
  GetFlowThread(To<LayoutBlockFlow>(box_))->AddLayoutResult(result, index);
}

}  // namespace blink
