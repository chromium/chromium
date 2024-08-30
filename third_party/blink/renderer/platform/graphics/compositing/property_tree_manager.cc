// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/layers/layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

PropertyTreeManager::EffectState::EffectState(const CurrentEffectState& other)
    : effect_id(other.effect_id),
      effect(other.effect),
      clip(other.clip),
      transform(other.transform),
      may_be_2d_axis_misaligned_to_render_surface(
          other.may_be_2d_axis_misaligned_to_render_surface),
      contained_by_non_render_surface_synthetic_rounded_clip(
          other.contained_by_non_render_surface_synthetic_rounded_clip) {}

PropertyTreeManager::CurrentEffectState::CurrentEffectState(
    const EffectState& other)
    : effect_id(other.effect_id),
      effect(other.effect),
      clip(other.clip),
      transform(other.transform),
      may_be_2d_axis_misaligned_to_render_surface(
          other.may_be_2d_axis_misaligned_to_render_surface),
      contained_by_non_render_surface_synthetic_rounded_clip(
          other.contained_by_non_render_surface_synthetic_rounded_clip) {}

PropertyTreeManager::PropertyTreeManager(PropertyTreeManagerClient& client,
                                         cc::PropertyTrees& property_trees,
                                         cc::Layer& root_layer,
                                         LayerListBuilder& layer_list_builder,
                                         int new_sequence_number)
    : client_(client),
      clip_tree_(property_trees.clip_tree_mutable()),
      effect_tree_(property_trees.effect_tree_mutable()),
      scroll_tree_(property_trees.scroll_tree_mutable()),
      transform_tree_(property_trees.transform_tree_mutable()),
      root_layer_(root_layer),
      layer_list_builder_(layer_list_builder),
      new_sequence_number_(new_sequence_number) {
  SetupRootTransformNode();
  SetupRootClipNode();
  SetupRootEffectNode();
  SetupRootScrollNode();
}

PropertyTreeManager::~PropertyTreeManager() {
  DCHECK(!effect_stack_.size()) << "PropertyTreeManager::Finalize() must be "
                                   "called at the end of tree conversion.";
}

void PropertyTreeManager::Finalize() {
  while (effect_stack_.size())
    CloseCcEffect();

  DCHECK(effect_stack_.empty());

  UpdatePixelMovingFilterClipExpanders();
}

static void UpdateCcTransformLocalMatrix(
    cc::TransformNode& compositor_node,
    const TransformPaintPropertyNode& transform_node) {
  if (transform_node.GetStickyConstraint() ||
      transform_node.GetAnchorPositionScrollData()) {
    // The sticky offset on the blink transform node is pre-computed and stored
    // to the local matrix. Cc applies sticky offset dynamically on top of the
    // local matrix. We should not set the local matrix on cc node if it is a
    // sticky node because the sticky offset would be applied twice otherwise.
    // Same for anchor positioning.
    DCHECK(compositor_node.local.IsIdentity());
    DCHECK_EQ(gfx::Point3F(), compositor_node.origin);
  } else if (transform_node.ScrollNode()) {
    DCHECK(transform_node.IsIdentityOr2dTranslation());
    // Blink creates a 2d transform node just for scroll offset whereas cc's
    // transform node has a special scroll offset field.
    compositor_node.scroll_offset =
        gfx::PointAtOffsetFromOrigin(-transform_node.Get2dTranslation());
    DCHECK(compositor_node.local.IsIdentity());
    DCHECK_EQ(gfx::Point3F(), compositor_node.origin);
  } else {
    DCHECK(!transform_node.ScrollNode());
    compositor_node.local = transform_node.Matrix();
    compositor_node.origin = transform_node.Origin();
  }
  compositor_node.needs_local_transform_update = true;
}

static void SetTransformTreePageScaleFactor(
    cc::TransformTree& transform_tree,
    const cc::TransformNode& page_scale_node) {
  DCHECK(page_scale_node.local.IsScale2d());
  auto page_scale = page_scale_node.local.To2dScale();
  DCHECK_EQ(page_scale.x(), page_scale.y());
  transform_tree.set_page_scale_factor(page_scale.x());
}

