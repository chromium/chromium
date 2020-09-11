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
  CompositedLayerMapping(const CompositedLayerMapping&) = delete;
  CompositedLayerMapping& operator=(const CompositedLayerMapping&) = delete;
  ~CompositedLayerMapping() override;

  PaintLayer& OwningLayer() const { return owning_layer_; }

  bool UpdateGraphicsLayerConfiguration(
      const PaintLayer* compositing_container);
  void UpdateGraphicsLayerGeometry(
      const PaintLayer* compositing_container,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);

  // Update whether layer needs blending.
  void UpdateContentsOpaque();

  GraphicsLayer* MainGraphicsLayer() const { return graphics_layer_.get(); }

  GraphicsLayer* ForegroundLayer() const { return foreground_layer_.get(); }

  GraphicsLayer* DecorationOutlineLayer() const {
    return decoration_outline_layer_.get();
  }

  GraphicsLayer* ScrollingContentsLayer() const {
    return scrolling_contents_layer_.get();
  }

  GraphicsLayer* MaskLayer() const { return mask_layer_.get(); }

  GraphicsLayer* ParentForSublayers() const;
  void SetSublayers(GraphicsLayerVector);

  // Returns the GraphicsLayer that |layer| is squashed into, which may be
  // NonScrollingSquashingLayer or ScrollingContentsLayer.
  GraphicsLayer* SquashingLayer(const PaintLayer& squashed_layer) const;

  GraphicsLayer* NonScrollingSquashingLayer() const {
    return non_scrolling_squashing_layer_.get();
  }
  const IntSize& NonScrollingSquashingLayerOffsetFromLayoutObject() const {
    return non_scrolling_squashing_layer_offset_from_layout_object_;
  }

  void SetAllLayersNeedDisplay();

  // Let all DrawsContent GraphicsLayers check raster invalidations after
  // a no-change paint.
  void SetNeedsCheckRasterInvalidation();

  // Notification from the layoutObject that its content changed.
  void ContentChanged(ContentChangeType);

  PhysicalRect CompositedBounds() const { return composited_bounds_; }

  void PositionOverflowControlsLayers();

  bool MayBeSquashedIntoScrollingContents(const PaintLayer& layer) const {
    return layer.AncestorScrollingLayer() == &owning_layer_;
  }

  // Returns true if the assignment actually changed the assigned squashing
  // layer.
  bool UpdateSquashingLayerAssignment(
      PaintLayer& squashed_layer,
      wtf_size_t next_non_scrolling_squashed_layer_index,
      wtf_size_t next_squashed_layer_in_scrolling_contents_index);
  void RemoveLayerFromSquashingGraphicsLayer(const PaintLayer&);
#if DCHECK_IS_ON()
  void AssertInSquashedLayersVector(const PaintLayer&) const;
