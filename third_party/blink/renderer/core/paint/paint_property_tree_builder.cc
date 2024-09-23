// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"

#include <memory>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/block_break_token.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/table/layout_table.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/css_mask_painter.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_needing_update.h"
#include "third_party/blink/renderer/core/paint/find_properties_needing_update.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/pre_paint_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/svg_root_painter.h"
#include "third_party/blink/renderer/core/paint/transform_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/style/style_overflow_clip_margin.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace blink {

namespace {

bool AreSubtreeUpdateReasonsIsolationPiercing(unsigned reasons) {
  // This is written to mean that if we have any reason other than the specified
  // ones then the reasons are isolation piercing. This means that if new
  // reasons are added, they will be isolation piercing by default.
  //  - Isolation establishes a containing block for all descendants, so it is
  //    not piercing.
  // TODO(vmpstr): Investigate if transform style is also isolated.
  return reasons &
         ~(static_cast<unsigned>(
             SubtreePaintPropertyUpdateReason::kContainerChainMayChange));
}

}  // namespace

PaintPropertyTreeBuilderFragmentContext::
    PaintPropertyTreeBuilderFragmentContext()
    : current_effect(&EffectPaintPropertyNode::Root()) {
  current.clip = absolute_position.clip = fixed_position.clip =
      &ClipPaintPropertyNode::Root();
  current.transform = absolute_position.transform = fixed_position.transform =
      &TransformPaintPropertyNode::Root();
  current.scroll = absolute_position.scroll = fixed_position.scroll =
      &ScrollPaintPropertyNode::Root();
}

PaintPropertyTreeBuilderContext::PaintPropertyTreeBuilderContext()
    : force_subtree_update_reasons(0),
      has_svg_hidden_container_ancestor(false),
      was_layout_shift_root(false),
      global_main_thread_scrolling_reasons(0),
      composited_scrolling_preference(
          static_cast<unsigned>(CompositedScrollingPreference::kDefault)) {}

void VisualViewportPaintPropertyTreeBuilder::Update(
    LocalFrameView& main_frame_view,
    VisualViewport& visual_viewport,
    PaintPropertyTreeBuilderContext& full_context) {
  PaintPropertyTreeBuilderFragmentContext& context =
      full_context.fragment_context;

  auto property_changed =
      visual_viewport.UpdatePaintPropertyNodesIfNeeded(context);

  context.current.transform = visual_viewport.GetScrollTranslationNode();
  context.absolute_position.transform =
      visual_viewport.GetScrollTranslationNode();
  context.fixed_position.transform = visual_viewport.GetScrollTranslationNode();

  context.current.scroll = visual_viewport.GetScrollNode();
  context.absolute_position.scroll = visual_viewport.GetScrollNode();
  context.fixed_position.scroll = visual_viewport.GetScrollNode();

  if (property_changed >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
    // Force piercing subtree update for the worst case (scroll node added/
    // removed). Not a big deal for performance because this is rare.
    full_context.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    // The main frame's paint chunks (e.g. scrollbars) may reference paint
    // properties of the visual viewport.
    if (auto* layout_view = main_frame_view.GetLayoutView())
      layout_view->Layer()->SetNeedsRepaint();
  }

  if (property_changed >
      PaintPropertyChangeType::kChangedOnlyCompositedValues) {
    main_frame_view.SetPaintArtifactCompositorNeedsUpdate();
  }

#if DCHECK_IS_ON()
  paint_property_tree_printer::UpdateDebugNames(visual_viewport);
#endif
}

void PaintPropertyTreeBuilder::SetupContextForFrame(
    LocalFrameView& frame_view,
    PaintPropertyTreeBuilderContext& full_context) {
  PaintPropertyTreeBuilderFragmentContext& context =
      full_context.fragment_context;

  // Block fragmentation doesn't cross frame boundaries.
  context.current.is_in_block_fragmentation = false;

  context.current.paint_offset += PhysicalOffset(frame_view.Location());
  context.rendering_context_id = 0;
  context.should_flatten_inherited_transform = true;
  context.absolute_position = context.current;
  full_context.container_for_absolute_position = nullptr;
  full_context.container_for_fixed_position = nullptr;
  context.fixed_position = context.current;
  context.fixed_position.fixed_position_children_fixed_to_root = true;
}

namespace {

class FragmentPaintPropertyTreeBuilder {
  STACK_ALLOCATED();

 public:
  FragmentPaintPropertyTreeBuilder(
      const LayoutObject& object,
      PrePaintInfo* pre_paint_info,
      PaintPropertyTreeBuilderContext& full_context,
      FragmentData& fragment_data)
      : object_(object),
        pre_paint_info_(pre_paint_info),
        full_context_(full_context),
        context_(full_context.fragment_context),
        fragment_data_(fragment_data),
        properties_(fragment_data.PaintProperties()) {}

#if DCHECK_IS_ON()
  ~FragmentPaintPropertyTreeBuilder() {
    if (properties_)
      paint_property_tree_printer::UpdateDebugNames(object_, *properties_);
  }
#endif

  ALWAYS_INLINE void UpdateForSelf();
  ALWAYS_INLINE void UpdateForChildren();

  const PaintPropertiesChangeInfo& PropertiesChanged() const {
    return properties_changed_;
  }

  bool HasIsolationNodes() const {
    // All or nothing check on the isolation nodes.
    DCHECK(!properties_ ||
           (properties_->TransformIsolationNode() &&
            properties_->ClipIsolationNode() &&
            properties_->EffectIsolationNode()) ||
           (!properties_->TransformIsolationNode() &&
            !properties_->ClipIsolationNode() &&
            !properties_->EffectIsolationNode()));
    return properties_ && properties_->TransformIsolationNode();
  }

 private:
  ALWAYS_INLINE bool CanPropagateSubpixelAccumulation() const;
  ALWAYS_INLINE void UpdatePaintOffset();
  ALWAYS_INLINE void UpdateForPaintOffsetTranslation(
      std::optional<gfx::Vector2d>&);
  ALWAYS_INLINE void UpdatePaintOffsetTranslation(
      const std::optional<gfx::Vector2d>&);
  ALWAYS_INLINE void SetNeedsPaintPropertyUpdateIfNeeded();
  ALWAYS_INLINE void UpdateForObjectLocation(
      std::optional<gfx::Vector2d>& paint_offset_translation);
  ALWAYS_INLINE void UpdateStickyTranslation();
  ALWAYS_INLINE void UpdateAnchorPositionScrollTranslation();

  void UpdateIndividualTransform(
      bool (*needs_property)(const LayoutObject&, CompositingReasons),
      void (*compute_matrix)(const LayoutBox& box,
                             const PhysicalRect& reference_box,
                             gfx::Transform& matrix),
      CompositingReasons compositing_reasons_for_property,
      CompositorElementIdNamespace compositor_namespace,
      bool (ComputedStyle::*running_on_compositor_test)() const,
      const TransformPaintPropertyNode* (ObjectPaintProperties::*getter)()
          const,
      PaintPropertyChangeType (ObjectPaintProperties::*updater)(
          const TransformPaintPropertyNodeOrAlias&,
          TransformPaintPropertyNode::State&&,
          const TransformPaintPropertyNode::AnimationState&),
      bool (ObjectPaintProperties::*clearer)());
  ALWAYS_INLINE void UpdateTranslate();
  ALWAYS_INLINE void UpdateRotate();
  ALWAYS_INLINE void UpdateScale();
  ALWAYS_INLINE void UpdateOffset();
  ALWAYS_INLINE void UpdateTransform();
  ALWAYS_INLINE void UpdateTransformForSVGChild(CompositingReasons);
  ALWAYS_INLINE bool NeedsEffect() const;
  ALWAYS_INLINE bool EffectCanUseCurrentClipAsOutputClip() const;
  ALWAYS_INLINE void UpdateViewTransitionSubframeRootEffect();
  ALWAYS_INLINE void UpdateViewTransitionEffect();
  ALWAYS_INLINE void UpdateViewTransitionClip();
  ALWAYS_INLINE void UpdateEffect();
  ALWAYS_INLINE void UpdateElementCaptureEffect();
  ALWAYS_INLINE void UpdateFilter();
  ALWAYS_INLINE void UpdateCssClip();
  ALWAYS_INLINE void UpdateClipPathClip();
  ALWAYS_INLINE void UpdateLocalBorderBoxContext();
  ALWAYS_INLINE bool NeedsOverflowControlsClip() const;
  ALWAYS_INLINE void UpdateOverflowControlsClip();
  ALWAYS_INLINE void UpdateBackgroundClip();
  ALWAYS_INLINE void UpdateInnerBorderRadiusClip();
  ALWAYS_INLINE void UpdateOverflowClip();
  ALWAYS_INLINE void UpdatePerspective();
  ALWAYS_INLINE void UpdateReplacedContentTransform();
  ALWAYS_INLINE void UpdateScrollAndScrollTranslation();
  ALWAYS_INLINE void UpdateOutOfFlowContext();
  // See core/paint/README.md for the description of isolation nodes.
  ALWAYS_INLINE void UpdateTransformIsolationNode();
  ALWAYS_INLINE void UpdateEffectIsolationNode();
  ALWAYS_INLINE void UpdateClipIsolationNode();
  ALWAYS_INLINE TransformPaintPropertyNode::TransformAndOrigin
  TransformAndOriginForSVGChild() const;
  ALWAYS_INLINE void UpdateLayoutShiftRootChanged(bool is_layout_shift_root);

  bool NeedsPaintPropertyUpdate() const {
    return object_.NeedsPaintPropertyUpdate() ||
           full_context_.force_subtree_update_reasons;
  }

  const PhysicalBoxFragment& BoxFragment() const {
    const auto& box = To<LayoutBox>(object_);
    if (pre_paint_info_) {
      if (pre_paint_info_->box_fragment) {
        return *pre_paint_info_->box_fragment;
      }
      // Just return the first fragment if we weren't provided with one. This
      // happens when rebuilding the property context objects before walking a
      // missed descendant. Depending on the purpose, callers might want to
      // check IsMissingActualFragment() and do something appropriate for the
      // situation, rather than using a half-bogus fragment in its full glory.
      // Block-offset and block-size will typically be wrong, for instance,
      // whereas inline-offset and inline-size may be useful, if we assume that
      // all fragmentainers have the same inline-size.
      return *box.GetPhysicalFragment(0);
    }
    // We only get here if we're not inside block fragmentation, so there should
    // only be one fragment.
    DCHECK_EQ(box.PhysicalFragmentCount(), 1u);
    return *box.GetPhysicalFragment(0);
  }

  // Return true if we haven't been provided with a physical fragment for this
  // object. BoxFragment() will still return one, but it's most likely not the
  // right one, so some special handling may be necessary.
  bool IsMissingActualFragment() const {
    bool is_missing = pre_paint_info_ && !pre_paint_info_->box_fragment;
    DCHECK(!is_missing || PrePaintDisableSideEffectsScope::IsDisabled());
    return is_missing;
  }

  bool IsInNGFragmentTraversal() const { return pre_paint_info_; }

  void SwitchToOOFContext(
      PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext&
          oof_context) const {
    context_.current = oof_context;

    // If we're not block-fragmented, simply setting a new context is all we
    // have to do.
    if (!oof_context.is_in_block_fragmentation)
      return;

    // Inside NG block fragmentation we have to perform an offset adjustment.
    // An OOF fragment that is contained by something inside a fragmentainer
    // will be a direct child of the fragmentainer, rather than a child of its
    // actual containing block. Set the paint offset to the correct one.
    context_.current.paint_offset =
        context_.current.paint_offset_for_oof_in_fragmentainer;
  }

  void ResetPaintOffset(PhysicalOffset new_offset = PhysicalOffset()) {
    context_.current.paint_offset_for_oof_in_fragmentainer -=
        context_.current.paint_offset - new_offset;
    context_.current.paint_offset = new_offset;
  }

  void OnUpdateTransform(PaintPropertyChangeType change) {
    if (change != PaintPropertyChangeType::kUnchanged) {
      properties_changed_.transform_changed =
          std::max(properties_changed_.transform_changed, change);
      properties_changed_.transform_change_is_scroll_translation_only = false;
    }
  }
  void OnUpdateScrollTranslation(PaintPropertyChangeType change) {
    properties_changed_.transform_changed =
        std::max(properties_changed_.transform_changed, change);
  }
  void OnClearTransform(bool cleared) {
    if (cleared) {
      properties_changed_.transform_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
      properties_changed_.transform_change_is_scroll_translation_only = false;
    }
  }