bool PropertyTreeManager::DirectlyUpdateCompositedOpacityValue(
    cc::LayerTreeHost& host,
    const EffectPaintPropertyNode& effect) {
  host.WaitForProtectedSequenceCompletion();
  auto* property_trees = host.property_trees();
  auto* cc_effect = property_trees->effect_tree_mutable().Node(
      effect.CcNodeId(property_trees->sequence_number()));
  if (!cc_effect)
    return false;

  // We directly update opacity only when it's not animating in compositor. If
  // the compositor has not cleared is_currently_animating_opacity, we should
  // clear it now to let the compositor respect the new value.
  cc_effect->is_currently_animating_opacity = false;

  cc_effect->opacity = effect.Opacity();
  cc_effect->effect_changed = true;
  property_trees->effect_tree_mutable().set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

bool PropertyTreeManager::DirectlyUpdateScrollOffsetTransform(
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  host.WaitForProtectedSequenceCompletion();
  auto* scroll_node = transform.ScrollNode();
  // Only handle scroll adjustments.
  if (!scroll_node)
    return false;

  auto* property_trees = host.property_trees();
  auto& scroll_tree = property_trees->scroll_tree_mutable();
  auto* cc_scroll_node = scroll_tree.Node(
      scroll_node->CcNodeId(property_trees->sequence_number()));
  if (!cc_scroll_node ||
      scroll_tree.ShouldRealizeScrollsOnMain(*cc_scroll_node)) {
    return false;
  }

  auto* cc_transform = property_trees->transform_tree_mutable().Node(
      transform.CcNodeId(property_trees->sequence_number()));
  if (!cc_transform)
    return false;

  DCHECK(!cc_transform->is_currently_animating);

  gfx::PointF scroll_offset =
      gfx::PointAtOffsetFromOrigin(-transform.Get2dTranslation());
  DirectlySetScrollOffset(host, scroll_node->GetCompositorElementId(),
                          scroll_offset);
  if (cc_transform->scroll_offset != scroll_offset) {
    UpdateCcTransformLocalMatrix(*cc_transform, transform);
    cc_transform->transform_changed = true;
    property_trees->transform_tree_mutable().set_needs_update(true);
    host.SetNeedsCommit();
  }
  return true;
}

bool PropertyTreeManager::DirectlyUpdateTransform(
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  host.WaitForProtectedSequenceCompletion();
  // If we have a ScrollNode, we should be using
  // DirectlyUpdateScrollOffsetTransform().
  DCHECK(!transform.ScrollNode());

  auto* property_trees = host.property_trees();
  auto* cc_transform = property_trees->transform_tree_mutable().Node(
      transform.CcNodeId(property_trees->sequence_number()));
  if (!cc_transform)
    return false;

  UpdateCcTransformLocalMatrix(*cc_transform, transform);

  // We directly update transform only when the transform is not animating in
  // compositor. If the compositor has not cleared the is_currently_animating
  // flag, we should clear it to let the compositor respect the new value.
  cc_transform->is_currently_animating = false;

  cc_transform->transform_changed = true;
  property_trees->transform_tree_mutable().set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

bool PropertyTreeManager::DirectlyUpdatePageScaleTransform(
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  host.WaitForProtectedSequenceCompletion();
  DCHECK(!transform.ScrollNode());

  auto* property_trees = host.property_trees();
  auto* cc_transform = property_trees->transform_tree_mutable().Node(
      transform.CcNodeId(property_trees->sequence_number()));
  if (!cc_transform)
    return false;

  UpdateCcTransformLocalMatrix(*cc_transform, transform);
  SetTransformTreePageScaleFactor(property_trees->transform_tree_mutable(),
                                  *cc_transform);
  cc_transform->transform_changed = true;
  property_trees->transform_tree_mutable().set_needs_update(true);
  return true;
}

void PropertyTreeManager::DirectlySetScrollOffset(
    cc::LayerTreeHost& host,
    CompositorElementId element_id,
    const gfx::PointF& scroll_offset) {
  host.WaitForProtectedSequenceCompletion();
  auto* property_trees = host.property_trees();
  if (property_trees->scroll_tree_mutable().SetScrollOffset(element_id,
                                                            scroll_offset)) {
    // Scroll offset animations are clobbered via |Layer::PushPropertiesTo|.
    if (auto* layer = host.LayerByElementId(element_id))
      layer->SetNeedsPushProperties();
    host.SetNeedsCommit();
  }
}

void PropertyTreeManager::DropCompositorScrollDeltaNextCommit(
    cc::LayerTreeHost& host,
    CompositorElementId element_id) {
  host.DropActiveScrollDeltaNextCommit(element_id);
}

uint32_t PropertyTreeManager::NonCompositedMainThreadScrollingReasons(
    const TransformPaintPropertyNode& scroll_translation) const {
  if (scroll_translation.ScrollNode()->GetCompositedScrollingPreference() ==
      CompositedScrollingPreference::kNotPreferred) {
    return cc::MainThreadScrollingReason::kPreferNonCompositedScrolling;
  }
  if (RuntimeEnabledFeatures::RasterInducingScrollEnabled() &&
      !client_.ShouldForceMainThreadRepaint(scroll_translation)) {
    return cc::MainThreadScrollingReason::kNotScrollingOnMain;
  }
  return cc::MainThreadScrollingReason::kNotOpaqueForTextAndLCDText;
}

uint32_t PropertyTreeManager::GetMainThreadScrollingReasons(
    const cc::LayerTreeHost& host,
    const ScrollPaintPropertyNode& scroll) {
  const auto* property_trees = host.property_trees();
  const auto* cc_scroll = property_trees->scroll_tree().Node(
      scroll.CcNodeId(property_trees->sequence_number()));
  return cc_scroll
             ? cc_scroll->main_thread_scrolling_reasons
             : cc::MainThreadScrollingReason::kPreferNonCompositedScrolling;
}

bool PropertyTreeManager::UsesCompositedScrolling(
    const cc::LayerTreeHost& host,
    const ScrollPaintPropertyNode& scroll) {
  CHECK(!RuntimeEnabledFeatures::RasterInducingScrollEnabled());
  const auto* property_trees = host.property_trees();
  const auto* cc_scroll = property_trees->scroll_tree().Node(
      scroll.CcNodeId(property_trees->sequence_number()));
  return cc_scroll && cc_scroll->is_composited;
}

void PropertyTreeManager::SetupRootTransformNode() {
  // cc is hardcoded to use transform node index 1 for device scale and
  // transform.
  transform_tree_.clear();
  cc::TransformNode& transform_node = *transform_tree_.Node(
      transform_tree_.Insert(cc::TransformNode(), cc::kRootPropertyNodeId));
  DCHECK_EQ(transform_node.id, cc::kSecondaryRootPropertyNodeId);

  // TODO(jaydasika): We shouldn't set ToScreen and FromScreen of root
  // transform node here. They should be set while updating transform tree in
  // cc.
  float device_scale_factor =
      root_layer_.layer_tree_host()->device_scale_factor();
  transform_tree_.set_device_scale_factor(device_scale_factor);
  gfx::Transform to_screen;
  to_screen.Scale(device_scale_factor, device_scale_factor);
  transform_tree_.SetToScreen(cc::kRootPropertyNodeId, to_screen);
  gfx::Transform from_screen = to_screen.GetCheckedInverse();
  transform_tree_.SetFromScreen(cc::kRootPropertyNodeId, from_screen);
  transform_tree_.set_needs_update(true);

  TransformPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                                 transform_node.id);
  root_layer_.SetTransformTreeIndex(transform_node.id);
}

void PropertyTreeManager::SetupRootClipNode() {
  // cc is hardcoded to use clip node index 1 for viewport clip.
  clip_tree_.clear();
  cc::ClipNode& clip_node = *clip_tree_.Node(
      clip_tree_.Insert(cc::ClipNode(), cc::kRootPropertyNodeId));
  DCHECK_EQ(clip_node.id, cc::kSecondaryRootPropertyNodeId);

  // TODO(bokan): This needs to come from the Visual Viewport which will
  // correctly account for the URL bar. In fact, the visual viewport property
  // tree builder should probably be the one to create the property tree state
  // and have this created in the same way as other layers.
  clip_node.clip =
      gfx::RectF(root_layer_.layer_tree_host()->device_viewport_rect());
  clip_node.transform_id = cc::kRootPropertyNodeId;

  ClipPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_, clip_node.id);
  root_layer_.SetClipTreeIndex(clip_node.id);
}

void PropertyTreeManager::SetupRootEffectNode() {
  // cc is hardcoded to use effect node index 1 for root render surface.
  effect_tree_.clear();
  cc::EffectNode& effect_node = *effect_tree_.Node(
      effect_tree_.Insert(cc::EffectNode(), cc::kInvalidPropertyNodeId));
  DCHECK_EQ(effect_node.id, cc::kSecondaryRootPropertyNodeId);

  static UniqueObjectId unique_id = NewUniqueObjectId();

  effect_node.element_id = CompositorElementIdFromUniqueObjectId(unique_id);
  effect_node.transform_id = cc::kRootPropertyNodeId;
  effect_node.clip_id = cc::kSecondaryRootPropertyNodeId;
  effect_node.render_surface_reason = cc::RenderSurfaceReason::kRoot;
  root_layer_.SetEffectTreeIndex(effect_node.id);

  EffectPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                              effect_node.id);
  SetCurrentEffectState(
      effect_node, CcEffectType::kEffect, EffectPaintPropertyNode::Root(),
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root());
}

void PropertyTreeManager::SetupRootScrollNode() {
  scroll_tree_.clear();
  cc::ScrollNode& scroll_node = *scroll_tree_.Node(
      scroll_tree_.Insert(cc::ScrollNode(), cc::kRootPropertyNodeId));
  DCHECK_EQ(scroll_node.id, cc::kSecondaryRootPropertyNodeId);
  scroll_node.transform_id = cc::kSecondaryRootPropertyNodeId;

  ScrollPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                              scroll_node.id);
  root_layer_.SetScrollTreeIndex(scroll_node.id);
}

static bool TransformsToAncestorHaveNonAxisAlignedActiveAnimation(
    const TransformPaintPropertyNode& descendant,
    const TransformPaintPropertyNode& ancestor) {
  if (&descendant == &ancestor)
    return false;
  for (const auto* n = &descendant; n != &ancestor; n = n->UnaliasedParent()) {
    if (n->HasActiveTransformAnimation() &&
        !n->TransformAnimationIsAxisAligned()) {
      return true;
    }
  }
  return false;
}

bool TransformsMayBe2dAxisMisaligned(const TransformPaintPropertyNode& a,
                                     const TransformPaintPropertyNode& b) {
  if (&a == &b)
    return false;
  if (!GeometryMapper::SourceToDestinationProjection(a, b)
           .Preserves2dAxisAlignment()) {
    return true;
  }
  const auto& lca = a.LowestCommonAncestor(b).Unalias();
  if (TransformsToAncestorHaveNonAxisAlignedActiveAnimation(a, lca) ||
      TransformsToAncestorHaveNonAxisAlignedActiveAnimation(b, lca))
    return true;
  return false;
}

