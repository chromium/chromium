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
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace gfx {
class Vector2dF;
}

namespace blink {

class ContentLayerClientImpl;
class JSONObject;
class PaintArtifact;
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
                   const FloatRoundedRect& rrect,
                   scoped_refptr<const RefCountedPath> path);

  cc::Layer* Layer() { return layer_.get(); }
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
  gfx::Vector2dF layer_origin_;
  SkRRect local_rrect_ = SkRRect::MakeEmpty();
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

  struct Settings {
    bool prefer_compositing_to_lcd_text = false;
  };

  // Updates the layer tree to match the provided paint artifact.
  //
  // Populates |animation_element_ids| with the CompositorElementId of all
  // animations for which we saw a paint chunk and created a layer.
  void Update(scoped_refptr<const PaintArtifact>,
              const ViewportProperties& viewport_properties,
              const Settings& settings);

  bool DirectlyUpdateCompositedOpacityValue(const EffectPaintPropertyNode&);
  bool DirectlyUpdateScrollOffsetTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdateTransform(const TransformPaintPropertyNode&);
  bool DirectlyUpdatePageScaleTransform(const TransformPaintPropertyNode&);

  // The root layer of the tree managed by this object.
  cc::Layer* RootLayer() const { return root_layer_.get(); }

  // Returns extra information recorded during unit tests.
  // While not part of the normal output of this class, this provides a simple
  // way of locating the layers of interest, since there are still a slew of
  // placeholder layers required.
  struct PLATFORM_EXPORT ExtraDataForTesting {
    cc::Layer* ScrollHitTestWebLayerAt(unsigned index);

    Vector<scoped_refptr<cc::Layer>> content_layers;
    Vector<scoped_refptr<cc::Layer>> synthesized_clip_layers;
    Vector<scoped_refptr<cc::Layer>> scroll_hit_test_layers;
    Vector<scoped_refptr<cc::Layer>> scrollbar_layers;
  };
  void EnableExtraDataForTesting();
  ExtraDataForTesting* GetExtraDataForTesting() const {
    return extra_data_for_testing_.get();
  }

  void SetTracksRasterInvalidations(bool);

  // Called when the local frame view that owns this compositor is
  // going to be removed from its frame.
  void WillBeRemovedFromFrame();

  std::unique_ptr<JSONObject> GetLayersAsJSON(
      LayerTreeFlags,
      const PaintArtifact* = nullptr) const;

#if DCHECK_IS_ON()
  void ShowDebugData();
