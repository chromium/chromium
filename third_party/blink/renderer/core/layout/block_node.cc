// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_node.h"

#include <memory>

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_button_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_marquee_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/block_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/flex/flex_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/forms/fieldset_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/forms/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/fragment_repeater.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/frame_set_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/geometry/fragment_geometry.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_input_node.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_set.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_utils.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"
#include "third_party/blink/renderer/core/layout/list/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/logical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/masonry/masonry_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_fraction_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_layout_utils.h"
#include "third_party/blink/renderer/core/layout/mathml/math_operator_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_padded_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_radical_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_row_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_scripts_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_space_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_token_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/mathml/math_under_over_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/paginated_root_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/replaced_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/shapes/shape_outside_info.h"
#include "third_party/blink/renderer/core/layout/simplified_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/space_utils.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/table/table_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/table/table_row_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/table/table_section_layout_algorithm.h"
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
#include "third_party/blink/renderer/core/paint/transform_utils.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

using mojom::blink::FormControlType;

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
NOINLINE void CreateAlgorithmAndRun(const LayoutAlgorithmParams& params,
                                    const Callback& callback) {
  Algorithm algorithm(params);
  callback(&algorithm);
}

template <typename Callback>
NOINLINE void DetermineMathMLAlgorithmAndRun(
    const LayoutBox& box,
    const LayoutAlgorithmParams& params,
    const Callback& callback) {
  DCHECK(box.IsMathML());
  // Currently math layout algorithms can only apply to MathML elements.
  auto* element = box.GetNode();
  if (element) {
    if (IsA<MathMLSpaceElement>(element)) {
      CreateAlgorithmAndRun<MathSpaceLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLFractionElement>(element) &&
               IsValidMathMLFraction(params.node)) {
      CreateAlgorithmAndRun<MathFractionLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLRadicalElement>(element) &&
               IsValidMathMLRadical(params.node)) {
      CreateAlgorithmAndRun<MathRadicalLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLPaddedElement>(element)) {
      CreateAlgorithmAndRun<MathPaddedLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLTokenElement>(element)) {
      if (IsOperatorWithSpecialShaping(params.node))
        CreateAlgorithmAndRun<MathOperatorLayoutAlgorithm>(params, callback);
      else if (IsTextOnlyToken(params.node))
        CreateAlgorithmAndRun<MathTokenLayoutAlgorithm>(params, callback);
      else
        CreateAlgorithmAndRun<BlockLayoutAlgorithm>(params, callback);
      return;
    } else if (IsA<MathMLScriptsElement>(element) &&
               IsValidMathMLScript(params.node)) {
      if (IsA<MathMLUnderOverElement>(element) &&
          !IsUnderOverLaidOutAsSubSup(params.node)) {
        CreateAlgorithmAndRun<MathUnderOverLayoutAlgorithm>(params, callback);
      } else {
        CreateAlgorithmAndRun<MathScriptsLayoutAlgorithm>(params, callback);
      }
      return;
    }
  }
  CreateAlgorithmAndRun<MathRowLayoutAlgorithm>(params, callback);
}

template <typename Callback>
NOINLINE void DetermineAlgorithmAndRun(const LayoutAlgorithmParams& params,
                                       const Callback& callback) {
  const ComputedStyle& style = params.node.Style();
  const LayoutBox& box = *params.node.GetLayoutBox();
  if (box.IsFlexibleBox()) {
    CreateAlgorithmAndRun<FlexLayoutAlgorithm>(params, callback);
  } else if (box.IsTable()) {
    CreateAlgorithmAndRun<TableLayoutAlgorithm>(params, callback);
  } else if (box.IsTableRow()) {
    CreateAlgorithmAndRun<TableRowLayoutAlgorithm>(params, callback);
  } else if (box.IsTableSection()) {
    CreateAlgorithmAndRun<TableSectionLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutCustom()) {
    CreateAlgorithmAndRun<CustomLayoutAlgorithm>(params, callback);
  } else if (box.IsMathML()) {
    DetermineMathMLAlgorithmAndRun(box, params, callback);
  } else if (box.IsLayoutGrid()) {
    CreateAlgorithmAndRun<GridLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutMasonry()) {
    CreateAlgorithmAndRun<MasonryLayoutAlgorithm>(params, callback);
  } else if (box.IsLayoutReplaced()) {
    CreateAlgorithmAndRun<ReplacedLayoutAlgorithm>(params, callback);
  } else if (box.IsFieldset()) {
    CreateAlgorithmAndRun<FieldsetLayoutAlgorithm>(params, callback);
  } else if (box.IsFrameSet()) {
    CreateAlgorithmAndRun<FrameSetLayoutAlgorithm>(params, callback);
  }
  // If there's a legacy layout box, we can only do block fragmentation if
  // we would have done block fragmentation with the legacy engine.
  // Otherwise writing data back into the legacy tree will fail. Look for
  // the flow thread.
  else if (GetFlowThread(box) && style.SpecifiesColumns()) {
    CreateAlgorithmAndRun<ColumnLayoutAlgorithm>(params, callback);
  } else if (!box.Parent() && params.node.IsPaginatedRoot()) [[unlikely]] {
    CreateAlgorithmAndRun<PaginatedRootLayoutAlgorithm>(params, callback);
  } else {
    CreateAlgorithmAndRun<BlockLayoutAlgorithm>(params, callback);
  }
}

inline const LayoutResult* LayoutWithAlgorithm(
    const LayoutAlgorithmParams& params) {
  const LayoutResult* result = nullptr;
  DetermineAlgorithmAndRun(params,
                           [&result]<typename Algorithm>(Algorithm* algorithm) {
                             result = algorithm->Layout();
                           });
  return result;
}

inline MinMaxSizesResult ComputeMinMaxSizesWithAlgorithm(
    const LayoutAlgorithmParams& params,
    const MinMaxSizesFloatInput& float_input) {
  MinMaxSizesResult result;
  DetermineAlgorithmAndRun(params, [&result, &float_input]<typename Algorithm>(
                                       Algorithm* algorithm) {
    result = algorithm->ComputeMinMaxSizes(float_input);
  });
  return result;
}

bool CanUseCachedIntrinsicInlineSizes(const ConstraintSpace& constraint_space,
                                      const MinMaxSizesFloatInput& float_input,
                                      const BlockNode& node) {
  // Obviously can't use the cache if our intrinsic logical widths are dirty.
  if (node.GetLayoutBox()->IntrinsicLogicalWidthsDirty())
    return false;

  // We don't store the float inline sizes for comparison, always skip the
  // cache in this case.
  if (float_input.float_left_inline_size || float_input.float_right_inline_size)
    return false;

  // Check if we have any percentage padding.
  const auto& style = node.Style();
  if (style.MayHavePadding() &&
      (style.PaddingTop().HasPercent() || style.PaddingRight().HasPercent() ||
       style.PaddingBottom().HasPercent() ||
       style.PaddingLeft().HasPercent())) {
    return false;
  }

  if (node.IsTableCell() && To<LayoutTableCell>(node.GetLayoutBox())
                                    ->IntrinsicLogicalWidthsBorderSizes() !=
                                constraint_space.TableCellBorders()) {
    return false;
  }

  // We may have something like:
  // "grid-template-columns: repeat(auto-fill, 50px); min-width: 50%;"
  // In this specific case our min/max sizes are now dependent on what
  // "min-width" resolves to - which is unique to grid.
  if (node.IsGrid()) {
    if (style.LogicalMinWidth().HasPercentOrStretch() ||
        style.LogicalMaxWidth().HasPercentOrStretch()) {
      return false;
    }
    // Also consider transferred min/max sizes.
    if (node.HasAspectRatio() &&
        (style.LogicalMinHeight().HasPercentOrStretch() ||
         style.LogicalMaxHeight().HasPercentOrStretch())) {
      return false;
    }
  }

  return true;
}