// A reason is conditional if it can be omitted if it controls less than two
// composited layers or render surfaces. We set the reason on an effect node
// when updating the cc effect property tree, and remove unnecessary ones in
// UpdateConditionalRenderSurfaceReasons() after layerization.
static bool IsConditionalRenderSurfaceReason(cc::RenderSurfaceReason reason) {
  return reason == cc::RenderSurfaceReason::kBlendModeDstIn ||
         reason == cc::RenderSurfaceReason::kOpacity ||
         reason == cc::RenderSurfaceReason::kOpacityAnimation;
}

void PropertyTreeManager::SetCurrentEffectState(
    const cc::EffectNode& cc_effect_node,
    CcEffectType effect_type,
    const EffectPaintPropertyNode& effect,
    const ClipPaintPropertyNode& clip,
    const TransformPaintPropertyNode& transform) {
  const auto* previous_transform =
      effect.IsRoot() ? nullptr : current_.transform.Get();
  current_.effect_id = cc_effect_node.id;
  current_.effect_type = effect_type;
  current_.effect = &effect;
  current_.clip = &clip;
  current_.transform = &transform;

  if (cc_effect_node.HasRenderSurface() &&
      !IsConditionalRenderSurfaceReason(cc_effect_node.render_surface_reason)) {
    current_.may_be_2d_axis_misaligned_to_render_surface =
        EffectState::kAligned;
    current_.contained_by_non_render_surface_synthetic_rounded_clip = false;
  } else {
    if (current_.may_be_2d_axis_misaligned_to_render_surface ==
            EffectState::kAligned &&
        previous_transform != current_.transform) {
      current_.may_be_2d_axis_misaligned_to_render_surface =
          EffectState::kUnknown;
    }
    current_.contained_by_non_render_surface_synthetic_rounded_clip |=
        (effect_type & CcEffectType::kSyntheticForNonTrivialClip);
  }
}

int PropertyTreeManager::EnsureCompositorTransformNode(
    const TransformPaintPropertyNode& transform_node) {
  int id = transform_node.CcNodeId(new_sequence_number_);
  if (id != cc::kInvalidPropertyNodeId) {
    DCHECK(transform_tree_.Node(id));
    return id;
  }

  DCHECK(transform_node.Parent());
  int parent_id =
      EnsureCompositorTransformNode(transform_node.Parent()->Unalias());
  id = transform_tree_.Insert(cc::TransformNode(), parent_id);

  if (auto* scroll_translation_for_fixed =
          transform_node.ScrollTranslationForFixed()) {
    // Fixed-position can cause different topologies of the transform tree and
    // the scroll tree. This ensures the ancestor scroll nodes of the scroll
    // node for a descendant transform node below is created.
    EnsureCompositorTransformNode(*scroll_translation_for_fixed);
  }

  cc::TransformNode& compositor_node = *transform_tree_.Node(id);
  UpdateCcTransformLocalMatrix(compositor_node, transform_node);

  compositor_node.should_undo_overscroll =
      transform_node.RequiresCompositingForFixedToViewport();
  compositor_node.transform_changed = transform_node.NodeChangeAffectsRaster();
  compositor_node.flattens_inherited_transform =
      transform_node.FlattensInheritedTransform();
  compositor_node.sorting_context_id = transform_node.RenderingContextId();
  compositor_node.delegates_to_parent_for_backface =
      transform_node.DelegatesToParentForBackface();

  if (transform_node.IsAffectedByOuterViewportBoundsDelta()) {
    compositor_node.moved_by_outer_viewport_bounds_delta_y = true;
    transform_tree_.AddNodeAffectedByOuterViewportBoundsDelta(id);
  }

  compositor_node.in_subtree_of_page_scale_layer =
      transform_node.IsInSubtreeOfPageScale();

  compositor_node.will_change_transform =
      transform_node.RequiresCompositingForWillChangeTransform() &&
      // cc assumes preference of performance over raster quality for
      // will-change:transform, but for SVG we still prefer raster quality, so
      // don't pass will-change:transform to cc for SVG.
      // TODO(crbug.com/1186020): find a better way to handle this.
      !transform_node.IsForSVGChild();

  if (const auto* sticky_constraint = transform_node.GetStickyConstraint()) {
    cc::StickyPositionNodeData& sticky_data =
        transform_tree_.EnsureStickyPositionData(id);
    sticky_data.constraints = *sticky_constraint;
    const auto& scroll_ancestor = transform_node.NearestScrollTranslationNode();
    sticky_data.scroll_ancestor = EnsureCompositorScrollAndTransformNode(
        scroll_ancestor, InfiniteIntRect());
    const auto& scroll_ancestor_compositor_node =
        *scroll_tree_.Node(sticky_data.scroll_ancestor);
    if (scroll_ancestor_compositor_node.scrolls_outer_viewport)
      transform_tree_.AddNodeAffectedByOuterViewportBoundsDelta(id);
    if (auto shifting_sticky_box_element_id =
            sticky_data.constraints.nearest_element_shifting_sticky_box) {
      sticky_data.nearest_node_shifting_sticky_box =
          transform_tree_.FindNodeFromElementId(shifting_sticky_box_element_id)
              ->id;
    }
    if (auto shifting_containing_block_element_id =
            sticky_data.constraints.nearest_element_shifting_containing_block) {
      // TODO(crbug.com/1224888): Get rid of the nullptr check below:
      if (cc::TransformNode* node = transform_tree_.FindNodeFromElementId(
              shifting_containing_block_element_id)) {
        sticky_data.nearest_node_shifting_containing_block = node->id;
      }
    }
  }

  if (const auto* data = transform_node.GetAnchorPositionScrollData()) {
    transform_tree_.EnsureAnchorPositionScrollData(id) = *data;
  }

  auto compositor_element_id = transform_node.GetCompositorElementId();
  if (compositor_element_id) {
    transform_tree_.SetElementIdForNodeId(id, compositor_element_id);
    compositor_node.element_id = compositor_element_id;
  }

  transform_node.SetCcNodeId(new_sequence_number_, id);

  // If this transform is a scroll offset translation, create the associated
  // compositor scroll property node and adjust the compositor transform node's
  // scroll offset.
  if (transform_node.ScrollNode()) {
    compositor_node.scrolls = true;
    compositor_node.should_be_snapped = true;
    int scroll_id = EnsureCompositorScrollNode(transform_node);
    cc::ScrollNode* scroll_node = scroll_tree_.Node(scroll_id);
    scroll_node->transform_id = id;
    scroll_node->is_composited =
        client_.NeedsCompositedScrolling(transform_node);
    if (!scroll_node->is_composited) {
      scroll_node->main_thread_scrolling_reasons |=
          NonCompositedMainThreadScrollingReasons(transform_node);
    }
  }

  compositor_node.visible_frame_element_id =
      transform_node.GetVisibleFrameElementId();

  // Attach the index of the nearest parent node associated with a frame.
  int parent_frame_id = cc::kInvalidPropertyNodeId;
  if (const auto* parent = transform_node.UnaliasedParent()) {
    if (parent->IsFramePaintOffsetTranslation()) {
      parent_frame_id = parent_id;
    } else {
      const auto* parent_compositor_node = transform_tree_.Node(parent_id);
      DCHECK(parent_compositor_node);
      parent_frame_id = parent_compositor_node->parent_frame_id;
    }
  }
  compositor_node.parent_frame_id = parent_frame_id;

  transform_tree_.set_needs_update(true);

  return id;
}

