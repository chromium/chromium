// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"

#include <memory>

#include "third_party/blink/renderer/core/css/style_engine.h"
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
#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"
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
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_token_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/ng_math_under_over_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment_repeater.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_frame_set_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_input_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_page_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_replaced_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_simplified_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_row_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/ng/table/ng_table_section_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_fraction_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_padded_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_radical_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_scripts_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_space_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_token_element.h"
#include "third_party/blink/renderer/core/mathml/mathml_under_over_element.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/size_f.h"

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
    } else if (IsA<MathMLTokenElement>(element)) {
      if (IsOperatorWithSpecialShaping(params.node))
        CreateAlgorithmAndRun<NGMathOperatorLayoutAlgorithm>(params, callback);
      else if (IsTextOnlyToken(params.node))
        CreateAlgorithmAndRun<NGMathTokenLayoutAlgorithm>(params, callback);
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
  } else if (box.IsLayoutNGGrid()) {
    CreateAlgorithmAndRun<NGGridLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutReplaced()) {
    CreateAlgorithmAndRun<NGReplacedLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutNGFieldset()) {
    CreateAlgorithmAndRun<NGFieldsetLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutNGFrameSet()) {
    CreateAlgorithmAndRun<NGFrameSetLayoutAlgorithm>(params, callback);
  }
  // If there's a legacy layout box, we can only do block fragmentation if
  // we would have done block fragmentation with the legacy engine.
  // Otherwise writing data back into the legacy tree will fail. Look for
  // the flow thread.
  else if (GetFlowThread(box) && style.SpecifiesColumns()) {
    CreateAlgorithmAndRun<NGColumnLayoutAlgorithm>(params, callback);
  } else if (UNLIKELY(!box.Parent() && params.node.IsPaginatedRoot())) {
    DCHECK(RuntimeEnabledFeatures::LayoutNGPrintingEnabled());
    CreateAlgorithmAndRun<NGPageLayoutAlgorithm>(params, callback);
  } else {
    CreateAlgorithmAndRun<NGBlockLayoutAlgorithm>(params, callback);
  }
}

inline const NGLayoutResult* LayoutWithAlgorithm(
    const NGLayoutAlgorithmParams& params) {
  const NGLayoutResult* result = nullptr;
  DetermineAlgorithmAndRun(params,
                           [&result](NGLayoutAlgorithmOperations* algorithm) {
                             result = algorithm->Layout();
                           });
  return result;
}

inline MinMaxSizesResult ComputeMinMaxSizesWithAlgorithm(
    const NGLayoutAlgorithmParams& params,
    const MinMaxSizesFloatInput& float_input) {
  MinMaxSizesResult result;
  DetermineAlgorithmAndRun(
      params, [&result, &float_input](NGLayoutAlgorithmOperations* algorithm) {
        result = algorithm->ComputeMinMaxSizes(float_input);
      });
  return result;
}

LayoutUnit CalculateAvailableInlineSizeForLegacy(
    const LayoutBox& box,
    const NGConstraintSpace& space) {
  if (box.ShouldComputeSizeAsReplaced()) {
    return space.ReplacedPercentageResolutionInlineSize()
        .ClampIndefiniteToZero();
  }

  return space.PercentageResolutionInlineSize().ClampIndefiniteToZero();
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
        !space.IsInitialBlockSizeIndefinite();
  }
  input->stretch_inline_size_if_auto = space.IsInlineAutoBehaviorStretch();
  input->stretch_block_size_if_auto =
      space.IsBlockAutoBehaviorStretch() &&
      space.AvailableSize().block_size != kIndefiniteSize;
}

bool CanUseCachedIntrinsicInlineSizes(const NGConstraintSpace& constraint_space,
                                      const MinMaxSizesFloatInput& float_input,
                                      const NGBlockNode& node) {
  // Obviously can't use the cache if our intrinsic logical widths are dirty.
  if (node.GetLayoutBox()->IntrinsicLogicalWidthsDirty())
    return false;

  // We don't store the float inline sizes for comparison, always skip the
  // cache in this case.
  if (float_input.float_left_inline_size || float_input.float_right_inline_size)
    return false;

  // Check if we have any percentage inline padding.
  const auto& style = node.Style();
  if (style.MayHavePadding() && (style.PaddingStart().IsPercentOrCalc() ||
                                 style.PaddingEnd().IsPercentOrCalc()))
    return false;

  if (node.HasAspectRatio() && (style.LogicalMinHeight().IsPercentOrCalc() ||
                                style.LogicalMaxHeight().IsPercentOrCalc()))
    return false;

  if (node.IsNGTableCell() && To<LayoutNGTableCell>(node.GetLayoutBox())
                                      ->IntrinsicLogicalWidthsBorderSizes() !=
                                  constraint_space.TableCellBorders())
    return false;

  // We may have something like:
  // "grid-template-columns: repeat(auto-fill, 50px); min-width: 50%;"
  // In this specific case our min/max sizes are now dependent on what
  // "min-width" resolves to - which is unique to grid.
  if (node.IsGrid() && (style.LogicalMinWidth().IsPercentOrCalc() ||
                        style.LogicalMaxWidth().IsPercentOrCalc()))
    return false;

  return true;
}

absl::optional<LayoutUnit> ContentMinimumInlineSize(
    const NGBlockNode& block_node,
    const NGBoxStrut& border_padding) {
  // Table layout is never allowed to go below the min-intrinsic size.
  if (block_node.IsTable())
    return absl::nullopt;

  const auto* node = block_node.GetDOMNode();
  const auto* marquee_element = DynamicTo<HTMLMarqueeElement>(node);
  if (marquee_element && marquee_element->IsHorizontal())
    return border_padding.InlineSum();

  const auto& style = block_node.Style();
  const auto& main_inline_size = style.LogicalWidth();

  if (!main_inline_size.IsPercentOrCalc())
    return absl::nullopt;

  // Manually resolve the main-length against zero. calc() expressions may
  // resolve to something greater than "zero".
  LayoutUnit inline_size =
      MinimumValueForLength(main_inline_size, LayoutUnit());
  if (style.BoxSizing() == EBoxSizing::kBorderBox)
    inline_size = std::max(border_padding.InlineSum(), inline_size);
  else
    inline_size += border_padding.InlineSum();

  if (block_node.IsTextControl())
    return inline_size;
  if (IsA<HTMLSelectElement>(node))
    return inline_size;
  if (const auto* input_element = DynamicTo<HTMLInputElement>(node)) {
    const AtomicString& type = input_element->type();
    if (type == input_type_names::kFile)
      return inline_size;
    if (type == input_type_names::kRange)
      return inline_size;
  }
  return absl::nullopt;
}

}  // namespace