std::optional<LayoutUnit> ContentMinimumInlineSize(
    const BlockNode& block_node,
    const BoxStrut& border_padding) {
  // Table layout is never allowed to go below the min-intrinsic size.
  if (block_node.IsTable())
    return std::nullopt;

  const auto* node = block_node.GetDOMNode();
  const auto* marquee_element = DynamicTo<HTMLMarqueeElement>(node);
  if (marquee_element && marquee_element->IsHorizontal())
    return border_padding.InlineSum();

  const auto& style = block_node.Style();
  const auto& main_inline_size = style.LogicalWidth();

  if (!main_inline_size.HasPercent()) {
    return std::nullopt;
  }

  // Manually resolve the main-length against zero. calc() expressions may
  // resolve to something greater than "zero".
  LayoutUnit inline_size =
      MinimumValueForLength(main_inline_size, LayoutUnit());
  if (style.BoxSizing() == EBoxSizing::kBorderBox)
    inline_size = std::max(border_padding.InlineSum(), inline_size);
  else
    inline_size += border_padding.InlineSum();

  const bool apply_form_sizing = style.ApplyControlFixedSize(node);
  if (block_node.IsTextControl() && apply_form_sizing) {
    return inline_size;
  }
  if (IsA<HTMLSelectElement>(node) && apply_form_sizing) {
    return inline_size;
  }
  if (const auto* input_element = DynamicTo<HTMLInputElement>(node)) {
    FormControlType type = input_element->FormControlType();
    if (type == FormControlType::kInputFile && apply_form_sizing) {
      return inline_size;
    }
    if (type == FormControlType::kInputRange) {
      return inline_size;
    }
  }
  return std::nullopt;
}

// Look for scroll markers inside `parent`, and attach them.
void AttachScrollMarkers(LayoutObject& parent,
                         Node::AttachContext& context,
                         bool has_absolute_containment = false,
                         bool has_fixed_containment = false) {
  if (parent.CanContainAbsolutePositionObjects()) {
    has_absolute_containment = true;
    if (parent.CanContainFixedPositionObjects()) {
      has_fixed_containment = true;
    }
  }

  for (LayoutObject* child = parent.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if ((child->IsFixedPositioned() && !has_fixed_containment) ||
        (child->IsAbsolutePositioned() && !has_absolute_containment)) {
      continue;
    }
    if (auto* element = DynamicTo<Element>(child->GetNode())) {
      if (PseudoElement* marker =
              element->GetPseudoElement(kPseudoIdScrollMarker)) {
        marker->AttachLayoutTree(context);
      }
    }
    // Descend into the subtree of the child unless it is a scroll marker group,
    // or establishes one.
    //
    // TODO(layout-dev): Need to enter nested scrollable containers if an outer
    // scrollable container has "stronger" containment than the inner one. E.g.
    // if the outer one is position:relative, and the inner one has a scroll
    // marker in an absolutely positioned subtree, the marker belongs in the
    // outermost scroll marker group.
    if (!child->IsScrollMarkerGroup() && !child->GetScrollMarkerGroup()) {
      AttachScrollMarkers(*child, context, has_absolute_containment,
                          has_fixed_containment);
    }
  }

  const LayoutBox* parent_box = DynamicTo<LayoutBox>(&parent);
  // If this is a multicol container, look for ::column::scroll-marker pseudo
  // elements, and attach them.
  if (parent_box && parent_box->IsFragmentationContextRoot()) {
    if (const ColumnPseudoElementsVector* column_pseudos =
            To<Element>(parent.EnclosingNode())->GetColumnPseudoElements()) {
      for (const auto& column_pseudo : *column_pseudos) {
        if (PseudoElement* scroll_marker =
                column_pseudo->GetPseudoElement(kPseudoIdScrollMarker)) {
          scroll_marker->AttachLayoutTree(context);
        }
      }
    }
  }
}

}  // namespace

