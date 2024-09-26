// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/out_of_flow_layout_part.h"

#include <math.h>

#include <algorithm>

#include "base/memory/values_equivalent.h"
#include "base/not_fatal_until.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/absolute_utils.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"
#include "third_party/blink/renderer/core/layout/anchor_query_map.h"
#include "third_party/blink/renderer/core/layout/column_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/disable_layout_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/grid/grid_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/grid/grid_placement.h"
#include "third_party/blink/renderer/core/layout/inline/physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_utils.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/logical_fragment.h"
#include "third_party/blink/renderer/core/layout/oof_positioned_node.h"
#include "third_party/blink/renderer/core/layout/paginated_root_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/layout/simplified_layout_algorithm.h"
#include "third_party/blink/renderer/core/layout/simplified_oof_layout_algorithm.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {

namespace {

// `margin_box_start`/`margin_box_end` and `imcb_inset_start`/`imcb_inset_end`
// are relative to the IMCB.
bool CalculateNonOverflowingRangeInOneAxis(
    LayoutUnit margin_box_start,
    LayoutUnit margin_box_end,
    LayoutUnit imcb_inset_start,
    LayoutUnit imcb_inset_end,
    LayoutUnit position_area_start,
    LayoutUnit position_area_end,
    bool has_non_auto_inset_start,
    bool has_non_auto_inset_end,
    std::optional<LayoutUnit>* out_scroll_min,
    std::optional<LayoutUnit>* out_scroll_max) {
  const LayoutUnit start_available_space = margin_box_start - imcb_inset_start;
  if (has_non_auto_inset_start) {
    // If the start inset is non-auto, then the start edges of both the
    // scroll-adjusted inset-modified containing block and the scroll-shifted
    // margin box always move by the same amount on scrolling. Then it overflows
    // if and only if it overflows at the initial scroll location.
    if (start_available_space < 0) {
      return false;
    }
  } else {
    // Otherwise, the start edge of the scroll-adjusted inset-modified
    // containing block is always at the same location, while that of the
    // scroll-shifted margin box can move by at most `start_available_space`
    // before overflowing.
    *out_scroll_max = position_area_start + start_available_space;
  }
  // Calculation for the end edge is symmetric.
  const LayoutUnit end_available_space = imcb_inset_end - margin_box_end;
  if (has_non_auto_inset_end) {
    if (end_available_space < 0) {
      return false;
    }
  } else {
    *out_scroll_min = -(position_area_end + end_available_space);
  }
  if (*out_scroll_min && *out_scroll_max &&
      out_scroll_min->value() > out_scroll_max->value()) {
    return false;
  }
  return true;
}

// Helper class to enumerate all the candidate styles to be passed to
// `TryCalculateOffset()`. The class should iterate through:
// - The base style, if no `position-try-fallbacks` is specified
// - The `@position-try` rule styles and try tactics if `position-try-fallbacks`
//   is specified
class OOFCandidateStyleIterator {
  STACK_ALLOCATED();

 public:
  explicit OOFCandidateStyleIterator(const LayoutObject& object,
                                     AnchorEvaluatorImpl& anchor_evaluator)
      : element_(DynamicTo<Element>(object.GetNode())),
        style_(object.Style()),
        anchor_evaluator_(anchor_evaluator) {
    Initialize();
  }

  bool HasPositionTryFallbacks() const {
    return position_try_fallbacks_ != nullptr;
  }

  // https://drafts.csswg.org/css-anchor-position-1/#propdef-position-try-order
  EPositionTryOrder PositionTryOrder() const { return position_try_order_; }

  // The current index into the position-try-fallbacks list. If nullopt, then
  // we're currently at the regular style, i.e. the one without any try fallback
  // included.
  std::optional<wtf_size_t> TryFallbackIndex() const {
    return try_fallback_index_;
  }

  const ComputedStyle& GetStyle() const { return *style_; }

  const ComputedStyle& GetBaseStyle() const {
    if (HasPositionTryFallbacks()) {
      return *GetStyle().GetBaseComputedStyleOrThis();
    }
    return GetStyle();
  }

  const ComputedStyle& ActivateBaseStyleForTryAttempt() {
    if (!HasPositionTryFallbacks()) {
      return GetStyle();
    }
    const ComputedStyle& base_style = GetBaseStyle();
    if (&base_style != &GetStyle()) {
      element_->GetLayoutObject()->SetStyle(
          &base_style, LayoutObject::ApplyStyleChanges::kNo);
    }
    return base_style;
  }

  const ComputedStyle& ActivateStyleForChosenFallback() {
    const ComputedStyle& style = GetStyle();
    element_->GetLayoutObject()->SetStyle(&style,
                                          LayoutObject::ApplyStyleChanges::kNo);
    return style;
  }

  bool MoveToNextStyle() {
    CHECK(position_try_fallbacks_);
    CHECK(element_);
    if (!try_fallback_index_.has_value()) {
      try_fallback_index_ = 0;
    } else {
      ++*try_fallback_index_;
    }
    // Need to loop in case a @position-try fallback does not exist.
    for (;
         *try_fallback_index_ < position_try_fallbacks_->GetFallbacks().size();
         ++*try_fallback_index_) {
      if (const ComputedStyle* style = UpdateStyle(*try_fallback_index_)) {
        style_ = style;
        return true;
      }
      // @position-try fallback does not exist.
    }
    return false;
  }

  void MoveToLastSuccessfulOrStyleWithoutFallbacks() {
    CHECK(element_);
    const CSSPropertyValueSet* try_set = nullptr;
    TryTacticList try_tactics = kNoTryTactics;
    if (OutOfFlowData* out_of_flow_data = element_->GetOutOfFlowData()) {
      // No successful fallbacks for this pass. Clear out the new successful
      // fallback candidate.
      out_of_flow_data->ClearPendingSuccessfulPositionFallback();
      if (out_of_flow_data->HasLastSuccessfulPositionFallback()) {
        try_set = out_of_flow_data->GetLastSuccessfulTrySet();
        try_tactics = out_of_flow_data->GetLastSuccessfulTryTactics();
      }
    }
    style_ = UpdateStyle(try_set, try_tactics);
  }

  std::optional<const CSSPropertyValueSet*> TrySetFromFallback(
      const PositionTryFallback& fallback) {
    if (!fallback.GetPositionArea().IsNone()) {
      // This fallback is an position-area(). Create a declaration block
      // with an equivalent position-area declaration.
      CSSPropertyValue declaration(
          CSSPropertyName(CSSPropertyID::kPositionArea),
          *ComputedStyleUtils::ValueForPositionArea(
              fallback.GetPositionArea()));
      return ImmutableCSSPropertyValueSet::Create(&declaration, /* length */ 1u,
                                                  kHTMLStandardMode);
    } else if (const ScopedCSSName* name = fallback.GetPositionTryName()) {
      if (const StyleRulePositionTry* rule = GetPositionTryRule(*name)) {
        return &rule->Properties();
      }
      return std::nullopt;
    }
    return nullptr;
  }

  void MoveToChosenTryFallbackIndex(std::optional<wtf_size_t> index) {
    CHECK(element_);
    const CSSPropertyValueSet* try_set = nullptr;
    TryTacticList try_tactics = kNoTryTactics;
    bool may_invalidate_last_successful = false;
    if (index.has_value()) {
      CHECK(position_try_fallbacks_);
      CHECK_LE(index.value(), position_try_fallbacks_->GetFallbacks().size());
      const PositionTryFallback& fallback =
          position_try_fallbacks_->GetFallbacks()[*index];
      try_tactics = fallback.GetTryTactic();
      std::optional<const CSSPropertyValueSet*> opt_try_set =
          TrySetFromFallback(fallback);
      CHECK(opt_try_set.has_value());
      try_set = opt_try_set.value();
      may_invalidate_last_successful =
          element_->EnsureOutOfFlowData().SetPendingSuccessfulPositionFallback(
              position_try_fallbacks_, try_set, try_tactics, index);
    } else if (OutOfFlowData* out_of_flow_data = element_->GetOutOfFlowData()) {
      may_invalidate_last_successful =
          out_of_flow_data->SetPendingSuccessfulPositionFallback(
              position_try_fallbacks_,
              /* try_set */ nullptr, kNoTryTactics, /* index */ std::nullopt);
    }
    if (may_invalidate_last_successful) {
      element_->GetDocument()
          .GetStyleEngine()
          .MarkLastSuccessfulPositionFallbackDirtyForElement(*element_);
    }
    if (index == try_fallback_index_) {
      // We're already at this position.
      return;
    }
    style_ = UpdateStyle(try_set, try_tactics);
  }

 private:
  void Initialize() {
    if (element_) {
      position_try_fallbacks_ = style_->GetPositionTryFallbacks();
      position_try_order_ = style_->PositionTryOrder();

      // If the base styles contain anchor*() queries, or depend on other
      // information produced by the AnchorEvaluator, then the ComputedStyle
      // produced by the main style recalc pass (which has no AnchorEvaluator)
      // is incorrect. For example, all anchor() queries would have evaluated
      // to their fallback value. Now that we have an AnchorEvaluator, we can
      // fix this by updating the style.
      //
      // Note that it's important to avoid the expensive call to UpdateStyle
      // here if we *don't* depend on anchor*(), since every out-of-flow will
      // reach this function, regardless of whether or not anchor positioning
      // is actually used.
      if (ElementStyleDependsOnAnchor(*element_, *style_)) {
        style_ = UpdateStyle(/* try_set */ nullptr, kNoTryTactics);
      }
    }
  }

  bool ElementStyleDependsOnAnchor(const Element& element,
                                   const ComputedStyle& style) {
    if (style.PositionAnchor() || element.ImplicitAnchorElement()) {
      // anchor-center offsets may need to be updated since the layout of the
      // anchor may have changed. anchor-center offsets are computed when a
      // default anchor is present.
      return true;
    }
    if (style.HasAnchorFunctions()) {
      return true;
    }
    return false;
  }

  const StyleRulePositionTry* GetPositionTryRule(
      const ScopedCSSName& scoped_name) {
    CHECK(element_);
    return element_->GetDocument().GetStyleEngine().GetPositionTryRule(
        scoped_name);
  }

  // Update the style using the specified index into `position_try_fallbacks_`
  // (which must exist), and return that updated style. Returns nullptr if
  // the fallback references a @position-try rule which doesn't exist.
  const ComputedStyle* UpdateStyle(wtf_size_t try_fallback_index) {
    // Previously evaluated anchor is not relevant if another position fallback
    // is applied.
    anchor_evaluator_.ClearAccessibilityAnchor();
    CHECK(position_try_fallbacks_);
    CHECK_LE(try_fallback_index,
             position_try_fallbacks_->GetFallbacks().size());
    const PositionTryFallback& fallback =
        position_try_fallbacks_->GetFallbacks()[try_fallback_index];
    std::optional<const CSSPropertyValueSet*> try_set =
        TrySetFromFallback(fallback);
    if (!try_set.has_value()) {
      // @position-try fallback does not exist.
      return nullptr;
    }
    return UpdateStyle(try_set.value(), fallback.GetTryTactic());
  }

  const ComputedStyle* UpdateStyle(const CSSPropertyValueSet* try_set,
                                   const TryTacticList& tactic_list) {
    CHECK(element_);
    element_->GetDocument().GetStyleEngine().UpdateStyleForOutOfFlow(
        *element_, try_set, tactic_list, &anchor_evaluator_);
    CHECK(element_->GetLayoutObject());
    // Returns LayoutObject ComputedStyle instead of element style for layout
    // purposes. The style may be different, in particular for body -> html
    // propagation of writing modes.
    return element_->GetLayoutObject()->Style();
  }

  Element* element_ = nullptr;

  // The current candidate style if no auto anchor fallback is triggered.
  // Otherwise, the base style for generating auto anchor fallbacks.
  const ComputedStyle* style_ = nullptr;

  // This evaluator is passed to StyleEngine::UpdateStyleForOutOfFlow to
  // evaluate anchor queries on the computed style.
  AnchorEvaluatorImpl& anchor_evaluator_;

  // If the current style is applying a `position-try-fallbacks` fallback, this
  // holds the list of fallbacks. Otherwise nullptr.
  const PositionTryFallbacks* position_try_fallbacks_ = nullptr;

  EPositionTryOrder position_try_order_ = EPositionTryOrder::kNormal;

  // If the current style is created using `position-try-fallbacks`, an index
  // into the list of fallbacks; otherwise nullopt.
  std::optional<wtf_size_t> try_fallback_index_;
};

const Element* GetPositionAnchorElement(
    const BlockNode& node,
    const ComputedStyle& style,
    const LogicalAnchorQuery* anchor_query) {
  if (!anchor_query) {
    return nullptr;
  }
  if (const ScopedCSSName* specifier = style.PositionAnchor()) {
    if (const LogicalAnchorReference* reference =
            anchor_query->AnchorReference(*node.GetLayoutBox(), specifier);
        reference && reference->layout_object) {
      return DynamicTo<Element>(reference->layout_object->GetNode());
    }
    return nullptr;
  }
  if (auto* element = DynamicTo<Element>(node.GetDOMNode())) {
    return element->ImplicitAnchorElement();
  }
  return nullptr;
}

const LayoutObject* GetPositionAnchorObject(
    const BlockNode& node,
    const ComputedStyle& style,
    const LogicalAnchorQuery* anchor_query) {
  if (const Element* element =
          GetPositionAnchorElement(node, style, anchor_query)) {
    return element->GetLayoutObject();
  }
  return nullptr;
}

gfx::Vector2dF GetAnchorOffset(const BlockNode& node,
                               const ComputedStyle& style,
                               const LogicalAnchorQuery* anchor_query) {
  if (const LayoutObject* anchor_object =
          GetPositionAnchorObject(node, style, anchor_query)) {
    if (const AnchorPositionScrollData* data =
            To<Element>(node.GetDOMNode())->GetAnchorPositionScrollData()) {
      return data->TotalOffset(*anchor_object);
    }
  }
  return gfx::Vector2dF();
}

// Updates `node`'s associated `PaintLayer` for `position-visibility`. See:
// https://drafts.csswg.org/css-anchor-position-1/#position-visibility. The
// values of `no-overflow` and `anchors-valid` are computed and directly update
// the `PaintLayer` in this function. The remaining value of `anchors-visible`
// is computed via an intersection observer set up in this function, and the
// `PaintLayer` is updated later during the post-layout intersection observer
// step.
void UpdatePositionVisibilityAfterLayout(
    const OutOfFlowLayoutPart::OffsetInfo& offset_info,
    const BlockNode& node,
    const LogicalAnchorQuery* anchor_query) {
  if (!anchor_query) {
    return;
  }

  // TODO(crbug.com/332933527): Support anchors-valid.

  PaintLayer* layer = node.GetLayoutBox()->Layer();
  CHECK(layer);
  bool has_no_overflow_visibility =
      node.Style().HasPositionVisibility(PositionVisibility::kNoOverflow);
  layer->SetInvisibleForPositionVisibility(
      LayerPositionVisibility::kNoOverflow,
      has_no_overflow_visibility && offset_info.overflows_containing_block);

  // TODO(wangxianzhu): We may be anchored in cases where we do not need scroll
  // adjustment, such as when the anchor and anchored have the same containing
  // block. For now though, these flags are true in this case.
  bool is_anchor_positioned = offset_info.needs_scroll_adjustment_in_x ||
                              offset_info.needs_scroll_adjustment_in_y;
  bool has_anchors_visible_visibility =
      node.Style().HasPositionVisibility(PositionVisibility::kAnchorsVisible);
  Element* anchored = DynamicTo<Element>(node.GetDOMNode());
  // https://drafts.csswg.org/css-anchor-position-1/#valdef-position-visibility-anchors-visible
  // We only need to track the default anchor for anchors-visible.
  const Element* anchor =
      anchored ? GetPositionAnchorElement(node, node.Style(), anchor_query)
               : nullptr;
  if (is_anchor_positioned && has_anchors_visible_visibility && anchor) {
    anchored->EnsureAnchorPositionScrollData()
        .EnsureAnchorPositionVisibilityObserver()
        .MonitorAnchor(anchor);
  } else if (anchored) {
    if (auto* scroll_data = anchored->GetAnchorPositionScrollData()) {
      if (auto* observer = scroll_data->GetAnchorPositionVisibilityObserver()) {
        observer->MonitorAnchor(nullptr);
      }
    }
  }
}

}  // namespace

