// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/graphics/compositing/property_tree_manager.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
struct ElementId;
class Layer;
}

namespace gfx {
class Vector2dF;
class ScrollOffset;
}

namespace blink {

class ContentLayerClientImpl;
class JSONObject;
class PaintArtifact;
class SynthesizedClip;
struct PaintChunk;

class LayerListBuilder {
 public:
  void Add(scoped_refptr<cc::Layer>);
  cc::LayerList Finalize();

 private:
  // The list becomes invalid once |Finalize| is called.
  bool list_valid_ = true;
  cc::LayerList list_;
};

// Responsible for managing compositing in terms of a PaintArtifact.
//
// Owns a subtree of the compositor layer tree, and updates it in response to
// changes in the paint artifact.
//
// PaintArtifactCompositor is the successor to PaintLayerCompositor, reflecting
// the new home of compositing decisions after paint in Slimming Paint v2.
class PLATFORM_EXPORT PaintArtifactCompositor final
    : private PropertyTreeManagerClient {
  USING_FAST_MALLOC(PaintArtifactCompositor);

 public:
  ~PaintArtifactCompositor();

  static std::unique_ptr<PaintArtifactCompositor> Create(
      base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                   const cc::ElementId&)> scroll_callback) {
    return base::WrapUnique(
        new PaintArtifactCompositor(std::move(scroll_callback)));
  }

  // Updates the layer tree to match the provided paint artifact.
  //
  // Populates |composited_element_ids| with the CompositorElementId of all
  // animations for which we saw a paint chunk and created a layer.
  void Update(scoped_refptr<const PaintArtifact>,
              CompositorElementIdSet& composited_element_ids,
              TransformPaintPropertyNode* viewport_scale_node);

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
  };
  void EnableExtraDataForTesting();
  ExtraDataForTesting* GetExtraDataForTesting() const {
    return extra_data_for_testing_.get();
  }

  void SetTracksRasterInvalidations(bool);

  // Called when the local frame view that owns this compositor is
  // going to be removed from its frame.
  void WillBeRemovedFromFrame();

  std::unique_ptr<JSONObject> LayersAsJSON(LayerTreeFlags) const;

#if DCHECK_IS_ON()
  void ShowDebugData();
#endif

 private:
  // A pending layer is a collection of paint chunks that will end up in
  // the same cc::Layer.
  struct PLATFORM_EXPORT PendingLayer {
    PendingLayer(const PaintChunk& first_paint_chunk,
                 size_t first_chunk_index,
                 bool requires_own_layer);
    // Merge another pending layer after this one, appending all its paint
    // chunks after chunks in this layer, with appropriate space conversion
    // applied. The merged layer must have a property tree state that's deeper
    // than this layer, i.e. can "upcast" to this layer's state.
    void Merge(const PendingLayer& guest);
    bool CanMerge(const PendingLayer& guest) const;
    // Mutate this layer's property tree state to a more general (shallower)
    // state, thus the name "upcast". The concrete effect of this is to
    // "decomposite" some of the properties, so that fewer properties will be
    // applied by the compositor, and more properties will be applied internally
    // to the chunks as Skia commands.
    void Upcast(const PropertyTreeState&);

    FloatRect bounds;
    Vector<size_t> paint_chunk_indices;
    FloatRect rect_known_to_be_opaque;
    PropertyTreeState property_tree_state;
    bool requires_own_layer;
  };

  PaintArtifactCompositor(
      base::RepeatingCallback<void(const gfx::ScrollOffset&,
                                   const cc::ElementId&)> scroll_callback);

  // Collects the PaintChunks into groups which will end up in the same
  // cc layer. This is the entry point of the layerization algorithm.
  void CollectPendingLayers(const PaintArtifact&,
                            Vector<PendingLayer>& pending_layers);
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
  static void LayerizeGroup(const PaintArtifact&,
                            Vector<PendingLayer>& pending_layers,
                            const EffectPaintPropertyNode&,
                            Vector<PaintChunk>::const_iterator& chunk_cursor);
  static bool MightOverlap(const PendingLayer&, const PendingLayer&);
  static bool CanDecompositeEffect(const EffectPaintPropertyNode*,
                                   const PendingLayer&);

  // Builds a leaf layer that represents a single paint chunk.
  // Note: cc::Layer API assumes the layer bounds start at (0, 0), but the
  // bounding box of a paint chunk does not necessarily start at (0, 0) (and
  // could even be negative). Internally the generated layer translates the
  // paint chunk to align the bounding box to (0, 0) and return the actual
  // origin of the paint chunk in the |layerOffset| outparam.
  scoped_refptr<cc::Layer> CompositedLayerForPendingLayer(
      scoped_refptr<const PaintArtifact>,
      const PendingLayer&,
      gfx::Vector2dF& layer_offset,
      Vector<std::unique_ptr<ContentLayerClientImpl>>&
          new_content_layer_clients,
      Vector<scoped_refptr<cc::Layer>>& new_scroll_hit_test_layers);

  const TransformPaintPropertyNode& ScrollTranslationForPendingLayer(
      const PaintArtifact&,
      const PendingLayer&);

  // If the pending layer is a special scroll hit test layer, return the
  // associated scroll offset translation node.
  const TransformPaintPropertyNode* ScrollTranslationForScrollHitTestLayer(
      const PaintArtifact&,
      const PendingLayer&);

  // Finds an existing or creates a new scroll hit test layer for the pending
  // layer, returning nullptr if the layer is not a scroll hit test layer.
  scoped_refptr<cc::Layer> ScrollHitTestLayerForPendingLayer(
      const PaintArtifact&,
      const PendingLayer&,
      gfx::Vector2dF& layer_offset);

  // Finds a client among the current vector of clients that matches the paint
  // chunk's id, or otherwise allocates a new one.
  std::unique_ptr<ContentLayerClientImpl> ClientForPaintChunk(
      const PaintChunk&);

  cc::Layer* CreateOrReuseSynthesizedClipLayer(
      const ClipPaintPropertyNode*,
      CompositorElementId& mask_isolation_id,
      CompositorElementId& mask_effect_id) final;

  // Provides a callback for notifying blink of composited scrolling.
  base::RepeatingCallback<void(const gfx::ScrollOffset&, const cc::ElementId&)>
      scroll_callback_;

  bool tracks_raster_invalidations_;

  scoped_refptr<cc::Layer> root_layer_;
  Vector<std::unique_ptr<ContentLayerClientImpl>> content_layer_clients_;
  struct SynthesizedClipEntry {
    const ClipPaintPropertyNode* key;
    std::unique_ptr<SynthesizedClip> synthesized_clip;
    bool in_use;
  };
  std::vector<SynthesizedClipEntry> synthesized_clip_cache_;

  Vector<scoped_refptr<cc::Layer>> scroll_hit_test_layers_;

  bool extra_data_for_testing_enabled_ = false;
  std::unique_ptr<ExtraDataForTesting> extra_data_for_testing_;

  friend class StubChromeClientForSPv2;
  friend class PaintArtifactCompositorTest;

  DISALLOW_COPY_AND_ASSIGN(PaintArtifactCompositor);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COMPOSITING_PAINT_ARTIFACT_COMPOSITOR_H_
