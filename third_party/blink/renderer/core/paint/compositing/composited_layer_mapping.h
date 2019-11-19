/*
 * Copyright (C) 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITED_LAYER_MAPPING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITED_LAYER_MAPPING_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_updater.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painting_info.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayerCompositor;

// A GraphicsLayerPaintInfo contains all the info needed to paint a partial
// subtree of Layers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
  DISALLOW_NEW();
  PaintLayer* paint_layer;

  PhysicalRect composited_bounds;

  // The clip rect to apply, in the local coordinate space of the squashed
  // layer, when painting it.
  ClipRect local_clip_rect_for_squashed_layer;
  PaintLayer* local_clip_rect_root;
  PhysicalOffset offset_from_clip_rect_root;

  // Offset describing where this squashed Layer paints into the shared
  // GraphicsLayer backing.
  IntSize offset_from_layout_object;
  bool offset_from_layout_object_set;

  GraphicsLayerPaintInfo()
      : paint_layer(nullptr), offset_from_layout_object_set(false) {}
};

enum GraphicsLayerUpdateScope {
  kGraphicsLayerUpdateNone,
  kGraphicsLayerUpdateLocal,
  kGraphicsLayerUpdateSubtree,
};

// CompositedLayerMapping keeps track of how PaintLayers correspond to
// GraphicsLayers of the composited layer tree. Each instance of
// CompositedLayerMapping manages a small cluster of GraphicsLayers and the
// references to which Layers and paint phases contribute to each GraphicsLayer.
//
// - If a PaintLayer is composited,
//   - if it paints into its own backings (GraphicsLayers), it owns a
//     CompositedLayerMapping (PaintLayer::compositedLayerMapping()) to keep
//     track the backings;
//   - if it paints into grouped backing (i.e. it's squashed), it has a pointer
//     (PaintLayer::groupedMapping()) to the CompositedLayerMapping into which
//     the PaintLayer is squashed;
// - Otherwise the PaintLayer doesn't own or directly reference any
//   CompositedLayerMapping.
class CORE_EXPORT CompositedLayerMapping final : public GraphicsLayerClient {
  USING_FAST_MALLOC(CompositedLayerMapping);

 public:
  explicit CompositedLayerMapping(PaintLayer&);
  ~CompositedLayerMapping() override;

  PaintLayer& OwningLayer() const { return owning_layer_; }

  bool UpdateGraphicsLayerConfiguration(
      const PaintLayer* compositing_container);
  void UpdateGraphicsLayerGeometry(
      const PaintLayer* compositing_container,
      const PaintLayer* compositing_stacking_context,
      Vector<PaintLayer*>& layers_needing_paint_invalidation,
      GraphicsLayerUpdater::UpdateContext& update_context);

  // Update whether background paints onto scrolling contents layer.
  // Returns (through the reference params) what invalidations are needed.
  void UpdateBackgroundPaintsOntoScrollingContentsLayer(
      bool& invalidate_graphics_layer,
      bool& invalidate_scrolling_contents_layer);

  // Update whether layer needs blending.
  void UpdateContentsOpaque();

  void UpdateRasterizationPolicy();

  GraphicsLayer* MainGraphicsLayer() const { return graphics_layer_.get(); }

  GraphicsLayer* ForegroundLayer() const { return foreground_layer_.get(); }

  GraphicsLayer* DecorationOutlineLayer() const {
    return decoration_outline_layer_.get();
  }

  bool HasScrollingLayer() const { return scrolling_layer_.get(); }
  GraphicsLayer* ScrollingLayer() const { return scrolling_layer_.get(); }
  GraphicsLayer* ScrollingContentsLayer() const {
    return scrolling_contents_layer_.get();
  }

  bool HasMaskLayer() const { return mask_layer_.get(); }
  GraphicsLayer* MaskLayer() const { return mask_layer_.get(); }

  GraphicsLayer* ParentForSublayers() const;
  GraphicsLayer* ChildForSuperlayers() const;
  void SetSublayers(const GraphicsLayerVector&);

  GraphicsLayer* SquashingContainmentLayer() const {
    return squashing_containment_layer_.get();
  }
  GraphicsLayer* SquashingLayer() const { return squashing_layer_.get(); }
  const IntSize& SquashingLayerOffsetFromLayoutObject() const {
    return squashing_layer_offset_from_layout_object_;
  }

  void SetSquashingContentsNeedDisplay();
  void SetContentsNeedDisplay();

  // Let all DrawsContent GraphicsLayers check raster invalidations after
  // a no-change paint.
  void SetNeedsCheckRasterInvalidation();

  // Notification from the layoutObject that its content changed.
  void ContentChanged(ContentChangeType);

  PhysicalRect CompositedBounds() const { return composited_bounds_; }

  void PositionOverflowControlsLayers();

  // Returns true if the assignment actually changed the assigned squashing
  // layer.
  bool UpdateSquashingLayerAssignment(PaintLayer* squashed_layer,
                                      wtf_size_t next_squashed_layer_index);
  void RemoveLayerFromSquashingGraphicsLayer(const PaintLayer*);
#if DCHECK_IS_ON()
  bool VerifyLayerInSquashingVector(const PaintLayer*);
#endif

  void FinishAccumulatingSquashingLayers(
      wtf_size_t next_squashed_layer_index,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateElementId();

  // GraphicsLayerClient interface
  void InvalidateTargetElementForTesting() override;
  IntRect ComputeInterestRect(
      const GraphicsLayer*,
      const IntRect& previous_interest_rect) const override;
  LayoutSize SubpixelAccumulation() const final;
  bool NeedsRepaint(const GraphicsLayer&) const override;
  void PaintContents(const GraphicsLayer*,
                     GraphicsContext&,
                     GraphicsLayerPaintingPhase,
                     const IntRect& interest_rect) const override;
  bool ShouldThrottleRendering() const override;
  bool IsUnderSVGHiddenContainer() const override;
  bool IsTrackingRasterInvalidations() const override;
  void GraphicsLayersDidChange() override;
  bool PaintBlockedByDisplayLockIncludingAncestors(
      DisplayLockContextLifecycleTarget) const override;
  void NotifyDisplayLockNeedsGraphicsLayerCollection() override;

#if DCHECK_IS_ON()
  void VerifyNotPainting() override;
#endif

  PhysicalRect ContentsBox() const;

  GraphicsLayer* LayerForHorizontalScrollbar() const {
    return layer_for_horizontal_scrollbar_.get();
  }
  GraphicsLayer* LayerForVerticalScrollbar() const {
    return layer_for_vertical_scrollbar_.get();
  }
  GraphicsLayer* LayerForScrollCorner() const {
    return layer_for_scroll_corner_.get();
  }

  // Returns true if the overflow controls cannot be positioned within this
  // CLM's internal hierarchy without incorrectly stacking under some
  // scrolling content. If this returns true, these controls must be
  // repositioned in the graphics layer tree to ensure that they stack above
  // scrolling content.
  bool NeedsToReparentOverflowControls() const;

  // Removes the overflow controls host layer from its parent and positions it
  // so that it can be inserted as a sibling to this CLM without changing
  // position.
  GraphicsLayer* DetachLayerForOverflowControls();

  // We may similarly need to reattach the layer for outlines and decorations.
  GraphicsLayer* DetachLayerForDecorationOutline();

  void SetBlendMode(BlendMode);

  bool NeedsGraphicsLayerUpdate() {
    return pending_update_scope_ > kGraphicsLayerUpdateNone;
  }
  void SetNeedsGraphicsLayerUpdate(GraphicsLayerUpdateScope scope) {
    pending_update_scope_ = std::max(
        static_cast<GraphicsLayerUpdateScope>(pending_update_scope_), scope);
  }
  void ClearNeedsGraphicsLayerUpdate() {
    pending_update_scope_ = kGraphicsLayerUpdateNone;
  }

  GraphicsLayerUpdater::UpdateType UpdateTypeForChildren(
      GraphicsLayerUpdater::UpdateType) const;

#if DCHECK_IS_ON()
  void AssertNeedsToUpdateGraphicsLayerBitsCleared() {
    DCHECK_EQ(pending_update_scope_,
              static_cast<unsigned>(kGraphicsLayerUpdateNone));
  }
#endif

  String DebugName(const GraphicsLayer*) const override;

  const ScrollableArea* GetScrollableAreaForTesting(
      const GraphicsLayer*) const override;

  PhysicalOffset ContentOffsetInCompositingLayer() const;

  // If there is a squashed layer painting into this CLM that is an ancestor of
  // the given LayoutObject, return it. Otherwise return nullptr.
  const GraphicsLayerPaintInfo* ContainingSquashedLayer(
      const LayoutObject*,
      unsigned max_squashed_layer_index);

  // Returns whether an adjustment happend.
  bool AdjustForCompositedScrolling(const GraphicsLayer*,
                                    IntSize& offset) const;

  // Returns true for layers with scrollable overflow which have a background
  // that can be painted into the composited scrolling contents layer (i.e.
  // the background can scroll with the content). When the background is also
  // opaque this allows us to composite the scroller even on low DPI as we can
  // draw with subpixel anti-aliasing.
  bool BackgroundPaintsOntoScrollingContentsLayer() const {
    return background_paints_onto_scrolling_contents_layer_;
  }

  // Returns true if the background paints onto the main graphics layer.
  // In some situations, we may paint background on both the main graphics layer
  // and the scrolling contents layer.
  bool BackgroundPaintsOntoGraphicsLayer() const {
    return background_paints_onto_graphics_layer_;
  }

  bool DrawsBackgroundOntoContentLayer() const {
    return draws_background_onto_content_layer_;
  }

 private:
  IntRect RecomputeInterestRect(const GraphicsLayer*) const;
  static bool InterestRectChangedEnoughToRepaint(
      const IntRect& previous_interest_rect,
      const IntRect& new_interest_rect,
      const IntSize& layer_size);

  static const GraphicsLayerPaintInfo* ContainingSquashedLayer(
      const LayoutObject*,
      const Vector<GraphicsLayerPaintInfo>& layers,
      unsigned max_squashed_layer_index);

  // Paints the scrollbar part associated with the given graphics layer into the
  // given context.
  void PaintScrollableArea(const GraphicsLayer*,
                           GraphicsContext&,
                           const IntRect& interest_rect) const;
  // Returns whether the given layer is part of the scrollable area, if any,
  // associated with this mapping.
  bool IsScrollableAreaLayer(const GraphicsLayer*) const;

  // Helper methods to updateGraphicsLayerGeometry:
  void ComputeGraphicsLayerParentLocation(
      const PaintLayer* compositing_container,
      IntPoint& graphics_layer_parent_location);
  void UpdateSquashingLayerGeometry(
      const IntPoint& graphics_layer_parent_location,
      const PaintLayer* compositing_container,
      const IntPoint& snapped_offset_from_composited_ancestor,
      Vector<GraphicsLayerPaintInfo>& layers,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateMainGraphicsLayerGeometry(
      const IntRect& relative_compositing_bounds,
      const IntRect& local_compositing_bounds,
      const IntPoint& graphics_layer_parent_location,
      GraphicsLayerUpdater::UpdateContext& update_context);
  void UpdateOverflowControlsHostLayerGeometry(
      const PaintLayer* compositing_stacking_context,
      const PaintLayer* compositing_container,
      IntPoint graphics_layer_parent_location);
  void UpdateChildTransformLayerGeometry();
  void UpdateMaskLayerGeometry();
  void UpdateForegroundLayerGeometry();
  void UpdateDecorationOutlineLayerGeometry(
      const IntSize& relative_compositing_bounds_size);
  void UpdateScrollingLayerGeometry(const IntRect& local_compositing_bounds);

  void CreatePrimaryGraphicsLayer();
  void DestroyGraphicsLayers();

  std::unique_ptr<GraphicsLayer> CreateGraphicsLayer(
      CompositingReasons,
      SquashingDisallowedReasons = SquashingDisallowedReason::kNone);
  bool ToggleScrollbarLayerIfNeeded(std::unique_ptr<GraphicsLayer>&,
                                    bool needs_layer,
                                    CompositingReasons);

  LayoutBoxModelObject& GetLayoutObject() const {
    return owning_layer_.GetLayoutObject();
  }
  PaintLayerCompositor* Compositor() const {
    return owning_layer_.Compositor();
  }

  void UpdateInternalHierarchy();
  void UpdatePaintingPhases();
  bool UpdateOverflowControlsLayers(bool needs_horizontal_scrollbar_layer,
                                    bool needs_vertical_scrollbar_layer,
                                    bool needs_scroll_corner_layer);
  bool UpdateForegroundLayer(bool needs_foreground_layer);
  bool UpdateDecorationOutlineLayer(bool needs_decoration_outline_layer);
  bool UpdateMaskLayer(bool needs_mask_layer);
  bool RequiresHorizontalScrollbarLayer() const;
  bool RequiresVerticalScrollbarLayer() const;
  bool RequiresScrollCornerLayer() const;
  bool UpdateScrollingLayers(bool scrolling_layers);
  bool UpdateSquashingLayers(bool needs_squashing_layers);
  void UpdateDrawsContentAndPaintsHitTest();
  void UpdateCompositedBounds();

  // Also sets subpixelAccumulation on the layer.
  void ComputeBoundsOfOwningLayer(
      const PaintLayer* composited_ancestor,
      IntRect& local_compositing_bounds,
      IntRect& compositing_bounds_relative_to_composited_ancestor,
      PhysicalOffset& offset_from_composited_ancestor,
      IntPoint& snapped_offset_from_composited_ancestor);

  GraphicsLayerPaintingPhase PaintingPhaseForPrimaryLayer() const;

  // Result is transform origin in pixels.
  FloatPoint3D ComputeTransformOrigin(const IntRect& border_box) const;

  void UpdateTransform(const ComputedStyle&);

  bool PaintsChildren() const;

  // Returns true if this layer has content that needs to be displayed by
  // painting into the backing store.
  bool ContainsPaintedContent() const;
  // Returns true if the Layer just contains an image that we can composite
  // directly.
  bool IsDirectlyCompositedImage() const;
  void UpdateImageContents();

  Color LayoutObjectBackgroundColor() const;
  void UpdateBackgroundColor();
  void UpdateContentsRect();
  void UpdateAfterPartResize();
  void UpdateCompositingReasons();

  static bool HasVisibleNonCompositingDescendant(PaintLayer* parent);

  void DoPaintTask(const GraphicsLayerPaintInfo&,
                   const GraphicsLayer&,
                   PaintLayerFlags,
                   GraphicsContext&,
                   const IntRect& clip) const;

  // Computes the background clip rect for the given squashed layer, up to any
  // containing layer that is squashed into the same squashing layer and
  // contains this squashed layer's clipping ancestor.  The clip rect is
  // returned in the coordinate space of the given squashed layer.  If there is
  // no such containing layer, returns the infinite rect.
  static void LocalClipRectForSquashedLayer(
      const PaintLayer& reference_layer,
      const Vector<GraphicsLayerPaintInfo>& layers,
      GraphicsLayerPaintInfo&);

  // Clear the groupedMapping entry on the layer at the given index, only if
  // that layer does not appear earlier in the set of layers for this object.
  bool InvalidateLayerIfNoPrecedingEntry(wtf_size_t);

  // Main GraphicsLayer of the CLM for the iframe's content document.
  GraphicsLayer* FrameContentsGraphicsLayer() const;

  PaintLayer& owning_layer_;

  // The hierarchy of layers that is maintained by the CompositedLayerMapping
  // looks like this:
  //
  //    + graphics_layer_
  //      + (scrolling_layer_ + scrolling_contents_layer_) [OPTIONAL]
  //      | + overflow_controls_host_layer_ [OPTIONAL]
  //      |   + layer_for_vertical_scrollbar_ [OPTIONAL]
  //      |   + layer_for_horizontal_scrollbar_ [OPTIONAL]
  //      |   + layer_for_scroll_corner_ [OPTIONAL]
  //      + decoration_outline_layer_ [OPTIONAL]
  // The overflow controls may need to be repositioned in the graphics layer
  // tree by the RLC to ensure that they stack above scrolling content.

  std::unique_ptr<GraphicsLayer> graphics_layer_;

  // Only used if the layer is using composited scrolling.
  std::unique_ptr<GraphicsLayer> scrolling_layer_;

  // Only used if the layer is using composited scrolling.
  std::unique_ptr<GraphicsLayer> scrolling_contents_layer_;

  // This layer is also added to the hierarchy by the RLB, but in a different
  // way than the layers above. It's added to graphics_layer_ as its mask layer
  // (naturally) if we have a mask, and isn't part of the typical hierarchy (it
  // has no children).
  // Only used if we have a mask.
  std::unique_ptr<GraphicsLayer> mask_layer_;

  // There is one other (optional) layer whose painting is managed by the
  // CompositedLayerMapping, but whose position in the hierarchy is maintained
  // by the PaintLayerCompositor. This is the foreground layer. The foreground
  // layer exists if we have composited descendants with negative z-order. We
  // need the extra layer in this case because the layer needs to draw both
  // below (for the background, say) and above (for the normal flow content,
  // say) the negative z-order descendants and this is impossible with a single
  // layer. The RLC handles inserting foreground_layer_ in the correct position
  // in our descendant list for us (right after the neg z-order dsecendants).
  // Only used in cases where we need to draw the foreground separately.
  std::unique_ptr<GraphicsLayer> foreground_layer_;

  std::unique_ptr<GraphicsLayer> layer_for_horizontal_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_vertical_scrollbar_;
  std::unique_ptr<GraphicsLayer> layer_for_scroll_corner_;

  // This layer contains the scrollbar and scroll corner layers and clips them
  // to the border box bounds of our LayoutObject. It is usually added to
  // graphics_layer_, but may be reparented by GraphicsLayerTreeBuilder to
  // ensure that scrollbars appear above scrolling content.
  std::unique_ptr<GraphicsLayer> overflow_controls_host_layer_;

  // DecorationLayer which paints outline.
  std::unique_ptr<GraphicsLayer> decoration_outline_layer_;

  // A squashing CLM has the following structure:
  // squashing_containment_layer_
  //   + graphics_layer_
  //   + squashing_layer_
  //
  // Stacking children of a squashed layer receive graphics layers that are
  // parented to the composited ancestor of the squashed layer (i.e. nearest
  // enclosing composited layer that is not
  // squashed).

  // Only used if any squashed layers exist, this contains the squashed layers
  // as siblings to the rest of the GraphicsLayer tree chunk.
  std::unique_ptr<GraphicsLayer> squashing_containment_layer_;

  // Only used if any squashed layers exist, this is the backing that squashed
  // layers paint into.
  std::unique_ptr<GraphicsLayer> squashing_layer_;
  Vector<GraphicsLayerPaintInfo> squashed_layers_;
  IntSize squashing_layer_offset_from_layout_object_;

  PhysicalRect composited_bounds_;

  unsigned pending_update_scope_ : 2;
  unsigned is_main_frame_layout_view_layer_ : 1;

  unsigned scrolling_contents_are_empty_ : 1;

  // Keep track of whether the background is painted onto the scrolling contents
  // layer for invalidations.
  unsigned background_paints_onto_scrolling_contents_layer_ : 1;

  // Solid color border boxes may be painted into both the scrolling contents
  // layer and the graphics layer because the scrolling contents layer is
  // clipped by the padding box.
  unsigned background_paints_onto_graphics_layer_ : 1;

  bool draws_background_onto_content_layer_;

  friend class CompositedLayerMappingTest;
  DISALLOW_COPY_AND_ASSIGN(CompositedLayerMapping);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITED_LAYER_MAPPING_H_
