/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_LAYER_H_

#include <memory>
#include "base/auto_reset.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_testing_transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/clip_rects_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer_resource_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_result.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class CompositedLayerMapping;
class CompositorFilterOperations;
class ComputedStyle;
class FilterEffect;
class FilterOperations;
class HitTestResult;
class HitTestingTransformState;
class PaintLayerCompositor;
class PaintLayerScrollableArea;
class ScrollingCoordinator;
class TransformationMatrix;

using PaintLayerId = uint64_t;

enum IncludeSelfOrNot { kIncludeSelf, kExcludeSelf };

enum CompositingQueryMode {
  kCompositingQueriesAreAllowed,
  kCompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases
};

// FIXME: remove this once the compositing query DCHECKS are no longer hit.
class CORE_EXPORT DisableCompositingQueryAsserts {
  STACK_ALLOCATED();

 public:
  DisableCompositingQueryAsserts();

 private:
  base::AutoReset<CompositingQueryMode> disabler_;
  DISALLOW_COPY_AND_ASSIGN(DisableCompositingQueryAsserts);
};

struct PaintLayerRareData {
  USING_FAST_MALLOC(PaintLayerRareData);

 public:
  PaintLayerRareData();
  ~PaintLayerRareData();

  // Our current relative position offset.
  LayoutSize offset_for_in_flow_position;

  std::unique_ptr<TransformationMatrix> transform;

  // Pointer to the enclosing Layer that caused us to be paginated. It is 0 if
  // we are not paginated.
  //
  // See LayoutMultiColumnFlowThread and
  // https://sites.google.com/a/chromium.org/dev/developers/design-documents/multi-column-layout
  // for more information about the multicol implementation. It's important to
  // understand the difference between flow thread coordinates and visual
  // coordinates when working with multicol in Layer, since Layer is one of the
  // few places where we have to worry about the visual ones. Internally we try
  // to use flow-thread coordinates whenever possible.
  PaintLayer* enclosing_pagination_layer;

  // These compositing reasons are updated whenever style changes, not while
  // updating compositing layers.  They should not be used to infer the
  // compositing state of this layer.
  CompositingReasons potential_compositing_reasons_from_style;

  CompositingReasons potential_compositing_reasons_from_non_style;

  // Once computed, indicates all that a layer needs to become composited using
  // the CompositingReasons enum bitfield.
  CompositingReasons compositing_reasons;

  // This captures reasons why a paint layer might be forced to be separately
  // composited rather than sharing a backing with another layer.
  SquashingDisallowedReasons squashing_disallowed_reasons;

  // If the layer paints into its own backings, this keeps track of the
  // backings.  It's nullptr if the layer is not composited or paints into
  // grouped backing.
  std::unique_ptr<CompositedLayerMapping> composited_layer_mapping;

  // If the layer paints into grouped backing (i.e. squashed), this points to
  // the grouped CompositedLayerMapping. It's null if the layer is not
  // composited or paints into its own backing.
  CompositedLayerMapping* grouped_mapping;

  Persistent<PaintLayerResourceInfo> resource_info;

  // The accumulated subpixel offset of a composited layer's composited bounds
  // compared to absolute coordinates.
  LayoutSize subpixel_accumulation;

  DISALLOW_COPY_AND_ASSIGN(PaintLayerRareData);
};

// PaintLayer is an old object that handles lots of unrelated operations.
//
// We want it to die at some point and be replaced by more focused objects,
// which would remove (or at least compartimentalize) a lot of complexity.
// See the STATUS OF PAINTLAYER section below.
//
// The class is central to painting and hit-testing. That's because it handles
// a lot of tasks (we included ones done by associated satellite objects for
// historical reasons):
// - Complex painting operations (opacity, clipping, filters, reflections, ...).
// - hardware acceleration (through PaintLayerCompositor).
// - scrolling (through PaintLayerScrollableArea).
// - some performance optimizations.
//
// The compositing code is also based on PaintLayer. The entry to it is the
// PaintLayerCompositor, which fills |composited_layer_mapping| for hardware
// accelerated layers.
//
// TODO(jchaffraix): Expand the documentation about hardware acceleration.
//
//
// ***** SELF-PAINTING LAYER *****
// One important concept about PaintLayer is "self-painting"
// (is_self_painting_layer_).
// PaintLayer started as the implementation of a stacking context. This meant
// that we had to use PaintLayer's painting order (the code is now in
// PaintLayerPainter and PaintLayerStackingNode) instead of the LayoutObject's
// children order. Over the years, as more operations were handled by
// PaintLayer, some LayoutObjects that were not stacking context needed to have
// a PaintLayer for bookkeeping reasons. One such example is the overflow hidden
// case that wanted hardware acceleration and thus had to allocate a PaintLayer
// to get it. However overflow hidden is something LayoutObject can paint
// without a PaintLayer, which includes a lot of painting overhead. Thus the
// self-painting flag was introduced. The flag is a band-aid solution done for
// performance reason only. It just brush over the underlying problem, which is
// that its design doesn't match the system's requirements anymore.
//
// Note that the self-painting flag determines how we paint a LayoutObject:
// - If the flag is true, the LayoutObject is painted through its PaintLayer,
//   which is required to apply complex paint operations. The paint order is
//   handled by PaintLayerPainter::paintChildren, where we look at children
//   PaintLayers.
// - If the flag is false, the LayoutObject is painted like normal children (ie
//   as if it didn't have a PaintLayer). The paint order is handled by
//   BlockPainter::paintChild that looks at children LayoutObjects.
// This means that the self-painting flag changes the painting order in a subtle
// way, which can potentially have visible consequences. Those bugs are called
// painting inversion as we invert the order of painting for 2 elements
// (painting one wrongly in front of the other).
// See https://crbug.com/370604 for an example.
//
//
// ***** STATUS OF PAINTLAYER *****
// We would like to remove this class in the future. The reasons for the removal
// are:
// - it has been a dumping ground for features for too long.
// - it is the wrong level of abstraction, bearing no correspondence to any CSS
//   concept.
//
// Its features need to be migrated to helper objects. This was started with the
// introduction of satellite objects: PaintLayer*. Those helper objects then
// need to be moved to the appropriate LayoutObject class, probably to a rare
// data field to avoid growing all the LayoutObjects.
//
// A good example of this is PaintLayerScrollableArea, which can only happen
// be instanciated for LayoutBoxes. With the current design, it's hard to know
// that by reading the code.
class CORE_EXPORT PaintLayer : public DisplayItemClient {

 public:
  PaintLayer(LayoutBoxModelObject&);
  ~PaintLayer() override;

  // DisplayItemClient methods
  String DebugName() const final;
  LayoutRect VisualRect() const final;

