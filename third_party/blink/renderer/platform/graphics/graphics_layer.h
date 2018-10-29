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
#include "base/memory/weak_ptr.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer_client.h"
#include "cc/layers/layer_sticky_position_constraint.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class Layer;
class PictureImageLayer;
class PictureLayer;
}

namespace blink {

class CompositorFilterOperations;
class Image;
class LinkHighlight;
class PaintController;
class RasterInvalidationTracking;
class RasterInvalidator;

typedef Vector<GraphicsLayer*, 64> GraphicsLayerVector;

// GraphicsLayer is an abstraction for a rendering surface with backing store,
// which may have associated transformation and animations.
class PLATFORM_EXPORT GraphicsLayer : public cc::LayerClient,
                                      public DisplayItemClient,
                                      private cc::ContentLayerClient {
  USING_FAST_MALLOC(GraphicsLayer);

 public:
  static std::unique_ptr<GraphicsLayer> Create(GraphicsLayerClient&);

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
  void SetOwnerNodeId(int id) { owner_node_id_ = id; }

  GraphicsLayer* Parent() const { return parent_; }
  void SetParent(GraphicsLayer*);  // Internal use only.

  const Vector<GraphicsLayer*>& Children() const { return children_; }
  // Returns true if the child list changed.
  bool SetChildren(const GraphicsLayerVector&);

  // Add child layers. If the child is already parented, it will be removed from
  // its old parent.
  void AddChild(GraphicsLayer*);
  void AddChildBelow(GraphicsLayer*, GraphicsLayer* sibling);

  void RemoveAllChildren();
  void RemoveFromParent();

  GraphicsLayer* MaskLayer() const { return mask_layer_; }
  void SetMaskLayer(GraphicsLayer*, bool is_rounded_corner_mask);

  GraphicsLayer* ContentsClippingMaskLayer() const {
    return contents_clipping_mask_layer_;
  }
  void SetContentsClippingMaskLayer(GraphicsLayer*);

  // The offset is the origin of the layoutObject minus the origin of the
  // graphics layer (so either zero or negative).
  IntSize OffsetFromLayoutObject() const {
    return offset_from_layout_object_;
  }
  void SetOffsetFromLayoutObject(const IntSize&);
  LayoutSize OffsetFromLayoutObjectWithSubpixelAccumulation() const;

  // The position of the layer (the location of its top-left corner in its
  // parent).
  const gfx::PointF& GetPosition() const;
  void SetPosition(const gfx::PointF&);

  const gfx::Point3F& TransformOrigin() const;
  void SetTransformOrigin(const gfx::Point3F&);

  // The size of the layer.
  const gfx::Size& Size() const;
  void SetSize(const gfx::Size&);

  const TransformationMatrix& Transform() const { return transform_; }
  void SetTransform(const TransformationMatrix&);

  bool ShouldFlattenTransform() const;
  void SetShouldFlattenTransform(bool);
  void SetRenderingContext(int id);

  bool MasksToBounds() const;
  void SetMasksToBounds(bool);

  bool DrawsContent() const { return draws_content_; }
  void SetDrawsContent(bool);

  bool ContentsAreVisible() const { return contents_visible_; }
  void SetContentsVisible(bool);

  void SetScrollParent(cc::Layer*);
  void SetClipParent(cc::Layer*);

  // For special cases, e.g. drawing missing tiles on Android.
  // The compositor should never paint this color in normal cases because the
  // Layer will paint the background by itself.
  RGBA32 BackgroundColor() const;
  void SetBackgroundColor(RGBA32);

  // Opaque means that we know the layer contents have no alpha.
  bool ContentsOpaque() const;
  void SetContentsOpaque(bool);

  bool BackfaceVisibility() const;
  void SetBackfaceVisibility(bool visible);

  float Opacity() const;
  void SetOpacity(float);

  BlendMode GetBlendMode() const;
  void SetBlendMode(BlendMode);
  bool IsRootForIsolatedGroup() const;
  void SetIsRootForIsolatedGroup(bool);

  void SetHitTestableWithoutDrawsContent(bool);
  bool GetHitTestableWithoutDrawsContent() const {
    return hit_testable_without_draws_content_;
  }

  void SetFilters(CompositorFilterOperations);
  void SetBackdropFilters(CompositorFilterOperations);

  void SetStickyPositionConstraint(const cc::LayerStickyPositionConstraint&);

  void SetFilterQuality(SkFilterQuality);

  // Some GraphicsLayers paint only the foreground or the background content
  GraphicsLayerPaintingPhase PaintingPhase() const { return painting_phase_; }
  void SetPaintingPhase(GraphicsLayerPaintingPhase);

  void SetNeedsDisplay();
  void SetContentsNeedsDisplay();

  // Set that the position/size of the contents (image or video).
  void SetContentsRect(const IntRect&);

  // Layer contents
  void SetContentsToImage(
      Image*,
      Image::ImageDecodingMode decode_mode,
      RespectImageOrientationEnum = kDoNotRespectImageOrientation);
  // If |prevent_contents_opaque_changes| is set to true, then calls to
  // SetContentsOpaque() will not be passed on to the |layer|. Use when
  // the client wants to have control of the opaqueness of the contents
  // |layer| independently of what outcome painting produces.
  void SetContentsToCcLayer(cc::Layer* layer,
                            bool prevent_contents_opaque_changes) {
    SetContentsTo(layer, prevent_contents_opaque_changes);
  }
  bool HasContentsLayer() const { return contents_layer_; }
  cc::Layer* ContentsLayer() const { return contents_layer_; }

  // For hosting this GraphicsLayer in a native layer hierarchy.
  cc::PictureLayer* CcLayer() const;

  int PaintCount() const { return paint_count_; }

  // Return a string with a human readable form of the layer tree. If debug is
  // true, pointers for the layers and timing data will be included in the
  // returned string.
  String GetLayerTreeAsTextForTesting(LayerTreeFlags = kLayerTreeNormal) const;

  void UpdateTrackingRasterInvalidations();
  void ResetTrackedRasterInvalidations();
  bool HasTrackedRasterInvalidations() const;
  RasterInvalidationTracking* GetRasterInvalidationTracking() const;
  void TrackRasterInvalidation(const DisplayItemClient&,
                               const IntRect&,
                               PaintInvalidationReason);

  void AddLinkHighlight(LinkHighlight*);
  void RemoveLinkHighlight(LinkHighlight*);
  const Vector<LinkHighlight*>& GetLinkHighlights() const {
    return link_highlights_;
  }

  int GetRenderingContext3D() const { return rendering_context3d_; }

  cc::PictureLayer* ContentLayer() const { return layer_.get(); }

  static void RegisterContentsLayer(cc::Layer*);
  static void UnregisterContentsLayer(cc::Layer*);

  IntRect InterestRect();
  void PaintRecursively();
  // Returns true if this layer is repainted.
  bool Paint(const IntRect* interest_rect,
             GraphicsContext::DisabledMode = GraphicsContext::kNothingDisabled);

  // cc::LayerClient implementation.
  std::unique_ptr<base::trace_event::TracedValue> TakeDebugInfo(
      cc::Layer*) override;
  void DidChangeScrollbarsHiddenIfOverlay(bool) override;

  PaintController& GetPaintController() const;

  void SetElementId(const CompositorElementId&);
  CompositorElementId GetElementId() const;

  // DisplayItemClient methods
  String DebugName() const final { return client_.DebugName(this); }
  LayoutRect VisualRect() const override;

  void SetHasWillChangeTransformHint(bool);

  void SetOverscrollBehavior(const cc::OverscrollBehavior&);

  void SetSnapContainerData(base::Optional<cc::SnapContainerData>);

  void SetIsResizedByBrowserControls(bool);
  void SetIsContainerForFixedPositionLayers(bool);

  bool HasLayerState() const { return layer_state_.get(); }
  void SetLayerState(const PropertyTreeState&, const IntPoint& layer_offset);
  const PropertyTreeState& GetPropertyTreeState() const {
    return layer_state_->state;
  }
  IntPoint GetOffsetFromTransformNode() const { return layer_state_->offset; }

  void SetContentsLayerState(const PropertyTreeState&,
                             const IntPoint& layer_offset);
  const PropertyTreeState& GetContentsPropertyTreeState() const {
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

  bool HasScrollParent() const { return has_scroll_parent_; }
  bool HasClipParent() const { return has_clip_parent_; }

 protected:
  String DebugName(cc::Layer*) const;

  explicit GraphicsLayer(GraphicsLayerClient&);

  friend class CompositedLayerMappingTest;
  friend class PaintControllerPaintTestBase;
  friend class GraphicsLayerTest;

 private:
  // cc::ContentLayerClient implementation.
  gfx::Rect PaintableRegion() final { return InterestRect(); }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) final;
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const final;

  void PaintRecursivelyInternal(Vector<GraphicsLayer*>& repainted_layers);

  // Returns true if PaintController::paintArtifact() changed and needs commit.
  bool PaintWithoutCommit(
      const IntRect* interest_rect,
      GraphicsContext::DisabledMode = GraphicsContext::kNothingDisabled);

  // Adds a child without calling updateChildList(), so that adding children
  // can be batched before updating.
  void AddChildInternal(GraphicsLayer*);

#if DCHECK_IS_ON()
  bool HasAncestor(GraphicsLayer*) const;
#endif

  void IncrementPaintCount() { ++paint_count_; }

  // Helper functions used by settors to keep layer's the state consistent.
  void UpdateChildList();
  void UpdateLayerIsDrawable();
  void UpdateContentsRect();

  void SetContentsTo(cc::Layer*, bool prevent_contents_opaque_changes);
  void SetupContentsLayer(cc::Layer*);
  void ClearContentsLayerIfUnregistered();
  cc::Layer* ContentsLayerIfRegistered();
  void SetContentsLayer(cc::Layer*);

  RasterInvalidator& EnsureRasterInvalidator();
  void SetNeedsDisplayInRect(const IntRect&);

  FloatSize VisualRectSubpixelOffset() const;

  GraphicsLayerClient& client_;

  // Offset from the owning layoutObject
  IntSize offset_from_layout_object_;

  TransformationMatrix transform_;

  bool prevent_contents_opaque_changes_ : 1;
  bool draws_content_ : 1;
  bool contents_visible_ : 1;
  bool hit_testable_without_draws_content_ : 1;
  bool needs_check_raster_invalidation_ : 1;

  bool has_scroll_parent_ : 1;
  bool has_clip_parent_ : 1;

  bool painted_ : 1;

  GraphicsLayerPaintingPhase painting_phase_;

  Vector<GraphicsLayer*> children_;
  GraphicsLayer* parent_;

  // Reference to mask layer. We don't own this.
  GraphicsLayer* mask_layer_;
  // Reference to clipping mask layer. We don't own this.
  GraphicsLayer* contents_clipping_mask_layer_;

  IntRect contents_rect_;

  int paint_count_;

  scoped_refptr<cc::PictureLayer> layer_;
  scoped_refptr<cc::PictureImageLayer> image_layer_;
  IntSize image_size_;
  cc::Layer* contents_layer_;
  // We don't have ownership of contents_layer_, but we do want to know if a
  // given layer is the same as our current layer in SetContentsTo(). Since
  // |contents_layer_| may be deleted at this point, we stash an ID away when we
  // know |contents_layer_| is alive and use that for comparisons from that
  // point on.
  int contents_layer_id_;

  Vector<LinkHighlight*> link_highlights_;

  int rendering_context3d_;

  CompositingReasons compositing_reasons_ = CompositingReason::kNone;
  SquashingDisallowedReasons squashing_disallowed_reasons_ =
      SquashingDisallowedReason::kNone;
  int owner_node_id_ = 0;

  mutable std::unique_ptr<PaintController> paint_controller_;

  IntRect previous_interest_rect_;

  struct LayerState {
    PropertyTreeState state;
    IntPoint offset;
  };
  std::unique_ptr<LayerState> layer_state_;
  std::unique_ptr<LayerState> contents_layer_state_;

  std::unique_ptr<RasterInvalidator> raster_invalidator_;

  base::WeakPtrFactory<GraphicsLayer> weak_ptr_factory_;

  FRIEND_TEST_ALL_PREFIXES(CompositingLayerPropertyUpdaterTest, MaskLayerState);

  DISALLOW_COPY_AND_ASSIGN(GraphicsLayer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GRAPHICS_LAYER_H_