int PropertyTreeManager::EnsureCompositorPageScaleTransformNode(
    const TransformPaintPropertyNode& node) {
  DCHECK(!node.IsInSubtreeOfPageScale());
  int id = EnsureCompositorTransformNode(node);
  DCHECK(transform_tree_.Node(id));
  cc::TransformNode& compositor_node = *transform_tree_.Node(id);
  SetTransformTreePageScaleFactor(transform_tree_, compositor_node);
  transform_tree_.set_needs_update(true);
  return id;
}

int PropertyTreeManager::EnsureCompositorClipNode(
    const ClipPaintPropertyNode& clip_node) {
  int id = clip_node.CcNodeId(new_sequence_number_);
  if (id != cc::kInvalidPropertyNodeId) {
    DCHECK(clip_tree_.Node(id));
    return id;
  }

  DCHECK(clip_node.UnaliasedParent());
  int parent_id = EnsureCompositorClipNode(*clip_node.UnaliasedParent());
  id = clip_tree_.Insert(cc::ClipNode(), parent_id);

  cc::ClipNode& compositor_node = *clip_tree_.Node(id);

  compositor_node.clip = clip_node.PaintClipRect().Rect();
  compositor_node.transform_id =
      EnsureCompositorTransformNode(clip_node.LocalTransformSpace().Unalias());
  if (clip_node.PixelMovingFilter()) {
    // We have to wait until the cc effect node for the filter is ready before
    // setting compositor_node.pixel_moving_filter_id.
    pixel_moving_filter_clip_expanders_.push_back(&clip_node);
  }

  clip_node.SetCcNodeId(new_sequence_number_, id);
  clip_tree_.set_needs_update(true);
  return id;
}

int PropertyTreeManager::EnsureCompositorScrollNode(
    const TransformPaintPropertyNode& scroll_translation) {
  const auto* scroll_node = scroll_translation.ScrollNode();
  CHECK(scroll_node);
  int scroll_id = EnsureCompositorScrollNodeInternal(*scroll_node);
  scroll_tree_.SetScrollOffset(
      scroll_node->GetCompositorElementId(),
      gfx::PointAtOffsetFromOrigin(-scroll_translation.Get2dTranslation()));
  return scroll_id;
}

int PropertyTreeManager::EnsureCompositorScrollNodeInternal(
    const ScrollPaintPropertyNode& scroll_node) {
  int id = scroll_node.CcNodeId(new_sequence_number_);
  if (id != cc::kInvalidPropertyNodeId) {
    return id;
  }

  CHECK(scroll_node.Parent());
  int parent_id = EnsureCompositorScrollNodeInternal(*scroll_node.Parent());
  id = scroll_tree_.Insert(cc::ScrollNode(), parent_id);

  cc::ScrollNode& compositor_node = *scroll_tree_.Node(id);
  compositor_node.container_origin = scroll_node.ContainerRect().origin();
  compositor_node.container_bounds = scroll_node.ContainerRect().size();
  compositor_node.bounds = scroll_node.ContentsRect().size();
  compositor_node.user_scrollable_horizontal =
      scroll_node.UserScrollableHorizontal();
  compositor_node.user_scrollable_vertical =
      scroll_node.UserScrollableVertical();
  compositor_node.prevent_viewport_scrolling_from_inner =
      scroll_node.PreventViewportScrollingFromInner();

  compositor_node.max_scroll_offset_affected_by_page_scale =
      scroll_node.MaxScrollOffsetAffectedByPageScale();
  compositor_node.overscroll_behavior =
      cc::OverscrollBehavior(static_cast<cc::OverscrollBehavior::Type>(
                                 scroll_node.OverscrollBehaviorX()),
                             static_cast<cc::OverscrollBehavior::Type>(
                                 scroll_node.OverscrollBehaviorY()));
  compositor_node.snap_container_data = scroll_node.GetSnapContainerData();

  auto compositor_element_id = scroll_node.GetCompositorElementId();
  if (compositor_element_id) {
    compositor_node.element_id = compositor_element_id;
    scroll_tree_.SetElementIdForNodeId(id, compositor_element_id);
  }

  // These three fields are either permanent for unpainted scrolls, or will be
  // overridden when we handle the painted scroll.
  compositor_node.transform_id = cc::kInvalidPropertyNodeId;
  compositor_node.is_composited = false;
  compositor_node.main_thread_scrolling_reasons =
      scroll_node.GetMainThreadScrollingReasons();
  if (RuntimeEnabledFeatures::ExcludePopupMainThreadScrollingReasonEnabled()) {
    CHECK_EQ(compositor_node.main_thread_scrolling_reasons,
             scroll_tree_.GetMainThreadRepaintReasons(compositor_node));
  }

  scroll_node.SetCcNodeId(new_sequence_number_, id);
  return id;
}

int PropertyTreeManager::EnsureCompositorScrollAndTransformNode(
    const TransformPaintPropertyNode& scroll_translation,
    const gfx::Rect& scrolling_contents_cull_rect) {
  const auto* scroll_node = scroll_translation.ScrollNode();
  DCHECK(scroll_node);
  EnsureCompositorTransformNode(scroll_translation);
  if (!scrolling_contents_cull_rect.Contains(scroll_node->ContentsRect())) {
    scroll_tree_.SetScrollingContentsCullRect(
        scroll_node->GetCompositorElementId(), scrolling_contents_cull_rect);
  }
  int id = scroll_node->CcNodeId(new_sequence_number_);
  DCHECK(scroll_tree_.Node(id));
  return id;
}

int PropertyTreeManager::EnsureCompositorInnerScrollAndTransformNode(
    const TransformPaintPropertyNode& scroll_translation) {
  int node_id = EnsureCompositorScrollAndTransformNode(scroll_translation,
                                                       InfiniteIntRect());
  scroll_tree_.Node(node_id)->scrolls_inner_viewport = true;
  return node_id;
}

int PropertyTreeManager::EnsureCompositorOuterScrollAndTransformNode(
    const TransformPaintPropertyNode& scroll_translation) {
  int node_id = EnsureCompositorScrollAndTransformNode(scroll_translation,
                                                       InfiniteIntRect());
  scroll_tree_.Node(node_id)->scrolls_outer_viewport = true;
  return node_id;
}

void PropertyTreeManager::EmitClipMaskLayer() {
  cc::EffectNode* mask_isolation = effect_tree_.Node(current_.effect_id);
  DCHECK(mask_isolation);
  bool needs_layer =
      !pending_synthetic_mask_layers_.Contains(mask_isolation->id) &&
      mask_isolation->mask_filter_info.IsEmpty();

  CompositorElementId mask_isolation_id, mask_effect_id;
  SynthesizedClip& clip = client_.CreateOrReuseSynthesizedClipLayer(
      *current_.clip, *current_.transform, needs_layer, mask_isolation_id,
      mask_effect_id);

  // Now we know the actual mask_isolation.element_id.
  // This overrides the element_id set in PopulateCcEffectNode() if the
  // backdrop effect was moved up to |mask_isolation|.
  mask_isolation->element_id = mask_isolation_id;

  if (!needs_layer)
    return;

  cc::EffectNode& mask_effect = *effect_tree_.Node(
      effect_tree_.Insert(cc::EffectNode(), current_.effect_id));
  // The address of mask_isolation may have changed when we insert
  // |mask_effect| into the tree.
  mask_isolation = effect_tree_.Node(current_.effect_id);

  mask_effect.element_id = mask_effect_id;
  mask_effect.clip_id = mask_isolation->clip_id;
  mask_effect.blend_mode = SkBlendMode::kDstIn;

  cc::PictureLayer* mask_layer = clip.Layer();

  layer_list_builder_.Add(mask_layer);
  mask_layer->set_property_tree_sequence_number(
      root_layer_.property_tree_sequence_number());
  mask_layer->SetTransformTreeIndex(
      EnsureCompositorTransformNode(*current_.transform));
  int scroll_id = EnsureCompositorScrollAndTransformNode(
      current_.transform->NearestScrollTranslationNode(), InfiniteIntRect());
  mask_layer->SetScrollTreeIndex(scroll_id);
  mask_layer->SetClipTreeIndex(mask_effect.clip_id);
  mask_layer->SetEffectTreeIndex(mask_effect.id);

  if (!mask_isolation->backdrop_filters.IsEmpty()) {
    mask_layer->SetIsBackdropFilterMask(true);
    auto element_id = CompositorElementIdWithNamespace(
        mask_effect.element_id, CompositorElementIdNamespace::kEffectMask);
    mask_layer->SetElementId(element_id);
    mask_isolation->backdrop_mask_element_id = element_id;
  }
}