const LayoutResult* BlockNode::Layout(
    const ConstraintSpace& constraint_space,
    const BlockBreakToken* break_token,
    const EarlyBreak* early_break,
    const ColumnSpannerPath* column_spanner_path) const {
  // The exclusion space internally is a pointer to a shared vector, and
  // equality of exclusion spaces is performed using pointer comparison on this
  // internal shared vector.
  // In order for the caching logic to work correctly we need to set the
  // pointer to the value previous shared vector.
  if (const LayoutResult* previous_result =
          box_->GetCachedLayoutResult(break_token)) {
    constraint_space.GetExclusionSpace().PreInitialize(
        previous_result->GetConstraintSpaceForCaching().GetExclusionSpace());
  }

  LayoutCacheStatus cache_status;

  // We may be able to hit the cache without calculating fragment geometry
  // (calculating that isn't necessarily very cheap). So, start off without it.
  std::optional<FragmentGeometry> fragment_geometry;

  // CachedLayoutResult() might clear flags, so remember the need for layout
  // before attempting to hit the cache.
  bool needed_layout = box_->NeedsLayout();
  if (needed_layout)
    box_->GetFrameView()->IncBlockLayoutCount();

  const LayoutResult* layout_result = box_->CachedLayoutResult(
      constraint_space, break_token, early_break, column_spanner_path,
      &fragment_geometry, &cache_status);

  if ((cache_status == LayoutCacheStatus::kHit ||
       cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout) &&
      needed_layout &&
      constraint_space.CacheSlot() == LayoutResultCacheSlot::kLayout &&
      box_->HasBrokenSpine() && !ChildLayoutBlockedByDisplayLock()) {
    // If we're not guaranteed to discard the old fragment (which we're only
    // guaranteed to do if we have decided to perform full layout), we need to
    // clone the result to pick the most recent fragments from the LayoutBox
    // children, because we stopped rebuilding the fragment spine right here
    // after performing subtree layout.
    layout_result = LayoutResult::CloneWithPostLayoutFragments(*layout_result);
    const auto& new_fragment =
        To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());
    // If we have fragment items, and we're not done (more fragments to follow),
    // be sure to miss the cache for any subsequent fragments, lest finalization
    // be missed (which could cause trouble for InlineCursor when walking the
    // items).
    bool clear_trailing_results =
        new_fragment.GetBreakToken() && new_fragment.HasItems();
    StoreResultInLayoutBox(layout_result, break_token, clear_trailing_results);
    box_->ClearHasBrokenSpine();
  }

  if (cache_status == LayoutCacheStatus::kHit) {
    DCHECK(layout_result);

    // We may have to update the margins on box_; we reuse the layout result
    // even if a percentage margin may have changed.
    UpdateMarginPaddingInfoIfNeeded(constraint_space,
                                    layout_result->GetPhysicalFragment());

    UpdateShapeOutsideInfoIfNeeded(*layout_result, constraint_space);

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

  // Only consider the size of the first container fragment.
  if (!IsBreakInside(break_token) && CanMatchSizeContainerQueries()) {
    if (auto* element = DynamicTo<Element>(GetDOMNode())) {
      // Consider scrollbars if they are stable (reset any auto scrollbars).
      BoxStrut scrollbar = fragment_geometry->scrollbar;
      {
        const auto& style = Style();
        if (style.IsScrollbarGutterAuto() &&
            style.OverflowBlockDirection() == EOverflow::kAuto) {
          scrollbar.inline_start = LayoutUnit();
          scrollbar.inline_end = LayoutUnit();
        }
        if (style.OverflowInlineDirection() == EOverflow::kAuto) {
          scrollbar.block_start = LayoutUnit();
          scrollbar.block_end = LayoutUnit();
        }
      }

      const LogicalSize available_size = CalculateChildAvailableSize(
          constraint_space, *this, fragment_geometry->border_box_size,
          fragment_geometry->border + scrollbar + fragment_geometry->padding);
      GetDocument().GetStyleEngine().UpdateStyleAndLayoutTreeForContainer(
          *element, available_size, ContainedAxes());

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

  LayoutAlgorithmParams params(*this, *fragment_geometry, constraint_space,
                               break_token, early_break);
  params.column_spanner_path = column_spanner_path;

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_.Get());

  // Try to perform "simplified" layout, unless it's a fragmentation context
  // root (the simplified layout algorithm doesn't support fragmentainers).
  if (cache_status == LayoutCacheStatus::kNeedsSimplifiedLayout &&
      (!block_flow || !block_flow->IsFragmentationContextRoot())) {
    DCHECK(layout_result);
#if DCHECK_IS_ON()
    const LayoutResult* previous_result = layout_result;
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
  } else if (cache_status == LayoutCacheStatus::kCanReuseLines) {
    params.previous_result = layout_result;
    layout_result = nullptr;
  } else {
    layout_result = nullptr;
  }

  // All these variables may change after layout due to scrollbars changing.
  BoxStrut scrollbars_before = ComputeScrollbars(constraint_space, *this);
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

  std::optional<PhysicalSize> optional_old_box_size;
  if (layout_result->Status() == LayoutResult::kSuccess &&
      !layout_result->GetPhysicalFragment().GetBreakToken()) {
    optional_old_box_size = box_->Size();
  }

  FinishLayout(block_flow, constraint_space, break_token, layout_result,
               optional_old_box_size);

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
  // Skip this part if side-effects aren't allowed, though. Also skip it if we
  // are resuming layout after a fragmentainer break. Changing the intrinsic
  // inline-size halfway through layout of a node doesn't make sense.
  BoxStrut scrollbars_after = ComputeScrollbars(constraint_space, *this);
  if ((scrollbars_before != scrollbars_after ||
       inline_size_before != fragment_geometry->border_box_size.inline_size) &&
      !DisableLayoutSideEffectsScope::IsDisabled() &&
      !IsBreakInside(break_token)) {
    bool freeze_horizontal = false, freeze_vertical = false;
    // If we're in a measure pass, freeze both scrollbars right away, to avoid
    // quadratic time complexity for deeply nested flexboxes.
    if (constraint_space.CacheSlot() == LayoutResultCacheSlot::kMeasure) {
      freeze_horizontal = freeze_vertical = true;
    }
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
      PhysicalSize old_box_size = box_->Size();
      params.previous_result = nullptr;
      box_->SetShouldSkipLayoutCache(true);

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

      if (auto* view = DynamicTo<LayoutView>(GetLayoutBox())) {
        view->InvalidateSvgRootsWithRelativeLengthDescendents();
      }
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
  // to the LayoutResult, removing this "side" data-structure.
  UpdateShapeOutsideInfoIfNeeded(*layout_result, constraint_space);

  return layout_result;
}

const LayoutResult* BlockNode::SimplifiedLayout(
    const PhysicalFragment& previous_fragment) const {
  const LayoutResult* previous_result = box_->GetSingleCachedLayoutResult();
  DCHECK(previous_result);

  // We might be be trying to perform simplfied layout on a fragment in the
  // "measure" cache slot, abort if this is the case.
  if (&previous_result->GetPhysicalFragment() != &previous_fragment) {
    return nullptr;
  }

  if (!box_->NeedsLayout())
    return previous_result;

  DCHECK(box_->NeedsSimplifiedLayoutOnly() ||
         box_->ChildLayoutBlockedByDisplayLock());

  // Perform layout on ourselves using the previous constraint space.
  const ConstraintSpace space(previous_result->GetConstraintSpaceForCaching());
  const LayoutResult* result = Layout(space, /* break_token */ nullptr);

  if (result->Status() != LayoutResult::kSuccess) {
    // TODO(crbug.com/1297864): The optimistic BFC block-offsets aren't being
    // set correctly for block-in-inline causing these layouts to fail.
    return nullptr;
  }

  const auto& old_fragment =
      To<PhysicalBoxFragment>(previous_result->GetPhysicalFragment());
  const auto& new_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());

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

const LayoutResult* BlockNode::LayoutRepeatableRoot(
    const ConstraintSpace& constraint_space,
    const BlockBreakToken* break_token) const {
  // We read and write the physical fragments vector in LayoutBox here, which
  // isn't allowed if side-effects are disabled. Call-sites must make sure that
  // we don't attempt to repeat content if side-effects are disabled.
  DCHECK(!DisableLayoutSideEffectsScope::IsDisabled());

  // When laying out repeatable content, we cannot at the same time allow it to
  // break inside.
  DCHECK(!constraint_space.HasBlockFragmentation());

  // We can't both resume and repeat!
  DCHECK(!IsBreakInside(break_token));

  bool is_first = !break_token || !break_token->IsRepeated();
  const LayoutResult* result;
  if (is_first) {
    // We're generating the first fragment for repeated content. Perform regular
    // layout.
    result = Layout(constraint_space, break_token);
    DCHECK(!result->GetPhysicalFragment().GetBreakToken());
  } else {
    // We're repeating. Create a shallow clone of the first result. Once we're
    // at the last fragment, we'll actually create a deep clone.
    result = LayoutResult::Clone(*box_->GetLayoutResult(0));
  }

  wtf_size_t index = FragmentIndex(break_token);
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  // We need to create a special "repeat" break token, which will be the
  // incoming break token when generating the next fragment. This is needed in
  // order to get the sequence numbers right, which is important when adding the
  // result to the LayoutBox, and it's also needed by pre-paint / paint.
  const BlockBreakToken* outgoing_break_token =
      BlockBreakToken::CreateRepeated(*this, index);
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

void BlockNode::FinishRepeatableRoot() const {
  DCHECK(!DisableLayoutSideEffectsScope::IsDisabled());

  // This is the last fragment. It won't be repeated again. We have already
  // created fragments for the repeated nodes, but the cloning was shallow.
  // We're now ready to deep-clone the entire subtree for each repeated
  // fragment, and update the layout result vector in the LayoutBox, including
  // setting correct break tokens with sequence numbers.

  // First remove the outgoing break token from the last fragment, that was set
  // in LayoutRepeatableRoot().
  const PhysicalBoxFragment& last_fragment = box_->PhysicalFragments().back();
  auto mutator = last_fragment.GetMutableForCloning();
  mutator.SetBreakToken(nullptr);

  box_->FinalizeLayoutResults();

  wtf_size_t fragment_count = box_->PhysicalFragmentCount();
  DCHECK_GE(fragment_count, 1u);
  box_->ClearNeedsLayout();
  for (wtf_size_t i = 1; i < fragment_count; i++) {
    const PhysicalBoxFragment& physical_fragment =
        *box_->GetPhysicalFragment(i);
    bool is_first = i == 1;
    bool is_last = i + 1 == fragment_count;
    FragmentRepeater repeater(is_first, is_last);
    repeater.CloneChildFragments(physical_fragment);
  }
}

void BlockNode::PrepareForLayout() const {
  auto* block = DynamicTo<LayoutBlock>(box_.Get());
  if (block && block->IsScrollContainer()) {
    DCHECK(block->GetScrollableArea());
    if (block->GetScrollableArea()->ShouldPerformScrollAnchoring())
      block->GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();
  }

  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    To<LayoutListItem>(box_.Get())->UpdateMarkerTextIfNeeded();
}

void BlockNode::FinishLayout(
    LayoutBlockFlow* block_flow,
    const ConstraintSpace& constraint_space,
    const BlockBreakToken* break_token,
    const LayoutResult* layout_result,
    const std::optional<PhysicalSize>& old_box_size) const {
  // Computing MinMax after layout. Do not modify the |LayoutObject| tree, paint
  // properties, and other global states.
  if (DisableLayoutSideEffectsScope::IsDisabled()) {
    box_->AddMeasureLayoutResult(layout_result);
    return;
  }

  if (layout_result->Status() != LayoutResult::kSuccess) {
    // Layout aborted, but there may be results from a previous layout lying
    // around. They are fine to keep, but since we aborted, it means that we
    // want to attempt layout again. Be sure to miss the cache.
    box_->SetShouldSkipLayoutCache(true);
    return;
  }

  const auto& physical_fragment =
      To<PhysicalBoxFragment>(layout_result->GetPhysicalFragment());

  if (auto* svg_root = DynamicTo<LayoutSVGRoot>(GetLayoutBox())) {
    // Calculate the new content rect for SVG roots.
    PhysicalRect content_rect = physical_fragment.LocalRect();
    content_rect.Contract(physical_fragment.Borders() +
                          physical_fragment.Padding());

    if (!svg_root->NeedsLayout()) {
      svg_root->SetNeedsLayout(layout_invalidation_reason::kSizeChanged,
                               kMarkOnlyThis);
    }
    svg_root->LayoutRoot(content_rect);
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
    const FragmentItems* items = physical_fragment.Items();
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
      // We still need to clear |InlineNodeData| in case it had inline
      // children.
      block_flow->ClearInlineNodeData();
    }
  } else {
    DCHECK(!physical_fragment.HasItems());
  }

  if (!layout_result->GetPhysicalFragment().GetBreakToken()) {
    DCHECK(old_box_size);
    if (box_->Size() != *old_box_size) {
      box_->SizeChanged();
    }
  }
  CopyFragmentDataToLayoutBox(constraint_space, *layout_result, break_token);
}

void BlockNode::StoreResultInLayoutBox(const LayoutResult* result,
                                       const BlockBreakToken* break_token,
                                       bool clear_trailing_results) const {
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());
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

MinMaxSizesResult BlockNode::ComputeMinMaxSizes(
    WritingMode container_writing_mode,
    const SizeType type,
    const ConstraintSpace& constraint_space,
    const MinMaxSizesFloatInput float_input) const {
  // TODO(layoutng) Can UpdateMarkerTextIfNeeded call be moved
  // somewhere else? List items need up-to-date markers before layout.
  if (IsListItem())
    To<LayoutListItem>(box_.Get())->UpdateMarkerTextIfNeeded();

  // There is a path below for which we don't need to compute the (relatively)
  // expensive geometry.
  std::optional<FragmentGeometry> cached_fragment_geometry;
  auto IntrinsicFragmentGeometry = [&]() -> FragmentGeometry& {
    if (!cached_fragment_geometry) {
      cached_fragment_geometry =
          CalculateInitialFragmentGeometry(constraint_space, *this,
                                           /* break_token */ nullptr,
                                           /* is_intrinsic */ true);
    }
    return *cached_fragment_geometry;
  };

  const bool is_in_perform_layout = box_->GetFrameView()->IsInPerformLayout();
  // In some scenarios, GridNG and FlexNG will run layout on their items during
  // MinMaxSizes computation. Instead of running (and possible caching incorrect
  // results), when we're not performing layout, just use border + padding.
  if (!is_in_perform_layout &&
      (IsGrid() ||
       (IsFlexibleBox() && Style().ResolvedIsColumnFlexDirection()))) {
    const FragmentGeometry& fragment_geometry = IntrinsicFragmentGeometry();
    const BoxStrut border_padding =
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
    CHECK(is_in_perform_layout);

    // If we're computing MinMax after layout, we need to disable side effects
    // so that |Layout| does not update the |LayoutObject| tree and other global
    // states.
    std::optional<DisableLayoutSideEffectsScope> disable_side_effects;
    if (!GetLayoutBox()->NeedsLayout())
      disable_side_effects.emplace();

    const LayoutResult* layout_result = Layout(constraint_space);
    DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);
    sizes = LogicalFragment({container_writing_mode, TextDirection::kLtr},
                            layout_result->GetPhysicalFragment())
                .InlineSize();
    const bool depends_on_block_constraints =
        Style().LogicalWidth().HasAuto() ||
        Style().LogicalWidth().HasPercentOrStretch() ||
        Style().LogicalMinWidth().HasPercentOrStretch() ||
        Style().LogicalMaxWidth().HasPercentOrStretch();
    return MinMaxSizesResult(sizes, depends_on_block_constraints);
  }

  // Returns if we are (directly) dependent on any block constraints.
  auto DependsOnBlockConstraints = [&]() -> bool {
    return Style().LogicalHeight().HasPercentOrStretch() ||
           Style().LogicalMinHeight().HasPercentOrStretch() ||
           Style().LogicalMaxHeight().HasPercentOrStretch() ||
           (Style().LogicalHeight().HasAuto() &&
            constraint_space.IsBlockAutoBehaviorStretch());
  };

  // Directly handle replaced elements, caching doesn't have substantial gains
  // as most layouts are interested in the min/max content contribution which
  // calls `ComputeReplacedSize` directly. This is mainly used by flex.
  if (IsReplaced()) {
    MinMaxSizes sizes;
    sizes = IntrinsicFragmentGeometry().border_box_size.inline_size;
    return {sizes, DependsOnBlockConstraints()};
  }

  const bool has_aspect_ratio = !Style().AspectRatio().IsAuto();
  if (has_aspect_ratio && type == SizeType::kContent) {
    const FragmentGeometry& fragment_geometry = IntrinsicFragmentGeometry();
    const BoxStrut border_padding =
        fragment_geometry.border + fragment_geometry.padding;
    if (fragment_geometry.border_box_size.block_size != kIndefiniteSize) {
      const LayoutUnit inline_size_from_ar = InlineSizeFromAspectRatio(
          border_padding, Style().LogicalAspectRatio(),
          Style().BoxSizingForAspectRatio(),
          fragment_geometry.border_box_size.block_size);
      return MinMaxSizesResult({inline_size_from_ar, inline_size_from_ar},
                               DependsOnBlockConstraints(),
                               /* applied_aspect_ratio */ true);
    }
  }

  bool can_use_cached_intrinsic_inline_sizes =
      CanUseCachedIntrinsicInlineSizes(constraint_space, float_input, *this);

  // Ensure the cache is invalid if we know we can't use our cached sizes.
  if (!can_use_cached_intrinsic_inline_sizes) {
    box_->SetIntrinsicLogicalWidthsDirty(kMarkOnlyThis);
  }

  std::optional<MinMaxSizesResult> result;

  // Use our cached sizes if we don't have a descendant which depends on our
  // block constraints.
  if (can_use_cached_intrinsic_inline_sizes &&
      !box_->IntrinsicLogicalWidthsDependsOnBlockConstraints()) {
    result = box_->CachedIndefiniteIntrinsicLogicalWidths();
  }

  // We might still be able to use the cached values for a specific initial
  // block-size.
  if (!result && can_use_cached_intrinsic_inline_sizes &&
      !UseParentPercentageResolutionBlockSizeForChildren()) {
    result = box_->CachedIntrinsicLogicalWidths(
        IntrinsicFragmentGeometry().border_box_size.block_size);
  }

  if (!result) {
    const FragmentGeometry& fragment_geometry = IntrinsicFragmentGeometry();
    result = ComputeMinMaxSizesWithAlgorithm(
        LayoutAlgorithmParams(*this, fragment_geometry, constraint_space),
        float_input);

    const BoxStrut border_padding =
        fragment_geometry.border + fragment_geometry.padding;
    if (auto min_size = ContentMinimumInlineSize(*this, border_padding)) {
      result->sizes.min_size = *min_size;
    }

    // Update the cache with this intermediate value.
    box_->SetIntrinsicLogicalWidths(
        fragment_geometry.border_box_size.block_size, *result);
    if (IsTableCell()) {
      To<LayoutTableCell>(box_.Get())
          ->SetIntrinsicLogicalWidthsBorderSizes(
              constraint_space.TableCellBorders());
    }
  }

  if (has_aspect_ratio) {
    const FragmentGeometry& fragment_geometry = IntrinsicFragmentGeometry();
    if (fragment_geometry.border_box_size.block_size == kIndefiniteSize) {
      // If the block size will be computed from the aspect ratio, we need
      // to take the max-block-size into account.
      // https://drafts.csswg.org/css-sizing-4/#aspect-ratio
      const BoxStrut border_padding =
          fragment_geometry.border + fragment_geometry.padding;
      const MinMaxSizes min_max = ComputeMinMaxInlineSizesFromAspectRatio(
          constraint_space, *this, border_padding);
      result->sizes.min_size =
          min_max.ClampSizeToMinAndMax(result->sizes.min_size);
      result->sizes.max_size =
          min_max.ClampSizeToMinAndMax(result->sizes.max_size);
    }
  }

  // Determine if we are dependent on the block-constraints.
  // We report to our parent if we depend on the %-block-size if we used the
  // input %-block-size, or one of children said it depended on this.
  result->depends_on_block_constraints =
      (DependsOnBlockConstraints() ||
       UseParentPercentageResolutionBlockSizeForChildren()) &&
      (result->depends_on_block_constraints || has_aspect_ratio);
  return *result;
}

LayoutInputNode BlockNode::NextSibling() const {
  LayoutObject* next_sibling = box_->NextSibling();

  // We may have some LayoutInline(s) still within the tree (due to treating
  // inline-level floats and/or OOF-positioned nodes as block-level), we need
  // to skip them and clear layout.
  while (next_sibling && next_sibling->IsInline()) {
#if DCHECK_IS_ON()
    if (!next_sibling->IsText()) {
      next_sibling->ShowLayoutTreeForThis();
    }
    DCHECK(next_sibling->IsText());
#endif
    // TODO(layout-dev): Clearing needs-layout within this accessor is an
    // unexpected side-effect. There may be additional invalidations that need
    // to be performed.
    next_sibling->ClearNeedsLayout();
    next_sibling = next_sibling->NextSibling();
  }

  if (!next_sibling)
    return nullptr;

  return BlockNode(To<LayoutBox>(next_sibling));
}

LayoutInputNode BlockNode::FirstChild() const {
  // If this layout is blocked by a display-lock, then we pretend this node has
  // no children.
  if (ChildLayoutBlockedByDisplayLock()) {
    return nullptr;
  }
  auto* block = DynamicTo<LayoutBlock>(box_.Get());
  if (!block) [[unlikely]] {
    return BlockNode(box_->FirstChildBox());
  }
  auto* child = GetLayoutObjectForFirstChildNode(block);
  if (!child)
    return nullptr;
  if (!AreNGBlockFlowChildrenInline(block))
    return BlockNode(To<LayoutBox>(child));

  InlineNode inline_node(To<LayoutBlockFlow>(block));
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
  return BlockNode(To<LayoutBox>(child));
}

BlockNode BlockNode::GetRenderedLegend() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return BlockNode(
      LayoutFieldset::FindInFlowLegend(*To<LayoutBlock>(box_.Get())));
}