// static
std::optional<LogicalSize> OutOfFlowLayoutPart::InitialContainingBlockFixedSize(
    BlockNode container) {
  if (!container.GetLayoutBox()->IsLayoutView() ||
      container.GetDocument().Printing())
    return std::nullopt;
  const auto* frame_view = container.GetDocument().View();
  DCHECK(frame_view);
  PhysicalSize size(
      frame_view->LayoutViewport()->ExcludeScrollbars(frame_view->Size()));
  return size.ConvertToLogical(container.Style().GetWritingMode());
}

OutOfFlowLayoutPart::OutOfFlowLayoutPart(BoxFragmentBuilder* container_builder)
    : container_builder_(container_builder),
      is_absolute_container_(container_builder->Node().IsAbsoluteContainer()),
      is_fixed_container_(container_builder->Node().IsFixedContainer()),
      has_block_fragmentation_(
          InvolvedInBlockFragmentation(*container_builder)) {
  // If there are no OOFs inside, we can return early, except if this is the
  // root. There may be top-layer nodes still to be added. Additionally, for
  // pagination, we might not have hauled any OOFs inside the fragmentainers
  // yet. See HandleFragmentation().
  if (!container_builder->HasOutOfFlowPositionedCandidates() &&
      !container_builder->HasOutOfFlowFragmentainerDescendants() &&
      !container_builder->HasMulticolsWithPendingOOFs() &&
      !container_builder->IsRoot()) {
    return;
  }

  // Disable first tier cache for grid layouts, as grid allows for out-of-flow
  // items to be placed in grid areas, which is complex to maintain a cache for.
  const BoxStrut border_scrollbar =
      container_builder->Borders() + container_builder->Scrollbar();
  default_containing_block_info_for_absolute_.writing_direction =
      GetConstraintSpace().GetWritingDirection();
  default_containing_block_info_for_fixed_.writing_direction =
      GetConstraintSpace().GetWritingDirection();
  default_containing_block_info_for_absolute_.is_scroll_container =
      container_builder_->Node().IsScrollContainer();
  default_containing_block_info_for_fixed_.is_scroll_container =
      container_builder_->Node().IsScrollContainer();
  if (container_builder_->HasBlockSize()) {
    default_containing_block_info_for_absolute_.rect.size =
        ShrinkLogicalSize(container_builder_->Size(), border_scrollbar);
    default_containing_block_info_for_fixed_.rect.size =
        InitialContainingBlockFixedSize(container_builder->Node())
            .value_or(default_containing_block_info_for_absolute_.rect.size);
  }
  LogicalOffset container_offset = {border_scrollbar.inline_start,
                                    border_scrollbar.block_start};
  default_containing_block_info_for_absolute_.rect.offset = container_offset;
  default_containing_block_info_for_fixed_.rect.offset = container_offset;
}

void OutOfFlowLayoutPart::Run() {
  if (container_builder_->IsPaginatedRoot()) {
    PropagateOOFsFromPageAreas();
  }

  HandleFragmentation();

  // If the container is display-locked, then we skip the layout of descendants,
  // so we can early out immediately.
  const BlockNode& node = container_builder_->Node();
  if (node.ChildLayoutBlockedByDisplayLock()) {
    return;
  }

  HeapVector<LogicalOofPositionedNode> candidates;
  ClearCollectionScope<HeapVector<LogicalOofPositionedNode>> clear_scope(
      &candidates);
  container_builder_->SwapOutOfFlowPositionedCandidates(&candidates);

  if (!candidates.empty()) {
    LayoutCandidates(&candidates);
  } else {
    container_builder_
        ->AdjustFixedposContainingBlockForFragmentainerDescendants();
    container_builder_->AdjustFixedposContainingBlockForInnerMulticols();
  }

  // If this is for the root fragment, now process top-layer elements.
  // We do this last as:
  //  - Additions/removals may occur while processing normal out-of-flow
  //    positioned elements (e.g. via a container-query).
  //  - They correctly reference any anchor()s from preceding elements.
  if (!container_builder_->IsRoot()) {
    return;
  }

  for (LayoutInputNode child = node.FirstChild(); child;
       child = child.NextSibling()) {
    if (!child.IsBlock()) {
      continue;
    }
    BlockNode block_child = To<BlockNode>(child);
    if (!block_child.IsInTopOrViewTransitionLayer() ||
        !block_child.IsOutOfFlowPositioned()) {
      continue;
    }

    // https://drafts.csswg.org/css-position-4/#top-styling
    // The static position for top-layer elements is just 0x0.
    container_builder_->AddOutOfFlowChildCandidate(
        block_child, LogicalOffset(),
        LogicalStaticPosition::InlineEdge::kInlineStart,
        LogicalStaticPosition::BlockEdge::kBlockStart,
        /*is_hidden_for_paint=*/false,
        /*allow_top_layer_nodes=*/true);

    // With one top-layer node added, run through the machinery again. Note that
    // we need to do this separately for each node, as laying out a node may
    // cause top-layer nodes to be added or removed.
    HandleFragmentation();
    container_builder_->SwapOutOfFlowPositionedCandidates(&candidates);
    LayoutCandidates(&candidates);
  }
}

void OutOfFlowLayoutPart::PropagateOOFsFromPageAreas() {
  DCHECK(container_builder_->IsPaginatedRoot());
  LogicalOffset offset_adjustment;
  for (wtf_size_t i = 0; i < ChildCount(); i++) {
    // Propagation from children stopped at the fragmentainers (the page area
    // fragments). Now collect any pending OOFs, and lay them out.
    const PhysicalBoxFragment& fragmentainer = GetChildFragment(i);
    if (fragmentainer.NeedsOOFPositionedInfoPropagation()) {
      container_builder_->PropagateOOFPositionedInfo(
          fragmentainer, LogicalOffset(), LogicalOffset(), offset_adjustment);
    }
    if (const auto* break_token = fragmentainer.GetBreakToken()) {
      offset_adjustment.block_offset = break_token->ConsumedBlockSize();
    }
  }
}

void OutOfFlowLayoutPart::HandleFragmentation() {
  // OOF fragmentation depends on LayoutBox data being up-to-date, which isn't
  // the case if side-effects are disabled. So we cannot safely do anything
  // here.
  if (DisableLayoutSideEffectsScope::IsDisabled()) {
    return;
  }

  if (!column_balancing_info_ &&
      (!container_builder_->IsBlockFragmentationContextRoot() ||
       has_block_fragmentation_)) {
    return;
  }

  if (container_builder_->Node().IsPaginatedRoot()) {
    HeapVector<LogicalOofPositionedNode> candidates;
    ClearCollectionScope<HeapVector<LogicalOofPositionedNode>> scope(
        &candidates);
    container_builder_->SwapOutOfFlowPositionedCandidates(&candidates);
    // Catch everything for paged layout. We want to fragment everything. If the
    // containing block is the initial containing block, it should be fragmented
    // now, and not bubble further to the viewport (where we'd end up with
    // non-fragmented layout). Note that we're not setting a containing block
    // fragment for the candidates, as that would confuse
    // GetContainingBlockInfo(), which expects a containing block fragment to
    // also have a LayoutObject, which fragmentainers don't. Fixing that is
    // possible, but requires special-code there. This approach seems easier.
    for (LogicalOofPositionedNode candidate : candidates) {
      container_builder_->AddOutOfFlowFragmentainerDescendant(candidate);
    }
  }

  DCHECK(!child_fragment_storage_ || !child_fragment_storage_->empty());
  DCHECK(
      !column_balancing_info_ ||
      !column_balancing_info_->out_of_flow_fragmentainer_descendants.empty());

  auto ShouldContinue = [&]() -> bool {
    if (column_balancing_info_)
      return column_balancing_info_->HasOutOfFlowFragmentainerDescendants();
    return container_builder_->HasOutOfFlowFragmentainerDescendants() ||
           container_builder_->HasMulticolsWithPendingOOFs();
  };

  while (ShouldContinue()) {
    HeapVector<LogicalOofNodeForFragmentation> fragmentainer_descendants;
    ClearCollectionScope<HeapVector<LogicalOofNodeForFragmentation>> scope(
        &fragmentainer_descendants);
    if (column_balancing_info_) {
      column_balancing_info_->SwapOutOfFlowFragmentainerDescendants(
          &fragmentainer_descendants);
      DCHECK(!fragmentainer_descendants.empty());
    } else {
      HandleMulticolsWithPendingOOFs(container_builder_);
      if (container_builder_->HasOutOfFlowFragmentainerDescendants()) {
        container_builder_->SwapOutOfFlowFragmentainerDescendants(
            &fragmentainer_descendants);
        DCHECK(!fragmentainer_descendants.empty());
      }
    }
    if (!fragmentainer_descendants.empty()) {
      LogicalOffset fragmentainer_progression = GetFragmentainerProgression(
          *container_builder_, GetFragmentainerType());
      LayoutFragmentainerDescendants(&fragmentainer_descendants,
                                     fragmentainer_progression);
    }
  }
  if (!column_balancing_info_) {
    for (auto& descendant : delayed_descendants_)
      container_builder_->AddOutOfFlowFragmentainerDescendant(descendant);
  }
}

OutOfFlowLayoutPart::ContainingBlockInfo
OutOfFlowLayoutPart::ApplyPositionAreaOffsets(
    const PositionAreaOffsets& offsets,
    const OutOfFlowLayoutPart::ContainingBlockInfo& container_info) const {
  ContainingBlockInfo adjusted_container_info(container_info);
  PhysicalToLogical converter(container_info.writing_direction,
                              offsets.top.value_or(LayoutUnit()),
                              offsets.right.value_or(LayoutUnit()),
                              offsets.bottom.value_or(LayoutUnit()),
                              offsets.left.value_or(LayoutUnit()));

  // Reduce the container size and adjust the offset based on the position-area.
  adjusted_container_info.rect.ContractEdges(
      converter.BlockStart(), converter.InlineEnd(), converter.BlockEnd(),
      converter.InlineStart());

  // For 'center' values (aligned with start and end anchor sides), the
  // containing block is aligned and sized with the anchor, regardless of
  // whether it's inside the original containing block or not. Otherwise,
  // ContractEdges above might have created a negative size if the position-area
  // is aligned with an anchor side outside the containing block.
  if (adjusted_container_info.rect.size.inline_size < LayoutUnit()) {
    DCHECK(converter.InlineStart() == LayoutUnit() ||
           converter.InlineEnd() == LayoutUnit())
        << "If aligned to both anchor edges, the size should never be "
           "negative.";
    // Collapse the inline size to 0 and align with the single anchor edge
    // defined by the position-area.
    if (converter.InlineStart() == LayoutUnit()) {
      DCHECK(converter.InlineEnd() != LayoutUnit());
      adjusted_container_info.rect.offset.inline_offset +=
          adjusted_container_info.rect.size.inline_size;
    }
    adjusted_container_info.rect.size.inline_size = LayoutUnit();
  }
  if (adjusted_container_info.rect.size.block_size < LayoutUnit()) {
    DCHECK(converter.BlockStart() == LayoutUnit() ||
           converter.BlockEnd() == LayoutUnit())
        << "If aligned to both anchor edges, the size should never be "
           "negative.";
    // Collapse the block size to 0 and align with the single anchor edge
    // defined by the position-area.
    if (converter.BlockStart() == LayoutUnit()) {
      DCHECK(converter.BlockEnd() != LayoutUnit());
      adjusted_container_info.rect.offset.block_offset +=
          adjusted_container_info.rect.size.block_size;
    }
    adjusted_container_info.rect.size.block_size = LayoutUnit();
  }
  return adjusted_container_info;
}

// Retrieve the stored ContainingBlockInfo needed for placing positioned nodes.
// When fragmenting, the ContainingBlockInfo is not stored ahead of time and
// must be generated on demand. The reason being that during fragmentation, we
// wait to place positioned nodes until they've reached the fragmentation
// context root. In such cases, we cannot use default |ContainingBlockInfo|
// since the fragmentation root is not the containing block of the positioned
// nodes. Rather, we must generate their ContainingBlockInfo based on the
// |candidate.containing_block.fragment|.
const OutOfFlowLayoutPart::ContainingBlockInfo
OutOfFlowLayoutPart::GetContainingBlockInfo(
    const LogicalOofPositionedNode& candidate) {
  const auto* container_object = container_builder_->GetLayoutObject();
  const auto& node_style = candidate.Node().Style();

  auto IsPlacedWithinGridArea = [&](const auto* containing_block) {
    if (!containing_block->IsLayoutGrid()) {
      return false;
    }

    return !node_style.GridColumnStart().IsAuto() ||
           !node_style.GridColumnEnd().IsAuto() ||
           !node_style.GridRowStart().IsAuto() ||
           !node_style.GridRowEnd().IsAuto();
  };

  auto GridAreaContainingBlockInfo =
      [&](const LayoutGrid& containing_grid, const GridLayoutData& layout_data,
          const BoxStrut& borders,
          const LogicalSize& size) -> OutOfFlowLayoutPart::ContainingBlockInfo {
    const auto& grid_style = containing_grid.StyleRef();
    GridItemData grid_item(candidate.Node(), grid_style);

    return {.writing_direction = grid_style.GetWritingDirection(),
            .rect = GridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
                containing_grid.CachedPlacementData(), layout_data, grid_style,
                borders, size, &grid_item)};
  };

  if (candidate.inline_container.container) {
    const auto it =
        containing_blocks_map_.find(candidate.inline_container.container);
    CHECK(it != containing_blocks_map_.end(), base::NotFatalUntil::M130);
    return it->value;
  }

  if (candidate.is_for_fragmentation) {
    LogicalOofNodeForFragmentation fragmentainer_descendant =
        To<LogicalOofNodeForFragmentation>(candidate);
    if (fragmentainer_descendant.containing_block.Fragment()) {
      DCHECK(container_builder_->IsBlockFragmentationContextRoot());

      const PhysicalFragment* containing_block_fragment =
          fragmentainer_descendant.containing_block.Fragment();
      const LayoutObject* containing_block =
          containing_block_fragment->GetLayoutObject();
      DCHECK(containing_block);

      bool is_placed_within_grid_area =
          IsPlacedWithinGridArea(containing_block);
      auto it = containing_blocks_map_.find(containing_block);
      if (it != containing_blocks_map_.end() && !is_placed_within_grid_area)
        return it->value;

      const auto writing_direction =
          containing_block->StyleRef().GetWritingDirection();
      LogicalSize size = containing_block_fragment->Size().ConvertToLogical(
          writing_direction.GetWritingMode());
      size.block_size =
          LayoutBoxUtils::TotalBlockSize(*To<LayoutBox>(containing_block));

      // TODO(1079031): This should eventually include scrollbar and border.
      BoxStrut border = To<PhysicalBoxFragment>(containing_block_fragment)
                            ->Borders()
                            .ConvertToLogical(writing_direction);

      if (is_placed_within_grid_area) {
        return GridAreaContainingBlockInfo(
            *To<LayoutGrid>(containing_block),
            *To<LayoutGrid>(containing_block)->LayoutData(), border, size);
      }

      LogicalSize content_size = ShrinkLogicalSize(size, border);
      LogicalOffset container_offset =
          LogicalOffset(border.inline_start, border.block_start);
      container_offset += fragmentainer_descendant.containing_block.Offset();

      ContainingBlockInfo containing_block_info{
          writing_direction, containing_block_fragment->IsScrollContainer(),
          LogicalRect(container_offset, content_size),
          fragmentainer_descendant.containing_block.RelativeOffset(),
          fragmentainer_descendant.containing_block.Offset()};

      return containing_blocks_map_
          .insert(containing_block, containing_block_info)
          .stored_value->value;
    }
  }

  if (IsPlacedWithinGridArea(container_object)) {
    return GridAreaContainingBlockInfo(
        *To<LayoutGrid>(container_object),
        container_builder_->GetGridLayoutData(), container_builder_->Borders(),
        {container_builder_->InlineSize(),
         container_builder_->FragmentBlockSize()});
  }

  return node_style.GetPosition() == EPosition::kAbsolute
             ? default_containing_block_info_for_absolute_
             : default_containing_block_info_for_fixed_;
}