void PropertyTreeManager::CloseCcEffect() {
  DCHECK(effect_stack_.size());
  const auto& previous_state = effect_stack_.back();

  // A backdrop effect (exotic blending or backdrop filter) that is masked by a
  // synthesized clip must have its effect to the outermost synthesized clip.
  // These operations need access to the backdrop of the enclosing effect. With
  // the isolation for a synthesized clip, a blank backdrop will be seen.
  // Therefore the backdrop effect is delegated to the outermost synthesized
  // clip, thus the clip can't be shared with sibling layers, and must be
  // closed now.
  bool clear_synthetic_effects =
      !IsCurrentCcEffectSynthetic() && current_.effect->MayHaveBackdropEffect();

  // We are about to close an effect that was synthesized for isolating
  // a clip mask. Now emit the actual clip mask that will be composited on
  // top of masked contents with SkBlendMode::kDstIn.
  if (IsCurrentCcEffectSyntheticForNonTrivialClip())
    EmitClipMaskLayer();

  if (IsCurrentCcEffectSynthetic())
    pending_synthetic_mask_layers_.erase(current_.effect_id);

  current_ = previous_state;
  effect_stack_.pop_back();

  if (clear_synthetic_effects) {
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();
  }
}

int PropertyTreeManager::SwitchToEffectNodeWithSynthesizedClip(
    const EffectPaintPropertyNode& next_effect,
    const ClipPaintPropertyNode& next_clip,
    bool layer_draws_content) {
  // This function is expected to be invoked right before emitting each layer.
  // It keeps track of the nesting of clip and effects, output a composited
  // effect node whenever an effect is entered, or a non-trivial clip is
  // entered. In the latter case, the generated composited effect node is
  // called a "synthetic effect", and the corresponding clip a "synthesized
  // clip". Upon exiting a synthesized clip, a mask layer will be appended,
  // which will be kDstIn blended on top of contents enclosed by the synthetic
  // effect, i.e. applying the clip as a mask.
  //
  // For example with the following clip and effect tree and pending layers:
  // E0 <-- E1
  // C0 <-- C1(rounded)
  // [P0(E1,C0), P1(E1,C1), P2(E0,C1)]
  // In effect stack diagram:
  // P0(C0) P1(C1)
  // [    E1     ] P2(C1)
  // [        E0        ]
  //
  // The following cc property trees and layers will be generated:
  // E0 <+- E1 <-- E_C1_1 <-- E_C1_1M
  //     +- E_C1_2 <-- E_C1_2M
  // C0 <-- C1
  // [L0(E1,C0), L1(E_C1_1, C1), L1M(E_C1_1M, C1), L2(E_C1_2, C1),
  //  L2M(E_C1_2M, C1)]
  // In effect stack diagram:
  //                 L1M(C1)
  //        L1(C1) [ E_C1_1M ]          L2M(C1)
  // L0(C0) [     E_C1_1     ] L2(C1) [ E_C1_2M ]
  // [          E1           ][     E_C1_2      ]
  // [                    E0                    ]
  //
  // As the caller iterates the layer list, the sequence of events happen in
  // the following order:
  // Prior to emitting P0, this method is invoked with (E1, C0). A compositor
  // effect node for E1 is generated as we are entering it. The caller emits P0.
  // Prior to emitting P1, this method is invoked with (E1, C1). A synthetic
  // compositor effect for C1 is generated as we are entering it. The caller
  // emits P1.
  // Prior to emitting P2, this method is invoked with (E0, C1). Both previously
  // entered effects must be closed, because synthetic effect for C1 is enclosed
  // by E1, thus must be closed before E1 can be closed. A mask layer L1M is
  // generated along with an internal effect node for blending. After closing
  // both effects, C1 has to be entered again, thus generates another synthetic
  // compositor effect. The caller emits P2.
  // At last, the caller invokes Finalize() to close the unclosed synthetic
  // effect. Another mask layer L2M is generated, along with its internal
  // effect node for blending.
  const auto& ancestor =
      current_.effect->LowestCommonAncestor(next_effect).Unalias();
  while (current_.effect != &ancestor)
    CloseCcEffect();

  BuildEffectNodesRecursively(next_effect);
  SynthesizeCcEffectsForClipsIfNeeded(next_clip, /*next_effect*/ nullptr);

  if (layer_draws_content)
    pending_synthetic_mask_layers_.clear();

  return current_.effect_id;
}

static bool IsNodeOnAncestorChain(const ClipPaintPropertyNode& find,
                                  const ClipPaintPropertyNode& current,
                                  const ClipPaintPropertyNode& ancestor) {
  // Precondition: |ancestor| must be an (inclusive) ancestor of |current|
  // otherwise the behavior is undefined.
  // Returns true if node |find| is one of the node on the ancestor chain
  // [current, ancestor). Returns false otherwise.
  DCHECK(ancestor.IsAncestorOf(current));

  for (const auto* node = &current; node != &ancestor;
       node = node->UnaliasedParent()) {
    if (node == &find)
      return true;
  }
  return false;
}

bool PropertyTreeManager::EffectStateMayBe2dAxisMisalignedToRenderSurface(
    EffectState& state,
    wtf_size_t index) {
  if (state.may_be_2d_axis_misaligned_to_render_surface ==
      EffectState::kUnknown) {
    // The root effect has render surface, so it's always kAligned.
    DCHECK_NE(0u, index);
    if (EffectStateMayBe2dAxisMisalignedToRenderSurface(
            effect_stack_[index - 1], index - 1)) {
      state.may_be_2d_axis_misaligned_to_render_surface =
          EffectState::kMisaligned;
    } else {
      state.may_be_2d_axis_misaligned_to_render_surface =
          TransformsMayBe2dAxisMisaligned(*effect_stack_[index - 1].transform,
                                          *current_.transform)
              ? EffectState::kMisaligned
              : EffectState::kAligned;
    }
  }
  return state.may_be_2d_axis_misaligned_to_render_surface ==
         EffectState::kMisaligned;
}

bool PropertyTreeManager::CurrentEffectMayBe2dAxisMisalignedToRenderSurface() {
  // |current_| is virtually the top of effect_stack_.
  return EffectStateMayBe2dAxisMisalignedToRenderSurface(current_,
                                                         effect_stack_.size());
}

