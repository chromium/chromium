// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/effect_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FragmentData;
class LayoutObject;
class LayoutNGTableSectionInterface;
class LocalFrameView;
class NGPhysicalBoxFragment;
class PaintLayer;
class VisualViewport;

// The context for PaintPropertyTreeBuilder.
// It's responsible for bookkeeping tree state in other order, for example, the
// most recent position container seen.
struct PaintPropertyTreeBuilderFragmentContext {
  DISALLOW_NEW();

 public:
  // Initializes all property tree nodes to the roots.
  PaintPropertyTreeBuilderFragmentContext();

  void Trace(Visitor*) const;

  // State that propagates on the containing block chain (and so is adjusted
  // when an absolute or fixed position object is encountered).
  struct ContainingBlockContext {
    DISALLOW_NEW();

   public:
    void Trace(Visitor*) const;

    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected
    // to refer the object's border box, then the callee will derive its own
    // border box by translating the space with its own layout location.
    const TransformPaintPropertyNodeOrAlias* transform = nullptr;
    // Corresponds to FragmentData::PaintOffset, which does not include
    // fragmentation offsets. See FragmentContext for the fragmented version.
    PhysicalOffset paint_offset;

    // "Additional offset to layout shift root" is the accumulation of paint
    // offsets encoded in PaintOffsetTranslations between the local transform
    // space and the layout shift root. The layout shift root is the nearest
    // ancestor with
    // - a transform node that is one of:
    //   * the transform property tree state of the containing LayoutView
    //   * a transform that is not identity or 2d translation
    //   * a replaced contents transform
    //   * a transform isolation node
    //   * a paint offset translation for a sticky or fixed position element
    // - or an overflow clip node.
    // The offset plus paint_offset is the offset for layout shift tracking.
    // It doesn't include transforms because we need to ignore transform changes
    // for layout shift tracking, see
    //    https://github.com/WICG/layout-instability#transform-changes
    // This field is the diff between the new and the old additional offsets to
    // layout shift root.
    PhysicalOffset additional_offset_to_layout_shift_root_delta;

    // Similar to additional_offset_to_layout_shift_root_delta but for scroll
    // offsets.
    gfx::Vector2dF scroll_offset_to_layout_shift_root_delta;

    // For paint invalidation optimization for subpixel movement under
    // composited layer. It's reset to zero if subpixel can't be propagated
    // thus the optimization is not applicable (e.g. when crossing a
    // non-translation transform).
    PhysicalOffset directly_composited_container_paint_offset_subpixel_delta;

    // The PaintLayer corresponding to the origin of |paint_offset|.
    Member<const LayoutObject> paint_offset_root;

    // True if any fixed-position children within this context are fixed to the
    // root of the FrameView (and hence above its scroll).
    bool fixed_position_children_fixed_to_root = false;

    // True if the layout shift root (see
    // additional_offset_to_layout_shift_root_delta for the definition) of this
    // object has changed.
    bool layout_shift_root_changed = false;

    bool is_in_block_fragmentation = false;

    // The clip node describes the accumulated raster clip for the current
    // subtree.  Note that the computed raster region in canvas space for a clip
    // node is independent from the transform and paint offset above. Also the
    // actual raster region may be affected by layerization and occlusion
    // tracking.
    const ClipPaintPropertyNodeOrAlias* clip = nullptr;
    // The scroll node contains information for scrolling such as the parent
    // scroll space, the extent that can be scrolled, etc. Because scroll nodes
    // reference a scroll offset transform, scroll nodes should be updated if
    // the transform tree changes.
    const ScrollPaintPropertyNode* scroll = nullptr;

    gfx::Vector2dF pending_scroll_anchor_adjustment;

    // Paint offset of the innermost fragmentainer minus accumulated offsets
    // that are baked in PaintOffsetTranslations since we entered the
    // fragmentainer.
    PhysicalOffset paint_offset_for_oof_in_fragmentainer;

    // The fragmentainer index of the nearest ancestor that participates in
    // block fragmentation. This is updated as we update properties for an
    // object that participates in block fragmentation. If we enter monolithic
    // content (legacy or NG), the index will be kept and inherited down the
    // tree, so that we eventually set the correct "NG" fragment index in the
    // FragmentData object, rather than using a bogus legacy flow-thread offset.
    wtf_size_t fragmentainer_idx = WTF::kNotFound;
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
  const EffectPaintPropertyNodeOrAlias* current_effect;
  bool this_or_ancestor_opacity_is_zero = false;

  // Whether newly created children should flatten their inherited transform
  // (equivalently, draw into the plane of their parent). Should generally
  // be updated whenever |transform| is; flattening only needs to happen
  // to immediate children.
  bool should_flatten_inherited_transform = false;
  // Rendering context for 3D sorting. See
  // TransformPaintPropertyNode::renderingContextId.
  unsigned rendering_context_id = 0;

  // If the object is a flow thread, this records the clip rect for this
  // fragment.
  absl::optional<PhysicalRect> fragment_clip;

