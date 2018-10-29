// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace cc {
class ClipTree;
class EffectTree;
class Layer;
class PropertyTrees;
class ScrollTree;
class TransformTree;
struct EffectNode;
struct TransformNode;
}

namespace blink {

class ClipPaintPropertyNode;
class LayerListBuilder;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class TransformPaintPropertyNode;

class PropertyTreeManagerClient {
 public:
  virtual cc::Layer* CreateOrReuseSynthesizedClipLayer(
      const ClipPaintPropertyNode*,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) = 0;
};

// Mutates a cc property tree to reflect Blink paint property tree
// state. Intended for use by PaintArtifactCompositor.
class PropertyTreeManager {

 public:
  PropertyTreeManager(PropertyTreeManagerClient&,
                      cc::PropertyTrees&,
                      cc::Layer* root_layer,
                      LayerListBuilder*);
  ~PropertyTreeManager() {
    DCHECK(!effect_stack_.size()) << "PropertyTreeManager::Finalize() must be "
                                     "called at the end of tree conversion.";
  }

  void SetupRootTransformNode();
  void SetupRootClipNode();
  void SetupRootEffectNode();
  void SetupRootScrollNode();

  // A brief discourse on cc property tree nodes, identifiers, and current and
  // future design evolution envisioned:
  //
  // cc property trees identify nodes by their |id|, which implementation-wise
  // is actually its index in the property tree's vector of its node type. More
  // recent cc code now refers to these as 'node indices', or 'property tree
  // indices'. |parent_id| is the same sort of 'node index' of that node's
  // parent.
  //
  // Note there are two other primary types of 'ids' referenced in cc property
  // tree related logic: (1) ElementId, also known Blink-side as
  // CompositorElementId, used by the animation system to allow tying an element
  // to its respective layer, and (2) layer ids. There are other ancillary ids
  // not relevant to any of the above, such as
  // cc::TransformNode::sorting_context_id
  // (a.k.a. blink::TransformPaintPropertyNode::renderingContextId()).
  //
  // There is a vision to move toward a world where cc property nodes have no
  // association with layers and instead have a |stable_id|. The id could come
  // from an ElementId in turn derived from the layout object responsible for
  // creating the property node.
  //
  // We would also like to explore moving to use a single shared property tree
  // representation across both cc and Blink. See
  // platform/graphics/paint/README.md for more.
  //
  // With the above as background, we can now state more clearly a description
  // of the below set of compositor node methods: they take Blink paint property
  // tree nodes as input, create a corresponding compositor property tree node
  // if none yet exists, and return the compositor node's 'node id', a.k.a.,
  // 'node index'.

  // Returns the compositor transform node id. If a compositor transform node
  // does not exist, it is created. Any transforms that are for scroll offset
  // translation will ensure the associated scroll node exists.
  int EnsureCompositorTransformNode(const TransformPaintPropertyNode*);
  int EnsureCompositorClipNode(const ClipPaintPropertyNode*);
  // Ensure the compositor scroll node using the associated scroll offset
  // translation.
  int EnsureCompositorScrollNode(
      const TransformPaintPropertyNode* scroll_offset_translation);

  void EnsureCompositorPageScaleTransformNode(
      const TransformPaintPropertyNode*);

  // This function is expected to be invoked right before emitting each layer.
  // It keeps track of the nesting of clip and effects, output a composited
  // effect node whenever an effect is entered, or a non-trivial clip is
  // entered. In the latter case, the generated composited effect node is
  // called a "synthetic effect", and the corresponding clip a "synthesized
  // clip". Upon exiting a synthesized clip, a mask layer will be appended,
  // which will be kDstIn blended on top of contents enclosed by the synthetic
  // effect, i.e. applying the clip as a mask.
  int SwitchToEffectNodeWithSynthesizedClip(
      const EffectPaintPropertyNode& next_effect,
      const ClipPaintPropertyNode& next_clip);
  // Expected to be invoked after emitting the last layer. This will exit all
  // effects on the effect stack, generating clip mask layers for all the
  // unclosed synthesized clips.
  void Finalize();