void OutOfFlowLayoutPart::ComputeInlineContainingBlocks(
    const HeapVector<LogicalOofPositionedNode>& candidates) {
  InlineContainingBlockUtils::InlineContainingBlockMap
      inline_container_fragments;

  for (auto& candidate : candidates) {
    if (candidate.inline_container.container &&
        !inline_container_fragments.Contains(
            candidate.inline_container.container)) {
      InlineContainingBlockUtils::InlineContainingBlockGeometry
          inline_geometry = {};
      inline_container_fragments.insert(
          candidate.inline_container.container.Get(), inline_geometry);
    }
  }

  // Fetch the inline start/end fragment geometry.
  InlineContainingBlockUtils::ComputeInlineContainerGeometry(
      &inline_container_fragments, container_builder_);

  LogicalSize container_builder_size = container_builder_->Size();
  PhysicalSize container_builder_physical_size = ToPhysicalSize(
      container_builder_size, GetConstraintSpace().GetWritingMode());
  AddInlineContainingBlockInfo(
      inline_container_fragments,
      default_containing_block_info_for_absolute_.writing_direction,
      container_builder_physical_size);
}

void OutOfFlowLayoutPart::ComputeInlineContainingBlocksForFragmentainer(
    const HeapVector<LogicalOofNodeForFragmentation>& descendants) {
  struct InlineContainingBlockInfo {
    InlineContainingBlockUtils::InlineContainingBlockMap map;
    // The relative offset of the inline's containing block to the
    // fragmentation context root.
    LogicalOffset relative_offset;
    // The offset of the containing block relative to the fragmentation context
    // root (not including any relative offset).
    LogicalOffset offset_to_fragmentation_context;
  };

  HeapHashMap<Member<const LayoutBox>, InlineContainingBlockInfo>
      inline_containg_blocks;

  // Collect the inline containers by shared containing block.
  for (auto& descendant : descendants) {
    if (descendant.inline_container.container) {
      DCHECK(descendant.containing_block.Fragment());
      const LayoutBox* containing_block = To<LayoutBox>(
          descendant.containing_block.Fragment()->GetLayoutObject());

      InlineContainingBlockUtils::InlineContainingBlockGeometry
          inline_geometry = {};
      inline_geometry.relative_offset =
          descendant.inline_container.relative_offset;
      auto it = inline_containg_blocks.find(containing_block);
      if (it != inline_containg_blocks.end()) {
        if (!it->value.map.Contains(descendant.inline_container.container)) {
          it->value.map.insert(descendant.inline_container.container.Get(),
                               inline_geometry);
        }
        continue;
      }
      InlineContainingBlockUtils::InlineContainingBlockMap inline_containers;
      inline_containers.insert(descendant.inline_container.container.Get(),
                               inline_geometry);
      InlineContainingBlockInfo inline_info{
          inline_containers, descendant.containing_block.RelativeOffset(),
          descendant.containing_block.Offset()};
      inline_containg_blocks.insert(containing_block, inline_info);
    }
  }

  for (auto& inline_containg_block : inline_containg_blocks) {
    const LayoutBox* containing_block = inline_containg_block.key;
    InlineContainingBlockInfo& inline_info = inline_containg_block.value;

    LogicalSize size(LayoutBoxUtils::InlineSize(*containing_block),
                     LayoutBoxUtils::TotalBlockSize(*containing_block));
    PhysicalSize container_builder_physical_size =
        ToPhysicalSize(size, containing_block->StyleRef().GetWritingMode());

    // Fetch the inline start/end fragment geometry.
    InlineContainingBlockUtils::ComputeInlineContainerGeometryForFragmentainer(
        containing_block, container_builder_physical_size, &inline_info.map);

    AddInlineContainingBlockInfo(
        inline_info.map, containing_block->StyleRef().GetWritingDirection(),
        container_builder_physical_size, inline_info.relative_offset,
        inline_info.offset_to_fragmentation_context,
        /* adjust_for_fragmentation */ true);
  }
}

void OutOfFlowLayoutPart::AddInlineContainingBlockInfo(
    const InlineContainingBlockUtils::InlineContainingBlockMap&
        inline_container_fragments,
    const WritingDirectionMode container_writing_direction,
    PhysicalSize container_builder_size,
    LogicalOffset containing_block_relative_offset,
    LogicalOffset containing_block_offset,
    bool adjust_for_fragmentation) {
  // Transform the start/end fragments into a ContainingBlockInfo.
  for (const auto& block_info : inline_container_fragments) {
    DCHECK(block_info.value.has_value());

    // The calculation below determines the size of the inline containing block
    // rect.
    //
    // To perform this calculation we:
    // 1. Determine the start_offset "^", this is at the logical-start (wrt.
    //    default containing block), of the start fragment rect.
    // 2. Determine the end_offset "$", this is at the logical-end (wrt.
    //    default containing block), of the end  fragment rect.
    // 3. Determine the logical rectangle defined by these two offsets.
    //
    // Case 1a: Same direction, overlapping fragments.
    //      +---------------
    // ---> |^*****-------->
    //      +*----*---------
    //       *    *
    // ------*----*+
    // ----> *****$| --->
    // ------------+
    //
    // Case 1b: Different direction, overlapping fragments.
    //      +---------------
    // ---> ^******* <-----|
    //      *------*--------
    //      *      *
    // -----*------*
    // |<-- *******$ --->
    // ------------+
    //
    // Case 2a: Same direction, non-overlapping fragments.
    //             +--------
    // --------->  |^ ----->
    //             +*-------
    //              *
    // --------+    *
    // ------->|    $ --->
    // --------+
    //
    // Case 2b: Same direction, non-overlapping fragments.
    //             +--------
    // --------->  ^ <-----|
    //             *--------
    //             *
    // --------+   *
    // | <------   $  --->
    // --------+
    //
    // Note in cases [1a, 2a] we need to account for the inline borders of the
    // rectangles, where-as in [1b, 2b] we do not. This is handled by the
    // is_same_direction check(s).
    //
    // Note in cases [2a, 2b] we don't allow a "negative" containing block size,
    // we clamp negative sizes to zero.
    const ComputedStyle* inline_cb_style = block_info.key->Style();
    DCHECK(inline_cb_style);

    const auto inline_writing_direction =
        inline_cb_style->GetWritingDirection();
    BoxStrut inline_cb_borders = ComputeBordersForInline(*inline_cb_style);
    DCHECK_EQ(container_writing_direction.GetWritingMode(),
              inline_writing_direction.GetWritingMode());

    bool is_same_direction =
        container_writing_direction == inline_writing_direction;

    // Step 1 - determine the start_offset.
    const PhysicalRect& start_rect =
        block_info.value->start_fragment_union_rect;
    LogicalOffset start_offset = start_rect.offset.ConvertToLogical(
        container_writing_direction, container_builder_size, start_rect.size);

    // Make sure we add the inline borders, we don't need to do this in the
    // inline direction if the blocks are in opposite directions.
    start_offset.block_offset += inline_cb_borders.block_start;
    if (is_same_direction)
      start_offset.inline_offset += inline_cb_borders.inline_start;

    // Step 2 - determine the end_offset.
    const PhysicalRect& end_rect = block_info.value->end_fragment_union_rect;
    LogicalOffset end_offset = end_rect.offset.ConvertToLogical(
        container_writing_direction, container_builder_size, end_rect.size);

    // Add in the size of the fragment to get the logical end of the fragment.
    end_offset += end_rect.size.ConvertToLogical(
        container_writing_direction.GetWritingMode());

    // Make sure we subtract the inline borders, we don't need to do this in the
    // inline direction if the blocks are in opposite directions.
    end_offset.block_offset -= inline_cb_borders.block_end;
    if (is_same_direction)
      end_offset.inline_offset -= inline_cb_borders.inline_end;

    // Make sure we don't end up with a rectangle with "negative" size.
    end_offset.inline_offset =
        std::max(end_offset.inline_offset, start_offset.inline_offset);
    end_offset.block_offset =
        std::max(end_offset.block_offset, start_offset.block_offset);

    // Step 3 - determine the logical rectangle.

    // Determine the logical size of the containing block.
    LogicalSize inline_cb_size = {
        end_offset.inline_offset - start_offset.inline_offset,
        end_offset.block_offset - start_offset.block_offset};
    DCHECK_GE(inline_cb_size.inline_size, LayoutUnit());
    DCHECK_GE(inline_cb_size.block_size, LayoutUnit());

    if (adjust_for_fragmentation) {
      // When fragmenting, the containing block will not be associated with the
      // current builder. Thus, we need to adjust the start offset to take the
      // writing mode of the builder into account.
      PhysicalSize physical_size =
          ToPhysicalSize(inline_cb_size, GetConstraintSpace().GetWritingMode());
      start_offset =
          start_offset
              .ConvertToPhysical(container_writing_direction,
                                 container_builder_size, physical_size)
              .ConvertToLogical(GetConstraintSpace().GetWritingDirection(),
                                container_builder_size, physical_size);
    }

    // Subtract out the inline relative offset, if set, so that it can be
    // applied after fragmentation is performed on the fragmentainer
    // descendants.
    DCHECK((block_info.value->relative_offset == LogicalOffset() &&
            containing_block_relative_offset == LogicalOffset() &&
            containing_block_offset == LogicalOffset()) ||
           container_builder_->IsBlockFragmentationContextRoot());
    LogicalOffset container_offset =
        start_offset - block_info.value->relative_offset;
    LogicalOffset total_relative_offset =
        containing_block_relative_offset + block_info.value->relative_offset;

    // The offset of the container is currently relative to the containing
    // block. Add the offset of the containng block to the fragmentation context
    // root so that it is relative to the fragmentation context root, instead.
    container_offset += containing_block_offset;

    // If an OOF has an inline containing block, the OOF offset that is written
    // back to legacy is relative to the containing block of the inline rather
    // than the inline itself. |containing_block_offset| will be used when
    // calculating this OOF offset. However, there may be some relative offset
    // between the containing block and the inline container that should be
    // included in the final OOF offset that is written back to legacy. Adjust
    // for that relative offset here.
    containing_blocks_map_.insert(
        block_info.key.Get(),
        ContainingBlockInfo{
            inline_writing_direction,
            /* is_scroll_container */ false,
            LogicalRect(container_offset, inline_cb_size),
            total_relative_offset,
            containing_block_offset - block_info.value->relative_offset});
  }
}

void OutOfFlowLayoutPart::LayoutCandidates(
    HeapVector<LogicalOofPositionedNode>* candidates) {
  while (candidates->size() > 0) {
    if (!has_block_fragmentation_ ||
        container_builder_->IsInitialColumnBalancingPass()) {
      ComputeInlineContainingBlocks(*candidates);
    }
    for (auto& candidate : *candidates) {
      LayoutBox* layout_box = candidate.box;
      if (!container_builder_->IsBlockFragmentationContextRoot()) {
        SaveStaticPositionOnPaintLayer(layout_box, candidate.static_position);
      }
      if (IsContainingBlockForCandidate(candidate)) {
        if (has_block_fragmentation_) {
          container_builder_->SetHasOutOfFlowInFragmentainerSubtree(true);
          if (!container_builder_->IsInitialColumnBalancingPass()) {
            LogicalOofNodeForFragmentation fragmentainer_descendant(candidate);
            container_builder_->AdjustFragmentainerDescendant(
                fragmentainer_descendant);
            container_builder_
                ->AdjustFixedposContainingBlockForInnerMulticols();
            container_builder_->AddOutOfFlowFragmentainerDescendant(
                fragmentainer_descendant);
            continue;
          }
        }

        NodeInfo node_info = SetupNodeInfo(candidate);
        NodeToLayout node_to_layout = {node_info, CalculateOffset(node_info)};
        const LayoutResult* result = LayoutOOFNode(node_to_layout);
        PhysicalBoxStrut physical_margins =
            node_to_layout.offset_info.node_dimensions.margins
                .ConvertToPhysical(
                    node_info.node.Style().GetWritingDirection());
        BoxStrut margins = physical_margins.ConvertToLogical(
            container_builder_->GetWritingDirection());
        container_builder_->AddResult(
            *result, result->OutOfFlowPositionedOffset(), margins,
            /* relative_offset */ std::nullopt, &candidate.inline_container);
        container_builder_->SetHasOutOfFlowFragmentChild(true);
        if (container_builder_->IsInitialColumnBalancingPass()) {
          container_builder_->PropagateTallestUnbreakableBlockSize(
              result->TallestUnbreakableBlockSize());
        }
      } else {
        container_builder_->AddOutOfFlowDescendant(candidate);
      }
    }

    // Sweep any candidates that might have been added.
    // This happens when an absolute container has a fixed child.
    candidates->Shrink(0);
    container_builder_->SwapOutOfFlowPositionedCandidates(candidates);
  }
}

void OutOfFlowLayoutPart::HandleMulticolsWithPendingOOFs(
    BoxFragmentBuilder* container_builder) {
  if (!container_builder->HasMulticolsWithPendingOOFs())
    return;

  FragmentBuilder::MulticolCollection multicols_handled;
  FragmentBuilder::MulticolCollection multicols_with_pending_oofs;
  container_builder->SwapMulticolsWithPendingOOFs(&multicols_with_pending_oofs);
  DCHECK(!multicols_with_pending_oofs.empty());

  while (!multicols_with_pending_oofs.empty()) {
    for (auto& multicol : multicols_with_pending_oofs) {
      DCHECK(!multicols_handled.Contains(multicol.key));
      LayoutOOFsInMulticol(BlockNode(multicol.key), multicol.value);
      multicols_handled.insert(multicol.key, multicol.value);
    }
    multicols_with_pending_oofs.clear();

    // Additional inner multicols may have been added while handling outer
    // ones. Add those that we haven't seen yet, and handle them.
    FragmentBuilder::MulticolCollection new_multicols;
    container_builder->SwapMulticolsWithPendingOOFs(&new_multicols);
    for (auto& multicol : new_multicols) {
      if (!multicols_handled.Contains(multicol.key)) {
        multicols_with_pending_oofs.insert(multicol.key, multicol.value);
      }
    }
  }
}