PropertyTreeManager::CcEffectType PropertyTreeManager::SyntheticEffectType(
    const ClipPaintPropertyNode& clip) {
  unsigned effect_type = CcEffectType::kEffect;
  if (clip.PaintClipRect().IsRounded() || clip.ClipPath())
    effect_type |= CcEffectType::kSyntheticForNonTrivialClip;

  // Cc requires that a rectangluar clip is 2d-axis-aligned with the render
  // surface to correctly apply the clip.
  if (CurrentEffectMayBe2dAxisMisalignedToRenderSurface() ||
      TransformsMayBe2dAxisMisaligned(clip.LocalTransformSpace().Unalias(),
                                      *current_.transform))
    effect_type |= CcEffectType::kSyntheticFor2dAxisAlignment;
  return static_cast<CcEffectType>(effect_type);
}

void PropertyTreeManager::ForceRenderSurfaceIfSyntheticRoundedCornerClip(
    PropertyTreeManager::EffectState& state) {
  if (state.effect_type & CcEffectType::kSyntheticForNonTrivialClip) {
    auto& effect_node = *effect_tree_.Node(state.effect_id);
    effect_node.render_surface_reason = cc::RenderSurfaceReason::kRoundedCorner;
  }
}

struct PendingClip {
  DISALLOW_NEW();

 public:
  Member<const ClipPaintPropertyNode> clip;
  PropertyTreeManager::CcEffectType type;

  void Trace(Visitor* visitor) const { visitor->Trace(clip); }
};

std::optional<gfx::RRectF> PropertyTreeManager::ShaderBasedRRect(
    const ClipPaintPropertyNode& clip,
    PropertyTreeManager::CcEffectType type,
    const TransformPaintPropertyNode& transform,
    const EffectPaintPropertyNode* next_effect) {
  if (type & CcEffectType::kSyntheticFor2dAxisAlignment) {
    return std::nullopt;
  }
  if (clip.ClipPath()) {
    return std::nullopt;
  }

  auto WidthAndHeightAreTheSame = [](const gfx::SizeF& size) {
    return size.width() == size.height();
  };

  const FloatRoundedRect::Radii& radii = clip.PaintClipRect().GetRadii();
  if (!WidthAndHeightAreTheSame(radii.TopLeft()) ||
      !WidthAndHeightAreTheSame(radii.TopRight()) ||
      !WidthAndHeightAreTheSame(radii.BottomRight()) ||
      !WidthAndHeightAreTheSame(radii.BottomLeft())) {
    return std::nullopt;
  }

  // Rounded corners that differ are not supported by the CALayerOverlay system
  // on Mac. Instead of letting it fall back to the (worse for memory and
  // battery) non-CALayerOverlay system for such cases, fall back to a
  // non-shader border-radius mask for the effect node.
#if BUILDFLAG(IS_MAC)
  if (radii.TopLeft() != radii.TopRight() ||
      radii.TopLeft() != radii.BottomRight() ||
      radii.TopLeft() != radii.BottomLeft()) {
    return std::nullopt;
  }
#endif

  gfx::Vector2dF translation;
  if (&transform != &clip.LocalTransformSpace()) {
    gfx::Transform projection = GeometryMapper::SourceToDestinationProjection(
        clip.LocalTransformSpace(), transform);
    if (!projection.IsIdentityOr2dTranslation()) {
      return std::nullopt;
    }
    translation = projection.To2dTranslation();
  }

  SkRRect rrect(clip.PaintClipRect());
  rrect.offset(translation.x(), translation.y());
  if (!rrect.isValid()) {
    return std::nullopt;
  }
  return gfx::RRectF(rrect);
}

int PropertyTreeManager::SynthesizeCcEffectsForClipsIfNeeded(
    const ClipPaintPropertyNode& target_clip,
    const EffectPaintPropertyNode* next_effect) {
  int backdrop_effect_clip_id = cc::kInvalidPropertyNodeId;
  bool should_realize_backdrop_effect = false;
  if (next_effect && next_effect->MayHaveBackdropEffect()) {
    // Exit all synthetic effect node if the next child has backdrop effect
    // (exotic blending mode or backdrop filter) because it has to access the
    // backdrop of enclosing effect.
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();

    // An effect node can't omit render surface if it has child with backdrop
    // effect, in order to define the scope of the backdrop.
    effect_tree_.Node(current_.effect_id)->render_surface_reason =
        cc::RenderSurfaceReason::kBackdropScope;
    should_realize_backdrop_effect = true;
    backdrop_effect_clip_id = EnsureCompositorClipNode(target_clip);
  } else {
    // Exit synthetic effects until there are no more synthesized clips below
    // our lowest common ancestor.
    const auto& lca =
        current_.clip->LowestCommonAncestor(target_clip).Unalias();
    while (current_.clip != &lca) {
      if (!IsCurrentCcEffectSynthetic()) {
        // TODO(crbug.com/803649): We still have clip hierarchy issues with
        // fragment clips. See crbug.com/1238656 for the test case. Will change
        // the above condition to DCHECK after LayoutNGBlockFragmentation is
        // fully launched.
        return cc::kInvalidPropertyNodeId;
      }
      const auto* pre_exit_clip = current_.clip.Get();
      CloseCcEffect();
      // We may run past the lowest common ancestor because it may not have
      // been synthesized.
      if (IsNodeOnAncestorChain(lca, *pre_exit_clip, *current_.clip))
        break;
    }
  }

  HeapVector<PendingClip, 8> pending_clips;
  const ClipPaintPropertyNode* clip_node = &target_clip;
  for (; clip_node && clip_node != current_.clip;
       clip_node = clip_node->UnaliasedParent()) {
    if (auto type = SyntheticEffectType(*clip_node))
      pending_clips.emplace_back(PendingClip{clip_node, type});
  }

  if (!clip_node) {
    // TODO(crbug.com/803649): We still have clip hierarchy issues with
    // fragment clips. See crbug.com/1238656 for the test case. Will change
    // the above condition to DCHECK after LayoutNGBlockFragmentation is fully
    // launched.
    return cc::kInvalidPropertyNodeId;
  }

  if (pending_clips.empty())
    return cc::kInvalidPropertyNodeId;

  int cc_effect_id_for_backdrop_effect = cc::kInvalidPropertyNodeId;
  for (auto i = pending_clips.size(); i--;) {
    auto& pending_clip = pending_clips[i];
    int clip_id = backdrop_effect_clip_id;

    // For a non-trivial clip, the synthetic effect is an isolation to enclose
    // only the layers that should be masked by the synthesized clip.
    // For a non-2d-axis-preserving clip, the synthetic effect creates a render
    // surface which is axis-aligned with the clip.
    cc::EffectNode& synthetic_effect = *effect_tree_.Node(
        effect_tree_.Insert(cc::EffectNode(), current_.effect_id));

    const auto& transform =
        should_realize_backdrop_effect
            ? next_effect->LocalTransformSpace().Unalias()
            : pending_clip.clip->LocalTransformSpace().Unalias();

    if (pending_clip.type & CcEffectType::kSyntheticFor2dAxisAlignment) {
      if (should_realize_backdrop_effect) {
        // We need a synthetic mask clip layer for the non-2d-axis-aligned clip
        // when we also need to realize a backdrop effect.
        pending_clip.type = static_cast<CcEffectType>(
            pending_clip.type | CcEffectType::kSyntheticForNonTrivialClip);
      } else {
        synthetic_effect.element_id =
            CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
        synthetic_effect.render_surface_reason =
            cc::RenderSurfaceReason::kClipAxisAlignment;
        // The clip of the synthetic effect is the parent of the clip, so that
        // the clip itself will be applied in the render surface.
        DCHECK(pending_clip.clip->UnaliasedParent());
        clip_id =
            EnsureCompositorClipNode(*pending_clip.clip->UnaliasedParent());
      }
    }

    if (pending_clip.type & CcEffectType::kSyntheticForNonTrivialClip) {
      if (clip_id == cc::kInvalidPropertyNodeId) {
        const auto* clip = pending_clip.clip.Get();
        // Some virtual/threaded/external/wpt/css/css-view-transitions/*
        // tests will fail without the following condition.
        // TODO(crbug.com/1345805): Investigate the reason and remove the
        // condition if possible.
        if (!current_.effect->ViewTransitionElementResourceId().IsValid()) {
          // Use the parent clip as the output clip of the synthetic effect so
          // that the clip will apply to the masked contents but not the mask
          // layer, to ensure the masked content is fully covered by the mask
          // layer (after AdjustMaskLayerGeometry) in case of rounding errors
          // of the clip in the compositor.
          DCHECK(clip->UnaliasedParent());
          clip = clip->UnaliasedParent();
        }
        clip_id = EnsureCompositorClipNode(*clip);
      }
      // For non-trivial clip, isolation_effect.element_id will be assigned
      // later when the effect is closed. For now the default value ElementId()
      // is used. See PropertyTreeManager::EmitClipMaskLayer().
      if (std::optional<gfx::RRectF> rrect = ShaderBasedRRect(
              *pending_clip.clip, pending_clip.type, transform, next_effect)) {
        synthetic_effect.mask_filter_info = gfx::MaskFilterInfo(*rrect);
        synthetic_effect.is_fast_rounded_corner = true;

        // Nested rounded corner clips need to force render surfaces for
        // clips other than the leaf ones, because the compositor doesn't
        // know how to apply two rounded clips to the same draw quad.
        if (current_.contained_by_non_render_surface_synthetic_rounded_clip) {
          ForceRenderSurfaceIfSyntheticRoundedCornerClip(current_);
          for (auto effect_it = effect_stack_.rbegin();
               effect_it != effect_stack_.rend(); ++effect_it) {
            auto& effect_node = *effect_tree_.Node(effect_it->effect_id);
            if (effect_node.HasRenderSurface() &&
                !IsConditionalRenderSurfaceReason(
                    effect_node.render_surface_reason)) {
              break;
            }
            ForceRenderSurfaceIfSyntheticRoundedCornerClip(*effect_it);
          }
        }
      } else {
        synthetic_effect.render_surface_reason =
            pending_clip.clip->PaintClipRect().IsRounded()
                ? cc::RenderSurfaceReason::kRoundedCorner
                : cc::RenderSurfaceReason::kClipPath;
      }
      pending_synthetic_mask_layers_.insert(synthetic_effect.id);
    }

    if (should_realize_backdrop_effect) {
      // Move the effect node containing backdrop effects up to the outermost
      // synthetic effect to ensure the backdrop effects can access the correct
      // backdrop.
      DCHECK(next_effect);
      DCHECK_EQ(cc_effect_id_for_backdrop_effect, cc::kInvalidPropertyNodeId);
      PopulateCcEffectNode(synthetic_effect, *next_effect, clip_id);
      cc_effect_id_for_backdrop_effect = synthetic_effect.id;
      should_realize_backdrop_effect = false;
    } else {
      synthetic_effect.clip_id = clip_id;
    }

    synthetic_effect.transform_id = EnsureCompositorTransformNode(transform);
    synthetic_effect.double_sided = !transform.IsBackfaceHidden();

    effect_stack_.emplace_back(current_);
    SetCurrentEffectState(synthetic_effect, pending_clip.type, *current_.effect,
                          *pending_clip.clip, transform);
  }

  return cc_effect_id_for_backdrop_effect;
}

