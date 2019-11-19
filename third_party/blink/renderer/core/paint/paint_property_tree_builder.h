// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FragmentData;
class LayoutObject;
class LayoutNGTableSectionInterface;
class LocalFrameView;
class PaintLayer;
class VisualViewport;

// The context for PaintPropertyTreeBuilder.
// It's responsible for bookkeeping tree state in other order, for example, the
// most recent position container seen.
struct PaintPropertyTreeBuilderFragmentContext {
  USING_FAST_MALLOC(PaintPropertyTreeBuilderFragmentContext);

 public:
  // Initializes all property tree nodes to the roots.
  PaintPropertyTreeBuilderFragmentContext();

  // State that propagates on the containing block chain (and so is adjusted
  // when an absolute or fixed position object is encountered).
  struct ContainingBlockContext {
    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected
    // to refer the object's border box, then the callee will derive its own
    // border box by translating the space with its own layout location.
    const TransformPaintPropertyNode* transform = nullptr;
    // Corresponds to FragmentData::PaintOffset, which does not include
    // fragmentation offsets. See FragmentContext for the fragmented version.
    PhysicalOffset paint_offset;
    // The PaintLayer corresponding to the origin of |paint_offset|.
    const LayoutObject* paint_offset_root = nullptr;
    // Whether newly created children should flatten their inherited transform
    // (equivalently, draw into the plane of their parent). Should generally
    // be updated whenever |transform| is; flattening only needs to happen
    // to immediate children.
    bool should_flatten_inherited_transform = false;

    // Rendering context for 3D sorting. See
    // TransformPaintPropertyNode::renderingContextId.
    unsigned rendering_context_id = 0;
    // The clip node describes the accumulated raster clip for the current
    // subtree.  Note that the computed raster region in canvas space for a clip
    // node is independent from the transform and paint offset above. Also the
    // actual raster region may be affected by layerization and occlusion
    // tracking.
    const ClipPaintPropertyNode* clip = nullptr;
    // The scroll node contains information for scrolling such as the parent
    // scroll space, the extent that can be scrolled, etc. Because scroll nodes
    // reference a scroll offset transform, scroll nodes should be updated if
    // the transform tree changes.
    const ScrollPaintPropertyNode* scroll = nullptr;

    // True if any fixed-position children within this context are fixed to the
    // root of the FrameView (and hence above its scroll).
    bool fixed_position_children_fixed_to_root = false;
  };

  ContainingBlockContext current;

  // Separate context for out-of-flow positioned and fixed positioned elements
  // are needed because they don't use DOM parent as their containing block.
  // These additional contexts normally pass through untouched, and are only
  // copied from the main context when the current element serves as the
  // containing block of corresponding positioned descendants.  Overflow clips
  // are also inherited by containing block tree instead of DOM tree, thus they
  // are included in the additional context too.
  ContainingBlockContext absolute_position;

  ContainingBlockContext fixed_position;

  // This is the same as current.paintOffset except when a floating object has
  // non-block ancestors under its containing block. Paint offsets of the
  // non-block ancestors should not be accumulated for the floating object.
  PhysicalOffset paint_offset_for_float;

  // The effect hierarchy is applied by the stacking context tree. It is
  // guaranteed that every DOM descendant is also a stacking context descendant.
  // Therefore, we don't need extra bookkeeping for effect nodes and can
  // generate the effect tree from a DOM-order traversal.
  const EffectPaintPropertyNode* current_effect;

  // If the object is a flow thread, this records the clip rect for this
  // fragment.
  base::Optional<PhysicalRect> fragment_clip;

  // If the object is fragmented, this records the logical top of this fragment
  // in the flow thread.
  LayoutUnit logical_top_in_flow_thread;

  // A repeating object paints at multiple places, once in each fragment.
  // The repeated paintings need to add an adjustment to the calculated paint
  // offset to paint at the desired place.
  PhysicalOffset repeating_paint_offset_adjustment;

  FloatSize paint_offset_delta;
  PhysicalOffset old_paint_offset;
};

struct PaintPropertyTreeBuilderContext {
  DISALLOW_NEW();

 public:
  PaintPropertyTreeBuilderContext();

  Vector<PaintPropertyTreeBuilderFragmentContext, 1> fragments;
  const LayoutObject* container_for_absolute_position = nullptr;
  const LayoutObject* container_for_fixed_position = nullptr;

  // The physical bounding box of all appearances of the repeating table section
  // in the flow thread or the paged LayoutView.
  PhysicalRect repeating_table_section_bounding_box;

#if DCHECK_IS_ON()
  // When DCHECK_IS_ON() we create PaintPropertyTreeBuilderContext even if not
  // needed. See FindPaintOffsetAndVisualRectNeedingUpdate.h.
  bool is_actually_needed = true;
#endif

  PaintLayer* painting_layer = nullptr;

  // In a fragmented context, repeating table headers and footers and their
  // descendants in paint order repeatedly paint in all fragments after the
  // fragment where the object first appears.
  const LayoutNGTableSectionInterface* repeating_table_section = nullptr;