void OutOfFlowLayoutPart::LayoutOOFsInMulticol(
    const BlockNode& multicol,
    const MulticolWithPendingOofs<LogicalOffset>* multicol_info) {
  HeapVector<LogicalOofNodeForFragmentation> oof_nodes_to_layout;
  ClearCollectionScope<HeapVector<LogicalOofNodeForFragmentation>>
      oof_nodes_scope(&oof_nodes_to_layout);
  HeapVector<MulticolChildInfo> multicol_children;
  ClearCollectionScope<HeapVector<MulticolChildInfo>> multicol_scope(
      &multicol_children);

  const BlockBreakToken* current_column_break_token = nullptr;
  const BlockBreakToken* previous_multicol_break_token = nullptr;

  LayoutUnit column_inline_progression = kIndefiniteSize;
  LogicalOffset multicol_offset = multicol_info->multicol_offset;

  // Create a simplified container builder for multicol children. It cannot be
  // used to generate a fragment (since no size has been set, for one), but is
  // suitable for holding child fragmentainers while we're cloning them.
  ConstraintSpace limited_multicol_constraint_space =
      CreateConstraintSpaceForMulticol(multicol);
  FragmentGeometry limited_fragment_geometry = CalculateInitialFragmentGeometry(
      limited_multicol_constraint_space, multicol, /* break_token */ nullptr);
  BoxFragmentBuilder limited_multicol_container_builder =
      CreateContainerBuilderForMulticol(multicol,
                                        limited_multicol_constraint_space,
                                        limited_fragment_geometry);
  // The block size that we set on the multicol builder doesn't matter since
  // we only care about the size of the fragmentainer children when laying out
  // the remaining OOFs.
  limited_multicol_container_builder.SetFragmentsTotalBlockSize(LayoutUnit());

  WritingDirectionMode writing_direction =
      multicol.Style().GetWritingDirection();
  const PhysicalBoxFragment* last_fragment_with_fragmentainer = nullptr;

  // Accumulate all of the pending OOF positioned nodes that are stored inside
  // |multicol|.
  for (auto& multicol_fragment : multicol.GetLayoutBox()->PhysicalFragments()) {
    const auto* multicol_box_fragment =
        To<PhysicalBoxFragment>(&multicol_fragment);

    const ComputedStyle& style = multicol_box_fragment->Style();
    const WritingModeConverter converter(writing_direction,
                                         multicol_box_fragment->Size());
    wtf_size_t current_column_index = kNotFound;

    if (column_inline_progression == kIndefiniteSize) {
      // TODO(almaher): This should eventually include scrollbar, as well.
      BoxStrut border_padding =
          multicol_box_fragment->Borders().ConvertToLogical(writing_direction) +
          multicol_box_fragment->Padding().ConvertToLogical(writing_direction);
      LayoutUnit available_inline_size =
          multicol_box_fragment->Size()
              .ConvertToLogical(writing_direction.GetWritingMode())
              .inline_size -
          border_padding.InlineSum();
      column_inline_progression =
          ColumnInlineProgression(available_inline_size, style);
    }

    // Collect the children of the multicol fragments.
    for (auto& child :
         multicol_box_fragment->GetMutableChildrenForOutOfFlow().Children()) {
      const auto* fragment = child.get();
      LogicalOffset offset =
          converter.ToLogical(child.Offset(), fragment->Size());
      if (fragment->IsFragmentainerBox()) {
        current_column_break_token =
            To<BlockBreakToken>(fragment->GetBreakToken());
        current_column_index = multicol_children.size();
        last_fragment_with_fragmentainer = multicol_box_fragment;
      }

      limited_multicol_container_builder.AddChild(*fragment, offset);
      multicol_children.emplace_back(MulticolChildInfo());
    }

    // If a column fragment is updated with OOF children, we may need to update
    // the reference to its break token in its parent's break token. There
    // should be at most one column break token per parent break token
    // (representing the last column laid out in that fragment). Thus, search
    // for |current_column_break_token| in |multicol_box_fragment|'s list of
    // child break tokens and update the stored MulticolChildInfo if found.
    const BlockBreakToken* break_token = multicol_box_fragment->GetBreakToken();
    if (current_column_index != kNotFound && break_token &&
        break_token->ChildBreakTokens().size()) {
      // If there is a column break token, it will be the last item in its
      // parent's list of break tokens.
      const auto children = break_token->ChildBreakTokens();
      const BlockBreakToken* child_token =
          To<BlockBreakToken>(children[children.size() - 1].Get());
      if (child_token == current_column_break_token) {
        MulticolChildInfo& child_info = multicol_children[current_column_index];
        child_info.parent_break_token = break_token;
      }
    }

    // Convert the OOF fragmentainer descendants to the logical coordinate space
    // and store the resulting nodes inside |oof_nodes_to_layout|.
    HeapVector<LogicalOofNodeForFragmentation> oof_fragmentainer_descendants;
    limited_multicol_container_builder.SwapOutOfFlowFragmentainerDescendants(
        &oof_fragmentainer_descendants);
    for (const auto& descendant : oof_fragmentainer_descendants) {
      if (oof_nodes_to_layout.empty() &&
          multicol_info->fixedpos_containing_block.Fragment() &&
          previous_multicol_break_token) {
        // At this point, the multicol offset is the offset from the fixedpos
        // containing block to the first multicol fragment holding OOF
        // fragmentainer descendants. Update this offset such that it is the
        // offset from the fixedpos containing block to the top of the first
        // multicol fragment.
        multicol_offset.block_offset -=
            previous_multicol_break_token->ConsumedBlockSize();
      }

      // If the containing block is not set, that means that the inner multicol
      // was its containing block, and the OOF will be laid out elsewhere. Also
      // skip descendants whose containing block is a column spanner, because
      // those need to be laid out further up in the tree.
      if (!descendant.containing_block.Fragment() ||
          descendant.containing_block.IsInsideColumnSpanner()) {
        continue;
      }
      oof_nodes_to_layout.push_back(descendant);
    }
    previous_multicol_break_token = break_token;
  }
  // When an OOF's CB is a spanner (or a descendant of a spanner), we will lay
  // out the OOF at the next fragmentation context root ancestor. As such, we
  // remove any such OOF nodes from the nearest multicol's list of OOF
  // descendants during OOF node propagation, which may cause
  // |oof_nodes_to_layout| to be empty. Return early if this is the case.
  if (oof_nodes_to_layout.empty())
    return;

  DCHECK(!limited_multicol_container_builder
              .HasOutOfFlowFragmentainerDescendants());

  // Any candidates in the children of the inner multicol have already been
  // propagated properly when the inner multicol was laid out.
  //
  // During layout of the OOF positioned descendants, which is about to take
  // place, new candidates may be discovered (when there's a fixedpos inside an
  // abspos, for instance), that will be transferred to the actual fragment
  // builder further below.
  limited_multicol_container_builder.ClearOutOfFlowPositionedCandidates();

  wtf_size_t old_fragment_count =
      limited_multicol_container_builder.Children().size();

  LogicalOffset fragmentainer_progression(column_inline_progression,
                                          LayoutUnit());

  // Layout the OOF positioned elements inside the inner multicol.
  OutOfFlowLayoutPart inner_part(&limited_multicol_container_builder);
  inner_part.outer_container_builder_ =
      outer_container_builder_ ? outer_container_builder_ : container_builder_;
  inner_part.LayoutFragmentainerDescendants(
      &oof_nodes_to_layout, fragmentainer_progression,
      multicol_info->fixedpos_containing_block.Fragment(), &multicol_children);

  wtf_size_t new_fragment_count =
      limited_multicol_container_builder.Children().size();

  if (old_fragment_count != new_fragment_count) {
    DCHECK_GT(new_fragment_count, old_fragment_count);
    // We created additional fragmentainers to hold OOFs, and this is in a
    // nested fragmentation context. This means that the multicol fragment has
    // already been created, and we will therefore need to replace one of those
    // fragments. Locate the last multicol container fragment that already has
    // fragmentainers, and append all new fragmentainers there. Note that this
    // means that we may end up with more inner fragmentainers than what we
    // actually have room for (so that they'll overflow in the inline
    // direction), because we don't attempt to put fragmentainers into
    // additional multicol fragments in outer fragmentainers. This is an
    // implementation limitation which we can hopefully live with.
    DCHECK(last_fragment_with_fragmentainer);
    LayoutBox& box = *last_fragment_with_fragmentainer->MutableOwnerLayoutBox();
    wtf_size_t fragment_count = box.PhysicalFragmentCount();
    DCHECK_GE(fragment_count, 1u);
    const LayoutResult* old_result = nullptr;
    wtf_size_t fragment_idx = fragment_count - 1;
    do {
      old_result = box.GetLayoutResult(fragment_idx);
      if (&old_result->GetPhysicalFragment() ==
          last_fragment_with_fragmentainer) {
        break;
      }
      DCHECK_GT(fragment_idx, 0u);
      fragment_idx--;
    } while (true);

    // We have located the right multicol container fragment to update.
    const auto& existing_fragment =
        To<PhysicalBoxFragment>(old_result->GetPhysicalFragment());
    WritingModeConverter converter(
        existing_fragment.Style().GetWritingDirection(),
        existing_fragment.Size());
    LayoutUnit additional_column_block_size;

    // Append the new fragmentainers to the multicol container fragment.
    auto fragment_mutator = existing_fragment.GetMutableForOofFragmentation();
    for (wtf_size_t i = old_fragment_count; i < new_fragment_count; i++) {
      const LogicalFragmentLink& child =
          limited_multicol_container_builder.Children()[i];
      fragment_mutator.AddChildFragmentainer(
          *To<PhysicalBoxFragment>(child.get()), child.offset);
      additional_column_block_size +=
          converter.ToLogical(child.fragment->Size()).block_size;
    }
    fragment_mutator.UpdateOverflow();

    // We've already written back to legacy for |multicol|, but if we added
    // new columns to hold any OOF descendants, we need to extend the final
    // size of the legacy flow thread to encompass those new columns.
    multicol.MakeRoomForExtraColumns(additional_column_block_size);
  }

  // Any descendants should have been handled in
  // LayoutFragmentainerDescendants(). However, if there were any candidates
  // found, pass them back to |container_builder_| so they can continue
  // propagating up the tree.
  DCHECK(
      !limited_multicol_container_builder.HasOutOfFlowPositionedDescendants());
  DCHECK(!limited_multicol_container_builder
              .HasOutOfFlowFragmentainerDescendants());
  limited_multicol_container_builder.TransferOutOfFlowCandidates(
      container_builder_, multicol_offset, multicol_info);

  // Add any inner multicols with OOF descendants that may have propagated up
  // while laying out the direct OOF descendants of the current multicol.
  FragmentBuilder::MulticolCollection multicols_with_pending_oofs;
  limited_multicol_container_builder.SwapMulticolsWithPendingOOFs(
      &multicols_with_pending_oofs);
  for (auto& descendant : multicols_with_pending_oofs) {
    container_builder_->AddMulticolWithPendingOOFs(BlockNode(descendant.key),
                                                   descendant.value);
  }
}

void OutOfFlowLayoutPart::LayoutFragmentainerDescendants(
    HeapVector<LogicalOofNodeForFragmentation>* descendants,
    LogicalOffset fragmentainer_progression,
    bool outer_context_has_fixedpos_container,
    HeapVector<MulticolChildInfo>* multicol_children) {
  multicol_children_ = multicol_children;
  outer_context_has_fixedpos_container_ = outer_context_has_fixedpos_container;
  DCHECK(multicol_children_ || !outer_context_has_fixedpos_container_);

  BoxFragmentBuilder* builder_for_anchor_query = container_builder_;
  if (outer_container_builder_) {
    // If this is an inner layout of the nested block fragmentation, and if this
    // block fragmentation context is block fragmented, |multicol_children|
    // doesn't have correct block offsets of fragmentainers anchor query needs.
    // Calculate the anchor query from the outer block fragmentation context
    // instead in order to get the correct offsets.
    for (const MulticolChildInfo& multicol_child : *multicol_children) {
      if (multicol_child.parent_break_token) {
        builder_for_anchor_query = outer_container_builder_;
        break;
      }
    }
  }
  LogicalAnchorQueryMap stitched_anchor_queries(
      *builder_for_anchor_query->Node().GetLayoutBox(),
      builder_for_anchor_query->Children(),
      builder_for_anchor_query->GetWritingDirection());

  const bool may_have_anchors_on_oof =
      std::any_of(descendants->begin(), descendants->end(),
                  [](const LogicalOofPositionedNode& node) {
                    return node.box->MayHaveAnchorQuery();
                  });

  HeapVector<HeapVector<NodeToLayout>> descendants_to_layout;
  ClearCollectionScope<HeapVector<HeapVector<NodeToLayout>>>
      descendants_to_layout_scope(&descendants_to_layout);

  // List of repeated fixed-positioned elements. Elements will be added as they
  // are discovered (which might not happen in the first iteration, if they are
  // nested inside another OOFs).
  HeapVector<NodeToLayout> repeated_fixedpos_descendants;
  ClearCollectionScope<HeapVector<NodeToLayout>>
      repeated_fixedpos_descendants_scope(&repeated_fixedpos_descendants);

  // The fragmentainer index at which we have to resume repetition of
  // fixed-positioned elements, if additional fragmentainers are added. We'll
  // add repeated elements to every fragmentainer that exists, but if there's a
  // nested OOF that triggers creation of additional fragmentainers, we'll need
  // to add the fixed-positioned elements to those as well.
  wtf_size_t previous_repeaded_fixedpos_resume_idx = WTF::kNotFound;

  while (descendants->size() > 0) {
    ComputeInlineContainingBlocksForFragmentainer(*descendants);

    // When there are anchor queries, each containing block should be laid out
    // separately. This loop chunks |descendants| by their containing blocks, if
    // they have anchor queries.
    base::span<LogicalOofNodeForFragmentation> descendants_span =
        base::make_span(*descendants);
    for (;;) {
      bool has_new_descendants_span = false;
      // The CSS containing block of the last descendant, to group |descendants|
      // by the CSS containing block.
      const LayoutObject* last_css_containing_block = nullptr;

      // Sort the descendants by fragmentainer index in |descendants_to_layout|.
      // This will ensure that the descendants are laid out in the correct
      // order.
      DCHECK(!descendants_span.empty());
      for (auto& descendant : descendants_span) {
        if (GetFragmentainerType() == kFragmentColumn) {
          auto* containing_block = To<LayoutBox>(
              descendant.containing_block.Fragment()->GetLayoutObject());
          DCHECK(containing_block);

          // We may try to lay out an OOF once we reach a column spanner or when
          // column balancing. However, if the containing block has not finished
          // layout, we should wait to lay out the OOF in case its position is
          // dependent on its containing block's final size.
          if (containing_block->PhysicalFragments().back().GetBreakToken()) {
            delayed_descendants_.push_back(descendant);
            continue;
          }
        }

        // Ensure each containing block is laid out before laying out other
        // containing blocks. The CSS Anchor Positioning may evaluate
        // differently when the containing block is different, and may refer to
        // other containing blocks that were already laid out.
        //
        // Do this only when needed, because doing so may rebuild fragmentainers
        // multiple times, which can hit the performance when there are many
        // containing blocks in the block formatting context.
        //
        // Use |LayoutObject::Container|, not |LayoutObject::ContainingBlock|.
        // The latter is not the CSS containing block for inline boxes. See the
        // comment of |LayoutObject::ContainingBlock|.
        //
        // Note |descendant.containing_block.fragment| is |ContainingBlock|, not
        // the CSS containing block.
        if (!stitched_anchor_queries.IsEmpty() || may_have_anchors_on_oof) {
          const LayoutObject* css_containing_block =
              descendant.box->Container();
          DCHECK(css_containing_block);
          if (css_containing_block != last_css_containing_block) {
            // Chunking the layout of OOFs by the containing blocks is done only
            // if it has anchor query, for the performance reasons to minimize
            // the number of rebuilding fragmentainer fragments.
            if (last_css_containing_block &&
                (last_css_containing_block->MayHaveAnchorQuery() ||
                 may_have_anchors_on_oof)) {
              has_new_descendants_span = true;
              descendants_span = descendants_span.subspan(
                  &descendant - descendants_span.data());
              break;
            }
            last_css_containing_block = css_containing_block;
          }
        }

        NodeInfo node_info = SetupNodeInfo(descendant);
        NodeToLayout node_to_layout = {
            node_info, CalculateOffset(node_info, &stitched_anchor_queries)};
        node_to_layout.containing_block_fragment =
            descendant.containing_block.Fragment();
        node_to_layout.offset_info.original_offset =
            node_to_layout.offset_info.offset;

        DCHECK(node_to_layout.offset_info.block_estimate);

        // Determine in which fragmentainer this OOF element will start its
        // layout and adjust the offset to be relative to that fragmentainer.
        wtf_size_t start_index = 0;
        ComputeStartFragmentIndexAndRelativeOffset(
            node_info.default_writing_direction.GetWritingMode(),
            *node_to_layout.offset_info.block_estimate,
            node_info.containing_block.ClippedContainerBlockOffset(),
            &start_index, &node_to_layout.offset_info.offset);
        if (start_index >= descendants_to_layout.size())
          descendants_to_layout.resize(start_index + 1);
        descendants_to_layout[start_index].emplace_back(node_to_layout);
      }

      HeapVector<NodeToLayout> fragmented_descendants;
      ClearCollectionScope<HeapVector<NodeToLayout>>
          fragmented_descendants_scope(&fragmented_descendants);
      fragmentainer_consumed_block_size_ = LayoutUnit();

      // Even if all OOFs are done creating fragments, we need to create enough
      // fragmentainers to encompass all monolithic overflow when printing.
      LayoutUnit monolithic_overflow;

      // Set to true if an OOF inside a fragmentainer breaks. This does not
      // include repeated fixed-positioned elements.
      bool last_fragmentainer_has_break_inside = false;

      // Layout the OOF descendants in order of fragmentainer index.
      for (wtf_size_t index = 0; index < descendants_to_layout.size();
           index++) {
        const PhysicalBoxFragment* fragment = nullptr;
        if (index < ChildCount()) {
          fragment = &GetChildFragment(index);
        } else if (column_balancing_info_) {
          column_balancing_info_->num_new_columns++;
        }

        // Skip over any column spanners.
        if (!fragment || fragment->IsFragmentainerBox()) {
          HeapVector<NodeToLayout>& pending_descendants =
              descendants_to_layout[index];

          if (!repeated_fixedpos_descendants.empty() &&
              index == previous_repeaded_fixedpos_resume_idx) {
            // This is a new fragmentainer, and we had previously added repeated
            // fixed-positioned elements to all preceding fragmentainers (in a
            // previous iteration; this may happen when there are nested OOFs).
            // We now need to make sure that we add the repeated
            // fixed-positioned elements to all new fragmentainers as well.
            fragmented_descendants.PrependVector(repeated_fixedpos_descendants);
            // We need to clear the vector, since we'll find and re-add all the
            // repeated elements (both these, and any new ones discovered) in
            // fragmented_descendants when we're done with the current loop.
            repeated_fixedpos_descendants.clear();
          }

          bool has_oofs_in_later_fragmentainer =
              index + 1 < descendants_to_layout.size();
          last_fragmentainer_has_break_inside = false;
          LayoutOOFsInFragmentainer(
              pending_descendants, index, fragmentainer_progression,
              has_oofs_in_later_fragmentainer, &monolithic_overflow,
              &last_fragmentainer_has_break_inside, &fragmented_descendants);

          // Retrieve the updated or newly added fragmentainer, and add its
          // block contribution to the consumed block size. Skip this if we are
          // column balancing, though, since this is only needed when adding
          // OOFs to the builder in the true layout pass.
          if (!column_balancing_info_) {
            fragment = &GetChildFragment(index);
            fragmentainer_consumed_block_size_ +=
                fragment->Size()
                    .ConvertToLogical(
                        container_builder_->Style().GetWritingMode())
                    .block_size;
          }
        }

        // Extend |descendants_to_layout| if an OOF element fragments into a
        // fragmentainer at an index that does not yet exist in
        // |descendants_to_layout|. We also need to do this if there's
        // monolithic overflow (when printing), so that there are enough
        // fragmentainers to paint the overflow. At the same time we need to
        // make sure that repeated fixed-positioned elements don't trigger
        // creation of additional fragmentainers on their own (since they'd just
        // repeat forever).
        if (index == descendants_to_layout.size() - 1 &&
            (last_fragmentainer_has_break_inside ||
             monolithic_overflow > LayoutUnit() ||
             (!fragmented_descendants.empty() && index + 1 < ChildCount()))) {
          descendants_to_layout.resize(index + 2);
        }
      }

      if (!fragmented_descendants.empty()) {
        // We have repeated fixed-positioned elements. If we add more
        // fragmentainers in the next iteration (because of nested OOFs), we
        // need to resume those when a new fragmentainer is added.
        DCHECK(container_builder_->Node().IsPaginatedRoot());
        DCHECK(previous_repeaded_fixedpos_resume_idx == WTF::kNotFound ||
               previous_repeaded_fixedpos_resume_idx <=
                   descendants_to_layout.size());
        previous_repeaded_fixedpos_resume_idx = descendants_to_layout.size();

        // Add all repeated fixed-positioned elements to a list that we'll
        // consume if we add more fragmentainers in a subsequent iteration
        // (because of nested OOFs), so that we keep on generating fragments for
        // the repeated fixed-positioned elements in the new fragmentainers as
        // well.
        repeated_fixedpos_descendants.AppendVector(fragmented_descendants);
      }
      descendants_to_layout.Shrink(0);

      if (!has_new_descendants_span)
        break;
      // If laying out by containing blocks and there are more containing blocks
      // to be laid out, move on to the next containing block. Before laying
      // them out, if OOFs have anchors, update the anchor queries.
      if (may_have_anchors_on_oof) {
        stitched_anchor_queries.SetChildren(
            builder_for_anchor_query->Children());
      }
    }

    // Sweep any descendants that might have been bubbled up from the fragment
    // to the |container_builder_|. This happens when we have nested absolute
    // position elements.
    //
    // Don't do this if we are in a column balancing pass, though, since we
    // won't propagate OOF info of nested OOFs in this case. Any OOFs already
    // added to the builder should remain there so that they can be handled
    // later.
    descendants->Shrink(0);
    if (!column_balancing_info_)
      container_builder_->SwapOutOfFlowFragmentainerDescendants(descendants);
  }

  if (container_builder_->Node().IsPaginatedRoot()) {
    // Finish repeated fixed-positioned elements.
    for (const NodeToLayout& node_to_layout : repeated_fixedpos_descendants) {
      const BlockNode& node = node_to_layout.node_info.node;
      DCHECK_EQ(node.Style().GetPosition(), EPosition::kFixed);
      node.FinishRepeatableRoot();
    }
  } else {
    DCHECK(repeated_fixedpos_descendants.empty());
  }
}