  void OnUpdateClip(PaintPropertyChangeType change) {
    properties_changed_.clip_changed =
        std::max(properties_changed_.clip_changed, change);
  }
  void OnClearClip(bool cleared) {
    if (cleared) {
      properties_changed_.clip_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }

  void OnUpdateEffect(PaintPropertyChangeType change) {
    properties_changed_.effect_changed =
        std::max(properties_changed_.effect_changed, change);
  }
  void OnClearEffect(bool cleared) {
    if (cleared) {
      properties_changed_.effect_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }

  void OnUpdateScroll(PaintPropertyChangeType change) {
    properties_changed_.scroll_changed =
        std::max(properties_changed_.scroll_changed, change);
  }
  void OnClearScroll(bool cleared) {
    if (cleared) {
      properties_changed_.scroll_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }

  CompositorElementId GetCompositorElementId(
      CompositorElementIdNamespace namespace_id) const {
    return CompositorElementIdFromUniqueObjectId(fragment_data_.UniqueId(),
                                                 namespace_id);
  }

  MainThreadScrollingReasons GetMainThreadScrollingReasons(
      bool user_scrollable) const;

  const LayoutObject& object_;
  PrePaintInfo* pre_paint_info_;
  // The tree builder context for the whole object.
  PaintPropertyTreeBuilderContext& full_context_;
  // The tree builder context for the current fragment, which is one of the
  // entries in |full_context.fragments|.
  PaintPropertyTreeBuilderFragmentContext& context_;
  FragmentData& fragment_data_;
  ObjectPaintProperties* properties_;
  PaintPropertiesChangeInfo properties_changed_;
  // These are updated in UpdateClipPathClip() and used in UpdateEffect() if
  // needs_mask_base_clip_path_ is true.
  bool needs_mask_based_clip_path_ = false;
  std::optional<gfx::RectF> clip_path_bounding_box_;
};

// True if a scroll translation is needed for static scroll offset (e.g.,
// overflow hidden with scroll), or if a scroll node is needed for composited
// scrolling.
static bool NeedsScrollOrScrollTranslation(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons,
    bool allow_scrolled_overflow_hidden = true) {
  if (!object.IsScrollContainer()) {
    return false;
  }
  if (direct_compositing_reasons & CompositingReason::kRootScroller) {
    return true;
  }

  auto* scrollable_area = To<LayoutBox>(object).GetScrollableArea();
  CHECK(scrollable_area);
  if (scrollable_area->ScrollsOverflow()) {
    return true;
  }
  if (allow_scrolled_overflow_hidden &&
      (!scrollable_area->ScrollPosition().IsOrigin() ||
       !scrollable_area->GetScrollOffset().IsZero())) {
    return true;
  }
  return false;
}

static bool NeedsScrollNode(const LayoutObject& object,
                            CompositingReasons direct_compositing_reasons) {
  return NeedsScrollOrScrollTranslation(
      object, direct_compositing_reasons,
      RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled());
}

static bool NeedsReplacedContentTransform(const LayoutObject& object) {
  if (object.IsSVGRoot())
    return true;

  if (auto* layout_embedded_object = DynamicTo<LayoutEmbeddedContent>(object))
    return layout_embedded_object->FrozenFrameSize().has_value();

  return false;
}

static bool NeedsPaintOffsetTranslationForOverflowControls(
    const LayoutBoxModelObject& object) {
  if (auto* area = object.GetScrollableArea()) {
    if (area->HorizontalScrollbar() || area->VerticalScrollbar() ||
        area->Resizer()) {
      return true;
    }
  }
  return false;
}

static bool IsInLocalSubframe(const LayoutObject& object) {
  const auto* parent_frame = object.GetFrame()->Tree().Parent();
  return parent_frame && parent_frame->IsLocalFrame();
}

static bool NeedsIsolationNodes(const LayoutObject& object) {
  if (!object.HasLayer())
    return false;

  // Paint containment establishes isolation.
  // Style & Layout containment also establish isolation.
  if (object.ShouldApplyPaintContainment() ||
      (object.ShouldApplyStyleContainment() &&
       object.ShouldApplyLayoutContainment())) {
    return true;
  }

  // Layout view establishes isolation with the exception of local roots (since
  // they are already essentially isolated).
  if (object.IsLayoutView() && IsInLocalSubframe(object)) {
    return true;
  }

  return false;
}

static bool NeedsStickyTranslation(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;

  return To<LayoutBoxModelObject>(object).StickyConstraints();
}

static bool NeedsAnchorPositionScrollTranslation(const LayoutObject& object) {
  if (const LayoutBox* box = DynamicTo<LayoutBox>(object))
    return box->NeedsAnchorPositionScrollAdjustment();
  return false;
}

static bool NeedsPaintOffsetTranslation(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons,
    const LayoutObject* container_for_fixed_position,
    const PaintLayer* painting_layer) {
  if (!object.IsBoxModelObject())
    return false;

  // An SVG children inherits no paint offset, because there is no such concept
  // within SVG. Though <foreignObject> can have its own paint offset due to the
  // x and y parameters of the element, which affects the offset of painting of
  // the <foreignObject> element and its children, it still behaves like other
  // SVG elements, in that the x and y offset is applied *after* any transform,
  // instead of before.
  if (object.IsSVGChild())
    return false;

  const auto& box_model = To<LayoutBoxModelObject>(object);

  if (IsA<LayoutView>(box_model)) {
    // A translation node for LayoutView is always created to ensure fixed and
    // absolute contexts use the correct transform space.
    return true;
  }

  if (NeedsIsolationNodes(box_model)) {
    DCHECK(box_model.HasLayer());
    return true;
  }

  if (box_model.HasTransform())
    return true;
  if (NeedsScrollOrScrollTranslation(object, direct_compositing_reasons))
    return true;
  if (NeedsStickyTranslation(object))
    return true;
  if (NeedsAnchorPositionScrollTranslation(object)) {
    return true;
  }
  if (NeedsPaintOffsetTranslationForOverflowControls(box_model))
    return true;
  if (NeedsReplacedContentTransform(object))
    return true;

  // Reference filter and reflection (which creates a reference filter) requires
  // zero paint offset.
  if (box_model.HasLayer() &&
      (object.StyleRef().Filter().HasReferenceFilter() ||
       object.HasReflection()))
    return true;

  if (auto* box = DynamicTo<LayoutBox>(box_model)) {
    if (box->IsFixedToView(container_for_fixed_position))
      return true;
  }

  // Though we don't treat hidden backface as a direct compositing reason, it's
  // very likely that the object will be composited, so a paint offset
  // translation will be beneficial.
  bool has_paint_offset_compositing_reason =
      direct_compositing_reasons != CompositingReason::kNone ||
      box_model.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden;
  if (has_paint_offset_compositing_reason) {
    // Don't let paint offset cross composited layer boundaries when possible,
    // to avoid unnecessary full layer paint/raster invalidation when paint
    // offset in ancestor transform node changes which should not affect the
    // descendants of the composited layer. For now because of
    // crbug.com/780242, this is limited to LayoutBlocks and LayoutReplaceds
    // that won't be escaped by floating objects and column spans when finding
    // their containing blocks. TODO(crbug.com/780242): This can be avoided if
    // we have fully correct paint property tree states for floating objects
    // and column spans.
    if (box_model.IsLayoutBlock() || object.IsLayoutReplaced() ||
        (direct_compositing_reasons &
         CompositingReason::kViewTransitionElement) ||
        (direct_compositing_reasons & CompositingReason::kElementCapture)) {
      return true;
    }
  }

  return false;
}

bool FragmentPaintPropertyTreeBuilder::CanPropagateSubpixelAccumulation()
    const {
  if (!object_.HasLayer())
    return true;

  if (full_context_.direct_compositing_reasons &
      CompositingReason::kPreventingSubpixelAccumulationReasons) {
    return false;
  }
  if (full_context_.direct_compositing_reasons &
      (CompositingReason::kActiveTransformAnimation |
       CompositingReason::kActiveRotateAnimation |
       CompositingReason::kActiveScaleAnimation)) {
    if (const auto* element = DynamicTo<Element>(object_.GetNode())) {
      DCHECK(element->GetElementAnimations());
      return element->GetElementAnimations()->IsIdentityOrTranslation();
    }
    return false;
  }

  const PaintLayer* layer = To<LayoutBoxModelObject>(object_).Layer();
  return !layer->Transform() || layer->Transform()->IsIdentityOrTranslation();
}

void FragmentPaintPropertyTreeBuilder::UpdateForPaintOffsetTranslation(
    std::optional<gfx::Vector2d>& paint_offset_translation) {
  if (!NeedsPaintOffsetTranslation(object_,
                                   full_context_.direct_compositing_reasons,
                                   full_context_.container_for_fixed_position,
                                   full_context_.painting_layer)) {
    return;
  }

  // We should use the same subpixel paint offset values for snapping regardless
  // of paint offset translation. If we create a paint offset translation we
  // round the paint offset but keep around the residual fractional component
  // (i.e. subpixel accumulation) for the transformed content to paint with.
  paint_offset_translation = ToRoundedVector2d(context_.current.paint_offset);
  // Don't propagate subpixel accumulation through paint isolation.
  if (NeedsIsolationNodes(object_)) {
    ResetPaintOffset();
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();
    return;
  }

  PhysicalOffset subpixel_accumulation =
      context_.current.paint_offset - PhysicalOffset(*paint_offset_translation);
  if (!subpixel_accumulation.IsZero() ||
      !context_.current
           .directly_composited_container_paint_offset_subpixel_delta
           .IsZero()) {
    // If the object has a non-translation transform, discard the fractional
    // paint offset which can't be transformed by the transform.
    if (!CanPropagateSubpixelAccumulation()) {
      ResetPaintOffset();
      context_.current
          .directly_composited_container_paint_offset_subpixel_delta =
          PhysicalOffset();
      return;
    }
  }

  ResetPaintOffset(subpixel_accumulation);

  if (full_context_.direct_compositing_reasons == CompositingReason::kNone)
    return;

  if (paint_offset_translation && properties_ &&
      properties_->PaintOffsetTranslation()) {
    // The composited subpixel movement optimization applies only if the
    // composited layer has and had PaintOffsetTranslation, so that both the
    // the old and new paint offsets are just subpixel accumulations.
    DCHECK_EQ(gfx::Point(), ToRoundedPoint(fragment_data_.PaintOffset()));
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        context_.current.paint_offset - fragment_data_.PaintOffset();
  } else {
    // Otherwise disable the optimization.
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const std::optional<gfx::Vector2d>& paint_offset_translation) {
  DCHECK(properties_);

  if (paint_offset_translation) {
    TransformPaintPropertyNode::State state{
        {gfx::Transform::MakeTranslation(*paint_offset_translation)}};
    state.flattens_inherited_transform =
        context_.should_flatten_inherited_transform;
    state.rendering_context_id = context_.rendering_context_id;
    state.direct_compositing_reasons =
        full_context_.direct_compositing_reasons &
        CompositingReason::kDirectReasonsForPaintOffsetTranslationProperty;
    if (auto* box = DynamicTo<LayoutBox>(object_)) {
      if (box->IsFixedToView(full_context_.container_for_fixed_position) &&
          object_.View()->FirstFragment().PaintProperties()->Scroll()) {
        state.scroll_translation_for_fixed = object_.View()
                                                 ->FirstFragment()
                                                 .PaintProperties()
                                                 ->ScrollTranslation();
      }
    }

    if (IsA<LayoutView>(object_)) {
      DCHECK(object_.GetFrame());
      state.is_frame_paint_offset_translation = true;
      state.visible_frame_element_id =
          object_.GetFrame()->GetVisibleToHitTesting()
              ? CompositorElementIdFromUniqueObjectId(
                    object_.GetDocument().GetDomNodeId(),
                    CompositorElementIdNamespace::kDOMNodeId)
              : cc::ElementId();
    }
    OnUpdateTransform(properties_->UpdatePaintOffsetTranslation(
        *context_.current.transform, std::move(state)));
    context_.current.transform = properties_->PaintOffsetTranslation();
    if (IsA<LayoutView>(object_)) {
      context_.absolute_position.transform =
          properties_->PaintOffsetTranslation();
      context_.fixed_position.transform = properties_->PaintOffsetTranslation();
    }

    if (!object_.ShouldAssumePaintOffsetTranslationForLayoutShiftTracking()) {
      context_.current.additional_offset_to_layout_shift_root_delta +=
          PhysicalOffset(*paint_offset_translation);
    }
  } else {
    OnClearTransform(properties_->ClearPaintOffsetTranslation());
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateStickyTranslation() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsStickyTranslation(object_)) {
      const auto& box_model = To<LayoutBoxModelObject>(object_);
      TransformPaintPropertyNode::State state{{gfx::Transform::MakeTranslation(
          gfx::Vector2dF(box_model.StickyPositionOffset()))}};
      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReason::kStickyPosition;
      // TODO(wangxianzhu): Not using GetCompositorElementId() here because
      // sticky elements don't work properly under multicol for now, to keep
      // consistency with CompositorElementIdFromUniqueObjectId() below.
      // This will be fixed by LayoutNG block fragments.
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          box_model.UniqueId(),
          CompositorElementIdNamespace::kStickyTranslation);
      state.rendering_context_id = context_.rendering_context_id;
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;

      if (state.direct_compositing_reasons) {
        const auto* layout_constraint = box_model.StickyConstraints();
        DCHECK(layout_constraint);
        const auto* scroll_container_properties =
            layout_constraint->containing_scroll_container_layer
                ->GetLayoutObject()
                .FirstFragment()
                .PaintProperties();
        // A scroll node is created conditionally (see NeedsScrollNode),
        // while sticky position attaches to anything that clips overflow.
        // No need to (actually can't) setup composited sticky constraint if
        // the clipping ancestor we attach to doesn't have a scroll node.
        bool scroll_container_scrolls =
            scroll_container_properties &&
            scroll_container_properties->Scroll() == context_.current.scroll;
        if (scroll_container_scrolls) {
          auto constraint = std::make_unique<CompositorStickyConstraint>();
          constraint->is_anchored_left =
              layout_constraint->left_inset.has_value();
          constraint->is_anchored_right =
              layout_constraint->right_inset.has_value();
          constraint->is_anchored_top =
              layout_constraint->top_inset.has_value();
          constraint->is_anchored_bottom =
              layout_constraint->bottom_inset.has_value();

          constraint->left_offset =
              layout_constraint->left_inset.value_or(LayoutUnit()).ToFloat();
          constraint->right_offset =
              layout_constraint->right_inset.value_or(LayoutUnit()).ToFloat();
          constraint->top_offset =
              layout_constraint->top_inset.value_or(LayoutUnit()).ToFloat();
          constraint->bottom_offset =
              layout_constraint->bottom_inset.value_or(LayoutUnit()).ToFloat();
          constraint->constraint_box_rect =
              gfx::RectF(layout_constraint->constraining_rect);
          constraint->scroll_container_relative_sticky_box_rect = gfx::RectF(
              layout_constraint->scroll_container_relative_sticky_box_rect);
          constraint->scroll_container_relative_containing_block_rect =
              gfx::RectF(layout_constraint
                             ->scroll_container_relative_containing_block_rect);
          if (const LayoutBoxModelObject* sticky_box_shifting_ancestor =
                  layout_constraint->nearest_sticky_layer_shifting_sticky_box) {
            constraint->nearest_element_shifting_sticky_box =
                CompositorElementIdFromUniqueObjectId(
                    sticky_box_shifting_ancestor->UniqueId(),
                    CompositorElementIdNamespace::kStickyTranslation);
          }
          if (const LayoutBoxModelObject* containing_block_shifting_ancestor =
                  layout_constraint
                      ->nearest_sticky_layer_shifting_containing_block) {
            constraint->nearest_element_shifting_containing_block =
                CompositorElementIdFromUniqueObjectId(
                    containing_block_shifting_ancestor->UniqueId(),
                    CompositorElementIdNamespace::kStickyTranslation);
          }
          state.sticky_constraint = std::move(constraint);
        }
      }

      OnUpdateTransform(properties_->UpdateStickyTranslation(
          *context_.current.transform, std::move(state)));
    } else {
      OnClearTransform(properties_->ClearStickyTranslation());
    }
  }

  if (properties_->StickyTranslation())
    context_.current.transform = properties_->StickyTranslation();
}

void FragmentPaintPropertyTreeBuilder::UpdateAnchorPositionScrollTranslation() {
  DCHECK(properties_);
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsAnchorPositionScrollTranslation(object_)) {
      const auto& box = To<LayoutBox>(object_);
      const AnchorPositionScrollData& anchor_position_scroll_data =
          *box.GetAnchorPositionScrollData();
      gfx::Vector2dF translation_offset =
          -anchor_position_scroll_data.AccumulatedAdjustment();
      TransformPaintPropertyNode::State state{
          {gfx::Transform::MakeTranslation(translation_offset)}};

      // TODO(crbug.com/1309178): We should disable composited scrolling if the
      // snapshot's scrollers do not match the current scrollers.

      DCHECK(full_context_.direct_compositing_reasons &
             CompositingReason::kAnchorPosition);
      state.direct_compositing_reasons = CompositingReason::kAnchorPosition;

      // TODO(crbug.com/1309178): Not using GetCompositorElementId() here
      // because anchor-positioned elements don't work properly under multicol
      // for now, to keep consistency with
      // CompositorElementIdFromUniqueObjectId() below. This will be fixed by
      // LayoutNG block fragments.
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          box.UniqueId(),
          CompositorElementIdNamespace::kAnchorPositionScrollTranslation);
      state.rendering_context_id = context_.rendering_context_id;
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;

      state.anchor_position_scroll_data =
          std::make_unique<cc::AnchorPositionScrollData>();
      state.anchor_position_scroll_data->adjustment_container_ids =
          std::vector<CompositorElementId>(
              anchor_position_scroll_data.AdjustmentContainerIds().begin(),
              anchor_position_scroll_data.AdjustmentContainerIds().end());
      state.anchor_position_scroll_data->accumulated_scroll_origin =
          anchor_position_scroll_data.AccumulatedAdjustmentScrollOrigin();
      state.anchor_position_scroll_data->needs_scroll_adjustment_in_x =
          anchor_position_scroll_data.NeedsScrollAdjustmentInX();
      state.anchor_position_scroll_data->needs_scroll_adjustment_in_y =
          anchor_position_scroll_data.NeedsScrollAdjustmentInY();

      OnUpdateTransform(properties_->UpdateAnchorPositionScrollTranslation(
          *context_.current.transform, std::move(state)));
    } else {
      OnClearTransform(properties_->ClearAnchorPositionScrollTranslation());
    }
  }

  if (properties_->AnchorPositionScrollTranslation()) {
    context_.current.transform = properties_->AnchorPositionScrollTranslation();
  }
}

// Directly updates the associated cc transform node if possible, and
// downgrades the |PaintPropertyChangeType| if successful.
static void DirectlyUpdateCcTransform(
    const TransformPaintPropertyNode& transform,
    const LayoutObject& object,
    PaintPropertyChangeType& change_type) {
  // We only assume worst-case overlap testing due to animations (see:
  // |GeometryMapper::VisualRectForCompositingOverlap()|) so we can only use
  // the direct transform update (which skips checking for compositing changes)
  // when animations are present.
  if (change_type == PaintPropertyChangeType::kChangedOnlySimpleValues &&
      transform.HasActiveTransformAnimation()) {
    if (auto* paint_artifact_compositor =
            object.GetFrameView()->GetPaintArtifactCompositor()) {
      bool updated =
          paint_artifact_compositor->DirectlyUpdateTransform(transform);
      if (updated) {
        change_type = PaintPropertyChangeType::kChangedOnlyCompositedValues;
        transform.CompositorSimpleValuesUpdated();
      }
    }
  }
}

static void DirectlyUpdateCcOpacity(const LayoutObject& object,
                                    ObjectPaintProperties& properties,
                                    PaintPropertyChangeType& change_type) {
  if (change_type == PaintPropertyChangeType::kChangedOnlySimpleValues &&
      properties.Effect()->HasDirectCompositingReasons()) {
    if (auto* paint_artifact_compositor =
            object.GetFrameView()->GetPaintArtifactCompositor()) {
      bool updated =
          paint_artifact_compositor->DirectlyUpdateCompositedOpacityValue(
              *properties.Effect());
      if (updated) {
        change_type = PaintPropertyChangeType::kChangedOnlyCompositedValues;
        properties.Effect()->CompositorSimpleValuesUpdated();
      }
    }
  }
}

// TODO(dbaron): Remove this function when we can remove the
// BackfaceVisibilityInteropEnabled() check, and have the caller use
// CompositingReason::kDirectReasonsForTransformProperty directly.
static CompositingReasons CompositingReasonsForTransformProperty() {
  CompositingReasons reasons =
      CompositingReason::kDirectReasonsForTransformProperty;

  if (RuntimeEnabledFeatures::BackfaceVisibilityInteropEnabled())
    reasons |= CompositingReason::kBackfaceInvisibility3DAncestor;

  return reasons;
}

// TODO(crbug.com/1278452): Merge SVG handling into the primary codepath.
static bool NeedsTransformForSVGChild(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (!object.IsSVGChild() || object.IsText())
    return false;
  if (direct_compositing_reasons &
      (CompositingReasonsForTransformProperty() |
       CompositingReason::kDirectReasonsForTranslateProperty |
       CompositingReason::kDirectReasonsForRotateProperty |
       CompositingReason::kDirectReasonsForScaleProperty))
    return true;
  return !object.LocalToSVGParentTransform().IsIdentity();
}

TransformPaintPropertyNode::TransformAndOrigin
FragmentPaintPropertyTreeBuilder::TransformAndOriginForSVGChild() const {
  if (full_context_.direct_compositing_reasons &
      CompositingReason::kActiveTransformAnimation) {
    if (CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG(
            *To<SVGElement>(object_.GetNode()))) {
      const gfx::RectF reference_box =
          TransformHelper::ComputeReferenceBox(object_);
      // Composited transform animation works only if
      // LocalToSVGParentTransform() reflects the CSS transform properties.
      // If this fails, we need to exclude the case in
      // CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG().
      DCHECK_EQ(TransformHelper::ComputeTransform(
                    object_.GetDocument(), object_.StyleRef(), reference_box,
                    ComputedStyle::kIncludeTransformOrigin),
                object_.LocalToSVGParentTransform());
      // For composited transform animation to work, we need to store transform
      // origin separately. It's baked in object_.LocalToSVGParentTransform().
      return {TransformHelper::ComputeTransform(
                  object_.GetDocument(), object_.StyleRef(), reference_box,
                  ComputedStyle::kExcludeTransformOrigin)
                  .ToTransform(),
              gfx::Point3F(TransformHelper::ComputeTransformOrigin(
                  object_.StyleRef(), reference_box))};
    }
  }
  return {object_.LocalToSVGParentTransform().ToTransform()};
}