BlockNode BlockNode::GetFieldsetContent() const {
  if (!IsFieldsetContainer())
    return nullptr;
  return BlockNode(
      To<LayoutFieldset>(box_.Get())->FindAnonymousFieldsetContentBox());
}

LayoutUnit BlockNode::EmptyLineBlockSize(
    const BlockBreakToken* incoming_break_token) const {
  // Only return a line-height for the first fragment.
  if (IsBreakInside(incoming_break_token))
    return LayoutUnit();
  return box_->LogicalHeightForEmptyLine();
}

String BlockNode::ToString() const {
  return String::Format("BlockNode: %s",
                        GetLayoutBox()->ToString().Ascii().c_str());
}

void BlockNode::CopyFragmentDataToLayoutBox(
    const ConstraintSpace& constraint_space,
    const LayoutResult& layout_result,
    const BlockBreakToken* previous_break_token) const {
  const auto& physical_fragment =
      To<PhysicalBoxFragment>(layout_result.GetPhysicalFragment());
  bool is_last_fragment = !physical_fragment.GetBreakToken();

  // TODO(mstensho): This should always be done by the parent algorithm, since
  // we may have auto margins, which only the parent is able to resolve. Remove
  // the following line when all layout modes do this properly.
  UpdateMarginPaddingInfoIfNeeded(constraint_space, physical_fragment);

  auto* block_flow = DynamicTo<LayoutBlockFlow>(box_.Get());
  LayoutMultiColumnFlowThread* flow_thread = GetFlowThread(block_flow);

  // Position the children inside the box. We skip this if display-lock prevents
  // child layout.
  if (!ChildLayoutBlockedByDisplayLock()) {
    if (flow_thread) [[unlikely]] {
      // Hold off writing legacy data for the entire multicol container until
      // done with the last fragment (we may have multiple if nested within
      // another fragmentation context). This way we'll get everything in order.
      // We'd otherwise mess up in complex cases of nested column balancing. The
      // column layout algorithms may retry layout for a given fragment, which
      // would confuse the code that writes back to legacy objects, so that we
      // wouldn't always update column sets or establish fragmentainer groups
      // correctly.
      if (is_last_fragment) {
        const BlockBreakToken* incoming_break_token = nullptr;
        for (const PhysicalBoxFragment& multicol_fragment :
             box_->PhysicalFragments()) {
          PlaceChildrenInFlowThread(flow_thread, constraint_space,
                                    multicol_fragment, incoming_break_token);
          incoming_break_token = multicol_fragment.GetBreakToken();
        }
      }
    } else {
      PlaceChildrenInLayoutBox(physical_fragment, previous_break_token);
    }
  }

  if (!is_last_fragment) [[unlikely]] {
    return;
  }

  box_->SetNeedsOverflowRecalc(
      LayoutObject::OverflowRecalcType::kOnlyVisualOverflowRecalc);
  box_->SetScrollableOverflowFromLayoutResults();
  box_->UpdateAfterLayout();

  if (flow_thread && Style().HasColumnRule()) [[unlikely]] {
    // Issue full invalidation, in case the number of column rules have changed.
    box_->ClearNeedsLayoutWithFullPaintInvalidation();
  } else {
    box_->ClearNeedsLayout();
  }

  // We should notify the display lock that we've done layout on self, and if
  // it's not blocked, on children.
  if (auto* context = box_->GetDisplayLockContext()) {
    if (!ChildLayoutBlockedByDisplayLock())
      context->DidLayoutChildren();
  }
}