AnchorEvaluatorImpl OutOfFlowLayoutPart::CreateAnchorEvaluator(
    const ContainingBlockInfo& container_info,
    const BlockNode& candidate,
    const LogicalAnchorQueryMap* anchor_queries) const {
  const LayoutObject* implicit_anchor = nullptr;
  const LayoutBox& candidate_layout_box = *candidate.GetLayoutBox();
  if (const Element* element =
          DynamicTo<Element>(candidate_layout_box.GetNode())) {
    if (const Element* implicit_anchor_element =
            element->ImplicitAnchorElement()) {
      implicit_anchor = implicit_anchor_element->GetLayoutObject();
    }
  }

  LogicalSize container_content_size = container_info.rect.size;
  PhysicalSize container_physical_content_size = ToPhysicalSize(
      container_content_size, GetConstraintSpace().GetWritingMode());
  WritingDirectionMode self_writing_direction =
      candidate.Style().GetWritingDirection();
  const WritingModeConverter container_converter(
      container_info.writing_direction, container_physical_content_size);
  if (anchor_queries) {
    // When the containing block is block-fragmented, the |container_builder_|
    // is the fragmentainer, not the containing block, and the coordinate system
    // is stitched. Use the given |anchor_query|.
    const LayoutObject* css_containing_block = candidate_layout_box.Container();
    CHECK(css_containing_block);
    return AnchorEvaluatorImpl(
        candidate_layout_box, *anchor_queries, implicit_anchor,
        *css_containing_block, container_converter, self_writing_direction,
        container_converter.ToPhysical(container_info.rect).offset,
        container_physical_content_size);
  }
  if (const LogicalAnchorQuery* anchor_query =
          container_builder_->AnchorQuery()) {
    // Otherwise the |container_builder_| is the containing block.
    return AnchorEvaluatorImpl(
        candidate_layout_box, *anchor_query, implicit_anchor,
        container_converter, self_writing_direction,
        container_converter.ToPhysical(container_info.rect).offset,
        container_physical_content_size);
  }
  return AnchorEvaluatorImpl();
}

OutOfFlowLayoutPart::NodeInfo OutOfFlowLayoutPart::SetupNodeInfo(
    const LogicalOofPositionedNode& oof_node) {
  BlockNode node = oof_node.Node();
  const PhysicalFragment* containing_block_fragment =
      oof_node.is_for_fragmentation
          ? To<LogicalOofNodeForFragmentation>(oof_node)
                .containing_block.Fragment()
          : nullptr;

#if DCHECK_IS_ON()
  const LayoutObject* container =
      containing_block_fragment ? containing_block_fragment->GetLayoutObject()
                                : container_builder_->GetLayoutObject();

  if (container) {
    // "OutOfFlowLayoutPart container is ContainingBlock" invariant cannot be
    // enforced for tables. Tables are special, in that the ContainingBlock is
    // TABLE, but constraint space is generated by TBODY/TR/. This happens
    // because TBODY/TR are not LayoutBlocks, but LayoutBoxModelObjects.
    DCHECK(container == node.GetLayoutBox()->ContainingBlock() ||
           node.GetLayoutBox()->ContainingBlock()->IsTable());
  } else {
    // If there's no layout object associated, the containing fragment should be
    // a page, and the containing block of the node should be the LayoutView.
    DCHECK_EQ(containing_block_fragment->GetBoxType(),
              PhysicalFragment::kPageArea);
    DCHECK_EQ(node.GetLayoutBox()->ContainingBlock(),
              node.GetLayoutBox()->View());
  }
#endif

  const ContainingBlockInfo base_container_info =
      GetContainingBlockInfo(oof_node);

  OofContainingBlock<LogicalOffset> containing_block;
  OofContainingBlock<LogicalOffset> fixedpos_containing_block;
  OofInlineContainer<LogicalOffset> fixedpos_inline_container;
  if (containing_block_fragment) {
    containing_block =
        To<LogicalOofNodeForFragmentation>(oof_node).containing_block;
    fixedpos_containing_block =
        To<LogicalOofNodeForFragmentation>(oof_node).fixedpos_containing_block;
    fixedpos_inline_container =
        To<LogicalOofNodeForFragmentation>(oof_node).fixedpos_inline_container;
  }

  return NodeInfo(
      node, oof_node.static_position, base_container_info,
      GetConstraintSpace().GetWritingDirection(),
      /* is_fragmentainer_descendant */ containing_block_fragment,
      containing_block, fixedpos_containing_block, fixedpos_inline_container,
      oof_node.requires_content_before_breaking, oof_node.is_hidden_for_paint);
}

const LayoutResult* OutOfFlowLayoutPart::LayoutOOFNode(
    NodeToLayout& oof_node_to_layout,
    const ConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const HeapHashSet<Member<Element>>* past_display_lock_elements = nullptr;
  if (auto* box = oof_node_to_layout.node_info.node.GetLayoutBox()) {
    past_display_lock_elements = box->DisplayLocksAffectedByAnchors();
  }

  const NodeInfo& node_info = oof_node_to_layout.node_info;
  OffsetInfo& offset_info = oof_node_to_layout.offset_info;

  BoxStrut scrollbars_before = ComputeScrollbarsForNonAnonymous(node_info.node);
  const LayoutResult* layout_result =
      Layout(oof_node_to_layout, fragmentainer_constraint_space,
             is_last_fragmentainer_so_far);

  // Since out-of-flow positioning sets up a constraint space with fixed
  // inline-size, the regular layout code (|BlockNode::Layout()|) cannot
  // re-layout if it discovers that a scrollbar was added or removed. Handle
  // that situation here. The assumption is that if intrinsic logical widths are
  // dirty after layout, AND its inline-size depends on the intrinsic logical
  // widths, it means that scrollbars appeared or disappeared.
  if (node_info.node.GetLayoutBox()->IntrinsicLogicalWidthsDirty() &&
      offset_info.inline_size_depends_on_min_max_sizes) {
    WritingDirectionMode writing_mode_direction =
        node_info.node.Style().GetWritingDirection();
    bool freeze_horizontal = false, freeze_vertical = false;
    BoxStrut scrollbars_after =
        ComputeScrollbarsForNonAnonymous(node_info.node);
    bool ignore_first_inline_freeze =
        scrollbars_after.InlineSum() && scrollbars_after.BlockSum();
    // If we're in a measure pass, freeze both scrollbars right away, to avoid
    // quadratic time complexity for deeply nested flexboxes.
    if (GetConstraintSpace().CacheSlot() == LayoutResultCacheSlot::kMeasure) {
      freeze_horizontal = freeze_vertical = true;
      ignore_first_inline_freeze = false;
    }
    do {
      // Freeze any scrollbars that appeared, and relayout. Repeat until both
      // have appeared, or until the scrollbar situation doesn't change,
      // whichever comes first.
      AddScrollbarFreeze(scrollbars_before, scrollbars_after,
                         writing_mode_direction, &freeze_horizontal,
                         &freeze_vertical);
      if (ignore_first_inline_freeze) {
        ignore_first_inline_freeze = false;
        // We allow to remove the inline-direction scrollbar only once
        // because the box might have unnecessary scrollbar due to
        // SetIsFixedInlineSize(true).
        if (writing_mode_direction.IsHorizontal())
          freeze_horizontal = false;
        else
          freeze_vertical = false;
      }
      scrollbars_before = scrollbars_after;
      PaintLayerScrollableArea::FreezeScrollbarsRootScope freezer(
          *node_info.node.GetLayoutBox(), freeze_horizontal, freeze_vertical);

      if (!IsBreakInside(oof_node_to_layout.break_token)) {
        // The offset itself does not need to be recalculated. However, the
        // |node_dimensions| and |initial_layout_result| may need to be updated,
        // so recompute the OffsetInfo.
        //
        // Only do this if we're currently building the first fragment of the
        // OOF. If we're resuming after a fragmentainer break, we can't update
        // our intrinsic inline-size. First of all, the intrinsic inline-size
        // should be the same across all fragments [1], and besides, this
        // operation would lead to performing a non-fragmented layout pass (to
        // measure intrinsic block-size; see IntrinsicBlockSizeFunc in
        // ComputeOutOfFlowBlockDimensions()), which in turn would overwrite the
        // result of the first fragment entry in LayoutBox without a break
        // token, causing major confusion everywhere.
        //
        // [1] https://drafts.csswg.org/css-break/#varying-size-boxes
        offset_info = CalculateOffset(node_info);
      }

      layout_result = Layout(oof_node_to_layout, fragmentainer_constraint_space,
                             is_last_fragmentainer_so_far);

      scrollbars_after = ComputeScrollbarsForNonAnonymous(node_info.node);
      DCHECK(!freeze_horizontal || !freeze_vertical ||
             scrollbars_after == scrollbars_before);
    } while (scrollbars_after != scrollbars_before);
  }

  auto& state = oof_node_to_layout.node_info.node.GetLayoutBox()
                    ->GetDocument()
                    .GetDisplayLockDocumentState();

  if (state.DisplayLockCount() >
      state.DisplayLockBlockingAllActivationCount()) {
    if (auto* box = oof_node_to_layout.node_info.node.GetLayoutBox()) {
      box->NotifyContainingDisplayLocksForAnchorPositioning(
          past_display_lock_elements,
          offset_info.display_locks_affected_by_anchors);
    }
  }

  return layout_result;
}

namespace {

// The spec says:
//
// "
// Implementations may choose to impose an implementation-defined limit on the
// length of position fallbacks lists, to limit the amount of excess layout work
// that may be required. This limit must be at least five.
// "
//
// We use 6 here because the first attempt is without anything from the
// position fallbacks list applied.
constexpr unsigned kMaxTryAttempts = 6;

// When considering multiple candidate styles (i.e. position-try-fallbacks),
// we keep track of each successful placement as a NonOverflowingCandidate.
// These candidates are then sorted according to the specified
// position-try-order.
//
// https://drafts.csswg.org/css-anchor-position-1/#position-try-order-property
struct NonOverflowingCandidate {
  DISALLOW_NEW();

 public:
  // The index into the position-try-fallbacks list that generated this
  // NonOverflowingCandidate. A value of nullopt means the regular styles
  // (without any position-try-fallback applied) generated the object.
  std::optional<wtf_size_t> try_fallback_index;
  // The result of TryCalculateOffset.
  OutOfFlowLayoutPart::OffsetInfo offset_info;