// SVG does not use the general transform update of |UpdateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
// TODO(crbug.com/1278452): Merge SVG handling into the primary codepath.
void FragmentPaintPropertyTreeBuilder::UpdateTransformForSVGChild(
    CompositingReasons direct_compositing_reasons) {
  DCHECK(properties_);
  DCHECK(object_.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object_.IsSVGForeignObject() ||
         context_.current.paint_offset.IsZero());

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsTransformForSVGChild(object_, direct_compositing_reasons)) {
      // The origin is included in the local transform, so leave origin empty.
      TransformPaintPropertyNode::State state;
      state.transform_and_origin = TransformAndOriginForSVGChild();

      // TODO(pdr): There is additional logic in
      // FragmentPaintPropertyTreeBuilder::UpdateTransform that likely needs to
      // be included here, such as setting animation_is_axis_aligned.
      state.direct_compositing_reasons =
          direct_compositing_reasons & CompositingReasonsForTransformProperty();
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;
      state.rendering_context_id = context_.rendering_context_id;
      state.is_for_svg_child = true;
      state.compositor_element_id = GetCompositorElementId(
          CompositorElementIdNamespace::kPrimaryTransform);

      TransformPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_animation_on_compositor =
          object_.StyleRef().IsRunningTransformAnimationOnCompositor();
      auto effective_change_type = properties_->UpdateTransform(
          *context_.current.transform, std::move(state), animation_state);
      DirectlyUpdateCcTransform(*properties_->Transform(), object_,
                                effective_change_type);
      OnUpdateTransform(effective_change_type);
    } else {
      OnClearTransform(properties_->ClearTransform());
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    context_.should_flatten_inherited_transform = true;
    context_.rendering_context_id = 0;
  }
}

static gfx::Point3F GetTransformOrigin(const LayoutBox& box,
                                       const PhysicalRect& reference_box) {
  // Transform origin has no effect without a transform or motion path.
  if (!box.HasTransform())
    return gfx::Point3F();
  const gfx::SizeF reference_box_size(reference_box.size);
  const auto& style = box.StyleRef();
  return gfx::Point3F(FloatValueForLength(style.GetTransformOrigin().X(),
                                          reference_box_size.width()) +
                          reference_box.X().ToFloat(),
                      FloatValueForLength(style.GetTransformOrigin().Y(),
                                          reference_box_size.height()) +
                          reference_box.Y().ToFloat(),
                      style.GetTransformOrigin().Z());
}

static bool NeedsIndividualTransform(
    const LayoutObject& object,
    CompositingReasons relevant_compositing_reasons,
    bool (*style_test)(const ComputedStyle&)) {
  if (object.IsText() || object.IsSVGChild())
    return false;

  if (relevant_compositing_reasons)
    return true;

  if (!object.IsBox())
    return false;

  if (style_test(object.StyleRef()))
    return true;

  return false;
}

static bool NeedsTranslate(const LayoutObject& object,
                           CompositingReasons direct_compositing_reasons) {
  return NeedsIndividualTransform(
      object,
      direct_compositing_reasons &
          CompositingReason::kDirectReasonsForTranslateProperty,
      [](const ComputedStyle& style) {
        return style.Translate() || style.HasCurrentTranslateAnimation();
      });
}

static bool NeedsRotate(const LayoutObject& object,
                        CompositingReasons direct_compositing_reasons) {
  return NeedsIndividualTransform(
      object,
      direct_compositing_reasons &
          CompositingReason::kDirectReasonsForRotateProperty,
      [](const ComputedStyle& style) {
        return style.Rotate() || style.HasCurrentRotateAnimation();
      });
}

static bool NeedsScale(const LayoutObject& object,
                       CompositingReasons direct_compositing_reasons) {
  return NeedsIndividualTransform(
      object,
      direct_compositing_reasons &
          CompositingReason::kDirectReasonsForScaleProperty,
      [](const ComputedStyle& style) {
        return style.Scale() || style.HasCurrentScaleAnimation();
      });
}

static bool NeedsOffset(const LayoutObject& object,
                        CompositingReasons direct_compositing_reasons) {
  return NeedsIndividualTransform(
      object, CompositingReason::kNone,
      [](const ComputedStyle& style) { return style.HasOffset(); });
}

static bool NeedsTransform(const LayoutObject& object,
                           CompositingReasons direct_compositing_reasons) {
  if (object.IsText() || object.IsSVGChild())
    return false;

  if (object.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden)
    return true;

  if (direct_compositing_reasons & CompositingReasonsForTransformProperty())
    return true;

  if (!object.IsBox())
    return false;

  if (object.StyleRef().HasTransformOperations() ||
      object.StyleRef().HasCurrentTransformAnimation() ||
      object.StyleRef().Preserves3D())
    return true;

  return false;
}

static bool UpdateBoxSizeAndCheckActiveAnimationAxisAlignment(
    const LayoutBox& object,
    CompositingReasons compositing_reasons) {
  if (!(compositing_reasons & (CompositingReason::kActiveTransformAnimation |
                               CompositingReason::kActiveScaleAnimation |
                               CompositingReason::kActiveRotateAnimation |
                               CompositingReason::kActiveTranslateAnimation)))
    return false;

  if (!object.GetNode() || !object.GetNode()->IsElementNode())
    return false;
  const Element* element = To<Element>(object.GetNode());
  auto* animations = element->GetElementAnimations();
  DCHECK(animations);
  return animations->UpdateBoxSizeAndCheckTransformAxisAlignment(
      gfx::SizeF(object.Size()));
}

static TransformPaintPropertyNode::TransformAndOrigin TransformAndOriginState(
    const LayoutBox& box,
    const PhysicalRect& reference_box,
    void (*compute_matrix)(const LayoutBox& box,
                           const PhysicalRect& reference_box,
                           gfx::Transform& matrix)) {
  gfx::Transform matrix;
  compute_matrix(box, reference_box, matrix);
  return {matrix, GetTransformOrigin(box, reference_box)};
}

static bool IsLayoutShiftRootTransform(
    const TransformPaintPropertyNode& transform) {
  // This is to keep the layout shift behavior before crrev.com/c/4024030.
  return transform.HasActiveTransformAnimation() ||
         !transform.IsIdentityOr2dTranslation();
}

void FragmentPaintPropertyTreeBuilder::UpdateIndividualTransform(
    bool (*needs_property)(const LayoutObject&, CompositingReasons),
    void (*compute_matrix)(const LayoutBox& box,
                           const PhysicalRect& reference_box,
                           gfx::Transform& matrix),
    CompositingReasons compositing_reasons_for_property,
    CompositorElementIdNamespace compositor_namespace,
    bool (ComputedStyle::*running_on_compositor_test)() const,
    const TransformPaintPropertyNode* (ObjectPaintProperties::*getter)() const,
    PaintPropertyChangeType (ObjectPaintProperties::*updater)(
        const TransformPaintPropertyNodeOrAlias&,
        TransformPaintPropertyNode::State&&,
        const TransformPaintPropertyNode::AnimationState&),
    bool (ObjectPaintProperties::*clearer)()) {
  // TODO(crbug.com/1278452): Merge SVG handling into the primary
  // codepath (which is this one).
  DCHECK(!object_.IsSVGChild());
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    if ((*needs_property)(object_, full_context_.direct_compositing_reasons)) {
      TransformPaintPropertyNode::State state;

      // A few pieces of the code are only for the 'transform' property
      // and not for the others.
      bool handling_transform_property =
          compositor_namespace ==
          CompositorElementIdNamespace::kPrimaryTransform;

      const ComputedStyle& style = object_.StyleRef();
      if (object_.IsBox()) {
        auto& box = To<LayoutBox>(object_);
        // Each individual fragment should have its own transform origin, based
        // on the fragment reference box.
        PhysicalRect reference_box = ComputeReferenceBox(BoxFragment());

        if (IsMissingActualFragment()) {
          // If the fragment doesn't really exist in the current fragmentainer,
          // treat its block-size as zero. See figure in
          // https://www.w3.org/TR/css-break-3/#transforms
          if (style.IsHorizontalWritingMode()) {
            reference_box.SetHeight(LayoutUnit());
          } else {
            reference_box.SetWidth(LayoutUnit());
          }
        }

        // If we are running transform animation on compositor, we should
        // disable 2d translation optimization to ensure that the compositor
        // gets the correct origin (which might be omitted by the optimization)
        // to the compositor, in case later animated values will use the origin.
        // See http://crbug.com/937929 for why we are not using
        // style.IsRunningTransformAnimationOnCompositor() etc. here.
        state.transform_and_origin =
            TransformAndOriginState(box, reference_box, compute_matrix);

        // TODO(trchen): transform-style should only be respected if a
        // PaintLayer is created. If a node with transform-style: preserve-3d
        // does not exist in an existing rendering context, it establishes a
        // new one.
        state.rendering_context_id = context_.rendering_context_id;
        if (handling_transform_property && style.Preserves3D() &&
            !state.rendering_context_id) {
          state.rendering_context_id = WTF::GetHash(&object_);
        }

        // TODO(crbug.com/1185254): Make this work correctly for block
        // fragmentation. It's the size of each individual PhysicalBoxFragment
        // that's interesting, not the total LayoutBox size.
        state.animation_is_axis_aligned =
            UpdateBoxSizeAndCheckActiveAnimationAxisAlignment(
                box, full_context_.direct_compositing_reasons);
      }

      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          compositing_reasons_for_property;

      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;
      if (running_on_compositor_test) {
        state.compositor_element_id =
            GetCompositorElementId(compositor_namespace);
      }

      if (handling_transform_property) {
        if (object_.HasHiddenBackface()) {
          state.backface_visibility =
              TransformPaintPropertyNode::BackfaceVisibility::kHidden;
        } else if (!context_.can_inherit_backface_visibility ||
                   style.Has3DTransformOperation()) {
          // We want to set backface-visibility back to visible, if the
          // parent doesn't allow this element to inherit backface visibility
          // (e.g. if the parent preserves 3d), or this element has a
          // syntactically-3D transform in *any* of the transform properties
          // (not just 'transform'). This means that backface-visibility on
          // an ancestor element no longer affects this element.
          state.backface_visibility =
              TransformPaintPropertyNode::BackfaceVisibility::kVisible;
        } else {
          // Otherwise we want to inherit backface-visibility.
          DCHECK_EQ(state.backface_visibility,
                    TransformPaintPropertyNode::BackfaceVisibility::kInherited);
        }
      }

      TransformPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_animation_on_compositor =
          running_on_compositor_test && (style.*running_on_compositor_test)();
      auto effective_change_type = (properties_->*updater)(
          *context_.current.transform, std::move(state), animation_state);
      DirectlyUpdateCcTransform(*(properties_->*getter)(), object_,
                                effective_change_type);
      OnUpdateTransform(effective_change_type);
    } else {
      OnClearTransform((properties_->*clearer)());
    }
  }

  if (const auto* transform = (properties_->*getter)()) {
    context_.current.transform = transform;
    if (!transform->Matrix().Is2dTransform()) {
      // We need to not flatten from this node through to this element's
      // transform node.  (If this is the transform node, we'll undo
      // this in the caller.)
      context_.should_flatten_inherited_transform = false;
    }
    if (!IsLayoutShiftRootTransform(*transform)) {
      context_.translation_2d_to_layout_shift_root_delta +=
          transform->Get2dTranslation();
    }
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateTranslate() {
  UpdateIndividualTransform(
      &NeedsTranslate,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        if (style.Translate())
          style.Translate()->Apply(matrix, gfx::SizeF(reference_box.size));
      },
      CompositingReason::kDirectReasonsForTranslateProperty,
      CompositorElementIdNamespace::kTranslateTransform,
      &ComputedStyle::IsRunningTranslateAnimationOnCompositor,
      &ObjectPaintProperties::Translate,
      &ObjectPaintProperties::UpdateTranslate,
      &ObjectPaintProperties::ClearTranslate);
}

void FragmentPaintPropertyTreeBuilder::UpdateRotate() {
  UpdateIndividualTransform(
      &NeedsRotate,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        if (style.Rotate())
          style.Rotate()->Apply(matrix, gfx::SizeF(reference_box.size));
      },
      CompositingReason::kDirectReasonsForRotateProperty,
      CompositorElementIdNamespace::kRotateTransform,
      &ComputedStyle::IsRunningRotateAnimationOnCompositor,
      &ObjectPaintProperties::Rotate, &ObjectPaintProperties::UpdateRotate,
      &ObjectPaintProperties::ClearRotate);
}

void FragmentPaintPropertyTreeBuilder::UpdateScale() {
  UpdateIndividualTransform(
      &NeedsScale,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        if (style.Scale())
          style.Scale()->Apply(matrix, gfx::SizeF(reference_box.size));
      },
      CompositingReason::kDirectReasonsForScaleProperty,
      CompositorElementIdNamespace::kScaleTransform,
      &ComputedStyle::IsRunningScaleAnimationOnCompositor,
      &ObjectPaintProperties::Scale, &ObjectPaintProperties::UpdateScale,
      &ObjectPaintProperties::ClearScale);
}

void FragmentPaintPropertyTreeBuilder::UpdateOffset() {
  UpdateIndividualTransform(
      &NeedsOffset,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        style.ApplyTransform(
            matrix, &box, reference_box,
            ComputedStyle::kExcludeTransformOperations,
            ComputedStyle::kExcludeTransformOrigin,
            ComputedStyle::kIncludeMotionPath,
            ComputedStyle::kExcludeIndependentTransformProperties);
      },
      CompositingReason::kNone,
      // TODO(dbaron): When we support animating offset on the
      // compositor, we need to use an element ID specific to offset.
      // This is currently unused.
      CompositorElementIdNamespace::kPrimary, nullptr,
      &ObjectPaintProperties::Offset, &ObjectPaintProperties::UpdateOffset,
      &ObjectPaintProperties::ClearOffset);
}

void FragmentPaintPropertyTreeBuilder::UpdateTransform() {
  UpdateIndividualTransform(
      &NeedsTransform,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        style.ApplyTransform(
            matrix, &box, reference_box,
            ComputedStyle::kIncludeTransformOperations,
            ComputedStyle::kExcludeTransformOrigin,
            ComputedStyle::kExcludeMotionPath,
            ComputedStyle::kExcludeIndependentTransformProperties);
      },
      CompositingReasonsForTransformProperty(),
      CompositorElementIdNamespace::kPrimaryTransform,
      &ComputedStyle::IsRunningTransformAnimationOnCompositor,
      &ObjectPaintProperties::Transform,
      &ObjectPaintProperties::UpdateTransform,
      &ObjectPaintProperties::ClearTransform);

  // Since we're doing a full update, clear list of objects waiting for a
  // deferred update
  object_.GetFrameView()->RemovePendingTransformUpdate(object_);

  // properties_->Transform() is present if a CSS transform is present,
  // and is also present if transform-style: preserve-3d is set.
  // See NeedsTransform.
  if (const auto* transform = properties_->Transform()) {
    context_.current.transform = transform;
    if (object_.StyleRef().Preserves3D()) {
      context_.rendering_context_id = transform->RenderingContextId();
      context_.should_flatten_inherited_transform = false;
    } else {
      context_.rendering_context_id = 0;
      context_.should_flatten_inherited_transform = true;
    }
  } else if (!object_.IsAnonymous()) {
    // 3D rendering contexts follow the DOM ancestor chain, so
    // flattening should apply regardless of presence of transform.
    context_.rendering_context_id = 0;
    context_.should_flatten_inherited_transform = true;
  }
}

static bool NeedsClipPathClipOrMask(const LayoutObject& object) {
  // We only apply clip-path if the LayoutObject has a layer or is an SVG
  // child. See NeedsEffect() for additional information on the former.
  return !object.IsText() && object.StyleRef().HasClipPath() &&
         (object.HasLayer() || object.IsSVGChild());
}

static bool NeedsEffectForViewTransition(const LayoutObject& object) {
  // The view-transition-name property when set creates a backdrop filter root.
  // We do this by ensuring that this object needs an effect node.
  //
  // This is not required for the root element since its snapshot comes from the
  // root stacking context which is already a backdrop filter root.
  const auto& style = object.StyleRef();
  if (style.ElementIsViewTransitionParticipant()) {
    DCHECK(
        ViewTransitionUtils::IsViewTransitionElementExcludingRootFromSupplement(
            *To<Element>(object.GetNode())));
    return true;
  } else {
#if DCHECK_IS_ON()
    auto* element = DynamicTo<Element>(object.GetNode());
    DCHECK(!element ||
           !ViewTransitionUtils::
               IsViewTransitionElementExcludingRootFromSupplement(*element))
        << element;
#endif
  }

  return style.ViewTransitionName() && !object.IsDocumentElement() &&
         !object.IsLayoutView();
}

static bool NeedsEffectIgnoringClipPath(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (object.IsText()) {
    DCHECK(!(direct_compositing_reasons &
             CompositingReason::kDirectReasonsForEffectProperty));
    return false;
  }

  if (direct_compositing_reasons &
      CompositingReason::kDirectReasonsForEffectProperty)
    return true;

  const ComputedStyle& style = object.StyleRef();

  // For now some objects (e.g. LayoutTableCol) with stacking context style
  // don't create layer thus are not actual stacking contexts, so the HasLayer()
  // condition. TODO(crbug.com/892734): Support effects for LayoutTableCol.
  const bool is_css_isolated_group =
      object.HasLayer() && object.IsStackingContext();

  if (!is_css_isolated_group && !object.IsSVG())
    return false;

  if (object.IsSVG() && SVGLayoutSupport::IsIsolationRequired(&object))
    return true;

  if (is_css_isolated_group) {
    const auto* layer = To<LayoutBoxModelObject>(object).Layer();
    DCHECK(layer);

    if (layer->HasNonIsolatedDescendantWithBlendMode())
      return true;
  }

  if (object.IsBlendingAllowed() &&
      WebCoreCompositeToSkiaComposite(
          kCompositeSourceOver, style.GetBlendMode()) != SkBlendMode::kSrcOver)
    return true;

  if (!style.BackdropFilter().IsEmpty())
    return true;

  if (style.Opacity() != 1.0f)
    return true;

  // A mask needs an effect node on the current LayoutObject to define the scope
  // of masked contents to be the current LayoutObject and its descendants.
  if (style.HasMask()) {
    return true;
  }

  // The view-transition-name property when set creates a backdrop filter root.
  // We do this by ensuring that this object needs an effect node.
  // This is not required for the root element since its snapshot comes from the
  // root stacking context which is already a backdrop filter root.
  if (NeedsEffectForViewTransition(object)) {
    return true;
  }

  return false;
}

bool FragmentPaintPropertyTreeBuilder::NeedsEffect() const {
  DCHECK(NeedsPaintPropertyUpdate());
  // A mask-based clip-path needs an effect node, similar to a normal mask.
  if (needs_mask_based_clip_path_)
    return true;
  return NeedsEffectIgnoringClipPath(object_,
                                     full_context_.direct_compositing_reasons);
}

