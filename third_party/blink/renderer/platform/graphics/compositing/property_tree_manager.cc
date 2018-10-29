// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"

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

PropertyTreeManager::PropertyTreeManager(PropertyTreeManagerClient& client,
                                         cc::PropertyTrees& property_trees,
                                         cc::Layer* root_layer,
                                         LayerListBuilder* layer_list_builder)
    : client_(client),
      property_trees_(property_trees),
      root_layer_(root_layer),
      layer_list_builder_(layer_list_builder) {
  SetupRootTransformNode();
  SetupRootClipNode();
  SetupRootEffectNode();
  SetupRootScrollNode();
}

void PropertyTreeManager::Finalize() {
  while (effect_stack_.size())
    CloseCcEffect();
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
  transform_node.source_node_id = transform_node.parent_id;

  // TODO(jaydasika): We shouldn't set ToScreen and FromScreen of root
  // transform node here. They should be set while updating transform tree in
  // cc.
  float device_scale_factor =
      root_layer_->layer_tree_host()->device_scale_factor();
  gfx::Transform to_screen;
  to_screen.Scale(device_scale_factor, device_scale_factor);
  transform_tree.SetToScreen(kRealRootNodeId, to_screen);
  gfx::Transform from_screen;
  bool invertible = to_screen.GetInverse(&from_screen);
  DCHECK(invertible);
  transform_tree.SetFromScreen(kRealRootNodeId, from_screen);
  transform_tree.set_needs_update(true);

  transform_node_map_.Set(&TransformPaintPropertyNode::Root(),
                          transform_node.id);
  root_layer_->SetTransformTreeIndex(transform_node.id);
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
  clip_node.clip = gfx::RectF(
      gfx::SizeF(root_layer_->layer_tree_host()->device_viewport_size()));
  clip_node.transform_id = kRealRootNodeId;

  clip_node_map_.Set(&ClipPaintPropertyNode::Root(), clip_node.id);
  root_layer_->SetClipTreeIndex(clip_node.id);
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
      CompositorElementIdFromUniqueObjectId(unique_id).GetInternalValue();
  effect_node.transform_id = kRealRootNodeId;
  effect_node.clip_id = kSecondaryRootNodeId;
  effect_node.has_render_surface = true;
  root_layer_->SetEffectTreeIndex(effect_node.id);

  SetCurrentEffectState(effect_node, CcEffectType::kEffect,
                        &EffectPaintPropertyNode::Root(),
                        &ClipPaintPropertyNode::Root());
}

void PropertyTreeManager::SetupRootScrollNode() {
  cc::ScrollTree& scroll_tree = property_trees_.scroll_tree;
  scroll_tree.clear();
  property_trees_.element_id_to_scroll_node_index.clear();
  cc::ScrollNode& scroll_node =
      *scroll_tree.Node(scroll_tree.Insert(cc::ScrollNode(), kRealRootNodeId));
  DCHECK_EQ(scroll_node.id, kSecondaryRootNodeId);
  scroll_node.transform_id = kSecondaryRootNodeId;

  scroll_node_map_.Set(&ScrollPaintPropertyNode::Root(), scroll_node.id);
  root_layer_->SetScrollTreeIndex(scroll_node.id);
}

void PropertyTreeManager::SetCurrentEffectState(
    const cc::EffectNode& cc_effect_node,
    CcEffectType effect_type,
    const EffectPaintPropertyNode* effect,
    const ClipPaintPropertyNode* clip) {
  current_.effect_id = cc_effect_node.id;
  current_.effect_type = effect_type;
  DCHECK(!effect->IsParentAlias() || !effect->Parent());
  current_.effect = effect;
  DCHECK(!clip->IsParentAlias() || !clip->Parent());
  current_.clip = clip;
  if (cc_effect_node.has_render_surface)
    current_.render_surface_transform = effect->LocalTransformSpace();
}

// TODO(crbug.com/504464): Remove this when move render surface decision logic
// into cc compositor thread.
void PropertyTreeManager::SetCurrentEffectHasRenderSurface() {
  GetEffectTree().Node(current_.effect_id)->has_render_surface = true;
  current_.render_surface_transform = current_.effect->LocalTransformSpace();
}