void PropertyTreeManager::BuildEffectNodesRecursively(
    const EffectPaintPropertyNode& next_effect) {
  if (&next_effect == current_.effect)
    return;

  DCHECK(next_effect.UnaliasedParent());
  BuildEffectNodesRecursively(*next_effect.UnaliasedParent());
  DCHECK_EQ(next_effect.UnaliasedParent(), current_.effect);

  bool has_multiple_groups = false;
  if (effect_tree_.Node(next_effect.CcNodeId(new_sequence_number_))) {
    // TODO(crbug.com/1064341): We have to allow one blink effect node to apply
    // to multiple groups in block fragments (multicol, etc.) due to the
    // current FragmentClip implementation. This can only be fixed by LayoutNG
    // block fragments. For now we'll create multiple cc effect nodes in the
    // case.
    // TODO(crbug.com/1253797): Actually this still happens with LayoutNG block
    // fragments due to paint order issue.
    has_multiple_groups = true;
  }

  int real_effect_node_id = cc::kInvalidPropertyNodeId;
  int output_clip_id = 0;
  const ClipPaintPropertyNode* output_clip = nullptr;
  if (next_effect.OutputClip()) {
    output_clip = &next_effect.OutputClip()->Unalias();
    real_effect_node_id =
        SynthesizeCcEffectsForClipsIfNeeded(*output_clip, &next_effect);
    output_clip_id = EnsureCompositorClipNode(*output_clip);
  } else {
    // If we don't have an output clip, then we'll use the clip of the last
    // non-synthetic effect. This means we should close all synthetic effects
    // on the stack first.
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();

    output_clip = current_.clip;
    DCHECK(output_clip);
    output_clip_id = effect_tree_.Node(current_.effect_id)->clip_id;
    DCHECK_EQ(output_clip_id, EnsureCompositorClipNode(*output_clip));
  }

  const auto& transform = next_effect.LocalTransformSpace().Unalias();
  auto& effect_node = *effect_tree_.Node(
      effect_tree_.Insert(cc::EffectNode(), current_.effect_id));
  if (real_effect_node_id == cc::kInvalidPropertyNodeId) {
    real_effect_node_id = effect_node.id;

    // |has_multiple_groups| implies that this paint effect node is split into
    // multiple CC effect nodes. This happens when we have non-contiguous paint
    // chunks which share the same paint effect node and as a result the same
    // shared element resource ID.
    // Since a shared element resource ID must be associated with a single CC
    // effect node, the code ensures that only one CC effect node (associated
    // with the first contiguous set of chunks) is tagged with the shared
    // element resource ID. The view transition should either prevent such
    // content or ensure effect nodes are contiguous. See crbug.com/1303081 for
    // details. This restriction also applies to element capture.
    DCHECK((!next_effect.ViewTransitionElementResourceId().IsValid() &&
            next_effect.ElementCaptureId()->is_zero()) ||
           !has_multiple_groups)
        << next_effect.ToString();
    PopulateCcEffectNode(effect_node, next_effect, output_clip_id);
  } else {
    // We have used the outermost synthetic effect for |next_effect| in
    // SynthesizeCcEffectsForClipsIfNeeded(), so |effect_node| is just a dummy
    // node to mark the end of continuous synthetic effects for |next_effect|.
    effect_node.clip_id = output_clip_id;
    effect_node.transform_id = EnsureCompositorTransformNode(transform);
    effect_node.element_id = next_effect.GetCompositorElementId();
  }

  if (has_multiple_groups) {
    if (effect_node.element_id) {
      // We are creating more than one cc effect nodes for one blink effect.
      // Give the extra cc effect node a unique stable id.
      effect_node.element_id =
          CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
    }
  } else {
    next_effect.SetCcNodeId(new_sequence_number_, real_effect_node_id);
  }

  CompositorElementId compositor_element_id =
      next_effect.GetCompositorElementId();
  if (compositor_element_id && !has_multiple_groups) {
    DCHECK(!effect_tree_.FindNodeFromElementId(compositor_element_id));
    effect_tree_.SetElementIdForNodeId(real_effect_node_id,
                                       compositor_element_id);
  }

  effect_stack_.emplace_back(current_);
  SetCurrentEffectState(effect_node, CcEffectType::kEffect, next_effect,
                        *output_clip, transform);
}