// An effect node can use the current clip as its output clip if the clip won't
// end before the effect ends. Having explicit output clip can let the later
// stages use more optimized code path.
bool FragmentPaintPropertyTreeBuilder::EffectCanUseCurrentClipAsOutputClip()
    const {
  DCHECK(NeedsEffect());

  if (!object_.HasLayer()) {
    // This is either SVG or it's the effect node to create flattening at the
    // leaves of a 3D scene.
    //
    // Either way, the effect never interleaves with clips, because
    // positioning is the only situation where clip order changes.
    return true;
  }

  const auto* layer = To<LayoutBoxModelObject>(object_).Layer();
  // Out-of-flow descendants not contained by this object may escape clips.
  if (layer->HasNonContainedAbsolutePositionDescendant()) {
    const auto* container = full_context_.container_for_absolute_position;
    // Check HasLocalBorderBoxProperties() because |container| may not have
    // updated paint properties if it appears in a later box fragment than
    // |object|. TODO(crbug.com/1371426): fix tree walk order in the case.
    if (!container->FirstFragment().HasLocalBorderBoxProperties() ||
        &container->FirstFragment().ContentsClip() != context_.current.clip) {
      return false;
    }
  }
  if (layer->HasFixedPositionDescendant() &&
      !object_.CanContainFixedPositionObjects()) {
    const auto* container = full_context_.container_for_fixed_position;
    // Same as the absolute-position case.
    if (!container->FirstFragment().HasLocalBorderBoxProperties() ||
        &container->FirstFragment().ContentsClip() != context_.current.clip) {
      return false;
    }
  }

  return true;
}

void FragmentPaintPropertyTreeBuilder::UpdateEffect() {
  DCHECK(properties_);
  // Since we're doing a full update, clear list of objects waiting for a
  // deferred update
  object_.GetFrameView()->RemovePendingOpacityUpdate(object_);
  const ComputedStyle& style = object_.StyleRef();

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsEffect()) {
      std::optional<gfx::RectF> mask_clip = CSSMaskPainter::MaskBoundingBox(
          object_, context_.current.paint_offset);
      if (mask_clip || needs_mask_based_clip_path_) {
        DCHECK(mask_clip || clip_path_bounding_box_.has_value());
        gfx::RectF combined_clip =
            mask_clip ? *mask_clip : *clip_path_bounding_box_;
        if (mask_clip && needs_mask_based_clip_path_)
          combined_clip.Intersect(*clip_path_bounding_box_);
        OnUpdateClip(properties_->UpdateMaskClip(
            *context_.current.clip,
            ClipPaintPropertyNode::State(
                *context_.current.transform, combined_clip,
                FloatRoundedRect(gfx::ToEnclosingRect(combined_clip)))));
        // We don't use MaskClip as the output clip of Effect, Mask and
        // ClipPathMask because we only want to apply MaskClip to the contents,
        // not the masks.
      } else {
        OnClearClip(properties_->ClearMaskClip());
      }

      CompositorElementId mask_compositor_element_id;
      if (mask_clip) {
        mask_compositor_element_id =
            GetCompositorElementId(CompositorElementIdNamespace::kEffectMask);
      }

      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      if (EffectCanUseCurrentClipAsOutputClip())
        state.output_clip = context_.current.clip;
      state.opacity = style.Opacity();
      if (object_.IsBlendingAllowed()) {
        state.blend_mode = WebCoreCompositeToSkiaComposite(
            kCompositeSourceOver, style.GetBlendMode());
      }
      if (object_.IsBoxModelObject()) {
        if (auto* layer = To<LayoutBoxModelObject>(object_).Layer()) {
          CompositorFilterOperations operations;
          gfx::RRectF bounds;
          layer->UpdateCompositorFilterOperationsForBackdropFilter(operations,
                                                                   bounds);
          if (!operations.IsEmpty()) {
            state.backdrop_filter_info = base::WrapUnique(
                new EffectPaintPropertyNode::BackdropFilterInfo{
                    std::move(operations), bounds, mask_compositor_element_id});
          }
        }
      }

      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReason::kDirectReasonsForEffectProperty;

      // If an effect node exists, add an additional direct compositing reason
      // for 3d transforms and will-change:transform to ensure it is composited.
      state.direct_compositing_reasons |=
          (full_context_.direct_compositing_reasons &
           CompositingReason::kAdditionalEffectCompositingTrigger);

      // We may begin to composite our subtree prior to an animation starts, but
      // a compositor element ID is only needed when an animation is current.
      // Currently, we use the existence of this id to check if effect nodes
      // have been created for animations on this element.
      if (state.direct_compositing_reasons) {
        state.compositor_element_id = GetCompositorElementId(
            CompositorElementIdNamespace::kPrimaryEffect);
      } else {
        // The effect node CompositorElementId is used to uniquely identify
        // renderpasses so even if we don't need one for animations we still
        // need to set an id. Using kPrimary avoids confusing cc::Animation
        // into thinking the element has been composited for animations.
        state.compositor_element_id =
            GetCompositorElementId(CompositorElementIdNamespace::kPrimary);
      }

      state.self_or_ancestor_participates_in_view_transition =
          context_.self_or_ancestor_participates_in_view_transition;

      EffectPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_opacity_animation_on_compositor =
          style.IsRunningOpacityAnimationOnCompositor();
      animation_state.is_running_backdrop_filter_animation_on_compositor =
          style.IsRunningBackdropFilterAnimationOnCompositor();

      const auto* parent_effect = context_.current_effect;
      // The transition pseudo element doesn't draw into the LayoutView's
      // effect, but rather as its sibling. So this re-parents the effect to
      // whatever the grand-parent effect was. Note that it doesn't matter
      // whether the grand-parent is the root stacking context or something
      // intermediate, as long as it is a sibling of the LayoutView context.
      // This makes it possible to capture the output of the LayoutView context
      // into one of the transition contexts. We also want that capture to be
      // without any additional effects, such as overscroll elasticity effects.
      if (object_.GetNode() &&
          object_.GetNode()->GetPseudoId() == kPseudoIdViewTransition) {
        if (IsInLocalSubframe(object_)) {
          parent_effect = object_.GetDocument()
                              .GetLayoutView()
                              ->FirstFragment()
                              .PaintProperties()
                              ->ViewTransitionSubframeRootEffect();
        } else {
          parent_effect = &EffectPaintPropertyNode::Root();
        }
        DCHECK(parent_effect);
      }
      DCHECK(parent_effect);

      auto effective_change_type = properties_->UpdateEffect(
          *parent_effect, std::move(state), animation_state);
      // If we have simple value change, which means opacity, we should try to
      // directly update it on the PaintArtifactCompositor in order to avoid
      // doing a full rebuild.
      DirectlyUpdateCcOpacity(object_, *properties_, effective_change_type);
      OnUpdateEffect(effective_change_type);

      auto mask_direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
                  CompositingReason::kDirectReasonsForBackdropFilter
              ? CompositingReason::kBackdropFilterMask
              : CompositingReason::kNone;

      if (mask_clip) {
        EffectPaintPropertyNode::State mask_state;
        mask_state.local_transform_space = context_.current.transform;
        mask_state.output_clip = context_.current.clip;
        mask_state.blend_mode = SkBlendMode::kDstIn;
        mask_state.compositor_element_id = mask_compositor_element_id;
        mask_state.direct_compositing_reasons = mask_direct_compositing_reasons;

        if (const auto* old_mask = properties_->Mask()) {
          // The mask node's output clip is used in the property tree state
          // when painting the mask, so the impact of its change should be the
          // same as a clip change in LocalBorderBoxProperties (see
          // UpdateLocalBorderBoxContext()).
          if (old_mask->OutputClip() != mask_state.output_clip)
            OnUpdateClip(PaintPropertyChangeType::kNodeAddedOrRemoved);
        }

        OnUpdateEffect(properties_->UpdateMask(*properties_->Effect(),
                                               std::move(mask_state)));
      } else {
        OnClearEffect(properties_->ClearMask());
      }

      if (needs_mask_based_clip_path_) {
        EffectPaintPropertyNode::State clip_path_state;
        clip_path_state.local_transform_space = context_.current.transform;
        clip_path_state.output_clip = context_.current.clip;
        clip_path_state.blend_mode = SkBlendMode::kDstIn;
        clip_path_state.compositor_element_id = GetCompositorElementId(
            CompositorElementIdNamespace::kEffectClipPath);
        if (!mask_clip) {
          clip_path_state.direct_compositing_reasons =
              mask_direct_compositing_reasons;
        }
        OnUpdateEffect(properties_->UpdateClipPathMask(
            properties_->Mask() ? *properties_->Mask() : *properties_->Effect(),
            std::move(clip_path_state)));
      } else {
        OnClearEffect(properties_->ClearClipPathMask());
      }
    } else {
      OnClearEffect(properties_->ClearEffect());
      OnClearEffect(properties_->ClearMask());
      OnClearEffect(properties_->ClearClipPathMask());
      OnClearClip(properties_->ClearMaskClip());
    }
  }

  if (const auto* effect = properties_->Effect()) {
    context_.current_effect = effect;
    context_.this_or_ancestor_opacity_is_zero |= effect->Opacity() == 0;
    if (properties_->MaskClip()) {
      context_.current.clip = context_.absolute_position.clip =
          context_.fixed_position.clip = properties_->MaskClip();
    }
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateElementCaptureEffect() {
  if (!NeedsPaintPropertyUpdate()) {
    return;
  }

  if (!(full_context_.direct_compositing_reasons &
        CompositingReason::kElementCapture)) {
    OnClearEffect(properties_->ClearElementCaptureEffect());
    return;
  }

  // If we have the correct compositing reason, we should be associated with a
  // node. In the case we are not, the effect is no longer valid.
  auto* element = DynamicTo<Element>(object_.GetNode());
  CHECK(element);
  CHECK(element->GetRestrictionTargetId());
  CHECK(context_.current.clip);
  CHECK(context_.current.transform);
  EffectPaintPropertyNode::State state;
  state.direct_compositing_reasons = CompositingReason::kElementCapture;
  state.local_transform_space = context_.current.transform;
  state.output_clip = context_.current.clip;
  state.restriction_target_id = *element->GetRestrictionTargetId();
  state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
      object_.UniqueId(), CompositorElementIdNamespace::kElementCapture);

  OnUpdateEffect(properties_->UpdateElementCaptureEffect(
      *context_.current_effect, std::move(state), {}));
  context_.current_effect = properties_->ElementCaptureEffect();
}

void FragmentPaintPropertyTreeBuilder::
    UpdateViewTransitionSubframeRootEffect() {
  if (NeedsPaintPropertyUpdate()) {
    const bool needs_node =
        object_.IsLayoutView() && IsInLocalSubframe(object_) &&
        ViewTransitionUtils::GetTransition(object_.GetDocument());

    bool needs_full_invalidation = false;

    if (needs_node) {
      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.output_clip = context_.current.clip;
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          object_.UniqueId(),
          CompositorElementIdNamespace::kViewTransitionSubframeRoot);
      if (const auto& layer =
              ViewTransitionUtils::GetTransition(object_.GetDocument())
                  ->GetSubframeSnapshotLayer()) {
        state.view_transition_element_resource_id =
            layer->ViewTransitionResourceId();
      }

      auto change_type = properties_->UpdateViewTransitionSubframeRootEffect(
          *context_.current_effect, std::move(state), {});
      needs_full_invalidation =
          change_type >= PaintPropertyChangeType::kNodeAddedOrRemoved;
      OnUpdateEffect(change_type);
    } else {
      bool node_removed = properties_->ClearViewTransitionSubframeRootEffect();
      needs_full_invalidation = node_removed;
      OnClearEffect(node_removed);
    }

    // This node is used an ancestor by nodes generated by descendants of the
    // LayoutView. This means creation or removal of this node needs to be
    // propagated past the isolation for subframes.
    if (needs_full_invalidation) {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    }
  }

  if (auto* effect = properties_->ViewTransitionSubframeRootEffect()) {
    context_.current_effect = effect;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateViewTransitionEffect() {
  if (NeedsPaintPropertyUpdate()) {
    const bool old_self_or_ancestor_participates_in_view_transition =
        properties_->ViewTransitionEffect() &&
        properties_->ViewTransitionEffect()
            ->SelfOrAncestorParticipatesInViewTransition();

    const bool needs_view_transition_effect =
        full_context_.direct_compositing_reasons &
        CompositingReason::kViewTransitionElement;

    if (needs_view_transition_effect) {
      auto* transition =
          ViewTransitionUtils::GetTransition(object_.GetDocument());
      DCHECK(transition);

      EffectPaintPropertyNode::State state;
      state.direct_compositing_reasons =
          CompositingReason::kViewTransitionElement;
      state.local_transform_space = context_.current.transform;
      state.output_clip = context_.current.clip;
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          object_.UniqueId(),
          CompositorElementIdNamespace::kViewTransitionElement);
      state.view_transition_element_resource_id =
          transition->GetSnapshotId(object_);

      // The value isn't set on the root, since clipping rules are different for
      // the root view transition element.
      if (object_.IsLayoutView()) {
        // The LayoutView can only have this bit set from an ancestor if it
        // belongs to a subframe.
        CHECK(!context_.self_or_ancestor_participates_in_view_transition ||
              IsInLocalSubframe(object_));
        state.self_or_ancestor_participates_in_view_transition =
            context_.self_or_ancestor_participates_in_view_transition;
      } else {
        state.self_or_ancestor_participates_in_view_transition = true;
      }

      OnUpdateEffect(properties_->UpdateViewTransitionEffect(
          *context_.current_effect, std::move(state), {}));
    } else {
      OnClearEffect(properties_->ClearViewTransitionEffect());
    }

    // Whether self and ancestor participate in a view transition needs to be
    // propagated to the subtree of the element that set the value.
    const auto* new_effect = properties_->ViewTransitionEffect();
    bool new_self_or_ancestor_participates_in_view_transition =
        new_effect && new_effect->SelfOrAncestorParticipatesInViewTransition();

    if (old_self_or_ancestor_participates_in_view_transition !=
        new_self_or_ancestor_participates_in_view_transition) {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    }
  }

  if (auto* effect = properties_->ViewTransitionEffect()) {
    context_.current_effect = effect;
    context_.self_or_ancestor_participates_in_view_transition |=
        effect->SelfOrAncestorParticipatesInViewTransition();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateViewTransitionClip() {
  if (NeedsPaintPropertyUpdate()) {
    if (full_context_.direct_compositing_reasons &
        CompositingReason::kViewTransitionElement) {
      auto* transition =
          ViewTransitionUtils::GetTransition(object_.GetDocument());
      DCHECK(transition);

      if (!transition->NeedsViewTransitionClipNode(object_)) {
        return;
      }

      OnUpdateClip(transition->UpdateCaptureClip(object_, context_.current.clip,
                                                 context_.current.transform));
      context_.current.clip = transition->GetCaptureClip(object_);
    }
  }
}

static bool IsLinkHighlighted(const LayoutObject& object) {
  return object.GetFrame()->GetPage()->GetLinkHighlight().IsHighlighting(
      object);
}

static bool IsClipPathDescendant(const LayoutObject& object) {
  // If the object itself is a resource container (root of a resource subtree)
  // it is not considered a clipPath descendant since it is independent of its
  // ancestors.
  if (object.IsSVGResourceContainer()) {
    return false;
  }
  const LayoutObject* parent = object.Parent();
  while (parent) {
    if (parent->IsSVGResourceContainer()) {
      auto* container = To<LayoutSVGResourceContainer>(parent);
      return container->ResourceType() == kClipperResourceType;
    }
    parent = parent->Parent();
  }
  return false;
}

static bool NeedsFilter(const LayoutObject& object,
                        const PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.direct_compositing_reasons &
      CompositingReason::kDirectReasonsForFilterProperty)
    return true;

  if (object.IsBoxModelObject() &&
      To<LayoutBoxModelObject>(object).HasLayer()) {
    if (object.StyleRef().HasFilter() || object.HasReflection()) {
      return true;
    }
  } else if (object.IsSVGChild() && !object.IsText() &&
             SVGResources::GetClient(object)) {
    if (object.StyleRef().HasFilter()) {
      // Filters don't apply to elements that are descendants of a <clipPath>.
      if (!full_context.has_svg_hidden_container_ancestor ||
          !IsClipPathDescendant(object))
        return true;
    }
  }
  return false;
}

static void UpdateFilterEffect(const LayoutObject& object,
                               const EffectPaintPropertyNode* effect_node,
                               CompositorFilterOperations& filter) {
  if (object.HasLayer()) {
    // Try to use the cached filter.
    if (effect_node) {
      filter = effect_node->Filter();
    }
    PaintLayer* layer = To<LayoutBoxModelObject>(object).Layer();
#if DCHECK_IS_ON()
    // We should have already updated the reference box.
    auto reference_box = layer->FilterReferenceBox();
    layer->UpdateFilterReferenceBox();
    DCHECK_EQ(reference_box, layer->FilterReferenceBox());
#endif
    layer->UpdateCompositorFilterOperationsForFilter(filter);
    return;
  }
  if (object.IsSVGChild() && !object.IsText()) {
    SVGElementResourceClient* client = SVGResources::GetClient(object);
    if (!client)
      return;
    if (!object.StyleRef().HasFilter())
      return;
    // Try to use the cached filter.
    if (effect_node)
      filter = effect_node->Filter();
    client->UpdateFilterData(filter);
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateFilter() {
  DCHECK(properties_);
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsFilter(object_, full_context_)) {
      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;

      UpdateFilterEffect(object_, properties_->Filter(), state.filter);

      // The CSS filter spec didn't specify how filters interact with overflow
      // clips. The implementation here mimics the old Blink/WebKit behavior for
      // backward compatibility.
      // Basically the output of the filter will be affected by clips that
      // applies to the current element. The descendants that paints into the
      // input of the filter ignores any clips collected so far. For example:
      // <div style="overflow:scroll">
      //   <div style="filter:blur(1px);">
      //     <div>A</div>
      //     <div style="position:absolute;">B</div>
      //   </div>
      // </div>
      // In this example "A" should be clipped if the filter was not present.
      // With the filter, "A" will be rastered without clipping, but instead
      // the blurred result will be clipped.
      // "B" should be also clipped because a filter always creates a containing
      // block for all descendants.
      state.output_clip = context_.current.clip;

      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReason::kDirectReasonsForFilterProperty;

      // If a filter node exists, add an additional direct compositing reason
      // for 3d transforms and will-change:transform to ensure it is composited.
      state.direct_compositing_reasons |=
          (full_context_.direct_compositing_reasons &
           CompositingReason::kAdditionalEffectCompositingTrigger);

      state.compositor_element_id =
          GetCompositorElementId(CompositorElementIdNamespace::kEffectFilter);

      state.self_or_ancestor_participates_in_view_transition =
          context_.self_or_ancestor_participates_in_view_transition;

      // This must be computed before std::move(state) below.
      bool needs_pixel_moving_filter_clip_expander =
          (state.direct_compositing_reasons &
           (CompositingReason::kWillChangeFilter |
            CompositingReason::kActiveFilterAnimation)) ||
          state.filter.HasFilterThatMovesPixels();

      EffectPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_filter_animation_on_compositor =
          object_.StyleRef().IsRunningFilterAnimationOnCompositor();
      OnUpdateEffect(properties_->UpdateFilter(
          *context_.current_effect, std::move(state), animation_state));

      if (needs_pixel_moving_filter_clip_expander) {
        OnUpdateClip(properties_->UpdatePixelMovingFilterClipExpander(
            *context_.current.clip,
            ClipPaintPropertyNode::State(*context_.current.transform,
                                         properties_->Filter())));
      } else {
        OnClearClip(properties_->ClearPixelMovingFilterClipExpander());
      }
    } else {
      OnClearEffect(properties_->ClearFilter());
      OnClearClip(properties_->ClearPixelMovingFilterClipExpander());
    }
  }

  if (properties_->Filter()) {
    context_.current_effect = properties_->Filter();
    if (const auto* input_clip = properties_->PixelMovingFilterClipExpander()) {
      context_.current.clip = input_clip;
    }
  } else {
    DCHECK(!properties_->PixelMovingFilterClipExpander());
  }
}

static FloatRoundedRect ToSnappedClipRect(const PhysicalRect& rect) {
  return FloatRoundedRect(ToPixelSnappedRect(rect));
}

static bool NeedsCssClip(const LayoutObject& object) {
  if (object.HasClip()) {
    DCHECK(!object.IsText());
    return true;
  }
  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateCssClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsCssClip(object_)) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object_.CanContainAbsolutePositionObjects());
      const auto& clip_rect =
          To<LayoutBox>(object_).ClipRect(context_.current.paint_offset);
      OnUpdateClip(properties_->UpdateCssClip(
          *context_.current.clip,
          ClipPaintPropertyNode::State(*context_.current.transform,
                                       gfx::RectF(clip_rect),
                                       ToSnappedClipRect(clip_rect))));
    } else {
      OnClearClip(properties_->ClearCssClip());
    }
  }

  if (properties_->CssClip())
    context_.current.clip = properties_->CssClip();
}