#endif

  void FinishAccumulatingSquashingLayers(
      wtf_size_t new_non_scrolling_squashed_layer_count,
      wtf_size_t new_squashed_layer_in_scrolling_contents_count,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);

  void UpdateElementId();

  // GraphicsLayerClient interface
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
  bool IsSVGRoot() const override;
  bool IsTrackingRasterInvalidations() const override;
  void GraphicsLayersDidChange() override;
  bool PaintBlockedByDisplayLockIncludingAncestors() const override;
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

  // Move overflow control layers from its parent into the vector.
  // Returns the number of layers moved.
  wtf_size_t MoveOverflowControlLayersInto(GraphicsLayerVector&,
                                           wtf_size_t position);

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
  const GraphicsLayerPaintInfo* ContainingSquashedLayerInSquashingLayer(
      const LayoutObject*,
      unsigned max_squashed_layer_index) const;

  // Returns whether an adjustment happend.
  bool AdjustForCompositedScrolling(const GraphicsLayer*,
                                    IntSize& offset) const;

  bool DrawsBackgroundOntoContentLayer() const {
    return draws_background_onto_content_layer_;
  }

 private:
  // Returns true for layers with scrollable overflow which have a background
  // that can be painted into the composited scrolling contents layer (i.e.
  // the background can scroll with the content). When the background is also
  // opaque this allows us to composite the scroller even on low DPI as we can
  // draw with subpixel anti-aliasing.
  bool BackgroundPaintsOntoScrollingContentsLayer() const {
    return GetLayoutObject().GetBackgroundPaintLocation() &
           kBackgroundPaintInScrollingContents;
  }

  // Returns true if the background paints onto the main graphics layer.
  // In some situations, we may paint background on both the main graphics layer
  // and the scrolling contents layer.
  bool BackgroundPaintsOntoGraphicsLayer() const {
    return GetLayoutObject().GetBackgroundPaintLocation() &
           kBackgroundPaintInGraphicsLayer;
  }

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

  // Returns whether the given layer is a repaint needed part of the scrollable
  // area, if any, associated with this mapping.
  bool IsScrollableAreaLayerWhichNeedsRepaint(const GraphicsLayer*) const;

  // Helper methods to updateGraphicsLayerGeometry:
  void ComputeGraphicsLayerParentLocation(
      const PaintLayer* compositing_container,
      IntPoint& graphics_layer_parent_location);
  void UpdateSquashingLayerGeometry(
      const PaintLayer* compositing_container,
      const IntPoint& snapped_offset_from_composited_ancestor,
      Vector<GraphicsLayerPaintInfo>& layers,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateMainGraphicsLayerGeometry(const IntRect& local_compositing_bounds);
  void UpdateMaskLayerGeometry();
  void UpdateForegroundLayerGeometry();
  void UpdateDecorationOutlineLayerGeometry(
      const IntSize& relative_compositing_bounds_size);
  void UpdateScrollingContentsLayerGeometry(
      Vector<PaintLayer*>& layers_needing_paint_invalidation);

  void CreatePrimaryGraphicsLayer();

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
  bool UpdateScrollingContentsLayer(bool needs_scrolling_contents_layer);
  bool UpdateSquashingLayers(bool needs_squashing_layers);
  void UpdateDrawsContentAndPaintsHitTest();
  void UpdateCompositedBounds();
  void UpdateGraphicsLayerContentsOpaque(bool should_check_children);

  // Also sets subpixelAccumulation on the layer.
  void ComputeBoundsOfOwningLayer(
      const PaintLayer* composited_ancestor,
      IntRect& local_compositing_bounds,
      IntPoint& snapped_offset_from_composited_ancestor);

  GraphicsLayerPaintingPhase PaintingPhaseForPrimaryLayer() const;

  bool PaintsChildren() const;

  // Returns true if this layer has content that needs to be displayed by
  // painting into the backing store.
  bool ContainsPaintedContent() const;

  void UpdateContentsRect();
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
  static void UpdateLocalClipRectForSquashedLayer(
      const PaintLayer& reference_layer,
      const Vector<GraphicsLayerPaintInfo>& layers,
      GraphicsLayerPaintInfo&);

  bool UpdateSquashingLayerAssignmentInternal(
      Vector<GraphicsLayerPaintInfo>& squashed_layers,
      PaintLayer& squashed_layer,
      wtf_size_t next_squashed_layer_index);
  void RemoveSquashedLayers(Vector<GraphicsLayerPaintInfo>& squashed_layers);

  void SetContentsNeedDisplay();

  PaintLayer& owning_layer_;

  // The hierarchy of layers that is maintained by the CompositedLayerMapping
  // looks like this:
  //
  //    + graphics_layer_
  //      + layer_for_vertical_scrollbar_ [OPTIONAL][*]
  //      + layer_for_horizontal_scrollbar_ [OPTIONAL][*]
  //      + layer_for_scroll_corner_ [OPTIONAL][*]
  //      + contents layers (or contents layers under scrolling_contents_layer_)
  //      + decoration_outline_layer_ [OPTIONAL]
  //      + mask_layer_ [ OPTIONAL ]
  //      + non_scrolling_squashing_layer_ [ OPTIONAL ]
  //
  // [*] Overlay overflow controls may be placed above
  //     scrolling_contents_layer_, or repositioned in the graphics layer tree
  //     to ensure that they stack above scrolling content.
  //
  // Contents layers are directly under |graphics_layer_|, or under
  // |scrolling_contents_layer_| when the layer is using composited scrolling.
  // If owning_layer_ is a stacking context, contents layers include:
  //   - negative z-index children
  //   - foreground_layer_
  //   - normal flow and positive z-index children
  // If owning_layer_ is not a stacking context, contents layers are normal
  // flow children.

  std::unique_ptr<GraphicsLayer> graphics_layer_;

  // Only used if the layer is using composited scrolling.
  std::unique_ptr<GraphicsLayer> scrolling_contents_layer_;
  IntSize previous_scroll_container_size_;

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

  // DecorationLayer which paints outline.
  std::unique_ptr<GraphicsLayer> decoration_outline_layer_;

  // Only used when |non_scrolling_squashed_layers_| is not empty. This is
  // the backing that |non_scrolling_squashed_layers_| paint into.
  std::unique_ptr<GraphicsLayer> non_scrolling_squashing_layer_;
  IntSize non_scrolling_squashing_layer_offset_from_layout_object_;

  // Layers that are squashed into |non_scrolling_squashing_layer_|.
  Vector<GraphicsLayerPaintInfo> non_scrolling_squashed_layers_;

  // Layers that are squashed into |scrolling_contents_layer_|. This is used
  // when |owning_layer_| is scrollable but is not a stacking context, and
  // there are scrolling stacked children that can be squashed into the
  // scrolling contents without breaking stacking order. We don't need a special
  // layer like |non_scrolling_squashing_layer_| because these squashed layers
  // are always contained by |scrolling_contents_layer_|.
  Vector<GraphicsLayerPaintInfo> squashed_layers_in_scrolling_contents_;

  PhysicalRect composited_bounds_;

  unsigned pending_update_scope_ : 2;

  bool draws_background_onto_content_layer_;

  friend class CompositedLayerMappingTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITED_LAYER_MAPPING_H_