void BlockNode::PlaceChildrenInLayoutBox(
    const PhysicalBoxFragment& physical_fragment,
    const BlockBreakToken* previous_break_token,
    bool needs_invalidation_check) const {
  for (const auto& child_fragment : physical_fragment.Children()) {
    // Skip any line-boxes we have as children, this is handled within
    // InlineNode at the moment.
    if (!child_fragment->IsBox())
      continue;

    const auto& box_fragment = *To<PhysicalBoxFragment>(child_fragment.get());
    if (!box_fragment.IsFirstForNode())
      continue;

    // The offset for an OOF positioned node that is added as a child of a
    // fragmentainer box is handled by
    // OutOfFlowLayoutPart::AddOOFToFragmentainer().
    if (physical_fragment.IsFragmentainerBox() &&
        child_fragment->IsOutOfFlowPositioned()) [[unlikely]] {
      continue;
    }

    CopyChildFragmentPosition(box_fragment, child_fragment.offset,
                              physical_fragment, previous_break_token,
                              needs_invalidation_check);
  }
}

void BlockNode::PlaceChildrenInFlowThread(
    LayoutMultiColumnFlowThread* flow_thread,
    const ConstraintSpace& space,
    const PhysicalBoxFragment& physical_fragment,
    const BlockBreakToken* previous_container_break_token) const {
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

  WritingModeConverter converter(space.GetWritingDirection(),
                                 physical_fragment.Size());

  const BlockBreakToken* previous_column_break_token = nullptr;
  LayoutUnit flow_thread_offset;

  if (IsBreakInside(previous_container_break_token)) {
    // This multicol container is nested inside another fragmentation context,
    // and this isn't its first fragment. Locate the break token for the
    // previous inner column contents, so that we include the correct amount of
    // consumed block-size in the child offsets. If there's a break token for
    // column contents, we'll find it at the back.
    const auto& child_break_tokens =
        previous_container_break_token->ChildBreakTokens();
    if (!child_break_tokens.empty()) {
      const auto* token = To<BlockBreakToken>(child_break_tokens.back().Get());
      // We also create break tokens for spanners, so we need to check.
      if (token->InputNode() == *this) {
        previous_column_break_token = token;
      }
    }
  }

  for (const auto& child : physical_fragment.Children()) {
    const auto& child_fragment = To<PhysicalBoxFragment>(*child);
    const auto* child_box = To<LayoutBox>(child_fragment.GetLayoutObject());
    if (child_box && child_box != box_) {
      CopyChildFragmentPosition(child_fragment, child.offset,
                                physical_fragment);
      continue;
    }

    DCHECK(!child_box);

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
    if (const FragmentItems* items = child_fragment.Items()) {
      CopyFragmentItemsToLayoutBox(child_fragment, *items,
                                   previous_column_break_token);
    }

    previous_column_break_token = child_fragment.GetBreakToken();
  }

  if (!physical_fragment.GetBreakToken()) {
    flow_thread->FinishLayoutFromNG(flow_thread_offset);
  }
}