  void Trace(Visitor* visitor) const { visitor->Trace(offset_info); }
};

EPositionTryOrder ToLogicalPositionTryOrder(
    EPositionTryOrder position_try_order,
    WritingDirectionMode writing_direction) {
  switch (position_try_order) {
    case EPositionTryOrder::kNormal:
    case EPositionTryOrder::kMostBlockSize:
    case EPositionTryOrder::kMostInlineSize:
      return position_try_order;
    case EPositionTryOrder::kMostWidth:
      return writing_direction.IsHorizontal()
                 ? EPositionTryOrder::kMostInlineSize
                 : EPositionTryOrder::kMostBlockSize;
    case EPositionTryOrder::kMostHeight:
      return writing_direction.IsHorizontal()
                 ? EPositionTryOrder::kMostBlockSize
                 : EPositionTryOrder::kMostInlineSize;
  }
}

// Sorts `candidates` according to `position_try_order`, such that the correct
// candidate is at candidates.front().
void SortNonOverflowingCandidates(
    EPositionTryOrder position_try_order,
    WritingDirectionMode writing_direction,
    HeapVector<NonOverflowingCandidate, kMaxTryAttempts>& candidates) {
  EPositionTryOrder logical_position_try_order =
      ToLogicalPositionTryOrder(position_try_order, writing_direction);

  if (logical_position_try_order == EPositionTryOrder::kNormal) {
    // §5.2, normal: "Try the position fallbacks in the order specified by
    // position-try-fallbacks".
    return;
  }

  // §5.2, most-block-size (etc): "Stably sort the position fallbacks list
  // according to this size, with the largest coming first".
  std::stable_sort(
      candidates.begin(), candidates.end(),
      [logical_position_try_order](const NonOverflowingCandidate& a,
                                   const NonOverflowingCandidate& b) {
        switch (logical_position_try_order) {
          case EPositionTryOrder::kMostBlockSize:
            return a.offset_info.imcb_for_position_order->BlockSize() >
                   b.offset_info.imcb_for_position_order->BlockSize();
          case EPositionTryOrder::kMostInlineSize:
            return a.offset_info.imcb_for_position_order->InlineSize() >
                   b.offset_info.imcb_for_position_order->InlineSize();
          case EPositionTryOrder::kNormal:
            // Should have exited early.
          case EPositionTryOrder::kMostWidth:
          case EPositionTryOrder::kMostHeight:
            // We should have already converted to logical.
            NOTREACHED_IN_MIGRATION();
            return false;
        }
      });
}

}  // namespace

OutOfFlowLayoutPart::OffsetInfo OutOfFlowLayoutPart::CalculateOffset(
    const NodeInfo& node_info,
    const LogicalAnchorQueryMap* anchor_queries) {
  // See non_overflowing_scroll_range.h for documentation.
  HeapVector<NonOverflowingScrollRange> non_overflowing_scroll_ranges;

  // Note: This assumes @position-try rounds can't affect
  // writing-mode/position-anchor.
  AnchorEvaluatorImpl anchor_evaluator = CreateAnchorEvaluator(
      node_info.base_container_info, node_info.node, anchor_queries);

  OOFCandidateStyleIterator iter(*node_info.node.GetLayoutBox(),
                                 anchor_evaluator);
  bool has_try_fallbacks = iter.HasPositionTryFallbacks();
  EPositionTryOrder position_try_order = iter.PositionTryOrder();

  unsigned attempts_left = kMaxTryAttempts;
  bool has_no_overflow_visibility =
      node_info.node.Style().HasPositionVisibility(
          PositionVisibility::kNoOverflow);
  // If `position-try-fallbacks` or `position-visibility: no-overflow` exists,
  // let |TryCalculateOffset| check if the result fits the available space.
  bool try_fit_available_space =
      has_try_fallbacks || has_no_overflow_visibility;
  // Non-overflowing candidates (i.e. successfully placed candidates) are
  // collected into a vector. If position-try-order is non-normal, then we
  // collect *all* such candidates into the vector, and sort them according
  // to position-try-order.
  HeapVector<NonOverflowingCandidate, kMaxTryAttempts>
      non_overflowing_candidates;
  do {
    NonOverflowingScrollRange non_overflowing_range;
    // Do @position-try placement decisions on the *base style* to avoid
    // interference from animations and transitions.
    const ComputedStyle& style = iter.ActivateBaseStyleForTryAttempt();
    // However, without @position-try, the style is the current style.
    CHECK(has_try_fallbacks || &style == &iter.GetStyle());
    std::optional<OffsetInfo> offset_info =
        TryCalculateOffset(node_info, style, anchor_evaluator,
                           try_fit_available_space, &non_overflowing_range);

    // Also check if it fits the containing block after applying scroll offset
    // (i.e. the scroll-adjusted inset-modified containing block).
    if (offset_info) {
      if (try_fit_available_space) {
        non_overflowing_scroll_ranges.push_back(non_overflowing_range);
        if (!non_overflowing_range.Contains(GetAnchorOffset(
                node_info.node, style, anchor_evaluator.AnchorQuery()))) {
          continue;
        }
      }
      non_overflowing_candidates.push_back(
          NonOverflowingCandidate{iter.TryFallbackIndex(), *offset_info});
    }
  } while ((non_overflowing_candidates.empty() ||
            position_try_order != EPositionTryOrder::kNormal) &&
           --attempts_left != 0 && has_try_fallbacks && iter.MoveToNextStyle());

  // https://drafts.csswg.org/css-anchor-position-1/#position-try-order-property
  SortNonOverflowingCandidates(position_try_order,
                               node_info.base_container_info.writing_direction,
                               non_overflowing_candidates);

  std::optional<OffsetInfo> offset_info =
      non_overflowing_candidates.empty()
          ? std::optional<OffsetInfo>()
          : non_overflowing_candidates.front().offset_info;

  if (try_fit_available_space) {
    bool overflows_containing_block = false;
    if (non_overflowing_candidates.empty()) {
      // None of the fallbacks worked out.
      // Fall back to style without any fallbacks applied.
      iter.MoveToLastSuccessfulOrStyleWithoutFallbacks();
      overflows_containing_block = true;
    } else {
      // Move the iterator to the chosen candidate.
      iter.MoveToChosenTryFallbackIndex(
          non_overflowing_candidates.front().try_fallback_index);
    }
    // Once the position-try-fallbacks placement has been decided, calculate the
    // offset again, using the non-base style.
    const ComputedStyle& style = iter.ActivateStyleForChosenFallback();
    NonOverflowingScrollRange non_overflowing_range_unused;
    offset_info = TryCalculateOffset(node_info, style, anchor_evaluator,
                                     /* try_fit_available_space */ false,
                                     &non_overflowing_range_unused);
    offset_info->overflows_containing_block = overflows_containing_block;
  }
  CHECK(offset_info);

  if (try_fit_available_space) {
    offset_info->non_overflowing_scroll_ranges =
        std::move(non_overflowing_scroll_ranges);
  } else {
    DCHECK(offset_info->non_overflowing_scroll_ranges.empty());
  }

  offset_info->accessibility_anchor = anchor_evaluator.AccessibilityAnchor();
  offset_info->display_locks_affected_by_anchors =
      anchor_evaluator.GetDisplayLocksAffectedByAnchors();

  return *offset_info;
}

std::optional<OutOfFlowLayoutPart::OffsetInfo>
OutOfFlowLayoutPart::TryCalculateOffset(
    const NodeInfo& node_info,
    const ComputedStyle& candidate_style,
    AnchorEvaluatorImpl& anchor_evaluator,
    bool try_fit_available_space,
    NonOverflowingScrollRange* out_non_overflowing_range) {
  // TryCalculateOffset may be called multiple times if we have multiple @try
  // candidates. However, the AnchorEvaluatorImpl instance remains the same
  // across TryCalculateOffset calls, and was created with the "original"
  // writing-mode/position-anchor values.
  //
  // Those properties are not allowed within @try, so it should not be possible
  // to end up with a candidate style with different values.
  DCHECK_EQ(node_info.node.Style().GetWritingDirection(),
            candidate_style.GetWritingDirection());
  DCHECK(base::ValuesEquivalent(node_info.node.Style().PositionAnchor(),
                                candidate_style.PositionAnchor()));

  const ContainingBlockInfo container_info = ([&]() -> ContainingBlockInfo {
    ContainingBlockInfo container_info = node_info.base_container_info;
    if (const std::optional<PositionAreaOffsets> offsets =
            candidate_style.PositionAreaOffsets()) {
      container_info =
          ApplyPositionAreaOffsets(offsets.value(), container_info);
    }
    return container_info;
  })();

  const WritingDirectionMode candidate_writing_direction =
      candidate_style.GetWritingDirection();
  const auto container_writing_direction = container_info.writing_direction;

  const LogicalRect& container_rect = container_info.rect;
  const PhysicalSize container_physical_content_size =
      ToPhysicalSize(container_rect.size,
                     node_info.default_writing_direction.GetWritingMode());

  // The container insets. Don't use the position-area offsets directly as they
  // may be clamped to produce non-negative space. Instead take the difference
  // between the base, and adjusted container-info.
  const BoxStrut container_insets = ([&]() -> BoxStrut {
    const LogicalRect& base_rect = node_info.base_container_info.rect;
    const BoxStrut insets(
        container_rect.offset.inline_offset - base_rect.offset.inline_offset,
        base_rect.InlineEndOffset() - container_rect.InlineEndOffset(),
        container_rect.offset.block_offset - base_rect.offset.block_offset,
        base_rect.BlockEndOffset() - container_rect.BlockEndOffset());

    // Convert into the candidate writing-direction.
    return insets.ConvertToPhysical(node_info.default_writing_direction)
        .ConvertToLogical(candidate_writing_direction);
  })();

  // Create a constraint space to resolve border/padding/insets.
  const ConstraintSpace space = ([&]() -> ConstraintSpace {
    ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                   candidate_writing_direction,
                                   /* is_new_fc */ true);
    builder.SetAvailableSize(container_rect.size);
    builder.SetPercentageResolutionSize(container_rect.size);

    if (container_builder_->IsInitialColumnBalancingPass()) {
      // The |fragmentainer_offset_delta| will not make a difference in the
      // initial column balancing pass.
      SetupSpaceBuilderForFragmentation(
          GetConstraintSpace(), node_info.node,
          /*fragmentainer_offset_delta=*/LayoutUnit(),
          GetConstraintSpace().FragmentainerBlockSize(),
          /*is_resuming_past_block_end_edge=*/false, &builder);
    }
    return builder.ToConstraintSpace();
  })();

  const LogicalAlignment alignment = ComputeAlignment(
      candidate_style, container_info.is_scroll_container,
      container_writing_direction, candidate_writing_direction);

  const LogicalOofInsets insets =
      ComputeOutOfFlowInsets(candidate_style, space.AvailableSize(), alignment,
                             candidate_writing_direction);

  // Adjust the |static_position| (which is currently relative to the default
  // container's border-box) to be relative to the padding-box.
  // Since |container_rect.offset| is relative to its fragmentainer in this
  // case, we also need to adjust the offset to account for this.
  LogicalStaticPosition static_position = node_info.static_position;
  static_position.offset +=
      node_info.containing_block.Offset() - container_rect.offset;

  // Convert to the candidate's writing-direction.
  static_position = static_position
                        .ConvertToPhysical({node_info.default_writing_direction,
                                            container_physical_content_size})
                        .ConvertToLogical({candidate_writing_direction,
                                           container_physical_content_size});

  const InsetModifiedContainingBlock imcb = ComputeInsetModifiedContainingBlock(
      node_info.node, space.AvailableSize(), alignment, insets, static_position,
      container_writing_direction, candidate_writing_direction);

  {
    auto& document = node_info.node.GetDocument();
    if (alignment.inline_alignment.GetPosition() != ItemPosition::kNormal) {
      if (insets.inline_start && insets.inline_end) {
        UseCounter::Count(document,
                          WebFeature::kOutOfFlowJustifySelfBothInsets);
      } else if (insets.inline_start || insets.inline_end) {
        UseCounter::Count(document,
                          WebFeature::kOutOfFlowJustifySelfSingleInset);
      } else {
        UseCounter::Count(document, WebFeature::kOutOfFlowJustifySelfNoInsets);
      }
    }

    if (alignment.block_alignment.GetPosition() != ItemPosition::kNormal) {
      if (insets.block_start && insets.block_end) {
        UseCounter::Count(document, WebFeature::kOutOfFlowAlignSelfBothInsets);
      } else if (insets.block_start || insets.block_end) {
        UseCounter::Count(document, WebFeature::kOutOfFlowAlignSelfSingleInset);
      } else {
        UseCounter::Count(document, WebFeature::kOutOfFlowAlignSelfNoInsets);
      }
    }
  }

  const BoxStrut border_padding = ComputeBorders(space, node_info.node) +
                                  ComputePadding(space, candidate_style);

  std::optional<LogicalSize> replaced_size;
  if (node_info.node.IsReplaced()) {
    // Create a new space with the IMCB size, and stretch constraints.
    ConstraintSpaceBuilder builder(candidate_style.GetWritingMode(),
                                   candidate_style.GetWritingDirection(),
                                   /* is_new_fc */ true);
    builder.SetAvailableSize(imcb.Size());
    builder.SetPercentageResolutionSize(space.PercentageResolutionSize());
    builder.SetReplacedPercentageResolutionSize(
        space.PercentageResolutionSize());

    const bool is_parallel =
        IsParallelWritingMode(container_writing_direction.GetWritingMode(),
                              candidate_writing_direction.GetWritingMode());
    const ItemPosition inline_position =
        (is_parallel ? candidate_style.JustifySelf()
                     : candidate_style.AlignSelf())
            .GetPosition();
    const bool is_inline_stretch = !imcb.has_auto_inline_inset &&
                                   inline_position == ItemPosition::kStretch;
    if (is_inline_stretch) {
      builder.SetInlineAutoBehavior(AutoSizeBehavior::kStretchExplicit);
    }
    const ItemPosition block_position =
        (is_parallel ? candidate_style.AlignSelf()
                     : candidate_style.JustifySelf())
            .GetPosition();
    const bool is_block_stretch =
        !imcb.has_auto_block_inset && block_position == ItemPosition::kStretch;
    if (is_block_stretch) {
      builder.SetBlockAutoBehavior(AutoSizeBehavior::kStretchExplicit);
    }

    replaced_size =
        ComputeReplacedSize(node_info.node, builder.ToConstraintSpace(),
                            border_padding, ReplacedSizeMode::kNormal);
  }

  const LogicalAnchorCenterPosition anchor_center_position =
      ComputeAnchorCenterPosition(candidate_style, alignment,
                                  candidate_writing_direction,
                                  space.AvailableSize());

  OffsetInfo offset_info;
  LogicalOofDimensions& node_dimensions = offset_info.node_dimensions;
  offset_info.inline_size_depends_on_min_max_sizes = ComputeOofInlineDimensions(
      node_info.node, candidate_style, space, imcb, anchor_center_position,
      alignment, border_padding, replaced_size, container_insets,
      container_writing_direction, &node_dimensions);

  PhysicalToLogicalGetter has_non_auto_inset(
      candidate_writing_direction, candidate_style,
      &ComputedStyle::IsTopInsetNonAuto, &ComputedStyle::IsRightInsetNonAuto,
      &ComputedStyle::IsBottomInsetNonAuto, &ComputedStyle::IsLeftInsetNonAuto);

  // Calculate the inline scroll offset range where the inline dimension fits.
  std::optional<InsetModifiedContainingBlock> imcb_for_position_fallback;
  std::optional<LayoutUnit> inline_scroll_min;
  std::optional<LayoutUnit> inline_scroll_max;
  if (try_fit_available_space) {
    imcb_for_position_fallback = ComputeIMCBForPositionFallback(
        space.AvailableSize(), alignment, insets, static_position,
        candidate_style, container_writing_direction,
        candidate_writing_direction);
    offset_info.imcb_for_position_order = imcb_for_position_fallback;
    if (!CalculateNonOverflowingRangeInOneAxis(
            node_dimensions.MarginBoxInlineStart(),
            node_dimensions.MarginBoxInlineEnd(),
            imcb_for_position_fallback->inline_start,
            imcb_for_position_fallback->InlineEndOffset(),
            container_insets.inline_start, container_insets.inline_end,
            has_non_auto_inset.InlineStart(), has_non_auto_inset.InlineEnd(),
            &inline_scroll_min, &inline_scroll_max)) {
      return std::nullopt;
    }
  }

  // We may have already pre-computed our block-dimensions when determining
  // our min/max sizes, only run if needed.
  if (node_dimensions.size.block_size == kIndefiniteSize) {
    offset_info.initial_layout_result = ComputeOofBlockDimensions(
        node_info.node, candidate_style, space, imcb, anchor_center_position,
        alignment, border_padding, replaced_size, container_insets,
        container_writing_direction, &node_dimensions);
  }

  // Calculate the block scroll offset range where the block dimension fits.
  std::optional<LayoutUnit> block_scroll_min;
  std::optional<LayoutUnit> block_scroll_max;
  if (try_fit_available_space) {
    if (!CalculateNonOverflowingRangeInOneAxis(
            node_dimensions.MarginBoxBlockStart(),
            node_dimensions.MarginBoxBlockEnd(),
            imcb_for_position_fallback->block_start,
            imcb_for_position_fallback->BlockEndOffset(),
            container_insets.block_start, container_insets.block_end,
            has_non_auto_inset.BlockStart(), has_non_auto_inset.BlockEnd(),
            &block_scroll_min, &block_scroll_max)) {
      return std::nullopt;
    }
  }

  offset_info.block_estimate = node_dimensions.size.block_size;
  offset_info.container_content_size =
      container_physical_content_size.ConvertToLogical(
          candidate_writing_direction.GetWritingMode());

  // Calculate the offsets.
  const BoxStrut inset =
      node_dimensions.inset.ConvertToPhysical(candidate_writing_direction)
          .ConvertToLogical(node_info.default_writing_direction);

  // |inset| is relative to the container's padding-box. Convert this to being
  // relative to the default container's border-box.
  offset_info.offset = container_rect.offset;
  offset_info.offset.inline_offset += inset.inline_start;
  offset_info.offset.block_offset += inset.block_start;

  // Calculate the absolutized insets to be stored on |LayoutResult|.
  // |node_dimensions.inset| doesn't include margins, but |insets| do. We add
  // margins into |used_insets| for the calculation, and then remove them at the
  // end.
  const BoxStrut used_insets = node_dimensions.inset - node_dimensions.margins;
  BoxStrut insets_to_store;
  insets_to_store.inline_start =
      insets.inline_start.value_or(used_insets.inline_start);
  insets_to_store.inline_end =
      insets.inline_end.value_or(used_insets.inline_end);
  insets_to_store.block_start =
      insets.block_start.value_or(used_insets.block_start);
  insets_to_store.block_end = insets.block_end.value_or(used_insets.block_end);
  offset_info.insets_for_get_computed_style =
      insets_to_store.ConvertToPhysical(candidate_writing_direction)
          .ConvertToLogical(node_info.default_writing_direction);

  if (try_fit_available_space) {
    out_non_overflowing_range->containing_block_range =
        LogicalScrollRange{inline_scroll_min, inline_scroll_max,
                           block_scroll_min, block_scroll_max}
            .ToPhysical(candidate_writing_direction);
    out_non_overflowing_range->anchor_object = GetPositionAnchorObject(
        node_info.node, candidate_style, anchor_evaluator.AnchorQuery());
  }

  bool anchor_center_x = anchor_center_position.inline_offset.has_value();
  bool anchor_center_y = anchor_center_position.block_offset.has_value();
  if (!candidate_writing_direction.IsHorizontal()) {
    std::swap(anchor_center_x, anchor_center_y);
  }
  offset_info.needs_scroll_adjustment_in_x =
      anchor_center_x || anchor_evaluator.NeedsScrollAdjustmentInX();
  offset_info.needs_scroll_adjustment_in_y =
      anchor_center_y || anchor_evaluator.NeedsScrollAdjustmentInY();

  return offset_info;
}

