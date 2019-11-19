// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"

#include "build/build_config.h"
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
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"

namespace blink {

namespace {

static constexpr int kInvalidNodeId = -1;
// cc's property trees use 0 for the root node (always non-null).
static constexpr int kRealRootNodeId = 0;
// cc allocates special nodes for root effects such as the device scale.
static constexpr int kSecondaryRootNodeId = 1;

}  // namespace

inline const TransformPaintPropertyNode&
PropertyTreeManager::EffectState::Transform() const {
  return effect_type == CcEffectType::kEffect ? effect->LocalTransformSpace()
                                              : clip->LocalTransformSpace();
}

PropertyTreeManager::PropertyTreeManager(
    PropertyTreeManagerClient& client,
    cc::PropertyTrees& property_trees,
    cc::Layer& root_layer,
    LayerListBuilder& layer_list_builder,
    int new_sequence_number)
    : client_(client),
      property_trees_(property_trees),
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
  DCHECK(effect_stack_.IsEmpty());
}

static void UpdateCcTransformLocalMatrix(
    cc::TransformNode& compositor_node,
    const TransformPaintPropertyNode& transform_node) {
  if (transform_node.GetStickyConstraint()) {
    // The sticky offset on the blink transform node is pre-computed and stored
    // to the local matrix. Cc applies sticky offset dynamically on top of the
    // local matrix. We should not set the local matrix on cc node if it is a
    // sticky node because the sticky offset would be applied twice otherwise.
    DCHECK(compositor_node.local.IsIdentity());
    DCHECK_EQ(gfx::Point3F(), compositor_node.origin);
  } else if (transform_node.IsIdentityOr2DTranslation()) {
    auto translation = transform_node.Translation2D();
    if (transform_node.ScrollNode()) {
      // Blink creates a 2d transform node just for scroll offset whereas cc's
      // transform node has a special scroll offset field.
      compositor_node.scroll_offset =
          gfx::ScrollOffset(-translation.Width(), -translation.Height());
      DCHECK(compositor_node.local.IsIdentity());
      DCHECK_EQ(gfx::Point3F(), compositor_node.origin);
    } else {
      compositor_node.local.matrix().setTranslate(translation.Width(),
                                                  translation.Height(), 0);
      DCHECK_EQ(FloatPoint3D(), transform_node.Origin());
      compositor_node.origin = gfx::Point3F();
    }
  } else {
    DCHECK(!transform_node.ScrollNode());
    FloatPoint3D origin = transform_node.Origin();
    compositor_node.local.matrix() =
        TransformationMatrix::ToSkMatrix44(transform_node.Matrix());
    compositor_node.origin = origin;
  }
  compositor_node.needs_local_transform_update = true;
}

static void SetTransformTreePageScaleFactor(
    cc::TransformTree* transform_tree,
    cc::TransformNode* page_scale_node) {
  DCHECK(page_scale_node->local.IsScale2d());
  auto page_scale = page_scale_node->local.Scale2d();
  DCHECK_EQ(page_scale.x(), page_scale.y());
  transform_tree->set_page_scale_factor(page_scale.x());
}