  // If the object is fragmented, this records the logical top of this fragment
  // in the flow thread.
  LayoutUnit logical_top_in_flow_thread;

  // A repeating object paints at multiple places, once in each fragment.
  // The repeated paintings need to add an adjustment to the calculated paint
  // offset to paint at the desired place.
  PhysicalOffset repeating_paint_offset_adjustment;

  PhysicalOffset old_paint_offset;

  // An additional offset that applies to the current fragment, but is detected
  // *before* the ContainingBlockContext is updated for it. Once the
  // ContainingBlockContext is set, this value should be added to
  // ContainingBlockContext::additional_offset_to_layout_shift_root_delta.
  PhysicalOffset pending_additional_offset_to_layout_shift_root_delta;

  // The delta between the old and new accumulated offsets of 2d translation
  // transforms to the layout shift root.
  gfx::Vector2dF translation_2d_to_layout_shift_root_delta;
};

struct PaintPropertyTreeBuilderContext final {
  STACK_ALLOCATED();

 public:
  PaintPropertyTreeBuilderContext();
  PaintPropertyTreeBuilderContext(const PaintPropertyTreeBuilderContext&) =
      default;
  ~PaintPropertyTreeBuilderContext();

  HeapVector<PaintPropertyTreeBuilderFragmentContext, 1> fragments;

  const LayoutObject* container_for_absolute_position = nullptr;
  const LayoutObject* container_for_fixed_position = nullptr;

  // The physical bounding box of all appearances of the repeating table section
  // in the flow thread or the paged LayoutView.
  PhysicalRect repeating_table_section_bounding_box;

#if DCHECK_IS_ON()
  // When DCHECK_IS_ON() and RuntimeEnabledFeatures::
  // PaintUnderInvalidationCheckingEnabled(), we create
  // PaintPropertyTreeBuilderContext even if not needed.
  // See PrePaintTreeWalkContext constructor.
  bool is_actually_needed = true;
#endif

  PaintLayer* painting_layer = nullptr;

  // In a fragmented context, repeating table headers and footers and their
  // descendants in paint order repeatedly paint in all fragments after the
  // fragment where the object first appears.
  const LayoutNGTableSectionInterface* repeating_table_section = nullptr;

  gfx::Vector2dF old_scroll_offset;

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
  unsigned force_subtree_update_reasons : 2 = 0u;

  // Note that the following bitfields are conceptually bool, but are declared
  // as unsigned in order to be packed in the same word as the above bitfield.

  // When printing, fixed-position objects and their descendants need to repeat
  // in each page.
  unsigned is_repeating_fixed_position : 1 = false;

  // True if the current subtree is underneath a LayoutSVGHiddenContainer
  // ancestor.
  unsigned has_svg_hidden_container_ancestor : 1 = false;

  // Whether this object was a layout shift root during the previous render
  // (not this one).
  unsigned was_layout_shift_root : 1 = false;

  unsigned was_main_thread_scrolling : 1 = false;
  unsigned scroll_unification_enabled : 1 = false;

  // Main thread scrolling reasons that apply to all scrollers in the current
  // LocalFrameView subtree.
  unsigned global_main_thread_scrolling_reasons : 5 = 0;
  static constexpr MainThreadScrollingReasons
      kGlobalMainThreadScrollingReasons =
          cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects |
          cc::MainThreadScrollingReason::kThreadedScrollingDisabled |
          cc::MainThreadScrollingReason::kPopupNoThreadedInput;
  static_assert(kGlobalMainThreadScrollingReasons < (1 << 6));

  // This is always recalculated in PaintPropertyTreeBuilder::UpdateForSelf()
  // which overrides the inherited value.
  CompositingReasons direct_compositing_reasons = CompositingReason::kNone;
};

class VisualViewportPaintPropertyTreeBuilder {
  STATIC_ONLY(VisualViewportPaintPropertyTreeBuilder);

 public:
  // Update the paint properties for the visual viewport and ensure the context
  // is up to date.
  static void Update(LocalFrameView& main_frame_view,
                     VisualViewport&,
                     PaintPropertyTreeBuilderContext&);
};

struct NGPrePaintInfo {
  STACK_ALLOCATED();

 public:
  NGPrePaintInfo(const NGPhysicalBoxFragment& box_fragment,
                 PhysicalOffset paint_offset,
                 wtf_size_t fragmentainer_idx,
                 bool is_first_for_node,
                 bool is_last_for_node,
                 bool is_inside_fragment_child,
                 bool fragmentainer_is_oof_containing_block)
      : box_fragment(box_fragment),
        paint_offset(paint_offset),
        fragmentainer_idx(fragmentainer_idx),
        is_first_for_node(is_first_for_node),
        is_last_for_node(is_last_for_node),
        is_inside_fragment_child(is_inside_fragment_child),
        fragmentainer_is_oof_containing_block(
            fragmentainer_is_oof_containing_block) {}