  LayoutBoxModelObject& GetLayoutObject() const { return layout_object_; }
  LayoutBox* GetLayoutBox() const {
    return layout_object_.IsBox() ? &ToLayoutBox(layout_object_) : nullptr;
  }
  PaintLayer* Parent() const { return parent_; }
  PaintLayer* PreviousSibling() const { return previous_; }
  PaintLayer* NextSibling() const { return next_; }
  PaintLayer* FirstChild() const { return first_; }
  PaintLayer* LastChild() const { return last_; }

  // TODO(wangxianzhu): Find a better name for it. 'paintContainer' might be
  // good but we can't use it for now because it conflicts with
  // PaintInfo::paintContainer.
  PaintLayer* CompositingContainer() const;

  void AddChild(PaintLayer* new_child, PaintLayer* before_child = nullptr);
  PaintLayer* RemoveChild(PaintLayer*);

  void ClearClipRects(ClipRectsCacheSlot = kNumberOfClipRectsCacheSlots);

  void RemoveOnlyThisLayerAfterStyleChange(const ComputedStyle* old_style);
  void InsertOnlyThisLayerAfterStyleChange();

  void StyleDidChange(StyleDifference, const ComputedStyle* old_style);

  // FIXME: Many people call this function while it has out-of-date information.
  bool IsSelfPaintingLayer() const { return is_self_painting_layer_; }

  bool IsTransparent() const {
    return GetLayoutObject().StyleRef().HasOpacity() ||
           GetLayoutObject().StyleRef().HasBlendMode() ||
           GetLayoutObject().HasMask();
  }

  const PaintLayer* Root() const {
    const PaintLayer* curr = this;
    while (curr->Parent())
      curr = curr->Parent();
    return curr;
  }

  LayoutPoint Location() const {
#if DCHECK_IS_ON()
    DCHECK(!needs_position_update_);
#endif
    return LocationInternal();
  }

  // FIXME: size() should DCHECK(!needs_position_update_) as well, but that
  // fails in some tests, for example, fast/repaint/clipped-relative.html.
  const LayoutSize& Size() const { return size_; }
  IntSize PixelSnappedSize() const {
    LayoutPoint location = layout_object_.IsBox()
                               ? ToLayoutBox(layout_object_).Location()
                               : LayoutPoint();
    return PixelSnappedIntSize(Size(), location);
  }

  void SetSizeHackForLayoutTreeAsText(const LayoutSize& size) { size_ = size; }

  // For LayoutTreeAsText
  LayoutRect RectIgnoringNeedsPositionUpdate() const {
    return LayoutRect(LocationInternal(), size_);
  }
#if DCHECK_IS_ON()
  bool NeedsPositionUpdate() const { return needs_position_update_; }
#endif

  bool IsRootLayer() const { return is_root_layer_; }

  PaintLayerCompositor* Compositor() const;

  // Notification from the layoutObject that its content changed (e.g. current
  // frame of image changed).  Allows updates of layer content without
  // invalidating paint.
  void ContentChanged(ContentChangeType);

  bool UpdateSize();
  void UpdateSizeAndScrollingAfterLayout();

  void UpdateLayerPosition();
  void UpdateLayerPositionsAfterLayout();
  void UpdateLayerPositionsAfterOverflowScroll();

  PaintLayer* EnclosingPaginationLayer() const {
    return rare_data_ ? rare_data_->enclosing_pagination_layer : nullptr;
  }

  void UpdateTransformationMatrix();
  PaintLayer* RenderingContextRoot();
  const PaintLayer* RenderingContextRoot() const;

  LayoutSize OffsetForInFlowPosition() const {
    return rare_data_ ? rare_data_->offset_for_in_flow_position : LayoutSize();
  }

  PaintLayerStackingNode* StackingNode() { return stacking_node_.get(); }
  const PaintLayerStackingNode* StackingNode() const {
    return stacking_node_.get();
  }

  bool SubtreeIsInvisible() const {
    return !HasVisibleContent() && !HasVisibleDescendant();
  }