bool PropertyTreeManager::DirectlyUpdateCompositedOpacityValue(
    cc::PropertyTrees* property_trees,
    cc::LayerTreeHost& host,
    const EffectPaintPropertyNode& effect) {
  auto* cc_effect = property_trees->effect_tree.Node(
      effect.CcNodeId(property_trees->sequence_number));
  if (!cc_effect)
    return false;

  // We directly update opacity only when it's not animating in compositor. If
  // the compositor has not cleared is_currently_animating_opacity, we should
  // clear it now to let the compositor respect the new value.
  cc_effect->is_currently_animating_opacity = false;

  cc_effect->opacity = effect.Opacity();
  cc_effect->effect_changed = true;
  property_trees->effect_tree.set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

bool PropertyTreeManager::DirectlyUpdateScrollOffsetTransform(
    cc::PropertyTrees* property_trees,
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  auto* scroll_node = transform.ScrollNode();
  // Only handle scroll adjustments.
  if (!scroll_node)
    return false;

  auto* cc_scroll_node = property_trees->scroll_tree.Node(
      scroll_node->CcNodeId(property_trees->sequence_number));
  if (!cc_scroll_node)
    return false;

  auto* cc_transform = property_trees->transform_tree.Node(
      transform.CcNodeId(property_trees->sequence_number));
  if (!cc_transform)
    return false;

  DCHECK(!cc_transform->is_currently_animating);

  UpdateCcTransformLocalMatrix(*cc_transform, transform);
  property_trees->scroll_tree.SetScrollOffset(
      scroll_node->GetCompositorElementId(), cc_transform->scroll_offset);

  cc_transform->transform_changed = true;
  property_trees->transform_tree.set_needs_update(true);
  property_trees->scroll_tree.set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

bool PropertyTreeManager::DirectlyUpdateTransform(
    cc::PropertyTrees* property_trees,
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  // If we have a ScrollNode, we should be using
  // DirectlyUpdateScrollOffsetTransform().
  DCHECK(!transform.ScrollNode());

  auto* cc_transform = property_trees->transform_tree.Node(
      transform.CcNodeId(property_trees->sequence_number));
  if (!cc_transform)
    return false;

  UpdateCcTransformLocalMatrix(*cc_transform, transform);

  // We directly update transform only when the transform is not animating in
  // compositor. If the compositor has not cleared the is_currently_animating
  // flag, we should clear it to let the compositor respect the new value.
  cc_transform->is_currently_animating = false;

  cc_transform->transform_changed = true;
  property_trees->transform_tree.set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

bool PropertyTreeManager::DirectlyUpdatePageScaleTransform(
    cc::PropertyTrees* property_trees,
    cc::LayerTreeHost& host,
    const TransformPaintPropertyNode& transform) {
  DCHECK(!transform.ScrollNode());

  auto* cc_transform = property_trees->transform_tree.Node(
      transform.CcNodeId(property_trees->sequence_number));
  if (!cc_transform)
    return false;

  UpdateCcTransformLocalMatrix(*cc_transform, transform);
  SetTransformTreePageScaleFactor(&property_trees->transform_tree,
                                  cc_transform);
  cc_transform->transform_changed = true;
  property_trees->transform_tree.set_needs_update(true);
  host.SetNeedsCommit();
  return true;
}

cc::TransformTree& PropertyTreeManager::GetTransformTree() {
  return property_trees_.transform_tree;
}

cc::ClipTree& PropertyTreeManager::GetClipTree() {
  return property_trees_.clip_tree;
}

cc::EffectTree& PropertyTreeManager::GetEffectTree() {
  return property_trees_.effect_tree;
}

cc::ScrollTree& PropertyTreeManager::GetScrollTree() {
  return property_trees_.scroll_tree;
}

void PropertyTreeManager::SetupRootTransformNode() {
  // cc is hardcoded to use transform node index 1 for device scale and
  // transform.
  cc::TransformTree& transform_tree = property_trees_.transform_tree;
  transform_tree.clear();
  property_trees_.element_id_to_transform_node_index.clear();
  cc::TransformNode& transform_node = *transform_tree.Node(
      transform_tree.Insert(cc::TransformNode(), kRealRootNodeId));
  DCHECK_EQ(transform_node.id, kSecondaryRootNodeId);

  // TODO(jaydasika): We shouldn't set ToScreen and FromScreen of root
  // transform node here. They should be set while updating transform tree in
  // cc.
  float device_scale_factor =
      root_layer_.layer_tree_host()->device_scale_factor();
  transform_tree.set_device_scale_factor(device_scale_factor);
  gfx::Transform to_screen;
  to_screen.Scale(device_scale_factor, device_scale_factor);
  transform_tree.SetToScreen(kRealRootNodeId, to_screen);
  gfx::Transform from_screen;
  bool invertible = to_screen.GetInverse(&from_screen);
  DCHECK(invertible);
  transform_tree.SetFromScreen(kRealRootNodeId, from_screen);
  transform_tree.set_needs_update(true);

  TransformPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                                 transform_node.id);
  root_layer_.SetTransformTreeIndex(transform_node.id);
}

void PropertyTreeManager::SetupRootClipNode() {
  // cc is hardcoded to use clip node index 1 for viewport clip.
  cc::ClipTree& clip_tree = property_trees_.clip_tree;
  clip_tree.clear();
  cc::ClipNode& clip_node =
      *clip_tree.Node(clip_tree.Insert(cc::ClipNode(), kRealRootNodeId));
  DCHECK_EQ(clip_node.id, kSecondaryRootNodeId);

  clip_node.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  // TODO(bokan): This needs to come from the Visual Viewport which will
  // correctly account for the URL bar. In fact, the visual viewport property
  // tree builder should probably be the one to create the property tree state
  // and have this created in the same way as other layers.
  clip_node.clip =
      gfx::RectF(root_layer_.layer_tree_host()->device_viewport_rect());
  clip_node.transform_id = kRealRootNodeId;

  ClipPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_, clip_node.id);
  root_layer_.SetClipTreeIndex(clip_node.id);
}

void PropertyTreeManager::SetupRootEffectNode() {
  // cc is hardcoded to use effect node index 1 for root render surface.
  cc::EffectTree& effect_tree = property_trees_.effect_tree;
  effect_tree.clear();
  property_trees_.element_id_to_effect_node_index.clear();
  cc::EffectNode& effect_node =
      *effect_tree.Node(effect_tree.Insert(cc::EffectNode(), kInvalidNodeId));
  DCHECK_EQ(effect_node.id, kSecondaryRootNodeId);

  static UniqueObjectId unique_id = NewUniqueObjectId();

  effect_node.stable_id =
      CompositorElementIdFromUniqueObjectId(unique_id).GetStableId();
  effect_node.transform_id = kRealRootNodeId;
  effect_node.clip_id = kSecondaryRootNodeId;
  effect_node.render_surface_reason = cc::RenderSurfaceReason::kRoot;
  root_layer_.SetEffectTreeIndex(effect_node.id);

  EffectPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                              effect_node.id);
  SetCurrentEffectState(effect_node, CcEffectType::kEffect,
                        EffectPaintPropertyNode::Root(),
                        ClipPaintPropertyNode::Root());
}

