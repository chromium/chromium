// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"

namespace cc {
class ClipTree;
class EffectTree;
class Layer;
class LayerTreeHost;
class PropertyTrees;
class ScrollTree;
class TransformTree;
struct EffectNode;
struct TransformNode;
enum class RenderSurfaceReason : uint8_t;
}

namespace blink {

class ClipPaintPropertyNode;
class LayerListBuilder;
class EffectPaintPropertyNode;
class ScrollPaintPropertyNode;
class SynthesizedClip;
class TransformPaintPropertyNode;

class PropertyTreeManagerClient {
 public:
  virtual SynthesizedClip& CreateOrReuseSynthesizedClipLayer(
      const ClipPaintPropertyNode&,
      bool needs_layer,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) = 0;
};

// Mutates a cc property tree to reflect Blink paint property tree
// state. Intended for use by PaintArtifactCompositor.
class PropertyTreeManager {
  DISALLOW_NEW();

 public:
  PropertyTreeManager(PropertyTreeManagerClient&,
                      cc::PropertyTrees& property_trees,
                      cc::Layer& root_layer,
                      LayerListBuilder& layer_list_builder,
                      int new_sequence_number);
  ~PropertyTreeManager();

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
  int EnsureCompositorTransformNode(const TransformPaintPropertyNode&);
  int EnsureCompositorClipNode(const ClipPaintPropertyNode&);
  // Ensure the compositor scroll node using the associated scroll offset
  // translation.
  int EnsureCompositorScrollNode(
      const TransformPaintPropertyNode& scroll_offset_translation);

  int EnsureCompositorPageScaleTransformNode(const TransformPaintPropertyNode&);

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
      const ClipPaintPropertyNode& next_clip,
      bool layer_draws_content);
  // Expected to be invoked after emitting the last layer. This will exit all
  // effects on the effect stack, generating clip mask layers for all the
  // unclosed synthesized clips.
  void Finalize();

  static bool DirectlyUpdateCompositedOpacityValue(
      cc::PropertyTrees*,
      cc::LayerTreeHost&,
      const EffectPaintPropertyNode&);
  static bool DirectlyUpdateScrollOffsetTransform(
      cc::PropertyTrees*,
      cc::LayerTreeHost&,
      const TransformPaintPropertyNode&);
  static bool DirectlyUpdateTransform(cc::PropertyTrees*,
                                      cc::LayerTreeHost&,
                                      const TransformPaintPropertyNode&);
  static bool DirectlyUpdatePageScaleTransform(
      cc::PropertyTrees*,
      cc::LayerTreeHost&,
      const TransformPaintPropertyNode&);

 private:
  void SetupRootTransformNode();
  void SetupRootClipNode();
  void SetupRootEffectNode();
  void SetupRootScrollNode();

  // The type of operation the current cc effect node applies.
  enum CcEffectType {
    // The cc effect corresponds to a Blink effect node.
    kEffect = 0,
    // The cc effect is synthetic for a blink clip node that has to be
    // rasterized because the clip is non-trivial.
    kSyntheticForNonTrivialClip = 1 << 0,
    // The cc effect is synthetic to create a render surface that is
    // 2d-axis-aligned with a blink clip node that is non-2d-axis-aligned
    // in the the original render surface. Cc requires a rectangular clip to be
    // 2d-axis-aligned with the render surface to correctly apply the clip.
    // TODO(crbug.com/504464): This will be changed when we move render surface
    // decision logic into the cc compositor thread.
    kSyntheticFor2dAxisAlignment = 1 << 1
  };

  static bool SupportsShaderBasedRoundedCorner(const ClipPaintPropertyNode&,
                                               CcEffectType type);

  struct EffectState {
    // The cc effect node that has the corresponding drawing state to the
    // effect and clip state from the last
    // SwitchToEffectNodeWithSynthesizedClip.
    int effect_id;

    CcEffectType effect_type;

    // The effect state of the cc effect node. It's never nullptr.
    const EffectPaintPropertyNode* effect;

    // The clip state of the cc effect node. This value may be shallower than
    // the one passed into SwitchToEffectNodeWithSynthesizedClip because not
    // every clip needs to be synthesized as cc effect. Is set to output clip of
    // the effect if the type is kEffect, or set to the synthesized clip node.
    // It's never nullptr.
    const ClipPaintPropertyNode* clip;