  bool HasVisibleContent() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_visible_content_;
  }

  bool HasVisibleDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_visible_descendant_;
  }

  void DirtyVisibleContentStatus();

  bool HasBoxDecorationsOrBackground() const;
  bool HasVisibleBoxDecorations() const;
  // True if this layer container layoutObjects that paint.
  bool HasNonEmptyChildLayoutObjects() const;

  // Gets the ancestor layer that serves as the containing block (in the sense
  // of LayoutObject::container() instead of LayoutObject::containingBlock())
  // of this layer. Normally the parent layer is the containing layer, except
  // for out of flow positioned, floating and multicol spanner layers whose
  // containing layer might be an ancestor of the parent layer.
  // If |ancestor| is specified, |*skippedAncestor| will be set to true if
  // |ancestor| is found in the ancestry chain between this layer and the
  // containing block layer; if not found, it will be set to false. Either both
  // |ancestor| and |skippedAncestor| should be nullptr, or none of them should.
  PaintLayer* ContainingLayer(const PaintLayer* ancestor = nullptr,
                              bool* skipped_ancestor = nullptr) const;

  bool IsPaintInvalidationContainer() const;

  // Do *not* call this method unless you know what you are dooing. You probably
  // want to call enclosingCompositingLayerForPaintInvalidation() instead.
  // If includeSelf is true, may return this.
  PaintLayer* EnclosingLayerWithCompositedLayerMapping(IncludeSelfOrNot) const;

  // Returns the enclosing layer root into which this layer paints, inclusive of
  // this one. Note that the enclosing layer may or may not have its own
  // GraphicsLayer backing, but is nevertheless the root for a call to the
  // Layer::paint*() methods.
  PaintLayer* EnclosingLayerForPaintInvalidation() const;

  // https://crbug.com/751768, this function can return nullptr sometimes.
  // Always check the result before using it, don't just DCHECK.
  PaintLayer* EnclosingLayerForPaintInvalidationCrossingFrameBoundaries() const;

  bool HasAncestorWithFilterThatMovesPixels() const;

  bool CanUseConvertToLayerCoords() const {
    // These LayoutObjects have an impact on their layers without the
    // layoutObjects knowing about it.
    return !GetLayoutObject().HasTransformRelatedProperty() &&
           !GetLayoutObject().IsSVGRoot();
  }

  void ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                            LayoutPoint&) const;
  void ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                            LayoutRect&) const;

  // Does the same as convertToLayerCoords() when not in multicol. For multicol,
  // however, convertToLayerCoords() calculates the offset in flow-thread
  // coordinates (what the layout engine uses internally), while this method
  // calculates the visual coordinates; i.e. it figures out which column the
  // layer starts in and adds in the offset. See
  // http://www.chromium.org/developers/design-documents/multi-column-layout for
  // more info.
  LayoutPoint VisualOffsetFromAncestor(
      const PaintLayer* ancestor_layer,
      LayoutPoint offset = LayoutPoint()) const;

  // Convert a bounding box from flow thread coordinates, relative to |this|, to
  // visual coordinates, relative to |ancestorLayer|.
  // See http://www.chromium.org/developers/design-documents/multi-column-layout
  // for more info on these coordinate types.  This method requires this layer
  // to be paginated; i.e. it must have an enclosingPaginationLayer().
  void ConvertFromFlowThreadToVisualBoundingBoxInAncestor(
      const PaintLayer* ancestor_layer,
      LayoutRect&) const;

  // The hitTest() method looks for mouse events by walking layers that
  // intersect the point from front to back.
  // |hit_test_area| is the rect in the space of this PaintLayer's
  // LayoutObject to consider for hit testing.
  bool HitTest(const HitTestLocation& location,
               HitTestResult&,
               const LayoutRect& hit_test_area);

  bool IntersectsDamageRect(const LayoutRect& layer_bounds,
                            const LayoutRect& damage_rect,
                            const LayoutPoint& offset_from_root) const;

  // MaybeIncludeTransformForAncestorLayer means that a transform on
  // |ancestorLayer| may be applied to the bounding box, in particular if
  // paintsWithTransform() is true.
  enum CalculateBoundsOptions {
    kMaybeIncludeTransformForAncestorLayer,
    kNeverIncludeTransformForAncestorLayer,
    kIncludeTransformsAndCompositedChildLayers,
  };

  // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
  LayoutRect PhysicalBoundingBox(const LayoutPoint& offset_from_root) const;
  LayoutRect PhysicalBoundingBox(const PaintLayer* ancestor_layer) const;
  LayoutRect PhysicalBoundingBoxIncludingStackingChildren(
      const LayoutPoint& offset_from_root,
      CalculateBoundsOptions = kMaybeIncludeTransformForAncestorLayer) const;
  LayoutRect FragmentsBoundingBox(const PaintLayer* ancestor_layer) const;

  LayoutRect BoundingBoxForCompositingOverlapTest() const;
  LayoutRect BoundingBoxForCompositing() const;

  // If true, this layer's children are included in its bounds for overlap
  // testing.  We can't rely on the children's positions if this layer has a
  // filter that could have moved the children's pixels around.
  bool OverlapBoundsIncludeChildren() const;

  // Static position is set in parent's coordinate space.
  LayoutUnit StaticInlinePosition() const { return static_inline_position_; }
  LayoutUnit StaticBlockPosition() const { return static_block_position_; }

  void SetStaticInlinePosition(LayoutUnit position) {
    static_inline_position_ = position;
  }
  void SetStaticBlockPosition(LayoutUnit position) {
    static_block_position_ = position;
  }

  LayoutSize SubpixelAccumulation() const;
  void SetSubpixelAccumulation(const LayoutSize&);

  bool HasTransformRelatedProperty() const {
    return GetLayoutObject().HasTransformRelatedProperty();
  }
  // Note that this transform has the transform-origin baked in.
  TransformationMatrix* Transform() const {
    return rare_data_ ? rare_data_->transform.get() : nullptr;
  }

  // currentTransform computes a transform which takes accelerated animations
  // into account. The resulting transform has transform-origin baked in. If the
  // layer does not have a transform, returns the identity matrix.
  TransformationMatrix CurrentTransform() const;
  TransformationMatrix RenderableTransform(GlobalPaintFlags) const;

  // Get the perspective transform, which is applied to transformed sublayers.
  // Returns true if the layer has a -webkit-perspective.
  // Note that this transform does not have the perspective-origin baked in.
  TransformationMatrix PerspectiveTransform() const;
  FloatPoint PerspectiveOrigin() const;
  bool Preserves3D() const {
    return GetLayoutObject().StyleRef().Preserves3D();
  }
  bool Has3DTransform() const {
    return rare_data_ && rare_data_->transform &&
           !rare_data_->transform->IsAffine();
  }

  // FIXME: reflections should force transform-style to be flat in the style:
  // https://bugs.webkit.org/show_bug.cgi?id=106959
  bool ShouldPreserve3D() const {
    return !GetLayoutObject().HasReflection() &&
           GetLayoutObject().StyleRef().Preserves3D();
  }

  // Returns |true| if any property that renders using filter operations is
  // used (including, but not limited to, 'filter' and 'box-reflect').
  bool HasFilterInducingProperty() const {
    return GetLayoutObject().HasFilterInducingProperty();
  }

  void* operator new(size_t);
  // Only safe to call from LayoutBoxModelObject::destroyLayer()
  void operator delete(void*);

  CompositingState GetCompositingState() const;

  // This returns true if our document is in a phase of its lifestyle during
  // which compositing state may legally be read.
  bool IsAllowedToQueryCompositingState() const;

  // Don't null check this.
  // FIXME: Rename.
  CompositedLayerMapping* GetCompositedLayerMapping() const;

  // Returns the GraphicsLayer owned by this PaintLayer's
  // CompositedLayerMapping (or groupedMapping()'s, if squashed),
  // into which the given LayoutObject paints. If null, assumes the
  // LayoutObject is *not* layoutObject().
  // Assumes that the given LayoutObject paints into one of the GraphicsLayers
  // associated with this PaintLayer.
  // Returns nullptr if this PaintLayer is not composited.
  GraphicsLayer* GraphicsLayerBacking(const LayoutObject* = nullptr) const;

  // NOTE: If you are using hasCompositedLayerMapping to determine the state of
  // compositing for this layer, (and not just to do bookkeeping related to the
  // mapping like, say, allocating or deallocating a mapping), then you may have
  // incorrect logic. Use compositingState() instead.
  // FIXME: This is identical to null checking compositedLayerMapping(), why not
  // just call that?
  bool HasCompositedLayerMapping() const {
    return rare_data_ && rare_data_->composited_layer_mapping;
  }
  void EnsureCompositedLayerMapping();
  void ClearCompositedLayerMapping(bool layer_being_destroyed = false);
  CompositedLayerMapping* GroupedMapping() const {
    return rare_data_ ? rare_data_->grouped_mapping : nullptr;
  }
  enum SetGroupMappingOptions {
    kInvalidateLayerAndRemoveFromMapping,
    kDoNotInvalidateLayerAndRemoveFromMapping
  };
  void SetGroupedMapping(CompositedLayerMapping*, SetGroupMappingOptions);

  bool NeedsCompositedScrolling() const;

  // Paint invalidation containers can be self-composited or squashed.
  // In the former case, these methods do nothing.
  // In the latter case, they adjust from the space of the squashed PaintLayer
  // to the space of the PaintLayer into which it squashes.
  //
  // Note that this method does *not* adjust rects into the space of any
  // particular GraphicsLayer. To do that requires adjusting for the
  // offsetFromLayoutObject of the desired GraphicsLayer (which can differ
  // for different GraphicsLayers belonging to the same
  // CompositedLayerMapping).
  static void MapPointInPaintInvalidationContainerToBacking(
      const LayoutBoxModelObject& paint_invalidation_container,
      FloatPoint&);
  static void MapRectInPaintInvalidationContainerToBacking(
      const LayoutBoxModelObject& paint_invalidation_container,
      LayoutRect&);

  // Adjusts the given rect (in the coordinate space of the LayoutObject) to the
  // coordinate space of |paintInvalidationContainer|'s GraphicsLayer backing.
  // Should use PaintInvalidatorContext::MapRectToPaintInvalidationBacking()
  // instead if PaintInvalidatorContext.
  static void MapRectToPaintInvalidationBacking(
      const LayoutObject&,
      const LayoutBoxModelObject& paint_invalidation_container,
      LayoutRect&);

  bool PaintsWithTransparency(GlobalPaintFlags global_paint_flags) const {
    return IsTransparent() && !PaintsIntoOwnBacking(global_paint_flags);
  }

  // Returns the ScrollingCoordinator associated with this layer, if
  // any. Otherwise nullptr.
  ScrollingCoordinator* GetScrollingCoordinator();

  // Returns true if the element or any ancestor is transformed.
  bool CompositesWithTransform() const;

  // Returns true if the element or any ancestor has non 1 opacity.
  bool CompositesWithOpacity() const;

  bool PaintsWithTransform(GlobalPaintFlags) const;
  bool PaintsIntoOwnBacking(GlobalPaintFlags) const;
  bool PaintsIntoOwnOrGroupedBacking(GlobalPaintFlags) const;

  bool SupportsSubsequenceCaching() const;

  // Returns true if background phase is painted opaque in the given rect.
  // The query rect is given in local coordinates.
  bool BackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

  bool ContainsDirtyOverlayScrollbars() const {
    return contains_dirty_overlay_scrollbars_;
  }
  void SetContainsDirtyOverlayScrollbars(bool dirty_scrollbars) {
    contains_dirty_overlay_scrollbars_ = dirty_scrollbars;
  }

  // If the input CompositorFilterOperation is not empty, it will be populated
  // only if |filter_on_effect_node_dirty_| is true or the reference box has
  // changed. Otherwise it will be populated unconditionally.
  void UpdateCompositorFilterOperationsForFilter(
      CompositorFilterOperations&) const;
  void SetFilterOnEffectNodeDirty() { filter_on_effect_node_dirty_ = true; }
  void ClearFilterOnEffectNodeDirty() { filter_on_effect_node_dirty_ = false; }

  void SetIsUnderSVGHiddenContainer(bool value) {
    is_under_svg_hidden_container_ = value;
  }
  bool IsUnderSVGHiddenContainer() { return is_under_svg_hidden_container_; }

  CompositorFilterOperations CreateCompositorFilterOperationsForBackdropFilter()
      const;

  bool PaintsWithFilters() const;
  FilterEffect* LastFilterEffect() const;

  // Maps "forward" to determine which pixels in a destination rect are
  // affected by pixels in the source rect.
  // See also FilterEffect::mapRect.
  FloatRect MapRectForFilter(const FloatRect&) const;

  // Calls the above, rounding outwards.
  LayoutRect MapLayoutRectForFilter(const LayoutRect&) const;

  bool HasFilterThatMovesPixels() const;

  PaintLayerResourceInfo* ResourceInfo() const {
    return rare_data_ ? rare_data_->resource_info.Get() : nullptr;
  }
  PaintLayerResourceInfo& EnsureResourceInfo();

  void UpdateFilters(const ComputedStyle* old_style,
                     const ComputedStyle& new_style);
  void UpdateClipPath(const ComputedStyle* old_style,
                      const ComputedStyle& new_style);

  Node* EnclosingNode() const;

  bool IsInTopLayer() const;

  // Returns true if the layer is sticky position and may stick to its
  // ancestor overflow layer.
  bool SticksToScroller() const;

  // Returns true if the layer is fixed position and will not move with
  // scrolling.
  bool FixedToViewport() const;
  bool ScrollsWithRespectTo(const PaintLayer*) const;

  bool IsAffectedByScrollOf(const PaintLayer* ancestor) const;

  void AddLayerHitTestRects(LayerHitTestRects&, TouchAction) const;

  // Compute rects only for this layer
  void ComputeSelfHitTestRects(LayerHitTestRects&, TouchAction) const;

  // FIXME: This should probably return a ScrollableArea but a lot of internal
  // methods are mistakenly exposed.
  PaintLayerScrollableArea* GetScrollableArea() const {
    return scrollable_area_.Get();
  }

  enum GeometryMapperOption { kUseGeometryMapper, kDoNotUseGeometryMapper };

  PaintLayerClipper Clipper(GeometryMapperOption) const;

  bool ScrollsOverflow() const;

  CompositingReasons DirectCompositingReasons() const {
    return rare_data_
               ? ((rare_data_->potential_compositing_reasons_from_style |
                   rare_data_->potential_compositing_reasons_from_non_style) &
                  CompositingReason::kComboAllDirectReasons)
               : CompositingReason::kNone;
  }

  CompositingReasons PotentialCompositingReasonsFromStyle() const {
    return rare_data_ ? rare_data_->potential_compositing_reasons_from_style
                      : CompositingReason::kNone;
  }
  void SetPotentialCompositingReasonsFromStyle(CompositingReasons reasons) {
    DCHECK(reasons ==
           (reasons & CompositingReason::kComboAllStyleDeterminedReasons));
    if (rare_data_ || reasons != CompositingReason::kNone)
      EnsureRareData().potential_compositing_reasons_from_style = reasons;
  }
  CompositingReasons PotentialCompositingReasonsFromNonStyle() const {
    return rare_data_ ? rare_data_->potential_compositing_reasons_from_non_style
                      : CompositingReason::kNone;
  }
  void SetPotentialCompositingReasonsFromNonStyle(CompositingReasons reasons) {
    DCHECK(reasons ==
           (reasons &
            CompositingReason::kComboAllDirectNonStyleDeterminedReasons));
    if (rare_data_ || reasons != CompositingReason::kNone)
      EnsureRareData().potential_compositing_reasons_from_non_style = reasons;
  }

  bool HasStyleDeterminedDirectCompositingReasons() const {
    return PotentialCompositingReasonsFromStyle() &
           CompositingReason::kComboAllDirectStyleDeterminedReasons;
  }

  struct AncestorDependentCompositingInputs {
   public:
    const PaintLayer* opacity_ancestor = nullptr;
    const PaintLayer* transform_ancestor = nullptr;
    const PaintLayer* filter_ancestor = nullptr;
    const PaintLayer* clip_path_ancestor = nullptr;
    const PaintLayer* mask_ancestor = nullptr;

    // The fist ancestor which can scroll. This is a subset of the
    // ancestorOverflowLayer chain where the scrolling layer is visible and
    // has a larger scroll content than its bounds.
    const PaintLayer* ancestor_scrolling_layer = nullptr;
    const PaintLayer* nearest_fixed_position_layer = nullptr;

    // A scroll parent is a compositor concept. It's only needed in blink
    // because we need to use it as a promotion trigger. A layer has a
    // scroll parent if neither its compositor scrolling ancestor, nor any
    // other layer scrolled by this ancestor, is a stacking ancestor of this
    // layer. Layers with scroll parents must be scrolled with the main
    // scrolling layer by the compositor.
    const PaintLayer* scroll_parent = nullptr;

    // A clip parent is another compositor concept that has leaked into
    // blink so that it may be used as a promotion trigger. Layers with clip
    // parents escape the clip of a stacking tree ancestor. The compositor
    // needs to know about clip parents in order to circumvent its normal
    // clipping logic.
    const PaintLayer* clip_parent = nullptr;

    // These two boxes do not include any applicable scroll offset of the
    // root PaintLayer.
    IntRect clipped_absolute_bounding_box;
    IntRect unclipped_absolute_bounding_box;

    const LayoutBoxModelObject* clipping_container = nullptr;
  };

  void SetNeedsCompositingInputsUpdate();
  bool ChildNeedsCompositingInputsUpdate() const {
    return child_needs_compositing_inputs_update_;
  }
  bool NeedsCompositingInputsUpdate() const {
    return needs_ancestor_dependent_compositing_inputs_update_;
  }

  void UpdateAncestorOverflowLayer(const PaintLayer* ancestor_overflow_layer) {
    ancestor_overflow_layer_ = ancestor_overflow_layer;
  }
  void UpdateAncestorDependentCompositingInputs(
      const AncestorDependentCompositingInputs&);
  void ClearChildNeedsCompositingInputsUpdate();

  const AncestorDependentCompositingInputs&
  GetAncestorDependentCompositingInputs() const {
    DCHECK(!needs_ancestor_dependent_compositing_inputs_update_);
    return EnsureAncestorDependentCompositingInputs();
  }

  // These two  do not include any applicable scroll offset of the
  // root PaintLayer.
  const IntRect& ClippedAbsoluteBoundingBox() const {
    return GetAncestorDependentCompositingInputs()
        .clipped_absolute_bounding_box;
  }
  const IntRect& UnclippedAbsoluteBoundingBox() const {
    return GetAncestorDependentCompositingInputs()
        .unclipped_absolute_bounding_box;
  }

  const PaintLayer* OpacityAncestor() const {
    return GetAncestorDependentCompositingInputs().opacity_ancestor;
  }
  const PaintLayer* TransformAncestor() const {
    return GetAncestorDependentCompositingInputs().transform_ancestor;
  }
  const PaintLayer& TransformAncestorOrRoot() const;
  const PaintLayer* FilterAncestor() const {
    return GetAncestorDependentCompositingInputs().filter_ancestor;
  }
  const LayoutBoxModelObject* ClippingContainer() const {
    return GetAncestorDependentCompositingInputs().clipping_container;
  }
  const PaintLayer* AncestorOverflowLayer() const {
    return ancestor_overflow_layer_;
  }
  const PaintLayer* AncestorScrollingLayer() const {
    return GetAncestorDependentCompositingInputs().ancestor_scrolling_layer;
  }
  const PaintLayer* NearestFixedPositionLayer() const {
    return GetAncestorDependentCompositingInputs().nearest_fixed_position_layer;
  }
  const PaintLayer* ScrollParent() const {
    return GetAncestorDependentCompositingInputs().scroll_parent;
  }
  const PaintLayer* ClipParent() const {
    return GetAncestorDependentCompositingInputs().clip_parent;
  }
  const PaintLayer* ClipPathAncestor() const {
    return GetAncestorDependentCompositingInputs().clip_path_ancestor;
  }
  const PaintLayer* MaskAncestor() const {
    return GetAncestorDependentCompositingInputs().mask_ancestor;
  }
  bool HasDescendantWithClipPath() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_descendant_with_clip_path_;
  }
  bool HasFixedPositionDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_fixed_position_descendant_;
  }
  bool HasStickyPositionDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_sticky_position_descendant_;
  }
  bool HasNonContainedAbsolutePositionDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_non_contained_absolute_position_descendant_;
  }
  bool HasSelfPaintingLayerDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has_self_painting_layer_descendant_;
  }
  bool IsNonStackedWithInFlowStackedDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return is_non_stacked_with_in_flow_stacked_descendant_;
  }

  // Returns true if there is a descendant with blend-mode that is
  // not contained within another enclosing stacking context other
  // than the stacking context blend-mode creates, or the stacking
  // context this PaintLayer might create. This is needed because
  // blend-mode content needs to blend with the containing stacking
  // context's painted output, but not the content in any grandparent
  // stacking contexts.
  bool HasNonIsolatedDescendantWithBlendMode() const;

  bool LostGroupedMapping() const {
    DCHECK(IsAllowedToQueryCompositingState());
    return lost_grouped_mapping_;
  }
  void SetLostGroupedMapping(bool b) {
    lost_grouped_mapping_ = b;
    needs_compositing_layer_assignment_ =
        needs_compositing_layer_assignment_ || b;
  }

  CompositingReasons GetCompositingReasons() const {
    DCHECK(IsAllowedToQueryCompositingState());
    return rare_data_ ? rare_data_->compositing_reasons
                      : CompositingReason::kNone;
  }
  void SetCompositingReasons(CompositingReasons,
                             CompositingReasons mask = CompositingReason::kAll);

  SquashingDisallowedReasons GetSquashingDisallowedReasons() const {
    DCHECK(IsAllowedToQueryCompositingState());
    return rare_data_ ? rare_data_->squashing_disallowed_reasons
                      : SquashingDisallowedReason::kNone;
  }
  void SetSquashingDisallowedReasons(SquashingDisallowedReasons);

  bool HasCompositingDescendant() const {
    DCHECK(IsAllowedToQueryCompositingState());
    return has_compositing_descendant_;
  }
  void SetHasCompositingDescendant(bool);

  bool ShouldIsolateCompositedDescendants() const {
    DCHECK(IsAllowedToQueryCompositingState());
    return should_isolate_composited_descendants_;
  }
  void SetShouldIsolateCompositedDescendants(bool);

  void UpdateDescendantDependentFlags();

  void UpdateSelfPaintingLayer();
  // This is O(depth) so avoid calling this in loops. Instead use optimizations
  // like those in PaintInvalidatorContext.
  PaintLayer* EnclosingSelfPaintingLayer();

  // Returned value does not include any composited scroll offset of
  // the transform ancestor.
  LayoutPoint ComputeOffsetFromAncestor(const PaintLayer& ancestor_layer) const;

  void DidUpdateScrollsOverflow();

  LayoutRect PaintingExtent(const PaintLayer* root_layer,
                            const LayoutSize& sub_pixel_accumulation,
                            GlobalPaintFlags);

  void AppendSingleFragmentIgnoringPagination(
      PaintLayerFragments&,
      const PaintLayer* root_layer,
      const LayoutRect* dirty_rect,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize,
      ShouldRespectOverflowClipType = kRespectOverflowClip,
      const LayoutPoint* offset_from_root = nullptr,
      const LayoutSize& sub_pixel_accumulation = LayoutSize()) const;

  void CollectFragments(
      PaintLayerFragments&,
      const PaintLayer* root_layer,
      const LayoutRect* dirty_rect,
      OverlayScrollbarClipBehavior = kIgnorePlatformOverlayScrollbarSize,
      ShouldRespectOverflowClipType = kRespectOverflowClip,
      const LayoutPoint* offset_from_root = nullptr,
      const LayoutSize& sub_pixel_accumulation = LayoutSize()) const;

  LayoutPoint LayoutBoxLocation() const {
    return GetLayoutObject().IsBox() ? ToLayoutBox(GetLayoutObject()).Location()
                                     : LayoutPoint();
  }

  enum TransparencyClipBoxBehavior {
    kPaintingTransparencyClipBox,
    kHitTestingTransparencyClipBox
  };

  enum TransparencyClipBoxMode {
    kDescendantsOfTransparencyClipBox,
    kRootOfTransparencyClipBox
  };

  static LayoutRect TransparencyClipBox(
      const PaintLayer*,
      const PaintLayer* root_layer,
      TransparencyClipBoxBehavior transparency_behavior,
      TransparencyClipBoxMode transparency_mode,
      const LayoutSize& sub_pixel_accumulation,
      GlobalPaintFlags = kGlobalPaintNormalPhase);

  bool NeedsRepaint() const { return needs_repaint_; }
  void SetNeedsRepaint();
  void ClearNeedsRepaintRecursively();

  // These previousXXX() functions are for subsequence caching. They save the
  // painting status of the layer during the previous painting with subsequence.
  // A painting without subsequence [1] doesn't change this status.  [1] See
  // shouldCreateSubsequence() in PaintLayerPainter.cpp for the cases we use
  // subsequence when painting a PaintLayer.

  LayoutRect PreviousPaintDirtyRect() const {
    return previous_paint_dirty_rect_;
  }
  void SetPreviousPaintDirtyRect(const LayoutRect& rect) {
    previous_paint_dirty_rect_ = rect;
  }

  PaintResult PreviousPaintResult() const {
    return static_cast<PaintResult>(previous_paint_result_);
  }
  void SetPreviousPaintResult(PaintResult result) {
    previous_paint_result_ = static_cast<unsigned>(result);
    DCHECK(previous_paint_result_ == static_cast<unsigned>(result));
  }

  // Used to skip PaintPhaseDescendantOutlinesOnly for layers that have never
  // had descendant outlines.  The flag is set during paint invalidation on a
  // self painting layer if any contained object has outline.  It's cleared
  // during painting if PaintPhaseDescendantOutlinesOnly painted nothing.
  // For more details, see core/paint/REAME.md#Empty paint phase optimization.
  bool NeedsPaintPhaseDescendantOutlines() const {
    return needs_paint_phase_descendant_outlines_ &&
           !previous_paint_phase_descendant_outlines_was_empty_;
  }
  void SetNeedsPaintPhaseDescendantOutlines() {
    DCHECK(IsSelfPaintingLayer());
    needs_paint_phase_descendant_outlines_ = true;
    previous_paint_phase_descendant_outlines_was_empty_ = false;
  }
  void SetPreviousPaintPhaseDescendantOutlinesEmpty(bool is_empty) {
    previous_paint_phase_descendant_outlines_was_empty_ = is_empty;
  }

  // Similar to above, but for PaintPhaseFloat.
  bool NeedsPaintPhaseFloat() const {
    return needs_paint_phase_float_ && !previous_paint_phase_float_was_empty_;
  }
  void SetNeedsPaintPhaseFloat() {
    DCHECK(IsSelfPaintingLayer());
    needs_paint_phase_float_ = true;
    previous_paint_phase_float_was_empty_ = false;
  }
  void SetPreviousPaintPhaseFloatEmpty(bool is_empty) {
    previous_paint_phase_float_was_empty_ = is_empty;
  }

  // Similar to above, but for PaintPhaseDescendantBlockBackgroundsOnly.
  bool NeedsPaintPhaseDescendantBlockBackgrounds() const {
    return needs_paint_phase_descendant_block_backgrounds_ &&
           !previous_paint_phase_descendant_block_backgrounds_was_empty_;
  }
  void SetNeedsPaintPhaseDescendantBlockBackgrounds() {
    DCHECK(IsSelfPaintingLayer());
    needs_paint_phase_descendant_block_backgrounds_ = true;
    previous_paint_phase_descendant_block_backgrounds_was_empty_ = false;
  }
  void SetPreviousPaintPhaseDescendantBlockBackgroundsEmpty(bool is_empty) {
    previous_paint_phase_descendant_block_backgrounds_was_empty_ = is_empty;
  }

  bool DescendantHasDirectOrScrollingCompositingReason() const {
    return descendant_has_direct_or_scrolling_compositing_reason_;
  }
  void SetDescendantHasDirectOrScrollingCompositingReason(bool value) {
    descendant_has_direct_or_scrolling_compositing_reason_ = value;
  }

  void SetNeedsCompositingRequirementsUpdate();
  void ClearNeedsCompositingRequirementsUpdate() {
    descendant_may_need_compositing_requirements_update_ = false;
  }
  bool DescendantMayNeedCompositingRequirementsUpdate() const {
    return descendant_may_need_compositing_requirements_update_;
  }

  ClipRectsCache* GetClipRectsCache() const { return clip_rects_cache_.get(); }
  ClipRectsCache& EnsureClipRectsCache() const {
    if (!clip_rects_cache_)
      clip_rects_cache_ = std::make_unique<ClipRectsCache>();
    return *clip_rects_cache_;
  }
  void ClearClipRectsCache() const { clip_rects_cache_.reset(); }

  bool Has3DTransformedDescendant() const {
    DCHECK(!needs_descendant_dependent_flags_update_);
    return has3d_transformed_descendant_;
  }

  // Whether the value of isSelfPaintingLayer() changed since the last clearing
  // (which happens after the flag is chedked during compositing update).
  bool SelfPaintingStatusChanged() const {
    DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    return self_painting_status_changed_;
  }
  void ClearSelfPaintingStatusChanged() {
    DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
    self_painting_status_changed_ = false;
  }

  // Returns true if this PaintLayer should be fragmented, relative
  // to the given |compositing_layer| backing. In SPv1 mode, fragmentation
  // may not cross compositing boundaries, so this wil return false
  // if EnclosingPaginationLayer() is above |compositing_layer|.
  // If |compositing_layer| is not provided, it will be computed if necessary.
  bool ShouldFragmentCompositedBounds(
      const PaintLayer* compositing_layer = nullptr) const;

  // See
  // https://chromium.googlesource.com/chromium/src.git/+/master/third_party/blink/renderer/core/paint/README.md
  // for the definition of a replaced normal-flow stacking element.
  bool IsReplacedNormalFlowStacking() const;

  void SetNeeedsCompositingReasonsUpdate() {
    needs_compositing_reasons_update_ = true;
  }

