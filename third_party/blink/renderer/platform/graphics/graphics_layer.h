/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidator.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class PictureLayer;
}  // namespace cc

namespace blink {

class PaintController;
class RasterInvalidationTracking;
class RasterInvalidator;

typedef Vector<GraphicsLayer*, 64> GraphicsLayerVector;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.
class PLATFORM_EXPORT GraphicsLayer : public DisplayItemClient,
                                      public LayerAsJSONClient,
                                      private cc::ContentLayerClient {
  USING_FAST_MALLOC(GraphicsLayer);

 public:
  explicit GraphicsLayer(GraphicsLayerClient&);
  ~GraphicsLayer() override;

  GraphicsLayerClient& Client() const { return client_; }

  void SetCompositingReasons(CompositingReasons reasons) {
    compositing_reasons_ = reasons;
  }
  CompositingReasons GetCompositingReasons() const {
    return compositing_reasons_;
  }

  SquashingDisallowedReasons GetSquashingDisallowedReasons() const {
    return squashing_disallowed_reasons_;
  }
  void SetSquashingDisallowedReasons(SquashingDisallowedReasons reasons) {
    squashing_disallowed_reasons_ = reasons;
  }

  void SetOwnerNodeId(DOMNodeId id) { owner_node_id_ = id; }

  GraphicsLayer* Parent() const { return parent_; }
  void SetParent(GraphicsLayer*);  // Internal use only.

  const Vector<GraphicsLayer*>& Children() const { return children_; }
  // Returns true if the child list changed.
  bool SetChildren(const GraphicsLayerVector&);

  // Add child layers. If the child is already parented, it will be removed from
  // its old parent.
  void AddChild(GraphicsLayer*);

  void RemoveAllChildren();
  void RemoveFromParent();

  // The offset is the origin of the layoutObject minus the origin of the
  // graphics layer (so either zero or negative).
  IntSize OffsetFromLayoutObject() const { return offset_from_layout_object_; }
  void SetOffsetFromLayoutObject(const IntSize&);

  // The size of the layer.
  const gfx::Size& Size() const;
  void SetSize(const gfx::Size&);

  bool DrawsContent() const { return draws_content_; }
  void SetDrawsContent(bool);

  // False if no hit test data will be recorded onto this GraphicsLayer.
  // This is different from |DrawsContent| because hit test data are internal
  // to blink and are not copied to the cc::Layer's display list.
  bool PaintsHitTest() const { return paints_hit_test_; }
  void SetPaintsHitTest(bool);

  bool PaintsContentOrHitTest() const {
    return draws_content_ || paints_hit_test_;
  }

  bool ContentsAreVisible() const { return contents_visible_; }
  void SetContentsVisible(bool);

  void SetContentsLayerBackgroundColor(Color color);

  // Opaque means that we know the layer contents have no alpha.
  bool ContentsOpaque() const;
  void SetContentsOpaque(bool);
  void SetContentsOpaqueForText(bool);

  void SetHitTestable(bool);
  bool IsHitTestable() const { return hit_testable_; }

  // Some GraphicsLayers paint only the foreground or the background content
  GraphicsLayerPaintingPhase PaintingPhase() const { return painting_phase_; }
  void SetPaintingPhase(GraphicsLayerPaintingPhase);

  void SetNeedsDisplay();
  void SetContentsNeedsDisplay();

  // Set that the position/size of the contents (image or video).
  void SetContentsRect(const IntRect&);

  // If |prevent_contents_opaque_changes| is set to true, then calls to
  // SetContentsOpaque() will not be passed on to |contents_layer|. Use when
  // the client wants to have control of the opaqueness of |contents_layer|
  // independently of what outcome painting produces.
  void SetContentsToCcLayer(scoped_refptr<cc::Layer> contents_layer,
                            bool prevent_contents_opaque_changes);
  bool HasContentsLayer() const { return ContentsLayer(); }
  cc::Layer* ContentsLayer() const { return contents_layer_.get(); }

  const IntRect& ContentsRect() const { return contents_rect_; }

  // For hosting this GraphicsLayer in a native layer hierarchy.
  cc::PictureLayer& CcLayer() const { return *layer_; }

  void UpdateTrackingRasterInvalidations();
  void ResetTrackedRasterInvalidations();
  bool HasTrackedRasterInvalidations() const;
  RasterInvalidationTracking* GetRasterInvalidationTracking() const;
  void TrackRasterInvalidation(const DisplayItemClient&,
                               const IntRect&,
                               PaintInvalidationReason);

  IntRect InterestRect();
  void PaintRecursively(HashSet<const GraphicsLayer*>& repainted_layers);
  // Returns true if this layer is repainted.
  bool Paint();

  PaintController& GetPaintController() const;

  void SetElementId(const CompositorElementId&);

  // DisplayItemClient methods
  String DebugName() const final { return client_.DebugName(this); }
  DOMNodeId OwnerNodeId() const final { return owner_node_id_; }

  // LayerAsJSONClient implementation.
  void AppendAdditionalInfoAsJSON(LayerTreeFlags,
                                  const cc::Layer&,
                                  JSONObject&) const override;

  bool HasLayerState() const { return layer_state_.get(); }
  void SetLayerState(const PropertyTreeStateOrAlias&,
                     const IntPoint& layer_offset);
  const PropertyTreeStateOrAlias& GetPropertyTreeState() const {
    return layer_state_->state;
  }
  IntPoint GetOffsetFromTransformNode() const { return layer_state_->offset; }

  void SetContentsLayerState(const PropertyTreeStateOrAlias&,
                             const IntPoint& layer_offset);
  const PropertyTreeStateOrAlias& GetContentsPropertyTreeState() const {
    return contents_layer_state_ ? contents_layer_state_->state
                                 : GetPropertyTreeState();
  }
  IntPoint GetContentsOffsetFromTransformNode() const {
    return contents_layer_state_ ? contents_layer_state_->offset
                                 : GetOffsetFromTransformNode();
  }

  // Capture the last painted result into a PaintRecord. This GraphicsLayer
  // must DrawsContent. The result is never nullptr.
  sk_sp<PaintRecord> CapturePaintRecord() const;

  void SetNeedsCheckRasterInvalidation() {
    needs_check_raster_invalidation_ = true;
  }

  bool PaintWithoutCommitForTesting(
      const base::Optional<IntRect>& interest_rect = base::nullopt);

  bool ShouldCreateLayersAfterPaint() const {
    return should_create_layers_after_paint_;
  }

 protected:
  String DebugName(const cc::Layer*) const;

 private:
  friend class CompositedLayerMappingTest;
  friend class GraphicsLayerTest;

  // cc::ContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return InterestRect(); }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) final;
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const final;

  void UpdateShouldCreateLayersAfterPaint();

  // Returns true if PaintController::PaintArtifact() changed and needs commit.
  bool PaintWithoutCommit(const IntRect* interest_rect = nullptr);

  // Adds a child without calling NotifyChildListChange(), so that adding
  // children can be batched before updating.
  void AddChildInternal(GraphicsLayer*);

#if DCHECK_IS_ON()
  bool HasAncestor(GraphicsLayer*) const;
#endif

  // Helper functions used by settors to keep layer's the state consistent.
  void NotifyChildListChange();
  void UpdateLayerIsDrawable();
  void UpdateContentsLayerBounds();

  void SetContentsTo(scoped_refptr<cc::Layer>,
                     bool prevent_contents_opaque_changes);

  RasterInvalidator& EnsureRasterInvalidator();
  void SetNeedsDisplayInRect(const IntRect&);

  GraphicsLayerClient& client_;

  // Offset from the owning layoutObject
  IntSize offset_from_layout_object_;

  TransformationMatrix transform_;

  bool prevent_contents_opaque_changes_ : 1;
  bool draws_content_ : 1;
  bool paints_hit_test_ : 1;
  bool contents_visible_ : 1;
  bool hit_testable_ : 1;
  bool needs_check_raster_invalidation_ : 1;
  // True if the cc::Layers for this GraphicsLayer should be created after
  // paint (in PaintArtifactCompositor). This depends on the display item list
  // and is updated after CommitNewDisplayItems.
  bool should_create_layers_after_paint_ : 1;

  GraphicsLayerPaintingPhase painting_phase_;

  Vector<GraphicsLayer*> children_;
  GraphicsLayer* parent_;

  IntRect contents_rect_;

  scoped_refptr<cc::PictureLayer> layer_;
  scoped_refptr<cc::Layer> contents_layer_;

  SquashingDisallowedReasons squashing_disallowed_reasons_ =
      SquashingDisallowedReason::kNone;

  mutable std::unique_ptr<PaintController> paint_controller_;

  IntRect previous_interest_rect_;

  struct LayerState {
    PropertyTreeStateOrAlias state;
    IntPoint offset;
  };
  std::unique_ptr<LayerState> layer_state_;
  std::unique_ptr<LayerState> contents_layer_state_;

  std::unique_ptr<RasterInvalidator> raster_invalidator_;
  RasterInvalidator::RasterInvalidationFunction raster_invalidation_function_;

  DOMNodeId owner_node_id_ = kInvalidDOMNodeId;
  CompositingReasons compositing_reasons_ = CompositingReason::kNone;

  DISALLOW_COPY_AND_ASSIGN(GraphicsLayer);
};

// Iterates all graphics layers that should be seen by the compositor in
// pre-order. |GraphicsLayerType| matches |GraphicsLayer&| or
// |const GraphicsLayer&|.
template <typename GraphicsLayerType,
          typename GraphicsLayerFunction,
          typename ContentsLayerFunction>
void ForAllActiveGraphicsLayers(
    GraphicsLayerType& layer,
    const GraphicsLayerFunction& graphics_layer_function,
    const ContentsLayerFunction& contents_layer_function) {
  if (layer.Client().ShouldThrottleRendering() ||
      layer.Client().IsUnderSVGHiddenContainer()) {
    return;
  }

  if (layer.Client().PaintBlockedByDisplayLockIncludingAncestors()) {
    // If we skip the layer, then we need to ensure to notify the
    // display-lock, since we need to force recollect the layers when we commit.
    layer.Client().NotifyDisplayLockNeedsGraphicsLayerCollection();
    return;
  }

  DCHECK(layer.HasLayerState());

  if (layer.PaintsContentOrHitTest() || layer.IsHitTestable())
    graphics_layer_function(layer);

  if (auto* contents_layer = layer.ContentsLayer())
    contents_layer_function(layer, *contents_layer);

  for (auto* child : layer.Children()) {
    ForAllActiveGraphicsLayers(*child, graphics_layer_function,
                               contents_layer_function);
  }
}

template <typename GraphicsLayerType, typename Function>
void ForAllActiveGraphicsLayers(GraphicsLayerType& layer,
                                const Function& function) {
  ForAllActiveGraphicsLayers(layer, function,
                             [](GraphicsLayerType&, const cc::Layer&) {});
}

template <typename GraphicsLayerType, typename Function>
void ForAllPaintingGraphicsLayers(GraphicsLayerType& layer,
                                  const Function& function) {
  ForAllActiveGraphicsLayers(layer, [&function](GraphicsLayerType& layer) {
    if (layer.PaintsContentOrHitTest())
      function(layer);
  });
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_