// See IsConditionalRenderSurfaceReason() for the definition of conditional
// render surface.
static cc::RenderSurfaceReason ConditionalRenderSurfaceReasonForEffect(
    const EffectPaintPropertyNode& effect) {
  if (effect.BlendMode() == SkBlendMode::kDstIn)
    return cc::RenderSurfaceReason::kBlendModeDstIn;
  if (effect.Opacity() != 1.f)
    return cc::RenderSurfaceReason::kOpacity;
  // TODO(crbug.com/1285498): Optimize for will-change: opacity.
  if (effect.HasActiveOpacityAnimation())
    return cc::RenderSurfaceReason::kOpacityAnimation;
  return cc::RenderSurfaceReason::kNone;
}

static cc::RenderSurfaceReason RenderSurfaceReasonForEffect(
    const EffectPaintPropertyNode& effect) {
  if (!effect.Filter().IsEmpty() ||
      effect.RequiresCompositingForWillChangeFilter()) {
    return cc::RenderSurfaceReason::kFilter;
  }
  if (effect.HasActiveFilterAnimation())
    return cc::RenderSurfaceReason::kFilterAnimation;
  if (effect.BackdropFilter() ||
      effect.RequiresCompositingForWillChangeBackdropFilter()) {
    return cc::RenderSurfaceReason::kBackdropFilter;
  }
  if (effect.HasActiveBackdropFilterAnimation())
    return cc::RenderSurfaceReason::kBackdropFilterAnimation;
  if (effect.BlendMode() != SkBlendMode::kSrcOver &&
      // The render surface for kDstIn is conditional. See above functions.
      effect.BlendMode() != SkBlendMode::kDstIn) {
    return cc::RenderSurfaceReason::kBlendMode;
  }
  if (effect.ViewTransitionElementResourceId().IsValid()) {
    return cc::RenderSurfaceReason::kViewTransitionParticipant;
  }
  // If the effect's transform node flattens the transform while it
  // participates in the 3d sorting context of an ancestor, cc needs a
  // render surface for correct flattening.
  // TODO(crbug.com/504464): Move the logic into cc compositor thread.
  if (effect.FlattensAtLeafOf3DScene())
    return cc::RenderSurfaceReason::k3dTransformFlattening;

  if (!effect.ElementCaptureId()->is_zero()) {
    return cc::RenderSurfaceReason::kSubtreeIsBeingCaptured;
  }
  auto conditional_reason = ConditionalRenderSurfaceReasonForEffect(effect);
  DCHECK(conditional_reason == cc::RenderSurfaceReason::kNone ||
         IsConditionalRenderSurfaceReason(conditional_reason));
  return conditional_reason;
}

void PropertyTreeManager::PopulateCcEffectNode(
    cc::EffectNode& effect_node,
    const EffectPaintPropertyNode& effect,
    int output_clip_id) {
  effect_node.element_id = effect.GetCompositorElementId();
  effect_node.clip_id = output_clip_id;
  effect_node.render_surface_reason = RenderSurfaceReasonForEffect(effect);
  effect_node.opacity = effect.Opacity();
  const auto& transform = effect.LocalTransformSpace().Unalias();
  effect_node.transform_id = EnsureCompositorTransformNode(transform);
  if (effect.MayHaveBackdropEffect()) {
    // We never have backdrop effect and filter on the same effect node.
    DCHECK(effect.Filter().IsEmpty());
    if (auto* backdrop_filter = effect.BackdropFilter()) {
      effect_node.backdrop_filters = backdrop_filter->AsCcFilterOperations();
      effect_node.backdrop_filter_bounds = effect.BackdropFilterBounds();
      effect_node.backdrop_mask_element_id = effect.BackdropMaskElementId();
    }
    effect_node.blend_mode = effect.BlendMode();
  } else {
    effect_node.filters = effect.Filter().AsCcFilterOperations();
  }
  effect_node.double_sided = !transform.IsBackfaceHidden();
  effect_node.effect_changed = effect.NodeChangeAffectsRaster();

  effect_node.view_transition_element_resource_id =
      effect.ViewTransitionElementResourceId();

  effect_node.subtree_capture_id =
      viz::SubtreeCaptureId(*effect.ElementCaptureId());
}

void PropertyTreeManager::UpdateConditionalRenderSurfaceReasons(
    const cc::LayerList& layers) {
  // This vector is indexed by effect node id. The value is the number of
  // layers and sub-render-surfaces controlled by this effect.
  wtf_size_t tree_size = base::checked_cast<wtf_size_t>(effect_tree_.size());
  Vector<int> effect_layer_counts(tree_size);
  Vector<bool> has_child_surface(tree_size);
  // Initialize the vector to count directly controlled layers.
  for (const auto& layer : layers) {
    if (layer->draws_content())
      effect_layer_counts[layer->effect_tree_index()]++;
  }

  // In the effect tree, parent always has lower id than children, so the
  // following loop will check descendants before parents and accumulate
  // effect_layer_counts.
  for (int id = tree_size - 1; id > cc::kSecondaryRootPropertyNodeId; id--) {
    auto* effect = effect_tree_.Node(id);
    if (effect_layer_counts[id] < 2 &&
        IsConditionalRenderSurfaceReason(effect->render_surface_reason) &&
        // kBlendModeDstIn should create a render surface if the mask itself
        // has any child render surface.
        !(effect->render_surface_reason ==
              cc::RenderSurfaceReason::kBlendModeDstIn &&
          has_child_surface[id])) {
      // The conditional render surface can be omitted because it controls less
      // than two layers or render surfaces.
      effect->render_surface_reason = cc::RenderSurfaceReason::kNone;
    }

    // We should not have visited the parent.
    DCHECK_NE(-1, effect_layer_counts[effect->parent_id]);
    if (effect->HasRenderSurface()) {
      // A sub-render-surface counts as one controlled layer of the parent.
      effect_layer_counts[effect->parent_id]++;
      has_child_surface[effect->parent_id] = true;
    } else {
      // Otherwise all layers count as controlled layers of the parent.
      effect_layer_counts[effect->parent_id] += effect_layer_counts[id];
      has_child_surface[effect->parent_id] |= has_child_surface[id];
    }

#if DCHECK_IS_ON()
    // Mark we have visited this effect.
    effect_layer_counts[id] = -1;
#endif
  }
}

// This is called after all property nodes have been converted and we know
// pixel_moving_filter_id for the pixel-moving clip expanders.
void PropertyTreeManager::UpdatePixelMovingFilterClipExpanders() {
  for (const auto& clip : pixel_moving_filter_clip_expanders_) {
    DCHECK(clip->PixelMovingFilter());
    cc::ClipNode* cc_clip =
        clip_tree_.Node(clip->CcNodeId(new_sequence_number_));
    DCHECK(cc_clip);
    cc_clip->pixel_moving_filter_id =
        clip->PixelMovingFilter()->CcNodeId(new_sequence_number_);
    // No DCHECK(!cc_clip->AppliesLocalClip()) because the PixelMovingFilter
    // may not be composited, and the clip node is a no-op node.
  }
  pixel_moving_filter_clip_expanders_.clear();
}

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::PendingClip)