int PropertyTreeManager::EnsureCompositorTransformNode(
    const TransformPaintPropertyNode* transform_node) {
  transform_node = transform_node->Unalias();
  auto it = transform_node_map_.find(transform_node);
  if (it != transform_node_map_.end())
    return it->value;

  int parent_id = EnsureCompositorTransformNode(transform_node->Parent());
  int id = GetTransformTree().Insert(cc::TransformNode(), parent_id);

  cc::TransformNode& compositor_node = *GetTransformTree().Node(id);
  compositor_node.source_node_id = parent_id;

  FloatPoint3D origin = transform_node->Origin();
  compositor_node.pre_local.matrix().setTranslate(-origin.X(), -origin.Y(),
                                                  -origin.Z());
  // The sticky offset on the blink transform node is pre-computed and stored
  // to the local matrix. Cc applies sticky offset dynamically on top of the
  // local matrix. We should not set the local matrix on cc node if it is a
  // sticky node because the sticky offset would be applied twice otherwise.
  if (!transform_node->GetStickyConstraint()) {
    compositor_node.local.matrix() =
        TransformationMatrix::ToSkMatrix44(transform_node->Matrix());
  }
  compositor_node.post_local.matrix().setTranslate(origin.X(), origin.Y(),
                                                   origin.Z());
  compositor_node.needs_local_transform_update = true;
  compositor_node.flattens_inherited_transform =
      transform_node->FlattensInheritedTransform();
  compositor_node.sorting_context_id = transform_node->RenderingContextId();

  if (transform_node->IsAffectedByOuterViewportBoundsDelta()) {
    compositor_node.moved_by_outer_viewport_bounds_delta_y = true;
    GetTransformTree().AddNodeAffectedByOuterViewportBoundsDelta(id);
  }

  if (const auto* sticky_constraint = transform_node->GetStickyConstraint()) {
    DCHECK(sticky_constraint->is_sticky);
    cc::StickyPositionNodeData* sticky_data =
        GetTransformTree().StickyPositionData(id);
    sticky_data->constraints = *sticky_constraint;
    // TODO(pdr): This could be a performance issue because it crawls up the
    // transform tree for each pending layer. If this is on profiles, we should
    // cache a lookup of transform node to scroll translation transform node.
    const auto& scroll_ancestor =
        transform_node->NearestScrollTranslationNode();
    sticky_data->scroll_ancestor = EnsureCompositorScrollNode(&scroll_ancestor);
    if (scroll_ancestor.ScrollNode()->ScrollsOuterViewport())
      GetTransformTree().AddNodeAffectedByOuterViewportBoundsDelta(id);
    if (CompositorElementId shifting_sticky_box_element_id =
            sticky_data->constraints.nearest_element_shifting_sticky_box) {
      sticky_data->nearest_node_shifting_sticky_box =
          GetTransformTree()
              .FindNodeFromElementId(shifting_sticky_box_element_id)
              ->id;
    }
    if (CompositorElementId shifting_containing_block_element_id =
            sticky_data->constraints
                .nearest_element_shifting_containing_block) {
      sticky_data->nearest_node_shifting_containing_block =
          GetTransformTree()
              .FindNodeFromElementId(shifting_containing_block_element_id)
              ->id;
    }
  }

  CompositorElementId compositor_element_id =
      transform_node->GetCompositorElementId();
  if (compositor_element_id) {
    property_trees_.element_id_to_transform_node_index[compositor_element_id] =
        id;
    compositor_node.element_id = compositor_element_id;
  }

  // If this transform is a scroll offset translation, create the associated
  // compositor scroll property node and adjust the compositor transform node's
  // scroll offset.
  if (auto* scroll_node = transform_node->ScrollNode()) {
    // Blink creates a 2d transform node just for scroll offset whereas cc's
    // transform node has a special scroll offset field. To handle this we
    // adjust cc's transform node to remove the 2d scroll translation and
    // instead set the scroll_offset field.
    auto scroll_offset_size = transform_node->Matrix().To2DTranslation();
    auto scroll_offset = gfx::ScrollOffset(-scroll_offset_size.Width(),
                                           -scroll_offset_size.Height());
    DCHECK(compositor_node.local.IsIdentityOr2DTranslation());
    compositor_node.scroll_offset = scroll_offset;
    compositor_node.local.MakeIdentity();
    compositor_node.scrolls = true;
    compositor_node.should_be_snapped = true;

    CreateCompositorScrollNode(scroll_node, compositor_node);
  }

  // If the parent transform node flattens transform (as |transform_node|
  // flattens inherited transform) while it participates in the 3d sorting
  // context of an ancestor, cc needs a render surface for correct flattening.
  // TODO(crbug.com/504464): Move the logic into cc compositor thread.
  auto* current_cc_effect = GetEffectTree().Node(current_.effect_id);
  if (current_cc_effect && !current_cc_effect->has_render_surface &&
      current_cc_effect->transform_id == parent_id &&
      transform_node->FlattensInheritedTransform() &&
      transform_node->Parent() &&
      transform_node->Parent()->RenderingContextId() &&
      !transform_node->Parent()->FlattensInheritedTransform())
    SetCurrentEffectHasRenderSurface();

  auto result = transform_node_map_.Set(transform_node, id);
  DCHECK(result.is_new_entry);
  GetTransformTree().set_needs_update(true);

  return id;
}