static std::optional<FloatRoundedRect> PathToRRect(const Path& path) {
  const SkPath sk_path = path.GetSkPath();
  if (sk_path.isInverseFillType()) {
    return std::nullopt;
  }
  SkRect rect;
  if (sk_path.isRect(&rect)) {
    return FloatRoundedRect(gfx::SkRectToRectF(rect));
  }
  SkRRect rrect;
  if (sk_path.isRRect(&rrect)) {
    return FloatRoundedRect(rrect);
  }
  if (sk_path.isOval(&rect)) {
    return FloatRoundedRect(SkRRect::MakeOval(rect));
  }
  return std::nullopt;
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathClip() {
  if (NeedsPaintPropertyUpdate()) {
    DCHECK(!clip_path_bounding_box_.has_value());
    if (NeedsClipPathClipOrMask(object_)) {
      // Recalc the composited clip path paint status, which depends on whether
      // we are in a block fragmentation
      ClipPathClipper::ResolveClipPathStatus(
          object_, context_.current.is_in_block_fragmentation);
      clip_path_bounding_box_ =
          ClipPathClipper::LocalClipPathBoundingBox(object_);
      if (clip_path_bounding_box_) {
        // SVG "children" does not have a paint offset, but for <foreignObject>
        // the paint offset can still be non-zero since it contains the 'x' and
        // 'y' portion of the geometry. (See also comment in
        // `NeedsPaintOffsetTranslation()`.)
        const gfx::Vector2dF paint_offset =
            !object_.IsSVGChild()
                ? gfx::Vector2dF(context_.current.paint_offset)
                : gfx::Vector2dF();
        clip_path_bounding_box_->Offset(paint_offset);
        if (std::optional<Path> path =
                ClipPathClipper::PathBasedClip(object_)) {
          path->Translate(paint_offset);
          std::optional<FloatRoundedRect> rrect;
          // TODO(crbug.com/337191311): The optimization breaks view-transition
          // if the bounding box of clip-path is larger than the contents.
          if (!(full_context_.direct_compositing_reasons &
                (CompositingReason::kViewTransitionElement |
                 CompositingReason::
                     kViewTransitionElementDescendantWithClipPath))) {
            rrect = PathToRRect(*path);
          }
          ClipPaintPropertyNode::State state(
              *context_.current.transform, *clip_path_bounding_box_,
              rrect.value_or(FloatRoundedRect(
                  gfx::ToEnclosingRect(*clip_path_bounding_box_))));
          if (!rrect) {
            state.clip_path = path;
          }
          OnUpdateClip(properties_->UpdateClipPathClip(*context_.current.clip,
                                                       std::move(state)));
        } else {
          // This means that the clip-path is too complex to be represented as a
          // Path. Will create ClipPathMask in UpdateEffect().
          needs_mask_based_clip_path_ = true;
        }
      }
    }

    if (!clip_path_bounding_box_ || needs_mask_based_clip_path_)
      OnClearClip(properties_->ClearClipPathClip());
  }

  if (properties_->ClipPathClip()) {
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = properties_->ClipPathClip();
  }
}

// The clipping behaviour for replaced elements is defined by overflow,
// overflow-clip-margin and paint containment. See resolution at:
// https://github.com/w3c/csswg-drafts/issues/7144#issuecomment-1090933632
static bool ReplacedElementAlwaysClipsToContentBox(
    const LayoutReplaced& replaced) {
  return !replaced.RespectsCSSOverflow();
}

// TODO(wangxianzhu): Combine the logic by overriding LayoutBox::
// ComputeOverflowClipAxes() in LayoutReplaced and subclasses and remove
// this function.
static bool NeedsOverflowClipForReplacedContents(
    const LayoutReplaced& replaced) {
  // <svg> may optionally allow overflow. If an overflow clip is required,
  // always create it without checking whether the actual content overflows.
  if (replaced.IsSVGRoot())
    return To<LayoutSVGRoot>(replaced).ClipsToContentBox();

  // A replaced element with border-radius always clips the content.
  if (replaced.StyleRef().HasBorderRadius())
    return true;

  // ImagePainter (but not painters for LayoutMedia whose IsImage is also true)
  // won't paint outside of the content box.
  if (replaced.IsImage() && !replaced.IsMedia())
    return false;

  // Non-plugin embedded contents are always sized to fit the content box.
  if (replaced.IsLayoutEmbeddedContent() && !replaced.IsEmbeddedObject())
    return false;

  return true;
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  if (const auto* replaced = DynamicTo<LayoutReplaced>(object)) {
    if (ReplacedElementAlwaysClipsToContentBox(*replaced) ||
        replaced->ClipsToContentBox())
      return NeedsOverflowClipForReplacedContents(*replaced);
  }

  if (object.IsSVGViewportContainer() &&
      SVGLayoutSupport::IsOverflowHidden(object))
    return true;

  if (!object.IsBox())
    return false;

  if (!To<LayoutBox>(object).ShouldClipOverflowAlongEitherAxis())
    return false;

  if (IsA<LayoutView>(object) && !object.GetFrame()->ClipsContent())
    return false;

  return true;
}

void FragmentPaintPropertyTreeBuilder::UpdateLocalBorderBoxContext() {
  if (!NeedsPaintPropertyUpdate())
    return;

  const TransformPaintPropertyNodeOrAlias* old_transform = nullptr;
  const ClipPaintPropertyNodeOrAlias* old_clip = nullptr;
  const EffectPaintPropertyNodeOrAlias* old_effect = nullptr;
  if (fragment_data_.HasLocalBorderBoxProperties()) {
    old_transform = &fragment_data_.LocalBorderBoxProperties().Transform();
    old_clip = &fragment_data_.LocalBorderBoxProperties().Clip();
    old_effect = &fragment_data_.LocalBorderBoxProperties().Effect();
  }
  const TransformPaintPropertyNodeOrAlias* new_transform = nullptr;
  const ClipPaintPropertyNodeOrAlias* new_clip = nullptr;
  const EffectPaintPropertyNodeOrAlias* new_effect = nullptr;

  if (object_.HasLayer() || properties_ || IsLinkHighlighted(object_) ||
      object_.CanContainFixedPositionObjects() ||
      object_.CanContainAbsolutePositionObjects()) {
    new_transform = context_.current.transform;
    new_clip = context_.current.clip;
    new_effect = context_.current_effect;
    fragment_data_.SetLocalBorderBoxProperties(
        PropertyTreeStateOrAlias(*new_transform, *new_clip, *new_effect));
  } else {
    fragment_data_.ClearLocalBorderBoxProperties();
  }

  if (old_transform != new_transform) {
    properties_changed_.transform_changed =
        PaintPropertyChangeType::kNodeAddedOrRemoved;
    properties_changed_.transform_change_is_scroll_translation_only = false;
  }
  if (old_clip != new_clip) {
    properties_changed_.clip_changed =
        PaintPropertyChangeType::kNodeAddedOrRemoved;
  }
  if (old_effect != new_effect) {
    properties_changed_.effect_changed =
        PaintPropertyChangeType::kNodeAddedOrRemoved;
  }
}

bool FragmentPaintPropertyTreeBuilder::NeedsOverflowControlsClip() const {
  if (!object_.IsScrollContainer())
    return false;

  const auto& box = To<LayoutBox>(object_);
  const auto* scrollable_area = box.GetScrollableArea();
  gfx::Rect scroll_controls_bounds =
      scrollable_area->ScrollCornerAndResizerRect();
  if (const auto* scrollbar = scrollable_area->HorizontalScrollbar())
    scroll_controls_bounds.Union(scrollbar->FrameRect());
  if (const auto* scrollbar = scrollable_area->VerticalScrollbar())
    scroll_controls_bounds.Union(scrollbar->FrameRect());
  gfx::Rect pixel_snapped_border_box_rect(
      gfx::Point(), scrollable_area->PixelSnappedBorderBoxSize());
  return !pixel_snapped_border_box_rect.Contains(scroll_controls_bounds);
}

static bool NeedsInnerBorderRadiusClip(const LayoutObject& object) {
  // If a replaced element always clips to its content box then the border
  // radius clip is applied by OverflowClip node. So we don't need to create an
  // additional clip node for the border radius.
  // If the replaced element respects `overflow` property and can have visible
  // overflow, we use a separate node for the border-radius. This is consistent
  // with other elements which respect `overflow`.
  if (object.IsLayoutReplaced() &&
      ReplacedElementAlwaysClipsToContentBox(To<LayoutReplaced>(object))) {
    return false;
  }

  // The check for overflowing both axes is due to this spec line:
  //   However, when one of overflow-x or overflow-y computes to clip and the
  //   other computes to visible, the clipping region is not rounded.
  // (https://drafts.csswg.org/css-overflow/#corner-clipping).
  return object.StyleRef().HasBorderRadius() && object.IsBox() &&
         NeedsOverflowClip(object) && object.ShouldClipOverflowAlongBothAxis();
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowControlsClip() {
  DCHECK(properties_);

  if (!NeedsPaintPropertyUpdate())
    return;

  if (NeedsOverflowControlsClip()) {
    // Clip overflow controls to the border box rect.
    const auto& clip_rect = PhysicalRect(context_.current.paint_offset,
                                         To<LayoutBox>(object_).Size());
    OnUpdateClip(properties_->UpdateOverflowControlsClip(
        *context_.current.clip,
        ClipPaintPropertyNode::State(*context_.current.transform,
                                     gfx::RectF(clip_rect),
                                     ToSnappedClipRect(clip_rect))));
  } else {
    OnClearClip(properties_->ClearOverflowControlsClip());
  }

  // We don't walk into custom scrollbars in PrePaintTreeWalk because
  // LayoutObjects under custom scrollbars don't support paint properties.
}

static bool NeedsBackgroundClip(const LayoutObject& object) {
  return object.CanCompositeBackgroundAttachmentFixed();
}

void FragmentPaintPropertyTreeBuilder::UpdateBackgroundClip() {
  DCHECK(properties_);

  if (!NeedsPaintPropertyUpdate()) {
    return;
  }

  if (IsMissingActualFragment()) {
    // TODO(crbug.com/1418917): Handle clipping correctly when the ancestor
    // fragment is missing. For now, don't apply any clipping in such
    // situations, since we risk overclipping.
    return;
  }

  if (NeedsBackgroundClip(object_)) {
    DCHECK(!object_.StyleRef().BackgroundLayers().Next());
    const auto& fragment = BoxFragment();
    PhysicalRect clip_rect(context_.current.paint_offset, fragment.Size());
    auto clip = object_.StyleRef().BackgroundLayers().Clip();
    if (clip == EFillBox::kContent || clip == EFillBox::kPadding) {
      PhysicalBoxStrut strut = fragment.Borders();
      if (clip == EFillBox::kContent) {
        strut += fragment.Padding();
      }
      strut.TruncateSides(fragment.SidesToInclude());
      clip_rect.Contract(strut);
    }
    OnUpdateClip(properties_->UpdateBackgroundClip(
        *context_.current.clip,
        ClipPaintPropertyNode::State(*context_.current.transform,
                                     gfx::RectF(clip_rect),
                                     ToSnappedClipRect(clip_rect))));
  } else {
    OnClearClip(properties_->ClearBackgroundClip());
  }

  // BackgroundClip doesn't have descendants, so it doesn't affect the
  // context_.current.affect descendants.clip.
}

static void AdjustRoundedClipForOverflowClipMargin(
    const LayoutBox& box,
    gfx::RectF& layout_clip_rect,
    FloatRoundedRect& paint_clip_rect) {
  const auto& style = box.StyleRef();
  auto overflow_clip_margin = style.OverflowClipMargin();
  if (!overflow_clip_margin || !box.ShouldApplyOverflowClipMargin())
    return;

  // The default rects map to the inner border-radius which is the padding-box.
  // First apply a margin for the reference-box.
  PhysicalBoxStrut outsets;
  switch (overflow_clip_margin->GetReferenceBox()) {
    case StyleOverflowClipMargin::ReferenceBox::kBorderBox:
      outsets = box.BorderOutsets();
      break;
    case StyleOverflowClipMargin::ReferenceBox::kPaddingBox:
      break;
    case StyleOverflowClipMargin::ReferenceBox::kContentBox:
      outsets = -box.PaddingOutsets();
      break;
  }

  outsets.Inflate(overflow_clip_margin->GetMargin());
  layout_clip_rect.Outset(gfx::OutsetsF(outsets));
  paint_clip_rect.OutsetForMarginOrShadow(gfx::OutsetsF(outsets));
}

void FragmentPaintPropertyTreeBuilder::UpdateInnerBorderRadiusClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (IsMissingActualFragment()) {
      // TODO(crbug.com/1418917): Handle clipping correctly when the ancestor
      // fragment is missing. For now, don't apply any clipping in such
      // situations, since we risk overclipping.
      return;
    }
    if (NeedsInnerBorderRadiusClip(object_)) {
      const auto& box = To<LayoutBox>(object_);
      PhysicalRect box_rect(context_.current.paint_offset, box.Size());
      gfx::RectF layout_clip_rect =
          RoundedBorderGeometry::RoundedInnerBorder(box.StyleRef(), box_rect)
              .Rect();
      FloatRoundedRect paint_clip_rect =
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(box.StyleRef(),
                                                                box_rect);

      gfx::Vector2dF offset(-OffsetInStitchedFragments(BoxFragment()));
      layout_clip_rect.Offset(offset);
      paint_clip_rect.Move(offset);

      AdjustRoundedClipForOverflowClipMargin(box, layout_clip_rect,
                                             paint_clip_rect);
      ClipPaintPropertyNode::State state(*context_.current.transform,
                                         layout_clip_rect, paint_clip_rect);
      OnUpdateClip(properties_->UpdateInnerBorderRadiusClip(
          *context_.current.clip, std::move(state)));
    } else {
      OnClearClip(properties_->ClearInnerBorderRadiusClip());
    }
  }

  if (auto* border_radius_clip = properties_->InnerBorderRadiusClip())
    context_.current.clip = border_radius_clip;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (IsMissingActualFragment()) {
      // TODO(crbug.com/1418917): Handle clipping correctly when the ancestor
      // fragment is missing. For now, don't apply any clipping in such
      // situations, since we risk overclipping.
      return;
    }

    if (NeedsOverflowClip(object_)) {
      ClipPaintPropertyNode::State state(*context_.current.transform,
                                         gfx::RectF(), FloatRoundedRect());

      if (object_.IsLayoutReplaced() &&
          ReplacedElementAlwaysClipsToContentBox(To<LayoutReplaced>(object_))) {
        const auto& replaced = To<LayoutReplaced>(object_);

        // Videos need to be pre-snapped so that they line up with the
        // display_rect and can enable hardware overlays. Adjust the base rect
        // here, before applying padding and corner rounding.
        PhysicalRect content_rect(context_.current.paint_offset,
                                  replaced.Size());
        if (IsA<LayoutVideo>(replaced)) {
          content_rect =
              LayoutReplaced::PreSnappedRectForPersistentSizing(content_rect);
        }
        // LayoutReplaced clips the foreground by rounded content box.
        auto clip_rect =
            RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
                replaced.StyleRef(), content_rect,
                PhysicalBoxStrut(
                    -(replaced.PaddingTop() + replaced.BorderTop()),
                    -(replaced.PaddingRight() + replaced.BorderRight()),
                    -(replaced.PaddingBottom() + replaced.BorderBottom()),
                    -(replaced.PaddingLeft() + replaced.BorderLeft())));
        if (replaced.IsLayoutEmbeddedContent()) {
          // Embedded objects are always sized to fit the content rect, but they
          // could overflow by 1px due to pre-snapping. Adjust clip rect to
          // match pre-snapped box as a special case.
          clip_rect.SetRect(
              gfx::RectF(clip_rect.Rect().origin(),
                         gfx::SizeF(replaced.ReplacedContentRect().size)));
        }
        // TODO(crbug.com/1248598): Should we use non-snapped clip rect for
        // the first parameter?
        state.SetClipRect(clip_rect.Rect(), clip_rect);
      } else if (object_.IsBox()) {
        const PhysicalBoxFragment& box_fragment = BoxFragment();
        PhysicalRect clip_rect =
            box_fragment.OverflowClipRect(context_.current.paint_offset,
                                          FindPreviousBreakToken(box_fragment));

        if (object_.IsLayoutReplaced()) {
          // TODO(crbug.com/1248598): Should we use non-snapped clip rect for
          // the first parameter?
          auto snapped_rect = ToSnappedClipRect(clip_rect);
          state.SetClipRect(snapped_rect.Rect(), snapped_rect);
        } else {
          state.SetClipRect(gfx::RectF(clip_rect),
                            ToSnappedClipRect(clip_rect));
        }

        state.layout_clip_rect_excluding_overlay_scrollbars =
            FloatClipRect(gfx::RectF(To<LayoutBox>(object_).OverflowClipRect(
                context_.current.paint_offset,
                kExcludeOverlayScrollbarSizeForHitTesting)));
      } else {
        DCHECK(object_.IsSVGViewportContainer());
        const auto& viewport_container =
            To<LayoutSVGViewportContainer>(object_);
        const auto clip_rect =
            viewport_container.LocalToSVGParentTransform().Inverse().MapRect(
                viewport_container.Viewport());
        // TODO(crbug.com/1248598): Should we use non-snapped clip rect for
        // the first parameter?
        state.SetClipRect(clip_rect, FloatRoundedRect(clip_rect));
      }
      OnUpdateClip(properties_->UpdateOverflowClip(*context_.current.clip,
                                                   std::move(state)));
    } else {
      OnClearClip(properties_->ClearOverflowClip());
    }
  }

  if (auto* overflow_clip = properties_->OverflowClip())
    context_.current.clip = overflow_clip;
}

