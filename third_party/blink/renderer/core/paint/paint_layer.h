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
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_testing_transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_static_position.h"
#include "third_party/blink/renderer/core/paint/clip_rects_cache.h"
#include "third_party/blink/renderer/core/paint/paint_layer_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_layer_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer_resource_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"
#include "third_party/blink/renderer/core/paint/paint_result.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/graphics/overlay_scrollbar_clip_behavior.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/squashing_disallowed_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

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

// Used in PaintLayerPaintOrderIterator.
enum PaintLayerIteration {
  kNegativeZOrderChildren = 1,
  // Normal flow children are not mandated by CSS 2.1 but are an artifact of
  // our implementation: we allocate PaintLayers for elements that
  // are not treated as stacking contexts and thus we need to walk them
  // during painting and hit-testing.
  kNormalFlowChildren = 1 << 1,
  kPositiveZOrderChildren = 1 << 2,

  kStackedChildren = kNegativeZOrderChildren | kPositiveZOrderChildren,
  kNormalFlowAndPositiveZOrderChildren =
      kNormalFlowChildren | kPositiveZOrderChildren,
  kAllChildren =
      kNegativeZOrderChildren | kNormalFlowChildren | kPositiveZOrderChildren
};

// FIXME: remove this once the compositing query DCHECKS are no longer hit.
class CORE_EXPORT DisableCompositingQueryAsserts {
  STACK_ALLOCATED();

 public:
  DisableCompositingQueryAsserts();
  DisableCompositingQueryAsserts(const DisableCompositingQueryAsserts&) =
      delete;
  DisableCompositingQueryAsserts& operator=(
      const DisableCompositingQueryAsserts&) = delete;

 private:
  base::AutoReset<CompositingQueryMode> disabler_;
};

struct PaintLayerRareData {
  USING_FAST_MALLOC(PaintLayerRareData);

 public:
  PaintLayerRareData();
  PaintLayerRareData(const PaintLayerRareData&) = delete;
  PaintLayerRareData& operator=(const PaintLayerRareData&) = delete;
  ~PaintLayerRareData();

  // The offset for an in-flow relative-positioned PaintLayer. This is not
  // set by any other style.
  PhysicalOffset offset_for_in_flow_rel_position;

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
  PhysicalOffset subpixel_accumulation;
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
  PaintLayer(const PaintLayer&) = delete;
  PaintLayer& operator=(const PaintLayer&) = delete;
  ~PaintLayer() override;

  // DisplayItemClient methods
  String DebugName() const final;
  DOMNodeId OwnerNodeId() const final;

  LayoutBoxModelObject& GetLayoutObject() const { return *layout_object_; }
  LayoutBox* GetLayoutBox() const {
    return layout_object_->IsBox() ? ToLayoutBox(layout_object_) : nullptr;
  }
  PaintLayer* Parent() const { return parent_; }
  PaintLayer* PreviousSibling() const { return previous_; }
  PaintLayer* NextSibling() const { return next_; }
  PaintLayer* FirstChild() const { return first_; }
  PaintLayer* LastChild() const { return last_; }

  const PaintLayer* CommonAncestor(const PaintLayer*) const;

  // TODO(wangxianzhu): Find a better name for it. 'paintContainer' might be
  // good but we can't use it for now because it conflicts with
  // PaintInfo::paintContainer.
  PaintLayer* CompositingContainer() const;
  PaintLayer* AncestorStackingContext() const;

  void AddChild(PaintLayer* new_child, PaintLayer* before_child = nullptr);
  void RemoveChild(PaintLayer*);

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

  // The physical offset from this PaintLayer to its ContainingLayer.
  // Does not include any scroll offset of the ContainingLayer. Also does not
  // include offsets for positioned elements.
  const PhysicalOffset& LocationWithoutPositionOffset() const {
#if DCHECK_IS_ON()
    DCHECK(!needs_position_update_);
#endif
    return location_without_position_offset_;
  }