// Copies data back to the legacy layout tree for a given child fragment.
void BlockNode::CopyChildFragmentPosition(
    const PhysicalBoxFragment& child_fragment,
    PhysicalOffset offset,
    const PhysicalBoxFragment& container_fragment,
    const BlockBreakToken* previous_container_break_token,
    bool needs_invalidation_check) const {
  auto* layout_box = To<LayoutBox>(child_fragment.GetMutableLayoutObject());
  if (!layout_box)
    return;

  if (child_fragment.GetBoxType() == PhysicalFragment::kPageContainer ||
      child_fragment.GetBoxType() == PhysicalFragment::kPageBorderBox) {
    // These fragment types don't need to write anything back to their
    // LayoutBox. Furthermore, they have no parent, so the check below would
    // fail.
    return;
  }

  DCHECK(layout_box->Parent()) << "Should be called on children only.";

  LayoutPoint point = LayoutBoxUtils::ComputeLocation(
      child_fragment, offset, container_fragment,
      previous_container_break_token);
  layout_box->SetLocation(point);

  if (needs_invalidation_check)
    layout_box->SetShouldCheckForPaintInvalidation();
}

void BlockNode::MakeRoomForExtraColumns(LayoutUnit block_size) const {
  auto* block_flow = DynamicTo<LayoutBlockFlow>(GetLayoutBox());
  DCHECK(block_flow && block_flow->MultiColumnFlowThread());
  MultiColumnFragmentainerGroup& last_group =
      block_flow->MultiColumnFlowThread()
          ->LastMultiColumnSet()
          ->LastFragmentainerGroup();
  last_group.ExtendLogicalBottomInFlowThread(block_size);
}