void PropertyTreeManager::EnsureCompositorPageScaleTransformNode(
    const TransformPaintPropertyNode* node) {
  int id = EnsureCompositorTransformNode(node);
  DCHECK(GetTransformTree().Node(id));
  cc::TransformNode& compositor_node = *GetTransformTree().Node(id);

  // The page scale node is special because its transform matrix is assumed to
  // be in the post_local matrix by the compositor. There should be no
  // translation from the origin so we clear the other matrices.
  DCHECK(node->Origin() == FloatPoint3D());
  compositor_node.post_local.matrix() = compositor_node.local.matrix();
  compositor_node.pre_local.matrix().setIdentity();
  compositor_node.local.matrix().setIdentity();
}

int PropertyTreeManager::EnsureCompositorClipNode(
    const ClipPaintPropertyNode* clip_node) {
  clip_node = clip_node->Unalias();
  auto it = clip_node_map_.find(clip_node);
  if (it != clip_node_map_.end())
    return it->value;

  int parent_id = EnsureCompositorClipNode(clip_node->Parent());
  int id = GetClipTree().Insert(cc::ClipNode(), parent_id);

  cc::ClipNode& compositor_node = *GetClipTree().Node(id);

  compositor_node.clip = clip_node->ClipRect().Rect();
  compositor_node.transform_id =
      EnsureCompositorTransformNode(clip_node->LocalTransformSpace());
  compositor_node.clip_type = cc::ClipNode::ClipType::APPLIES_LOCAL_CLIP;

  auto result = clip_node_map_.Set(clip_node, id);
  DCHECK(result.is_new_entry);
  GetClipTree().set_needs_update(true);
  return id;
}