const NGLayoutResult* NGBlockNode::Layout(
    const NGConstraintSpace& constraint_space,
    const NGBlockBreakToken* break_token,
    const NGEarlyBreak* early_break,
    const NGColumnSpannerPath* column_spanner_path) const {
  // Use the old layout code and synthesize a fragment.
  if (!CanUseNewLayout())
    return RunLegacyLayout(constraint_space);

  // The exclusion space internally is a pointer to a shared vector, and
  // equality of exclusion spaces is performed using pointer comparison on this
  // internal shared vector.
  // In order for the caching logic to work correctly we need to set the
  // pointer to the value previous shared vector.
  if (const NGLayoutResult* previous_result =
          box_->GetCachedLayoutResult(break_token)) {
    constraint_space.ExclusionSpace().PreInitialize(
        previous_result->GetConstraintSpaceForCaching().ExclusionSpace());
  }

  NGLayoutCacheStatus cache_status;

  // We may be able to hit the cache without calculating fragment geometry
  // (calculating that isn't necessarily very cheap). So, start off without it.
  absl::optional<NGFragmentGeometry> fragment_geometry;

  // CachedLayoutResult() might clear flags, so remember the need for layout
  // before attempting to hit the cache.
  bool needed_layout = box_->NeedsLayout();
  if (needed_layout)
    box_->GetFrameView()->IncBlockLayoutCount();

  const NGLayoutResult* layout_result = box_->CachedLayoutResult(
      constraint_space, break_token, early_break, column_spanner_path,
      &fragment_geometry, &cache_status);

  if ((cache_status == NGLayoutCacheStatus::kHit ||
       cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout) &&
      needed_layout && constraint_space.CacheSlot() == NGCacheSlot::kLayout &&
      box_->HasBrokenSpine() && !ChildLayoutBlockedByDisplayLock()) {
    // If we're not guaranteed to discard the old fragment (which we're only
    // guaranteed to do if we have decided to perform full layout), we need to
    // clone the result to pick the most recent fragments from the LayoutBox
    // children, because we stopped rebuilding the fragment spine right here
    // after performing subtree layout.
    layout_result =
        NGLayoutResult::CloneWithPostLayoutFragments(*layout_result);
    const auto& new_fragment =
        To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());
    // If we have fragment items, and we're not done (more fragments to follow),
    // be sure to miss the cache for any subsequent fragments, lest finalization
    // be missed (which could cause trouble for NGInlineCursor when walking the
    // items).
    bool clear_trailing_results =
        new_fragment.BreakToken() && new_fragment.HasItems();
    StoreResultInLayoutBox(layout_result, break_token, clear_trailing_results);
    box_->ClearHasBrokenSpine();
  }

  if (cache_status == NGLayoutCacheStatus::kHit) {
    DCHECK(layout_result);

    // We may have to update the margins on box_; we reuse the layout result
    // even if a percentage margin may have changed.
    UpdateMarginPaddingInfoIfNeeded(constraint_space);

    UpdateShapeOutsideInfoIfNeeded(
        *layout_result, constraint_space.PercentageResolutionInlineSize());

    // Return the cached result unless we're marked for layout. We may have
    // added or removed scrollbars during overflow recalculation, which may have
    // marked us for layout. In that case the cached result is unusable, and we
    // need to re-lay out now.
    if (!box_->NeedsLayout())
      return layout_result;
  }

  if (!fragment_geometry) {
    fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this, break_token);
  }

  if (
      // Only consider the size of the first container fragment.
      !IsBreakInside(break_token) && CanMatchSizeContainerQueries()) {
    if (auto* element = DynamicTo<Element>(GetDOMNode())) {
      LogicalSize available_size = CalculateChildAvailableSize(
          constraint_space, *this, fragment_geometry->border_box_size,
          fragment_geometry->border + fragment_geometry->padding);
      LogicalAxes contained_axes = ContainedAxes();
      GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
          *element, available_size, contained_axes);

      // Try the cache again. Container query matching may have affected
      // elements in the subtree, so that we need full layout instead of
      // simplified layout, for instance.
      layout_result = box_->CachedLayoutResult(
          constraint_space, break_token, early_break, column_spanner_path,
          &fragment_geometry, &cache_status);
    }
  }

  TextAutosizer::NGLayoutScope text_autosizer_layout_scope(
      box_, fragment_geometry->border_box_size.inline_size);

  PrepareForLayout();

  NGLayoutAlgorithmParams params(*this, *fragment_geometry, constraint_space,
                                 break_token, early_break);
  params.column_spanner_path = column_spanner_path;

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_.Get());

  // Try to perform "simplified" layout, unless it's a fragmentation context
  // root (the simplified layout algorithm doesn't support fragmentainers).
  if (cache_status == NGLayoutCacheStatus::kNeedsSimplifiedLayout &&
      (!block_flow || !block_flow->IsFragmentationContextRoot())) {
    DCHECK(layout_result);
#if DCHECK_IS_ON()
    const NGLayoutResult* previous_result = layout_result;
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
    params.previous_result = layout_result;
    layout_result = nullptr;
  } else {
    layout_result = nullptr;
  }

  // All these variables may change after layout due to scrollbars changing.
  NGBoxStrut scrollbars_before = ComputeScrollbars(constraint_space, *this);
  const LayoutUnit inline_size_before =
      fragment_geometry->border_box_size.inline_size;
  const bool intrinsic_logical_widths_dirty_before =
      box_->IntrinsicLogicalWidthsDirty();

  if (!layout_result)
    layout_result = LayoutWithAlgorithm(params);

  // PaintLayerScrollableArea::UpdateAfterLayout() may remove the vertical
  // scrollbar. In vertical-rl or RTL, the vertical scrollbar is on the
  // block-start edge or the inline-start edge, it produces a negative
  // MaximumScrollOffset(), and can cause a wrong clamping. So we delay
  // clamping the offset.
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  FinishLayout(block_flow, constraint_space, break_token, layout_result,
               box_->Size());

  // We may be intrinsicly sized (shrink-to-fit), if our intrinsic logical
  // widths are now dirty, re-calculate our inline-size for comparison.
  if (!intrinsic_logical_widths_dirty_before &&
      box_->IntrinsicLogicalWidthsDirty()) {
    fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this, break_token);
  }

  // We may need to relayout if:
  // - Our scrollbars have changed causing our size to change (shrink-to-fit)
  //   or the available space to our children changing.
  // - A child changed scrollbars causing our size to change (shrink-to-fit).
  //
  // Skip this part if side-effects aren't allowed, though. Calling
  // ClearLayoutResults() when in this state is forbidden. Also skip it if we
  // are resuming layout after a fragmentainer break. Changing the intrinsic
  // inline-size halfway through layout of a node doesn't make sense.
  NGBoxStrut scrollbars_after = ComputeScrollbars(constraint_space, *this);
  if ((scrollbars_before != scrollbars_after ||
       inline_size_before != fragment_geometry->border_box_size.inline_size) &&
      !NGDisableSideEffectsScope::IsDisabled() && !IsBreakInside(break_token)) {
    bool freeze_horizontal = false, freeze_vertical = false;
    // If we're in a measure pass, freeze both scrollbars right away, to avoid
    // quadratic time complexity for deeply nested flexboxes.
    if (constraint_space.CacheSlot() == NGCacheSlot::kMeasure)
      freeze_horizontal = freeze_vertical = true;
    do {
      // Freeze any scrollbars that appeared, and relayout. Repeat until both
      // have appeared, or until the scrollbar situation doesn't change,
      // whichever comes first.
      AddScrollbarFreeze(scrollbars_before, scrollbars_after,
                         constraint_space.GetWritingDirection(),
                         &freeze_horizontal, &freeze_vertical);
      scrollbars_before = scrollbars_after;
      PaintLayerScrollableArea::FreezeScrollbarsRootScope freezer(
          *box_, freeze_horizontal, freeze_vertical);

      // We need to clear any previous results when scrollbars change. For
      // example - we may have stored a "measure" layout result which will be
      // incorrect if we try and reuse it.
      LayoutSize old_box_size = box_->Size();
      params.previous_result = nullptr;
      box_->ClearLayoutResults();

#if DCHECK_IS_ON()
      // Ensure turning on/off scrollbars only once at most, when we call
      // |LayoutWithAlgorithm| recursively.
      DEFINE_STATIC_LOCAL(
          Persistent<HeapHashSet<WeakMember<LayoutBox>>>, scrollbar_changed,
          (MakeGarbageCollected<HeapHashSet<WeakMember<LayoutBox>>>()));
      DCHECK(scrollbar_changed->insert(box_.Get()).is_new_entry);
#endif

      // Scrollbar changes are hard to detect. Make sure everyone gets the
      // message.
      box_->SetNeedsLayout(layout_invalidation_reason::kScrollbarChanged,
                           kMarkOnlyThis);

      fragment_geometry = CalculateInitialFragmentGeometry(constraint_space,
                                                           *this, break_token);
      layout_result = LayoutWithAlgorithm(params);
      FinishLayout(block_flow, constraint_space, break_token, layout_result,
                   old_box_size);

#if DCHECK_IS_ON()
      scrollbar_changed->erase(box_);
#endif

      scrollbars_after = ComputeScrollbars(constraint_space, *this);
      DCHECK(!freeze_horizontal || !freeze_vertical ||
             scrollbars_after == scrollbars_before);
    } while (scrollbars_after != scrollbars_before);
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

  return layout_result;
}