void BlockNode::FinishPageContainerLayout(const LayoutResult* result) const {
  DCHECK_EQ(result->Status(), LayoutResult::kSuccess);
  DCHECK(result->GetPhysicalFragment().GetBoxType() ==
             PhysicalFragment::kPageContainer ||
         result->GetPhysicalFragment().GetBoxType() ==
             PhysicalFragment::kPageBorderBox);
  DCHECK(
      To<PhysicalBoxFragment>(result->GetPhysicalFragment()).IsOnlyForNode());
  StoreResultInLayoutBox(result, /*BlockBreakToken=*/nullptr);
}

void BlockNode::CopyFragmentItemsToLayoutBox(
    const PhysicalBoxFragment& container,
    const FragmentItems& items,
    const BlockBreakToken* previous_break_token) const {
  LayoutUnit previously_consumed_block_size;
  if (previous_break_token) {
    previously_consumed_block_size =
        previous_break_token->ConsumedBlockSizeForLegacy();
  }
  bool initial_container_is_flipped = Style().IsFlippedBlocksWritingMode();
  for (InlineCursor cursor(container, items); cursor; cursor.MoveToNext()) {
    if (const PhysicalBoxFragment* child = cursor.Current().BoxFragment()) {
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
        layout_box->SetLocation(maybe_flipped_offset.ToLayoutPoint());
        if (layout_box->HasSelfPaintingLayer()) [[unlikely]] {
          layout_box->Layer()->SetNeedsVisualOverflowRecalc();
        }
#if DCHECK_IS_ON()
        layout_box->InvalidateVisualOverflowForDCheck();
#endif
        continue;
      }

      // Legacy compatibility. This flag is used in paint layer for
      // invalidation.
      if (auto* layout_inline = DynamicTo<LayoutInline>(layout_object)) {
        if (layout_inline->HasSelfPaintingLayer()) [[unlikely]] {
          layout_inline->Layer()->SetNeedsVisualOverflowRecalc();
        }
      }
    }
  }
}

bool BlockNode::IsInlineFormattingContextRoot(
    InlineNode* first_child_out) const {
  if (const auto* block = DynamicTo<LayoutBlockFlow>(box_.Get())) {
    if (!AreNGBlockFlowChildrenInline(block))
      return false;
    LayoutInputNode first_child = FirstChild();
    if (first_child.IsInline()) {
      if (first_child_out)
        *first_child_out = To<InlineNode>(first_child);
      return true;
    }
  }
  return false;
}

bool BlockNode::IsInlineLevel() const {
  return GetLayoutBox()->IsInline();
}

bool BlockNode::IsAtomicInlineLevel() const {
  // LayoutObject::IsAtomicInlineLevel() returns true for e.g., <img
  // style="display: block">. Check IsInline() as well.
  return GetLayoutBox()->IsAtomicInlineLevel() && GetLayoutBox()->IsInline();
}

bool BlockNode::IsInTopOrViewTransitionLayer() const {
  return GetLayoutBox()->IsInTopOrViewTransitionLayer();
}