void PropertyTreeManager::CreateCompositorScrollNode(
    const ScrollPaintPropertyNode* scroll_node,
    const cc::TransformNode& scroll_offset_translation) {
  DCHECK(!scroll_node_map_.Contains(scroll_node));

  auto parent_it = scroll_node_map_.find(scroll_node->Parent());
  // Compositor transform nodes up to scroll_offset_translation must exist.
  // Scrolling uses the transform tree for scroll offsets so this means all
  // ancestor scroll nodes must also exist.
  DCHECK(parent_it != scroll_node_map_.end());
  int parent_id = parent_it->value;
  int id = GetScrollTree().Insert(cc::ScrollNode(), parent_id);

  cc::ScrollNode& compositor_node = *GetScrollTree().Node(id);
  compositor_node.scrollable = true;

  compositor_node.container_bounds =
      static_cast<gfx::Size>(scroll_node->ContainerRect().Size());
  compositor_node.bounds = static_cast<gfx::Size>(scroll_node->ContentsSize());
  compositor_node.user_scrollable_horizontal =
      scroll_node->UserScrollableHorizontal();
  compositor_node.user_scrollable_vertical =
      scroll_node->UserScrollableVertical();
  compositor_node.scrolls_inner_viewport = scroll_node->ScrollsInnerViewport();
  compositor_node.scrolls_outer_viewport = scroll_node->ScrollsOuterViewport();
  compositor_node.max_scroll_offset_affected_by_page_scale =
      scroll_node->MaxScrollOffsetAffectedByPageScale();
  compositor_node.main_thread_scrolling_reasons =
      scroll_node->GetMainThreadScrollingReasons();
  compositor_node.overscroll_behavior = cc::OverscrollBehavior(
      static_cast<cc::OverscrollBehavior::OverscrollBehaviorType>(
          scroll_node->OverscrollBehaviorX()),
      static_cast<cc::OverscrollBehavior::OverscrollBehaviorType>(
          scroll_node->OverscrollBehaviorY()));
  compositor_node.snap_container_data = scroll_node->GetSnapContainerData();

  auto compositor_element_id = scroll_node->GetCompositorElementId();
  if (compositor_element_id) {
    compositor_node.element_id = compositor_element_id;
    property_trees_.element_id_to_scroll_node_index[compositor_element_id] = id;
  }

  compositor_node.transform_id = scroll_offset_translation.id;

  // TODO(pdr): Set the scroll node's non_fast_scrolling_region value.

  auto result = scroll_node_map_.Set(scroll_node, id);
  DCHECK(result.is_new_entry);

  GetScrollTree().SetScrollOffset(compositor_element_id,
                                  scroll_offset_translation.scroll_offset);
  GetScrollTree().set_needs_update(true);
}

int PropertyTreeManager::EnsureCompositorScrollNode(
    const TransformPaintPropertyNode* scroll_offset_translation) {
  const auto* scroll_node = scroll_offset_translation->ScrollNode();
  DCHECK(scroll_node);
  EnsureCompositorTransformNode(scroll_offset_translation);
  auto it = scroll_node_map_.find(scroll_node);
  DCHECK(it != scroll_node_map_.end());
  return it->value;
}

void PropertyTreeManager::EmitClipMaskLayer() {
  int clip_id = EnsureCompositorClipNode(current_.clip);
  CompositorElementId mask_isolation_id, mask_effect_id;
  cc::Layer* mask_layer = client_.CreateOrReuseSynthesizedClipLayer(
      current_.clip, mask_isolation_id, mask_effect_id);

  cc::EffectNode& mask_isolation = *GetEffectTree().Node(current_.effect_id);
  // Assignment of mask_isolation.stable_id was delayed until now.
  // See PropertyTreeManager::SynthesizeCcEffectsForClipsIfNeeded().
  DCHECK_EQ(static_cast<uint64_t>(cc::EffectNode::INVALID_STABLE_ID),
            mask_isolation.stable_id);
  mask_isolation.stable_id = mask_isolation_id.GetInternalValue();

  cc::EffectNode& mask_effect = *GetEffectTree().Node(
      GetEffectTree().Insert(cc::EffectNode(), current_.effect_id));
  mask_effect.stable_id = mask_effect_id.GetInternalValue();
  mask_effect.clip_id = clip_id;
  mask_effect.has_render_surface = true;
  mask_effect.blend_mode = SkBlendMode::kDstIn;

  const auto* clip_space = current_.clip->LocalTransformSpace();
  layer_list_builder_->Add(mask_layer);
  mask_layer->set_property_tree_sequence_number(
      root_layer_->property_tree_sequence_number());
  mask_layer->SetTransformTreeIndex(EnsureCompositorTransformNode(clip_space));
  // TODO(pdr): This could be a performance issue because it crawls up the
  // transform tree for each pending layer. If this is on profiles, we should
  // cache a lookup of transform node to scroll translation transform node.
  int scroll_id =
      EnsureCompositorScrollNode(&clip_space->NearestScrollTranslationNode());
  mask_layer->SetScrollTreeIndex(scroll_id);
  mask_layer->SetClipTreeIndex(clip_id);
  mask_layer->SetEffectTreeIndex(mask_effect.id);
}