void PropertyTreeManager::SetupRootScrollNode() {
  cc::ScrollTree& scroll_tree = property_trees_.scroll_tree;
  scroll_tree.clear();
  property_trees_.element_id_to_scroll_node_index.clear();
  cc::ScrollNode& scroll_node =
      *scroll_tree.Node(scroll_tree.Insert(cc::ScrollNode(), kRealRootNodeId));
  DCHECK_EQ(scroll_node.id, kSecondaryRootNodeId);
  scroll_node.transform_id = kSecondaryRootNodeId;

  ScrollPaintPropertyNode::Root().SetCcNodeId(new_sequence_number_,
                                              scroll_node.id);
  root_layer_.SetScrollTreeIndex(scroll_node.id);
}

static bool TransformsToAncestorHaveNonAxisAlignedActiveAnimation(
    const TransformPaintPropertyNode& descendant,
    const TransformPaintPropertyNode& ancestor) {
  if (&descendant == &ancestor)
    return false;
  for (const auto* n = &descendant; n != &ancestor; n = n->Parent()) {
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
  const auto& translation_2d_or_matrix =
      GeometryMapper::SourceToDestinationProjection(a, b);
  if (!translation_2d_or_matrix.IsIdentityOr2DTranslation() &&
      !translation_2d_or_matrix.Matrix().Preserves2dAxisAlignment())
    return true;
  const auto& lca = LowestCommonAncestor(a, b);
  if (TransformsToAncestorHaveNonAxisAlignedActiveAnimation(a, lca) ||
      TransformsToAncestorHaveNonAxisAlignedActiveAnimation(b, lca))
    return true;
  return false;
}

void PropertyTreeManager::SetCurrentEffectState(
    const cc::EffectNode& cc_effect_node,
    CcEffectType effect_type,
    const EffectPaintPropertyNode& effect,
    const ClipPaintPropertyNode& clip) {
  const auto* previous_transform =
      effect.IsRoot() ? nullptr : &current_.Transform();
  current_.effect_id = cc_effect_node.id;
  current_.effect_type = effect_type;
  DCHECK(!effect.IsParentAlias() || !effect.Parent());
  current_.effect = &effect;
  DCHECK(!clip.IsParentAlias() || !clip.Parent());
  current_.clip = &clip;

  if (cc_effect_node.HasRenderSurface()) {
    current_.may_be_2d_axis_misaligned_to_render_surface =
        EffectState::kAligned;
    current_.contained_by_non_render_surface_synthetic_rounded_clip = false;
  } else {
    if (current_.may_be_2d_axis_misaligned_to_render_surface ==
            EffectState::kAligned &&
        previous_transform != &current_.Transform()) {
      current_.may_be_2d_axis_misaligned_to_render_surface =
          EffectState::kUnknown;
    }
    current_.contained_by_non_render_surface_synthetic_rounded_clip |=
        (effect_type & CcEffectType::kSyntheticForNonTrivialClip);
  }
}

// TODO(crbug.com/504464): Remove this when move render surface decision logic
// into cc compositor thread.
void PropertyTreeManager::SetCurrentEffectRenderSurfaceReason(
    cc::RenderSurfaceReason reason) {
  auto* effect = GetEffectTree().Node(current_.effect_id);
  effect->render_surface_reason = reason;
}

int PropertyTreeManager::EnsureCompositorTransformNode(
    const TransformPaintPropertyNode& transform_node_arg) {
  const auto& transform_node = transform_node_arg.Unalias();
  int id = transform_node.CcNodeId(new_sequence_number_);
  if (id != kInvalidNodeId) {
    DCHECK(GetTransformTree().Node(id));
    return id;
  }

  DCHECK(transform_node.Parent());
  int parent_id = EnsureCompositorTransformNode(*transform_node.Parent());
  id = GetTransformTree().Insert(cc::TransformNode(), parent_id);

  cc::TransformNode& compositor_node = *GetTransformTree().Node(id);
  UpdateCcTransformLocalMatrix(compositor_node, transform_node);
  compositor_node.transform_changed = transform_node.NodeChangeAffectsRaster();
  compositor_node.flattens_inherited_transform =
      transform_node.FlattensInheritedTransform();
  compositor_node.sorting_context_id = transform_node.RenderingContextId();

  if (transform_node.IsAffectedByOuterViewportBoundsDelta()) {
    compositor_node.moved_by_outer_viewport_bounds_delta_y = true;
    GetTransformTree().AddNodeAffectedByOuterViewportBoundsDelta(id);
  }

  compositor_node.in_subtree_of_page_scale_layer =
      transform_node.IsInSubtreeOfPageScale();

  if (const auto* sticky_constraint = transform_node.GetStickyConstraint()) {
    cc::StickyPositionNodeData& sticky_data =
        GetTransformTree().EnsureStickyPositionData(id);
    sticky_data.constraints = *sticky_constraint;
    // TODO(pdr): This could be a performance issue because it crawls up the
    // transform tree for each pending layer. If this is on profiles, we should
    // cache a lookup of transform node to scroll translation transform node.
    const auto& scroll_ancestor = transform_node.NearestScrollTranslationNode();
    sticky_data.scroll_ancestor = EnsureCompositorScrollNode(scroll_ancestor);
    if (scroll_ancestor.ScrollNode()->ScrollsOuterViewport())
      GetTransformTree().AddNodeAffectedByOuterViewportBoundsDelta(id);
    if (auto shifting_sticky_box_element_id =
            sticky_data.constraints.nearest_element_shifting_sticky_box) {
      sticky_data.nearest_node_shifting_sticky_box =
          GetTransformTree()
              .FindNodeFromElementId(shifting_sticky_box_element_id)
              ->id;
    }
    if (auto shifting_containing_block_element_id =
            sticky_data.constraints.nearest_element_shifting_containing_block) {
      sticky_data.nearest_node_shifting_containing_block =
          GetTransformTree()
              .FindNodeFromElementId(shifting_containing_block_element_id)
              ->id;
    }
  }

  auto compositor_element_id = transform_node.GetCompositorElementId();
  if (compositor_element_id) {
    property_trees_.element_id_to_transform_node_index[compositor_element_id] =
        id;
    compositor_node.element_id = compositor_element_id;
  }

  // If this transform is a scroll offset translation, create the associated
  // compositor scroll property node and adjust the compositor transform node's
  // scroll offset.
  if (auto* scroll_node = transform_node.ScrollNode()) {
    compositor_node.scrolls = true;
    compositor_node.should_be_snapped = true;
    CreateCompositorScrollNode(*scroll_node, compositor_node);
  }

  // If the parent transform node flattens transform (as |transform_node|
  // flattens inherited transform) while it participates in the 3d sorting
  // context of an ancestor, cc needs a render surface for correct flattening.
  // TODO(crbug.com/504464): Move the logic into cc compositor thread.
  auto* current_cc_effect = GetEffectTree().Node(current_.effect_id);
  if (current_cc_effect && !current_cc_effect->HasRenderSurface() &&
      current_cc_effect->transform_id == parent_id &&
      transform_node.FlattensInheritedTransform() && transform_node.Parent() &&
      transform_node.Parent()->RenderingContextId() &&
      !transform_node.Parent()->FlattensInheritedTransform()) {
    SetCurrentEffectRenderSurfaceReason(
        cc::RenderSurfaceReason::k3dTransformFlattening);
  }

  transform_node.SetCcNodeId(new_sequence_number_, id);
  GetTransformTree().set_needs_update(true);

  return id;
}

int PropertyTreeManager::EnsureCompositorPageScaleTransformNode(
    const TransformPaintPropertyNode& node) {
  DCHECK(!node.IsInSubtreeOfPageScale());
  int id = EnsureCompositorTransformNode(node);
  DCHECK(GetTransformTree().Node(id));
  cc::TransformNode& compositor_node = *GetTransformTree().Node(id);
  SetTransformTreePageScaleFactor(&GetTransformTree(), &compositor_node);
  GetTransformTree().set_needs_update(true);
  return id;
}

int PropertyTreeManager::EnsureCompositorClipNode(
    const ClipPaintPropertyNode& clip_node_arg) {
  const auto& clip_node = clip_node_arg.Unalias();
  int id = clip_node.CcNodeId(new_sequence_number_);
  if (id != kInvalidNodeId) {
    DCHECK(GetClipTree().Node(id));
    return id;
  }

  DCHECK(clip_node.Parent());
  int parent_id = EnsureCompositorClipNode(*clip_node.Parent());
  id = GetClipTree().Insert(cc::ClipNode(), parent_id);

  cc::ClipNode& compositor_node = *GetClipTree().Node(id);

  compositor_node.clip = clip_node.ClipRect().Rect();
  compositor_node.transform_id =
      EnsureCompositorTransformNode(clip_node.LocalTransformSpace());
  compositor_node.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;

  clip_node.SetCcNodeId(new_sequence_number_, id);
  GetClipTree().set_needs_update(true);
  return id;
}

void PropertyTreeManager::CreateCompositorScrollNode(
    const ScrollPaintPropertyNode& scroll_node,
    const cc::TransformNode& scroll_offset_translation) {
  DCHECK(!GetScrollTree().Node(scroll_node.CcNodeId(new_sequence_number_)));

  int parent_id = scroll_node.Parent()->CcNodeId(new_sequence_number_);
  // Compositor transform nodes up to scroll_offset_translation must exist.
  // Scrolling uses the transform tree for scroll offsets so this means all
  // ancestor scroll nodes must also exist.
  DCHECK(GetScrollTree().Node(parent_id));
  int id = GetScrollTree().Insert(cc::ScrollNode(), parent_id);

  cc::ScrollNode& compositor_node = *GetScrollTree().Node(id);
  compositor_node.scrollable = true;

  compositor_node.container_bounds =
      static_cast<gfx::Size>(scroll_node.ContainerRect().Size());
  compositor_node.bounds = static_cast<gfx::Size>(scroll_node.ContentsSize());
  compositor_node.user_scrollable_horizontal =
      scroll_node.UserScrollableHorizontal();
  compositor_node.user_scrollable_vertical =
      scroll_node.UserScrollableVertical();
  compositor_node.scrolls_inner_viewport = scroll_node.ScrollsInnerViewport();
  compositor_node.scrolls_outer_viewport = scroll_node.ScrollsOuterViewport();
  compositor_node.prevent_viewport_scrolling_from_inner =
      scroll_node.PreventViewportScrollingFromInner();

  // |scrolls_using_viewport| should only ever be set on the inner scroll node.
  DCHECK(!compositor_node.prevent_viewport_scrolling_from_inner ||
         compositor_node.scrolls_inner_viewport);

  compositor_node.max_scroll_offset_affected_by_page_scale =
      scroll_node.MaxScrollOffsetAffectedByPageScale();
  compositor_node.main_thread_scrolling_reasons =
      scroll_node.GetMainThreadScrollingReasons();
  compositor_node.overscroll_behavior = cc::OverscrollBehavior(
      static_cast<cc::OverscrollBehavior::OverscrollBehaviorType>(
          scroll_node.OverscrollBehaviorX()),
      static_cast<cc::OverscrollBehavior::OverscrollBehaviorType>(
          scroll_node.OverscrollBehaviorY()));
  compositor_node.snap_container_data = scroll_node.GetSnapContainerData();

  auto compositor_element_id = scroll_node.GetCompositorElementId();
  if (compositor_element_id) {
    compositor_node.element_id = compositor_element_id;
    property_trees_.element_id_to_scroll_node_index[compositor_element_id] = id;
  }

  compositor_node.transform_id = scroll_offset_translation.id;

  // TODO(pdr): Set the scroll node's non_fast_scrolling_region value.

  scroll_node.SetCcNodeId(new_sequence_number_, id);

  GetScrollTree().SetScrollOffset(compositor_element_id,
                                  scroll_offset_translation.scroll_offset);
  GetScrollTree().set_needs_update(true);
}

int PropertyTreeManager::EnsureCompositorScrollNode(
    const TransformPaintPropertyNode& scroll_offset_translation) {
  const auto* scroll_node = scroll_offset_translation.ScrollNode();
  DCHECK(scroll_node);
  EnsureCompositorTransformNode(scroll_offset_translation);
  int id = scroll_node->CcNodeId(new_sequence_number_);
  DCHECK(GetScrollTree().Node(id));
  return id;
}

void PropertyTreeManager::EmitClipMaskLayer() {
  cc::EffectNode& mask_isolation = *GetEffectTree().Node(current_.effect_id);

  bool needs_layer =
      !pending_synthetic_mask_layers_.Contains(mask_isolation.id) &&
      mask_isolation.rounded_corner_bounds.IsEmpty();

  int clip_id = EnsureCompositorClipNode(*current_.clip);
  CompositorElementId mask_isolation_id, mask_effect_id;
  SynthesizedClip& clip = client_.CreateOrReuseSynthesizedClipLayer(
      *current_.clip, needs_layer, mask_isolation_id, mask_effect_id);

  // Assignment of mask_isolation.stable_id was delayed until now.
  // See PropertyTreeManager::SynthesizeCcEffectsForClipsIfNeeded().
  DCHECK_EQ(static_cast<uint64_t>(cc::EffectNode::INVALID_STABLE_ID),
            mask_isolation.stable_id);

  mask_isolation.stable_id = mask_isolation_id.GetStableId();

  if (!needs_layer)
    return;

  cc::EffectNode& mask_effect = *GetEffectTree().Node(
      GetEffectTree().Insert(cc::EffectNode(), current_.effect_id));
  mask_effect.stable_id = mask_effect_id.GetStableId();
  mask_effect.clip_id = clip_id;
  mask_effect.blend_mode = SkBlendMode::kDstIn;

  cc::Layer* mask_layer = clip.Layer();

  const auto& clip_space = current_.clip->LocalTransformSpace();
  layer_list_builder_.Add(mask_layer);
  mask_layer->set_property_tree_sequence_number(
      root_layer_.property_tree_sequence_number());
  mask_layer->SetTransformTreeIndex(EnsureCompositorTransformNode(clip_space));
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  int scroll_id =
      EnsureCompositorScrollNode(clip_space.NearestScrollTranslationNode());
  mask_layer->SetScrollTreeIndex(scroll_id);
  mask_layer->SetClipTreeIndex(clip_id);
  mask_layer->SetEffectTreeIndex(mask_effect.id);
}

void PropertyTreeManager::CloseCcEffect() {
  DCHECK(effect_stack_.size());
  const auto& previous_state = effect_stack_.back();

  // An effect with exotic blending or backdrop-filter that is masked by a
  // synthesized clip must have its blending to the outermost synthesized clip.
  // These operations need access to the backdrop of the enclosing effect. With
  // the isolation for a synthesized clip, a blank backdrop will be seen.
  // Therefore the blending is delegated to the outermost synthesized clip, thus
  // the clip can't be shared with sibling layers, and must be closed now.
  bool clear_synthetic_effects =
      !IsCurrentCcEffectSynthetic() &&
      current_.effect->BlendMode() != SkBlendMode::kSrcOver;

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
      LowestCommonAncestor(*current_.effect, next_effect).Unalias();
  while (current_.effect != &ancestor)
    CloseCcEffect();

  BuildEffectNodesRecursively(next_effect);
  SynthesizeCcEffectsForClipsIfNeeded(next_clip, SkBlendMode::kSrcOver);

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

  for (const auto* node = &current; node != &ancestor; node = node->Parent()) {
    if (node == &find)
      return true;
  }
  return false;
}

bool PropertyTreeManager::EffectStateMayBe2dAxisMisalignedToRenderSurface(
    EffectState& state,
    size_t index) {
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
          TransformsMayBe2dAxisMisaligned(effect_stack_[index - 1].Transform(),
                                          current_.Transform())
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
  if (clip.ClipRect().IsRounded() || clip.ClipPath())
    effect_type |= CcEffectType::kSyntheticForNonTrivialClip;

  // Cc requires that a rectangluar clip is 2d-axis-aligned with the render
  // surface to correctly apply the clip.
  if (CurrentEffectMayBe2dAxisMisalignedToRenderSurface() ||
      TransformsMayBe2dAxisMisaligned(clip.LocalTransformSpace(),
                                      current_.Transform()))
    effect_type |= CcEffectType::kSyntheticFor2dAxisAlignment;
  return static_cast<CcEffectType>(effect_type);
}

void PropertyTreeManager::ForceRenderSurfaceIfSyntheticRoundedCornerClip(
    PropertyTreeManager::EffectState& state) {
  if (state.effect_type & CcEffectType::kSyntheticForNonTrivialClip) {
    auto& effect_node = *GetEffectTree().Node(state.effect_id);
    effect_node.render_surface_reason = cc::RenderSurfaceReason::kRoundedCorner;
  }
}

bool PropertyTreeManager::SupportsShaderBasedRoundedCorner(
    const ClipPaintPropertyNode& clip,
    PropertyTreeManager::CcEffectType type) {
  if (!RuntimeEnabledFeatures::FastBorderRadiusEnabled())
    return false;

  if (type & CcEffectType::kSyntheticFor2dAxisAlignment)
    return false;

  if (clip.ClipPath())
    return false;

  auto WidthAndHeightAreTheSame = [](const FloatSize& size) {
    return size.Width() == size.Height();
  };

  const FloatRoundedRect::Radii& radii = clip.ClipRect().GetRadii();
  if (!WidthAndHeightAreTheSame(radii.TopLeft()) ||
      !WidthAndHeightAreTheSame(radii.TopRight()) ||
      !WidthAndHeightAreTheSame(radii.BottomRight()) ||
      !WidthAndHeightAreTheSame(radii.BottomLeft())) {
    return false;
  }

  // Rounded corners that differ are not supported by the
  // CALayerOverlay system on Mac. Instead of letting it fall back
  // to the (worse for memory and battery) non-CALayerOverlay system
  // for such cases, fall back to a non-fast border-radius mask for
  // the effect node.
#if defined(OS_MACOSX)
  if (radii.TopLeft() != radii.TopRight() ||
      radii.TopLeft() != radii.BottomRight() ||
      radii.TopLeft() != radii.BottomLeft()) {
    return false;
  }
#endif

  return true;
}

SkBlendMode PropertyTreeManager::SynthesizeCcEffectsForClipsIfNeeded(
    const ClipPaintPropertyNode& target_clip_arg,
    SkBlendMode delegated_blend) {
  const auto* target_clip = &target_clip_arg.Unalias();
  if (delegated_blend != SkBlendMode::kSrcOver) {
    // Exit all synthetic effect node if the next child has exotic blending mode
    // because it has to access the backdrop of enclosing effect.
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();

    // An effect node can't omit render surface if it has child with exotic
    // blending mode. See comments below for more detail.
    // TODO(crbug.com/504464): Remove premature optimization here.
    SetCurrentEffectRenderSurfaceReason(cc::RenderSurfaceReason::kBlendMode);
  } else {
    // Exit synthetic effects until there are no more synthesized clips below
    // our lowest common ancestor.
    const auto& lca =
        LowestCommonAncestor(*current_.clip, *target_clip).Unalias();
    while (current_.clip != &lca) {
      if (!IsCurrentCcEffectSynthetic()) {
        // This happens in pre-CompositeAfterPaint due to some clip-escaping
        // corner cases that are very difficult to fix in legacy architecture.
        // In CompositeAfterPaint this should never happen.
        if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
          NOTREACHED();
        return delegated_blend;
      }
      const auto* pre_exit_clip = current_.clip;
      CloseCcEffect();
      // We may run past the lowest common ancestor because it may not have
      // been synthesized.
      if (IsNodeOnAncestorChain(lca, *pre_exit_clip, *current_.clip))
        break;
    }
  }

  struct PendingClip {
    const ClipPaintPropertyNode* clip;
    CcEffectType type;
  };
  Vector<PendingClip> pending_clips;
  for (; target_clip && target_clip != current_.clip;
       target_clip = SafeUnalias(target_clip->Parent())) {
    if (auto type = SyntheticEffectType(*target_clip))
      pending_clips.emplace_back(PendingClip{target_clip, type});
  }

  if (!target_clip) {
    // This means that current_.clip is not an ancestor of the target clip.
    // which happens in pre-CompositeAfterPaint due to some clip-escaping
    // corner cases that are very difficult to fix in legacy architecture.
    // In CompositeAfterPaint this should never happen.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      NOTREACHED();
    return delegated_blend;
  }

  for (size_t i = pending_clips.size(); i--;) {
    const auto& pending_clip = pending_clips[i];

    // For a non-trivial clip, the synthetic effect is an isolation to enclose
    // only the layers that should be masked by the synthesized clip.
    // For a non-2d-axis-preserving clip, the synthetic effect creates a render
    // surface which is axis-aligned with the clip.
    cc::EffectNode& synthetic_effect = *GetEffectTree().Node(
        GetEffectTree().Insert(cc::EffectNode(), current_.effect_id));
    if (pending_clip.type & CcEffectType::kSyntheticForNonTrivialClip) {
      synthetic_effect.clip_id = EnsureCompositorClipNode(*pending_clip.clip);
      // For non-trivial clip, isolation_effect.stable_id will be assigned later
      // when the effect is closed. For now the default value INVALID_STABLE_ID
      // is used. See PropertyTreeManager::EmitClipMaskLayer().
    } else {
      synthetic_effect.stable_id =
          CompositorElementIdFromUniqueObjectId(NewUniqueObjectId())
              .GetStableId();
      // The clip of the synthetic effect is the parent of the clip, so that
      // the clip itself will be applied in the render surface.
      DCHECK(pending_clip.clip->Parent());
      synthetic_effect.clip_id =
          EnsureCompositorClipNode(*pending_clip.clip->Parent());
    }
    const auto& transform = pending_clip.clip->LocalTransformSpace();
    synthetic_effect.transform_id = EnsureCompositorTransformNode(transform);
    synthetic_effect.double_sided = !transform.IsBackfaceHidden();
    if (pending_clip.type & CcEffectType::kSyntheticForNonTrivialClip) {
      if (SupportsShaderBasedRoundedCorner(*pending_clip.clip,
                                           pending_clip.type)) {
        synthetic_effect.rounded_corner_bounds =
            gfx::RRectF(pending_clip.clip->ClipRect());
        synthetic_effect.is_fast_rounded_corner = true;

        // Nested rounded corner clips need to force render surfaces for
        // clips other than the leaf ones, because the compositor doesn't
        // know how to apply two rounded clips to the same draw quad.
        if (current_.contained_by_non_render_surface_synthetic_rounded_clip) {
          ForceRenderSurfaceIfSyntheticRoundedCornerClip(current_);
          for (auto effect_it = effect_stack_.rbegin();
               effect_it != effect_stack_.rend(); ++effect_it) {
            auto& effect_node = *GetEffectTree().Node(effect_it->effect_id);
            if (effect_node.HasRenderSurface())
              break;
            ForceRenderSurfaceIfSyntheticRoundedCornerClip(*effect_it);
          }
        }
      } else {
        synthetic_effect.render_surface_reason =
            pending_clip.clip->ClipRect().IsRounded()
                ? cc::RenderSurfaceReason::kRoundedCorner
                : cc::RenderSurfaceReason::kClipPath;
      }
      pending_synthetic_mask_layers_.insert(synthetic_effect.id);
    }
    if (pending_clip.type & CcEffectType::kSyntheticFor2dAxisAlignment) {
      synthetic_effect.render_surface_reason =
          cc::RenderSurfaceReason::kClipAxisAlignment;
    }

    // Clip and kDstIn do not commute. This shall never be reached because
    // kDstIn is only used internally to implement CSS clip-path and mask,
    // and there is never a difference between the output clip of the effect
    // and the mask content.
    DCHECK(delegated_blend != SkBlendMode::kDstIn);
    synthetic_effect.blend_mode = delegated_blend;
    delegated_blend = SkBlendMode::kSrcOver;

    effect_stack_.emplace_back(current_);
    SetCurrentEffectState(synthetic_effect, pending_clip.type, *current_.effect,
                          *pending_clip.clip);
  }

  return delegated_blend;
}

void PropertyTreeManager::BuildEffectNodesRecursively(
    const EffectPaintPropertyNode& next_effect_arg) {
  const auto& next_effect = next_effect_arg.Unalias();
  if (&next_effect == current_.effect)
    return;

  DCHECK(next_effect.Parent());
  BuildEffectNodesRecursively(*next_effect.Parent());
  DCHECK_EQ(&next_effect.Parent()->Unalias(), current_.effect);

#if DCHECK_IS_ON()
  DCHECK(!GetEffectTree().Node(next_effect.CcNodeId(new_sequence_number_)))
      << "Malformed paint artifact. Paint chunks under the same effect should "
         "be contiguous.";
#endif

  // If we don't have an output clip, then we'll use the clip of the last
  // non-synthetic effect. This means we should close all synthetic effects on
  // the stack first.
  if (!next_effect.OutputClip()) {
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();
  }

  SkBlendMode blend_mode;
  const ClipPaintPropertyNode* output_clip;
  int output_clip_id;
  std::tie(blend_mode, output_clip, output_clip_id) =
      GetBlendModeAndOutputClipForEffect(next_effect);

  int effect_node_id =
      GetEffectTree().Insert(cc::EffectNode(), current_.effect_id);
  auto& effect_node = *GetEffectTree().Node(effect_node_id);
  next_effect.SetCcNodeId(new_sequence_number_, effect_node_id);

  PopulateCcEffectNode(effect_node, next_effect, output_clip_id, blend_mode);

  CompositorElementId compositor_element_id =
      next_effect.GetCompositorElementId();
  if (compositor_element_id) {
    DCHECK(!property_trees_.element_id_to_effect_node_index.contains(
        compositor_element_id));
    property_trees_.element_id_to_effect_node_index[compositor_element_id] =
        effect_node.id;
  }

  effect_stack_.emplace_back(current_);
  SetCurrentEffectState(effect_node, CcEffectType::kEffect, next_effect,
                        *output_clip);
}

std::tuple<SkBlendMode, const ClipPaintPropertyNode*, int>
PropertyTreeManager::GetBlendModeAndOutputClipForEffect(
    const EffectPaintPropertyNode& effect) {
  SkBlendMode blend_mode;
  int output_clip_id;
  const auto* output_clip = SafeUnalias(effect.OutputClip());
  if (output_clip) {
    blend_mode =
        SynthesizeCcEffectsForClipsIfNeeded(*output_clip, effect.BlendMode());
    output_clip_id = EnsureCompositorClipNode(*output_clip);
  } else {
    DCHECK(!IsCurrentCcEffectSynthetic());
    // An effect node can't omit render surface if it has child with exotic
    // blending mode.
    // TODO(crbug.com/504464): Remove premature optimization here.
    if (effect.BlendMode() != SkBlendMode::kSrcOver)
      SetCurrentEffectRenderSurfaceReason(cc::RenderSurfaceReason::kBlendMode);

    blend_mode = effect.BlendMode();
    output_clip = current_.clip;
    DCHECK(output_clip);
    output_clip_id = GetEffectTree().Node(current_.effect_id)->clip_id;
    DCHECK_EQ(output_clip_id, EnsureCompositorClipNode(*output_clip));
  }
  return std::make_tuple(blend_mode, output_clip, output_clip_id);
}

void PropertyTreeManager::PopulateCcEffectNode(
    cc::EffectNode& effect_node,
    const EffectPaintPropertyNode& effect,
    int output_clip_id,
    SkBlendMode blend_mode) {
  effect_node.stable_id = effect.GetCompositorElementId().GetStableId();
  effect_node.clip_id = output_clip_id;

  // An effect with filters or backdrop filters needs a render surface.
  // Also, kDstIn and kSrcOver blend modes have fast paths if only one layer
  // is under the blend mode. This value is adjusted in PaintArtifactCompositor
  // ::UpdateRenderSurfaceForEffects() to account for more than one layer.
  if (!effect.Filter().IsEmpty()) {
    effect_node.render_surface_reason = cc::RenderSurfaceReason::kFilter;
  } else if (effect.HasActiveFilterAnimation()) {
    effect_node.render_surface_reason =
        cc::RenderSurfaceReason::kFilterAnimation;
  } else if (!effect.BackdropFilter().IsEmpty()) {
    effect_node.render_surface_reason =
        cc::RenderSurfaceReason::kBackdropFilter;
  } else if (effect.HasActiveBackdropFilterAnimation()) {
    effect_node.render_surface_reason =
        cc::RenderSurfaceReason::kBackdropFilterAnimation;
  } else if (blend_mode != SkBlendMode::kSrcOver &&
             blend_mode != SkBlendMode::kDstIn) {
    effect_node.render_surface_reason = cc::RenderSurfaceReason::kBlendMode;
  }

  effect_node.opacity = effect.Opacity();
  if (effect.GetColorFilter() != kColorFilterNone) {
    // Currently color filter is only used by SVG masks.
    // We are cutting corner here by support only specific configuration.
    DCHECK(effect.GetColorFilter() == kColorFilterLuminanceToAlpha);
    DCHECK(blend_mode == SkBlendMode::kDstIn);
    DCHECK(effect.Filter().IsEmpty());
    effect_node.filters.Append(cc::FilterOperation::CreateReferenceFilter(
        sk_make_sp<ColorFilterPaintFilter>(SkLumaColorFilter::Make(),
                                           nullptr)));
  } else {
    effect_node.filters = effect.Filter().AsCcFilterOperations();
    effect_node.backdrop_filters =
        effect.BackdropFilter().AsCcFilterOperations();
    effect_node.backdrop_filter_bounds = effect.BackdropFilterBounds();
    effect_node.backdrop_mask_element_id = effect.BackdropMaskElementId();
    effect_node.filters_origin = effect.FiltersOrigin();
    effect_node.transform_id =
        EnsureCompositorTransformNode(effect.LocalTransformSpace());
  }
  effect_node.blend_mode = blend_mode;
  effect_node.double_sided = !effect.LocalTransformSpace().IsBackfaceHidden();
  effect_node.effect_changed = effect.NodeChangeAffectsRaster();
}

}  // namespace blink