static gfx::PointF PerspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Perspective origin has no effect without perspective.
  DCHECK(style.HasPerspective());
  return PointForLengthPoint(style.PerspectiveOrigin(), gfx::SizeF(box.Size()));
}

static bool NeedsPerspective(const LayoutObject& object) {
  return object.IsBox() && object.StyleRef().HasPerspective();
}

void FragmentPaintPropertyTreeBuilder::UpdatePerspective() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsPerspective(object_)) {
      const ComputedStyle& style = object_.StyleRef();
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      gfx::Transform matrix;
      matrix.ApplyPerspectiveDepth(style.UsedPerspective());
      TransformPaintPropertyNode::State state{
          {matrix,
           gfx::Point3F(PerspectiveOrigin(To<LayoutBox>(object_)) +
                        gfx::Vector2dF(context_.current.paint_offset))}};
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;
      state.rendering_context_id = context_.rendering_context_id;
      OnUpdateTransform(properties_->UpdatePerspective(
          *context_.current.transform, std::move(state)));
    } else {
      OnClearTransform(properties_->ClearPerspective());
    }
  }

  if (properties_->Perspective()) {
    context_.current.transform = properties_->Perspective();
    context_.should_flatten_inherited_transform = false;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateReplacedContentTransform() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate() && !NeedsReplacedContentTransform(object_)) {
    OnClearTransform(properties_->ClearReplacedContentTransform());
  } else if (NeedsPaintPropertyUpdate()) {
    AffineTransform content_to_parent_space;
    if (object_.IsSVGRoot()) {
      content_to_parent_space =
          SVGRootPainter(To<LayoutSVGRoot>(object_))
              .TransformToPixelSnappedBorderBox(context_.current.paint_offset);
    } else if (object_.IsLayoutEmbeddedContent()) {
      content_to_parent_space =
          To<LayoutEmbeddedContent>(object_).EmbeddedContentTransform();
    }
    if (!content_to_parent_space.IsIdentity()) {
      TransformPaintPropertyNode::State state;
      state.transform_and_origin = {content_to_parent_space.ToTransform()};
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;
      state.rendering_context_id = context_.rendering_context_id;
      OnUpdateTransform(properties_->UpdateReplacedContentTransform(
          *context_.current.transform, std::move(state)));
    } else {
      OnClearTransform(properties_->ClearReplacedContentTransform());
    }
  }

  if (properties_->ReplacedContentTransform()) {
    context_.current.transform = properties_->ReplacedContentTransform();
    context_.should_flatten_inherited_transform = true;
    context_.rendering_context_id = 0;
  }

  if (object_.IsSVGRoot()) {
    // SVG painters don't use paint offset. The paint offset is baked into
    // the transform node instead.
    context_.current.paint_offset = PhysicalOffset();
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();
  }
}

MainThreadScrollingReasons
FragmentPaintPropertyTreeBuilder::GetMainThreadScrollingReasons(
    bool user_scrollable) const {
  DCHECK(IsA<LayoutBox>(object_));
  auto* scrollable_area = To<LayoutBox>(object_).GetScrollableArea();
  DCHECK(scrollable_area);
  MainThreadScrollingReasons reasons =
      full_context_.global_main_thread_scrolling_reasons;
  if (scrollable_area->BackgroundNeedsRepaintOnScroll()) {
    reasons |= cc::MainThreadScrollingReason::kBackgroundNeedsRepaintOnScroll;
  }
  // Use main-thread scrolling if the scroller is not user scrollable
  // because the cull rect is not expanded (see CanExpandForScroll in
  // cull_rect.cc), and the scroller is not registered in
  // LocalFrameView::UserScrollableAreas().
  // TODO(crbug.com/349864862): Even if we expand cull rect,
  // virtual/threaded-prefer-compositing/fast/scroll-behavior/overflow-hidden-*.html
  // will still time out, which will need investigating if we want to improve
  // scroll performance of non-user-scrollable scrollers.
  if (!user_scrollable) {
    reasons |= cc::MainThreadScrollingReason::kPreferNonCompositedScrolling;
  }
  return reasons;
}

void FragmentPaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsScrollNode(object_, full_context_.direct_compositing_reasons)) {
      const auto& box = To<LayoutBox>(object_);
      PaintLayerScrollableArea* scrollable_area = box.GetScrollableArea();
      ScrollPaintPropertyNode::State state;

      PhysicalRect clip_rect =
          box.OverflowClipRect(context_.current.paint_offset);
      state.container_rect = ToPixelSnappedRect(clip_rect);
      state.contents_size =
          scrollable_area->PixelSnappedContentsSize(clip_rect.offset);
      state.overflow_clip_node = properties_->OverflowClip();
      state.user_scrollable_horizontal =
          scrollable_area->UserInputScrollable(kHorizontalScrollbar);
      state.user_scrollable_vertical =
          scrollable_area->UserInputScrollable(kVerticalScrollbar);
      state.composited_scrolling_preference =
          static_cast<CompositedScrollingPreference>(
              full_context_.composited_scrolling_preference);
      state.main_thread_scrolling_reasons = GetMainThreadScrollingReasons(
          state.user_scrollable_horizontal || state.user_scrollable_vertical);

      state.compositor_element_id = scrollable_area->GetScrollElementId();

      state.overscroll_behavior =
          cc::OverscrollBehavior(static_cast<cc::OverscrollBehavior::Type>(
                                     box.StyleRef().OverscrollBehaviorX()),
                                 static_cast<cc::OverscrollBehavior::Type>(
                                     box.StyleRef().OverscrollBehaviorY()));

      state.snap_container_data =
          box.GetScrollableArea() &&
                  box.GetScrollableArea()->GetSnapContainerData()
              ? std::optional<cc::SnapContainerData>(
                    *box.GetScrollableArea()->GetSnapContainerData())
              : std::nullopt;

      OnUpdateScroll(properties_->UpdateScroll(*context_.current.scroll,
                                               std::move(state)));

      // While in a view transition, page content is painted into a "snapshot"
      // surface by creating a new effect node to force a separate surface.
      // e.g.:
      //    #Root
      //      +--ViewTransitionEffect
      //         +--PageContentEffect
      //            +--...
      // However, frame scrollbars paint after all other content so the paint
      // chunks look like this:
      // [
      //    ...
      //    FrameBackground (effect: ViewTransitionEffect),
      //    PageContent (effect: PageContentEffect),
      //    FrameScrollbar (effect ViewTransitionEffect),
      //    ...
      // ]
      // The non-contiguous node causes the creation of two compositor effect
      // nodes from this one paint effect node which isn't supported by view
      // transitions. Create a separate effect node, a child of the root, for
      // any frame scrollbars so that:
      // 1) they don't cause multiple compositor effect nodes for a view
      //    transition
      // 2) scrollbars aren't captured in the root snapshot.
      bool transition_forces_scrollbar_effect_nodes =
          object_.IsLayoutView() &&
          ViewTransitionUtils::GetTransition(object_.GetDocument());

      // Overflow controls are not clipped by InnerBorderRadiusClip or
      // OverflowClip, so the output clip should skip them.
      const auto* overflow_control_effect_output_clip = context_.current.clip;
      if (const auto* clip_to_skip = properties_->InnerBorderRadiusClip()
                                         ? properties_->InnerBorderRadiusClip()
                                         : properties_->OverflowClip()) {
        overflow_control_effect_output_clip = clip_to_skip->Parent();
      }

      auto setup_scrollbar_effect_node =
          [this, scrollable_area, transition_forces_scrollbar_effect_nodes,
           overflow_control_effect_output_clip](
              ScrollbarOrientation orientation) {
            Scrollbar* scrollbar = scrollable_area->GetScrollbar(orientation);

            bool scrollbar_is_overlay =
                scrollbar && scrollbar->IsOverlayScrollbar();

            bool needs_effect_node =
                scrollbar && (transition_forces_scrollbar_effect_nodes ||
                              scrollbar_is_overlay);

            if (needs_effect_node) {
              EffectPaintPropertyNode::State effect_state;
              effect_state.local_transform_space = context_.current.transform;
              effect_state.output_clip = overflow_control_effect_output_clip;
              effect_state.compositor_element_id =
                  scrollable_area->GetScrollbarElementId(orientation);

              if (scrollbar_is_overlay) {
                effect_state.direct_compositing_reasons =
                    CompositingReason::kActiveOpacityAnimation;
              }

              const EffectPaintPropertyNodeOrAlias* parent =
                  transition_forces_scrollbar_effect_nodes
                      ? &EffectPaintPropertyNode::Root()
                      : context_.current_effect;

              PaintPropertyChangeType change_type =
                  orientation == ScrollbarOrientation::kHorizontalScrollbar
                      ? properties_->UpdateHorizontalScrollbarEffect(
                            *parent, std::move(effect_state))
                      : properties_->UpdateVerticalScrollbarEffect(
                            *parent, std::move(effect_state));
              OnUpdateEffect(change_type);
            } else {
              bool result =
                  orientation == ScrollbarOrientation::kHorizontalScrollbar
                      ? properties_->ClearHorizontalScrollbarEffect()
                      : properties_->ClearVerticalScrollbarEffect();
              OnClearEffect(result);
            }
          };

      setup_scrollbar_effect_node(ScrollbarOrientation::kVerticalScrollbar);
      setup_scrollbar_effect_node(ScrollbarOrientation::kHorizontalScrollbar);

      bool has_scroll_corner =
          scrollable_area->HorizontalScrollbar() &&
          scrollable_area->VerticalScrollbar() &&
          !scrollable_area->VerticalScrollbar()->IsOverlayScrollbar();
      DCHECK(!has_scroll_corner ||
             !scrollable_area->HorizontalScrollbar()->IsOverlayScrollbar());

      if (transition_forces_scrollbar_effect_nodes && has_scroll_corner) {
        // The scroll corner needs to paint with the scrollbars during a
        // transition, for the same reason as explained above. Scroll corners
        // are only painted for non-overlay scrollbars.
        EffectPaintPropertyNode::State effect_state;
        effect_state.local_transform_space = context_.current.transform;
        effect_state.output_clip = overflow_control_effect_output_clip;
        effect_state.compositor_element_id =
            scrollable_area->GetScrollCornerElementId();
        OnUpdateEffect(properties_->UpdateScrollCornerEffect(
            EffectPaintPropertyNode::Root(), std::move(effect_state)));
      } else {
        OnClearEffect(properties_->ClearScrollCornerEffect());
      }
    } else {
      OnClearScroll(properties_->ClearScroll());
      OnClearEffect(properties_->ClearVerticalScrollbarEffect());
      OnClearEffect(properties_->ClearHorizontalScrollbarEffect());
      OnClearEffect(properties_->ClearScrollCornerEffect());
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(
            object_, full_context_.direct_compositing_reasons)) {
      const auto& box = To<LayoutBox>(object_);
      DCHECK(box.GetScrollableArea());

      gfx::PointF scroll_position = box.GetScrollableArea()->ScrollPosition();
      TransformPaintPropertyNode::State state{{gfx::Transform::MakeTranslation(
          -scroll_position.OffsetFromOrigin())}};
      if (!box.GetScrollableArea()->PendingScrollAnchorAdjustment().IsZero()) {
        context_.current.pending_scroll_anchor_adjustment +=
            box.GetScrollableArea()->PendingScrollAnchorAdjustment();
        box.GetScrollableArea()->ClearPendingScrollAnchorAdjustment();
      }
      state.flattens_inherited_transform =
          context_.should_flatten_inherited_transform;
      state.rendering_context_id = context_.rendering_context_id;
      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReason::kDirectReasonsForScrollTranslationProperty;
      state.scroll = properties_->Scroll();

      // The scroll translation node always inherits backface visibility, which
      // means if scroll and transform are both present, we will use the
      // transform property tree node to determine visibility of the scrolling
      // contents.
      DCHECK_EQ(state.backface_visibility,
                TransformPaintPropertyNode::BackfaceVisibility::kInherited);

      auto effective_change_type = properties_->UpdateScrollTranslation(
          *context_.current.transform, std::move(state));
      // Even if effective_change_type is kUnchanged, we might still need to
      // DirectlyUpdateScrollOffsetTransform, in case the cc::TransformNode
      // was also updated in LayerTreeHost::ApplyCompositorChanges.
      if (effective_change_type <=
              PaintPropertyChangeType::kChangedOnlySimpleValues &&
          // In platform code, only scroll translations with scroll nodes are
          // treated as scroll translations with overlap testing treatment.
          // A scroll translation without a scroll node (see NeedsScrollNode)
          // needs full PaintArtifactCompositor update on scroll.
          properties_->Scroll()) {
        if (auto* paint_artifact_compositor =
                object_.GetFrameView()->GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdateScrollOffsetTransform(
                  *properties_->ScrollTranslation());
          if (updated &&
              effective_change_type ==
                  PaintPropertyChangeType::kChangedOnlySimpleValues) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            properties_->ScrollTranslation()->CompositorSimpleValuesUpdated();
          }
        }
      }
      OnUpdateScrollTranslation(effective_change_type);
    } else {
      OnClearTransform(properties_->ClearScrollTranslation());
    }
  }

  if (properties_->Scroll())
    context_.current.scroll = properties_->Scroll();

  if (const auto* scroll_translation = properties_->ScrollTranslation()) {
    context_.current.transform = scroll_translation;
    // See comments for ScrollTranslation in object_paint_properties.h for the
    // reason of adding ScrollOrigin().
    context_.current.paint_offset +=
        PhysicalOffset(To<LayoutBox>(object_).ScrollOrigin());
    // A scroller creates a layout shift root, so we just calculate one scroll
    // offset delta without accumulation.
    context_.current.scroll_offset_to_layout_shift_root_delta =
        scroll_translation->Get2dTranslation() -
        full_context_.old_scroll_offset;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateOutOfFlowContext() {
  if (!object_.IsBoxModelObject() && !properties_)
    return;

  if (object_.CanContainAbsolutePositionObjects())
    context_.absolute_position = context_.current;

  if (IsA<LayoutView>(object_)) {
    const auto* initial_fixed_transform = context_.fixed_position.transform;

    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = true;

    // Fixed position transform should not be affected.
    context_.fixed_position.transform = initial_fixed_transform;

    // Scrolling in a fixed position element should chain up through the
    // LayoutView.
    if (properties_->Scroll())
      context_.fixed_position.scroll = properties_->Scroll();
    if (properties_->ScrollTranslation()) {
      // Also undo the ScrollOrigin part in paint offset that was added when
      // ScrollTranslation was updated.
      context_.fixed_position.paint_offset -=
          PhysicalOffset(To<LayoutBox>(object_).ScrollOrigin());
    }
  } else if (object_.CanContainFixedPositionObjects()) {
    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = false;
  } else if (properties_ && properties_->CssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* css_clip = properties_->CssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context_.fixed_position.clip == css_clip->Parent()) {
      context_.fixed_position.clip = css_clip;
    } else {
      if (NeedsPaintPropertyUpdate()) {
        OnUpdateClip(properties_->UpdateCssClipFixedPosition(
            *context_.fixed_position.clip,
            ClipPaintPropertyNode::State(css_clip->LocalTransformSpace(),
                                         css_clip->LayoutClipRect().Rect(),
                                         css_clip->PaintClipRect())));
      }
      if (properties_->CssClipFixedPosition())
        context_.fixed_position.clip = properties_->CssClipFixedPosition();
      return;
    }
  }

  if (NeedsPaintPropertyUpdate() && properties_)
    OnClearClip(properties_->ClearCssClipFixedPosition());
}