bool BlockNode::HasAspectRatio() const {
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

LogicalSize BlockNode::GetAspectRatio() const {
  // The CSS parser will ensure that this will only be set if the feature
  // is enabled.
  const EAspectRatioType ar_type = Style().AspectRatio().GetType();
  if (ar_type == EAspectRatioType::kRatio ||
      (ar_type == EAspectRatioType::kAutoAndRatio && !IsReplaced())) {
    return Style().LogicalAspectRatio();
  }

  if (!ShouldApplySizeContainment()) {
    IntrinsicSizingInfo legacy_sizing_info;
    To<LayoutReplaced>(box_.Get())
        ->ComputeIntrinsicSizingInfo(legacy_sizing_info);
    if (!legacy_sizing_info.aspect_ratio.IsEmpty()) {
      return StyleAspectRatio::LayoutRatioFromSizeF(
                 legacy_sizing_info.aspect_ratio)
          .ConvertToLogical(Style().GetWritingMode());
    }
  }

  if (ar_type == EAspectRatioType::kAutoAndRatio) {
    return Style().LogicalAspectRatio();
  }
  return LogicalSize();
}

std::optional<gfx::Transform> BlockNode::GetTransformForChildFragment(
    const PhysicalBoxFragment& child_fragment,
    PhysicalSize size) const {
  const auto* child_layout_object = child_fragment.GetLayoutObject();
  DCHECK(child_layout_object);

  if (!child_layout_object->ShouldUseTransformFromContainer(box_))
    return std::nullopt;

  std::optional<gfx::Transform> fragment_transform;
  if (!child_fragment.IsOnlyForNode()) {
    // If we're fragmented, there's no correct transform stored for
    // us. Calculate it now.
    fragment_transform.emplace();
    fragment_transform->MakeIdentity();
    const PhysicalRect reference_box = ComputeReferenceBox(child_fragment);
    child_fragment.Style().ApplyTransform(
        *fragment_transform, box_, reference_box,
        ComputedStyle::kIncludeTransformOperations,
        ComputedStyle::kIncludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
  }

  gfx::Transform transform;
  child_layout_object->GetTransformFromContainer(
      box_, PhysicalOffset(), transform, &size,
      base::OptionalToPtr(fragment_transform));

  return transform;
}

bool BlockNode::HasNonVisibleBlockOverflow() const {
  OverflowClipAxes clip_axes = GetOverflowClipAxes();
  if (Style().IsHorizontalWritingMode())
    return clip_axes & kOverflowClipY;
  return clip_axes & kOverflowClipX;
}

bool BlockNode::IsCustomLayoutLoaded() const {
  return To<LayoutCustom>(box_.Get())->IsLoaded();
}

void BlockNode::HandleScrollMarkerGroup() const {
  BlockNode group_node = GetScrollMarkerGroup();
  if (!group_node) {
    return;
  }

  GetDocument().GetStyleEngine().SetInScrollMarkersAttachment(true);

  // Detach all markers.
  while (LayoutObject* child = group_node.GetLayoutBox()->SlowFirstChild()) {
    // Anonymous wrappers may have been inserted. Search for the marker.
    for (LayoutObject* walker = child; walker;
         walker = walker->NextInPreOrder(child)) {
      if (walker->GetNode() &&
          walker->GetNode()->IsScrollMarkerPseudoElement()) {
        walker->GetNode()->DetachLayoutTree(/*performing_reattach=*/true);
        break;
      }
    }
  }
  DCHECK(!group_node.GetLayoutBox()->SlowFirstChild());

  Node::AttachContext context;
  context.parent = group_node.GetLayoutBox();
  DCHECK(context.parent);

  auto* scroll_marker_group =
      To<ScrollMarkerGroupPseudoElement>(group_node.GetLayoutBox()->GetNode());
  scroll_marker_group->ClearFocusGroup();
  if (PseudoElement* scroll_next_button =
          scroll_marker_group->OriginatingElement()->GetPseudoElement(
              kPseudoIdScrollNextButton)) {
    To<ScrollButtonPseudoElement>(scroll_next_button)
        ->SetScrollMarkerGroup(scroll_marker_group);
  }
  if (PseudoElement* scroll_prev_button =
          scroll_marker_group->OriginatingElement()->GetPseudoElement(
              kPseudoIdScrollPrevButton)) {
    To<ScrollButtonPseudoElement>(scroll_prev_button)
        ->SetScrollMarkerGroup(scroll_marker_group);
  }
  AttachScrollMarkers(*box_, context);

  DCHECK(GetDocument().GetStyleEngine().InScrollMarkersAttachment());
  GetDocument().GetStyleEngine().SetInScrollMarkersAttachment(false);

  // The ::scroll-marker-group has now been populated with markers. If the group
  // comes after the principal box, we can return, and let the parent layout
  // algorithm (whatever that is) handle it as part of normal layout.
  if (!group_node.GetLayoutBox()->IsScrollMarkerGroupBefore()) {
    return;
  }

  // If the group comes before the principal box, it means that we might already
  // be past it, layout-wise. Lay it out again, and replace the innards of the
  // fragment from the previous layout. This should be safe, as long as the box
  // establishes sufficient amounts of containment.
  const LayoutResult* result =
      group_node.GetLayoutBox()->GetCachedLayoutResult(nullptr);
  if (!result) {
    // This may happen e.g. if the ::scroll-marker-group is out-of-flow
    // positioned, and hasn't been laid out yet (which is great, because then we
    // won't have to do the innards-replacement).
    return;
  }
  const auto& fragment = To<PhysicalBoxFragment>(result->GetPhysicalFragment());

  // A ::scroll-marker-group should be monolithic.
  DCHECK(fragment.IsOnlyForNode());

  const ConstraintSpace& space = result->GetConstraintSpaceForCaching();
  const LayoutResult* new_result = group_node.Layout(space);
  // TODO(layout-dev): It's being genetically modified all right, but we're not
  // really "cloning".
  fragment.GetMutableForCloning().ReplaceChildren(
      To<PhysicalBoxFragment>(new_result->GetPhysicalFragment()));
  // The second layout would have replaced the original layout result with the
  // new one, but we want to keep the original result.
  group_node.StoreResultInLayoutBox(result, /*BlockBreakToken=*/nullptr);
}

MathScriptType BlockNode::ScriptType() const {
  DCHECK(IsA<MathMLScriptsElement>(GetDOMNode()));
  return To<MathMLScriptsElement>(GetDOMNode())->GetScriptType();
}

bool BlockNode::HasIndex() const {
  DCHECK(IsA<MathMLRadicalElement>(GetDOMNode()));
  return To<MathMLRadicalElement>(GetDOMNode())->HasIndex();
}

const LayoutResult* BlockNode::LayoutAtomicInline(
    const ConstraintSpace& parent_constraint_space,
    const ComputedStyle& parent_style,
    bool use_first_line_style,
    BaselineAlgorithmType baseline_algorithm_type) {
  ConstraintSpaceBuilder builder(parent_constraint_space,
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
  ConstraintSpace constraint_space = builder.ToConstraintSpace();
  const LayoutResult* result = Layout(constraint_space);
  if (!DisableLayoutSideEffectsScope::IsDisabled()) {
    // TODO(kojii): Investigate why ClearNeedsLayout() isn't called
    // automatically when it's being laid out.
    GetLayoutBox()->ClearNeedsLayout();
  }
  return result;
}

const LayoutResult* BlockNode::RunSimplifiedLayout(
    const LayoutAlgorithmParams& params,
    const LayoutResult& previous_result) const {
  SimplifiedLayoutAlgorithm algorithm(params, previous_result);
  if (const auto* previous_box_fragment = DynamicTo<PhysicalBoxFragment>(
          &previous_result.GetPhysicalFragment())) {
    if (previous_box_fragment->HasItems())
      return algorithm.LayoutWithItemsBuilder();
  }
  return algorithm.Layout();
}

void BlockNode::UpdateMarginPaddingInfoIfNeeded(
    const ConstraintSpace& space,
    const PhysicalFragment& fragment) const {
  // Table-cells don't have margins, and aren't grid-items.
  if (space.IsTableCell())
    return;

  if (Style().MayHaveMargin()) {
    // We set the initial margin data here because RebuildFragmentTreeSpine()
    // and atomic inline layout don't use BoxFragmentBuilder::AddResult().
    // TODO(crbug.com/1353190): Try to move margin computation to them.
    To<PhysicalBoxFragment>(fragment).GetMutableForContainerLayout().SetMargins(
        ComputePhysicalMargins(space, Style()));

    // This margin data may be overwritten by BoxFragmentBuilder::AddResult().
  }

  if (Style().MayHaveMargin() || Style().MayHavePadding()) {
    // Copy back the %-size so that |LayoutBoxModelObject::ComputedCSSPadding|
    // is able to return the correct value. This isn't ideal, but eventually
    // we'll answer these queries from the fragment.
    const auto* containing_block = box_->ContainingBlock();
    if (containing_block && containing_block->IsLayoutGrid()) [[unlikely]] {
      box_->SetOverrideContainingBlockContentLogicalWidth(
          space.MarginPaddingPercentageResolutionSize().inline_size);
    }
  }
}

// Floats can optionally have a shape area, specified by "shape-outside". The
// current shape machinery requires setting the size of the float after layout
// in the parents writing mode.
void BlockNode::UpdateShapeOutsideInfoIfNeeded(
    const LayoutResult& layout_result,
    const ConstraintSpace& constraint_space) const {
  if (!box_->IsFloating() || !box_->GetShapeOutsideInfo())
    return;

  if (layout_result.Status() != LayoutResult::kSuccess) {
    return;
  }

  // The box_ may not have a valid size yet (due to an intermediate layout),
  // use the fragment's size instead.
  PhysicalSize box_size = layout_result.GetPhysicalFragment().Size();

  // TODO(ikilpatrick): Ideally this should be moved to a LayoutResult
  // computing the shape area. There may be an issue with the new fragmentation
  // model and computing the correct sizes of shapes.
  ShapeOutsideInfo* shape_outside = box_->GetShapeOutsideInfo();
  WritingMode writing_mode = box_->ContainingBlock()->Style()->GetWritingMode();
  BoxStrut margins = ComputePhysicalMargins(constraint_space, Style())
                         .ConvertToLogical({writing_mode, TextDirection::kLtr});
  shape_outside->SetReferenceBoxLogicalSize(
      box_size.ConvertToLogical(writing_mode),
      LogicalSize(margins.InlineSum(), margins.BlockSum()));
  shape_outside->SetPercentageResolutionInlineSize(
      constraint_space.PercentageResolutionInlineSize());
}

void BlockNode::StoreColumnSizeAndCount(LayoutUnit inline_size, int count) {
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