 private:
  bool BuildEffectNodesRecursively(const EffectPaintPropertyNode* next_effect);
  SkBlendMode SynthesizeCcEffectsForClipsIfNeeded(
      const ClipPaintPropertyNode* target_clip,
      SkBlendMode delegated_blend,
      bool effect_is_newly_built);
  void EmitClipMaskLayer();
  void CloseCcEffect();

  bool IsCurrentCcEffectSynthetic() const {
    return current_.effect_type != CcEffectType::kEffect;
  }
  bool IsCurrentCcEffectSyntheticForNonTrivialClip() const {
    return current_.effect_type == CcEffectType::kSyntheticForNonTrivialClip;
  }

  // The type of operation the current cc effect node applies.
  enum class CcEffectType {
    // The cc effect corresponds to a Blink effect node.
    kEffect,
    // The cc effect is synthetic for a blink clip node that has to be
    // rasterized because the clip is non-trivial.
    kSyntheticForNonTrivialClip,
    // The cc effect is synthetic to create a render surface that is
    // 2d-axis-aligned with a blink clip node that is non-2d-axis-aligned
    // in the the original render surface. Cc requires a rectangular clip to be
    // 2d-axis-aligned with the render surface to correctly apply the clip.
    // TODO(crbug.com/504464): This will be changed when we move render surface
    // decision logic into the cc compositor thread.
    kSyntheticFor2dAxisAlignment,
  };

  base::Optional<CcEffectType> NeedsSyntheticEffect(
      const ClipPaintPropertyNode&) const;

  void SetCurrentEffectState(const cc::EffectNode&,
                             CcEffectType,
                             const EffectPaintPropertyNode*,
                             const ClipPaintPropertyNode*);
  void SetCurrentEffectHasRenderSurface();

  cc::TransformTree& GetTransformTree();
  cc::ClipTree& GetClipTree();
  cc::EffectTree& GetEffectTree();
  cc::ScrollTree& GetScrollTree();

  // Should only be called from EnsureCompositorTransformNode as part of
  // creating the associated scroll offset transform node.
  void CreateCompositorScrollNode(
      const ScrollPaintPropertyNode*,
      const cc::TransformNode& scroll_offset_translation);

  PropertyTreeManagerClient& client_;

  // Property trees which should be updated by the manager.
  cc::PropertyTrees& property_trees_;

  // The special layer which is the parent of every other layers.
  // This is where clip mask layers we generated for synthesized clips are
  // appended into.
  cc::Layer* root_layer_;

  LayerListBuilder* layer_list_builder_;

  // Maps from Blink-side property tree nodes to cc property node indices.
  HashMap<const TransformPaintPropertyNode*, int> transform_node_map_;
  HashMap<const ClipPaintPropertyNode*, int> clip_node_map_;
  HashMap<const ScrollPaintPropertyNode*, int> scroll_node_map_;

  struct EffectState {
    // The cc effect node that has the corresponding drawing state to the
    // effect and clip state from the last
    // SwitchToEffectNodeWithSynthesizedClip.
    int effect_id;
    CcEffectType effect_type;
    // The effect state of the cc effect node.
    const EffectPaintPropertyNode* effect;
    // The clip state of the cc effect node. This value may be shallower than
    // the one passed into SwitchToEffectNodeWithSynthesizedClip because not
    // every clip needs to be synthesized as cc effect.
    // Is set to output clip of the effect if the type is kEffect, or set to the
    // synthesized clip node if the type is kSyntheticForNonTrivialClip.
    const ClipPaintPropertyNode* clip;
    // The transform space of the containing render surface.
    // TODO(crbug.com/504464): Remove this when move render surface decision
    // logic into cc compositor thread.
    const TransformPaintPropertyNode* render_surface_transform;
  };

  // The current effect state. Virtually it's the top of the effect stack if
  // it and effect_stack_ are treated as a whole stack.
  EffectState current_;

  // This keep track of cc effect stack. Whenever a new cc effect is nested,
  // a new entry is pushed, and the entry will be popped when the effect closed.
  // Note: This is a "restore stack", i.e. the top element does not represent
  // the current state (which is in current_), but the state prior to most
  // recent push.
  Vector<EffectState> effect_stack_;

#if DCHECK_IS_ON()
  HashSet<const EffectPaintPropertyNode*> effect_nodes_converted_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PropertyTreeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_
