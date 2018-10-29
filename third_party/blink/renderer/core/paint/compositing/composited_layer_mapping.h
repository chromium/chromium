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
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class PaintLayerCompositor;

// A GraphicsLayerPaintInfo contains all the info needed to paint a partial
// subtree of Layers into a GraphicsLayer.
struct GraphicsLayerPaintInfo {
  DISALLOW_NEW();
  PaintLayer* paint_layer;

  LayoutRect composited_bounds;

  // The clip rect to apply, in the local coordinate space of the squashed
  // layer, when painting it.
  ClipRect local_clip_rect_for_squashed_layer;
  PaintLayer* local_clip_rect_root;
  LayoutPoint offset_from_clip_rect_root;

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
      Vector<PaintLayer*>& layers_needing_paint_invalidation);

  // Update whether background paints onto scrolling contents layer.
  // Returns (through the reference params) what invalidations are needed.
  void UpdateBackgroundPaintsOntoScrollingContentsLayer(
      bool& invalidate_graphics_layer,
      bool& invalidate_scrolling_contents_layer);

  // Update whether layer needs blending.
  void UpdateContentsOpaque();

  void UpdateRasterizationPolicy();

  GraphicsLayer* MainGraphicsLayer() const { return graphics_layer_.get(); }

  // Layer to clip children
  bool HasClippingLayer() const { return child_containment_layer_.get(); }
  GraphicsLayer* ClippingLayer() const {
    return child_containment_layer_.get();
  }

  // Layer to get clipped by ancestor
  bool HasAncestorClippingLayer() const {
    return ancestor_clipping_layer_.get();
  }
  GraphicsLayer* AncestorClippingLayer() const {
    return ancestor_clipping_layer_.get();
  }

  GraphicsLayer* AncestorClippingMaskLayer() const {
    return ancestor_clipping_mask_layer_.get();
  }

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

  bool HasChildClippingMaskLayer() const {
    return child_clipping_mask_layer_.get();
  }
  GraphicsLayer* ChildClippingMaskLayer() const {
    return child_clipping_mask_layer_.get();
  }

  GraphicsLayer* ParentForSublayers() const;
  GraphicsLayer* ChildForSuperlayers() const;
  void SetSublayers(const GraphicsLayerVector&);

  bool HasChildTransformLayer() const { return child_transform_layer_.get(); }
  GraphicsLayer* ChildTransformLayer() const {
    return child_transform_layer_.get();
  }

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

  LayoutRect CompositedBounds() const { return composited_bounds_; }

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
  void UpdateRenderingContext();
  void UpdateShouldFlattenTransform();
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
  bool IsTrackingRasterInvalidations() const override;
  void SetOverlayScrollbarsHidden(bool) override;

#if DCHECK_IS_ON()
  void VerifyNotPainting() override;