const LayoutResult* OutOfFlowLayoutPart::Layout(
    const NodeToLayout& oof_node_to_layout,
    const ConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const OffsetInfo& offset_info = oof_node_to_layout.offset_info;

  const LayoutResult* layout_result = offset_info.initial_layout_result;
  // Reset the layout result computed earlier to allow fragmentation in the next
  // layout pass, if needed. Also do this if we're inside repeatable content, as
  // the pre-computed layout result is unusable then.
  if (fragmentainer_constraint_space ||
      GetConstraintSpace().IsInsideRepeatableContent()) {
    layout_result = nullptr;
  }

  // Skip this step if we produced a fragment that can be reused when
  // estimating the block-size.
  if (!layout_result) {
    layout_result =
        GenerateFragment(oof_node_to_layout, fragmentainer_constraint_space,
                         is_last_fragmentainer_so_far);
  }

  DCHECK_EQ(layout_result->Status(), LayoutResult::kSuccess);

  layout_result->GetMutableForOutOfFlow().SetOutOfFlowInsetsForGetComputedStyle(
      offset_info.insets_for_get_computed_style);

  layout_result->GetMutableForOutOfFlow().SetOutOfFlowPositionedOffset(
      offset_info.offset);

  layout_result->GetMutableForOutOfFlow().SetNeedsScrollAdjustment(
      offset_info.needs_scroll_adjustment_in_x,
      offset_info.needs_scroll_adjustment_in_y);

  layout_result->GetMutableForOutOfFlow().SetNonOverflowingScrollRanges(
      offset_info.non_overflowing_scroll_ranges);

  layout_result->GetMutableForOutOfFlow().SetAccessibilityAnchor(
      offset_info.accessibility_anchor);

  layout_result->GetMutableForOutOfFlow().SetDisplayLocksAffectedByAnchors(
      offset_info.display_locks_affected_by_anchors);

  const BlockNode& node = oof_node_to_layout.node_info.node;

  UpdatePositionVisibilityAfterLayout(offset_info, node,
                                      container_builder_->AnchorQuery());

  if (AXObjectCache* cache = node.GetDocument().ExistingAXObjectCache()) {
    cache->CSSAnchorChanged(node.GetLayoutBox());
  }

  return layout_result;
}

bool OutOfFlowLayoutPart::IsContainingBlockForCandidate(
    const LogicalOofPositionedNode& candidate) {
  // Fragmentainers are not allowed to be containing blocks.
  if (container_builder_->IsFragmentainerBoxType())
    return false;

  EPosition position = candidate.Node().Style().GetPosition();

  // Candidates whose containing block is inline are always positioned inside
  // closest parent block flow.
  if (candidate.inline_container.container) {
    DCHECK(candidate.inline_container.container
               ->CanContainOutOfFlowPositionedElement(position));
    return container_builder_->GetLayoutObject() ==
           candidate.box->ContainingBlock();
  }
  return (is_absolute_container_ && position == EPosition::kAbsolute) ||
         (is_fixed_container_ && position == EPosition::kFixed);
}

// The fragment is generated in one of these two scenarios:
// 1. To estimate candidate's block size, in this case block_size is
//    container's available size.
// 2. To compute final fragment, when block size is known from the absolute
//    position calculation.
const LayoutResult* OutOfFlowLayoutPart::GenerateFragment(
    const NodeToLayout& oof_node_to_layout,
    const ConstraintSpace* fragmentainer_constraint_space,
    bool is_last_fragmentainer_so_far) {
  const NodeInfo& node_info = oof_node_to_layout.node_info;
  const OffsetInfo& offset_info = oof_node_to_layout.offset_info;
  const BlockBreakToken* break_token = oof_node_to_layout.break_token;
  const BlockNode& node = node_info.node;
  const auto& style = node.Style();
  const LayoutUnit block_offset = offset_info.offset.block_offset;

  LayoutUnit inline_size = offset_info.node_dimensions.size.inline_size;
  LayoutUnit block_size = offset_info.block_estimate.value_or(
      offset_info.container_content_size.block_size);
  LogicalSize logical_size(inline_size, block_size);
  // Convert from logical size in the writing mode of the child to the logical
  // size in the writing mode of the container. That's what the constraint space
  // builder expects.
  PhysicalSize physical_size =
      ToPhysicalSize(logical_size, style.GetWritingMode());
  LogicalSize available_size =
      physical_size.ConvertToLogical(GetConstraintSpace().GetWritingMode());
  bool is_repeatable = false;

  ConstraintSpaceBuilder builder(GetConstraintSpace(),
                                 style.GetWritingDirection(),
                                 /* is_new_fc */ true);
  builder.SetAvailableSize(available_size);
  builder.SetPercentageResolutionSize(offset_info.container_content_size);
  builder.SetIsFixedInlineSize(true);
  builder.SetIsHiddenForPaint(node_info.is_hidden_for_paint);

  // In some cases we will need the fragment size in order to calculate the
  // offset. We may have to lay out to get the fragment size. For block
  // fragmentation, we *need* to know the block-offset before layout. In other
  // words, in that case, we may have to lay out, calculate the offset, and then
  // lay out again at the correct block-offset.
  if (offset_info.block_estimate.has_value() &&
      (!fragmentainer_constraint_space || !offset_info.initial_layout_result)) {
    builder.SetIsFixedBlockSize(true);
  }

  if (fragmentainer_constraint_space) {
    if (container_builder_->Node().IsPaginatedRoot() &&
        style.GetPosition() == EPosition::kFixed &&
        !oof_node_to_layout.containing_block_fragment) {
      // Paginated fixed-positioned elements are repeated on every page, if
      // contained by the initial containing block (i.e. when not contained by a
      // transformed element or similar) and may therefore not fragment.
      builder.SetShouldRepeat(true);
      builder.SetIsInsideRepeatableContent(true);
      builder.DisableMonolithicOverflowPropagation();
      is_repeatable = true;
    } else {
      // Note that we pass the pristine size of the fragmentainer here, which
      // means that we're not going to make room for any cloned borders that
      // might exist in the containing block chain of the OOF. This is
      // reasonable in a way, since they are out of flow after all, but, then
      // again, it's not really defined how out of flow positioned descendants
      // should behave when contained by something with cloned box decorations.
      //
      // See https://github.com/w3c/csswg-drafts/issues/10553
      SetupSpaceBuilderForFragmentation(
          *fragmentainer_constraint_space, node,
          fragmentainer_constraint_space->FragmentainerOffset() + block_offset,
          fragmentainer_constraint_space->FragmentainerBlockSize(),
          node_info.requires_content_before_breaking, &builder);

      // Out-of-flow positioned elements whose containing block is inside
      // clipped overflow shouldn't generate any additional fragmentainers. Just
      // place everything in the last fragmentainer. This is similar to what
      // LayoutAlgorithm::RelayoutWithoutFragmentation() does for in-flow
      // content overflowing a clipped ancestor, except that in this case we
      // know up front that we should disable fragmentation.
      //
      // Note that this approach isn't perfect. We don't know where (in which
      // fragmentainer) the clipped container ends. It may have ended in some
      // fragmentainer earlier than the last one, in which case we should have
      // finished this OOF there. But we have no (easy) way of telling where
      // that might be. But as long as the OOF doesn't contribute to any
      // additional fragmentainers, we should be (pretty) good.
      if (node_info.containing_block.IsFragmentedInsideClippedContainer()) {
        if (is_last_fragmentainer_so_far) {
          builder.DisableFurtherFragmentation();
        }
        builder.DisableMonolithicOverflowPropagation();
      }
    }
  } else if (container_builder_->IsInitialColumnBalancingPass()) {
    SetupSpaceBuilderForFragmentation(
        GetConstraintSpace(), node,
        GetConstraintSpace().FragmentainerOffset() + block_offset,
        GetConstraintSpace().FragmentainerBlockSize(),
        /*requires_content_before_breaking=*/false, &builder);
  }
  ConstraintSpace space = builder.ToConstraintSpace();

  if (is_repeatable)
    return node.LayoutRepeatableRoot(space, break_token);
  return node.Layout(space, break_token);
}

void OutOfFlowLayoutPart::LayoutOOFsInFragmentainer(
    HeapVector<NodeToLayout>& pending_descendants,
    wtf_size_t index,
    LogicalOffset fragmentainer_progression,
    bool has_oofs_in_later_fragmentainer,
    LayoutUnit* monolithic_overflow,
    bool* has_actual_break_inside,
    HeapVector<NodeToLayout>* fragmented_descendants) {
  wtf_size_t num_children = ChildCount();
  bool is_new_fragment = index >= num_children;
  bool is_last_fragmentainer_so_far = index + 1 >= num_children;

  DCHECK(fragmented_descendants);
  HeapVector<NodeToLayout> descendants_continued;
  ClearCollectionScope<HeapVector<NodeToLayout>> descendants_continued_scope(
      &descendants_continued);
  std::swap(*fragmented_descendants, descendants_continued);

  // If |index| is greater than the number of current children, and there are no
  // OOF children to be added, we will still need to add an empty fragmentainer
  // in its place. We also need to update the fragmentainer in case of
  // overflowed monolithic content (may happen in pagination for printing), or
  // if this is the hitherto last fragmentainer (it needs to be updated with an
  // outgoing break token, if nothing else).
  //
  // Otherwise, return early since there is no work to do.
  if (pending_descendants.empty() && descendants_continued.empty() &&
      *monolithic_overflow <= LayoutUnit() && !is_new_fragment &&
      !is_last_fragmentainer_so_far) {
    return;
  }

  // If we are a new fragment, find a non-spanner fragmentainer as a basis.
  wtf_size_t last_fragmentainer_index = index;
  while (last_fragmentainer_index >= num_children ||
         !GetChildFragment(last_fragmentainer_index).IsFragmentainerBox()) {
    DCHECK_GT(num_children, 0u);
    last_fragmentainer_index--;
  }

  const LogicalFragmentLink& container_link =
      FragmentationContextChildren()[last_fragmentainer_index];
  const BlockNode& node = container_builder_->Node();
  LogicalOffset fragmentainer_offset = container_link.offset;
  if (is_new_fragment) {
    // The fragmentainer being requested doesn't exist yet. This just means that
    // there are OOFs past the last fragmentainer that hold in-flow content.
    // Create and append an empty fragmentainer. Creating a fragmentainer is
    // algorithm-specific and not necessarily a trivial job, so leave it to the
    // fragmentation context algorithms.
    //
    // Afterwards we'll run SimplifiedOofLayoutAlgorithm and merge the results
    // from that algorithm into the new empty fragmentainer.
    const PhysicalBoxFragment& previous_fragmentainer =
        GetChildFragment(last_fragmentainer_index);
    const PhysicalBoxFragment* new_fragmentainer;
    if (node.IsPaginatedRoot()) {
      bool needs_total_page_count;
      new_fragmentainer = &PaginatedRootLayoutAlgorithm::CreateEmptyPage(
          node, GetConstraintSpace(), index, previous_fragmentainer,
          &needs_total_page_count);
      needs_total_page_count_ |= needs_total_page_count;
      additional_pages_were_added_ = true;
    } else {
      new_fragmentainer = &ColumnLayoutAlgorithm::CreateEmptyColumn(
          node, GetConstraintSpace(), previous_fragmentainer);
    }
    fragmentainer_offset += fragmentainer_progression;
    AddFragmentainer(*new_fragmentainer, fragmentainer_offset);
    DCHECK_EQ(index + 1, ChildCount());
  }

  const ConstraintSpace& space = GetFragmentainerConstraintSpace(index);
  const PhysicalBoxFragment* fragmentainer = &GetChildFragment(index);
  FragmentGeometry fragment_geometry =
      CalculateInitialFragmentGeometry(space, node, /* break_token */ nullptr);
  LayoutAlgorithmParams params(node, fragment_geometry, space,
                               PreviousFragmentainerBreakToken(index));
  // This algorithm will be used to add new OOFs. The existing fragment passed
  // is the last fragmentainer created so far.
  SimplifiedOofLayoutAlgorithm algorithm(params, *fragmentainer);

  if (has_oofs_in_later_fragmentainer) {
    algorithm.SetHasSubsequentChildren();
  }

  // Layout any OOF elements that are a continuation of layout first.
  for (auto& descendant : descendants_continued) {
    AddOOFToFragmentainer(descendant, &space, fragmentainer_offset, index,
                          is_last_fragmentainer_so_far, has_actual_break_inside,
                          &algorithm, fragmented_descendants);
  }
  // Once we've laid out the OOF elements that are a continuation of layout,
  // we can layout the OOF elements that start layout in the current
  // fragmentainer.
  for (auto& descendant : pending_descendants) {
    AddOOFToFragmentainer(descendant, &space, fragmentainer_offset, index,
                          is_last_fragmentainer_so_far, has_actual_break_inside,
                          &algorithm, fragmented_descendants);
  }

  // Don't update the builder when performing column balancing.
  if (column_balancing_info_) {
    return;
  }

  const LayoutResult* fragmentainer_result = algorithm.Layout();
  const auto& new_fragmentainer =
      To<PhysicalBoxFragment>(fragmentainer_result->GetPhysicalFragment());

  // The new fragmentainer was just prepared by the algorithm as a temporary
  // placeholder fragmentainer which will be "poured" into the existing one, and
  // then forgotten. This will add new OOFs (and whatever relevant info they
  // propagated).
  fragmentainer->GetMutableForOofFragmentation().Merge(new_fragmentainer);

  if (const BlockBreakToken* break_token = fragmentainer->GetBreakToken()) {
    *monolithic_overflow = break_token->MonolithicOverflow();
  } else {
    *monolithic_overflow = LayoutUnit();
  }
}