void PropertyTreeManager::CloseCcEffect() {
  DCHECK(effect_stack_.size());
  const auto& previous_state = effect_stack_.back();

  // An effect with exotic blending that is masked by a synthesized clip must
  // have its blending to the outermost synthesized clip. It is because
  // blending needs access to the backdrop of the enclosing effect. With
  // the isolation for a synthesized clip, a blank backdrop will be seen.
  // Therefore the blending is delegated to the outermost synthesized clip,
  // thus the clip can't be shared with sibling layers, and must be closed now.
  bool clear_synthetic_effects =
      !IsCurrentCcEffectSynthetic() &&
      current_.effect->BlendMode() != SkBlendMode::kSrcOver;

  // We are about to close an effect that was synthesized for isolating
  // a clip mask. Now emit the actual clip mask that will be composited on
  // top of masked contents with SkBlendMode::kDstIn.
  if (IsCurrentCcEffectSyntheticForNonTrivialClip())
    EmitClipMaskLayer();

  current_ = previous_state;
  effect_stack_.pop_back();

  if (clear_synthetic_effects) {
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();
  }
}

static bool TransformsAre2dAxisAligned(const TransformPaintPropertyNode* a,
                                       const TransformPaintPropertyNode* b) {
  return a == b || GeometryMapper::SourceToDestinationProjection(a, b)
                       .Preserves2dAxisAlignment();
}