#endif

  LayoutRect ContentsBox() const;

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

  void UpdateFilters();
  void UpdateBackdropFilters();

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

  LayoutSize ContentOffsetInCompositingLayer() const;

  // Returned value does not include any composited scroll offset of
  // the transform ancestor.
  LayoutPoint SquashingOffsetFromTransformedAncestor() const {
    return squashing_layer_offset_from_transformed_ancestor_;
  }

  // If there is a squashed layer painting into this CLM that is an ancestor of
  // the given LayoutObject, return it. Otherwise return nullptr.
  const GraphicsLayerPaintInfo* ContainingSquashedLayer(
      const LayoutObject*,
      unsigned max_squashed_layer_index);

  void UpdateScrollingBlockSelection();

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

  // Returns the PaintLayer which establishes the clip state that
  // MainGraphicsLayer will inherit from the composited layer hierarchy, after
  // taking scroll parent and clip parent into consideration. The clip state can
  // be different from the inherited clip state as defined by CSS spec.
  // Those differences then need to be applied by AncestorClippingLayer.
  const PaintLayer* ClipInheritanceAncestor() const {
    return clip_inheritance_ancestor_;
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
      LayoutPoint* offset_from_transformed_ancestor,
      Vector<PaintLayer*>& layers_needing_paint_invalidation);
  void UpdateMainGraphicsLayerGeometry(
      const IntRect& relative_compositing_bounds,
      const IntRect& local_compositing_bounds,
      const IntPoint& graphics_layer_parent_location);
  void UpdateAncestorClippingLayerGeometry(
      const PaintLayer* compositing_container,
      const IntPoint& snapped_offset_from_composited_ancestor,
      IntPoint& graphics_layer_parent_location);
  void UpdateOverflowControlsHostLayerGeometry(
      const PaintLayer* compositing_stacking_context,
      const PaintLayer* compositing_container,
      IntPoint graphics_layer_parent_location);
  void UpdateChildContainmentLayerGeometry();
  void UpdateChildTransformLayerGeometry();
  void UpdateMaskLayerGeometry();
  void UpdateTransformGeometry(
      const IntPoint& snapped_offset_from_composited_ancestor,
      const IntRect& relative_compositing_bounds);
  void UpdateForegroundLayerGeometry();
  void UpdateDecorationOutlineLayerGeometry(
      const IntSize& relative_compositing_bounds_size);
  void UpdateScrollingLayerGeometry(const IntRect& local_compositing_bounds);
  void UpdateChildClippingMaskLayerGeometry();
  void UpdateStickyConstraints(const ComputedStyle&);

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
  bool UpdateClippingLayers(bool needs_ancestor_clip,
                            bool needs_descendant_clip);
  bool UpdateClippingLayers(bool needs_ancestor_clip,
                            bool needs_ancestor_clipping_mask,
                            bool needs_descendant_clip);
  bool UpdateChildTransformLayer(bool needs_child_transform_layer);
  bool UpdateOverflowControlsLayers(bool needs_horizontal_scrollbar_layer,
                                    bool needs_vertical_scrollbar_layer,
                                    bool needs_scroll_corner_layer,
                                    bool needs_ancestor_clip);
  bool UpdateForegroundLayer(bool needs_foreground_layer);
  bool UpdateDecorationOutlineLayer(bool needs_decoration_outline_layer);
  bool UpdateMaskLayer(bool needs_mask_layer);
  bool UpdateChildClippingMaskLayer(bool needs_child_clipping_mask_layer);
  bool RequiresHorizontalScrollbarLayer() const;
  bool RequiresVerticalScrollbarLayer() const;
  bool RequiresScrollCornerLayer() const;
  bool UpdateScrollingLayers(bool scrolling_layers);
  void UpdateScrollParent(const PaintLayer*);
  void UpdateClipParent(const PaintLayer* scroll_parent);
  bool UpdateSquashingLayers(bool needs_squashing_layers);
  void UpdateDrawsContent();
  void UpdateChildrenTransform();
  void UpdateCompositedBounds();
  void UpdateOverscrollBehavior();
  void UpdateSnapContainerData();
  void RegisterScrollingLayers();

  // Also sets subpixelAccumulation on the layer.
  void ComputeBoundsOfOwningLayer(
      const PaintLayer* composited_ancestor,
      IntRect& local_compositing_bounds,
      IntRect& compositing_bounds_relative_to_composited_ancestor,
      LayoutPoint& offset_from_composited_ancestor,
      IntPoint& snapped_offset_from_composited_ancestor);

  GraphicsLayerPaintingPhase PaintingPhaseForPrimaryLayer() const;

  // Result is transform origin in pixels.
  FloatPoint3D ComputeTransformOrigin(const IntRect& border_box) const;

  void UpdateHitTestableWithoutDrawsContent(const bool&);
  void UpdateOpacity(const ComputedStyle&);
  void UpdateTransform(const ComputedStyle&);
  void UpdateLayerBlendMode(const ComputedStyle&);
  void UpdateIsRootForIsolatedGroup();
  // Return the opacity value that this layer should use for compositing.
  float CompositingOpacity(float layout_object_opacity) const;

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
  // FIXME: unify this code with the code that sets up ancestor_clipping_layer_.
  // They are doing very similar things.
  static void LocalClipRectForSquashedLayer(
      const PaintLayer& reference_layer,
      const Vector<GraphicsLayerPaintInfo>& layers,
      GraphicsLayerPaintInfo&);

  // Conservatively check whether there exists any border-radius clip that
  // must be applied by an ancestor clipping mask layer. There are two inputs
  // to this function: the bounds of contents that are going to be clipped
  // by ancestor clipping layer, and the compositing ancestor which we are
  // going to inherit clip state from.
  // The function works by collecting all border-radius clips between the
  // current layer and the inherited clip, i.e. those are the clips that are
  // going to be applied by the ancestor clipping mask layer. A fast
  // approximation test is used to determine whether the contents exceed
  // the bounds of any of the clips. The function may return false positive
  // (apply mask layer when not strictly needed), but never false negative,
  // as its purpose is only for optimization.
  bool AncestorRoundedCornersWillClip(
      const FloatRect& bounds_in_ancestor_space) const;

  // Return true in |owningLayerIsClipped| iff there is any clip in between
  // the current layer and the inherited clip state. The inherited clip state
  // is determined by the interoperation between compositing container, clip
  // parent, and scroll parent.
  // Return true in |owningLayerIsMasked| iff |owningLayerIsClipped| is true
  // and any of the clip needs to be applied as a painted mask.
  void OwningLayerClippedOrMaskedByLayerNotAboveCompositedAncestor(
      bool& owning_layer_is_clipped,
      bool& owning_layer_is_masked) const;

  const PaintLayer* ScrollParent() const;
  const PaintLayer* CompositedClipParent() const;
  void UpdateClipInheritanceAncestor(const PaintLayer* compositing_container);

  // Clear the groupedMapping entry on the layer at the given index, only if
  // that layer does not appear earlier in the set of layers for this object.
  bool InvalidateLayerIfNoPrecedingEntry(wtf_size_t);

  // Main GraphicsLayer of the CLM for the iframe's content document.
  GraphicsLayer* FrameContentsGraphicsLayer() const;

  PaintLayer& owning_layer_;

  // The hierarchy of layers that is maintained by the CompositedLayerMapping
  // looks like this:
  //
  //  + ancestor_clipping_layer_ [OPTIONAL]
  //    + graphics_layer_
  //      + child_transform_layer_ [OPTIONAL]
  //      | + child_containment_layer_ [OPTIONAL]
  //      |   <-OR->
  //      |   (scrolling_layer_ + scrolling_contents_layer_) [OPTIONAL]
  //      + overflow_controls_ancestor_clipping_layer_ [OPTIONAL]
  //      | + overflow_controls_host_layer_ [OPTIONAL]
  //      |   + layer_for_vertical_scrollbar_ [OPTIONAL]
  //      |   + layer_for_horizontal_scrollbar_ [OPTIONAL]
  //      |   + layer_for_scroll_corner_ [OPTIONAL]
  //      + decoration_outline_layer_ [OPTIONAL]
  // The overflow controls may need to be repositioned in the graphics layer
  // tree by the RLC to ensure that they stack above scrolling content.
  //
  // We need an ancestor clipping layer if our clipping ancestor is not our
  // ancestor in the clipping tree. Here's what that might look like.
  //
  // Let A = the clipping ancestor,
  //     B = the clip descendant, and
  //     SC = the stacking context that is the ancestor of A and B in the
  //          stacking tree.
  //
  // SC
  //  + A = graphics_layer_
  //  |  + child_containment_layer_
  //  |     + ...
  //  ...
  //  |
  //  + B = ancestor_clipping_layer_ [+]
  //     + graphics_layer_
  //        + ...
  //
  // In this case B is clipped by another layer that doesn't happen to be its
  // ancestor: A.  So we create an ancestor clipping layer for B, [+], which
  // ensures that B is clipped as if it had been A's descendant.
  // In addition, the ancestor_clipping_layer_ will have an associated
  // mask layer if the ancestor, A, has a border radius that requires a
  // rounded corner clip rect. The mask is not part of the layer tree; rather
  // it is attached to the ancestor_clipping_layer_ itself.
  //
  // Layers that require a CSS mask also have a mask layer attached to them.

  // Only used if we are clipped by an ancestor which is not a stacking context.
  std::unique_ptr<GraphicsLayer> ancestor_clipping_layer_;

  // Only used is there is an ancestor_clipping_layer_ that also needs to apply
  // a clipping mask (for CSS clips or border radius).
  std::unique_ptr<GraphicsLayer> ancestor_clipping_mask_layer_;

  std::unique_ptr<GraphicsLayer> graphics_layer_;

  // Only used if we have clipping on a stacking context with compositing
  // children.
  std::unique_ptr<GraphicsLayer> child_containment_layer_;

  // Only used if we have perspective.
  std::unique_ptr<GraphicsLayer> child_transform_layer_;

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

  // Only used if we have to clip child layers or accelerated contents with
  // border radius or clip-path.
  std::unique_ptr<GraphicsLayer> child_clipping_mask_layer_;

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

  // The reparented overflow controls sometimes need to be clipped by a
  // non-ancestor. In just the same way we need an ancestor clipping layer to
  // clip this CLM's internal hierarchy, we add another layer to clip the
  // overflow controls. We could combine this with
  // overflow_controls_host_layer_, but that would require manually intersecting
  // their clips, and shifting the overflow controls to compensate for this
  // clip's offset. By using a separate layer, the overflow controls can remain
  // ignorant of ancestor clipping.
  std::unique_ptr<GraphicsLayer> overflow_controls_ancestor_clipping_layer_;

  // DecorationLayer which paints outline.
  std::unique_ptr<GraphicsLayer> decoration_outline_layer_;

  // A squashing CLM has two possible squashing-related structures.
  //
  // If ancestor_clipping_layer_ is present:
  //
  // ancestor_clipping_layer_
  //   + graphics_layer_
  //   + squashing_layer_
  //
  // If not:
  //
  // squashing_containment_layer_
  //   + graphics_layer_
  //   + squashing_layer_
  //
  // Stacking children of a squashed layer receive graphics layers that are
  // parented to the compositd ancestor of the squashed layer (i.e. nearest
  // enclosing composited layer that is not
  // squashed).

  // Only used if any squashed layers exist and ancestor_clipping_layer_ is
  // not present, to contain the squashed layers as siblings to the rest of the
  // GraphicsLayer tree chunk.
  std::unique_ptr<GraphicsLayer> squashing_containment_layer_;

  // Only used if any squashed layers exist, this is the backing that squashed
  // layers paint into.
  std::unique_ptr<GraphicsLayer> squashing_layer_;
  Vector<GraphicsLayerPaintInfo> squashed_layers_;
  LayoutPoint squashing_layer_offset_from_transformed_ancestor_;
  IntSize squashing_layer_offset_from_layout_object_;

  LayoutRect composited_bounds_;

  // We keep track of the scrolling contents offset, so that when it changes we
  // can notify the ScrollingCoordinator, which passes on main-thread scrolling
  // updates to the compositor.
  DoubleSize scrolling_contents_offset_;

  const PaintLayer* clip_inheritance_ancestor_;

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