  // This is the scroll offset that's actually used to display to the screen.
  // It should only be used in paint/compositing type use cases (includes hit
  // testing, intersection observer). Most other cases should use the unsnapped
  // offset from LayoutBox (for layout) or the source offset from the
  // ScrollableArea.
  LayoutSize PixelSnappedScrolledContentOffset() const;

  // FIXME: size() should DCHECK(!needs_position_update_) as well, but that
  // fails in some tests, for example, fast/repaint/clipped-relative.html.
  const LayoutSize& Size() const { return size_; }
  // TODO(crbug.com/962299): This method snaps to pixels incorrectly because
  // Location() is not the correct paint offset. It's also incorrect in flipped
  // blocks writing mode.
  IntSize PixelSnappedSize() const {
    LayoutPoint location = layout_object_->IsBox()
                               ? ToLayoutBox(layout_object_)->Location()
                               : LayoutPoint();
    return PixelSnappedIntSize(Size(), location);
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

  PaintLayer* EnclosingPaginationLayer() const {
    return rare_data_ ? rare_data_->enclosing_pagination_layer : nullptr;
  }

  void UpdateTransformationMatrix();

  bool IsStackingContextWithNegativeZOrderChildren() const {
    DCHECK(!stacking_node_ || GetLayoutObject().IsStackingContext());
    return stacking_node_ && !stacking_node_->NegZOrderList().IsEmpty();
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

  // True if this layer paints box decorations or a background. Touch-action
  // rects are painted as part of the background so these are included here.
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

  PaintLayer* EnclosingDirectlyCompositableLayer(IncludeSelfOrNot) const;

  // https://crbug.com/751768, this function can return nullptr sometimes.
  // Always check the result before using it, don't just DCHECK.
  PaintLayer* EnclosingLayerForPaintInvalidationCrossingFrameBoundaries() const;

  PaintLayer* EnclosingDirectlyCompositableLayerCrossingFrameBoundaries() const;

  bool HasAncestorWithFilterThatMovesPixels() const;

  bool CanUseConvertToLayerCoords() const {
    // These LayoutObjects have an impact on their layers without the
    // layoutObjects knowing about it.
    return !GetLayoutObject().HasTransformRelatedProperty() &&
           !GetLayoutObject().IsSVGRoot();
  }

  void ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                            PhysicalOffset&) const;
  void ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                            PhysicalRect&) const;

  // Does the same as convertToLayerCoords() when not in multicol. For multicol,
  // however, convertToLayerCoords() calculates the offset in flow-thread
  // coordinates (what the layout engine uses internally), while this method
  // calculates the visual coordinates; i.e. it figures out which column the
  // layer starts in and adds in the offset. See
  // http://www.chromium.org/developers/design-documents/multi-column-layout for
  // more info.
  PhysicalOffset VisualOffsetFromAncestor(
      const PaintLayer* ancestor_layer,
      PhysicalOffset offset = PhysicalOffset()) const;

  // Convert a bounding box from flow thread coordinates, relative to |this|, to
  // visual coordinates, relative to |ancestorLayer|.
  // See http://www.chromium.org/developers/design-documents/multi-column-layout
  // for more info on these coordinate types.  This method requires this layer
  // to be paginated; i.e. it must have an enclosingPaginationLayer().
  void ConvertFromFlowThreadToVisualBoundingBoxInAncestor(
      const PaintLayer* ancestor_layer,
      PhysicalRect&) const;

  // The hitTest() method looks for mouse events by walking layers that
  // intersect the point from front to back.
  // |hit_test_area| is the rect in the space of this PaintLayer's
  // LayoutObject to consider for hit testing.
  bool HitTest(const HitTestLocation& location,
               HitTestResult&,
               const PhysicalRect& hit_test_area);

  bool IntersectsDamageRect(const PhysicalRect& layer_bounds,
                            const PhysicalRect& damage_rect,
                            const PhysicalOffset& offset_from_root) const;

