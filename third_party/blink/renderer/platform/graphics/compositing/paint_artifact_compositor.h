// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/property_tree.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#endif

namespace cc {
class ScrollbarLayerBase;
}

namespace gfx {
class Vector2dF;
}

namespace blink {

class ContentLayerClientImpl;
class JSONObject;
class PaintArtifact;
class PropertyTreeManager;
class SynthesizedClip;
struct PaintChunk;

using CompositorScrollCallbacks = cc::ScrollCallbacks;

class LayerListBuilder {
 public:
  void Add(scoped_refptr<cc::Layer>);
  cc::LayerList Finalize();

 private:
  // The list becomes invalid once |Finalize| is called.
  bool list_valid_ = true;
  cc::LayerList list_;
#if DCHECK_IS_ON()
  HashSet<int> layer_ids_;
#endif
};

// This class maintains unique stable cc effect IDs (and optionally a persistent
// mask layer) for reuse across compositing cycles. The mask layer paints a
// rounded rect, which is an updatable parameter of the class. The caller is
// responsible for inserting the mask layer into layer list and associating with
// property nodes. The mask layer may be omitted if the caller determines it is
// not necessary (e.g. because there is no content to mask).
//
// The typical application of the mask layer is to create an isolating effect
// node to paint the clipped contents, and at the end draw the mask layer with
// a kDstIn blend effect. This is why two stable cc effect IDs are provided.
// Even if the mask layer is not present, it's important for the isolation
// effect node to be stable, to minimize render surface damage.
class SynthesizedClip : private cc::ContentLayerClient {
 public:
  SynthesizedClip() : layer_(nullptr) {
    mask_isolation_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
    mask_effect_id_ =
        CompositorElementIdFromUniqueObjectId(NewUniqueObjectId());
  }
  ~SynthesizedClip() override {
    if (layer_)
      layer_->ClearClient();
  }

  void UpdateLayer(bool needs_layer,
                   const ClipPaintPropertyNode&,
                   const TransformPaintPropertyNode&);

  cc::PictureLayer* Layer() { return layer_.get(); }
  CompositorElementId GetMaskIsolationId() const { return mask_isolation_id_; }
  CompositorElementId GetMaskEffectId() const { return mask_effect_id_; }

 private:
  // ContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return gfx::Rect(layer_->bounds()); }
  bool FillsBoundsCompletely() const final { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const final { return 0; }

  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) final;