#if DCHECK_IS_ON()
  void SetStackingParent(PaintLayerStackingNode* stacking_parent) {
    stacking_parent_ = stacking_parent;
  }
  PaintLayerStackingNode* StackingParent() { return stacking_parent_; }
  bool IsInStackingParentZOrderLists() const;
#endif

  void SetNeedsCompositingLayerAssignment();
  void ClearNeedsCompositingLayerAssignment();

  bool NeedsCompositingLayerAssignment() const {
    return needs_compositing_layer_assignment_;
  }
  bool StackingDescendantNeedsCompositingLayerAssignment() const {
    return descendant_needs_compositing_layer_assignment_;
  }

 private:
  void SetNeedsCompositingInputsUpdateInternal();

  void Update3DTransformedDescendantStatus();

  // Bounding box in the coordinates of this layer.
  LayoutRect LogicalBoundingBox() const;

  bool HasOverflowControls() const;

  enum UpdateLayerPositionBehavior { AllLayers, OnlyStickyLayers };
  void UpdateLayerPositionRecursive(UpdateLayerPositionBehavior = AllLayers,
                                    bool dirty_compositing_if_needed = true);

  void SetNextSibling(PaintLayer* next) { next_ = next; }
  void SetPreviousSibling(PaintLayer* prev) { previous_ = prev; }
  void SetFirstChild(PaintLayer* first) { first_ = first; }
  void SetLastChild(PaintLayer* last) { last_ = last; }

  void UpdateHasSelfPaintingLayerDescendant() const;

  struct HitTestRecursionData {
    const LayoutRect& rect;
    // Whether location.Intersects(rect) returns true.
    const HitTestLocation& location;
    const HitTestLocation& original_location;
    const bool intersects_location;
    HitTestRecursionData(const LayoutRect& rect_arg,
                         const HitTestLocation& location_arg,
                         const HitTestLocation& original_location_arg);
  };

  PaintLayer* HitTestLayer(PaintLayer* root_layer,
                           PaintLayer* container_layer,
                           HitTestResult&,
                           const HitTestRecursionData& recursion_data,
                           bool applied_transform,
                           HitTestingTransformState* = nullptr,
                           double* z_offset = nullptr);
  PaintLayer* HitTestLayerByApplyingTransform(
      PaintLayer* root_layer,
      PaintLayer* container_layer,
      HitTestResult&,
      const HitTestRecursionData& recursion_data,
      HitTestingTransformState* = nullptr,
      double* z_offset = nullptr,
      const LayoutPoint& translation_offset = LayoutPoint());
  PaintLayer* HitTestChildren(
      ChildrenIteration,
      PaintLayer* root_layer,
      HitTestResult&,
      const HitTestRecursionData& recursion_data,
      HitTestingTransformState*,
      double* z_offset_for_descendants,
      double* z_offset,
      HitTestingTransformState* unflattened_transform_state,
      bool depth_sort_descendants);

  HitTestingTransformState CreateLocalTransformState(
      PaintLayer* root_layer,
      PaintLayer* container_layer,
      const HitTestRecursionData& recursion_data,
      const HitTestingTransformState* container_transform_state,
      const LayoutPoint& translation_offset = LayoutPoint()) const;

  bool HitTestContents(HitTestResult&,
                       const LayoutPoint& fragment_offset,
                       const HitTestLocation&,
                       HitTestFilter) const;
  bool HitTestContentsForFragments(const PaintLayerFragments&,
                                   const LayoutPoint& offset,
                                   HitTestResult&,
                                   const HitTestLocation&,
                                   HitTestFilter,
                                   bool& inside_clip_rect) const;
  PaintLayer* HitTestTransformedLayerInFragments(PaintLayer* root_layer,
                                                 PaintLayer* container_layer,
                                                 HitTestResult&,
                                                 const HitTestRecursionData&,
                                                 HitTestingTransformState*,
                                                 double* z_offset,
                                                 ShouldRespectOverflowClipType);
  bool HitTestClippedOutByClipPath(PaintLayer* root_layer,
                                   const HitTestLocation&) const;

  bool ChildBackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const;

  bool ShouldBeSelfPaintingLayer() const;

  void UpdateStackingNode(bool needs_stacking_node);

  FilterOperations FilterOperationsIncludingReflection() const;

  bool RequiresScrollableArea() const;
  void UpdateScrollableArea();

  void MarkAncestorChainForDescendantDependentFlagsUpdate();

  bool AttemptDirectCompositingUpdate(const StyleDifference&,
                                      const ComputedStyle* old_style);
  void UpdateTransform(const ComputedStyle* old_style,
                       const ComputedStyle& new_style);

  void RemoveAncestorOverflowLayer(const PaintLayer* removed_layer);

  void UpdatePaginationRecursive(bool needs_pagination_update = false);
  void ClearPaginationRecursive();

  void SetNeedsRepaintInternal();
  void MarkCompositingContainerChainForNeedsRepaint();

  PaintLayerRareData& EnsureRareData() {
    if (!rare_data_)
      rare_data_ = std::make_unique<PaintLayerRareData>();
    return *rare_data_;
  }

  void MergeNeedsPaintPhaseFlagsFrom(const PaintLayer& layer) {
    needs_paint_phase_descendant_outlines_ |=
        layer.needs_paint_phase_descendant_outlines_;
    needs_paint_phase_float_ |= layer.needs_paint_phase_float_;
    needs_paint_phase_descendant_block_backgrounds_ |=
        layer.needs_paint_phase_descendant_block_backgrounds_;
  }

  void ExpandRectForStackingChildren(const PaintLayer& composited_layer,
                                     LayoutRect& result,
                                     PaintLayer::CalculateBoundsOptions) const;

  // The return value is in the space of |stackingParent|, if non-null, or
  // |this| otherwise.
  LayoutRect BoundingBoxForCompositingInternal(
      const PaintLayer& composited_layer,
      const PaintLayer* stacking_parent,
      CalculateBoundsOptions) const;

  FloatRect FilterReferenceBox(const FilterOperations&, float zoom) const;

  LayoutPoint LocationInternal() const;

  AncestorDependentCompositingInputs& EnsureAncestorDependentCompositingInputs()
      const {
    if (!ancestor_dependent_compositing_inputs_) {
      ancestor_dependent_compositing_inputs_ =
          std::make_unique<AncestorDependentCompositingInputs>();
    }
    return *ancestor_dependent_compositing_inputs_;
  }

  // Self-painting layer is an optimization where we avoid the heavy Layer
  // painting machinery for a Layer allocated only to handle the overflow clip
  // case.
  // FIXME(crbug.com/332791): Self-painting layer should be merged into the
  // overflow-only concept.
  unsigned is_self_painting_layer_ : 1;

  const unsigned is_root_layer_ : 1;

  unsigned has_visible_content_ : 1;
  unsigned needs_descendant_dependent_flags_update_ : 1;
  unsigned has_visible_descendant_ : 1;