int PropertyTreeManager::SwitchToEffectNodeWithSynthesizedClip(
    const EffectPaintPropertyNode& next_effect,
    const ClipPaintPropertyNode& next_clip) {
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
      *LowestCommonAncestor(*current_.effect, next_effect).Unalias();
  while (current_.effect != &ancestor)
    CloseCcEffect();

  bool newly_built = BuildEffectNodesRecursively(&next_effect);
  SynthesizeCcEffectsForClipsIfNeeded(&next_clip, SkBlendMode::kSrcOver,
                                      newly_built);
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

base::Optional<PropertyTreeManager::CcEffectType>
PropertyTreeManager::NeedsSyntheticEffect(
    const ClipPaintPropertyNode& clip) const {
  if (clip.ClipRect().IsRounded() || clip.ClipPath())
    return CcEffectType::kSyntheticForNonTrivialClip;

  // Cc requires that a rectangluar clip is 2d-axis-aligned with the render
  // surface to correctly apply the clip.
  if (!TransformsAre2dAxisAligned(clip.LocalTransformSpace(),
                                  current_.render_surface_transform))
    return CcEffectType::kSyntheticFor2dAxisAlignment;

  return base::nullopt;
}

SkBlendMode PropertyTreeManager::SynthesizeCcEffectsForClipsIfNeeded(
    const ClipPaintPropertyNode* target_clip,
    SkBlendMode delegated_blend,
    bool effect_is_newly_built) {
  auto* unaliased_target_clip = target_clip->Unalias();
  if (delegated_blend != SkBlendMode::kSrcOver) {
    // Exit all synthetic effect node if the next child has exotic blending mode
    // because it has to access the backdrop of enclosing effect.
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();

    // An effect node can't omit render surface if it has child with exotic
    // blending mode. See comments below for more detail.
    // TODO(crbug.com/504464): Remove premature optimization here.
    SetCurrentEffectHasRenderSurface();
  } else {
    // Exit synthetic effects until there are no more synthesized clips below
    // our lowest common ancestor.
    const auto& lca =
        *LowestCommonAncestor(*current_.clip, *unaliased_target_clip).Unalias();
    while (current_.clip != &lca) {
      DCHECK(IsCurrentCcEffectSynthetic());
      const auto* pre_exit_clip = current_.clip;
      CloseCcEffect();
      // We may run past the lowest common ancestor because it may not have
      // been synthesized.
      if (IsNodeOnAncestorChain(lca, *pre_exit_clip, *current_.clip))
        break;
    }

    // If the effect is an existing node, i.e. already has at least one paint
    // chunk or child effect, and by reaching here it implies we are going to
    // attach either another paint chunk or child effect to it. We can no longer
    // omit render surface for it even for opacity-only node.
    // See comments in PropertyTreeManager::BuildEffectNodesRecursively().
    // TODO(crbug.com/504464): Remove premature optimization here.
    if (!effect_is_newly_built && !IsCurrentCcEffectSynthetic() &&
        current_.effect->Opacity() != 1.f)
      SetCurrentEffectHasRenderSurface();
  }

  DCHECK(current_.clip->IsAncestorOf(*unaliased_target_clip));

  struct PendingClip {
    const ClipPaintPropertyNode* clip;
    CcEffectType type;
  };
  Vector<PendingClip> pending_clips;
  for (; unaliased_target_clip != current_.clip;
       unaliased_target_clip = unaliased_target_clip->Parent()->Unalias()) {
    DCHECK(unaliased_target_clip);
    if (auto type = NeedsSyntheticEffect(*unaliased_target_clip))
      pending_clips.emplace_back(PendingClip{unaliased_target_clip, *type});
  }

  for (size_t i = pending_clips.size(); i--;) {
    const auto& pending_clip = pending_clips[i];

    // For a non-trivial clip, the synthetic effect is an isolation to enclose
    // only the layers that should be masked by the synthesized clip.
    // For a non-2d-axis-preserving clip, the synthetic effect creates a render
    // surface which is axis-aligned with the clip.
    cc::EffectNode& synthetic_effect = *GetEffectTree().Node(
        GetEffectTree().Insert(cc::EffectNode(), current_.effect_id));
    if (pending_clip.type == CcEffectType::kSyntheticForNonTrivialClip) {
      synthetic_effect.clip_id = EnsureCompositorClipNode(pending_clip.clip);
      // For non-trivial clip, isolation_effect.stable_id will be assigned later
      // when the effect is closed. For now the default value INVALID_STABLE_ID
      // is used. See PropertyTreeManager::EmitClipMaskLayer().
    } else {
      DCHECK_EQ(pending_clip.type, CcEffectType::kSyntheticFor2dAxisAlignment);
      synthetic_effect.stable_id =
          CompositorElementIdFromUniqueObjectId(NewUniqueObjectId())
              .GetInternalValue();
      // The clip of the synthetic effect is the parent of the clip, so that
      // the clip itself will be applied in the render surface.
      synthetic_effect.clip_id =
          EnsureCompositorClipNode(pending_clip.clip->Parent());
    }
    synthetic_effect.transform_id =
        EnsureCompositorTransformNode(pending_clip.clip->LocalTransformSpace());
    synthetic_effect.has_render_surface = true;
    // Clip and kDstIn do not commute. This shall never be reached because
    // kDstIn is only used internally to implement CSS clip-path and mask,
    // and there is never a difference between the output clip of the effect
    // and the mask content.
    DCHECK(delegated_blend != SkBlendMode::kDstIn);
    synthetic_effect.blend_mode = delegated_blend;
    delegated_blend = SkBlendMode::kSrcOver;

    effect_stack_.emplace_back(current_);
    SetCurrentEffectState(synthetic_effect, pending_clip.type, current_.effect,
                          pending_clip.clip);
    current_.render_surface_transform =
        pending_clip.clip->LocalTransformSpace();
  }

  return delegated_blend;
}

bool PropertyTreeManager::BuildEffectNodesRecursively(
    const EffectPaintPropertyNode* next_effect) {
  next_effect = SafeUnalias(next_effect);
  if (next_effect == current_.effect)
    return false;
  DCHECK(next_effect);

  bool newly_built = BuildEffectNodesRecursively(next_effect->Parent());
  DCHECK_EQ(next_effect->Parent()->Unalias(), current_.effect);

#if DCHECK_IS_ON()
  DCHECK(!effect_nodes_converted_.Contains(next_effect))
      << "Malformed paint artifact. Paint chunks under the same effect should "
         "be contiguous.";
  effect_nodes_converted_.insert(next_effect);
#endif

  SkBlendMode used_blend_mode;
  int output_clip_id;
  const auto* output_clip = SafeUnalias(next_effect->OutputClip());
  if (output_clip) {
    used_blend_mode = SynthesizeCcEffectsForClipsIfNeeded(
        output_clip, next_effect->BlendMode(), newly_built);
    output_clip_id = EnsureCompositorClipNode(output_clip);
  } else {
    while (IsCurrentCcEffectSynthetic())
      CloseCcEffect();
    // An effect node can't omit render surface if it has child with exotic
    // blending mode, nor being opacity-only node with more than one child.
    // TODO(crbug.com/504464): Remove premature optimization here.
    if (next_effect->BlendMode() != SkBlendMode::kSrcOver ||
        (!newly_built && current_.effect->Opacity() != 1.f))
      SetCurrentEffectHasRenderSurface();

    used_blend_mode = next_effect->BlendMode();
    output_clip = current_.clip;
    output_clip_id = GetEffectTree().Node(current_.effect_id)->clip_id;
    DCHECK_EQ(output_clip_id, EnsureCompositorClipNode(output_clip));
  }

  cc::EffectNode& effect_node = *GetEffectTree().Node(
      GetEffectTree().Insert(cc::EffectNode(), current_.effect_id));
  effect_node.stable_id =
      next_effect->GetCompositorElementId().GetInternalValue();
  effect_node.clip_id = output_clip_id;
  // Every effect is supposed to have render surface enabled for grouping,
  // but we can get away without one if the effect is opacity-only and has only
  // one compositing child with kSrcOver blend mode. This is both for
  // optimization and not introducing sub-pixel differences in layout tests.
  // See PropertyTreeManager::switchToEffectNode() and above where we
  // retrospectively enable render surface when more than one compositing child
  // or a child with exotic blend mode is detected.
  // TODO(crbug.com/504464): There is ongoing work in cc to delay render surface
  // decision until later phase of the pipeline. Remove premature optimization
  // here once the work is ready.
  if (!next_effect->Filter().IsEmpty() ||
      used_blend_mode != SkBlendMode::kSrcOver)
    effect_node.has_render_surface = true;

  effect_node.opacity = next_effect->Opacity();
  if (next_effect->GetColorFilter() != kColorFilterNone) {
    // Currently color filter is only used by SVG masks.
    // We are cutting corner here by support only specific configuration.
    DCHECK(next_effect->GetColorFilter() == kColorFilterLuminanceToAlpha);
    DCHECK(used_blend_mode == SkBlendMode::kDstIn);
    DCHECK(next_effect->Filter().IsEmpty());
    effect_node.filters.Append(cc::FilterOperation::CreateReferenceFilter(
        sk_make_sp<ColorFilterPaintFilter>(SkLumaColorFilter::Make(),
                                           nullptr)));
  } else {
    effect_node.filters = next_effect->Filter().AsCcFilterOperations();
    effect_node.filters_origin = next_effect->FiltersOrigin();
    effect_node.transform_id =
        EnsureCompositorTransformNode(next_effect->LocalTransformSpace());
  }
  effect_node.blend_mode = used_blend_mode;
  effect_node.double_sided =
      !next_effect->LocalTransformSpace()->IsBackfaceHidden();
  CompositorElementId compositor_element_id =
      next_effect->GetCompositorElementId();
  if (compositor_element_id) {
    DCHECK(property_trees_.element_id_to_effect_node_index.find(
               compositor_element_id) ==
           property_trees_.element_id_to_effect_node_index.end());
    property_trees_.element_id_to_effect_node_index[compositor_element_id] =
        effect_node.id;
  }
  effect_stack_.emplace_back(current_);
  SetCurrentEffectState(effect_node, CcEffectType::kEffect, next_effect,
                        output_clip);

  return true;
}

}  // namespace blink