 private:
  scoped_refptr<cc::PictureLayer> layer_;
  GeometryMapper::Translation2DOrMatrix translation_2d_or_matrix_;
  bool rrect_is_local_ = false;
  SkRRect rrect_;
  scoped_refptr<const RefCountedPath> path_;
  CompositorElementId mask_isolation_id_;
  CompositorElementId mask_effect_id_;
};

// Responsible for managing compositing in terms of a PaintArtifact.
//
// Owns a subtree of the compositor layer tree, and updates it in response to
// changes in the paint artifact.
//
// PaintArtifactCompositor is the successor to PaintLayerCompositor, reflecting
// the new home of compositing decisions after paint with CompositeAfterPaint.
class PLATFORM_EXPORT PaintArtifactCompositor final
    : private PropertyTreeManagerClient {
  USING_FAST_MALLOC(PaintArtifactCompositor);

 public:
  PaintArtifactCompositor(
      base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks);
  ~PaintArtifactCompositor();

  struct ViewportProperties {
    const TransformPaintPropertyNode* overscroll_elasticity_transform = nullptr;
    const TransformPaintPropertyNode* page_scale = nullptr;
    const TransformPaintPropertyNode* inner_scroll_translation = nullptr;
    const ClipPaintPropertyNode* outer_clip = nullptr;
    const TransformPaintPropertyNode* outer_scroll_translation = nullptr;
  };

  // Updates the layer tree to match the provided paint artifact.
  //
  // |scroll_translation_nodes| is the complete set of scroll nodes, including
  // noncomposited nodes, and is used for Scroll Unification to generate scroll
  // nodes for noncomposited scrollers to complete the compositor's scroll
  // property tree.
  void Update(
      scoped_refptr<const PaintArtifact>,
      const ViewportProperties& viewport_properties,
      const Vector<const TransformPaintPropertyNode*>& scroll_translation_nodes,
      const HashSet<const GraphicsLayer*>& repainted_layers);

  bool DirectlyUpdateCompositedOpacityValue(const EffectPaintPropertyNode&);
  bool DirectlyUpdateScrollOffsetTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdateTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdatePageScaleTransform(const TransformPaintPropertyNode&);

  // Directly sets cc::ScrollTree::current_scroll_offset. This doesn't affect
  // cc::TransformNode::scroll_offset (which will be synched with blink
  // transform node in DirectlyUpdateScrollOffsetTransform() or Update()).
  bool DirectlySetScrollOffset(CompositorElementId,
                               const FloatPoint& scroll_offset);

  // The root layer of the tree managed by this object.
  cc::Layer* RootLayer() const { return root_layer_.get(); }

  void SetTracksRasterInvalidations(bool);

  // Called when the local frame view that owns this compositor is
  // going to be removed from its frame.
  void WillBeRemovedFromFrame();

  std::unique_ptr<JSONArray> GetPendingLayersAsJSON() const;

  std::unique_ptr<JSONObject> GetLayersAsJSON(LayerTreeFlags) const;

#if DCHECK_IS_ON()
  void ShowDebugData();
#endif

  const Vector<std::unique_ptr<ContentLayerClientImpl>>&
  ContentLayerClientsForTesting() const {
    return content_layer_clients_;
  }

  // Update the cc::Layer's touch action region from the touch action rects of
  // the paint chunks.
  static void UpdateTouchActionRects(cc::Layer&,
                                     const gfx::Vector2dF& layer_offset,
                                     const PropertyTreeState& layer_state,
                                     const PaintChunkSubset& paint_chunks);

  // Update the cc::Layer's non-fast scrollable region from the non-fast regions
  // in the paint chunks.
  static void UpdateNonFastScrollableRegions(
      cc::Layer&,
      const gfx::Vector2dF& layer_offset,
      const PropertyTreeState& layer_state,
      const PaintChunkSubset& paint_chunks,
      PropertyTreeManager* = nullptr);

  void SetNeedsUpdate() { needs_update_ = true; }
  bool NeedsUpdate() const { return needs_update_; }
  void ClearNeedsUpdateForTesting() { needs_update_ = false; }

  // Returns true if a property tree node associated with |element_id| exists
  // on any of the PropertyTrees constructed by |Update|.
  bool HasComposited(CompositorElementId element_id) const;

  void SetLayerDebugInfoEnabled(bool);

  // TODO(wangxianzhu): Make this private and refactor when removing
  // pre-CompositeAfterPaint.
  static void UpdateLayerDebugInfo(cc::Layer& layer,
                                   const PaintChunk::Id&,
                                   CompositingReasons,
                                   RasterInvalidationTracking*);

  Vector<cc::Layer*> SynthesizedClipLayersForTesting() const;

 private:
  // A pending layer is a collection of paint chunks that will end up in
  // the same cc::Layer.
  struct PLATFORM_EXPORT PendingLayer {
    enum CompositingType {
      kScrollHitTestLayer,
      kGraphicsLayerWrapper,
      kForeignLayer,
      kScrollbarLayer,
      kOverlap,
      kOther,
    };

    PendingLayer(scoped_refptr<const PaintArtifact>,
                 const PaintChunk& first_paint_chunk,
                 wtf_size_t first_chunk_index,
                 CompositingType compositng_type = kOther);

    // Merges |guest| into |this| if it can, by appending chunks of |guest|
    // after chunks of |this|, with appropriate space conversion applied to
    // both layers from their original property tree states to |merged_state|.
    // Returns whether the merge is successful.
    bool Merge(const PendingLayer& guest);

    // Returns true if |guest| can be merged into |this|, and sets the output
    // parameters with the property tree state and bounds of the merged layer.
    // |guest_state| is for cases that we want to check if we can merge |guest|
    // if it has |guest_state| in the future (which may be different from its
    // current state).
    bool CanMerge(const PendingLayer& guest,
                  const PropertyTreeState& guest_state,
                  PropertyTreeState* merged_state = nullptr,
                  FloatRect* merged_bounds = nullptr) const;

    // Mutate this layer's property tree state to a more general (shallower)
    // state, thus the name "upcast". The concrete effect of this is to
    // "decomposite" some of the properties, so that fewer properties will be
    // applied by the compositor, and more properties will be applied internally
    // to the chunks as Skia commands.
    void Upcast(const PropertyTreeState&);

    const PaintChunk& FirstPaintChunk() const;
    const DisplayItem& FirstDisplayItem() const;

    // Returns the largest rect known to be opaque given two opaque rects.
    static FloatRect UniteRectsKnownToBeOpaque(const FloatRect&,
                                               const FloatRect&);
    FloatRect MapRectKnownToBeOpaque(const PropertyTreeState&) const;

    std::unique_ptr<JSONObject> ToJSON() const;

    FloatRect VisualRectForOverlapTesting() const;

    bool MayDrawContent() const;

    bool RequiresOwnLayer() const {
      return compositing_type != kOverlap && compositing_type != kOther;
    }

    // The rects are in the space of property_tree_state.
    FloatRect bounds;
    FloatRect rect_known_to_be_opaque;
    scoped_refptr<const PaintArtifact> paint_artifact;
    // Paint chunk indices from |paint_artifact.PaintChunks()|.
    Vector<wtf_size_t> paint_chunk_indices;
    PropertyTreeState property_tree_state;
    FloatPoint offset_of_decomposited_transforms;
    CompositingType compositing_type;
  };

  void UpdateRepaintedLayerProperties(
      const HashSet<const GraphicsLayer*>& repainted_layers) const;

  void DecompositeTransforms(const PaintArtifact&);

  // Collects the PaintChunks into groups which will end up in the same
  // cc layer. This is the entry point of the layerization algorithm.
  void CollectPendingLayers(scoped_refptr<const PaintArtifact>);

  // This is the internal recursion of collectPendingLayers. This function
  // loops over the list of paint chunks, scoped by an isolated group
  // (i.e. effect node). Inside of the loop, chunks are tested for overlap
  // and merge compatibility. Subgroups are handled by recursion, and will
  // be tested for "decompositing" upon return.
  // Merge compatibility means consecutive chunks may be layerized into the
  // same backing (i.e. merged) if their property states don't cross
  // direct-compositing boundary.
  // Non-consecutive chunks that are nevertheless compatible may still be
  // merged, if reordering of the chunks won't affect the ultimate result.
  // This is determined by overlap testing such that chunks can be safely
  // reordered if their effective bounds in screen space can't overlap.
  // The recursion only tests merge & overlap for chunks scoped by the same
  // group. This is where "decompositing" came in. Upon returning from a
  // recursion, the layerization of the subgroup may be tested for merge &
  // overlap with other chunks in the parent group, if grouping requirement
  // can be satisfied (and the effect node has no direct reason).
  void LayerizeGroup(scoped_refptr<const PaintArtifact>,
                     const EffectPaintPropertyNode&,
                     Vector<PaintChunk>::const_iterator& chunk_cursor);
  static bool MightOverlap(const PendingLayer&, const PendingLayer&);
  bool DecompositeEffect(const EffectPaintPropertyNode& parent_effect,
                         wtf_size_t first_layer_in_parent_group_index,
                         const EffectPaintPropertyNode& effect,
                         wtf_size_t layer_index);

  // Builds a leaf layer that represents a single paint chunk.
  scoped_refptr<cc::Layer> CompositedLayerForPendingLayer(
      const PendingLayer&,
      Vector<std::unique_ptr<ContentLayerClientImpl>>&
          new_content_layer_clients,
      Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers,
      Vector<scoped_refptr<cc::ScrollbarLayerBase>>& new_scrollbar_layers,
      const HashSet<const GraphicsLayer*>& repainted_layers);

  bool PropertyTreeStateChanged(const PropertyTreeState&) const;

  const TransformPaintPropertyNode& NearestScrollTranslationForLayer(
      const PendingLayer&);

  // If the pending layer has scroll hit test data, return the associated
  // scroll translation node.
  const TransformPaintPropertyNode* ScrollTranslationForLayer(
      const PendingLayer&);

  // Returns the cc::Layer if the pending layer contains a foreign layer or a
  // wrapper of a GraphicsLayer. If it's the latter and the graphics layer has
  // been repainted, also updates the layer properties.
  scoped_refptr<cc::Layer> WrappedCcLayerForPendingLayer(
      const PendingLayer&,
      const HashSet<const GraphicsLayer*>& repainted_layers);

  // Finds an existing or creates a new scroll hit test layer for the pending
  // layer, returning nullptr if the layer is not a scroll hit test layer.
  scoped_refptr<cc::Layer> ScrollHitTestLayerForPendingLayer(
      const PendingLayer&);

  // Finds an existing or creates a new scrollbar layer for the pending layer,
  // returning nullptr if the layer is not a scrollbar layer.
  scoped_refptr<cc::ScrollbarLayerBase> ScrollbarLayerForPendingLayer(
      const PendingLayer&);

  // Finds a client among the current vector of clients that matches the paint
  // chunk's id, or otherwise allocates a new one.
  std::unique_ptr<ContentLayerClientImpl> ClientForPaintChunk(
      const PaintChunk&);

  // if |needs_layer| is false, no cc::Layer is created, |mask_effect_id| is
  // not set, and the Layer() method on the returned SynthesizedClip returns
  // nullptr.
  // However, |mask_isolation_id| is always set.
  SynthesizedClip& CreateOrReuseSynthesizedClipLayer(
      const ClipPaintPropertyNode&,
      const TransformPaintPropertyNode&,
      bool needs_layer,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) final;

  static void UpdateRenderSurfaceForEffects(
      cc::EffectTree&,
      const cc::LayerList&,
      const Vector<const EffectPaintPropertyNode*>&);

  bool CanDirectlyUpdateProperties() const;

  CompositingReasons GetCompositingReasons(
      const PendingLayer& layer,
      const PendingLayer* previous_layer) const;

  // For notifying blink of composited scrolling.
  base::WeakPtr<CompositorScrollCallbacks> scroll_callbacks_;

  bool tracks_raster_invalidations_ = false;
  bool needs_update_ = true;
  bool layer_debug_info_enabled_ = false;

  scoped_refptr<cc::Layer> root_layer_;
  Vector<std::unique_ptr<ContentLayerClientImpl>> content_layer_clients_;
  struct SynthesizedClipEntry {
    const ClipPaintPropertyNode* key;
    std::unique_ptr<SynthesizedClip> synthesized_clip;
    bool in_use;
  };
  Vector<SynthesizedClipEntry> synthesized_clip_cache_;

  Vector<scoped_refptr<cc::Layer>> scroll_hit_test_layers_;
  Vector<scoped_refptr<cc::ScrollbarLayerBase>> scrollbar_layers_;

  Vector<PendingLayer, 0> pending_layers_;

  friend class StubChromeClientForCAP;
  friend class PaintArtifactCompositorTest;

  DISALLOW_COPY_AND_ASSIGN(PaintArtifactCompositor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