#if DCHECK_IS_ON()
  unsigned needs_position_update_ : 1;
#endif

  // Set on a stacking context layer that has 3D descendants anywhere
  // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
  unsigned has3d_transformed_descendant_ : 1;

  unsigned contains_dirty_overlay_scrollbars_ : 1;

  unsigned needs_ancestor_dependent_compositing_inputs_update_ : 1;
  unsigned child_needs_compositing_inputs_update_ : 1;

  // Used only while determining what layers should be composited. Applies to
  // the tree of z-order lists.
  unsigned has_compositing_descendant_ : 1;

  // Should be for stacking contexts having unisolated blending descendants.
  unsigned should_isolate_composited_descendants_ : 1;

  // True if this layout layer just lost its grouped mapping due to the
  // CompositedLayerMapping being destroyed, and we don't yet know to what
  // graphics layer this Layer will be assigned.
  unsigned lost_grouped_mapping_ : 1;

  unsigned needs_repaint_ : 1;
  unsigned previous_paint_result_ : 1;  // PaintResult
  static_assert(kMaxPaintResult <= 2,
                "Should update number of bits of previous_paint_result_");

  unsigned needs_paint_phase_descendant_outlines_ : 1;
  unsigned previous_paint_phase_descendant_outlines_was_empty_ : 1;
  unsigned needs_paint_phase_float_ : 1;
  unsigned previous_paint_phase_float_was_empty_ : 1;
  unsigned needs_paint_phase_descendant_block_backgrounds_ : 1;
  unsigned previous_paint_phase_descendant_block_backgrounds_was_empty_ : 1;

  // These bitfields are part of ancestor/descendant dependent compositing
  // inputs.
  unsigned has_descendant_with_clip_path_ : 1;
  unsigned has_non_isolated_descendant_with_blend_mode_ : 1;
  unsigned has_fixed_position_descendant_ : 1;
  unsigned has_sticky_position_descendant_ : 1;
  unsigned has_non_contained_absolute_position_descendant_ : 1;

  unsigned self_painting_status_changed_ : 1;

  // It's set to true when filter style or filter resource changes, indicating
  // that we need to update the filter field of the effect paint property node.
  // It's cleared when the effect paint property node is updated.
  unsigned filter_on_effect_node_dirty_ : 1;

  // True if the current subtree is underneath a LayoutSVGHiddenContainer
  // ancestor.
  unsigned is_under_svg_hidden_container_ : 1;

  unsigned descendant_has_direct_or_scrolling_compositing_reason_ : 1;
  unsigned needs_compositing_reasons_update_ : 1;

  unsigned descendant_may_need_compositing_requirements_update_ : 1;
  unsigned needs_compositing_layer_assignment_ : 1;
  unsigned descendant_needs_compositing_layer_assignment_ : 1;

  unsigned has_self_painting_layer_descendant_ : 1;
  unsigned is_non_stacked_with_in_flow_stacked_descendant_ : 1;

  LayoutBoxModelObject& layout_object_;

  PaintLayer* parent_;
  PaintLayer* previous_;
  PaintLayer* next_;
  PaintLayer* first_;
  PaintLayer* last_;

  // Our (x,y) coordinates are in our containing layer's coordinate space.
  LayoutPoint location_;

  // The layer's size.
  //
  // If the associated LayoutBoxModelObject is a LayoutBox, it's its border
  // box. Otherwise, this is the LayoutInline's lines' bounding box.
  LayoutSize size_;

  // Cached normal flow values for absolute positioned elements with static
  // left/top values.
  LayoutUnit static_inline_position_;
  LayoutUnit static_block_position_;

  // The first ancestor having a non visible overflow.
  const PaintLayer* ancestor_overflow_layer_;

  mutable std::unique_ptr<AncestorDependentCompositingInputs>
      ancestor_dependent_compositing_inputs_;

  Persistent<PaintLayerScrollableArea> scrollable_area_;

  mutable std::unique_ptr<ClipRectsCache> clip_rects_cache_;

  std::unique_ptr<PaintLayerStackingNode> stacking_node_;

  LayoutRect previous_paint_dirty_rect_;

  std::unique_ptr<PaintLayerRareData> rare_data_;

#if DCHECK_IS_ON()
  PaintLayerStackingNode* stacking_parent_;
#endif

  FRIEND_TEST_ALL_PREFIXES(PaintLayerTest,
                           DescendantDependentFlagsStopsAtThrottledFrames);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerTest,
                           PaintLayerTransformUpdatedOnStyleTransformAnimation);

  DISALLOW_COPY_AND_ASSIGN(PaintLayer);
};

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the WebCore namespace for ease of invocation from gdb.
CORE_EXPORT void showLayerTree(const blink::PaintLayer*);
CORE_EXPORT void showLayerTree(const blink::LayoutObject*);
#endif

#endif  // Layer_h