  // MaybeIncludeTransformForAncestorLayer means that a transform on
  // |ancestorLayer| may be applied to the bounding box, in particular if
  // paintsWithTransform() is true.
  enum CalculateBoundsOptions {
    kIncludeClipsAndMaybeIncludeTransformForAncestorLayer,
    kIncludeTransformsAndCompositedChildLayers,
  };

  // Bounding box relative to some ancestor layer. Pass offsetFromRoot if known.
  PhysicalRect PhysicalBoundingBox(
      const PhysicalOffset& offset_from_root) const;
  PhysicalRect PhysicalBoundingBox(const PaintLayer* ancestor_layer) const;
  PhysicalRect FragmentsBoundingBox(const PaintLayer* ancestor_layer) const;

  PhysicalRect BoundingBoxForCompositingOverlapTest() const;
  PhysicalRect BoundingBoxForCompositing() const;


  // Static position is set in parent's coordinate space.
  LayoutUnit StaticInlinePosition() const { return static_inline_position_; }
  LayoutUnit StaticBlockPosition() const { return static_block_position_; }

  void SetStaticInlinePosition(LayoutUnit position) {
    static_inline_position_ = position;
  }
  void SetStaticBlockPosition(LayoutUnit position) {
    static_block_position_ = position;
  }

  using InlineEdge = NGLogicalStaticPosition::InlineEdge;
  using BlockEdge = NGLogicalStaticPosition::BlockEdge;
  InlineEdge StaticInlineEdge() const {
    return static_cast<InlineEdge>(static_inline_edge_);
  }
  BlockEdge StaticBlockEdge() const {
    return static_cast<BlockEdge>(static_block_edge_);
  }

  void SetStaticPositionFromNG(const NGLogicalStaticPosition& position) {
    static_inline_position_ = position.offset.inline_offset;
    static_block_position_ = position.offset.block_offset;
    static_inline_edge_ = position.inline_edge;
    static_block_edge_ = position.block_edge;
  }

  NGLogicalStaticPosition GetStaticPosition() const {
    NGLogicalStaticPosition position;
    position.offset.inline_offset = static_inline_position_;
    position.offset.block_offset = static_block_position_;
    position.inline_edge = StaticInlineEdge();
    position.block_edge = StaticBlockEdge();
    return position;
  }

  PhysicalOffset SubpixelAccumulation() const;
  void SetSubpixelAccumulation(const PhysicalOffset&);

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