    // Whether the transform space of this state may be 2d axis misaligned to
    // the containing render surface. As there may be new render surfaces
    // created between this state and the current known ancestor render surface
    // after this state is created, we must conservatively accumulate this flag
    // from the known render surface instead of checking if the combined
    // transform is 2d axis aligned, in case of:
    //  Effect1 (Current known render surface)
    //  Rotate(45deg)
    //  Effect2 (Not known now, but may become render surface later)
    //  Rotate(-45deg)
    //  Clip (Would be mistakenly treated as 2d axis aligned if we used
    //
    // It's lazily computed if it can't be trivially known when we create this
    // EffectState.
    enum {
      kAligned,
      kMisaligned,
      kUnknown,
    } may_be_2d_axis_misaligned_to_render_surface;

    // Whether this effect or an ancestor has a synthetic rounded clip between
    // self and the next render surface. This is used to force a render surface
    // for all ancestor synthetic rounded clips if a descendant is found.
    bool contained_by_non_render_surface_synthetic_rounded_clip;

    // The transform space of the state.
    const TransformPaintPropertyNode& Transform() const;
  };

  void CollectAnimationElementId(CompositorElementId);
  void BuildEffectNodesRecursively(const EffectPaintPropertyNode& next_effect);
  void ForceRenderSurfaceIfSyntheticRoundedCornerClip(EffectState& state);
  SkBlendMode SynthesizeCcEffectsForClipsIfNeeded(
      const ClipPaintPropertyNode& target_clip,
      SkBlendMode delegated_blend);
  void EmitClipMaskLayer();
  void CloseCcEffect();

  // For a given effect node, this returns the blend mode, clip property node,
  // and an int indicating cc clip node's id.
  std::tuple<SkBlendMode, const ClipPaintPropertyNode*, int>
  GetBlendModeAndOutputClipForEffect(const EffectPaintPropertyNode&);
  void PopulateCcEffectNode(cc::EffectNode&,
                            const EffectPaintPropertyNode&,
                            int output_clip_id,
                            SkBlendMode);

  bool IsCurrentCcEffectSynthetic() const { return current_.effect_type; }
  bool IsCurrentCcEffectSyntheticForNonTrivialClip() const {
    return current_.effect_type & CcEffectType::kSyntheticForNonTrivialClip;
  }

  bool EffectStateMayBe2dAxisMisalignedToRenderSurface(EffectState&,
                                                       size_t index);
  bool CurrentEffectMayBe2dAxisMisalignedToRenderSurface();
  CcEffectType SyntheticEffectType(const ClipPaintPropertyNode&);

  void SetCurrentEffectState(const cc::EffectNode&,
                             CcEffectType,
                             const EffectPaintPropertyNode&,
                             const ClipPaintPropertyNode&);
  void SetCurrentEffectRenderSurfaceReason(cc::RenderSurfaceReason);

  cc::TransformTree& GetTransformTree();
  cc::ClipTree& GetClipTree();
  cc::EffectTree& GetEffectTree();
  cc::ScrollTree& GetScrollTree();

  // Should only be called from EnsureCompositorTransformNode as part of
  // creating the associated scroll offset transform node.
  void CreateCompositorScrollNode(
      const ScrollPaintPropertyNode&,
      const cc::TransformNode& scroll_offset_translation);

  PropertyTreeManagerClient& client_;

  // Property trees which should be updated by the manager.
  cc::PropertyTrees& property_trees_;

  // The special layer which is the parent of every other layers.
  // This is where clip mask layers we generated for synthesized clips are
  // appended into.
  cc::Layer& root_layer_;

  LayerListBuilder& layer_list_builder_;

  int new_sequence_number_;

  // The current effect state. Virtually it's the top of the effect stack if
  // it and effect_stack_ are treated as a whole stack.
  EffectState current_;

  // This keep track of cc effect stack. Whenever a new cc effect is nested,
  // a new entry is pushed, and the entry will be popped when the effect closed.
  // Note: This is a "restore stack", i.e. the top element does not represent
  // the current state (which is in current_), but the state prior to most
  // recent push.
  Vector<EffectState> effect_stack_;

  // A set of synthetic clips masks which will be applied if a layer under them
  // is encountered which draws content (and thus necessitates the mask).
  HashSet<int> pending_synthetic_mask_layers_;

  DISALLOW_COPY_AND_ASSIGN(PropertyTreeManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PROPERTY_TREE_MANAGER_H_