const NGLayoutResult* NGBlockNode::SimplifiedLayout(
    const NGPhysicalFragment& previous_fragment) const {
  const NGLayoutResult* previous_result = box_->GetSingleCachedLayoutResult();
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
  const NGLayoutResult* result = Layout(space, /* break_token */ nullptr);

  if (result->Status() != NGLayoutResult::kSuccess) {
    // TODO(crbug.com/1297864): The optimistic BFC block-offsets aren't being
    // set correctly for block-in-inline causing these layouts to fail.
    NOTREACHED();
    return nullptr;
  }

  const auto& old_fragment =
      To<NGPhysicalBoxFragment>(previous_result->PhysicalFragment());
  const auto& new_fragment =
      To<NGPhysicalBoxFragment>(result->PhysicalFragment());

  // Simplified layout has the ability to add/remove scrollbars, this can cause
  // a couple (rare) edge-cases which will make the fragment different enough
  // that the parent should perform a full layout.
  //  - The size has changed.
  //  - The alignment baseline has shifted.
  // We return a nullptr in these cases indicating to our parent that it needs
  // to perform a full layout.
  if (old_fragment.Size() != new_fragment.Size())
    return nullptr;
  if (old_fragment.FirstBaseline() != new_fragment.FirstBaseline())
    return nullptr;
  if (old_fragment.LastBaseline() != new_fragment.LastBaseline())
    return nullptr;

#if DCHECK_IS_ON()
  result->CheckSameForSimplifiedLayout(*previous_result);
#endif

  return result;
}

const NGLayoutResult* NGBlockNode::LayoutRepeatableRoot(
    const NGConstraintSpace& constraint_space,
    const NGBlockBreakToken* break_token) const {
  // We read and write the physical fragments vector in LayoutBox here, which
  // isn't allowed if side-effects are disabled. Call-sites must make sure that
  // we don't attempt to repeat content if side-effects are disabled.
  DCHECK(!NGDisableSideEffectsScope::IsDisabled());

  // When laying out repeatable content, we cannot at the same time allow it to
  // break inside.
  DCHECK(!constraint_space.HasBlockFragmentation());

  // We can't both resume and repeat!
  DCHECK(!IsBreakInside(break_token));

  bool is_first = !break_token || !break_token->IsRepeated();
  const NGLayoutResult* result;
  if (is_first) {
    // We're generating the first fragment for repeated content. Perform regular
    // layout.
    result = Layout(constraint_space, break_token);
    DCHECK(!result->PhysicalFragment().BreakToken());
  } else {
    // We're repeating. Create a shallow clone of the first result. Once we're
    // at the last fragment, we'll actually create a deep clone.
    result = NGLayoutResult::Clone(*box_->GetLayoutResult(0));
  }

  wtf_size_t index = FragmentIndex(break_token);
  const auto& fragment = To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  // We need to create a special "repeat" break token, which will be the
  // incoming break token when generating the next fragment. This is needed in
  // order to get the sequence numbers right, which is important when adding the
  // result to the LayoutBox, and it's also needed by pre-paint / paint.
  const NGBlockBreakToken* outgoing_break_token =
      NGBlockBreakToken::CreateRepeated(*this, index);
  auto mutator = fragment.GetMutableForCloning();
  mutator.SetBreakToken(outgoing_break_token);
  if (!is_first) {
    mutator.ClearIsFirstForNode();

    // Any OOFs whose containing block is an ancestor of the repeated section is
    // not to be repeated.
    mutator.ClearPropagatedOOFs();

    box_->SetLayoutResult(result, index);
  }

  if (!constraint_space.ShouldRepeat()) {
    FinishRepeatableRoot();
  }

  return result;
}

void NGBlockNode::FinishRepeatableRoot() const {
  DCHECK(!NGDisableSideEffectsScope::IsDisabled());

  // This is the last fragment. It won't be repeated again. We have already
  // created fragments for the repeated nodes, but the cloning was shallow.
  // We're now ready to deep-clone the entire subtree for each repeated
  // fragment, and update the layout result vector in the LayoutBox, including
  // setting correct break tokens with sequence numbers.

  // First remove the outgoing break token from the last fragment, that was set
  // in LayoutRepeatableRoot().
  const NGPhysicalBoxFragment& last_fragment = box_->PhysicalFragments().back();
  auto mutator = last_fragment.GetMutableForCloning();
  mutator.SetBreakToken(nullptr);

  box_->FinalizeLayoutResults();

  wtf_size_t fragment_count = box_->PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  box_->ClearNeedsLayout();
  for (wtf_size_t i = 1; i < fragment_count; i++) {
    const NGPhysicalBoxFragment& physical_fragment =
        *box_->GetPhysicalFragment(i);
    bool is_first = i == 1;
    bool is_last = i + 1 == fragment_count;
    NGFragmentRepeater repeater(is_first, is_last);
    repeater.CloneChildFragments(physical_fragment);
  }
}

const NGLayoutResult* NGBlockNode::CachedLayoutResultForOutOfFlowPositioned(
    LogicalSize container_content_size) const {
  DCHECK(IsOutOfFlowPositioned());

  if (box_->NeedsLayout())
    return nullptr;

  // If there are multiple fragments, we wouldn't know which one to use, since
  // no break token is passed.
  if (box_->PhysicalFragmentCount() > 1)
    return nullptr;

  const NGLayoutResult* cached_layout_result =
      box_->GetSingleCachedLayoutResult();
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
  auto* block = DynamicTo<LayoutBlock>(box_.Get());
  if (block && block->IsScrollContainer()) {
    DCHECK(block->GetScrollableArea());
    if (block->GetScrollableArea()->ShouldPerformScrollAnchoring())
      block->GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();
  }

  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    To<LayoutNGListItem>(box_.Get())->UpdateMarkerTextIfNeeded();
}

void NGBlockNode::FinishLayout(LayoutBlockFlow* block_flow,
                               const NGConstraintSpace& constraint_space,
                               const NGBlockBreakToken* break_token,
                               const NGLayoutResult* layout_result,
                               LayoutSize old_box_size) const {
  // Computing MinMax after layout. Do not modify the |LayoutObject| tree, paint
  // properties, and other global states.
  if (NGDisableSideEffectsScope::IsDisabled())
    return;

  // If we abort layout and don't clear the cached layout-result, we can end
  // up in a state where the layout-object tree doesn't match fragment tree
  // referenced by this layout-result.
  if (layout_result->Status() != NGLayoutResult::kSuccess) {
    // This would be really dangerous to do if we're not at the first fragment,
    // though, as it would mean that we'd also clear the first successful
    // result(s).
    DCHECK(!IsBreakInside(break_token));

    box_->ClearLayoutResults();
    return;
  }

  const auto& physical_fragment =
      To<NGPhysicalBoxFragment>(layout_result->PhysicalFragment());

  if (box_->IsLayoutReplaced()) {
    DCHECK(CanUseNewLayout());
    // NG replaced elements are painted with legacy painters. We need to force
    // a legacy "layout" so that paint invalidation flags are updated. But we
    // don't want to use the size that legacy calculates, so we force legacy to
    // use NG's size via BoxLayoutExtraInput's override fields.
    BoxLayoutExtraInput input(*box_);
    SetupBoxLayoutExtraInput(constraint_space, *box_, &input);
    NGBoxFragment fragment(constraint_space.GetWritingDirection(),
                           physical_fragment);
    DCHECK_EQ(input.override_inline_size.value_or(fragment.InlineSize()),
              fragment.InlineSize())
        << "Forced inline size wasn't the fragment's inline size?";
    DCHECK_EQ(input.override_block_size.value_or(fragment.BlockSize()),
              fragment.BlockSize())
        << "Forced block size wasn't the fragment's block size?";
    input.override_inline_size = fragment.InlineSize();
    input.override_block_size = fragment.BlockSize();
    if (RuntimeEnabledFeatures::LayoutNGReplacedNoBoxSettersEnabled()) {
      input.border_padding_for_replaced =
          physical_fragment.Borders() + physical_fragment.Padding();
    }
    box_->ComputeAndSetBlockDirectionMargins(box_->ContainingBlock());
    if (box_->NeedsLayout())
      box_->LayoutIfNeeded();
    else
      box_->ForceLayout();

#if DCHECK_IS_ON()
    if (!RuntimeEnabledFeatures::LayoutNGReplacedNoBoxSettersEnabled()) {
      // Assert that legacy uses the size NG forces above. But legacy sends
      // LayoutUnit to float and back, which can slightly change the result. So
      // give a 1px cushion.
      PhysicalSize difference =
          PhysicalSize(box_->Size()) - physical_fragment.Size();
      DCHECK_LE(difference.width.Abs(), LayoutUnit(1))
          << box_->Size() << " " << physical_fragment.Size();
      DCHECK_LE(difference.height.Abs(), LayoutUnit(1))
          << box_->Size() << " " << physical_fragment.Size();
    }
#endif
  }

  // If we miss the cache for one result (fragment), we need to clear the
  // remaining ones, to make sure that we don't hit the cache for subsequent
  // fragments. If we re-lay out (which is what we just did), there's no way to
  // tell what happened in this subtree. Some fragment vector in the subtree may
  // have been tampered with, which would cause trouble if we start hitting the
  // cache again later on.
  bool clear_trailing_results =
      break_token || box_->PhysicalFragmentCount() > 1;

  StoreResultInLayoutBox(layout_result, break_token, clear_trailing_results);

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
      if (items)
        CopyFragmentItemsToLayoutBox(physical_fragment, *items, break_token);
    } else {
      // We still need to clear |NGInlineNodeData| in case it had inline
      // children.
      block_flow->ClearNGInlineNodeData();
    }
  } else {
    DCHECK(!physical_fragment.HasItems());
  }

  CopyFragmentDataToLayoutBox(constraint_space, *layout_result, break_token);
  if (RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled() &&
      !layout_result->PhysicalFragment().BreakToken() &&
      box_->Size() != old_box_size) {
    box_->SizeChanged();
  }
}