  // Specifies the reason the subtree update was forced. For simplicity, this
  // only categorizes it into two categories:
  // - Isolation piercing, meaning that the update is required for subtrees
  //   under an isolation boundary.
  // - Isolation blocked, meaning that the recursion can be blocked by
  //   isolation.
  enum SubtreeUpdateReason : unsigned {
    kSubtreeUpdateIsolationPiercing = 1 << 0,
    kSubtreeUpdateIsolationBlocked = 1 << 1
  };

  // True if a change has forced all properties in a subtree to be updated. This
  // can be set due to paint offset changes or when the structure of the
  // property tree changes (i.e., a node is added or removed).
  unsigned force_subtree_update_reasons : 2;

  // Note that the next four bitfields are conceptually bool, but are declared
  // as unsigned in order to be packed in the same word as the above bitfield.

  // Whether a clip paint property node appeared, disappeared, or changed
  // its clip since this variable was last set to false. This is used
  // to find out whether a clip changed since the last transform update.
  // Code outside of this class resets clip_changed to false when transforms
  // change.
  unsigned clip_changed : 1;

  // When printing, fixed-position objects and their descendants need to repeat
  // in each page.
  unsigned is_repeating_fixed_position : 1;

  // True if the current subtree is underneath a LayoutSVGHiddenContainer
  // ancestor.
  unsigned has_svg_hidden_container_ancestor : 1;

  // Whether composited raster invalidation is supported for this object.
  // If not, subtree invalidations occur on every property tree change.
  unsigned supports_composited_raster_invalidation : 1;

  // This is always recalculated in PaintPropertyTreeBuilder::UpdateForSelf()
  // which overrides the inherited value.
  CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
};

class VisualViewportPaintPropertyTreeBuilder {
  STATIC_ONLY(VisualViewportPaintPropertyTreeBuilder);

 public:
  // Update the paint properties for the visual viewport and ensure the context
  // is up to date. Returns the maximum paint property change type for any of
  // the viewport nodes.
  static PaintPropertyChangeType Update(VisualViewport&,
                                        PaintPropertyTreeBuilderContext&);
};

// Creates paint property tree nodes for non-local effects in the layout tree.
// Non-local effects include but are not limited to: overflow clip, transform,
// fixed-pos, animation, mask, filters, etc. It expects to be invoked for each
// layout tree node in DOM order during the PrePaint lifecycle phase.
class PaintPropertyTreeBuilder {
  STACK_ALLOCATED();

 public:
  static void SetupContextForFrame(LocalFrameView&,
                                   PaintPropertyTreeBuilderContext&);

  PaintPropertyTreeBuilder(const LayoutObject& object,
                           PaintPropertyTreeBuilderContext& context)
      : object_(object), context_(context) {}

  // Update the paint properties that affect this object (e.g., properties like
  // paint offset translation) and ensure the context is up to date. Also
  // handles updating the object's paintOffset.
  // Returns whether any paint property of the object has changed.
  PaintPropertyChangeType UpdateForSelf();

  // Update the paint properties that affect children of this object (e.g.,
  // scroll offset transform) and ensure the context is up to date.
  // Returns whether any paint property of the object has changed.
  PaintPropertyChangeType UpdateForChildren();

 private:
  ALWAYS_INLINE void InitFragmentPaintProperties(
      FragmentData&,
      bool needs_paint_properties,
      const PhysicalOffset& pagination_offset = PhysicalOffset(),
      LayoutUnit logical_top_in_flow_thread = LayoutUnit());
  ALWAYS_INLINE void InitSingleFragmentFromParent(bool needs_paint_properties);
  ALWAYS_INLINE bool ObjectTypeMightNeedMultipleFragmentData() const;
  ALWAYS_INLINE bool ObjectTypeMightNeedPaintProperties() const;
  ALWAYS_INLINE void UpdateCompositedLayerPaginationOffset();
  ALWAYS_INLINE PaintPropertyTreeBuilderFragmentContext
  ContextForFragment(const base::Optional<PhysicalRect>& fragment_clip,
                     LayoutUnit logical_top_in_flow_thread) const;
  ALWAYS_INLINE void CreateFragmentContextsInFlowThread(
      bool needs_paint_properties);
  ALWAYS_INLINE bool IsRepeatingInPagedMedia() const;
  ALWAYS_INLINE bool ObjectIsRepeatingTableSectionInPagedMedia() const;
  ALWAYS_INLINE void CreateFragmentContextsForRepeatingFixedPosition();
  ALWAYS_INLINE void
  CreateFragmentContextsForRepeatingTableSectionInPagedMedia();
  ALWAYS_INLINE void CreateFragmentDataForRepeatingInPagedMedia(
      bool needs_paint_properties);
  // Returns whether ObjectPaintProperties were allocated or deleted.
  ALWAYS_INLINE bool UpdateFragments();
  ALWAYS_INLINE void UpdatePaintingLayer();
  ALWAYS_INLINE void UpdateRepeatingTableSectionPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableHeaderPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableFooterPaintOffsetAdjustment();

  const LayoutObject& object_;
  PaintPropertyTreeBuilderContext& context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_