  bool IsAllowedToQueryCompositingInputs() const;

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
      PhysicalOffset&);

  bool PaintsWithTransparency(GlobalPaintFlags global_paint_flags) const {
    return IsTransparent() && !PaintsIntoOwnBacking(global_paint_flags);
  }

  // Returns the ScrollingCoordinator associated with this layer, if
  // any. Otherwise nullptr.
  ScrollingCoordinator* GetScrollingCoordinator();

  // Returns true if the element or any ancestor is transformed.
  bool CompositesWithTransform() const;

  bool PaintsWithTransform(GlobalPaintFlags) const;
  bool PaintsIntoOwnBacking(GlobalPaintFlags) const;
  bool PaintsIntoOwnOrGroupedBacking(GlobalPaintFlags) const;

  bool SupportsSubsequenceCaching() const;

  // Returns true if background phase is painted opaque in the given rect.
  // The query rect is given in local coordinates.
  // if |should_check_children| is true, checks non-composited stacking children
  // recursively to see if they paint opaquely over the rect.
  bool BackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&,
                                         bool should_check_children) const;

  // If the input CompositorFilterOperation is not empty, it will be populated
  // only if |filter_on_effect_node_dirty_| is true or the reference box has
  // changed. Otherwise it will be populated unconditionally.
  void UpdateCompositorFilterOperationsForFilter(
      CompositorFilterOperations& operations) const;
  void SetFilterOnEffectNodeDirty() { filter_on_effect_node_dirty_ = true; }
  void ClearFilterOnEffectNodeDirty() { filter_on_effect_node_dirty_ = false; }

  // |backdrop_filter_bounds| is an out param from both of these functions, and
  // it represents the clipping bounds for the filtered backdrop image only.
  // This rect lives in the local transform space of the containing
  // EffectPaintPropertyNode. If the input CompositorFilterOperation is not
  // empty, it will be populated only if |backdrop_filter_on_effect_node_dirty_|
  // is true or the reference box has changed. Otherwise it will be populated
  // unconditionally.
  void UpdateCompositorFilterOperationsForBackdropFilter(
      CompositorFilterOperations& operations,
      base::Optional<gfx::RRectF>* backdrop_filter_bounds) const;
  CompositorFilterOperations CreateCompositorFilterOperationsForBackdropFilter()
      const;
  void SetBackdropFilterOnEffectNodeDirty() {
    backdrop_filter_on_effect_node_dirty_ = true;
  }
  void ClearBackdropFilterOnEffectNodeDirty() {
    backdrop_filter_on_effect_node_dirty_ = false;
  }

  void SetIsUnderSVGHiddenContainer(bool value) {
    is_under_svg_hidden_container_ = value;
  }
  bool IsUnderSVGHiddenContainer() const {
    return is_under_svg_hidden_container_;
  }

  bool PaintsWithFilters() const;

  // Maps "forward" to determine which pixels in a destination rect are
  // affected by pixels in the source rect.
  // See also FilterEffect::mapRect.
  FloatRect MapRectForFilter(const FloatRect&) const;

  // Calls the above, rounding outwards.
  PhysicalRect MapRectForFilter(const PhysicalRect&) const;

  bool HasFilterThatMovesPixels() const;

  PaintLayerResourceInfo* ResourceInfo() const {
    return rare_data_ ? rare_data_->resource_info.Get() : nullptr;
  }
  PaintLayerResourceInfo& EnsureResourceInfo();

  // Filter reference box is the area over which the filter is computed, in the
  // local coordinate system of the effect node containing the filter.
  FloatRect FilterReferenceBox() const;
  FloatRect BackdropFilterReferenceBox() const;
  gfx::RRectF BackdropFilterBounds(const FloatRect& reference_box) const;

  void UpdateFilterReferenceBox();
  void UpdateFilters(const ComputedStyle* old_style,
                     const ComputedStyle& new_style);
  void UpdateBackdropFilters(const ComputedStyle* old_style,
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

  // FIXME: This should probably return a ScrollableArea but a lot of internal
  // methods are mistakenly exposed.
  PaintLayerScrollableArea* GetScrollableArea() const {
    return scrollable_area_.Get();
  }

  enum class GeometryMapperOption {
    kUseGeometryMapper,
    kDoNotUseGeometryMapper
  };

  PaintLayerClipper Clipper(GeometryMapperOption) const;

  bool ScrollsOverflow() const;

  CompositingReasons DirectCompositingReasons() const {
    return rare_data_
               ? ((rare_data_->potential_compositing_reasons_from_style |
                   rare_data_->potential_compositing_reasons_from_non_style) &
                  CompositingReason::kComboAllDirectReasons)
               : CompositingReason::kNone;
  }

  bool CanBeCompositedForDirectReasons() const;

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

    // Nearest layer that has layout containment applied to its LayoutObject.
    // Squashing is disallowed across contain layout boundaries to provide
    // better isolation.
    const PaintLayer* nearest_contained_layout_layer = nullptr;

    // These two boxes do not include any applicable scroll offset of the
    // root PaintLayer.
    IntRect clipped_absolute_bounding_box;
    IntRect unclipped_absolute_bounding_box;

    const LayoutBoxModelObject* clipping_container = nullptr;
  };
  bool NeedsVisualOverflowRecalc() const {
    return needs_visual_overflow_recalc_;
  }
  void SetNeedsVisualOverflowRecalc();
  void SetNeedsCompositingInputsUpdate(bool mark_ancestor_flags = true);

  // Notifies the Compositor if one exists that it should rebuild the graphics
  // layer tree.
  void SetNeedsGraphicsLayerRebuild();

  // Mark this PaintLayer as needing raster invalidation checking after the
  // next compositing update step.
  void SetNeedsCheckRasterInvalidation();
  bool NeedsCheckRasterInvalidation() const {
    return needs_check_raster_invalidation_;
  }
  void ClearNeedsCheckRasterInvalidation() {
    needs_check_raster_invalidation_ = false;
  }

  // This methods marks everything from this layer up to the |ancestor| argument
  // (both included).
  void SetChildNeedsCompositingInputsUpdateUpToAncestor(PaintLayer* ancestor);
  // Use this internal method only for cases during the descendant-dependent
  // tree walk.
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

  // These two do not include any applicable scroll offset of the
  // root PaintLayer, unless CompositingOptimizationsEnabled is on.
  const IntRect ClippedAbsoluteBoundingBox() const;
  const IntRect UnclippedAbsoluteBoundingBox() const;

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
  const PaintLayer* NearestContainedLayoutLayer() const {
    return GetAncestorDependentCompositingInputs()
        .nearest_contained_layout_layer;
  }
  const PaintLayer* ClipPathAncestor() const {
    return GetAncestorDependentCompositingInputs().clip_path_ancestor;
  }
  const PaintLayer* MaskAncestor() const {
    return GetAncestorDependentCompositingInputs().mask_ancestor;
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

  // See
  // PaintLayerStackingNode::layer_to_overlay_overflow_controls_painting_after_.
  bool NeedsReorderOverlayOverflowControls() const {
    return needs_reorder_overlay_overflow_controls_;
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
  PhysicalOffset ComputeOffsetFromAncestor(
      const PaintLayer& ancestor_layer) const;

  void DidUpdateScrollsOverflow();

  void AppendSingleFragmentIgnoringPagination(
      PaintLayerFragments&,
      const PaintLayer* root_layer,
      const CullRect* cull_rect,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize,
      ShouldRespectOverflowClipType = kRespectOverflowClip,
      const PhysicalOffset* offset_from_root = nullptr,
      const PhysicalOffset& sub_pixel_accumulation = PhysicalOffset()) const;

  void CollectFragments(
      PaintLayerFragments&,
      const PaintLayer* root_layer,
      const CullRect* cull_rect,
      OverlayScrollbarClipBehavior = kIgnoreOverlayScrollbarSize,
      ShouldRespectOverflowClipType = kRespectOverflowClip,
      const PhysicalOffset* offset_from_root = nullptr,
      const PhysicalOffset& sub_pixel_accumulation = PhysicalOffset()) const;

  bool SelfNeedsRepaint() const { return self_needs_repaint_; }
  bool DescendantNeedsRepaint() const { return descendant_needs_repaint_; }
  bool SelfOrDescendantNeedsRepaint() const {
    return self_needs_repaint_ || descendant_needs_repaint_;
  }
  void SetNeedsRepaint();
  void ClearNeedsRepaintRecursively();

  // These previousXXX() functions are for subsequence caching. They save the
  // painting status of the layer during the previous painting with subsequence.
  // A painting without subsequence [1] doesn't change this status.  [1] See
  // shouldCreateSubsequence() in PaintLayerPainter.cpp for the cases we use
  // subsequence when painting a PaintLayer.

  CullRect PreviousCullRect() const { return previous_cull_rect_; }
  void SetPreviousCullRect(const CullRect& rect) { previous_cull_rect_ = rect; }

  PaintResult PreviousPaintResult() const {
    return static_cast<PaintResult>(previous_paint_result_);
  }
  void SetPreviousPaintResult(PaintResult result) {
    previous_paint_result_ = static_cast<unsigned>(result);
    DCHECK(previous_paint_result_ == static_cast<unsigned>(result));
  }

  // Used to skip PaintPhaseDescendantOutlinesOnly for layers that have never
  // had descendant outlines.  The flag is set during paint invalidation on a
  // self painting layer if any contained object has outline.
  // For more details, see core/paint/REAME.md#Empty paint phase optimization.
  bool NeedsPaintPhaseDescendantOutlines() const {
    return needs_paint_phase_descendant_outlines_;
  }
  void SetNeedsPaintPhaseDescendantOutlines() {
    DCHECK(IsSelfPaintingLayer());
    needs_paint_phase_descendant_outlines_ = true;
  }

  // Similar to above, but for PaintPhaseFloat.
  bool NeedsPaintPhaseFloat() const { return needs_paint_phase_float_; }
  void SetNeedsPaintPhaseFloat() {
    DCHECK(IsSelfPaintingLayer());
    needs_paint_phase_float_ = true;
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
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    return self_painting_status_changed_;
  }
  void ClearSelfPaintingStatusChanged() {
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
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

  void SetNeedsCompositingReasonsUpdate() {
    needs_compositing_reasons_update_ = true;
  }

#if DCHECK_IS_ON()
  void SetStackingParent(PaintLayerStackingNode* stacking_parent) {
    stacking_parent_ = stacking_parent;
  }
  PaintLayerStackingNode* StackingParent() { return stacking_parent_; }
  bool IsInStackingParentZOrderLists() const;
  bool LayerListMutationAllowed() const { return layer_list_mutation_allowed_; }
#endif

  void SetNeedsCompositingLayerAssignment();
  void ClearNeedsCompositingLayerAssignment();
  void PropagateDescendantNeedsCompositingLayerAssignment();

  bool NeedsCompositingLayerAssignment() const {
    return needs_compositing_layer_assignment_;
  }
  bool StackingDescendantNeedsCompositingLayerAssignment() const {
    return descendant_needs_compositing_layer_assignment_;
  }

  void DirtyStackingContextZOrderLists();

  PhysicalOffset OffsetForInFlowRelPosition() const {
    return rare_data_ ? rare_data_->offset_for_in_flow_rel_position
                      : PhysicalOffset();
  }

  bool NeedsPaintOffsetTranslationForCompositing() const {
    DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
    DCHECK(IsAllowedToQueryCompositingInputs());
    return needs_paint_offset_translation_for_compositing_;
  }

  void SetNeedsPaintOffsetTranslationForCompositing(bool b) {
    needs_paint_offset_translation_for_compositing_ = b;
  }

 private:
  bool PaintsWithDirectReasonIntoOwnBacking(GlobalPaintFlags) const;

  void SetNeedsCompositingInputsUpdateInternal();

  void Update3DTransformedDescendantStatus();

  // Bounding box in the coordinates of this layer.
  PhysicalRect LocalBoundingBox() const;

  bool HasOverflowControls() const;

  void UpdateLayerPositionRecursive();

  void SetNextSibling(PaintLayer* next) { next_ = next; }
  void SetPreviousSibling(PaintLayer* prev) { previous_ = prev; }
  void SetFirstChild(PaintLayer* first) { first_ = first; }
  void SetLastChild(PaintLayer* last) { last_ = last; }

  void UpdateHasSelfPaintingLayerDescendant() const;

  struct HitTestRecursionData {
    const PhysicalRect& rect;
    // Whether location.Intersects(rect) returns true.
    const HitTestLocation& location;
    const HitTestLocation& original_location;
    const bool intersects_location;
    HitTestRecursionData(const PhysicalRect& rect_arg,
                         const HitTestLocation& location_arg,
                         const HitTestLocation& original_location_arg);
  };

  PaintLayer* HitTestLayer(PaintLayer* root_layer,
                           PaintLayer* container_layer,
                           HitTestResult&,
                           const HitTestRecursionData& recursion_data,
                           bool applied_transform,
                           HitTestingTransformState* = nullptr,
                           double* z_offset = nullptr,
                           bool check_resizer_only = false);
  PaintLayer* HitTestLayerByApplyingTransform(
      PaintLayer* root_layer,
      PaintLayer* container_layer,
      HitTestResult&,
      const HitTestRecursionData& recursion_data,
      HitTestingTransformState*,
      double* z_offset,
      bool check_resizer_only,
      const PhysicalOffset& translation_offset = PhysicalOffset());
  PaintLayer* HitTestChildren(
      PaintLayerIteration,
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
      const PhysicalOffset& translation_offset = PhysicalOffset()) const;

  bool HitTestContents(HitTestResult&,
                       const NGPhysicalBoxFragment*,
                       const PhysicalOffset& fragment_offset,
                       const HitTestLocation&,
                       HitTestFilter) const;
  bool HitTestContentsForFragments(const PaintLayerFragments&,
                                   const PhysicalOffset& offset,
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
                                                 bool check_resizer_only,
                                                 ShouldRespectOverflowClipType);
  bool HitTestClippedOutByClipPath(PaintLayer* root_layer,
                                   const HitTestLocation&) const;

  bool ChildBackgroundIsKnownToBeOpaqueInRect(const PhysicalRect&) const;

  bool ShouldBeSelfPaintingLayer() const;

  void UpdateStackingNode();

  FilterOperations FilterOperationsIncludingReflection() const;

  bool RequiresScrollableArea() const;
  void UpdateScrollableArea();

  // Indicates whether the descendant-dependent tree walk bit should also
  // be set.
  enum DescendantDependentFlagsUpdateFlag {
    NeedsDescendantDependentUpdate,
    DoesNotNeedDescendantDependentUpdate
  };

  // Marks the ancestor chain for paint property update, and if
  // the flag is set, the descendant-dependent tree walk as well.
  void MarkAncestorChainForFlagsUpdate(
      DescendantDependentFlagsUpdateFlag = NeedsDescendantDependentUpdate);

  bool AttemptDirectCompositingUpdate(const StyleDifference&,
                                      const ComputedStyle* old_style);
  void UpdateTransform(const ComputedStyle* old_style,
                       const ComputedStyle& new_style);

  void RemoveAncestorOverflowLayer(const PaintLayer* removed_layer);

  void UpdatePaginationRecursive(bool needs_pagination_update = false);
  void ClearPaginationRecursive();

  void SetSelfNeedsRepaint();
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
  }

  void ExpandRectForStackingChildren(const PaintLayer& composited_layer,
                                     PhysicalRect& result,
                                     PaintLayer::CalculateBoundsOptions) const;

  // The return value is in the space of |stackingParent|, if non-null, or
  // |this| otherwise.
  PhysicalRect BoundingBoxForCompositingInternal(
      const PaintLayer& composited_layer,
      const PaintLayer* stacking_parent,
      CalculateBoundsOptions) const;
  bool ShouldApplyTransformToBoundingBox(const PaintLayer& composited_layer,
                                         CalculateBoundsOptions) const;

  AncestorDependentCompositingInputs& EnsureAncestorDependentCompositingInputs()
      const {
    if (!ancestor_dependent_compositing_inputs_) {
      ancestor_dependent_compositing_inputs_ =
          std::make_unique<AncestorDependentCompositingInputs>();
    }
    return *ancestor_dependent_compositing_inputs_;
  }

  // This is private because PaintLayerStackingNode is only for PaintLayer and
  // PaintLayerPaintOrderIterator.
  PaintLayerStackingNode* StackingNode() const { return stacking_node_.get(); }

  void SetNeedsReorderOverlayOverflowControls(bool b) {
    needs_reorder_overlay_overflow_controls_ = b;
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
  unsigned needs_visual_overflow_recalc_ : 1;

  unsigned has_visible_descendant_ : 1;

#if DCHECK_IS_ON()
  unsigned needs_position_update_ : 1;
#endif

  // Set on a stacking context layer that has 3D descendants anywhere
  // in a preserves3D hierarchy. Hint to do 3D-aware hit testing.
  unsigned has3d_transformed_descendant_ : 1;

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

  unsigned self_needs_repaint_ : 1;
  unsigned descendant_needs_repaint_ : 1;
  unsigned previous_paint_result_ : 1;  // PaintResult
  static_assert(kMaxPaintResult <= 2,
                "Should update number of bits of previous_paint_result_");

  unsigned needs_paint_phase_descendant_outlines_ : 1;
  unsigned needs_paint_phase_float_ : 1;

  // These bitfields are part of ancestor/descendant dependent compositing
  // inputs.
  unsigned has_non_isolated_descendant_with_blend_mode_ : 1;
  unsigned has_fixed_position_descendant_ : 1;
  unsigned has_sticky_position_descendant_ : 1;
  unsigned has_non_contained_absolute_position_descendant_ : 1;
  unsigned has_stacked_descendant_in_current_stacking_context_ : 1;

  unsigned self_painting_status_changed_ : 1;

  // These are set to true when filter style or filter resource changes,
  // indicating that we need to update the filter (or backdrop_filter) field of
  // the effect paint property node. They are cleared when the effect paint
  // property node is updated.
  unsigned filter_on_effect_node_dirty_ : 1;
  unsigned backdrop_filter_on_effect_node_dirty_ : 1;

  // True if the current subtree is underneath a LayoutSVGHiddenContainer
  // ancestor.
  unsigned is_under_svg_hidden_container_ : 1;

  unsigned descendant_has_direct_or_scrolling_compositing_reason_ : 1;
  unsigned needs_compositing_reasons_update_ : 1;

  unsigned descendant_may_need_compositing_requirements_update_ : 1;
  unsigned needs_compositing_layer_assignment_ : 1;
  unsigned descendant_needs_compositing_layer_assignment_ : 1;

  unsigned has_self_painting_layer_descendant_ : 1;

  unsigned needs_reorder_overlay_overflow_controls_ : 1;
  unsigned static_inline_edge_ : 2;
  unsigned static_block_edge_ : 2;

  unsigned needs_paint_offset_translation_for_compositing_ : 1;

  unsigned needs_check_raster_invalidation_ : 1;

#if DCHECK_IS_ON()
  mutable unsigned layer_list_mutation_allowed_ : 1;
#endif

  LayoutBoxModelObject* const layout_object_;

  PaintLayer* parent_;
  PaintLayer* previous_;
  PaintLayer* next_;
  PaintLayer* first_;
  PaintLayer* last_;

  // Our (x,y) coordinates are in our containing layer's coordinate space,
  // excluding positioning offset and scroll.
  PhysicalOffset location_without_position_offset_;

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

  CullRect previous_cull_rect_;

  std::unique_ptr<PaintLayerRareData> rare_data_;

#if DCHECK_IS_ON()
  PaintLayerStackingNode* stacking_parent_;
#endif

  // For layer_list_mutation_allowed_.
  friend class PaintLayerListMutationDetector;

  // For stacking_node_ to avoid exposing it publicly.
  friend class PaintLayerPaintOrderIterator;
  friend class PaintLayerPaintOrderReverseIterator;
  friend class PaintLayerStackingNode;

  FRIEND_TEST_ALL_PREFIXES(PaintLayerTest,
                           DescendantDependentFlagsStopsAtThrottledFrames);
  FRIEND_TEST_ALL_PREFIXES(PaintLayerTest,
                           PaintLayerTransformUpdatedOnStyleTransformAnimation);
};

#if DCHECK_IS_ON()
class PaintLayerListMutationDetector {
  STACK_ALLOCATED();

 public:
  explicit PaintLayerListMutationDetector(const PaintLayer& layer)
      : layer_(layer),
        previous_mutation_allowed_state_(layer.layer_list_mutation_allowed_) {
    layer.layer_list_mutation_allowed_ = false;
  }

  ~PaintLayerListMutationDetector() {
    layer_.layer_list_mutation_allowed_ = previous_mutation_allowed_state_;
  }

 private:
  const PaintLayer& layer_;
  bool previous_mutation_allowed_state_;
};
#endif

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
CORE_EXPORT void showLayerTree(const blink::PaintLayer*);
CORE_EXPORT void showLayerTree(const blink::LayoutObject*);
#endif

#endif  // Layer_h