void OutOfFlowLayoutPart::AddOOFToFragmentainer(
    NodeToLayout& descendant,
    const ConstraintSpace* fragmentainer_space,
    LogicalOffset fragmentainer_offset,
    wtf_size_t index,
    bool is_last_fragmentainer_so_far,
    bool* has_actual_break_inside,
    SimplifiedOofLayoutAlgorithm* algorithm,
    HeapVector<NodeToLayout>* fragmented_descendants) {
  const LayoutResult* result = LayoutOOFNode(descendant, fragmentainer_space,
                                             is_last_fragmentainer_so_far);
  DCHECK_EQ(result->Status(), LayoutResult::kSuccess);

  // Apply the relative positioned offset now that fragmentation is complete.
  LogicalOffset oof_offset = result->OutOfFlowPositionedOffset();
  LogicalOffset relative_offset =
      descendant.node_info.base_container_info.relative_offset;
  LogicalOffset adjusted_offset = oof_offset + relative_offset;

  // In the case where an OOF descendant of |descendant| has its containing
  // block outside the current fragmentation context, we will want to apply an
  // additional offset to |oof_offset| in PropagateOOFPositionedInfo() such that
  // it's the offset relative to the current builder rather than the offset such
  // that all fragmentainers are stacked on top of each other.
  LogicalOffset offset_adjustment = fragmentainer_offset;

  result->GetMutableForOutOfFlow().SetOutOfFlowPositionedOffset(
      adjusted_offset);

  LogicalOffset additional_fixedpos_offset;
  if (descendant.node_info.fixedpos_containing_block.Fragment()) {
    additional_fixedpos_offset =
        descendant.offset_info.original_offset -
        descendant.node_info.fixedpos_containing_block.Offset();
    // Currently, |additional_fixedpos_offset| is the offset from the top of
    // |descendant| to the fixedpos containing block. Adjust this so that it
    // includes the block contribution of |descendant| from previous
    // fragmentainers. This ensures that any fixedpos descendants in the current
    // fragmentainer have the correct static position.
    if (descendant.break_token) {
      additional_fixedpos_offset.block_offset +=
          descendant.break_token->ConsumedBlockSize();
    }
  } else if (outer_context_has_fixedpos_container_) {
    // If the fixedpos containing block is in an outer fragmentation context,
    // we should adjust any fixedpos static positions such that they are
    // relative to the top of the inner multicol. These will eventually be
    // updated again with the offset from the multicol to the fixedpos
    // containing block such that the static positions are relative to the
    // containing block.
    DCHECK(multicol_children_);
    for (wtf_size_t i = std::min(index, multicol_children_->size()); i > 0u;
         i--) {
      MulticolChildInfo& column_info = (*multicol_children_)[i - 1];
      if (column_info.parent_break_token) {
        additional_fixedpos_offset.block_offset +=
            column_info.parent_break_token->ConsumedBlockSize();
        break;
      }
    }
  }

  const auto& physical_fragment =
      To<PhysicalBoxFragment>(result->GetPhysicalFragment());
  const BlockBreakToken* break_token = physical_fragment.GetBreakToken();
  if (break_token) {
    // We must continue layout in the next fragmentainer. Update any information
    // in NodeToLayout, and add the node to |fragmented_descendants|.
    NodeToLayout fragmented_descendant = descendant;
    fragmented_descendant.break_token = break_token;
    if (!break_token->IsRepeated()) {
      // Fragmented nodes usually resume at the block-start of the next
      // fragmentainer. One exception is if there's fragmentainer overflow
      // caused by monolithic content in paged media. Then we need to move past
      // that.
      fragmented_descendant.offset_info.offset.block_offset =
          break_token->MonolithicOverflow();
      *has_actual_break_inside = true;
    }
    fragmented_descendants->emplace_back(fragmented_descendant);
  }

  // Figure out if the current OOF affects column balancing. Then return since
  // we don't want to add the OOFs to the builder until the current columns have
  // completed layout.
  if (column_balancing_info_) {
    LayoutUnit space_shortage = CalculateSpaceShortage(
        *fragmentainer_space, result, oof_offset.block_offset,
        fragmentainer_space->FragmentainerBlockSize());
    column_balancing_info_->PropagateSpaceShortage(space_shortage);
    // We don't check the break appeal of the layout result to determine if
    // there is a violating break because OOFs aren't affected by the various
    // break rules. However, OOFs aren't pushed to the next fragmentainer if
    // they don't fit (when they are monolithic). Use |has_violating_break| to
    // tell the column algorithm when this happens so that it knows to attempt
    // to expand the columns in such cases.
    if (!column_balancing_info_->has_violating_break) {
      if (space_shortage > LayoutUnit() && !physical_fragment.GetBreakToken()) {
        column_balancing_info_->has_violating_break = true;
      }
    }
    return;
  }

  // Propagate new data to the |container_builder_| manually. Unlike when in
  // regular layout, MutableForOofFragmentation / SimplifiedOofLayoutAlgorithm
  // won't do this for us.
  container_builder_->PropagateChildAnchors(
      physical_fragment, oof_offset + relative_offset + offset_adjustment);
  container_builder_->PropagateStickyDescendants(physical_fragment);
  LayoutUnit containing_block_adjustment =
      container_builder_->BlockOffsetAdjustmentForFragmentainer(
          fragmentainer_consumed_block_size_);
  if (result->GetPhysicalFragment().NeedsOOFPositionedInfoPropagation()) {
    container_builder_->PropagateOOFPositionedInfo(
        result->GetPhysicalFragment(), oof_offset, relative_offset,
        offset_adjustment,
        /* inline_container */ nullptr, containing_block_adjustment,
        &descendant.node_info.containing_block,
        &descendant.node_info.fixedpos_containing_block,
        &descendant.node_info.fixedpos_inline_container,
        additional_fixedpos_offset);
  }
  algorithm->AppendOutOfFlowResult(result);

  // Copy the offset of the OOF node back to legacy such that it is relative
  // to its containing block rather than the fragmentainer that it is being
  // added to.
  if (!descendant.break_token) {
    const auto* container =
        To<PhysicalBoxFragment>(descendant.containing_block_fragment.Get());

    if (!container) {
      // If we're paginated, we don't have a containing block fragment, but we
      // need one now, to calcualte the position correctly for the legacy
      // engine. Just pick the first page, which actually happens to be defined
      // as the initial containing block:
      // https://www.w3.org/TR/CSS22/page.html#page-box
      DCHECK(container_builder_->Node().IsPaginatedRoot());
      container = &GetChildFragment(0);
    }

    LogicalOffset legacy_offset =
        descendant.offset_info.original_offset -
        descendant.node_info.base_container_info.offset_to_border_box;
    descendant.node_info.node.CopyChildFragmentPosition(
        physical_fragment,
        legacy_offset.ConvertToPhysical(
            container->Style().GetWritingDirection(), container->Size(),
            physical_fragment.Size()),
        *container, /* previous_container_break_token */ nullptr);
  }
}

ConstraintSpace OutOfFlowLayoutPart::GetFragmentainerConstraintSpace(
    wtf_size_t index) {
  DCHECK_LT(index, ChildCount());
  const PhysicalBoxFragment& fragment = GetChildFragment(index);
  DCHECK(fragment.IsFragmentainerBox());
  const WritingMode container_writing_mode =
      container_builder_->Style().GetWritingMode();
  LogicalSize fragmentainer_size =
      fragment.Size().ConvertToLogical(container_writing_mode);
  LogicalSize percentage_resolution_size =
      LogicalSize(fragmentainer_size.inline_size,
                  container_builder_->ChildAvailableSize().block_size);

  // In the current implementation it doesn't make sense to restrict imperfect
  // breaks inside OOFs, since we never break and resume OOFs in a subsequent
  // outer fragmentainer anyway (we'll always stay in the current outer
  // fragmentainer and just create overflowing columns in the current row,
  // rather than moving to the next one).
  BreakAppeal min_break_appeal = kBreakAppealLastResort;

  return CreateConstraintSpaceForFragmentainer(
      GetConstraintSpace(), GetFragmentainerType(), fragmentainer_size,
      percentage_resolution_size, /* balance_columns */ false,
      min_break_appeal);
}

// Compute in which fragmentainer the OOF element will start its layout and
// position the offset relative to that fragmentainer.
void OutOfFlowLayoutPart::ComputeStartFragmentIndexAndRelativeOffset(
    WritingMode default_writing_mode,
    LayoutUnit block_estimate,
    std::optional<LayoutUnit> clipped_container_block_offset,
    wtf_size_t* start_index,
    LogicalOffset* offset) const {
  // The sum of all previous fragmentainers' block size.
  LayoutUnit used_block_size;
  // The sum of all previous fragmentainers' block size + the current one.
  LayoutUnit current_max_block_size;
  // The block size for the last fragmentainer we encountered.
  LayoutUnit fragmentainer_block_size;

  LayoutUnit target_block_offset = offset->block_offset;
  if (clipped_container_block_offset &&
      container_builder_->Node().IsPaginatedRoot()) {
    // If we're printing, and we have an OOF inside a clipped container, prevent
    // the start fragmentainer from preceding that of the clipped container.
    // This way we increase the likelihood of luring the OOF into the same
    // fragmentainer as the clipped container, so that we get the correct clip
    // rectangle during pre-paint.
    //
    // TODO(crbug.com/1371426): We might be able to get rid of this, if we
    // either get pre-paint to handle missing ancestor fragments better, or if
    // we rewrite OOF layout to always generate the necessary ancestor
    // fragments.
    target_block_offset =
        std::max(target_block_offset, *clipped_container_block_offset);
  }
  // TODO(bebeaudr): There is a possible performance improvement here as we'll
  // repeat this for each abspos in a same fragmentainer.
  wtf_size_t child_index = 0;
  for (; child_index < ChildCount(); child_index++) {
    const PhysicalBoxFragment& child_fragment = GetChildFragment(child_index);
    if (child_fragment.IsFragmentainerBox()) {
      fragmentainer_block_size = child_fragment.Size()
                                     .ConvertToLogical(default_writing_mode)
                                     .block_size;
      fragmentainer_block_size =
          ClampedToValidFragmentainerCapacity(fragmentainer_block_size);
      current_max_block_size += fragmentainer_block_size;

      // Edge case: an abspos with an height of 0 positioned exactly at the
      // |current_max_block_size| won't be fragmented, so no break token will be
      // produced - as we'd expect. However, the break token is used to compute
      // the |fragmentainer_consumed_block_size_| stored on the
      // |container_builder_| when we have a nested abspos. Because we use that
      // value to position the nested abspos, its start offset would be off by
      // exactly one fragmentainer block size.
      if (target_block_offset < current_max_block_size ||
          (target_block_offset == current_max_block_size &&
           block_estimate == 0)) {
        *start_index = child_index;
        offset->block_offset -= used_block_size;
        return;
      }
      used_block_size = current_max_block_size;
    }
  }
  // If the right fragmentainer hasn't been found yet, the OOF element will
  // start its layout in a proxy fragment.
  LayoutUnit remaining_block_offset = offset->block_offset - used_block_size;
  wtf_size_t additional_fragment_count =
      int(floorf(remaining_block_offset / fragmentainer_block_size));
  *start_index = child_index + additional_fragment_count;
  offset->block_offset = remaining_block_offset -
                         additional_fragment_count * fragmentainer_block_size;
}

void OutOfFlowLayoutPart::SaveStaticPositionOnPaintLayer(
    LayoutBox* layout_box,
    const LogicalStaticPosition& position) const {
  const LayoutObject* parent =
      GetLayoutObjectForParentNode<const LayoutObject*>(layout_box);
  const LayoutObject* container = container_builder_->GetLayoutObject();
  if (parent == container ||
      (parent->IsLayoutInline() && parent->ContainingBlock() == container)) {
    DCHECK(layout_box->Layer());
    layout_box->Layer()->SetStaticPositionFromNG(
        ToStaticPositionForLegacy(position));
  }
}

LogicalStaticPosition OutOfFlowLayoutPart::ToStaticPositionForLegacy(
    LogicalStaticPosition position) const {
  // Legacy expects the static position to include the block contribution from
  // previous columns.
  if (const auto* break_token = container_builder_->PreviousBreakToken())
    position.offset.block_offset += break_token->ConsumedBlockSizeForLegacy();
  return position;
}

const PhysicalBoxFragment& OutOfFlowLayoutPart::GetChildFragment(
    wtf_size_t index) const {
  const LogicalFragmentLink& link = FragmentationContextChildren()[index];
  if (!container_builder_->Node().IsPaginatedRoot()) {
    return To<PhysicalBoxFragment>(*link.get());
  }
  DCHECK_EQ(link->GetBoxType(), PhysicalFragment::kPageContainer);
  return GetPageArea(GetPageBorderBox(To<PhysicalBoxFragment>(*link.get())));
}

const BlockBreakToken* OutOfFlowLayoutPart::PreviousFragmentainerBreakToken(
    wtf_size_t index) const {
  for (wtf_size_t i = index; i > 0; --i) {
    const PhysicalBoxFragment& previous_fragment = GetChildFragment(i - 1);
    if (previous_fragment.IsFragmentainerBox()) {
      return previous_fragment.GetBreakToken();
    }
  }
  return nullptr;
}

void OutOfFlowLayoutPart::ColumnBalancingInfo::PropagateSpaceShortage(
    LayoutUnit space_shortage) {
  UpdateMinimalSpaceShortage(space_shortage, &minimal_space_shortage);
}

void OutOfFlowLayoutPart::MulticolChildInfo::Trace(Visitor* visitor) const {
  visitor->Trace(parent_break_token);
}

void OutOfFlowLayoutPart::NodeInfo::Trace(Visitor* visitor) const {
  visitor->Trace(node);
  visitor->Trace(containing_block);
  visitor->Trace(fixedpos_containing_block);
  visitor->Trace(fixedpos_inline_container);
}

void OutOfFlowLayoutPart::OffsetInfo::Trace(Visitor* visitor) const {
  visitor->Trace(initial_layout_result);
  visitor->Trace(non_overflowing_scroll_ranges);
  visitor->Trace(accessibility_anchor);
  visitor->Trace(display_locks_affected_by_anchors);
}

void OutOfFlowLayoutPart::NodeToLayout::Trace(Visitor* visitor) const {
  visitor->Trace(node_info);
  visitor->Trace(offset_info);
  visitor->Trace(break_token);
  visitor->Trace(containing_block_fragment);
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NonOverflowingCandidate)