void NGBlockNode::StoreResultInLayoutBox(const NGLayoutResult* result,
                                         const NGBlockBreakToken* break_token,
                                         bool clear_trailing_results) const {
  const auto& fragment = To<NGPhysicalBoxFragment>(result->PhysicalFragment());
  wtf_size_t fragment_idx = 0;

  if (fragment.IsOnlyForNode()) {
    box_->SetCachedLayoutResult(std::move(result), 0);
  } else {
    // Add all layout results (and fragments) generated from a node to a list in
    // the layout object. Some extra care is required to correctly overwrite
    // intermediate layout results: The sequence number of an incoming break
    // token corresponds with the fragment index in the layout object (off by 1,
    // though). When writing back a layout result, we remove any fragments in
    // the layout box at higher indices than that of the one we're writing back.
    fragment_idx = FragmentIndex(break_token);
    box_->SetLayoutResult(std::move(result), fragment_idx);
  }

  if (clear_trailing_results)
    box_->ShrinkLayoutResults(fragment_idx + 1);
}

MinMaxSizesResult NGBlockNode::ComputeMinMaxSizes(
    WritingMode container_writing_mode,
    const MinMaxSizesType type,
    const NGConstraintSpace& constraint_space,
    const MinMaxSizesFloatInput float_input) const {
  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    To<LayoutNGListItem>(box_.Get())->UpdateMarkerTextIfNeeded();

  const bool is_in_perform_layout = box_->GetFrameView()->IsInPerformLayout();
  // In some scenarios, GridNG will run layout on its children during
  // MinMaxSizes computation. Instead of running (and possible caching incorrect
  // results), when we're not performing layout, just use border + padding.
  if (!is_in_perform_layout && IsGrid()) {
    const NGFragmentGeometry fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this,
                                         /* break_token */ nullptr,
                                         /* is_intrinsic */ true);
    const NGBoxStrut border_padding =
        fragment_geometry.border + fragment_geometry.padding;
    MinMaxSizes sizes;
    sizes.min_size = border_padding.InlineSum();
    sizes.max_size = sizes.min_size;
    return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
  }

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
    if (!is_in_perform_layout) {
      sizes = ComputeMinMaxSizesFromLegacy(type, constraint_space);
      return MinMaxSizesResult(sizes,
                               /* depends_on_block_constraints */ false);
    }

    // If we're computing MinMax after layout, we need to disable side effects
    // so that |Layout| does not update the |LayoutObject| tree and other global
    // states.
    absl::optional<NGDisableSideEffectsScope> disable_side_effects;
    if (!GetLayoutBox()->NeedsLayout())
      disable_side_effects.emplace();

    const NGLayoutResult* layout_result = Layout(constraint_space);
    DCHECK_EQ(layout_result->Status(), NGLayoutResult::kSuccess);
    sizes = NGFragment({container_writing_mode, TextDirection::kLtr},
                       layout_result->PhysicalFragment())
                .InlineSize();
    return MinMaxSizesResult(sizes,
                             /* depends_on_block_constraints */ false);
  }

  // Returns if we are (directly) dependent on any block constraints.
  auto DependsOnBlockConstraints = [&]() -> bool {
    return Style().LogicalHeight().IsPercentOrCalc() ||
           Style().LogicalMinHeight().IsPercentOrCalc() ||
           Style().LogicalMaxHeight().IsPercentOrCalc() ||
           (Style().LogicalHeight().IsAuto() &&
            constraint_space.IsBlockAutoBehaviorStretch());
  };

  if (!Style().AspectRatio().IsAuto() && !IsReplaced() &&
      type == MinMaxSizesType::kContent) {
    const NGFragmentGeometry fragment_geometry =
        CalculateInitialFragmentGeometry(constraint_space, *this,
                                         /* break_token */ nullptr,
                                         /* is_intrinsic */ true);
    const NGBoxStrut border_padding =
        fragment_geometry.border + fragment_geometry.padding;
    if (fragment_geometry.border_box_size.block_size != kIndefiniteSize) {
      const LayoutUnit inline_size_from_ar = InlineSizeFromAspectRatio(
          border_padding, Style().LogicalAspectRatio(),
          Style().BoxSizingForAspectRatio(),
          fragment_geometry.border_box_size.block_size);
      return MinMaxSizesResult({inline_size_from_ar, inline_size_from_ar},
                               DependsOnBlockConstraints());
    }
  }

  bool can_use_cached_intrinsic_inline_sizes =
      CanUseCachedIntrinsicInlineSizes(constraint_space, float_input, *this);

  // Use our cached sizes if we don't have a descendant which depends on our
  // block constraints.
  if (can_use_cached_intrinsic_inline_sizes &&
      !box_->IntrinsicLogicalWidthsChildDependsOnBlockConstraints()) {
    MinMaxSizes sizes = box_->IsTable() && !box_->IsLayoutNGObject()
                            ? box_->PreferredLogicalWidths()
                            : box_->IntrinsicLogicalWidths(type);
    bool depends_on_block_constraints =
        box_->IntrinsicLogicalWidthsDependsOnBlockConstraints();
    return MinMaxSizesResult(sizes, depends_on_block_constraints);
  }

  // Determine if we are dependent on the block-constraints.
  bool self_depends_on_block_constraints =
      DependsOnBlockConstraints() ||
      UseParentPercentageResolutionBlockSizeForChildren();

  const NGFragmentGeometry fragment_geometry = CalculateInitialFragmentGeometry(
      constraint_space, *this, /* break_token */ nullptr,
      /* is_intrinsic */ true);
  const LayoutUnit initial_block_size =
      fragment_geometry.border_box_size.block_size;

  // We might still be able to use the cached values if our children don't
  // depend on the *input* %-block-size.
  if (can_use_cached_intrinsic_inline_sizes &&
      initial_block_size == box_->IntrinsicLogicalWidthsInitialBlockSize() &&
      !UseParentPercentageResolutionBlockSizeForChildren()) {
    DCHECK(box_->IntrinsicLogicalWidthsChildDependsOnBlockConstraints());
    MinMaxSizes sizes = box_->IsTable() && !box_->IsLayoutNGObject()
                            ? box_->PreferredLogicalWidths()
                            : box_->IntrinsicLogicalWidths(type);
    return MinMaxSizesResult(sizes, self_depends_on_block_constraints);
  }

  box_->SetIntrinsicLogicalWidthsDirty(kMarkOnlyThis);

  if (!CanUseNewLayout()) {
    MinMaxSizes sizes = ComputeMinMaxSizesFromLegacy(type, constraint_space);

    // Update the cache bits for this legacy root (but not the intrinsic
    // inline-sizes themselves).
    box_->SetIntrinsicLogicalWidthsFromNG(
        initial_block_size, self_depends_on_block_constraints,
        /* child_depends_on_block_constraints */ true,
        /* sizes */ nullptr);

    return MinMaxSizesResult(sizes, self_depends_on_block_constraints);
  }

  const NGBoxStrut border_padding =
      fragment_geometry.border + fragment_geometry.padding;

  MinMaxSizesResult result = ComputeMinMaxSizesWithAlgorithm(
      NGLayoutAlgorithmParams(*this, fragment_geometry, constraint_space),
      float_input);

  if (auto min_size = ContentMinimumInlineSize(*this, border_padding))
    result.sizes.min_size = *min_size;

  bool depends_on_block_constraints =
      self_depends_on_block_constraints && result.depends_on_block_constraints;

  if (!Style().AspectRatio().IsAuto() &&
      BlockLengthUnresolvable(constraint_space, Style().LogicalHeight())) {
    // If the block size will be computed from the aspect ratio, we need
    // to take the max-block-size into account.
    // https://drafts.csswg.org/css-sizing-4/#aspect-ratio
    MinMaxSizes min_max = ComputeMinMaxInlineSizesFromAspectRatio(
        constraint_space, Style(), border_padding);
    result.sizes.min_size = min_max.ClampSizeToMinAndMax(result.sizes.min_size);
    result.sizes.max_size = min_max.ClampSizeToMinAndMax(result.sizes.max_size);
    depends_on_block_constraints =
        depends_on_block_constraints ||
        Style().LogicalMinHeight().IsPercentOrCalc() ||
        Style().LogicalMaxHeight().IsPercentOrCalc();
  }

  box_->SetIntrinsicLogicalWidthsFromNG(
      initial_block_size, depends_on_block_constraints,
      /* child_depends_on_block_constraints */
      result.depends_on_block_constraints, &result.sizes);

  if (IsNGTableCell()) {
    To<LayoutNGTableCell>(box_.Get())
        ->SetIntrinsicLogicalWidthsBorderSizes(
            constraint_space.TableCellBorders());
  }

  // We report to our parent if we depend on the %-block-size if we used the
  // input %-block-size, or one of children said it depended on this.
  result.depends_on_block_constraints = depends_on_block_constraints;
  return result;
}