  // The fragment for the LayoutObject currently being processed, or, in the
  // case of text and non-atomic inlines: the fragment of the containing block.
  const NGPhysicalBoxFragment& box_fragment;

  FragmentData* fragment_data = nullptr;
  PhysicalOffset paint_offset;
  wtf_size_t fragmentainer_idx;
  bool is_first_for_node;
  bool is_last_for_node;

  // True if |box_fragment| is the containing block of the LayoutObject
  // currently being processed. Otherwise, |box_fragment| is a fragment for the
  // LayoutObject itself.
  bool is_inside_fragment_child;

  // Due to how out-of-flow layout inside fragmentation works, if an out-of-flow
  // positioned element is contained by something that's part of a fragmentation
  // context (e.g. abspos in relpos in multicol) the containing block (as far as
  // NG layout is concerned) is a fragmentainer, not the relpos. Then this flag
  // is true. It's false if the containing block doesn't participate in block
  // fragmentation, e.g. if we're inside monolithic content.
  bool fragmentainer_is_oof_containing_block;
};

struct PaintPropertiesChangeInfo {
  STACK_ALLOCATED();

 public:
  PaintPropertyChangeType transform_changed =
      PaintPropertyChangeType::kUnchanged;
  PaintPropertyChangeType clip_changed = PaintPropertyChangeType::kUnchanged;
  PaintPropertyChangeType effect_changed = PaintPropertyChangeType::kUnchanged;
  PaintPropertyChangeType scroll_changed = PaintPropertyChangeType::kUnchanged;

  void Merge(const PaintPropertiesChangeInfo& other) {
    transform_changed = std::max(transform_changed, other.transform_changed);
    clip_changed = std::max(clip_changed, other.clip_changed);
    effect_changed = std::max(effect_changed, other.effect_changed);
    scroll_changed = std::max(scroll_changed, other.scroll_changed);
  }

  PaintPropertyChangeType Max() const {
    return std::max(
        {transform_changed, clip_changed, effect_changed, scroll_changed});
  }
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
                           NGPrePaintInfo* pre_paint_info,
                           PaintPropertyTreeBuilderContext& context)
      : object_(object), pre_paint_info_(pre_paint_info), context_(context) {}

  // Update the paint properties that affect this object (e.g., properties like
  // paint offset translation) and ensure the context is up to date. Also
  // handles updating the object's paintOffset.
  void UpdateForSelf();

  // Update the paint properties that affect children of this object (e.g.,
  // scroll offset transform) and ensure the context is up to date.
  void UpdateForChildren();

  void IssueInvalidationsAfterUpdate();

  bool PropertiesChanged() const {
    return properties_changed_.Max() > PaintPropertyChangeType::kUnchanged;
  }

  static void DirectlyUpdateTransformMatrix(const LayoutObject& object);
  static void DirectlyUpdateOpacityValue(const LayoutObject& object);

  static bool ScheduleDeferredTransformNodeUpdate(LayoutObject& object);
  static bool ScheduleDeferredOpacityNodeUpdate(LayoutObject& object);

 private:
  ALWAYS_INLINE void InitFragmentPaintProperties(
      FragmentData&,
      bool needs_paint_properties,
      PaintPropertyTreeBuilderFragmentContext&);
  ALWAYS_INLINE void InitFragmentPaintPropertiesForLegacy(
      FragmentData&,
      bool needs_paint_properties,
      const PhysicalOffset& pagination_offset,
      PaintPropertyTreeBuilderFragmentContext&);
  ALWAYS_INLINE void InitFragmentPaintPropertiesForNG(
      bool needs_paint_properties);
  ALWAYS_INLINE void InitSingleFragmentFromParent(bool needs_paint_properties);
  ALWAYS_INLINE bool ObjectTypeMightNeedMultipleFragmentData() const;
  ALWAYS_INLINE bool ObjectTypeMightNeedPaintProperties() const;
  ALWAYS_INLINE PaintPropertyTreeBuilderFragmentContext
  ContextForFragment(const absl::optional<PhysicalRect>& fragment_clip,
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
  ALWAYS_INLINE void UpdateFragments();
  ALWAYS_INLINE void UpdatePaintingLayer();
  ALWAYS_INLINE void UpdateRepeatingTableSectionPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableHeaderPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableFooterPaintOffsetAdjustment();
  ALWAYS_INLINE bool IsAffectedByOuterViewportBoundsDelta() const;

  ALWAYS_INLINE void UpdateGlobalMainThreadScrollingReasons();

  bool IsInNGFragmentTraversal() const { return pre_paint_info_; }
  static bool CanDoDeferredTransformNodeUpdate(const LayoutObject& object);
  static bool CanDoDeferredOpacityNodeUpdate(const LayoutObject& object);

  const LayoutObject& object_;
  NGPrePaintInfo* pre_paint_info_;
  PaintPropertyTreeBuilderContext& context_;
  PaintPropertiesChangeInfo properties_changed_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::PaintPropertyTreeBuilderFragmentContext)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_PROPERTY_TREE_BUILDER_H_