#endif

  const Vector<std::unique_ptr<ContentLayerClientImpl>>&
  ContentLayerClientsForTesting() const {
    return content_layer_clients_;
  }

  // Update the cc::Layer's touch action region from the touch action rects of
  // the paint chunks.
  static void UpdateTouchActionRects(cc::Layer*,
                                     const gfx::Vector2dF& layer_offset,
                                     const PropertyTreeState& layer_state,
                                     const PaintChunkSubset& paint_chunks);

  // Update the cc::Layer's non-fast scrollable region from the non-fast regions
  // in the paint chunks.
  static void UpdateNonFastScrollableRegions(
      cc::Layer*,
      const gfx::Vector2dF& layer_offset,
      const PropertyTreeState& layer_state,
      const PaintChunkSubset& paint_chunks);

  void SetNeedsUpdate() { needs_update_ = true; }
  bool NeedsUpdate() const { return needs_update_; }
  void ClearNeedsUpdateForTesting() { needs_update_ = false; }

  // Returns true if a property tree node associated with |element_id| exists
  // on any of the PropertyTrees constructed by |Update|.
  bool HasComposited(CompositorElementId element_id) const;

  void SetLayerDebugInfoEnabled(bool);

  // TODO(wangxianzhu): Make this private and refactor when removing
  // pre-CompositeAfterPaint.
  static void UpdateLayerDebugInfo(cc::Layer* layer,
                                   const PaintChunk::Id&,
                                   CompositingReasons,
                                   RasterInvalidationTracking*);

 private:
  // A pending layer is a collection of paint chunks that will end up in
  // the same cc::Layer.
  struct PLATFORM_EXPORT PendingLayer {
    PendingLayer(const PaintChunk& first_paint_chunk,
                 wtf_size_t first_chunk_index,
                 bool requires_own_layer);
    // Merge another pending layer after this one, appending all its paint
    // chunks after chunks in this layer, with appropriate space conversion
    // applied. The merged layer must have a property tree state that's deeper
    // than this layer, i.e. can "upcast" to this layer's state.
    void Merge(const PendingLayer& guest);
    // |guest_state| is for cases that we want to check if we can merge |guest|
    // if it has |guest_state| (which may be different from its current state).
    bool CanMerge(const PendingLayer& guest,
                  const PropertyTreeState& guest_state) const;
    // Mutate this layer's property tree state to a more general (shallower)
    // state, thus the name "upcast". The concrete effect of this is to
    // "decomposite" some of the properties, so that fewer properties will be
    // applied by the compositor, and more properties will be applied internally
    // to the chunks as Skia commands.
    void Upcast(const PropertyTreeState&);

    const PaintChunk& FirstPaintChunk(const PaintArtifact&) const;

    // The rects are in the space of property_tree_state.
    FloatRect bounds;
    FloatRect rect_known_to_be_opaque;
    Vector<wtf_size_t> paint_chunk_indices;
    PropertyTreeState property_tree_state;
    FloatPoint offset_of_decomposited_transforms;
    bool requires_own_layer;
  };

  void DecompositeTransforms(const PaintArtifact&);

  // Collects the PaintChunks into groups which will end up in the same
  // cc layer. This is the entry point of the layerization algorithm.
  void CollectPendingLayers(const PaintArtifact&, const Settings& settings);

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
  void LayerizeGroup(const PaintArtifact&,
                     const Settings& settings,
                     const EffectPaintPropertyNode&,
                     Vector<PaintChunk>::const_iterator& chunk_cursor);
  static bool MightOverlap(const PendingLayer&, const PendingLayer&);
  bool DecompositeEffect(const EffectPaintPropertyNode& unaliased_parent_effect,
                         size_t first_layer_in_parent_group_index,
                         const EffectPaintPropertyNode& unaliased_effect,
                         size_t layer_index);

  // Builds a leaf layer that represents a single paint chunk.
  scoped_refptr<cc::Layer> CompositedLayerForPendingLayer(
      scoped_refptr<const PaintArtifact>,
      const PendingLayer&,
      Vector<std::unique_ptr<ContentLayerClientImpl>>&
          new_content_layer_clients,
      Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers,
      Vector<scoped_refptr<cc::Layer>>& new_scrollbar_layers);

  bool PropertyTreeStateChanged(const PropertyTreeState&) const;

  const TransformPaintPropertyNode& ScrollOffsetTranslationForLayer(
      const PaintArtifact&,
      const PendingLayer&);

  // If the pending layer is a special scroll hit test layer, return the
  // associated hit test information.
  const HitTestData::ScrollHitTest* ScrollHitTestForLayer(const PaintArtifact&,
                                                          const PendingLayer&);

  // Finds an existing or creates a new scroll hit test layer for the pending
  // layer, returning nullptr if the layer is not a scroll hit test layer.
  scoped_refptr<cc::Layer> ScrollHitTestLayerForPendingLayer(
      const PaintArtifact&,
      const PendingLayer&);

  // Finds an existing or creates a new scrollbar layer for the pending layer,
  // returning nullptr if the layer is not a scrollbar layer.
  scoped_refptr<cc::Layer> ScrollbarLayerForPendingLayer(const PaintArtifact&,
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
      bool needs_layer,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) final;

  static void UpdateRenderSurfaceForEffects(
      cc::EffectTree&,
      const cc::LayerList&,
      const Vector<const EffectPaintPropertyNode*>&);

  cc::PropertyTrees* GetPropertyTreesForDirectUpdate();

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
  Vector<scoped_refptr<cc::Layer>> scrollbar_layers_;

  Vector<PendingLayer, 0> pending_layers_;

  bool extra_data_for_testing_enabled_ = false;
  std::unique_ptr<ExtraDataForTesting> extra_data_for_testing_;

  friend class StubChromeClientForCAP;
  friend class PaintArtifactCompositorTest;

  DISALLOW_COPY_AND_ASSIGN(PaintArtifactCompositor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