MinMaxSizes NGBlockNode::ComputeMinMaxSizesFromLegacy(
    const MinMaxSizesType type,
    const NGConstraintSpace& space) const {
  BoxLayoutExtraInput extra_input(*box_);
  SetupBoxLayoutExtraInput(space, *box_, &extra_input);

  // Tables don't calculate their min/max content contribution the same way as
  // other layout nodes. This is because width/min-width/etc have a different
  // meaning for tables.
  //
  // Due to this the min/max content contribution is their min/max content size.
  MinMaxSizes sizes = box_->IsTable() ? box_->PreferredLogicalWidths()
                                      : box_->IntrinsicLogicalWidths(type);

  return sizes;
}

NGLayoutInputNode NGBlockNode::NextSibling() const {
  LayoutObject* next_sibling = box_->NextSibling();

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

  return NGBlockNode(To<LayoutBox>(next_sibling));
}

NGLayoutInputNode NGBlockNode::FirstChild() const {
  auto* block = DynamicTo<LayoutBlock>(box_.Get());
  if (UNLIKELY(!block))
    return NGBlockNode(box_->FirstChildBox());
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (!AreNGBlockFlowChildrenInline(block))
    return NGBlockNode(To<LayoutBox>(child));

  NGInlineNode inline_node(To<LayoutBlockFlow>(block));
  if (!inline_node.IsBlockLevel())
    return std::move(inline_node);

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
  return NGBlockNode(To<LayoutBox>(child));
}

NGBlockNode NGBlockNode::GetRenderedLegend() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return NGBlockNode(
      LayoutFieldset::FindInFlowLegend(*To<LayoutBlock>(box_.Get())));
}

NGBlockNode NGBlockNode::GetFieldsetContent() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return NGBlockNode(
      To<LayoutNGFieldset>(box_.Get())->FindAnonymousFieldsetContentBox());
}

bool NGBlockNode::CanUseNewLayout(const LayoutBox& box) {
  if (box.ForceLegacyLayout())
    return false;
  return box.IsLayoutNGObject() || box.IsLayoutReplaced();
}

bool NGBlockNode::CanUseNewLayout() const {
  return CanUseNewLayout(*box_);
}

LayoutUnit NGBlockNode::EmptyLineBlockSize(
    const NGBlockBreakToken* incoming_break_token) const {
  // Only return a line-height for the first fragment.
  if (IsBreakInside(incoming_break_token))
    return LayoutUnit();
  return box_->LogicalHeightForEmptyLine();
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
  bool is_last_fragment = !physical_fragment.BreakToken();

  if (!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled()) {
    NGBoxFragment fragment(constraint_space.GetWritingDirection(),
                           physical_fragment);
    NGBoxStrut borders = fragment.Borders();
    NGBoxStrut scrollbars = ComputeScrollbars(constraint_space, *this);
    NGBoxStrut padding = fragment.Padding();
    NGBoxStrut border_scrollbar_padding = borders + scrollbars + padding;

    // For each fragment we process, we'll accumulate the block-size. We reset
    // it at the first fragment, and accumulate at each method call for
    // fragments belonging to the same layout object. Inline-size will only be
    // set at the first fragment. Subsequent fragments may have different
    // inline-size (either because fragmentainer inline-size is variable, or
    // e.g. because available inline-size is affected by floats). The legacy
    // engine doesn't handle variable inline-size (since it doesn't really
    // understand fragmentation). This means that things like offsetWidth won't
    // work correctly (since that's still being handled by the legacy engine),
    // but at least layout, painting and hit-testing will be correct.
    if (LIKELY(physical_fragment.IsFirstForNode())) {
      box_->SetSize(LayoutSize(physical_fragment.Size().width,
                               physical_fragment.Size().height));
      // If this is a fragment from a node that didn't break into multiple
      // fragments, write back the intrinsic size. We skip this if the node has
      // fragmented, since intrinsic block-size is rather meaningless in that
      // case, because the block-size may have been affected by something on
      // the outside (i.e. the fragmentainer).
      //
      // If we had a fixed block size, our children will have sized themselves
      // relative to the fixed size, which would make our intrinsic size
      // incorrect (too big). So skip the write-back in that case, too.
      if (LIKELY(is_last_fragment && !constraint_space.IsFixedBlockSize())) {
        box_->SetIntrinsicContentLogicalHeight(
            layout_result.IntrinsicBlockSize() -
            border_scrollbar_padding.BlockSum());
      }
    } else {
      // Update logical height, unless this fragment is past the block-end of
      // the generating node (happens with overflow).
      if (previous_break_token && !previous_break_token->IsAtBlockEnd()) {
        box_->SetLogicalHeight(
            fragment.Size().block_size +
            previous_break_token->ConsumedBlockSizeForLegacy());
      } else {
        DCHECK_EQ(fragment.Size().block_size, LayoutUnit());
      }
    }
  }

  // TODO(mstensho): This should always be done by the parent algorithm, since
  // we may have auto margins, which only the parent is able to resolve. Remove
  // the following line when all layout modes do this properly.
  UpdateMarginPaddingInfoIfNeeded(constraint_space);

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_.Get());
  LayoutMultiColumnFlowThread* flow_thread = GetFlowThread(block_flow);

  // Position the children inside the box. We skip this if display-lock prevents
  // child layout.
  if (!ChildLayoutBlockedByDisplayLock()) {
    if (UNLIKELY(flow_thread)) {
      // Hold off writing legacy data for the entire multicol container until
      // done with the last fragment (we may have multiple if nested within
      // another fragmentation context). This way we'll get everything in order.
      // We'd otherwise mess up in complex cases of nested column balancing. The
      // column layout algorithms may retry layout for a given fragment, which
      // would confuse the code that writes back to legacy objects, so that we
      // wouldn't always update column sets or establish fragmentainer groups
      // correctly.
      if (is_last_fragment) {
        const NGBlockBreakToken* incoming_break_token = nullptr;
        for (const NGPhysicalBoxFragment& multicol_fragment :
             box_->PhysicalFragments()) {
          PlaceChildrenInFlowThread(flow_thread, constraint_space,
                                    multicol_fragment, incoming_break_token);
          incoming_break_token = multicol_fragment.BreakToken();
        }
      }
    } else {
      PlaceChildrenInLayoutBox(physical_fragment, previous_break_token);
    }
  }

  if (UNLIKELY(!is_last_fragment))
    return;

  LayoutBlock* block = DynamicTo<LayoutBlock>(box_.Get());
  bool needs_full_invalidation = false;
  if (LIKELY(block)) {
#if DCHECK_IS_ON()
    block->CheckPositionedObjectsNeedLayout();
#endif

    if (UNLIKELY(flow_thread && Style().HasColumnRule())) {
      // Issue full invalidation, in case the number of column rules have
      // changed.
      needs_full_invalidation = true;
    } else if (block->StyleForContinuationOutline()) {
      // When this is a block-in-inline created by |SplineInlines|, we may need
      // to paint outlines for this. See |NGBoxFragmentPainter|.
      needs_full_invalidation = true;
    }

    block->SetNeedsOverflowRecalc(
        LayoutObject::OverflowRecalcType::kOnlyVisualOverflowRecalc);
    block->SetLayoutOverflowFromLayoutResults();
  }

  // Replaced elements already have |LayoutBox::UpdateAfterLayout| called when
  // we force a layout for them inside |NGBlockNode::FinishLayout|.
  if (RuntimeEnabledFeatures::LayoutNGUnifyUpdateAfterLayoutEnabled() ||
      !box_->IsLayoutReplaced())
    box_->UpdateAfterLayout();

  if (needs_full_invalidation)
    box_->ClearNeedsLayoutWithFullPaintInvalidation();
  else
    box_->ClearNeedsLayout();

  if (!RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled()) {
    // Overflow computation depends on this being set.
    if (LIKELY(block_flow)) {
      block_flow->SetIsSelfCollapsingFromNG(layout_result.IsSelfCollapsing());
    }
  }

  // We should notify the display lock that we've done layout on self, and if
  // it's not blocked, on children.
  if (auto* context = box_->GetDisplayLockContext()) {
    if (!ChildLayoutBlockedByDisplayLock())
      context->DidLayoutChildren();
  }
}