void FragmentPaintPropertyTreeBuilder::UpdateTransformIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdateTransform(properties_->UpdateTransformIsolationNode(
          *context_.current.transform));
    } else {
      OnClearTransform(properties_->ClearTransformIsolationNode());
    }
  }
  if (properties_->TransformIsolationNode())
    context_.current.transform = properties_->TransformIsolationNode();
}

void FragmentPaintPropertyTreeBuilder::UpdateEffectIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdateEffect(
          properties_->UpdateEffectIsolationNode(*context_.current_effect));
    } else {
      OnClearEffect(properties_->ClearEffectIsolationNode());
    }
  }
  if (properties_->EffectIsolationNode())
    context_.current_effect = properties_->EffectIsolationNode();
}

void FragmentPaintPropertyTreeBuilder::UpdateClipIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdateClip(
          properties_->UpdateClipIsolationNode(*context_.current.clip));
    } else {
      OnClearClip(properties_->ClearClipIsolationNode());
    }
  }
  if (properties_->ClipIsolationNode())
    context_.current.clip = properties_->ClipIsolationNode();
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffset() {
  if (object_.IsBoxModelObject()) {
    const auto& box_model_object = To<LayoutBoxModelObject>(object_);
    switch (box_model_object.StyleRef().GetPosition()) {
      case EPosition::kStatic:
      case EPosition::kRelative:
        break;
      case EPosition::kAbsolute: {
        DCHECK_EQ(full_context_.container_for_absolute_position,
                  box_model_object.Container());
        SwitchToOOFContext(context_.absolute_position);
        break;
      }
      case EPosition::kSticky:
        break;
      case EPosition::kFixed: {
        DCHECK_EQ(full_context_.container_for_fixed_position,
                  box_model_object.Container());
        SwitchToOOFContext(context_.fixed_position);

        // Fixed-position elements that are fixed to the viewport have a
        // transform above the scroll of the LayoutView. Child content is
        // relative to that transform, and hence the fixed-position element.
        if (context_.fixed_position.fixed_position_children_fixed_to_root)
          context_.current.paint_offset_root = &box_model_object;
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  if (const auto* box = DynamicTo<LayoutBox>(&object_)) {
    if (pre_paint_info_) {
      context_.current.paint_offset += pre_paint_info_->paint_offset;

      // Determine whether we're inside block fragmentation or not. OOF
      // descendants need special treatment inside block fragmentation.
      context_.current.is_in_block_fragmentation =
          pre_paint_info_->fragmentainer_is_oof_containing_block &&
          !BoxFragment().IsMonolithic();
    } else {
      // TODO(pdr): Several calls in this function walk back up the tree to
      // calculate containers (e.g., physicalLocation,
      // offsetForInFlowPosition*).  The containing block and other containers
      // can be stored on PaintPropertyTreeBuilderFragmentContext instead of
      // recomputing them.
      context_.current.paint_offset += box->PhysicalLocation();
    }
  }

  context_.current.additional_offset_to_layout_shift_root_delta +=
      context_.pending_additional_offset_to_layout_shift_root_delta;
  context_.pending_additional_offset_to_layout_shift_root_delta =
      PhysicalOffset();
}

void FragmentPaintPropertyTreeBuilder::SetNeedsPaintPropertyUpdateIfNeeded() {
  if (PrePaintDisableSideEffectsScope::IsDisabled()) {
    return;
  }

  if (object_.HasLayer()) {
    PaintLayer* layer = To<LayoutBoxModelObject>(object_).Layer();
    layer->UpdateFilterReferenceBox();
  }

  if (!object_.IsBox())
    return;

  const LayoutBox& box = To<LayoutBox>(object_);

  if (box.IsLayoutReplaced() &&
      box.PreviousPhysicalContentBoxRect() != box.PhysicalContentBoxRect()) {
    box.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
    if (box.IsLayoutEmbeddedContent()) {
      if (const auto* child_view =
              To<LayoutEmbeddedContent>(box).ChildLayoutView()) {
        child_view->GetMutableForPainting()
            .SetOnlyThisNeedsPaintPropertyUpdate();
      }
    }
  }

  // We could check the change of border-box, padding-box or content-box
  // according to background-clip, but checking layout change is much simpler
  // and good enough for the rare cases of NeedsBackgroundClip().
  if (NeedsBackgroundClip(box) && box.ShouldCheckLayoutForPaintInvalidation()) {
    box.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
  }

  // If we reach FragmentPaintPropertyTreeBuilder for an object needing a
  // pending transform update, we need to go ahead and do a regular transform
  // update so that the context (e.g.,
  // |translation_2d_to_layout_shift_root_delta|) is updated properly.
  // See: ../paint/README.md#Transform-update-optimization for more on
  // optimized transform updates
  if (object_.GetFrameView()->RemovePendingTransformUpdate(object_))
    object_.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
  if (object_.GetFrameView()->RemovePendingOpacityUpdate(object_))
    object_.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();

  if (box.Size() == box.PreviousSize()) {
    return;
  }

  // The overflow clip paint property depends on the border box rect through
  // overflowClipRect(). The border box rect's size equals the frame rect's
  // size so we trigger a paint property update when the frame rect changes.
  if (NeedsOverflowClip(box) || NeedsInnerBorderRadiusClip(box) ||
      // The used value of CSS clip may depend on size of the box, e.g. for
      // clip: rect(auto auto auto -5px).
      NeedsCssClip(box) ||
      // Relative lengths (e.g., percentage values) in transform, perspective,
      // transform-origin, and perspective-origin can depend on the size of the
      // frame rect, so force a property update if it changes. TODO(pdr): We
      // only need to update properties if there are relative lengths.
      box.HasTransform() || NeedsPerspective(box) ||
      // CSS mask and clip-path comes with an implicit clip to the border box.
      box.HasMask() || box.HasClipPath() ||
      // Backdrop-filter's bounds use the border box rect.
      !box.StyleRef().BackdropFilter().IsEmpty()) {
    box.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
  }

  // The filter generated for reflection depends on box size.
  if (box.HasReflection()) {
    DCHECK(box.HasLayer());
    box.Layer()->SetFilterOnEffectNodeDirty();
    box.GetMutableForPainting().SetOnlyThisNeedsPaintPropertyUpdate();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForObjectLocation(
    std::optional<gfx::Vector2d>& paint_offset_translation) {
  context_.old_paint_offset = fragment_data_.PaintOffset();
  UpdatePaintOffset();
  UpdateForPaintOffsetTranslation(paint_offset_translation);

  PhysicalOffset paint_offset_delta =
      fragment_data_.PaintOffset() - context_.current.paint_offset;
  if (!paint_offset_delta.IsZero() &&
      !PrePaintDisableSideEffectsScope::IsDisabled()) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    full_context_.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    object_.GetMutableForPainting().SetShouldCheckForPaintInvalidation();
    fragment_data_.SetPaintOffset(context_.current.paint_offset);

    if (object_.IsBox()) {
      // See PaintLayerScrollableArea::PixelSnappedBorderBoxSize() for the
      // reason of this.
      if (auto* scrollable_area = To<LayoutBox>(object_).GetScrollableArea())
        scrollable_area->PositionOverflowControls();
    }
    if (!RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
      object_.GetMutableForPainting()
          .InvalidateIntersectionObserverCachedRects();
    }
  }

  if (paint_offset_translation)
    context_.current.paint_offset_root = &To<LayoutBoxModelObject>(object_);
}

static bool IsLayoutShiftRoot(const LayoutObject& object,
                              const FragmentData& fragment) {
  const auto* properties = fragment.PaintProperties();
  if (!properties)
    return false;
  if (IsA<LayoutView>(object))
    return true;
  for (const TransformPaintPropertyNode* transform :
       properties->AllCSSTransformPropertiesOutsideToInside()) {
    if (transform && IsLayoutShiftRootTransform(*transform))
      return true;
  }
  if (properties->ReplacedContentTransform())
    return true;
  if (properties->TransformIsolationNode())
    return true;
  if (auto* offset_translation = properties->PaintOffsetTranslation()) {
    if (offset_translation->RequiresCompositingForFixedPosition() &&
        // This is to keep the de facto CLS behavior with crrev.com/1036822.
        object.GetFrameView()->LayoutViewport()->HasOverflow()) {
      return true;
    }
  }
  if (properties->StickyTranslation()) {
    return true;
  }
  if (properties->AnchorPositionScrollTranslation()) {
    return true;
  }
  if (properties->OverflowClip())
    return true;
  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateForSelf() {
#if DCHECK_IS_ON()
  bool should_check_paint_under_invalidation =
      RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      !PrePaintDisableSideEffectsScope::IsDisabled();
  std::optional<FindPaintOffsetNeedingUpdateScope> check_paint_offset;
  if (should_check_paint_under_invalidation) {
    check_paint_offset.emplace(object_, fragment_data_,
                               full_context_.is_actually_needed);
  }
#endif

  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without NeedsPaintPropertyUpdate.
  std::optional<gfx::Vector2d> paint_offset_translation;
  UpdateForObjectLocation(paint_offset_translation);
  if (&fragment_data_ == &object_.FirstFragment())
    SetNeedsPaintPropertyUpdateIfNeeded();

  if (properties_) {
    // Update of PaintOffsetTranslation is checked by
    // FindPaintOffsetNeedingUpdateScope.
    UpdatePaintOffsetTranslation(paint_offset_translation);
  }

#if DCHECK_IS_ON()
  std::optional<FindPropertiesNeedingUpdateScope> check_paint_properties;
  if (should_check_paint_under_invalidation) {
    bool force_subtree_update = full_context_.force_subtree_update_reasons;
    check_paint_properties.emplace(object_, fragment_data_,
                                   force_subtree_update);
  }
#endif

  if (properties_) {
    UpdateStickyTranslation();
    UpdateAnchorPositionScrollTranslation();
    if (object_.IsSVGChild()) {
      // TODO(crbug.com/1278452): Merge SVG handling into the primary codepath.
      UpdateTransformForSVGChild(full_context_.direct_compositing_reasons);
    } else {
      UpdateTranslate();
      UpdateRotate();
      UpdateScale();
      UpdateOffset();
      UpdateTransform();
    }
    UpdateElementCaptureEffect();
    UpdateViewTransitionSubframeRootEffect();

    // When layered capture is enabled (see the inverse condition below), the
    // effects (clip/clip-path/opacity/mask/filter) are rendered in an ancestor
    // of the view transition capture. The corresponding CSS is copied to the
    // view-transition pseudo-elements instead of being captured into the
    // texture as content.
    if (!RuntimeEnabledFeatures::ViewTransitionLayeredCaptureEnabled()) {
      UpdateViewTransitionEffect();
      UpdateViewTransitionClip();
    }
    UpdateClipPathClip();
    UpdateEffect();
    UpdateCssClip();
    UpdateFilter();

    // See comment above in inverse condition.
    if (RuntimeEnabledFeatures::ViewTransitionLayeredCaptureEnabled()) {
      UpdateViewTransitionEffect();
      UpdateViewTransitionClip();
    }
    UpdateOverflowControlsClip();
    UpdateBackgroundClip();
  } else if (!object_.IsAnonymous()) {
    // 3D rendering contexts follow the DOM ancestor chain, so
    // flattening should apply regardless of presence of transform.
    context_.rendering_context_id = 0;
    context_.should_flatten_inherited_transform = true;
  }
  UpdateLocalBorderBoxContext();
  UpdateLayoutShiftRootChanged(IsLayoutShiftRoot(object_, fragment_data_));

  // For LayoutView, additional_offset_to_layout_shift_root_delta applies to
  // neither itself nor descendants. For other layout shift roots, we clear the
  // delta at the end of UpdateForChildren() because the delta still applies to
  // the object itself. Same for translation_2d_to_layout_shift_delta and
  // scroll_offset_to_layout_shift_root_delta.
  if (IsA<LayoutView>(object_)) {
    context_.current.additional_offset_to_layout_shift_root_delta =
        PhysicalOffset();
    context_.translation_2d_to_layout_shift_root_delta = gfx::Vector2dF();
    context_.current.scroll_offset_to_layout_shift_root_delta =
        gfx::Vector2dF();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForChildren() {
#if DCHECK_IS_ON()
  // Will be used though a reference by check_paint_offset, so it's declared
  // here to out-live check_paint_offset. It's false because paint offset
  // should not change during this function.
  const bool needs_paint_offset_update = false;
  std::optional<FindPaintOffsetNeedingUpdateScope> check_paint_offset;
  std::optional<FindPropertiesNeedingUpdateScope> check_paint_properties;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() &&
      !PrePaintDisableSideEffectsScope::IsDisabled()) {
    check_paint_offset.emplace(object_, fragment_data_,
                               needs_paint_offset_update);
    bool force_subtree_update = full_context_.force_subtree_update_reasons;
    check_paint_properties.emplace(object_, fragment_data_,
                                   force_subtree_update);
  }
#endif

  // Child transform nodes should not inherit backface visibility if the parent
  // transform node preserves 3d. This is before UpdatePerspective() because
  // perspective itself doesn't affect backface visibility inheritance.
  context_.can_inherit_backface_visibility =
      context_.should_flatten_inherited_transform;

  if (properties_) {
    UpdateInnerBorderRadiusClip();
    UpdateOverflowClip();
    UpdatePerspective();
    UpdateReplacedContentTransform();
    UpdateScrollAndScrollTranslation();
    UpdateTransformIsolationNode();
    UpdateEffectIsolationNode();
    UpdateClipIsolationNode();
  }
  UpdateOutOfFlowContext();

  bool is_layout_shift_root = IsLayoutShiftRoot(object_, fragment_data_);
  UpdateLayoutShiftRootChanged(is_layout_shift_root);
  if (full_context_.was_layout_shift_root || is_layout_shift_root) {
    // A layout shift root (e.g. with mere OverflowClip) may have non-zero
    // paint offset. Exclude the layout shift root's paint offset delta from
    // additional_offset_to_layout_shift_root_delta.
    context_.current.additional_offset_to_layout_shift_root_delta =
        context_.old_paint_offset - fragment_data_.PaintOffset();
    context_.translation_2d_to_layout_shift_root_delta = gfx::Vector2dF();
    // Don't reset scroll_offset_to_layout_shift_root_delta if this object has
    // scroll translation because we need to propagate the delta to descendants.
    if (!properties_ || !properties_->ScrollTranslation()) {
      context_.current.scroll_offset_to_layout_shift_root_delta =
          gfx::Vector2dF();
      context_.current.pending_scroll_anchor_adjustment = gfx::Vector2dF();
    }
  }

#if DCHECK_IS_ON()
  if (properties_)
    properties_->Validate();
#endif
}

void FragmentPaintPropertyTreeBuilder::UpdateLayoutShiftRootChanged(
    bool is_layout_shift_root) {
  if (is_layout_shift_root != full_context_.was_layout_shift_root) {
    context_.current.layout_shift_root_changed = true;
  } else if (is_layout_shift_root && full_context_.was_layout_shift_root) {
    context_.current.layout_shift_root_changed = false;
  }
}

}  // namespace

void PaintPropertyTreeBuilder::InitPaintProperties() {
  bool needs_paint_properties =
      ObjectTypeMightNeedPaintProperties() &&
      (NeedsPaintOffsetTranslation(object_, context_.direct_compositing_reasons,
                                   context_.container_for_fixed_position,
                                   context_.painting_layer) ||
       NeedsStickyTranslation(object_) ||
       NeedsAnchorPositionScrollTranslation(object_) ||
       NeedsTranslate(object_, context_.direct_compositing_reasons) ||
       NeedsRotate(object_, context_.direct_compositing_reasons) ||
       NeedsScale(object_, context_.direct_compositing_reasons) ||
       NeedsOffset(object_, context_.direct_compositing_reasons) ||
       NeedsTransform(object_, context_.direct_compositing_reasons) ||
       NeedsEffectIgnoringClipPath(object_,
                                   context_.direct_compositing_reasons) ||
       NeedsClipPathClipOrMask(object_) ||
       NeedsTransformForSVGChild(object_,
                                 context_.direct_compositing_reasons) ||
       NeedsFilter(object_, context_) || NeedsCssClip(object_) ||
       NeedsBackgroundClip(object_) || NeedsInnerBorderRadiusClip(object_) ||
       NeedsOverflowClip(object_) || NeedsPerspective(object_) ||
       NeedsReplacedContentTransform(object_) ||
       NeedsScrollOrScrollTranslation(object_,
                                      context_.direct_compositing_reasons));

  // If the object is a text, none of the above function should return true.
  DCHECK(!needs_paint_properties || !object_.IsText());

  FragmentData& fragment = GetFragmentData();
  if (const auto* properties = fragment.PaintProperties()) {
    if (const auto* translation = properties->PaintOffsetTranslation()) {
      // If there is a paint offset translation, it only causes a net change
      // in additional_offset_to_layout_shift_root_delta by the amount the
      // paint offset translation changed from the prior frame. To implement
      // this, we record a negative offset here, and then re-add it in
      // UpdatePaintOffsetTranslation. The net effect is that the value
      // of additional_offset_to_layout_shift_root_delta is the difference
      // between the old and new paint offset translation.
      context_.fragment_context
          .pending_additional_offset_to_layout_shift_root_delta =
          -PhysicalOffset::FromVector2dFRound(translation->Get2dTranslation());
    }
    gfx::Vector2dF translation2d;
    for (const TransformPaintPropertyNode* transform :
         properties->AllCSSTransformPropertiesOutsideToInside()) {
      if (transform) {
        if (IsLayoutShiftRootTransform(*transform)) {
          translation2d = gfx::Vector2dF();
          break;
        }
        translation2d += transform->Get2dTranslation();
      }
    }
    context_.fragment_context.translation_2d_to_layout_shift_root_delta -=
        translation2d;
  }

  if (needs_paint_properties) {
    fragment.EnsureId();
    fragment.EnsurePaintProperties();
  } else if (auto* properties = fragment.PaintProperties()) {
    if (properties->HasTransformNode()) {
      properties_changed_.transform_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
      properties_changed_.transform_change_is_scroll_translation_only = false;
    }
    if (properties->HasClipNode()) {
      properties_changed_.clip_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    if (properties->HasEffectNode()) {
      properties_changed_.effect_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    if (properties->Scroll()) {
      properties_changed_.scroll_changed =
          PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
    fragment.ClearPaintProperties();
  }

  if (object_.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context_.fragment_context = PaintPropertyTreeBuilderFragmentContext();
    context_.has_svg_hidden_container_ancestor = true;

    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        context_.fragment_context;
    fragment_context.current.paint_offset_root =
        fragment_context.absolute_position.paint_offset_root =
            fragment_context.fixed_position.paint_offset_root = &object_;

    object_.GetMutableForPainting().FragmentList().Shrink(1);
  }

  if (object_.HasLayer()) {
    To<LayoutBoxModelObject>(object_).Layer()->SetIsUnderSVGHiddenContainer(
        context_.has_svg_hidden_container_ancestor);
  }
}

FragmentData& PaintPropertyTreeBuilder::GetFragmentData() const {
  if (pre_paint_info_) {
    CHECK(pre_paint_info_->fragment_data);
    return *pre_paint_info_->fragment_data;
  }
  return object_.GetMutableForPainting().FirstFragment();
}

void PaintPropertyTreeBuilder::UpdateFragmentData() {
  FragmentData& fragment = GetFragmentData();
  if (IsInNGFragmentTraversal()) {
    context_.fragment_context.current.fragmentainer_idx =
        pre_paint_info_->fragmentainer_idx;
  } else {
    DCHECK_EQ(&fragment, &object_.FirstFragment());
    const FragmentDataList& fragment_list =
        object_.GetMutableForPainting().FragmentList();
    wtf_size_t old_fragment_count = fragment_list.size();
    object_.GetMutableForPainting().FragmentList().Shrink(1);

    if (context_.fragment_context.current.fragmentainer_idx == WTF::kNotFound) {
      // We're not fragmented, but we may have been previously. Reset the
      // fragmentainer index.
      fragment.SetFragmentID(0);

      if (old_fragment_count > 1u) {
        object_.GetMutableForPainting().FragmentCountChanged();
      }
    } else {
      // We're inside monolithic content, but further out there's a
      // fragmentation context. Keep the fragmentainer index, so that the
      // contents end up in the right one.
      fragment.SetFragmentID(
          context_.fragment_context.current.fragmentainer_idx);
    }
  }
}

bool PaintPropertyTreeBuilder::ObjectTypeMightNeedPaintProperties() const {
  return !object_.IsText() && (object_.IsBoxModelObject() || object_.IsSVG());
}

void PaintPropertyTreeBuilder::UpdatePaintingLayer() {
  if (object_.HasLayer() &&
      To<LayoutBoxModelObject>(object_).HasSelfPaintingLayer()) {
    context_.painting_layer = To<LayoutBoxModelObject>(object_).Layer();
  } else if (object_.IsInlineRubyText()) {
    // Physical fragments and fragment items for ruby-text boxes are not
    // managed by inline parents.
    context_.painting_layer = object_.PaintingLayer();
  }
  DCHECK(context_.painting_layer == object_.PaintingLayer());
}

void PaintPropertyTreeBuilder::UpdateForSelf() {
  // These are not inherited from the parent context but calculated here.
  context_.direct_compositing_reasons =
      CompositingReasonFinder::DirectReasonsForPaintProperties(
          object_, context_.container_for_fixed_position);
  if (const auto* box = DynamicTo<LayoutBox>(object_)) {
    box->GetMutableForPainting().UpdateBackgroundPaintLocation();
    if (auto* scrollable_area = box->GetScrollableArea()) {
      bool force_prefer_compositing =
          CompositingReasonFinder::ShouldForcePreferCompositingToLCDText(
              object_, context_.direct_compositing_reasons);
      context_.composited_scrolling_preference = static_cast<unsigned>(
          force_prefer_compositing ? CompositedScrollingPreference::kPreferred
          : scrollable_area->PrefersNonCompositedScrolling()
              ? CompositedScrollingPreference::kNotPreferred
              : CompositedScrollingPreference::kDefault);
    }
  }

  if (Platform::Current()->IsLowEndDevice()) {
    // Don't composite "trivial" 3D transforms such as translateZ(0).
    // These transforms still force comosited scrolling (see above).
    context_.direct_compositing_reasons &=
        ~CompositingReason::kTrivial3DTransform;
  }

  if (context_.fragment_context
          .self_or_ancestor_participates_in_view_transition &&
      object_.StyleRef().HasClipPath()) {
    context_.direct_compositing_reasons |=
        CompositingReason::kViewTransitionElementDescendantWithClipPath;
  }

  context_.was_layout_shift_root =
      IsLayoutShiftRoot(object_, object_.FirstFragment());

  if (IsA<LayoutView>(object_)) {
    UpdateGlobalMainThreadScrollingReasons();
  }

  context_.old_scroll_offset = gfx::Vector2dF();
  if (const auto* properties = object_.FirstFragment().PaintProperties()) {
    if (const auto* old_scroll_translation = properties->ScrollTranslation()) {
      DCHECK(context_.was_layout_shift_root);
      context_.old_scroll_offset = old_scroll_translation->Get2dTranslation();
    }
  }

  UpdatePaintingLayer();
  UpdateFragmentData();
  InitPaintProperties();

  FragmentPaintPropertyTreeBuilder builder(object_, pre_paint_info_, context_,
                                           GetFragmentData());
  builder.UpdateForSelf();
  properties_changed_.Merge(builder.PropertiesChanged());

  if (!PrePaintDisableSideEffectsScope::IsDisabled()) {
    object_.GetMutableForPainting()
        .SetShouldAssumePaintOffsetTranslationForLayoutShiftTracking(false);
  }
}

void PaintPropertyTreeBuilder::UpdateGlobalMainThreadScrollingReasons() {
  DCHECK(IsA<LayoutView>(object_));

  if (object_.GetFrameView()
          ->RequiresMainThreadScrollingForBackgroundAttachmentFixed()) {
    context_.global_main_thread_scrolling_reasons |=
        cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  if (!RuntimeEnabledFeatures::ExcludePopupMainThreadScrollingReasonEnabled() &&
      !object_.GetFrame()->Client()->GetWebFrame()) {
    // If there's no WebFrame, then there's no WebFrameWidget, and we can't do
    // threaded scrolling.  This currently only happens in a WebPagePopup.
    context_.global_main_thread_scrolling_reasons |=
        cc::MainThreadScrollingReason::kPopupNoThreadedInput;
  }

  constexpr auto kGlobalReasons =
      PaintPropertyTreeBuilderContext::kGlobalMainThreadScrollingReasons;
  DCHECK_EQ(context_.global_main_thread_scrolling_reasons & ~kGlobalReasons,
            0u);
  if (auto* properties = object_.FirstFragment().PaintProperties()) {
    if (auto* scroll = properties->Scroll()) {
      if ((scroll->GetMainThreadScrollingReasons() & kGlobalReasons) !=
          context_.global_main_thread_scrolling_reasons) {
        // The changed global_main_thread_scrolling_reasons needs to propagate
        // to all scroll nodes in this view.
        context_.force_subtree_update_reasons |=
            PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
      }
    }
  }
}

void PaintPropertyTreeBuilder::UpdateForChildren() {
  if (!ObjectTypeMightNeedPaintProperties())
    return;

  // For now, only consider single fragment elements as possible isolation
  // boundaries.
  // TODO(crbug.com/890932): See if this is needed.
  bool is_isolated = true;
  FragmentPaintPropertyTreeBuilder builder(object_, pre_paint_info_, context_,
                                           GetFragmentData());
  // The element establishes an isolation boundary if it has isolation nodes
  // before and after updating the children. In other words, if it didn't have
  // isolation nodes previously then we still want to do a subtree walk. If it
  // now doesn't have isolation nodes, then of course it is also not isolated.
  is_isolated &= builder.HasIsolationNodes();
  builder.UpdateForChildren();
  is_isolated &= builder.HasIsolationNodes();

  properties_changed_.Merge(builder.PropertiesChanged());

  if (object_.CanContainAbsolutePositionObjects())
    context_.container_for_absolute_position = &object_;
  if (object_.CanContainFixedPositionObjects())
    context_.container_for_fixed_position = &object_;

  if (properties_changed_.Max() >=
          PaintPropertyChangeType::kNodeAddedOrRemoved ||
      object_.SubtreePaintPropertyUpdateReasons() !=
          static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone)) {
    // Force a piercing subtree update if the scroll tree hierarchy changes
    // because the scroll tree does not have isolation nodes and non-piercing
    // updates can fail to update scroll descendants.
    if (properties_changed_.scroll_changed >=
            PaintPropertyChangeType::kNodeAddedOrRemoved ||
        AreSubtreeUpdateReasonsIsolationPiercing(
            object_.SubtreePaintPropertyUpdateReasons())) {
      context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    } else {
      context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    }
  }

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled() &&
      (properties_changed_.transform_changed >
           (properties_changed_.transform_change_is_scroll_translation_only
                ? PaintPropertyChangeType::kChangedOnlySimpleValues
                : PaintPropertyChangeType::kUnchanged) ||
       properties_changed_.clip_changed > PaintPropertyChangeType::kUnchanged ||
       properties_changed_.scroll_changed >
           PaintPropertyChangeType::kUnchanged)) {
    object_.GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }

  if (is_isolated) {
    context_.force_subtree_update_reasons &=
        ~PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
  }
}

void PaintPropertyTreeBuilder::UpdateForPageBorderBox(
    const PhysicalBoxFragment& page_container) {
  const PhysicalBoxFragment& page_border_box = *pre_paint_info_->box_fragment;
  DCHECK_EQ(page_border_box.GetBoxType(), PhysicalFragment::kPageBorderBox);

  // Since the page border box fragment is responsible for @page borders and
  // other decorations, in addition to the document background, it needs to be
  // in the coordinate system of paginated layout.
  float scale = TargetScaleForPage(page_container);

  PhysicalRect target_content_rect = page_border_box.ContentRect();
  // Scale to the coordinate system of the target (e.g. paper).
  target_content_rect.Scale(scale);
  // The offset, on the other hand, is already in the coordinate system of
  // the target.
  PhysicalOffset page_border_box_offset = pre_paint_info_->paint_offset;
  target_content_rect.offset += page_border_box_offset;
  gfx::Transform matrix =
      gfx::Transform::MakeTranslation(gfx::Vector2dF(page_border_box_offset));
  matrix.Scale(scale);
  TransformPaintPropertyNode::State transform_state{{matrix}};

  PaintPropertyTreeBuilderFragmentContext& fragment_context =
      context_.fragment_context;
  FragmentData& fragment_data = object_.GetMutableForPainting().FirstFragment();
  fragment_data.EnsurePaintProperties().UpdateTransform(
      *fragment_context.current.transform, std::move(transform_state));
  fragment_data.SetLocalBorderBoxProperties(PropertyTreeStateOrAlias(
      *fragment_data.PaintProperties()->Transform(),
      *fragment_context.current.clip, *fragment_context.current_effect));
}

bool PaintPropertyTreeBuilder::ScheduleDeferredTransformNodeUpdate(
    LayoutObject& object) {
  if (CanDoDeferredTransformNodeUpdate(object)) {
    object.GetFrameView()->AddPendingTransformUpdate(object);
    return true;
  }
  return false;
}

bool PaintPropertyTreeBuilder::ScheduleDeferredOpacityNodeUpdate(
    LayoutObject& object) {
  if (CanDoDeferredOpacityNodeUpdate(object)) {
    object.GetFrameView()->AddPendingOpacityUpdate(object);
    return true;
  }
  return false;
}

// Fast-path for directly updating transforms. Returns true if successful. This
// is similar to |FragmentPaintPropertyTreeBuilder::UpdateIndividualTransform|.
void PaintPropertyTreeBuilder::DirectlyUpdateTransformMatrix(
    const LayoutObject& object) {
  DCHECK(CanDoDeferredTransformNodeUpdate(object));

  auto& box = To<LayoutBox>(object);
  const PhysicalRect reference_box = ComputeReferenceBox(box);
  FragmentData* fragment_data = &object.GetMutableForPainting().FirstFragment();
  auto* properties = fragment_data->PaintProperties();
  auto* transform = properties->Transform();
  auto transform_and_origin = TransformAndOriginState(
      box, reference_box,
      [](const LayoutBox& box, const PhysicalRect& reference_box,
         gfx::Transform& matrix) {
        const ComputedStyle& style = box.StyleRef();
        style.ApplyTransform(
            matrix, &box, reference_box,
            ComputedStyle::kIncludeTransformOperations,
            ComputedStyle::kExcludeTransformOrigin,
            ComputedStyle::kExcludeMotionPath,
            ComputedStyle::kExcludeIndependentTransformProperties);
      });

  TransformPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_animation_on_compositor =
      box.StyleRef().IsRunningTransformAnimationOnCompositor();
  auto effective_change_type = properties->DirectlyUpdateTransformAndOrigin(
      std::move(transform_and_origin), animation_state);
  DirectlyUpdateCcTransform(*transform, object, effective_change_type);

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled() &&
      effective_change_type > PaintPropertyChangeType::kUnchanged) {
    object.GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }

  if (effective_change_type >=
      PaintPropertyChangeType::kChangedOnlySimpleValues) {
    object.GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
  }

  PaintPropertiesChangeInfo properties_changed{
      .transform_changed = effective_change_type,
      .transform_change_is_scroll_translation_only = false,
  };
  CullRectUpdater::PaintPropertiesChanged(object, properties_changed);
}

void PaintPropertyTreeBuilder::DirectlyUpdateOpacityValue(
    const LayoutObject& object) {
  DCHECK(CanDoDeferredOpacityNodeUpdate(object));
  const ComputedStyle& style = object.StyleRef();

  EffectPaintPropertyNode::AnimationState animation_state;
  animation_state.is_running_opacity_animation_on_compositor =
      style.IsRunningOpacityAnimationOnCompositor();
  animation_state.is_running_backdrop_filter_animation_on_compositor =
      style.IsRunningBackdropFilterAnimationOnCompositor();

  FragmentData* fragment_data = &object.GetMutableForPainting().FirstFragment();
  auto* properties = fragment_data->PaintProperties();
  auto effective_change_type =
      properties->DirectlyUpdateOpacity(style.Opacity(), animation_state);
  // If we have simple value change, which means opacity, we should try to
  // directly update it on the PaintArtifactCompositor in order to avoid
  // needing to run the property tree builder at all.
  DirectlyUpdateCcOpacity(object, *properties, effective_change_type);

  if (effective_change_type >=
      PaintPropertyChangeType::kChangedOnlySimpleValues) {
    object.GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
  }
}

void PaintPropertyTreeBuilder::IssueInvalidationsAfterUpdate() {
  // We need to update property tree states of paint chunks.
  auto max_change = properties_changed_.Max();
  if (max_change >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
    context_.painting_layer->SetNeedsRepaint();
    if (object_.IsDocumentElement()) {
      // View background painting depends on existence of the document element's
      // paint properties (see callsite of ViewPainter::PaintRootGroup()).
      // Invalidate view background display item clients.
      // SetBackgroundNeedsFullPaintInvalidation() won't work here because we
      // have already walked the LayoutView in PrePaintTreeWalk.
      LayoutView* layout_view = object_.View();
      layout_view->Layer()->SetNeedsRepaint();
      auto reason = PaintInvalidationReason::kBackground;
      static_cast<const DisplayItemClient*>(layout_view)->Invalidate(reason);
      if (auto* scrollable_area = layout_view->GetScrollableArea()) {
        scrollable_area->GetScrollingBackgroundDisplayItemClient().Invalidate(
            reason);
      }
    }
  }

  if (max_change > PaintPropertyChangeType::kChangedOnlyCompositedValues) {
    object_.GetFrameView()->SetPaintArtifactCompositorNeedsUpdate();
  }

  CullRectUpdater::PaintPropertiesChanged(object_, properties_changed_);
}

bool PaintPropertyTreeBuilder::CanDoDeferredTransformNodeUpdate(
    const LayoutObject& object) {
  // If we already need a full update, do not do the direct update.
  if (object.NeedsPaintPropertyUpdate() ||
      object.DescendantNeedsPaintPropertyUpdate()) {
    return false;
  }

  // SVG transforms use a different codepath (see:
  // |FragmentPaintPropertyTreeBuilder::UpdateTransformForSVGChild|).
  if (object.IsSVGChild())
    return false;

  // Only boxes have transform values (see:
  // |FragmentPaintPropertyTreeBuilder::UpdateIndividualTransform|).
  if (!object.IsBox())
    return false;

  // This fast path does not support iterating over each fragment, so do not
  // run the fast path in the presence of fragmentation.
  if (object.IsFragmented()) {
    return false;
  }

  auto* properties = object.FirstFragment().PaintProperties();
  // Cannot directly update properties if they have not been created yet.
  if (!properties || !properties->Transform())
    return false;

  return true;
}

bool PaintPropertyTreeBuilder::CanDoDeferredOpacityNodeUpdate(
    const LayoutObject& object) {
  // If we already need a full update, do not do the direct update.
  if (object.NeedsPaintPropertyUpdate() ||
      object.DescendantNeedsPaintPropertyUpdate()) {
    return false;
  }

  // In some cases where we need to remove the update, objects that are not
  // boxes can cause a bug. (See SetNeedsPaintPropertyUpdateIfNeeded)
  if (!object.IsBox())
    return false;

  // This fast path does not support iterating over each fragment, so do not
  // run the fast path in the presence of fragmentation.
  if (object.IsFragmented()) {
    return false;
  }

  auto* properties = object.FirstFragment().PaintProperties();
  // Cannot directly update properties if they have not been created yet.
  if (!properties || !properties->Effect())
    return false;

  // Descendant state depends on opacity being zero, so we can't do a direct
  // update if it changes
  bool old_opacity_is_zero = properties->Effect()->Opacity() == 0;
  bool new_opacity_is_zero = object.Style()->Opacity() == 0;
  if (old_opacity_is_zero != new_opacity_is_zero) {
    return false;
  }

  return true;
}

}  // namespace blink