void NGBlockNode::PlaceChildrenInLayoutBox(
    const NGPhysicalBoxFragment& physical_fragment,
    const NGBlockBreakToken* previous_break_token,
    bool needs_invalidation_check) const {
  for (const auto& child_fragment : physical_fragment.Children()) {
    // Skip any line-boxes we have as children, this is handled within
    // NGInlineNode at the moment.
    if (!child_fragment->IsBox())
      continue;

    const auto& box_fragment = *To<NGPhysicalBoxFragment>(child_fragment.get());
    if (!box_fragment.IsFirstForNode())
      continue;

    // The offset for an OOF positioned node that is added as a child of a
    // fragmentainer box is handled by
    // NGOutOfFlowLayoutPart::AddOOFToFragmentainer().
    if (UNLIKELY(physical_fragment.IsFragmentainerBox() &&
                 child_fragment->IsOutOfFlowPositioned()))
      continue;

    CopyChildFragmentPosition(box_fragment, child_fragment.offset,
                              physical_fragment, previous_break_token,
                              needs_invalidation_check);
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

  NGBoxStrut border_scrollbar_padding;
  WritingModeConverter converter(space.GetWritingDirection(),
                                 physical_fragment.Size());
  LayoutUnit column_row_inline_size;
  const bool copy_back = !RuntimeEnabledFeatures::LayoutNGNoCopyBackEnabled();
  if (copy_back) {
    border_scrollbar_padding = ComputeBorders(space, *this) +
                               ComputeScrollbars(space, *this) +
                               ComputePadding(space, Style());
    column_row_inline_size =
        converter.ToLogical(physical_fragment.Size()).inline_size -
        border_scrollbar_padding.InlineSum();
  }

  const NGBlockBreakToken* previous_column_break_token = nullptr;
  LayoutMultiColumnSet* pending_column_set = nullptr;
  LayoutUnit flow_thread_offset;
  bool has_processed_first_column_in_flow_thread = false;
  bool should_append_fragmentainer_group = false;
  bool should_expand_last_set = false;

  if (IsBreakInside(previous_container_break_token)) {
    // This multicol container is nested inside another fragmentation context,
    // and this isn't its first fragment. Locate the break token for the
    // previous inner column contents, so that we include the correct amount of
    // consumed block-size in the child offsets. If there's a break token for
    // column contents, we'll find it at the back.
    const auto& child_break_tokens =
        previous_container_break_token->ChildBreakTokens();
    if (!child_break_tokens.empty()) {
      const auto* token =
          To<NGBlockBreakToken>(child_break_tokens.back().Get());
      // We also create break tokens for spanners, so we need to check.
      if (token->InputNode() == *this) {
        previous_column_break_token = token;
        if (copy_back) {
          flow_thread_offset =
              previous_column_break_token->ConsumedBlockSizeForLegacy();

          // We're usually resuming layout into a column set that has already
          // been started in an earlier fragment, but in some cases the column
          // set starts exactly at the outer fragmentainer boundary (right after
          // a spanner that took up all remaining space in the earlier
          // fragment), and when this happens, we need to initialize the set
          // now.
          pending_column_set = flow_thread->PendingColumnSetForNG();

          // If we resume with column content (without being interrupted by a
          // spanner) in this multicol fragment, we need to add another
          // fragmentainer group to the column set that we're resuming.
          should_append_fragmentainer_group = true;
        }
      }
    }
  } else {
    if (copy_back) {
      // This is the first fragment generated for the multicol container (there
      // may be multiple fragments if we're nested inside another fragmentation
      // context).
      flow_thread->StartLayoutFromNG();
      pending_column_set =
          DynamicTo<LayoutMultiColumnSet>(flow_thread->FirstMultiColumnBox());
    }
  }

  for (const auto& child : physical_fragment.Children()) {
    const auto& child_fragment = To<NGPhysicalBoxFragment>(*child);
    const auto* child_box = To<LayoutBox>(child_fragment.GetLayoutObject());
    if (child_box && child_box != box_) {
      CopyChildFragmentPosition(child_fragment, child.offset,
                                physical_fragment);
      if (!copy_back) {
        continue;
      }
      if (!child_box->IsColumnSpanAll())
        continue;
      LayoutBox* placeholder = child_box->SpannerPlaceholder();
      if (!child_fragment.BreakToken()) {
        // Last fragment for this spanner. Update its placeholder.
        placeholder->SetLocation(child_box->Location());
        placeholder->SetSize(child_box->Size());
      }

      flow_thread->SkipColumnSpanner(child_box, flow_thread_offset);

      if (auto* previous_column_set = DynamicTo<LayoutMultiColumnSet>(
              placeholder->PreviousSiblingMultiColumnBox())) {
        previous_column_set->FinishLayoutFromNG();
      }

      if (pending_column_set) {
        // The legacy tree builder (the flow thread code) sometimes
        // incorrectly keeps column sets that shouldn't be there anymore. If
        // we have two column spanners, that are in fact adjacent, even though
        // there's a spurious column set between them, the column set hasn't
        // been initialized correctly (since we still have a
        // pending_column_set at this point). Say hello to the column set that
        // shouldn't exist, so that it gets some initialization.
        pending_column_set->SetIsIgnoredByNG();
      }

      LayoutBox* next_box = placeholder->NextSiblingMultiColumnBox();
      pending_column_set = DynamicTo<LayoutMultiColumnSet>(next_box);

      // If this multicol container was nested inside another fragmentation
      // context, and we're resuming at a subsequent fragment, we'll normally
      // append another fragmentainer group for column contents. But since we
      // found a spanner first, we won't do that, since we'll move to another
      // column set (if there's more column content at all).
      should_append_fragmentainer_group = false;

      // If there is no column set after the spanner, we should expand the last
      // column set (if any) to encompass any columns that were created after
      // the spanner. Only do this if we're actually past the last column set,
      // though. We may have adjacent spanner placeholders, because the legacy
      // and NG engines disagree on whether there's column content in-between
      // (NG will create column content if the parent block of a spanner has
      // trailing margin / border / padding, while legacy does not).
      should_expand_last_set = !next_box && flow_thread->LastMultiColumnSet();
      continue;
    }

    DCHECK(!child_box);

    if (copy_back) {
      LogicalSize logical_size = FragmentainerLogicalCapacity(child_fragment);

      if (has_processed_first_column_in_flow_thread) {
        // Non-uniform fragmentainer widths not supported by legacy layout.
        DCHECK_EQ(flow_thread->LogicalWidth(), logical_size.inline_size);
      } else {
        // The offset of the flow thread is the same as that of the first
        // column.
        LayoutPoint point = LayoutBoxUtils::ComputeLocation(
            child_fragment, child.offset, physical_fragment,
            previous_container_break_token);
        flow_thread->SetLocationAndUpdateOverflowControlsIfNeeded(point);
        flow_thread->SetLogicalWidth(logical_size.inline_size);
        has_processed_first_column_in_flow_thread = true;
      }

      if (pending_column_set) {
        // We're visiting this column set for the first time in this layout
        // pass. Set up what we can set up. That's everything except for the
        // block-size. Set the inline-size to that of the content-box of the
        // multicol container. The inline-offset will be the content-box edge of
        // the multicol container, and the block-offset will be the block-offset
        // of the column itself. It doesn't matter which column from the same
        // row we use, since all columns have the same block-offset and
        // block-size (so just use the first one).
        LogicalOffset logical_offset(
            border_scrollbar_padding.inline_start,
            converter.ToLogical(child.offset, child_fragment.Size())
                .block_offset);
        LogicalSize column_set_logical_size(column_row_inline_size,
                                            logical_size.block_size);
        PhysicalOffset physical_offset = converter.ToPhysical(
            logical_offset, converter.ToPhysical(column_set_logical_size));
        // We have calculated the physical offset relative to the border edge of
        // this multicol container fragment. We'll now convert it to a legacy
        // engine LayoutPoint, which will also take care of converting it into
        // the flow thread coordinate space, if we happen to be nested inside
        // another fragmentation context.
        LayoutPoint point = LayoutBoxUtils::ComputeLocation(
            child_fragment, physical_offset, physical_fragment,
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
      } else if (should_expand_last_set) {
        if (logical_size.block_size > LayoutUnit()) {
          auto* last_set = flow_thread->LastMultiColumnSet();
          last_set->LastFragmentainerGroup().ExtendColumnBlockSizeFromNG(
              logical_size.block_size);
          last_set->EndFlow(flow_thread_offset + logical_size.block_size);
          last_set->FinishLayoutFromNG();
        }
        should_expand_last_set = false;
      }

      flow_thread->SetCurrentColumnBlockSizeFromNG(logical_size.block_size);

      flow_thread_offset += logical_size.block_size;
    }

    // Each anonymous child of a multicol container constitutes one column.
    // Position each child fragment in the first column that they occur,
    // relatively to the block-start of the flow thread.
    //
    // We may fail to detect visual movement of flow thread children if the
    // child re-uses a cached result, since the LayoutBox's frame_rect_ is in
    // the flow thread coordinate space. If the column block-size or inline-size
    // has changed, we might miss paint invalidation, unless we request it to be
    // checked explicitly. We only need to do this for direct flow thread
    // children, since movement detection works fine for descendants. If it's
    // not detected during layout (due to cache hits), it will be detected
    // during pre-paint.
    //
    // TODO(mstensho): Get rid of this in the future if we become able to
    // compare visual offsets rather than flow thread offsets.
    PlaceChildrenInLayoutBox(child_fragment, previous_column_break_token,
                             /* needs_invalidation_check */ true);

    // If the multicol container has inline children, there may still be floats
    // there, but they aren't stored as child fragments of |column| in that case
    // (but rather inside fragment items). Make sure that they get positioned,
    // too.
    if (const NGFragmentItems* items = child_fragment.Items()) {
      CopyFragmentItemsToLayoutBox(child_fragment, *items,
                                   previous_column_break_token);
    }

    previous_column_break_token = child_fragment.BreakToken();
  }

  if (!physical_fragment.BreakToken())
    flow_thread->FinishLayoutFromNG(flow_thread_offset);
}

// Copies data back to the legacy layout tree for a given child fragment.
void NGBlockNode::CopyChildFragmentPosition(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const NGPhysicalBoxFragment& container_fragment,
    const NGBlockBreakToken* previous_container_break_token,
    bool needs_invalidation_check) const {
  auto* layout_box = To<LayoutBox>(child_fragment.GetMutableLayoutObject());
  if (!layout_box)
    return;

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  LayoutPoint point = LayoutBoxUtils::ComputeLocation(
      child_fragment, offset, container_fragment,
      previous_container_break_token);
  layout_box->SetLocationAndUpdateOverflowControlsIfNeeded(point);

  if (needs_invalidation_check)
    layout_box->SetShouldCheckForPaintInvalidation();
}

void NGBlockNode::MakeRoomForExtraColumns(LayoutUnit block_size) const {
  auto* block_flow = DynamicTo<LayoutBlockFlow>(GetLayoutBox());
  DCHECK(block_flow && block_flow->MultiColumnFlowThread());
  MultiColumnFragmentainerGroup& last_group =
      block_flow->MultiColumnFlowThread()
          ->LastMultiColumnSet()
          ->LastFragmentainerGroup();
  last_group.ExtendLogicalBottomInFlowThread(block_size);
}

void NGBlockNode::CopyFragmentItemsToLayoutBox(
    const NGPhysicalBoxFragment& container,
    const NGFragmentItems& items,
    const NGBlockBreakToken* previous_break_token) const {
  LayoutUnit previously_consumed_block_size;
  if (previous_break_token) {
    previously_consumed_block_size =
        previous_break_token->ConsumedBlockSizeForLegacy();
  }
  bool initial_container_is_flipped = Style().IsFlippedBlocksWritingMode();
  for (NGInlineCursor cursor(container, items); cursor; cursor.MoveToNext()) {
    if (const NGPhysicalBoxFragment* child = cursor.Current().BoxFragment()) {
      // Replaced elements and inline blocks need Location() set relative to
      // their block container. Similarly for block-in-inline anonymous wrapper
      // blocks, but those may actually fragment, so we need to make sure that
      // we only do this when at the first fragment.
      if (!child->IsFirstForNode())
        continue;

      LayoutObject* layout_object = child->GetMutableLayoutObject();
      if (!layout_object)
        continue;
      if (auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
        PhysicalOffset maybe_flipped_offset =
            cursor.Current().OffsetInContainerFragment();
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
#if DCHECK_IS_ON()
        layout_box->InvalidateVisualOverflow();
#endif
        continue;
      }

      // Legacy compatibility. This flag is used in paint layer for
      // invalidation.
      if (auto* layout_inline = DynamicTo<LayoutInline>(layout_object)) {
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
  if (const auto* block = DynamicTo<LayoutBlockFlow>(box_.Get())) {
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

bool NGBlockNode::IsInTopOrViewTransitionLayer() const {
  return GetLayoutBox()->IsInTopOrViewTransitionLayer();
}

bool NGBlockNode::HasAspectRatio() const {
  if (!Style().AspectRatio().IsAuto()) {
    DCHECK(!GetAspectRatio().IsEmpty());
    return true;
  }
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
  if (ratio.GetType() == EAspectRatioType::kRatio ||
      (ratio.GetType() == EAspectRatioType::kAutoAndRatio && !IsReplaced()))
    return Style().LogicalAspectRatio();

  if (!ShouldApplySizeContainment()) {
    IntrinsicSizingInfo legacy_sizing_info;
    To<LayoutReplaced>(box_.Get())
        ->ComputeIntrinsicSizingInfo(legacy_sizing_info);
    if (!legacy_sizing_info.aspect_ratio.IsEmpty()) {
      PhysicalSize layout_ratio = StyleAspectRatio::LayoutRatioFromSizeF(
          legacy_sizing_info.aspect_ratio);
      return {layout_ratio.width, layout_ratio.height};
    }
  }
  if (ratio.GetType() == EAspectRatioType::kAutoAndRatio)
    return Style().LogicalAspectRatio();
  return LogicalSize();
}

LogicalSize NGBlockNode::GetReplacedSizeOverrideIfAny(
    const NGConstraintSpace& space) const {
  DCHECK(IsReplaced());
  if (!box_->IsSVGRoot())
    return LogicalSize();
  const LayoutSVGRoot& svg_root = To<LayoutSVGRoot>(*box_);
  LayoutSize size_override = svg_root.GetContainerSize();
  if (!size_override.IsEmpty()) {
    return PhysicalSize(size_override)
        .ConvertToLogical(Style().GetWritingMode());
  }
  if (svg_root.IsEmbeddedThroughFrameContainingSVGDocument())
    return space.AvailableSize();
  return LogicalSize();
}

absl::optional<gfx::Transform> NGBlockNode::GetTransformForChildFragment(
    const NGPhysicalBoxFragment& child_fragment,
    PhysicalSize size) const {
  const auto* child_layout_object = child_fragment.GetLayoutObject();
  DCHECK(child_layout_object);

  if (!child_layout_object->ShouldUseTransformFromContainer(box_))
    return absl::nullopt;

  gfx::Transform transform;
  child_layout_object->GetTransformFromContainer(box_, PhysicalOffset(),
                                                 transform, &size);

  return transform;
}

bool NGBlockNode::HasNonVisibleBlockOverflow() const {
  OverflowClipAxes clip_axes = GetOverflowClipAxes();
  if (Style().IsHorizontalWritingMode())
    return clip_axes & kOverflowClipY;
  return clip_axes & kOverflowClipX;
}

bool NGBlockNode::IsCustomLayoutLoaded() const {
  DCHECK(box_->IsLayoutNGCustom());
  return To<LayoutNGCustom>(box_.Get())->IsLoaded();
}

MathScriptType NGBlockNode::ScriptType() const {
  DCHECK(IsA<MathMLScriptsElement>(GetDOMNode()));
  return To<MathMLScriptsElement>(GetDOMNode())->GetScriptType();
}

bool NGBlockNode::HasIndex() const {
  DCHECK(IsA<MathMLRadicalElement>(GetDOMNode()));
  return To<MathMLRadicalElement>(GetDOMNode())->HasIndex();
}

const NGLayoutResult* NGBlockNode::LayoutAtomicInline(
    const NGConstraintSpace& parent_constraint_space,
    const ComputedStyle& parent_style,
    bool use_first_line_style,
    NGBaselineAlgorithmType baseline_algorithm_type) {
  NGConstraintSpaceBuilder builder(parent_constraint_space,
                                   Style().GetWritingDirection(),
                                   /* is_new_fc */ true);
  SetOrthogonalFallbackInlineSizeIfNeeded(parent_style, *this, &builder);

  builder.SetIsPaintedAtomically(true);
  builder.SetUseFirstLineStyle(use_first_line_style);

  builder.SetBaselineAlgorithmType(baseline_algorithm_type);

  builder.SetAvailableSize(parent_constraint_space.AvailableSize());
  builder.SetPercentageResolutionSize(
      parent_constraint_space.PercentageResolutionSize());
  builder.SetReplacedPercentageResolutionSize(
      parent_constraint_space.ReplacedPercentageResolutionSize());
  NGConstraintSpace constraint_space = builder.ToConstraintSpace();
  const NGLayoutResult* result = Layout(constraint_space);
  if (!NGDisableSideEffectsScope::IsDisabled()) {
    // TODO(kojii): Investigate why ClearNeedsLayout() isn't called
    // automatically when it's being laid out.
    GetLayoutBox()->ClearNeedsLayout();
  }
  return result;
}

const NGLayoutResult* NGBlockNode::RunLegacyLayout(
    const NGConstraintSpace& constraint_space) const {
  // This is an exit-point from LayoutNG to the legacy engine. This means that
  // we need to be at a formatting context boundary, since NG and legacy don't
  // cooperate on e.g. margin collapsing.
  DCHECK(!box_->IsLayoutBlock() ||
         To<LayoutBlock>(box_.Get())->CreatesNewFormattingContext());

  // We cannot enter legacy layout for something fragmentable if we're inside an
  // NG block fragmentation context. LayoutNG and legacy block fragmentation
  // cannot cooperate within the same fragmentation context.
  DCHECK(!constraint_space.HasBlockFragmentation() ||
         box_->GetNGPaginationBreakability() == LayoutBox::kForbidBreaks);

  const NGLayoutResult* old_layout_result = box_->GetSingleCachedLayoutResult();
  const NGLayoutResult* old_measure_result = box_->GetCachedMeasureResult();

  const NGLayoutResult* layout_result =
      constraint_space.CacheSlot() == NGCacheSlot::kMeasure ? old_measure_result
                                                            : old_layout_result;
  if (constraint_space.CacheSlot() == NGCacheSlot::kLayout && !layout_result)
    layout_result = old_measure_result;

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
    DCHECK(!box_->IsLayoutNGObject());
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
    NGBoxFragmentBuilder builder(*this, box_->Style(), constraint_space,
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

    // When side effects are disabled, it's not possible to disable side effects
    // completely for legacy, but at least keep the fragment tree unaffected.
    if (!NGDisableSideEffectsScope::IsDisabled()) {
      // Legacy layout clears both layout and measure results, in
      // LayoutBox::UpdateAfterLayout(), because that code has no way of knowing
      // whether the legacy object is laid out by an NG container or not. We
      // will now store the new layout result, either the measure result or the
      // actual layout result, depending on the cache slot selected. Make sure
      // that we leave the *other* result untouched, by first canceling what
      // UpdateAfterLayout() did.
      box_->RestoreLegacyLayoutResults(old_measure_result, old_layout_result);

      box_->SetCachedLayoutResult(layout_result, 0);

      // If |SetCachedLayoutResult| did not update cached |LayoutResult|,
      // |NeedsLayout()| flag should not be cleared.
      if (needed_layout) {
        if (layout_result != box_->GetSingleCachedLayoutResult()) {
          // TODO(kojii): If we failed to update CachedLayoutResult for other
          // reasons, we'd like to review it.
          NOTREACHED();
          box_->SetNeedsLayout(layout_invalidation_reason::kUnknown);
        }
      }
    }
  } else if (layout_result && !NGDisableSideEffectsScope::IsDisabled()) {
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
      layout_result = MakeGarbageCollected<NGLayoutResult>(
          *layout_result, constraint_space, layout_result->EndMarginStrut(),
          layout_result->BfcLineOffset(), layout_result->BfcBlockOffset(),
          LayoutUnit() /* block_offset_delta */);
      box_->SetCachedLayoutResult(layout_result, 0);
    }
  }

  UpdateShapeOutsideInfoIfNeeded(
      *layout_result, constraint_space.PercentageResolutionInlineSize());

  return layout_result;
}

const NGLayoutResult* NGBlockNode::RunSimplifiedLayout(
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
  switch (constraint_space.BaselineAlgorithmType()) {
    case NGBaselineAlgorithmType::kDefault: {
      LayoutUnit position = box_->FirstLineBoxBaseline();
      if (position != -1)
        builder->SetFirstBaseline(position);
      break;
    }
    case NGBaselineAlgorithmType::kInlineBlock: {
      LayoutUnit position =
          AtomicInlineBaselineFromLegacyLayout(constraint_space);
      if (position != -1)
        builder->SetFirstBaseline(position);
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

void NGBlockNode::UpdateMarginPaddingInfoIfNeeded(
    const NGConstraintSpace& space) const {
  // Table-cells don't have margins, and aren't grid-items.
  if (space.IsTableCell())
    return;

  if (Style().MayHaveMargin())
    box_->SetMargin(ComputePhysicalMargins(space, Style()));

  if (Style().MayHaveMargin() || Style().MayHavePadding()) {
    // Copy back the %-size so that |LayoutBoxModelObject::ComputedCSSPadding|
    // is able to return the correct value. This isn't ideal, but eventually
    // we'll answer these queries from the fragment.
    const auto* containing_block = box_->ContainingBlock();
    if (UNLIKELY(containing_block && containing_block->IsLayoutNGGrid())) {
      box_->SetOverrideContainingBlockContentLogicalWidth(
          space.PercentageResolutionInlineSizeForParentWritingMode());
    }
  }
}

// Floats can optionally have a shape area, specified by "shape-outside". The
// current shape machinery requires setting the size of the float after layout
// in the parents writing mode.
void NGBlockNode::UpdateShapeOutsideInfoIfNeeded(
    const NGLayoutResult& layout_result,
    LayoutUnit percentage_resolution_inline_size) const {
  if (!box_->IsFloating() || !box_->GetShapeOutsideInfo())
    return;

  if (layout_result.Status() != NGLayoutResult::kSuccess)
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

void NGBlockNode::InsertIntoLegacyPositionedObjectsOf(
    LayoutBlock* containing_block) const {
  DCHECK(box_->IsOutOfFlowPositioned());
  DCHECK(containing_block);
  DCHECK_EQ(containing_block, box_->ContainingBlock());
  containing_block->InsertPositionedObject(box_);
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

void NGBlockNode::StoreColumnSizeAndCount(LayoutUnit inline_size, int count) {
  LayoutMultiColumnFlowThread* flow_thread =
      To<LayoutBlockFlow>(box_.Get())->MultiColumnFlowThread();
  // We have no chance to unregister the inline size for the
  // LayoutMultiColumnFlowThread.
  TextAutosizer::MaybeRegisterInlineSize(*flow_thread, inline_size);

  flow_thread->SetColumnCountFromNG(count);
}

static bool g_devtools_layout = false;
bool DevtoolsReadonlyLayoutScope::InDevtoolsLayout() {
  return g_devtools_layout;
}

DevtoolsReadonlyLayoutScope::DevtoolsReadonlyLayoutScope() {
  DCHECK(!g_devtools_layout);
  g_devtools_layout = true;
}

DevtoolsReadonlyLayoutScope::~DevtoolsReadonlyLayoutScope() {
  DCHECK(g_devtools_layout);
  g_devtools_layout = false;
}

}  // namespace blink
