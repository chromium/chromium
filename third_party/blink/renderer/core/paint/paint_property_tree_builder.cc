// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"

#include <memory>

#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/overscroll_behavior.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/document_transition/document_transition.h"
#include "third_party/blink/renderer/core/document_transition/document_transition_supplement.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/css_mask_painter.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_needing_update.h"
#include "third_party/blink/renderer/core/paint/find_properties_needing_update.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/svg_root_painter.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/document_transition_shared_element_id.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

namespace {

bool AreSubtreeUpdateReasonsIsolationPiercing(unsigned reasons) {
  // This is written to mean that if we have any reason other than the specified
  // ones then the reasons are isolation piercing. This means that if new
  // reasons are added, they will be isolation piercing by default.
  //  - Isolation establishes a containing block for all descendants, so it is
  //    not piercing.
  // TODO(vmpstr): Investigate if transform style is also isolated.
  return reasons &
         ~(static_cast<unsigned>(
             SubtreePaintPropertyUpdateReason::kContainerChainMayChange));
}

DocumentTransitionSharedElementId GetSharedElementId(
    const LayoutObject& object) {
  Element* element = DynamicTo<Element>(object.GetNode());
  // If we're not compositing this element for document transition, then it has
  // no shared element id.
  if (!element || !element->ShouldCompositeForDocumentTransition())
    return {};

  auto* document_transition_supplement =
      DocumentTransitionSupplement::FromIfExists(element->GetDocument());
  if (!document_transition_supplement)
    return {};
  return document_transition_supplement->GetTransition()->GetSharedElementId(
      element);
}

}  // namespace

PaintPropertyTreeBuilderFragmentContext::
    PaintPropertyTreeBuilderFragmentContext()
    : current_effect(&EffectPaintPropertyNode::Root()) {
  current.clip = absolute_position.clip = fixed_position.clip =
      &ClipPaintPropertyNode::Root();
  current.transform = absolute_position.transform = fixed_position.transform =
      &TransformPaintPropertyNode::Root();
  current.scroll = absolute_position.scroll = fixed_position.scroll =
      &ScrollPaintPropertyNode::Root();
}

PaintPropertyTreeBuilderContext::PaintPropertyTreeBuilderContext()
    : force_subtree_update_reasons(0u),
      clip_changed(false),
      is_repeating_fixed_position(false),
      has_svg_hidden_container_ancestor(false),
      supports_composited_raster_invalidation(true),
      was_layout_shift_root(false),
      was_main_thread_scrolling(false) {}

PaintPropertyChangeType VisualViewportPaintPropertyTreeBuilder::Update(
    VisualViewport& visual_viewport,
    PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.fragments.IsEmpty())
    full_context.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());

  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];

  auto property_changed =
      visual_viewport.UpdatePaintPropertyNodesIfNeeded(context);

  if (const EffectPaintPropertyNode* overscroll_elasticity_effect_node =
          visual_viewport.GetOverscrollElasticityEffectNode()) {
    context.current_effect = overscroll_elasticity_effect_node;
  }

  context.current.transform = visual_viewport.GetScrollTranslationNode();
  context.absolute_position.transform =
      visual_viewport.GetScrollTranslationNode();
  context.fixed_position.transform = visual_viewport.GetScrollTranslationNode();

  context.current.scroll = visual_viewport.GetScrollNode();
  context.absolute_position.scroll = visual_viewport.GetScrollNode();
  context.fixed_position.scroll = visual_viewport.GetScrollNode();

  if (property_changed >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
    // Force piercing subtree update for the worst case (scroll node added/
    // removed). Not a big deal for performance because this is rare.
    full_context.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
  }

#if DCHECK_IS_ON()
  paint_property_tree_printer::UpdateDebugNames(visual_viewport);
#endif
  return property_changed;
}

void PaintPropertyTreeBuilder::SetupContextForFrame(
    LocalFrameView& frame_view,
    PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.fragments.IsEmpty())
    full_context.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());

  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];
  context.current.paint_offset += PhysicalOffset(frame_view.Location());
  context.current.rendering_context_id = 0;
  context.current.should_flatten_inherited_transform = true;
  context.absolute_position = context.current;
  full_context.container_for_absolute_position = nullptr;
  full_context.container_for_fixed_position = nullptr;
  context.fixed_position = context.current;
  context.fixed_position.fixed_position_children_fixed_to_root = true;
}

namespace {

class FragmentPaintPropertyTreeBuilder {
  STACK_ALLOCATED();

 public:
  FragmentPaintPropertyTreeBuilder(
      const LayoutObject& object,
      NGPrePaintInfo* pre_paint_info,
      PaintPropertyTreeBuilderContext& full_context,
      PaintPropertyTreeBuilderFragmentContext& context,
      FragmentData& fragment_data)
      : object_(object),
        pre_paint_info_(pre_paint_info),
        full_context_(full_context),
        context_(context),
        fragment_data_(fragment_data),
        properties_(fragment_data.PaintProperties()) {}

  ~FragmentPaintPropertyTreeBuilder() {
    if (property_changed_ >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
      // Tree topology changes are blocked by isolation.
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    }
#if DCHECK_IS_ON()
    if (properties_)
      paint_property_tree_printer::UpdateDebugNames(object_, *properties_);
#endif
  }

  ALWAYS_INLINE void UpdateForSelf();
  ALWAYS_INLINE void UpdateForChildren();

  PaintPropertyChangeType PropertyChanged() const { return property_changed_; }
  bool HasIsolationNodes() const {
    // All or nothing check on the isolation nodes.
    DCHECK(!properties_ ||
           (properties_->TransformIsolationNode() &&
            properties_->ClipIsolationNode() &&
            properties_->EffectIsolationNode()) ||
           (!properties_->TransformIsolationNode() &&
            !properties_->ClipIsolationNode() &&
            !properties_->EffectIsolationNode()));
    return properties_ && properties_->TransformIsolationNode();
  }

 private:
  ALWAYS_INLINE bool CanPropagateSubpixelAccumulation() const;
  ALWAYS_INLINE void UpdatePaintOffset();
  ALWAYS_INLINE void UpdateForPaintOffsetTranslation(absl::optional<IntPoint>&);
  ALWAYS_INLINE void UpdatePaintOffsetTranslation(
      const absl::optional<IntPoint>&);
  ALWAYS_INLINE void SetNeedsPaintPropertyUpdateIfNeeded();
  ALWAYS_INLINE void UpdateForObjectLocationAndSize(
      absl::optional<IntPoint>& paint_offset_translation);
  ALWAYS_INLINE void UpdateClipPathCache();
  ALWAYS_INLINE void UpdateStickyTranslation();
  ALWAYS_INLINE void UpdateTransform();
  ALWAYS_INLINE void UpdateTransformForSVGChild(CompositingReasons);
  ALWAYS_INLINE bool EffectCanUseCurrentClipAsOutputClip() const;
  ALWAYS_INLINE void UpdateEffect();
  ALWAYS_INLINE void UpdateFilter();
  ALWAYS_INLINE void UpdateFragmentClip();
  ALWAYS_INLINE void UpdateCssClip();
  ALWAYS_INLINE void UpdateClipPathClip();
  ALWAYS_INLINE void UpdateLocalBorderBoxContext();
  ALWAYS_INLINE bool NeedsOverflowControlsClip() const;
  ALWAYS_INLINE void UpdateOverflowControlsClip();
  ALWAYS_INLINE void UpdateInnerBorderRadiusClip();
  ALWAYS_INLINE void UpdateOverflowClip();
  ALWAYS_INLINE void UpdatePerspective();
  ALWAYS_INLINE void UpdateReplacedContentTransform();
  ALWAYS_INLINE void UpdateScrollAndScrollTranslation();
  ALWAYS_INLINE void UpdateOutOfFlowContext();
  // See core/paint/README.md for the description of isolation nodes.
  ALWAYS_INLINE void UpdateTransformIsolationNode();
  ALWAYS_INLINE void UpdateEffectIsolationNode();
  ALWAYS_INLINE void UpdateClipIsolationNode();
  ALWAYS_INLINE void SetTransformNodeStateForSVGChild(
      TransformPaintPropertyNode::State&);
  ALWAYS_INLINE void UpdateLayoutShiftRootChanged(bool is_layout_shift_root);

  bool NeedsPaintPropertyUpdate() const {
    return object_.NeedsPaintPropertyUpdate() ||
           full_context_.force_subtree_update_reasons;
  }

  bool IsInNGFragmentTraversal() const { return pre_paint_info_; }

  void SwitchToOOFContext(
      PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext&
          oof_context) const {
    context_.current = oof_context;

    // If we're not block-fragmented, or if we're traversing the fragment tree
    // to an orphaned object, simply setting a new context is all we have to do.
    if (!oof_context.is_in_block_fragmentation ||
        (pre_paint_info_ && pre_paint_info_->is_inside_orphaned_object))
      return;

    // Inside NG block fragmentation we have to perform an offset adjustment.
    // An OOF fragment that is contained by something inside a fragmentainer
    // will be a direct child of the fragmentainer, rather than a child of its
    // actual containing block. We therefore need to adjust the offset to make
    // us relative to the fragmentainer before applying the offset of the OOF.
    PhysicalOffset delta =
        oof_context.paint_offset - context_.fragmentainer_paint_offset;
    // So, we did store |fragmentainer_paint_offset| when entering the
    // fragmentainer, but the offset may have been reset by
    // UpdateForPaintOffsetTranslation() since we entered it, which we'll need
    // to compensate for now.
    delta += context_.adjustment_for_oof_in_fragmentainer;
    context_.current.paint_offset -= delta;
  }

  void ResetPaintOffset(PhysicalOffset new_offset = PhysicalOffset()) {
    context_.adjustment_for_oof_in_fragmentainer +=
        context_.current.paint_offset - new_offset;
    context_.current.paint_offset = new_offset;
  }

  void OnUpdate(PaintPropertyChangeType change) {
    property_changed_ = std::max(property_changed_, change);
  }
  // Like |OnUpdate| but sets |clip_changed| if the clip values change.
  void OnUpdateClip(PaintPropertyChangeType change) {
    OnUpdate(change);
    full_context_.clip_changed |=
        change >= PaintPropertyChangeType::kChangedOnlySimpleValues;
  }
  // Like |OnUpdate| but forces a piercing subtree update if the scroll tree
  // hierarchy changes because the scroll tree does not have isolation nodes
  // and non-piercing updates can fail to update scroll descendants.
  void OnUpdateScroll(PaintPropertyChangeType change) {
    OnUpdate(change);
    if (change >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    }
  }
  void OnClear(bool cleared) {
    if (cleared) {
      property_changed_ = PaintPropertyChangeType::kNodeAddedOrRemoved;
    }
  }
  // See: |OnUpdateScroll|.
  void OnClearScroll(bool cleared) {
    OnClear(cleared);
    if (cleared) {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    }
  }
  void OnClearClip(bool cleared) {
    OnClear(cleared);
    full_context_.clip_changed |= cleared;
  }

  CompositorElementId GetCompositorElementId(
      CompositorElementIdNamespace namespace_id) const {
    return CompositorElementIdFromUniqueObjectId(fragment_data_.UniqueId(),
                                                 namespace_id);
  }

  // If we need to composite, and CompositeAfterPaint isn't enabled, we're not
  // really allowed to fragment for real, but LayoutNG has already done that for
  // us. Return true if this is the situation and we need to Stitch all
  // fragments back together, like a tall strip.
  bool RequiresFragmentStitching() const {
    return pre_paint_info_ && object_.IsBox() &&
           !RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
           CompositingReasonFinder::PotentialCompositingReasonsFromStyle(
               object_);
  }

  const LayoutObject& object_;
  NGPrePaintInfo* pre_paint_info_;
  // The tree builder context for the whole object.
  PaintPropertyTreeBuilderContext& full_context_;
  // The tree builder context for the current fragment, which is one of the
  // entries in |full_context.fragments|.
  PaintPropertyTreeBuilderFragmentContext& context_;
  FragmentData& fragment_data_;
  ObjectPaintProperties* properties_;
  PaintPropertyChangeType property_changed_ =
      PaintPropertyChangeType::kUnchanged;
};

// True if a scroll translation is needed for static scroll offset (e.g.,
// overflow hidden with scroll), or if a scroll node is needed for composited
// scrolling.
static bool NeedsScrollOrScrollTranslation(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (!object.IsScrollContainer())
    return false;

  const LayoutBox& box = To<LayoutBox>(object);
  if (!box.GetScrollableArea())
    return false;

  ScrollOffset scroll_offset = box.GetScrollableArea()->GetScrollOffset();
  return !scroll_offset.IsZero() ||
         box.NeedsScrollNode(direct_compositing_reasons);
}

static bool NeedsReplacedContentTransform(const LayoutObject& object) {
  return object.IsSVGRoot();
}

static bool NeedsPaintOffsetTranslationForOverflowControls(
    const LayoutBoxModelObject& object) {
  if (auto* area = object.GetScrollableArea()) {
    if (area->HorizontalScrollbar() || area->VerticalScrollbar() ||
        area->Resizer()) {
      return true;
    }
  }
  return false;
}

static bool NeedsIsolationNodes(const LayoutObject& object) {
  if (!object.HasLayer())
    return false;

  // Paint containment establishes isolation.
  // Style & Layout containment also establish isolation.
  if (object.ShouldApplyPaintContainment() ||
      (object.ShouldApplyStyleContainment() &&
       object.ShouldApplyLayoutContainment())) {
    return true;
  }

  // Layout view establishes isolation with the exception of local roots (since
  // they are already essentially isolated).
  if (IsA<LayoutView>(object)) {
    const auto* parent_frame = object.GetFrame()->Tree().Parent();
    return IsA<LocalFrame>(parent_frame);
  }
  return false;
}

static bool NeedsStickyTranslation(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;

  return object.StyleRef().HasStickyConstrainedPosition();
}

static bool NeedsPaintOffsetTranslation(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (!object.IsBoxModelObject())
    return false;

  // <foreignObject> inherits no paint offset, because there is no such
  // concept within SVG. However, the foreign object can have its own paint
  // offset due to the x and y parameters of the element. This affects the
  // offset of painting of the <foreignObject> element and its children.
  // However, <foreignObject> otherwise behaves like other SVG elements, in
  // that the x and y offset is applied *after* any transform, instead of
  // before. Therefore there is no paint offset translation needed.
  if (object.IsSVGForeignObject())
    return false;

  const auto& box_model = To<LayoutBoxModelObject>(object);

  if (IsA<LayoutView>(box_model)) {
    // A translation node for LayoutView is always created to ensure fixed and
    // absolute contexts use the correct transform space.
    return true;
  }

  if (NeedsIsolationNodes(box_model)) {
    DCHECK(box_model.HasLayer());
    return true;
  }

  if (box_model.HasLayer() && box_model.Layer()->PaintsWithTransform(
                                  kGlobalPaintFlattenCompositingLayers)) {
    return true;
  }
  if (NeedsScrollOrScrollTranslation(object, direct_compositing_reasons))
    return true;
  if (NeedsStickyTranslation(object))
    return true;
  if (NeedsPaintOffsetTranslationForOverflowControls(box_model))
    return true;
  if (NeedsReplacedContentTransform(object))
    return true;

  // Reference filter and reflection (which creates a reference filter) requires
  // zero paint offset.
  if (box_model.HasLayer() &&
      (object.StyleRef().Filter().HasReferenceFilter() ||
       object.HasReflection()))
    return true;

  // Don't let paint offset cross composited layer boundaries when possible, to
  // avoid unnecessary full layer paint/raster invalidation when paint offset in
  // ancestor transform node changes which should not affect the descendants
  // of the composited layer. For now because of crbug.com/780242, this is
  // limited to LayoutBlocks and LayoutReplaceds that won't be escaped by
  // floating objects and column spans when finding their containing blocks.
  // TODO(crbug.com/780242): This can be avoided if we have fully correct
  // paint property tree states for floating objects and column spans.
  if ((box_model.IsLayoutBlock() || object.IsLayoutReplaced()) &&
      // TODO(wangxianzhu): Don't depend on PaintLayer for CompositeAfterPaint.
      object.HasLayer()) {
    PaintLayer* layer = To<LayoutBoxModelObject>(object).Layer();
    if (!layer->EnclosingPaginationLayer()) {
      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        if (direct_compositing_reasons != CompositingReason::kNone)
          return true;
        // In CompositeAfterPaint though we don't treat hidden backface as
        // a direct compositing reason, it's very likely that the object will
        // be composited, so a paint offset translation will be beneficial.
        if (box_model.StyleRef().BackfaceVisibility() ==
            EBackfaceVisibility::kHidden)
          return true;
      } else {
        if (layer->NeedsPaintOffsetTranslationForCompositing())
          return true;
      }
    }
  }

  return false;
}

bool FragmentPaintPropertyTreeBuilder::CanPropagateSubpixelAccumulation()
    const {
  if (!object_.HasLayer())
    return true;

  if (full_context_.direct_compositing_reasons &
      CompositingReason::kPreventingSubpixelAccumulationReasons) {
    return false;
  }
  if (full_context_.direct_compositing_reasons &
      CompositingReason::kActiveTransformAnimation) {
    if (const auto* element = DynamicTo<Element>(object_.GetNode())) {
      DCHECK(element->GetElementAnimations());
      return element->GetElementAnimations()->IsIdentityOrTranslation();
    }
    return false;
  }

  const PaintLayer* layer = To<LayoutBoxModelObject>(object_).Layer();
  return !layer->Transform() || layer->Transform()->IsIdentityOrTranslation();
}

void FragmentPaintPropertyTreeBuilder::UpdateForPaintOffsetTranslation(
    absl::optional<IntPoint>& paint_offset_translation) {
  if (!NeedsPaintOffsetTranslation(
          object_, full_context_.direct_compositing_reasons))
    return;

  // We should use the same subpixel paint offset values for snapping regardless
  // of paint offset translation. If we create a paint offset translation we
  // round the paint offset but keep around the residual fractional component
  // (i.e. subpixel accumulation) for the transformed content to paint with.
  // In pre-CompositeAfterPaint, if the object has layer, this corresponds to
  // PaintLayer::SubpixelAccumulation().
  paint_offset_translation = RoundedIntPoint(context_.current.paint_offset);
  // Don't propagate subpixel accumulation through paint isolation. In
  // pre-CompositeAfterPaint we still need to keep consistence with the legacy
  // compositing code.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      NeedsIsolationNodes(object_)) {
    ResetPaintOffset();
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();
    return;
  }

  // LayoutNG block fragmentation rocket science: If we need to composite, and
  // CompositeAfterPaint isn't enabled, we're not really allowed to fragment for
  // real, but the layout engine has already done that. Stitch all fragments
  // together like a tall strip. This will match legacy behavior pretty well,
  // including the concept of "pagination struts" found in legacy fragmentation.
  PhysicalOffset new_paint_offset;
  PhysicalOffset subpixel_accumulation;
  if (RequiresFragmentStitching()) {
    const LayoutBox& box = To<LayoutBox>(object_);
    const NGPhysicalBoxFragment& fragment = pre_paint_info_->box_fragment;

    // Calculate this fragment's rectangle relatively to the enclosing stitched
    // layout object, with help from the break token.
    //
    // Note that we're using the writing mode of this object here. Since the
    // object got fragmented, though, we can be sure that it's in the same
    // writing mode as the fragmentation context (or it would have been treated
    // as a monolithic element).
    WritingModeConverter converter(box.StyleRef().GetWritingDirection(),
                                   PhysicalSize(box.Size()));
    LogicalOffset logical_offset;
    if (const NGBlockBreakToken* incoming_break_token =
            FindPreviousBreakToken(fragment))
      logical_offset.block_offset = incoming_break_token->ConsumedBlockSize();
    LogicalRect logical_rect(logical_offset,
                             converter.ToLogical(fragment.Size()));
    // Find the offset relatively to the top/left edge of the enclosing stitched
    // layout object. This is the paint offset that we need to store in the
    // FragmentData. The paint offset is normally simply reset when applying
    // PaintOffsetTranslation, but in the stitching case we're cloning the
    // PaintOffsetTranslation across all fragments, so we'll need the paint
    // offset in order to get to the right offset for each fragment.
    new_paint_offset = converter.ToPhysical(logical_rect).offset;

    if (pre_paint_info_->fragment_data == &object_.FirstFragment()) {
      // Make the translation relatively to the top/left corner of the box. In
      // vertical-rl writing mode, the first fragment is not the leftmost one.
      PhysicalOffset topleft = context_.current.paint_offset - new_paint_offset;
      paint_offset_translation = RoundedIntPoint(topleft);
      subpixel_accumulation =
          topleft - PhysicalOffset(*paint_offset_translation);
    } else {
      // We're not at the first fragment. Clone the paint offset translation of
      // the first fragment.
      const FragmentData& first_fragment = object_.FirstFragment();
      const auto* properties = first_fragment.PaintProperties();
      FloatSize translation =
          properties->PaintOffsetTranslation()->Translation2D();
      paint_offset_translation = RoundedIntPoint(FloatPoint(translation));
      subpixel_accumulation =
          first_fragment.PaintOffset() -
          PhysicalOffset(RoundedIntPoint(first_fragment.PaintOffset()));
    }
  } else {
    subpixel_accumulation = context_.current.paint_offset -
                            PhysicalOffset(*paint_offset_translation);
  }

  if (!subpixel_accumulation.IsZero() ||
      !context_.current
           .directly_composited_container_paint_offset_subpixel_delta
           .IsZero()) {
    // If the object has a non-translation transform, discard the fractional
    // paint offset which can't be transformed by the transform.
    if (!CanPropagateSubpixelAccumulation()) {
      ResetPaintOffset(new_paint_offset);
      context_.current
          .directly_composited_container_paint_offset_subpixel_delta =
          PhysicalOffset();
      return;
    }
  }

  ResetPaintOffset(new_paint_offset + subpixel_accumulation);

  bool can_be_directly_composited =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
          ? full_context_.direct_compositing_reasons != CompositingReason::kNone
          : object_.CanBeCompositedForDirectReasons();
  if (!can_be_directly_composited)
    return;

  if (paint_offset_translation && properties_ &&
      properties_->PaintOffsetTranslation() && new_paint_offset.IsZero()) {
    // The composited subpixel movement optimization applies only if the
    // composited layer has and had PaintOffsetTranslation, so that both the
    // the old and new paint offsets are just subpixel accumulations.
    DCHECK_EQ(IntPoint(), RoundedIntPoint(fragment_data_.PaintOffset()));
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        context_.current.paint_offset - fragment_data_.PaintOffset();
  } else {
    // Otherwise disable the optimization.
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const absl::optional<IntPoint>& paint_offset_translation) {
  DCHECK(properties_);

  if (paint_offset_translation) {
    FloatSize new_translation(ToIntSize(*paint_offset_translation));
    TransformPaintPropertyNode::State state{new_translation};
    state.flags.flattens_inherited_transform =
        context_.current.should_flatten_inherited_transform;
    state.rendering_context_id = context_.current.rendering_context_id;
    state.direct_compositing_reasons =
        full_context_.direct_compositing_reasons &
        CompositingReason::kDirectReasonsForPaintOffsetTranslationProperty;
    if (state.direct_compositing_reasons & CompositingReason::kFixedPosition &&
        object_.View()->FirstFragment().PaintProperties()->Scroll()) {
      state.scroll_translation_for_fixed = object_.View()
                                               ->FirstFragment()
                                               .PaintProperties()
                                               ->ScrollTranslation();
    }

    if (IsA<LayoutView>(object_)) {
      DCHECK(object_.GetFrame());
      state.flags.is_frame_paint_offset_translation = true;
      state.visible_frame_element_id =
          object_.GetFrame()->GetVisibleToHitTesting()
              ? CompositorElementIdFromUniqueObjectId(
                    DOMNodeIds::IdForNode(&object_.GetDocument()),
                    CompositorElementIdNamespace::kDOMNodeId)
              : cc::ElementId();
    }
    OnUpdate(properties_->UpdatePaintOffsetTranslation(
        *context_.current.transform, std::move(state)));
    context_.current.transform = properties_->PaintOffsetTranslation();
    if (IsA<LayoutView>(object_)) {
      context_.absolute_position.transform =
          properties_->PaintOffsetTranslation();
      context_.fixed_position.transform = properties_->PaintOffsetTranslation();
    }

    if (!object_.ShouldAssumePaintOffsetTranslationForLayoutShiftTracking()) {
      context_.current.additional_offset_to_layout_shift_root_delta +=
          PhysicalOffset(*paint_offset_translation);
    }
  } else {
    OnClear(properties_->ClearPaintOffsetTranslation());
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateStickyTranslation() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsStickyTranslation(object_)) {
      const auto& box_model = To<LayoutBoxModelObject>(object_);
      TransformPaintPropertyNode::State state{
          FloatSize(box_model.StickyPositionOffset())};
      // TODO(wangxianzhu): Not using GetCompositorElementId() here because
      // sticky elements don't work properly under multicol for now, to keep
      // consistency with CompositorElementIdFromUniqueObjectId() below.
      // This will be fixed by LayoutNG block fragments.
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          box_model.UniqueId(),
          CompositorElementIdNamespace::kStickyTranslation);
      state.rendering_context_id = context_.current.rendering_context_id;
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;

      auto* layer = box_model.Layer();
      const auto* scroller_properties = layer->AncestorScrollContainerLayer()
                                            ->GetLayoutObject()
                                            .FirstFragment()
                                            .PaintProperties();
      // A scroll node is only created if an object can be scrolled manually,
      // while sticky position attaches to anything that clips overflow.
      // No need to (actually can't) setup composited sticky constraint if
      // the clipping ancestor we attach to doesn't have a scroll node.
      // TODO(crbug.com/881555): If the clipping ancestor does have a scroll
      // node, this really should be a DCHECK the current scrolll node
      // matches it. i.e.
      // if (scroller_properties && scroller_properties->Scroll()) {
      //   DCHECK_EQ(scroller_properties->Scroll(), context_.current.scroll);
      // However there is a bug that AncestorScrollContainerLayer() may be
      // computed incorrectly with clip escaping involved.
      bool nearest_scroller_is_clip =
          scroller_properties &&
          scroller_properties->Scroll() == context_.current.scroll;

      // Additionally, we also want to make sure that the nearest scroller
      // actually translates this node. If it doesn't (e.g. a position fixed
      // node in a scrolling document), there's no point in adding a constraint
      // since scrolling won't affect it. Indeed, if we do add it, the
      // compositor assumes scrolling does affect it and produces incorrect
      // results.
      bool translates_with_nearest_scroller =
          context_.current.transform->Unalias()
              .NearestScrollTranslationNode()
              .ScrollNode() == context_.current.scroll;
      if (nearest_scroller_is_clip && translates_with_nearest_scroller) {
        const StickyPositionScrollingConstraints& layout_constraint =
            layer->AncestorScrollContainerLayer()
                ->GetScrollableArea()
                ->GetStickyConstraintsMap()
                .at(layer);
        auto constraint = std::make_unique<CompositorStickyConstraint>();
        constraint->is_anchored_left = layout_constraint.is_anchored_left;
        constraint->is_anchored_right = layout_constraint.is_anchored_right;
        constraint->is_anchored_top = layout_constraint.is_anchored_top;
        constraint->is_anchored_bottom = layout_constraint.is_anchored_bottom;

        constraint->left_offset = layout_constraint.left_offset.ToFloat();
        constraint->right_offset = layout_constraint.right_offset.ToFloat();
        constraint->top_offset = layout_constraint.top_offset.ToFloat();
        constraint->bottom_offset = layout_constraint.bottom_offset.ToFloat();
        constraint->constraint_box_rect =
            FloatRect(box_model.ComputeStickyConstrainingRect());
        constraint->scroll_container_relative_sticky_box_rect = FloatRect(
            layout_constraint.scroll_container_relative_sticky_box_rect);
        constraint->scroll_container_relative_containing_block_rect = FloatRect(
            layout_constraint.scroll_container_relative_containing_block_rect);
        if (PaintLayer* sticky_box_shifting_ancestor =
                layout_constraint.nearest_sticky_layer_shifting_sticky_box) {
          constraint->nearest_element_shifting_sticky_box =
              CompositorElementIdFromUniqueObjectId(
                  sticky_box_shifting_ancestor->GetLayoutObject().UniqueId(),
                  CompositorElementIdNamespace::kStickyTranslation);
        }
        if (PaintLayer* containing_block_shifting_ancestor =
                layout_constraint
                    .nearest_sticky_layer_shifting_containing_block) {
          constraint->nearest_element_shifting_containing_block =
              CompositorElementIdFromUniqueObjectId(
                  containing_block_shifting_ancestor->GetLayoutObject()
                      .UniqueId(),
                  CompositorElementIdNamespace::kStickyTranslation);
        }
        state.sticky_constraint = std::move(constraint);
      }

      OnUpdate(properties_->UpdateStickyTranslation(*context_.current.transform,
                                                    std::move(state)));
    } else {
      OnClear(properties_->ClearStickyTranslation());
    }
  }

  if (properties_->StickyTranslation())
    context_.current.transform = properties_->StickyTranslation();
}

// TODO(crbug.com/900241): Remove this function and let the caller use
// CompositingReason::kDirectReasonForTransformProperty directly.
static CompositingReasons CompositingReasonsForTransformProperty() {
  CompositingReasons reasons =
      CompositingReason::kDirectReasonsForTransformProperty;
  // TODO(crbug.com/900241): Check for nodes for each KeyframeModel target
  // property instead of creating all nodes and only create a transform/
  // effect/filter node if needed.
  reasons |= CompositingReason::kComboActiveAnimation;
  // We also need to create a transform node if will-change creates other nodes,
  // to avoid raster invalidation caused by creating/deleting those nodes when
  // starting/stopping an animation. See: https://crbug.com/942681.
  reasons |= CompositingReason::kWillChangeOpacity;
  reasons |= CompositingReason::kWillChangeFilter;
  reasons |= CompositingReason::kWillChangeBackdropFilter;

  if (RuntimeEnabledFeatures::BackfaceVisibilityInteropEnabled())
    reasons |= CompositingReason::kBackfaceInvisibility3DAncestor;

  return reasons;
}

static bool NeedsTransformForSVGChild(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (!object.IsSVGChild() || object.IsText())
    return false;
  if (direct_compositing_reasons & CompositingReasonsForTransformProperty())
    return true;
  // TODO(pdr): Check for the presence of a transform instead of the value.
  // Checking for an identity matrix will cause the property tree structure
  // to change during animations if the animation passes through the
  // identity matrix.
  return !object.LocalToSVGParentTransform().IsIdentity();
}

static void SetTransformNodeStateFromAffineTransform(
    TransformPaintPropertyNode::State& state,
    const AffineTransform& transform) {
  if (transform.IsIdentityOrTranslation())
    state.transform_and_origin = {FloatSize(transform.E(), transform.F())};
  else
    state.transform_and_origin = {TransformationMatrix(transform)};
}

void FragmentPaintPropertyTreeBuilder::SetTransformNodeStateForSVGChild(
    TransformPaintPropertyNode::State& state) {
  if (full_context_.direct_compositing_reasons &
      CompositingReason::kActiveTransformAnimation) {
    if (CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG(
            *To<SVGElement>(object_.GetNode()))) {
      // For composited transform animation to work, we need to store transform
      // origin separately. It's baked in object_.LocalToSVGParentTransform().
      state.transform_and_origin = {
          TransformationMatrix(TransformHelper::ComputeTransform(
              object_, ComputedStyle::kExcludeTransformOrigin)),
          FloatPoint3D(TransformHelper::ComputeTransformOrigin(object_))};
      // Composited transform animation works only if
      // LocalToSVGParentTransform() reflects the CSS transform properties.
      // If this fails, we need to exclude the case in
      // CompositorAnimations::CanStartTransformAnimationOnCompositorForSVG().
      DCHECK_EQ(TransformHelper::ComputeTransform(
                    object_, ComputedStyle::kIncludeTransformOrigin),
                object_.LocalToSVGParentTransform());
    } else {
      // We composite the object but can't start composited animation. Still
      // keep the compositing reason because it still improves performance of
      // main thread animation, but avoid the 2d translation optimization to
      // meet the requirement of TransformPaintPropertyNode.
      state.transform_and_origin = {
          TransformationMatrix(object_.LocalToSVGParentTransform())};
    }
  } else {
    SetTransformNodeStateFromAffineTransform(
        state, object_.LocalToSVGParentTransform());
  }
}

// SVG does not use the general transform update of |UpdateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void FragmentPaintPropertyTreeBuilder::UpdateTransformForSVGChild(
    CompositingReasons direct_compositing_reasons) {
  DCHECK(properties_);
  DCHECK(object_.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object_.IsSVGForeignObject() ||
         context_.current.paint_offset.IsZero());

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsTransformForSVGChild(object_, direct_compositing_reasons)) {
      // The origin is included in the local transform, so leave origin empty.
      TransformPaintPropertyNode::State state;
      SetTransformNodeStateForSVGChild(state);

      // TODO(pdr): There is additional logic in
      // FragmentPaintPropertyTreeBuilder::UpdateTransform that likely needs to
      // be included here, such as setting animation_is_axis_aligned, which may
      // be the only important difference remaining.
      state.direct_compositing_reasons =
          direct_compositing_reasons & CompositingReasonsForTransformProperty();
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.rendering_context_id = context_.current.rendering_context_id;
      state.flags.is_for_svg_child = true;
      state.compositor_element_id = GetCompositorElementId(
          CompositorElementIdNamespace::kPrimaryTransform);

      TransformPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_animation_on_compositor =
          object_.StyleRef().IsRunningTransformAnimationOnCompositor();
      auto effective_change_type = properties_->UpdateTransform(
          *context_.current.transform, std::move(state), animation_state);
      if (effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues &&
          properties_->Transform()->HasDirectCompositingReasons()) {
        if (auto* paint_artifact_compositor =
                object_.GetFrameView()->GetPaintArtifactCompositor()) {
          bool updated = paint_artifact_compositor->DirectlyUpdateTransform(
              *properties_->Transform());
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            properties_->Transform()->CompositorSimpleValuesUpdated();
          }
        }
      }
      OnUpdate(effective_change_type);
    } else {
      OnClear(properties_->ClearTransform());
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    context_.current.should_flatten_inherited_transform = false;
    context_.current.rendering_context_id = 0;
  }
}

static FloatPoint3D TransformOrigin(const ComputedStyle& style,
                                    PhysicalSize size) {
  // Transform origin has no effect without a transform or motion path.
  if (!style.HasTransform())
    return FloatPoint3D();
  FloatSize border_box_size(size);
  return FloatPoint3D(
      FloatValueForLength(style.TransformOriginX(), border_box_size.Width()),
      FloatValueForLength(style.TransformOriginY(), border_box_size.Height()),
      style.TransformOriginZ());
}

}  // namespace

bool PaintPropertyTreeBuilder::NeedsTransform(
    const LayoutObject& object,
    CompositingReasons direct_compositing_reasons) {
  if (object.IsText())
    return false;

  if (object.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden)
    return true;

  if (direct_compositing_reasons & CompositingReasonsForTransformProperty())
    return true;

  if (!object.IsBox())
    return false;

  if (object.StyleRef().HasTransform() || object.StyleRef().Preserves3D())
    return true;

  return false;
}

namespace {

static bool UpdateBoxSizeAndCheckActiveAnimationAxisAlignment(
    const LayoutBox& object,
    CompositingReasons compositing_reasons) {
  if (!(compositing_reasons & CompositingReason::kActiveTransformAnimation))
    return false;

  if (!object.GetNode() || !object.GetNode()->IsElementNode())
    return false;
  const Element* element = To<Element>(object.GetNode());
  auto* animations = element->GetElementAnimations();
  DCHECK(animations);
  return animations->UpdateBoxSizeAndCheckTransformAxisAlignment(
      FloatSize(object.Size()));
}

void FragmentPaintPropertyTreeBuilder::UpdateTransform() {
  if (object_.IsSVGChild()) {
    UpdateTransformForSVGChild(full_context_.direct_compositing_reasons);
    return;
  }

  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    const ComputedStyle& style = object_.StyleRef();
    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    if (PaintPropertyTreeBuilder::NeedsTransform(
            object_, full_context_.direct_compositing_reasons)) {
      TransformPaintPropertyNode::State state;

      if (object_.IsBox()) {
        auto& box = To<LayoutBox>(object_);
        // Each individual fragment should have its own transform origin, based
        // on the fragment size. We'll do that, unless the fragments aren't to
        // be stitched together.
        PhysicalSize size;
        if (!pre_paint_info_ || RequiresFragmentStitching())
          size = PhysicalSize(box.Size());
        else
          size = pre_paint_info_->box_fragment.Size();
        TransformationMatrix matrix;
        style.ApplyTransform(
            matrix, size.ToLayoutSize(), ComputedStyle::kExcludeTransformOrigin,
            ComputedStyle::kIncludeMotionPath,
            ComputedStyle::kIncludeIndependentTransformProperties);
        // If we are running transform animation on compositor, we should
        // disable 2d translation optimization to ensure that the compositor
        // gets the correct origin (which might be omitted by the optimization)
        // to the compositor, in case later animated values will use the origin.
        // See http://crbug.com/937929 for why we are not using
        // style.IsRunningTransformAnimationOnCompositor() here.
        bool disable_2d_translation_optimization =
            full_context_.direct_compositing_reasons &
            CompositingReason::kActiveTransformAnimation;
        if (!disable_2d_translation_optimization &&
            matrix.IsIdentityOr2DTranslation()) {
          state.transform_and_origin = {matrix.To2DTranslation()};
        } else {
          state.transform_and_origin = {matrix,
                                        TransformOrigin(box.StyleRef(), size)};
        }

        // We want to track whether (a) this element is in a preserve-3d scene
        // and (b) has a matrix that puts it into the third dimension in some
        // way.  The test we use for (b) is stricter than
        // !matrix.Is2dTransform() or !matrix.IsFlat(); we're interested
        // *only* in things that cause this element to have a nonzero z
        // position within the 3-D scene.
        if (context_.current.rendering_context_id &&
            (matrix.M13() != 0.0 || matrix.M23() != 0.0 ||
             matrix.M43() != 0.0)) {
          UseCounter::Count(object_.GetDocument(),
                            WebFeature::kTransform3dScene);
        }

        // TODO(trchen): transform-style should only be respected if a
        // PaintLayer is created. If a node with transform-style: preserve-3d
        // does not exist in an existing rendering context, it establishes a
        // new one.
        state.rendering_context_id = context_.current.rendering_context_id;
        if (style.Preserves3D() && !state.rendering_context_id) {
          state.rendering_context_id =
              PtrHash<const LayoutObject>::GetHash(&object_);
        }

        // TODO(crbug.com/1185254): Make this work correctly for block
        // fragmentation. It's the size of each individual NGPhysicalBoxFragment
        // that's interesting, not the total LayoutBox size.
        state.flags.animation_is_axis_aligned =
            UpdateBoxSizeAndCheckActiveAnimationAxisAlignment(
                box, full_context_.direct_compositing_reasons);
      }

      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReasonsForTransformProperty();
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.backface_visibility =
          object_.HasHiddenBackface()
              ? TransformPaintPropertyNode::BackfaceVisibility::kHidden
              : TransformPaintPropertyNode::BackfaceVisibility::kVisible;
      state.compositor_element_id = GetCompositorElementId(
          CompositorElementIdNamespace::kPrimaryTransform);

      TransformPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_animation_on_compositor =
          style.IsRunningTransformAnimationOnCompositor();
      auto effective_change_type = properties_->UpdateTransform(
          *context_.current.transform, std::move(state), animation_state);
      if (effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues &&
          properties_->Transform()->HasDirectCompositingReasons()) {
        if (auto* paint_artifact_compositor =
                object_.GetFrameView()->GetPaintArtifactCompositor()) {
          bool updated = paint_artifact_compositor->DirectlyUpdateTransform(
              *properties_->Transform());
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            properties_->Transform()->CompositorSimpleValuesUpdated();
          }
        }
      }
      OnUpdate(effective_change_type);
    } else {
      OnClear(properties_->ClearTransform());
    }
  }

  // properties_->Transform() is present if a CSS transform is present,
  // and is also present if transform-style: preserve-3d is set.
  // See NeedsTransform.
  if (const auto* transform = properties_->Transform()) {
    context_.current.transform = transform;
    if (object_.StyleRef().Preserves3D()) {
      context_.current.rendering_context_id = transform->RenderingContextId();
      context_.current.should_flatten_inherited_transform = false;
    } else {
      context_.current.rendering_context_id = 0;
      context_.current.should_flatten_inherited_transform = true;
    }
    if (transform->IsIdentityOr2DTranslation()) {
      context_.translation_2d_to_layout_shift_root_delta +=
          transform->Translation2D();
    }
  } else if (RuntimeEnabledFeatures::TransformInteropEnabled() &&
             object_.IsForElement()) {
    // With kTransformInterop enabled, 3D rendering contexts follow the
    // DOM ancestor chain, so flattening should apply regardless of
    // presence of transform.
    context_.current.rendering_context_id = 0;
    context_.current.should_flatten_inherited_transform = true;
  }
}

static bool MayNeedClipPathClip(const LayoutObject& object) {
  // We only apply clip-path if the LayoutObject has a layer or is an SVG
  // child. See NeedsEffect() for additional information on the former.
  return !object.IsText() && object.StyleRef().HasClipPath() &&
         (object.HasLayer() || object.IsSVGChild());
}

static bool NeedsClipPathClip(const LayoutObject& object,
                              const FragmentData& fragment_data) {
  // We should have already updated the clip path cache when this is called.
  if (fragment_data.ClipPathPath()) {
    DCHECK(MayNeedClipPathClip(object));
    return true;
  }
  return false;
}

// TODO(crbug.com/900241): When this bug is fixed, we should let NeedsEffect()
// use CompositingReason::kDirectReasonForEffectProperty directly instead of
// calling this function. We should still call this function in UpdateEffect().
static CompositingReasons CompositingReasonsForEffectProperty() {
  CompositingReasons reasons =
      CompositingReason::kDirectReasonsForEffectProperty;
  // TODO(crbug.com/900241): Check for nodes for each KeyframeModel target
  // property instead of creating all nodes and only create a transform/
  // effect/filter node if needed.
  reasons |= CompositingReason::kComboActiveAnimation;
  // We also need to create an effect node if will-change creates other nodes,
  // to avoid raster invalidation caused by creating/deleting those nodes when
  // starting/stopping an animation. See: https://crbug.com/942681.
  // In CompositeAfterPaint, this also avoids decomposition of the effect when
  // the object is forced compositing with will-change:transform.
  reasons |= CompositingReason::kWillChangeTransform;
  reasons |= CompositingReason::kWillChangeFilter;
  return reasons;
}

static bool NeedsEffect(const LayoutObject& object,
                        CompositingReasons direct_compositing_reasons) {
  if (object.IsText())
    return false;

  const ComputedStyle& style = object.StyleRef();

  // For now some objects (e.g. LayoutTableCol) with stacking context style
  // don't create layer thus are not actual stacking contexts, so the HasLayer()
  // condition. TODO(crbug.com/892734): Support effects for LayoutTableCol.
  const bool is_css_isolated_group =
      object.HasLayer() && object.IsStackingContext();

  if (!is_css_isolated_group && !object.IsSVG())
    return false;

  if (object.IsSVG() && SVGLayoutSupport::IsIsolationRequired(&object))
    return true;

  if (is_css_isolated_group) {
    const auto* layer = To<LayoutBoxModelObject>(object).Layer();
    DCHECK(layer);

    if (layer->HasNonIsolatedDescendantWithBlendMode())
      return true;

    // An effect node is required by cc if the layer flattens its subtree but it
    // is treated as a 3D object by its parent.
    if (!layer->Preserves3D() && layer->HasSelfPaintingLayerDescendant() &&
        layer->Parent() && layer->Parent()->Preserves3D())
      return true;
  }

  SkBlendMode blend_mode = object.IsBlendingAllowed()
                               ? WebCoreCompositeToSkiaComposite(
                                     kCompositeSourceOver, style.GetBlendMode())
                               : SkBlendMode::kSrcOver;
  if (blend_mode != SkBlendMode::kSrcOver)
    return true;

  if (!style.BackdropFilter().IsEmpty())
    return true;

  if (style.Opacity() != 1.0f)
    return true;

  if (direct_compositing_reasons & CompositingReasonsForEffectProperty())
    return true;

  if (object.StyleRef().HasMask())
    return true;

  if (object.StyleRef().HasClipPath() &&
      object.FirstFragment().ClipPathBoundingBox() &&
      !object.FirstFragment().ClipPathPath()) {
    // If the object has a valid clip-path but can't use path-based clip-path,
    // a clip-path effect node must be created.
    return true;
  }

  return false;
}

// An effect node can use the current clip as its output clip if the clip won't
// end before the effect ends. Having explicit output clip can let the later
// stages use more optimized code path.
bool FragmentPaintPropertyTreeBuilder::EffectCanUseCurrentClipAsOutputClip()
    const {
  DCHECK(NeedsEffect(object_, full_context_.direct_compositing_reasons));

  if (!object_.HasLayer()) {
    // An SVG object's effect never interleaves with clips.
    DCHECK(object_.IsSVG());
    return true;
  }

  const auto* layer = To<LayoutBoxModelObject>(object_).Layer();
  // Out-of-flow descendants not contained by this object may escape clips.
  if (layer->HasNonContainedAbsolutePositionDescendant() &&
      &object_.ContainerForAbsolutePosition()
              ->FirstFragment()
              .PostOverflowClip() != context_.current.clip)
    return false;
  if (layer->HasFixedPositionDescendant() &&
      !object_.CanContainFixedPositionObjects() &&
      &object_.ContainerForFixedPosition()
              ->FirstFragment()
              .PostOverflowClip() != context_.current.clip)
    return false;

  // Some descendants under a pagination container (e.g. composited objects
  // in SPv1 and column spanners) may escape fragment clips.
  // TODO(crbug.com/803649): Remove this when we fix fragment clip hierarchy
  // issues.
  if (layer->EnclosingPaginationLayer())
    return false;

  return true;
}

void FragmentPaintPropertyTreeBuilder::UpdateEffect() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsEffect(object_, full_context_.direct_compositing_reasons)) {
      absl::optional<IntRect> mask_clip = CSSMaskPainter::MaskBoundingBox(
          object_, context_.current.paint_offset);
      bool has_clip_path =
          style.HasClipPath() && fragment_data_.ClipPathBoundingBox();
      bool has_mask_based_clip_path =
          has_clip_path && !fragment_data_.ClipPathPath();
      absl::optional<IntRect> clip_path_clip;
      if (has_mask_based_clip_path)
        clip_path_clip = fragment_data_.ClipPathBoundingBox();

      const auto* output_clip = EffectCanUseCurrentClipAsOutputClip()
                                    ? context_.current.clip
                                    : nullptr;

      if (mask_clip || clip_path_clip) {
        IntRect combined_clip = mask_clip ? *mask_clip : *clip_path_clip;
        if (mask_clip && clip_path_clip)
          combined_clip.Intersect(*clip_path_clip);

        OnUpdateClip(properties_->UpdateMaskClip(
            *context_.current.clip,
            ClipPaintPropertyNode::State(context_.current.transform,
                                         FloatRoundedRect(combined_clip))));
        output_clip = properties_->MaskClip();
      } else {
        OnClearClip(properties_->ClearMaskClip());
      }

      CompositorElementId mask_compositor_element_id;
      if (mask_clip) {
        mask_compositor_element_id =
            GetCompositorElementId(CompositorElementIdNamespace::kEffectMask);
      }

      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.output_clip = output_clip;
      state.opacity = style.Opacity();
      if (object_.IsBlendingAllowed()) {
        state.blend_mode = WebCoreCompositeToSkiaComposite(
            kCompositeSourceOver, style.GetBlendMode());
      }
      if (object_.IsBoxModelObject()) {
        if (auto* layer = To<LayoutBoxModelObject>(object_).Layer()) {
          CompositorFilterOperations operations;
          gfx::RRectF bounds;
          // Try to use the cached effect for backdrop-filter.
          if (properties_->Effect() &&
              properties_->Effect()->BackdropFilter()) {
            operations = *properties_->Effect()->BackdropFilter();
            bounds = properties_->Effect()->BackdropFilterBounds();
          }
          layer->UpdateCompositorFilterOperationsForBackdropFilter(operations,
                                                                   bounds);
          if (!operations.IsEmpty()) {
            state.backdrop_filter_info = base::WrapUnique(
                new EffectPaintPropertyNode::BackdropFilterInfo{
                    std::move(operations), bounds, mask_compositor_element_id});
          }
        }
      }

      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReasonsForEffectProperty();

      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        // If an effect node exists, add an additional direct compositing reason
        // for 3d transforms to ensure it is composited.
        CompositingReasons additional_transform_compositing_trigger =
            CompositingReason::k3DTransform |
            CompositingReason::kTrivial3DTransform;
        state.direct_compositing_reasons |=
            (full_context_.direct_compositing_reasons &
             additional_transform_compositing_trigger);
      }

      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        state.direct_compositing_reasons |=
            full_context_.direct_compositing_reasons &
            CompositingReason::kSVGRoot;
      }

      // We may begin to composite our subtree prior to an animation starts, but
      // a compositor element ID is only needed when an animation is current.
      // Currently, we use the existence of this id to check if effect nodes
      // have been created for animations on this element.
      if (state.direct_compositing_reasons) {
        state.compositor_element_id = GetCompositorElementId(
            CompositorElementIdNamespace::kPrimaryEffect);
      } else {
        // The effect node CompositorElementId is used to uniquely identify
        // renderpasses so even if we don't need one for animations we still
        // need to set an id. Using kPrimary avoids confusing cc::Animation
        // into thinking the element has been composited for animations.
        state.compositor_element_id =
            GetCompositorElementId(CompositorElementIdNamespace::kPrimary);
      }

      // TODO(crbug.com/900241): Remove these setters when we can use
      // state.direct_compositing_reasons to check for active animations.
      state.has_active_opacity_animation = style.HasCurrentOpacityAnimation();
      state.has_active_backdrop_filter_animation =
          style.HasCurrentBackdropFilterAnimation();

      state.document_transition_shared_element_id = GetSharedElementId(object_);

      EffectPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_opacity_animation_on_compositor =
          style.IsRunningOpacityAnimationOnCompositor();
      animation_state.is_running_backdrop_filter_animation_on_compositor =
          style.IsRunningBackdropFilterAnimationOnCompositor();
      auto effective_change_type = properties_->UpdateEffect(
          *context_.current_effect, std::move(state), animation_state);
      // If we have simple value change, which means opacity, we should try to
      // directly update it on the PaintArtifactCompositor in order to avoid
      // doing a full rebuild.
      if (effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues &&
          properties_->Effect()->HasDirectCompositingReasons()) {
        if (auto* paint_artifact_compositor =
                object_.GetFrameView()->GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdateCompositedOpacityValue(
                  *properties_->Effect());
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            properties_->Effect()->CompositorSimpleValuesUpdated();
          }
        }
      }
      OnUpdate(effective_change_type);

      auto mask_direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
                  CompositingReason::kDirectReasonsForBackdropFilter
              ? CompositingReason::kBackdropFilterMask
              : CompositingReason::kNone;

      if (mask_clip) {
        EffectPaintPropertyNode::State mask_state;
        mask_state.local_transform_space = context_.current.transform;
        mask_state.output_clip = output_clip;
        mask_state.blend_mode = SkBlendMode::kDstIn;
        mask_state.compositor_element_id = mask_compositor_element_id;
        mask_state.direct_compositing_reasons = mask_direct_compositing_reasons;
        OnUpdate(properties_->UpdateMask(*properties_->Effect(),
                                         std::move(mask_state)));
      } else {
        OnClear(properties_->ClearMask());
      }

      if (has_mask_based_clip_path) {
        EffectPaintPropertyNode::State clip_path_state;
        clip_path_state.local_transform_space = context_.current.transform;
        clip_path_state.output_clip = output_clip;
        clip_path_state.blend_mode = SkBlendMode::kDstIn;
        clip_path_state.compositor_element_id = GetCompositorElementId(
            CompositorElementIdNamespace::kEffectClipPath);
        if (!mask_clip) {
          clip_path_state.direct_compositing_reasons =
              mask_direct_compositing_reasons;
        }
        OnUpdate(properties_->UpdateClipPathMask(
            properties_->Mask() ? *properties_->Mask() : *properties_->Effect(),
            std::move(clip_path_state)));
      } else {
        OnClear(properties_->ClearClipPathMask());
      }
    } else {
      OnClear(properties_->ClearEffect());
      OnClear(properties_->ClearMask());
      OnClear(properties_->ClearClipPathMask());
      OnClearClip(properties_->ClearMaskClip());
    }
  }

  if (const auto* effect = properties_->Effect()) {
    context_.current_effect = effect;
    context_.this_or_ancestor_opacity_is_zero |= effect->Opacity() == 0;
    if (properties_->MaskClip()) {
      context_.current.clip = context_.absolute_position.clip =
          context_.fixed_position.clip = properties_->MaskClip();
    }
  }
}

static bool IsLinkHighlighted(const LayoutObject& object) {
  return object.GetFrame()->GetPage()->GetLinkHighlight().IsHighlighting(
      object);
}

// TODO(crbug.com/900241): When this bug is fixed, we should let NeedsFilter()
// use CompositingReason::kDirectReasonForFilterProperty directly instead of
// calling this function. We should still call this function in UpdateFilter().
static CompositingReasons CompositingReasonsForFilterProperty() {
  CompositingReasons reasons =
      CompositingReason::kDirectReasonsForFilterProperty;
  // TODO(crbug.com/900241): Check for nodes for each KeyframeModel target
  // property instead of creating all nodes and only create a transform/
  // effect/filter node if needed.
  reasons |= CompositingReason::kComboActiveAnimation;

  // We also need to create a filter node if will-change creates other nodes,
  // to avoid raster invalidation caused by creating/deleting those nodes when
  // starting/stopping an animation. See: https://crbug.com/942681.
  // In CompositeAfterPaint, this also avoids decomposition of the filter when
  // the object is forced compositing with will-change.
  reasons |= CompositingReason::kWillChangeTransform |
             CompositingReason::kWillChangeOpacity |
             CompositingReason::kWillChangeBackdropFilter;
  return reasons;
}

static bool IsClipPathDescendant(const LayoutObject& object) {
  // If the object itself is a resource container (root of a resource subtree)
  // it is not considered a clipPath descendant since it is independent of its
  // ancestors.
  if (object.IsSVGResourceContainer())
    return false;
  const LayoutObject* parent = object.Parent();
  while (parent) {
    if (parent->IsSVGResourceContainer()) {
      auto* container = To<LayoutSVGResourceContainer>(parent);
      return container->ResourceType() == kClipperResourceType;
    }
    parent = parent->Parent();
  }
  return false;
}

static bool NeedsFilter(const LayoutObject& object,
                        const PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.direct_compositing_reasons &
      CompositingReasonsForFilterProperty())
    return true;

  if (object.IsBoxModelObject() &&
      To<LayoutBoxModelObject>(object).HasLayer()) {
    if (object.StyleRef().HasFilter() || object.HasReflection())
      return true;
  } else if (object.IsSVGChild() && !object.IsText() &&
             SVGResources::GetClient(object)) {
    if (object.StyleRef().HasFilter()) {
      // Filters don't apply to elements that are descendants of a <clipPath>.
      if (!full_context.has_svg_hidden_container_ancestor ||
          !IsClipPathDescendant(object))
        return true;
    }
  }
  return false;
}

static void UpdateFilterEffect(const LayoutObject& object,
                               const EffectPaintPropertyNode* effect_node,
                               CompositorFilterOperations& filter) {
  if (object.HasLayer()) {
    // Try to use the cached filter.
    if (effect_node)
      filter = effect_node->Filter();
    PaintLayer* layer = To<LayoutBoxModelObject>(object).Layer();
#if DCHECK_IS_ON()
    // We should have already updated the reference box.
    auto reference_box = layer->FilterReferenceBox();
    layer->UpdateFilterReferenceBox();
    DCHECK_EQ(reference_box, layer->FilterReferenceBox());
#endif
    layer->UpdateCompositorFilterOperationsForFilter(filter);
    return;
  }
  if (object.IsSVGChild() && !object.IsText()) {
    SVGElementResourceClient* client = SVGResources::GetClient(object);
    if (!client)
      return;
    if (!object.StyleRef().HasFilter())
      return;
    // Try to use the cached filter.
    if (effect_node)
      filter = effect_node->Filter();
    client->UpdateFilterData(filter);
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateFilter() {
  DCHECK(properties_);
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsFilter(object_, full_context_)) {
      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;

      UpdateFilterEffect(object_, properties_->Filter(), state.filter);

      // The CSS filter spec didn't specify how filters interact with overflow
      // clips. The implementation here mimics the old Blink/WebKit behavior for
      // backward compatibility.
      // Basically the output of the filter will be affected by clips that
      // applies to the current element. The descendants that paints into the
      // input of the filter ignores any clips collected so far. For example:
      // <div style="overflow:scroll">
      //   <div style="filter:blur(1px);">
      //     <div>A</div>
      //     <div style="position:absolute;">B</div>
      //   </div>
      // </div>
      // In this example "A" should be clipped if the filter was not present.
      // With the filter, "A" will be rastered without clipping, but instead
      // the blurred result will be clipped.
      // On the other hand, "B" should not be clipped because the overflow clip
      // is not in its containing block chain, but as the filter output will be
      // clipped, so a blurred "B" may still be invisible.
      if (!state.filter.IsEmpty() ||
          (full_context_.direct_compositing_reasons &
           CompositingReason::kActiveFilterAnimation))
        state.output_clip = context_.current.clip;

      // TODO(trchen): A filter may contain spatial operations such that an
      // output pixel may depend on an input pixel outside of the output clip.
      // We should generate a special clip node to represent this expansion.

      // We may begin to composite our subtree prior to an animation starts,
      // but a compositor element ID is only needed when an animation is
      // current.
      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReasonsForFilterProperty();

      if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
        // If an effect node exists, add an additional direct compositing reason
        // for 3d transforms to ensure it is composited.
        CompositingReasons additional_transform_compositing_trigger =
            CompositingReason::k3DTransform |
            CompositingReason::kTrivial3DTransform;
        state.direct_compositing_reasons |=
            (full_context_.direct_compositing_reasons &
             additional_transform_compositing_trigger);
      }

      state.compositor_element_id =
          GetCompositorElementId(CompositorElementIdNamespace::kEffectFilter);

      // TODO(crbug.com/900241): Remove the setter when we can use
      // state.direct_compositing_reasons to check for active animations.
      state.has_active_filter_animation =
          object_.StyleRef().HasCurrentFilterAnimation();

      EffectPaintPropertyNode::AnimationState animation_state;
      animation_state.is_running_filter_animation_on_compositor =
          object_.StyleRef().IsRunningFilterAnimationOnCompositor();
      OnUpdate(properties_->UpdateFilter(*context_.current_effect,
                                         std::move(state), animation_state));
    } else {
      OnClear(properties_->ClearFilter());
    }
  }

  if (properties_->Filter()) {
    context_.current_effect = properties_->Filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    if (const auto* input_clip = properties_->Filter()->OutputClip()) {
      DCHECK_EQ(input_clip, context_.current.clip);
      context_.absolute_position.clip = context_.fixed_position.clip =
          input_clip;
    }
  }
}

static FloatRoundedRect ToSnappedClipRect(const PhysicalRect& rect) {
  return FloatRoundedRect(PixelSnappedIntRect(rect));
}

void FragmentPaintPropertyTreeBuilder::UpdateFragmentClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (context_.fragment_clip) {
      const auto& clip_rect = *context_.fragment_clip;
      OnUpdateClip(properties_->UpdateFragmentClip(
          *context_.current.clip,
          ClipPaintPropertyNode::State(context_.current.transform,
                                       FloatRoundedRect(FloatRect(clip_rect)),
                                       ToSnappedClipRect(clip_rect))));
    } else {
      OnClearClip(properties_->ClearFragmentClip());
    }
  }

  if (properties_->FragmentClip())
    context_.current.clip = properties_->FragmentClip();
}

static bool NeedsCssClip(const LayoutObject& object) {
  if (object.HasClip()) {
    DCHECK(!object.IsText());
    return true;
  }
  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateCssClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsCssClip(object_)) {
      // Create clip node for descendants that are not fixed position.
      // We don't have to setup context.absolutePosition.clip here because this
      // object must be a container for absolute position descendants, and will
      // copy from in-flow context later at updateOutOfFlowContext() step.
      DCHECK(object_.CanContainAbsolutePositionObjects());
      const auto& clip_rect =
          To<LayoutBox>(object_).ClipRect(context_.current.paint_offset);
      OnUpdateClip(properties_->UpdateCssClip(
          *context_.current.clip,
          ClipPaintPropertyNode::State(context_.current.transform,
                                       FloatRoundedRect(FloatRect(clip_rect)),
                                       ToSnappedClipRect(clip_rect))));
    } else {
      OnClearClip(properties_->ClearCssClip());
    }
  }

  if (properties_->CssClip())
    context_.current.clip = properties_->CssClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathClip() {
  if (NeedsPaintPropertyUpdate()) {
    if (!NeedsClipPathClip(object_, fragment_data_)) {
      OnClearClip(properties_->ClearClipPathClip());
    } else {
      ClipPaintPropertyNode::State state(
          context_.current.transform,
          FloatRoundedRect(*fragment_data_.ClipPathBoundingBox()));
      state.clip_path = fragment_data_.ClipPathPath();
      OnUpdateClip(properties_->UpdateClipPathClip(*context_.current.clip,
                                                   std::move(state)));
    }
  }

  if (properties_->ClipPathClip()) {
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = properties_->ClipPathClip();
  }
}

// TODO(wangxianzhu): Combine the logic by overriding LayoutBox::
// ComputeOverflowClipAxes() in LayoutReplaced and subclasses and remove
// this function.
static bool NeedsOverflowClipForReplacedContents(
    const LayoutReplaced& replaced) {
  // <svg> may optionally allow overflow. If an overflow clip is required,
  // always create it without checking whether the actual content overflows.
  if (replaced.IsSVGRoot())
    return To<LayoutSVGRoot>(replaced).ShouldApplyViewportClip();

  // A replaced element with border-radius always clips the content.
  if (replaced.StyleRef().HasBorderRadius())
    return true;

  // ImagePainter (but not painters for LayoutMedia whose IsImage is also true)
  // won't paint outside of the content box.
  if (replaced.IsImage() && !replaced.IsMedia())
    return false;

  // Non-plugin embedded contents are always sized to fit the content box.
  if (replaced.IsLayoutEmbeddedContent() && !replaced.IsEmbeddedObject())
    return false;

  return true;
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  if (const auto* replaced = DynamicTo<LayoutReplaced>(object))
    return NeedsOverflowClipForReplacedContents(*replaced);

  if (object.IsSVGViewportContainer() &&
      SVGLayoutSupport::IsOverflowHidden(object))
    return true;

  if (!object.IsBox())
    return false;

  if (!To<LayoutBox>(object).ShouldClipOverflowAlongEitherAxis())
    return false;

  if (IsA<LayoutView>(object) && !object.GetFrame()->ClipsContent())
    return false;

  return true;
}

void FragmentPaintPropertyTreeBuilder::UpdateLocalBorderBoxContext() {
  if (!NeedsPaintPropertyUpdate())
    return;

  if (object_.HasLayer() || properties_ || IsLinkHighlighted(object_)) {
    DCHECK(context_.current.transform);
    DCHECK(context_.current.clip);
    DCHECK(context_.current_effect);
    PropertyTreeStateOrAlias local_border_box(*context_.current.transform,
                                              *context_.current.clip,
                                              *context_.current_effect);

    if (!fragment_data_.HasLocalBorderBoxProperties() ||
        local_border_box != fragment_data_.LocalBorderBoxProperties())
      property_changed_ = PaintPropertyChangeType::kNodeAddedOrRemoved;

    fragment_data_.SetLocalBorderBoxProperties(std::move(local_border_box));
  } else {
    fragment_data_.ClearLocalBorderBoxProperties();
  }
}

bool FragmentPaintPropertyTreeBuilder::NeedsOverflowControlsClip() const {
  if (!object_.IsScrollContainer())
    return false;

  const auto& box = To<LayoutBox>(object_);
  const auto* scrollable_area = box.GetScrollableArea();
  IntRect scroll_controls_bounds =
      scrollable_area->ScrollCornerAndResizerRect();
  if (const auto* scrollbar = scrollable_area->HorizontalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  if (const auto* scrollbar = scrollable_area->VerticalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  IntRect pixel_snapped_border_box_rect(
      IntPoint(), box.PixelSnappedBorderBoxSize(context_.current.paint_offset));
  return !pixel_snapped_border_box_rect.Contains(scroll_controls_bounds);
}

static bool NeedsInnerBorderRadiusClip(const LayoutObject& object) {
  // Replaced elements don't have scrollbars thus needs no separate clip
  // for the padding box (InnerBorderRadiusClip) and the client box (padding
  // box minus scrollbar, OverflowClip).
  // Furthermore, replaced elements clip to the content box instead,
  if (object.IsLayoutReplaced())
    return false;

  return object.StyleRef().HasBorderRadius() && object.IsBox() &&
         NeedsOverflowClip(object);
}

static PhysicalOffset VisualOffsetFromPaintOffsetRoot(
    const PaintPropertyTreeBuilderFragmentContext& context,
    const PaintLayer* child) {
  const LayoutObject* paint_offset_root = context.current.paint_offset_root;
  PaintLayer* painting_layer = paint_offset_root->PaintingLayer();
  PhysicalOffset result = child->VisualOffsetFromAncestor(painting_layer);
  if (!paint_offset_root->HasLayer() ||
      To<LayoutBoxModelObject>(paint_offset_root)->Layer() != painting_layer) {
    result -= paint_offset_root->OffsetFromAncestor(
        &painting_layer->GetLayoutObject());
  }

  // Convert the result into the space of the scrolling contents space.
  if (const auto* properties =
          paint_offset_root->FirstFragment().PaintProperties()) {
    if (const auto* scroll_translation = properties->ScrollTranslation()) {
      result -= PhysicalOffset::FromFloatSizeRound(
          scroll_translation->Translation2D());
    }
  }
  return result;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowControlsClip() {
  DCHECK(properties_);

  if (!NeedsPaintPropertyUpdate())
    return;

  if (NeedsOverflowControlsClip()) {
    // Clip overflow controls to the border box rect. Not wrapped with
    // OnUpdateClip() because this clip doesn't affect descendants. Wrap with
    // OnUpdate() to let PrePaintTreeWalk see the change. This may cause
    // unnecessary subtree update, but is not a big deal because it is rare.
    const auto& clip_rect = PhysicalRect(context_.current.paint_offset,
                                         To<LayoutBox>(object_).Size());
    OnUpdate(properties_->UpdateOverflowControlsClip(
        *context_.current.clip,
        ClipPaintPropertyNode::State(context_.current.transform,
                                     FloatRoundedRect(FloatRect(clip_rect)),
                                     ToSnappedClipRect(clip_rect))));
  } else {
    OnClear(properties_->ClearOverflowControlsClip());
  }

  // We don't walk into custom scrollbars in PrePaintTreeWalk because
  // LayoutObjects under custom scrollbars don't support paint properties.
}

void FragmentPaintPropertyTreeBuilder::UpdateInnerBorderRadiusClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsInnerBorderRadiusClip(object_)) {
      const auto& box = To<LayoutBox>(object_);
      PhysicalRect box_rect(context_.current.paint_offset, box.Size());
      ClipPaintPropertyNode::State state(
          context_.current.transform,
          RoundedBorderGeometry::RoundedInnerBorder(box.StyleRef(), box_rect),
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(box.StyleRef(),
                                                                box_rect));
      OnUpdateClip(properties_->UpdateInnerBorderRadiusClip(
          *context_.current.clip, std::move(state)));
    } else {
      OnClearClip(properties_->ClearInnerBorderRadiusClip());
    }
  }

  if (auto* border_radius_clip = properties_->InnerBorderRadiusClip())
    context_.current.clip = border_radius_clip;
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsOverflowClip(object_)) {
      ClipPaintPropertyNode::State state(context_.current.transform,
                                         FloatRoundedRect());

      if (object_.IsLayoutReplaced()) {
        const auto& replaced = To<LayoutReplaced>(object_);

        // Videos need to be pre-snapped so that they line up with the
        // display_rect and can enable hardware overlays. Adjust the base rect
        // here, before applying padding and corner rounding.
        PhysicalRect content_rect(context_.current.paint_offset,
                                  replaced.Size());
        if (IsA<LayoutVideo>(replaced)) {
          content_rect =
              LayoutReplaced::PreSnappedRectForPersistentSizing(content_rect);
        }
        // LayoutReplaced clips the foreground by rounded content box.
        auto clip_rect = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
            replaced.StyleRef(), content_rect,
            LayoutRectOutsets(
                -(replaced.PaddingTop() + replaced.BorderTop()),
                -(replaced.PaddingRight() + replaced.BorderRight()),
                -(replaced.PaddingBottom() + replaced.BorderBottom()),
                -(replaced.PaddingLeft() + replaced.BorderLeft())));
        state.SetClipRect(clip_rect, clip_rect);
        if (replaced.IsLayoutEmbeddedContent()) {
          // Embedded objects are always sized to fit the content rect, but they
          // could overflow by 1px due to pre-snapping. Adjust clip rect to
          // match pre-snapped box as a special case.
          FloatRect adjusted_rect = clip_rect.Rect();
          adjusted_rect.SetSize(FloatSize(replaced.ReplacedContentRect().size));
          FloatRoundedRect adjusted_clip_rect(adjusted_rect,
                                              clip_rect.GetRadii());
          state.SetClipRect(adjusted_clip_rect, adjusted_clip_rect);
        }
      } else if (object_.IsBox()) {
        PhysicalRect clip_rect;
        if (pre_paint_info_) {
          clip_rect = pre_paint_info_->box_fragment.OverflowClipRect(
              context_.current.paint_offset,
              FindPreviousBreakToken(pre_paint_info_->box_fragment));
        } else {
          clip_rect = To<LayoutBox>(object_).OverflowClipRect(
              context_.current.paint_offset);
        }
        state.SetClipRect(FloatRoundedRect(FloatRect(clip_rect)),
                          ToSnappedClipRect(clip_rect));

        state.clip_rect_excluding_overlay_scrollbars =
            FloatClipRect(FloatRect(To<LayoutBox>(object_).OverflowClipRect(
                context_.current.paint_offset,
                kExcludeOverlayScrollbarSizeForHitTesting)));
      } else {
        DCHECK(object_.IsSVGViewportContainer());
        const auto& viewport_container =
            To<LayoutSVGViewportContainer>(object_);
        const auto clip_rect = FloatRoundedRect(
            viewport_container.LocalToSVGParentTransform().Inverse().MapRect(
                viewport_container.Viewport()));
        state.SetClipRect(clip_rect, clip_rect);
      }
      OnUpdateClip(properties_->UpdateOverflowClip(*context_.current.clip,
                                                   std::move(state)));
    } else {
      OnClearClip(properties_->ClearOverflowClip());
    }
  }

  if (auto* overflow_clip = properties_->OverflowClip())
    context_.current.clip = overflow_clip;
}

static FloatPoint PerspectiveOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Perspective origin has no effect without perspective.
  DCHECK(style.HasPerspective());
  FloatSize border_box_size(box.Size());
  return FloatPointForLengthPoint(style.PerspectiveOrigin(), border_box_size);
}

static bool NeedsPerspective(const LayoutObject& object) {
  return object.IsBox() && object.StyleRef().HasPerspective();
}

void FragmentPaintPropertyTreeBuilder::UpdatePerspective() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsPerspective(object_)) {
      const ComputedStyle& style = object_.StyleRef();
      // The perspective node must not flatten (else nothing will get
      // perspective), but it should still extend the rendering context as
      // most transform nodes do.
      TransformPaintPropertyNode::State state{
          TransformPaintPropertyNode::TransformAndOrigin(
              TransformationMatrix().ApplyPerspective(style.UsedPerspective()),
              PerspectiveOrigin(To<LayoutBox>(object_)) +
                  FloatSize(context_.current.paint_offset))};
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.rendering_context_id = context_.current.rendering_context_id;
      OnUpdate(properties_->UpdatePerspective(*context_.current.transform,
                                              std::move(state)));
    } else {
      OnClear(properties_->ClearPerspective());
    }
  }

  if (properties_->Perspective()) {
    context_.current.transform = properties_->Perspective();
    context_.current.should_flatten_inherited_transform = false;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateReplacedContentTransform() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate() && !NeedsReplacedContentTransform(object_)) {
    OnClear(properties_->ClearReplacedContentTransform());
  } else if (NeedsPaintPropertyUpdate()) {
    AffineTransform content_to_parent_space;
    if (object_.IsSVGRoot()) {
      content_to_parent_space =
          SVGRootPainter(To<LayoutSVGRoot>(object_))
              .TransformToPixelSnappedBorderBox(context_.current.paint_offset);
    } else {
      NOTREACHED();
    }
    if (!content_to_parent_space.IsIdentity()) {
      TransformPaintPropertyNode::State state;
      SetTransformNodeStateFromAffineTransform(state, content_to_parent_space);
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.rendering_context_id = context_.current.rendering_context_id;
      OnUpdate(properties_->UpdateReplacedContentTransform(
          *context_.current.transform, std::move(state)));
    } else {
      OnClear(properties_->ClearReplacedContentTransform());
    }
  }

  if (object_.IsSVGRoot()) {
    // SVG painters don't use paint offset. The paint offset is baked into
    // the transform node instead.
    context_.current.paint_offset = PhysicalOffset();
    context_.current.directly_composited_container_paint_offset_subpixel_delta =
        PhysicalOffset();

    // Only <svg> paints its subtree as replaced contents. Other replaced
    // element type may have shadow DOM that should not be affected by the
    // replaced object fit.
    if (properties_->ReplacedContentTransform()) {
      context_.current.transform = properties_->ReplacedContentTransform();
      // TODO(pdr): SVG does not support 3D transforms so this should be
      // should_flatten_inherited_transform = true.
      context_.current.should_flatten_inherited_transform = false;
      context_.current.rendering_context_id = 0;
    }
  }
}

static MainThreadScrollingReasons GetMainThreadScrollingReasons(
    const LayoutObject& object,
    MainThreadScrollingReasons ancestor_reasons) {
  auto reasons = ancestor_reasons;
  if (!object.IsBox())
    return reasons;

  if (IsA<LayoutView>(object)) {
    if (object.GetFrameView()
            ->RequiresMainThreadScrollingForBackgroundAttachmentFixed()) {
      reasons |=
          cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
    }

    // TODO(pdr): This should apply to all scrollable areas, not just the
    // viewport. This is not a user-visible bug because the threaded scrolling
    // setting is only for testing.
    if (!object.GetFrame()->GetSettings()->GetThreadedScrollingEnabled())
      reasons |= cc::MainThreadScrollingReason::kThreadedScrollingDisabled;
  }
  return reasons;
}

void FragmentPaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation() {
  DCHECK(properties_);

  FloatSize old_scroll_offset;
  if (const auto* old_scroll_translation = properties_->ScrollTranslation()) {
    DCHECK(full_context_.was_layout_shift_root);
    old_scroll_offset = old_scroll_translation->Translation2D();
  }

  if (NeedsPaintPropertyUpdate()) {
    if (object_.IsBox() && To<LayoutBox>(object_).NeedsScrollNode(
                               full_context_.direct_compositing_reasons)) {
      const auto& box = To<LayoutBox>(object_);
      PaintLayerScrollableArea* scrollable_area = box.GetScrollableArea();
      ScrollPaintPropertyNode::State state;

      // The container bounds are snapped to integers to match the equivalent
      // bounds on cc::ScrollNode. The offset is snapped to match the current
      // integer offsets used in CompositedLayerMapping.
      state.container_rect = PixelSnappedIntRect(
          box.OverflowClipRect(context_.current.paint_offset));
      state.contents_size = scrollable_area->PixelSnappedContentsSize(
          context_.current.paint_offset);

      state.user_scrollable_horizontal =
          scrollable_area->UserInputScrollable(kHorizontalScrollbar);
      state.user_scrollable_vertical =
          scrollable_area->UserInputScrollable(kVerticalScrollbar);

      // TODO(bokan): We probably don't need to pass ancestor reasons down the
      // scroll tree. On the compositor, in
      // LayerTreeHostImpl::FindScrollNodeForDeviceViewportPoint, we walk up
      // the scroll tree looking at all the ancestor MainThreadScrollingReasons.
      // https://crbug.com/985127.
      auto ancestor_reasons =
          context_.current.scroll->GetMainThreadScrollingReasons();
      state.main_thread_scrolling_reasons =
          GetMainThreadScrollingReasons(object_, ancestor_reasons);

      // Main thread scrolling reasons depend on their ancestor's reasons
      // so ensure the entire subtree is updated when reasons change.
      if (auto* existing_scroll = properties_->Scroll()) {
        if (existing_scroll->GetMainThreadScrollingReasons() !=
            state.main_thread_scrolling_reasons) {
          // Main thread scrolling reasons cross into isolation.
          full_context_.force_subtree_update_reasons |=
              PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
        }
      }

      state.compositor_element_id = scrollable_area->GetScrollElementId();

      state.overscroll_behavior =
          cc::OverscrollBehavior(static_cast<cc::OverscrollBehavior::Type>(
                                     box.StyleRef().OverscrollBehaviorX()),
                                 static_cast<cc::OverscrollBehavior::Type>(
                                     box.StyleRef().OverscrollBehaviorY()));

      state.snap_container_data =
          box.GetScrollableArea() &&
                  box.GetScrollableArea()->GetSnapContainerData()
              ? absl::optional<cc::SnapContainerData>(
                    *box.GetScrollableArea()->GetSnapContainerData())
              : absl::nullopt;

      OnUpdateScroll(properties_->UpdateScroll(*context_.current.scroll,
                                               std::move(state)));

      // Create opacity effect nodes for overlay scrollbars for their fade
      // animation in the compositor.
      if (scrollable_area->VerticalScrollbar() &&
          scrollable_area->VerticalScrollbar()->IsOverlayScrollbar()) {
        EffectPaintPropertyNode::State effect_state;
        effect_state.local_transform_space = context_.current.transform;
        effect_state.direct_compositing_reasons =
            CompositingReason::kActiveOpacityAnimation;
        effect_state.has_active_opacity_animation = true;
        effect_state.compositor_element_id =
            scrollable_area->GetScrollbarElementId(
                ScrollbarOrientation::kVerticalScrollbar);
        OnUpdate(properties_->UpdateVerticalScrollbarEffect(
            *context_.current_effect, std::move(effect_state)));
      } else {
        OnClear(properties_->ClearVerticalScrollbarEffect());
      }

      if (scrollable_area->HorizontalScrollbar() &&
          scrollable_area->HorizontalScrollbar()->IsOverlayScrollbar()) {
        EffectPaintPropertyNode::State effect_state;
        effect_state.local_transform_space = context_.current.transform;
        effect_state.direct_compositing_reasons =
            CompositingReason::kActiveOpacityAnimation;
        effect_state.has_active_opacity_animation = true;
        effect_state.compositor_element_id =
            scrollable_area->GetScrollbarElementId(
                ScrollbarOrientation::kHorizontalScrollbar);
        OnUpdate(properties_->UpdateHorizontalScrollbarEffect(
            *context_.current_effect, std::move(effect_state)));
      } else {
        OnClear(properties_->ClearHorizontalScrollbarEffect());
      }
    } else {
      OnClearScroll(properties_->ClearScroll());
      OnClear(properties_->ClearVerticalScrollbarEffect());
      OnClear(properties_->ClearHorizontalScrollbarEffect());
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(
            object_, full_context_.direct_compositing_reasons)) {
      const auto& box = To<LayoutBox>(object_);
      DCHECK(box.GetScrollableArea());

      // Bake ScrollOrigin into ScrollTranslation. See comments for
      // ScrollTranslation in object_paint_properties.h for details.
      FloatPoint scroll_position = FloatPoint(box.ScrollOrigin()) +
                                   box.GetScrollableArea()->GetScrollOffset();
      TransformPaintPropertyNode::State state{-ToFloatSize(scroll_position)};
      if (!box.GetScrollableArea()->PendingScrollAnchorAdjustment().IsZero()) {
        context_.current.pending_scroll_anchor_adjustment +=
            box.GetScrollableArea()->PendingScrollAnchorAdjustment();
        box.GetScrollableArea()->ClearPendingScrollAnchorAdjustment();
      }
      state.flags.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.rendering_context_id = context_.current.rendering_context_id;
      state.direct_compositing_reasons =
          full_context_.direct_compositing_reasons &
          CompositingReason::kDirectReasonsForScrollTranslationProperty;
      state.scroll = properties_->Scroll();
      // If scroll and transform are both present, we should use the
      // transform property tree node to determine visibility of the
      // scrolling contents.
      if (object_.StyleRef().HasTransform() &&
          object_.StyleRef().BackfaceVisibility() ==
              EBackfaceVisibility::kHidden)
        state.flags.delegates_to_parent_for_backface = true;
      auto effective_change_type = properties_->UpdateScrollTranslation(
          *context_.current.transform, std::move(state));
      if (effective_change_type ==
              PaintPropertyChangeType::kChangedOnlySimpleValues &&
          properties_->ScrollTranslation()->HasDirectCompositingReasons()) {
        if (auto* paint_artifact_compositor =
                object_.GetFrameView()->GetPaintArtifactCompositor()) {
          bool updated =
              paint_artifact_compositor->DirectlyUpdateScrollOffsetTransform(
                  *properties_->ScrollTranslation());
          if (updated) {
            effective_change_type =
                PaintPropertyChangeType::kChangedOnlyCompositedValues;
            properties_->ScrollTranslation()->CompositorSimpleValuesUpdated();
          }
        }
      }
      OnUpdate(effective_change_type);
    } else {
      OnClear(properties_->ClearScrollTranslation());
    }
  }

  if (properties_->Scroll())
    context_.current.scroll = properties_->Scroll();

  if (const auto* scroll_translation = properties_->ScrollTranslation()) {
    context_.current.transform = scroll_translation;
    // See comments for ScrollTranslation in object_paint_properties.h for the
    // reason of adding ScrollOrigin().
    context_.current.paint_offset +=
        PhysicalOffset(To<LayoutBox>(object_).ScrollOrigin());

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        scroll_translation->Translation2D() != old_scroll_offset) {
      // Scrolling can change overlap relationship for sticky positioned or
      // elements fixed to an overflow: hidden view that programmatically
      // scrolls via script. In this case the fixed transform doesn't have
      // enough information to perform the expansion - there is no scroll node
      // to describe the bounds of the scrollable content.
      auto* frame_view = object_.GetFrameView();
      if (frame_view->HasStickyViewportConstrainedObject()) {
        // TODO(crbug.com/1117658): Implement better sticky overlap testing.
        frame_view->SetPaintArtifactCompositorNeedsUpdate();
      } else if (frame_view->HasViewportConstrainedObjects() &&
                 !frame_view->GetLayoutView()
                      ->FirstFragment()
                      .PaintProperties()
                      ->Scroll()) {
        frame_view->SetPaintArtifactCompositorNeedsUpdate();
      } else if (!object_.IsStackingContext() &&
                 // TODO(wangxianzhu): for accuracy, this should be something
                 // like ContainsStackedDescendants(). Evaluate this, and
                 // refine if this causes too much more updates.
                 To<LayoutBoxModelObject>(object_)
                     .Layer()
                     ->HasSelfPaintingLayerDescendant()) {
        // If the scroller is not a stacking context but contains stacked
        // descendants, we need to update compositing because the stacked
        // descendants may change overlap relationship with other stacked
        // elements that are not contained by this scroller.
        frame_view->SetPaintArtifactCompositorNeedsUpdate();
      }
    }

    // A scroller creates a layout shift root, so we just calculate one scroll
    // offset delta without accumulation.
    context_.current.scroll_offset_to_layout_shift_root_delta =
        scroll_translation->Translation2D() - old_scroll_offset;
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateOutOfFlowContext() {
  if (!object_.IsBoxModelObject() && !properties_)
    return;

  if (object_.IsLayoutBlock())
    context_.paint_offset_for_float = context_.current.paint_offset;

  if (object_.CanContainAbsolutePositionObjects())
    context_.absolute_position = context_.current;

  if (IsA<LayoutView>(object_)) {
    const auto* initial_fixed_transform = context_.fixed_position.transform;

    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = true;

    // Fixed position transform should not be affected.
    context_.fixed_position.transform = initial_fixed_transform;

    // Scrolling in a fixed position element should chain up through the
    // LayoutView.
    if (properties_->Scroll())
      context_.fixed_position.scroll = properties_->Scroll();
    if (properties_->ScrollTranslation()) {
      // Also undo the ScrollOrigin part in paint offset that was added when
      // ScrollTranslation was updated.
      context_.fixed_position.paint_offset -=
          PhysicalOffset(To<LayoutBox>(object_).ScrollOrigin());
    }
  } else if (object_.CanContainFixedPositionObjects()) {
    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = false;
  } else if (properties_ && properties_->CssClip()) {
    // CSS clip applies to all descendants, even if this object is not a
    // containing block ancestor of the descendant. It is okay for
    // absolute-position descendants because having CSS clip implies being
    // absolute position container. However for fixed-position descendants we
    // need to insert the clip here if we are not a containing block ancestor of
    // them.
    auto* css_clip = properties_->CssClip();

    // Before we actually create anything, check whether in-flow context and
    // fixed-position context has exactly the same clip. Reuse if possible.
    if (context_.fixed_position.clip == css_clip->Parent()) {
      context_.fixed_position.clip = css_clip;
    } else {
      if (NeedsPaintPropertyUpdate()) {
        OnUpdate(properties_->UpdateCssClipFixedPosition(
            *context_.fixed_position.clip,
            ClipPaintPropertyNode::State(&css_clip->LocalTransformSpace(),
                                         css_clip->PixelSnappedClipRect())));
      }
      if (properties_->CssClipFixedPosition())
        context_.fixed_position.clip = properties_->CssClipFixedPosition();
      return;
    }
  }

  if (NeedsPaintPropertyUpdate() && properties_)
    OnClear(properties_->ClearCssClipFixedPosition());
}

void FragmentPaintPropertyTreeBuilder::UpdateTransformIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdate(properties_->UpdateTransformIsolationNode(
          *context_.current.transform));
    } else {
      OnClear(properties_->ClearTransformIsolationNode());
    }
  }
  if (properties_->TransformIsolationNode())
    context_.current.transform = properties_->TransformIsolationNode();
}

void FragmentPaintPropertyTreeBuilder::UpdateEffectIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdate(
          properties_->UpdateEffectIsolationNode(*context_.current_effect));
    } else {
      OnClear(properties_->ClearEffectIsolationNode());
    }
  }
  if (properties_->EffectIsolationNode())
    context_.current_effect = properties_->EffectIsolationNode();
}

void FragmentPaintPropertyTreeBuilder::UpdateClipIsolationNode() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsIsolationNodes(object_)) {
      OnUpdate(properties_->UpdateClipIsolationNode(*context_.current.clip));
    } else {
      OnClear(properties_->ClearClipIsolationNode());
    }
  }
  if (properties_->ClipIsolationNode())
    context_.current.clip = properties_->ClipIsolationNode();
}

static PhysicalRect MapLocalRectToAncestorLayer(
    const LayoutBox& box,
    const PhysicalRect& local_rect,
    const PaintLayer& ancestor_layer) {
  return box.LocalToAncestorRect(local_rect, &ancestor_layer.GetLayoutObject(),
                                 kIgnoreTransforms);
}

static bool IsRepeatingTableSection(const LayoutObject& object) {
  if (!object.IsTableSection())
    return false;
  const auto& section = ToInterface<LayoutNGTableSectionInterface>(object);
  return section.IsRepeatingHeaderGroup() || section.IsRepeatingFooterGroup();
}

static PhysicalRect BoundingBoxInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // The special path for fragmented layers ensures that the bounding box also
  // covers contents visual overflow, so that the fragments will cover all
  // fragments of contents except for self-painting layers, because we initiate
  // fragment painting of contents from the layer.
  if (object.HasLayer() &&
      // Table section may repeat, and doesn't need the special layer path
      // because it doesn't have contents visual overflow.
      !object.IsTableSection()) {
    const auto* layer = To<LayoutBoxModelObject>(object).Layer();
    if (layer->ShouldFragmentCompositedBounds()) {
      ClipRect clip;
      layer->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
          .CalculateBackgroundClipRect(
              ClipRectsContext(&enclosing_pagination_layer, nullptr), clip);
      return Intersection(
          clip.Rect(), layer->PhysicalBoundingBox(&enclosing_pagination_layer));
    }
  }

  PhysicalRect local_bounds;
  const LayoutBox* local_space_object = nullptr;
  if (object.IsBox()) {
    local_space_object = To<LayoutBox>(&object);
    local_bounds = local_space_object->PhysicalBorderBoxRect();
  } else {
    // Non-boxes paint in the space of their containing block.
    local_space_object = object.ContainingBlock();
    // For non-SVG we can get a more accurate result with LocalVisualRect,
    // instead of falling back to the bounds of the enclosing block.
    if (!object.IsSVG()) {
      local_bounds = object.LocalVisualRect();
    } else {
      local_bounds = PhysicalRect::EnclosingRect(
          SVGLayoutSupport::LocalVisualRect(object));
    }
  }

  // The link highlight covers block visual overflows, continuations, etc. which
  // may intersect with more fragments than the object itself.
  if (IsLinkHighlighted(object)) {
    local_bounds.Unite(UnionRect(object.OutlineRects(
        PhysicalOffset(), NGOutlineType::kIncludeBlockVisualOverflow)));
  }

  // Compute the bounding box without transforms.
  auto bounding_box = MapLocalRectToAncestorLayer(
      *local_space_object, local_bounds, enclosing_pagination_layer);

  if (!IsRepeatingTableSection(object))
    return bounding_box;

  const auto& section = ToInterface<LayoutNGTableSectionInterface>(object);
  const auto& table = *section.TableInterface();

  if (section.IsRepeatingHeaderGroup()) {
    // Now bounding_box covers the original header. Expand it to intersect
    // with all fragments containing the original and repeatings, i.e. to
    // intersect any fragment containing any row.
    if (const auto* bottom_section = table.BottomNonEmptySectionInterface()) {
      const auto* bottom_section_box =
          To<LayoutBox>(bottom_section->ToLayoutObject());
      bounding_box.Unite(MapLocalRectToAncestorLayer(
          *bottom_section_box, bottom_section_box->PhysicalBorderBoxRect(),
          enclosing_pagination_layer));
    }
    return bounding_box;
  }

  DCHECK(section.IsRepeatingFooterGroup());
  // Similar to repeating header, expand bounding_box to intersect any
  // fragment containing any row first.
  if (const auto* top_section = table.TopNonEmptySectionInterface()) {
    const auto* top_section_box = To<LayoutBox>(top_section->ToLayoutObject());
    bounding_box.Unite(MapLocalRectToAncestorLayer(
        *top_section_box, top_section_box->PhysicalBorderBoxRect(),
        enclosing_pagination_layer));
    // However, the first fragment intersecting the expanded bounding_box may
    // not have enough space to contain the repeating footer. Exclude the
    // total height of the first row and repeating footers from the top of
    // bounding_box to exclude the first fragment without enough space.
    LayoutUnit top_exclusion = table.RowOffsetFromRepeatingFooter();
    if (top_section != &section) {
      top_exclusion +=
          To<LayoutBox>(top_section->FirstRowInterface()->ToLayoutObject())
              ->LogicalHeight() +
          table.VBorderSpacing();
    }
    // Subtract 1 to ensure overlap of 1 px for a fragment that has exactly
    // one row plus space for the footer.
    if (top_exclusion)
      top_exclusion -= 1;
    bounding_box.ShiftTopEdgeTo(bounding_box.Y() + top_exclusion);
  }
  return bounding_box;
}

static PhysicalOffset PaintOffsetInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // Non-boxes use their containing blocks' paint offset.
  if (!object.IsBox() && !object.HasLayer()) {
    return PaintOffsetInPaginationContainer(*object.ContainingBlock(),
                                            enclosing_pagination_layer);
  }
  return object.LocalToAncestorPoint(
      PhysicalOffset(), &enclosing_pagination_layer.GetLayoutObject(),
      kIgnoreTransforms);
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffset() {
  // Paint offsets for fragmented content are computed from scratch.
  const auto* enclosing_pagination_layer =
      full_context_.painting_layer->EnclosingPaginationLayer();
  if (enclosing_pagination_layer &&
      // Except if the paint_offset_root is below the pagination container, in
      // which case fragmentation offsets are already baked into the paint
      // offset transform for paint_offset_root.
      !context_.current.paint_offset_root->PaintingLayer()
           ->EnclosingPaginationLayer()) {
    if (object_.StyleRef().GetPosition() == EPosition::kAbsolute) {
      if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
        // FIXME(dbaron): When the TransformInteropEnabled flag is removed
        // because it's always enabled, we should move these variables from
        // PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext to
        // PaintPropertyTreeBuilderFragmentContext.
        context_.absolute_position.should_flatten_inherited_transform =
            context_.current.should_flatten_inherited_transform;
        context_.absolute_position.rendering_context_id =
            context_.current.rendering_context_id;
      }
      context_.current = context_.absolute_position;
    } else if (object_.StyleRef().GetPosition() == EPosition::kFixed) {
      if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
        // FIXME(dbaron): When the TransformInteropEnabled flag is removed
        // because it's always enabled, we should move these variables from
        // PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext to
        // PaintPropertyTreeBuilderFragmentContext.
        context_.fixed_position.should_flatten_inherited_transform =
            context_.current.should_flatten_inherited_transform;
        context_.fixed_position.rendering_context_id =
            context_.current.rendering_context_id;
      }
      context_.current = context_.fixed_position;
    }

    // Set fragment visual paint offset.
    PhysicalOffset paint_offset =
        PaintOffsetInPaginationContainer(object_, *enclosing_pagination_layer);

    paint_offset += fragment_data_.LegacyPaginationOffset();
    paint_offset += context_.repeating_paint_offset_adjustment;
    paint_offset +=
        VisualOffsetFromPaintOffsetRoot(context_, enclosing_pagination_layer);

    // The paint offset root can have a subpixel paint offset adjustment. The
    // paint offset root always has one fragment.
    const auto& paint_offset_root_fragment =
        context_.current.paint_offset_root->FirstFragment();
    paint_offset += paint_offset_root_fragment.PaintOffset();

    context_.current.paint_offset = paint_offset;
    return;
  }

  if (!pre_paint_info_) {
    if (object_.IsFloating() && !object_.IsInLayoutNGInlineFormattingContext())
      context_.current.paint_offset = context_.paint_offset_for_float;

    // Multicolumn spanners are painted starting at the multicolumn container
    // (but still inherit properties in layout-tree order) so reset the paint
    // offset.
    if (object_.IsColumnSpanAll()) {
      context_.current.paint_offset =
          object_.Container()->FirstFragment().PaintOffset();
    }
  }

  if (object_.IsBoxModelObject()) {
    const auto& box_model_object = To<LayoutBoxModelObject>(object_);
    switch (box_model_object.StyleRef().GetPosition()) {
      case EPosition::kStatic:
        break;
      case EPosition::kRelative:
        context_.current.paint_offset +=
            box_model_object.OffsetForInFlowPosition();
        break;
      case EPosition::kAbsolute: {
#if DCHECK_IS_ON()
        if (!pre_paint_info_ || !pre_paint_info_->is_inside_orphaned_object) {
          DCHECK_EQ(full_context_.container_for_absolute_position,
                    box_model_object.Container());
        }
#endif
        if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
          // FIXME(dbaron): When the TransformInteropEnabled flag is removed
          // because it's always enabled, we should move these variables from
          // PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext
          // to PaintPropertyTreeBuilderFragmentContext.
          context_.absolute_position.should_flatten_inherited_transform =
              context_.current.should_flatten_inherited_transform;
          context_.absolute_position.rendering_context_id =
              context_.current.rendering_context_id;
        }
        SwitchToOOFContext(context_.absolute_position);

        // Absolutely positioned content in an inline should be positioned
        // relative to the inline.
        const auto* container = full_context_.container_for_absolute_position;
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainAbsolutePositionObjects());
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              To<LayoutInline>(container)->OffsetForInFlowPositionedInline(
                  To<LayoutBox>(box_model_object));
        }
        break;
      }
      case EPosition::kSticky:
        break;
      case EPosition::kFixed: {
#if DCHECK_IS_ON()
        if (!pre_paint_info_ || !pre_paint_info_->is_inside_orphaned_object) {
          DCHECK_EQ(full_context_.container_for_fixed_position,
                    box_model_object.Container());
        }
#endif
        if (RuntimeEnabledFeatures::TransformInteropEnabled()) {
          // FIXME(dbaron): When the TransformInteropEnabled flag is removed
          // because it's always enabled, we should move these variables from
          // PaintPropertyTreeBuilderFragmentContext::ContainingBlockContext
          // to PaintPropertyTreeBuilderFragmentContext.
          context_.fixed_position.should_flatten_inherited_transform =
              context_.current.should_flatten_inherited_transform;
          context_.fixed_position.rendering_context_id =
              context_.current.rendering_context_id;
        }
        SwitchToOOFContext(context_.fixed_position);
        // Fixed-position elements that are fixed to the viewport have a
        // transform above the scroll of the LayoutView. Child content is
        // relative to that transform, and hence the fixed-position element.
        if (context_.fixed_position.fixed_position_children_fixed_to_root)
          context_.current.paint_offset_root = &box_model_object;

        const auto* container = full_context_.container_for_fixed_position;
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainFixedPositionObjects());
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              To<LayoutInline>(container)->OffsetForInFlowPositionedInline(
                  To<LayoutBox>(box_model_object));
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }

  if (const auto* box = DynamicTo<LayoutBox>(&object_)) {
    if (pre_paint_info_) {
      context_.current.paint_offset += pre_paint_info_->paint_offset;

      // Determine whether we're inside block fragmentation or not. OOF
      // descendants need special treatment inside block fragmentation.
      context_.current.is_in_block_fragmentation =
          pre_paint_info_->fragmentainer_idx != WTF::kNotFound &&
          box->GetNGPaginationBreakability() != LayoutBox::kForbidBreaks;
    } else {
      // TODO(pdr): Several calls in this function walk back up the tree to
      // calculate containers (e.g., physicalLocation,
      // offsetForInFlowPosition*).  The containing block and other containers
      // can be stored on PaintPropertyTreeBuilderFragmentContext instead of
      // recomputing them.
      context_.current.paint_offset += box->PhysicalLocation();

      // This is a weird quirk that table cells paint as children of table rows,
      // but their location have the row's location baked-in.
      // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
      if (object_.IsTableCellLegacy()) {
        LayoutObject* parent_row = object_.Parent();
        DCHECK(parent_row && parent_row->IsTableRow());
        context_.current.paint_offset -=
            To<LayoutBox>(parent_row)->PhysicalLocation();
      }
    }
  }

  context_.current.paint_offset += context_.repeating_paint_offset_adjustment;

  context_.current.additional_offset_to_layout_shift_root_delta +=
      context_.pending_additional_offset_to_layout_shift_root_delta;
  context_.pending_additional_offset_to_layout_shift_root_delta =
      PhysicalOffset();
}

void FragmentPaintPropertyTreeBuilder::SetNeedsPaintPropertyUpdateIfNeeded() {
  if (object_.HasLayer()) {
    PaintLayer* layer = To<LayoutBoxModelObject>(object_).Layer();
    layer->UpdateFilterReferenceBox();
  }

  if (!object_.IsBox())
    return;

  const LayoutBox& box = To<LayoutBox>(object_);

  if (box.IsLayoutReplaced() &&
      box.PreviousPhysicalContentBoxRect() != box.PhysicalContentBoxRect()) {
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
    if (box.IsLayoutEmbeddedContent()) {
      if (const auto* child_view =
              To<LayoutEmbeddedContent>(box).ChildLayoutView())
        child_view->GetMutableForPainting().SetNeedsPaintPropertyUpdate();
    }
  }

  if (box.Size() == box.PreviousSize())
    return;

  // CSS mask and clip-path comes with an implicit clip to the border box.
  // Currently only CAP generate and take advantage of those.
  const bool box_generates_property_nodes_for_mask_and_clip_path =
      box.HasMask() || box.HasClipPath();
  // The overflow clip paint property depends on the border box rect through
  // overflowClipRect(). The border box rect's size equals the frame rect's
  // size so we trigger a paint property update when the frame rect changes.
  if (NeedsOverflowClip(box) || NeedsInnerBorderRadiusClip(box) ||
      // The used value of CSS clip may depend on size of the box, e.g. for
      // clip: rect(auto auto auto -5px).
      NeedsCssClip(box) ||
      // Relative lengths (e.g., percentage values) in transform, perspective,
      // transform-origin, and perspective-origin can depend on the size of the
      // frame rect, so force a property update if it changes. TODO(pdr): We
      // only need to update properties if there are relative lengths.
      box.StyleRef().HasTransform() || NeedsPerspective(box) ||
      box_generates_property_nodes_for_mask_and_clip_path) {
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }

  if (MayNeedClipPathClip(box))
    box.GetMutableForPainting().InvalidateClipPathCache();

  // The filter generated for reflection depends on box size.
  if (box.HasReflection()) {
    DCHECK(box.HasLayer());
    box.Layer()->SetFilterOnEffectNodeDirty();
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForObjectLocationAndSize(
    absl::optional<IntPoint>& paint_offset_translation) {
  context_.old_paint_offset = fragment_data_.PaintOffset();
  UpdatePaintOffset();
  UpdateForPaintOffsetTranslation(paint_offset_translation);

  PhysicalOffset paint_offset_delta =
      fragment_data_.PaintOffset() - context_.current.paint_offset;
  if (!paint_offset_delta.IsZero()) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes. However, they are blocked by
    // isolation if subpixel accumulation doesn't change or CompositeAfterPaint
    // is enabled.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
        !paint_offset_delta.HasFraction()) {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    } else {
      full_context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    }

    object_.GetMutableForPainting().SetShouldCheckForPaintInvalidation();
    fragment_data_.SetPaintOffset(context_.current.paint_offset);
    fragment_data_.InvalidateClipPathCache();

    if (object_.IsBox()) {
      // See PaintLayerScrollableArea::PixelSnappedBorderBoxSize() for the
      // reason of this.
      if (auto* scrollable_area = To<LayoutBox>(object_).GetScrollableArea())
        scrollable_area->PositionOverflowControls();
    }

    object_.GetMutableForPainting().InvalidateIntersectionObserverCachedRects();
    object_.GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }

  if (paint_offset_translation)
    context_.current.paint_offset_root = &To<LayoutBoxModelObject>(object_);
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathCache() {
  if (!MayNeedClipPathClip(object_)) {
    fragment_data_.ClearClipPathCache();
    return;
  }

  if (fragment_data_.IsClipPathCacheValid())
    return;

  absl::optional<FloatRect> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(object_);
  if (!bounding_box) {
    fragment_data_.ClearClipPathCache();
    return;
  }
  bounding_box->MoveBy(FloatPoint(fragment_data_.PaintOffset()));

  absl::optional<Path> path = ClipPathClipper::PathBasedClip(object_);
  if (path)
    path->Translate(ToFloatSize(FloatPoint(fragment_data_.PaintOffset())));
  fragment_data_.SetClipPathCache(
      EnclosingIntRect(*bounding_box),
      path ? AdoptRef(new RefCountedPath(std::move(*path))) : nullptr);
}

static bool IsLayoutShiftRoot(const LayoutObject& object,
                              const FragmentData& fragment) {
  const auto* properties = fragment.PaintProperties();
  if (!properties)
    return false;
  if (IsA<LayoutView>(object))
    return true;
  if (auto* transform = properties->Transform()) {
    if (!transform->IsIdentityOr2DTranslation())
      return true;
  }
  if (properties->ReplacedContentTransform())
    return true;
  if (properties->TransformIsolationNode())
    return true;
  if (auto* offset_translation = properties->PaintOffsetTranslation()) {
    if (offset_translation->RequiresCompositingForScrollDependentPosition())
      return true;
  }
  if (properties->OverflowClip())
    return true;
  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateForSelf() {
#if DCHECK_IS_ON()
  FindPaintOffsetNeedingUpdateScope check_paint_offset(
      object_, fragment_data_, full_context_.is_actually_needed);
#endif

  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without NeedsPaintPropertyUpdate.
  absl::optional<IntPoint> paint_offset_translation;
  UpdateForObjectLocationAndSize(paint_offset_translation);
  if (&fragment_data_ == &object_.FirstFragment())
    SetNeedsPaintPropertyUpdateIfNeeded();
  UpdateClipPathCache();

  if (properties_) {
    {
#if DCHECK_IS_ON()
      FindPropertiesNeedingUpdateScope check_fragment_clip(
          object_, fragment_data_, full_context_.force_subtree_update_reasons);
#endif
      UpdateFragmentClip();
    }
    // Update of PaintOffsetTranslation is checked by
    // FindPaintOffsetNeedingUpdateScope.
    UpdatePaintOffsetTranslation(paint_offset_translation);
  }

#if DCHECK_IS_ON()
  FindPropertiesNeedingUpdateScope check_paint_properties(
      object_, fragment_data_, full_context_.force_subtree_update_reasons);
#endif

  if (properties_) {
    UpdateStickyTranslation();
    UpdateTransform();
    UpdateClipPathClip();
    UpdateEffect();
    UpdateCssClip();
    UpdateFilter();
    UpdateOverflowControlsClip();
  } else if (RuntimeEnabledFeatures::TransformInteropEnabled() &&
             object_.IsForElement()) {
    // With kTransformInterop enabled, 3D rendering contexts follow the
    // DOM ancestor chain, so flattening should apply regardless of
    // presence of transform.
    context_.current.rendering_context_id = 0;
    context_.current.should_flatten_inherited_transform = true;
  }
  UpdateLocalBorderBoxContext();
  UpdateLayoutShiftRootChanged(IsLayoutShiftRoot(object_, fragment_data_));

  // For LayoutView, additional_offset_to_layout_shift_root_delta applies to
  // neither itself nor descendants. For other layout shift roots, we clear the
  // delta at the end of UpdateForChildren() because the delta still applies to
  // the object itself. Same for translation_2d_to_layout_shift_delta and
  // scroll_offset_to_layout_shift_root_delta.
  if (IsA<LayoutView>(object_)) {
    context_.current.additional_offset_to_layout_shift_root_delta =
        PhysicalOffset();
    context_.translation_2d_to_layout_shift_root_delta = FloatSize();
    context_.current.scroll_offset_to_layout_shift_root_delta = FloatSize();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForChildren() {
#if DCHECK_IS_ON()
  // Paint offset should not change during this function.
  const bool needs_paint_offset_update = false;
  FindPaintOffsetNeedingUpdateScope check_paint_offset(
      object_, fragment_data_, needs_paint_offset_update);

  FindPropertiesNeedingUpdateScope check_paint_properties(
      object_, fragment_data_, full_context_.force_subtree_update_reasons);
#endif

  if (properties_) {
    UpdateInnerBorderRadiusClip();
    UpdateOverflowClip();
    UpdatePerspective();
    UpdateReplacedContentTransform();
    UpdateScrollAndScrollTranslation();
    UpdateTransformIsolationNode();
    UpdateEffectIsolationNode();
    UpdateClipIsolationNode();
  }
  UpdateOutOfFlowContext();

  bool is_layout_shift_root = IsLayoutShiftRoot(object_, fragment_data_);
  UpdateLayoutShiftRootChanged(is_layout_shift_root);
  if (full_context_.was_layout_shift_root || is_layout_shift_root) {
    // A layout shift root (e.g. with mere OverflowClip) may have non-zero
    // paint offset. Exclude the layout shift root's paint offset delta from
    // additional_offset_to_layout_shift_root_delta.
    context_.current.additional_offset_to_layout_shift_root_delta =
        context_.old_paint_offset - fragment_data_.PaintOffset();
    context_.translation_2d_to_layout_shift_root_delta = FloatSize();
    // Don't reset scroll_offset_to_layout_shift_root_delta if this object has
    // scroll translation because we need to propagate the delta to descendants.
    if (!properties_ || !properties_->ScrollTranslation()) {
      context_.current.scroll_offset_to_layout_shift_root_delta = FloatSize();
      context_.current.pending_scroll_anchor_adjustment = FloatSize();
    }
  }

#if DCHECK_IS_ON()
  if (properties_)
    properties_->Validate();
#endif
}

void FragmentPaintPropertyTreeBuilder::UpdateLayoutShiftRootChanged(
    bool is_layout_shift_root) {
  if (is_layout_shift_root != full_context_.was_layout_shift_root) {
    context_.current.layout_shift_root_changed = true;
  } else if (is_layout_shift_root && full_context_.was_layout_shift_root) {
    context_.current.layout_shift_root_changed = false;
  }
}

}  // namespace

void PaintPropertyTreeBuilder::InitFragmentPaintProperties(
    FragmentData& fragment,
    bool needs_paint_properties,
    PaintPropertyTreeBuilderFragmentContext& context) {
  if (const auto* properties = fragment.PaintProperties()) {
    if (const auto* translation = properties->PaintOffsetTranslation()) {
      // If there is a paint offset translation, it only causes a net change
      // in additional_offset_to_layout_shift_root_delta by the amount the
      // paint offset translation changed from the prior frame. To implement
      // this, we record a negative offset here, and then re-add it in
      // UpdatePaintOffsetTranslation. The net effect is that the value
      // of additional_offset_to_layout_shift_root_delta is the difference
      // between the old and new paint offset translation.
      context.pending_additional_offset_to_layout_shift_root_delta =
          -PhysicalOffset::FromFloatSizeRound(translation->Translation2D());
    }
    if (const auto* transform = properties->Transform()) {
      if (transform->IsIdentityOr2DTranslation()) {
        context.translation_2d_to_layout_shift_root_delta -=
            transform->Translation2D();
      }
    }
  }

  if (needs_paint_properties) {
    fragment.EnsurePaintProperties();
  } else if (fragment.PaintProperties()) {
    // Tree topology changes are blocked by isolation.
    context_.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    fragment.ClearPaintProperties();
  }
}

void PaintPropertyTreeBuilder::InitFragmentPaintPropertiesForLegacy(
    FragmentData& fragment,
    bool needs_paint_properties,
    const PhysicalOffset& pagination_offset,
    PaintPropertyTreeBuilderFragmentContext& context) {
  DCHECK(!IsInNGFragmentTraversal());
  InitFragmentPaintProperties(fragment, needs_paint_properties, context);
  fragment.SetLegacyPaginationOffset(pagination_offset);
  fragment.SetLogicalTopInFlowThread(context.logical_top_in_flow_thread);
}

void PaintPropertyTreeBuilder::InitFragmentPaintPropertiesForNG(
    bool needs_paint_properties) {
  if (context_.fragments.IsEmpty())
    context_.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());
  else
    context_.fragments.resize(1);
  InitFragmentPaintProperties(*pre_paint_info_->fragment_data,
                              needs_paint_properties, context_.fragments[0]);
}

void PaintPropertyTreeBuilder::InitSingleFragmentFromParent(
    bool needs_paint_properties) {
  FragmentData& first_fragment =
      object_.GetMutableForPainting().FirstFragment();
  first_fragment.ClearNextFragment();
  if (context_.fragments.IsEmpty()) {
    context_.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());
  } else {
    context_.fragments.resize(1);
    context_.fragments[0].fragment_clip.reset();
    context_.fragments[0].logical_top_in_flow_thread = LayoutUnit();
  }
  InitFragmentPaintPropertiesForLegacy(first_fragment, needs_paint_properties,
                                       PhysicalOffset(), context_.fragments[0]);

  // Column-span:all skips pagination container in the tree hierarchy, so it
  // should also skip any fragment clip created by the skipped pagination
  // container. We also need to skip fragment clip if the layer doesn't allow
  // fragmentation.
  bool skip_fragment_clip_for_composited_layer = false;
  if (object_.HasLayer()) {
    const auto* layer = To<LayoutBoxModelObject>(object_).Layer();
    skip_fragment_clip_for_composited_layer =
        layer->EnclosingPaginationLayer() &&
        !layer->ShouldFragmentCompositedBounds();
  }
  if (!skip_fragment_clip_for_composited_layer && !object_.IsColumnSpanAll())
    return;

  const auto* pagination_layer_in_tree_hierarchy =
      object_.Parent()->EnclosingLayer()->EnclosingPaginationLayer();
  if (!pagination_layer_in_tree_hierarchy)
    return;

  const auto& clip_container =
      pagination_layer_in_tree_hierarchy->GetLayoutObject();
  const auto* properties = clip_container.FirstFragment().PaintProperties();
  if (!properties || !properties->FragmentClip())
    return;

  // Skip fragment clip for composited layer only when there are no other clips.
  // TODO(crbug.com/803649): This is still incorrect if this object first
  // appear in the second or later fragment of its parent.
  if (skip_fragment_clip_for_composited_layer &&
      properties->FragmentClip() != context_.fragments[0].current.clip)
    return;

  // However, because we don't allow an object's clip to escape the
  // output clip of the object's effect, we can't skip fragment clip if
  // between this object and the container there is any effect that has
  // an output clip. TODO(crbug.com/803649): Fix this workaround.
  const auto& clip_container_effect = clip_container.FirstFragment()
                                          .LocalBorderBoxProperties()
                                          .Effect()
                                          .Unalias();
  for (const auto* effect = &context_.fragments[0].current_effect->Unalias();
       effect && effect != &clip_container_effect;
       effect = effect->UnaliasedParent()) {
    if (effect->OutputClip())
      return;
  }

  // Skip the fragment clip.
  context_.fragments[0].current.clip = properties->FragmentClip()->Parent();
}

void PaintPropertyTreeBuilder::UpdateCompositedLayerPaginationOffset() {
  DCHECK(!IsInNGFragmentTraversal());

  const auto* enclosing_pagination_layer =
      context_.painting_layer->EnclosingPaginationLayer();
  if (!enclosing_pagination_layer)
    return;

  // We reach here because context_.painting_layer is in a composited layer
  // under the pagination layer. SPv1* doesn't fragment composited layers,
  // but we still need to set correct pagination offset for correct paint
  // offset calculation.
  DCHECK(!context_.painting_layer->ShouldFragmentCompositedBounds());
  FragmentData& first_fragment =
      object_.GetMutableForPainting().FirstFragment();
  bool may_use_self_pagination_offset = false;
  const PaintLayer* parent_pagination_offset_layer = nullptr;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    parent_pagination_offset_layer =
        context_.painting_layer
            ->EnclosingCompositedScrollingLayerUnderPagination(kIncludeSelf);
    if (parent_pagination_offset_layer->GetLayoutObject() == object_) {
      may_use_self_pagination_offset = true;
      parent_pagination_offset_layer =
          parent_pagination_offset_layer
              ->EnclosingCompositedScrollingLayerUnderPagination(kExcludeSelf);
    }
  } else {
    may_use_self_pagination_offset = object_.CanBeCompositedForDirectReasons();
    parent_pagination_offset_layer =
        context_.painting_layer->EnclosingDirectlyCompositableLayer(
            may_use_self_pagination_offset ? kExcludeSelf : kIncludeSelf);
  }

  if (may_use_self_pagination_offset &&
      (!parent_pagination_offset_layer ||
       !parent_pagination_offset_layer->EnclosingPaginationLayer())) {
    // |object_| establishes the top level composited layer under the
    // pagination layer.
    FragmentainerIterator iterator(
        To<LayoutFlowThread>(enclosing_pagination_layer->GetLayoutObject()),
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer)
            .ToLayoutRect());
    if (!iterator.AtEnd()) {
      first_fragment.SetLegacyPaginationOffset(
          PhysicalOffsetToBeNoop(iterator.PaginationOffset()));
      first_fragment.SetLogicalTopInFlowThread(
          iterator.FragmentainerLogicalTopInFlowThread());
    }
  } else if (parent_pagination_offset_layer) {
    // All objects under the composited layer use the same pagination offset.
    const auto& fragment =
        parent_pagination_offset_layer->GetLayoutObject().FirstFragment();
    first_fragment.SetLegacyPaginationOffset(fragment.LegacyPaginationOffset());
    first_fragment.SetLogicalTopInFlowThread(fragment.LogicalTopInFlowThread());
  }
}

void PaintPropertyTreeBuilder::
    UpdateRepeatingTableSectionPaintOffsetAdjustment() {
  if (!context_.repeating_table_section)
    return;

  if (object_ == context_.repeating_table_section->ToLayoutObject()) {
    if (ToInterface<LayoutNGTableSectionInterface>(object_)
            .IsRepeatingHeaderGroup())
      UpdateRepeatingTableHeaderPaintOffsetAdjustment();
    else if (ToInterface<LayoutNGTableSectionInterface>(object_)
                 .IsRepeatingFooterGroup())
      UpdateRepeatingTableFooterPaintOffsetAdjustment();
  } else if (!context_.painting_layer->EnclosingPaginationLayer()) {
    // When repeating a table section in paged media, paint_offset is inherited
    // by descendants, so we only need to adjust point offset for the table
    // section.
    for (auto& fragment_context : context_.fragments) {
      fragment_context.repeating_paint_offset_adjustment = PhysicalOffset();
    }
  }

  // Otherwise the object is a descendant of the object which initiated the
  // repeating. It just uses repeating_paint_offset_adjustment in its fragment
  // contexts inherited from the initiating object.
}

// TODO(wangxianzhu): For now this works for horizontal-bt writing mode only.
// Need to support vertical writing modes.
void PaintPropertyTreeBuilder::
    UpdateRepeatingTableHeaderPaintOffsetAdjustment() {
  const auto& section = ToInterface<LayoutNGTableSectionInterface>(object_);
  DCHECK(section.IsRepeatingHeaderGroup());

  LayoutUnit fragment_height;
  LayoutUnit original_offset_in_flow_thread =
      context_.repeating_table_section_bounding_box.Y();
  LayoutUnit original_offset_in_fragment;
  const LayoutFlowThread* flow_thread = nullptr;
  if (const auto* pagination_layer =
          context_.painting_layer->EnclosingPaginationLayer()) {
    flow_thread = &To<LayoutFlowThread>(pagination_layer->GetLayoutObject());
    // TODO(crbug.com/757947): This shouldn't be possible but happens to
    // column-spanners in nested multi-col contexts.
    if (!flow_thread->IsPageLogicalHeightKnown())
      return;

    fragment_height =
        flow_thread->PageLogicalHeightForOffset(original_offset_in_flow_thread);
    original_offset_in_fragment =
        fragment_height - flow_thread->PageRemainingLogicalHeightForOffset(
                              original_offset_in_flow_thread,
                              LayoutBox::kAssociateWithLatterPage);
  } else {
    // The containing LayoutView serves as the virtual pagination container
    // for repeating table section in paged media.
    fragment_height = object_.View()->PageLogicalHeight();
    original_offset_in_fragment =
        IntMod(original_offset_in_flow_thread, fragment_height);
  }

  const LayoutNGTableInterface& table = *section.TableInterface();

  // This is total height of repeating headers seen by the table - height of
  // this header (which is the lowest repeating header seen by this table.
  auto repeating_offset_in_fragment =
      table.RowOffsetFromRepeatingHeader() -
      To<LayoutBox>(section.ToLayoutObject())->LogicalHeight();

  // For a repeating table header, the original location (which may be in the
  // middle of the fragment) and repeated locations (which should be always,
  // together with repeating headers of outer tables, aligned to the top of
  // the fragments) may be different. Therefore, for fragments other than the
  // first, adjust by |alignment_offset|.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;

  // It's the table sections that make room for repeating headers. Stop
  // repeating when we're past the last section. There may be trailing
  // border-spacing, and also bottom captions. No room has been made for a
  // repeated header there.
  auto sections_logical_height =
      To<LayoutBox>(table.BottomSectionInterface()->ToLayoutObject())
          ->LogicalBottom() -
      To<LayoutBox>(table.TopSectionInterface()->ToLayoutObject())
          ->LogicalTop();
  auto content_remaining = sections_logical_height - table.VBorderSpacing();

  for (wtf_size_t i = 0; i < context_.fragments.size(); ++i) {
    auto& fragment_context = context_.fragments[i];
    fragment_context.repeating_paint_offset_adjustment = PhysicalOffset();
    // Adjust paint offsets of repeatings (not including the original).
    if (i)
      fragment_context.repeating_paint_offset_adjustment.top = adjustment;

    // Calculate the adjustment for the repeating which will appear in the next
    // fragment.
    adjustment += fragment_height;

    if (adjustment >= content_remaining)
      break;

    // Calculate the offset of the next fragment in flow thread. It's used to
    // get the height of that fragment.
    fragment_offset_in_flow_thread += fragment_height;

    if (flow_thread) {
      fragment_height = flow_thread->PageLogicalHeightForOffset(
          fragment_offset_in_flow_thread);
    }
  }
}

void PaintPropertyTreeBuilder::
    UpdateRepeatingTableFooterPaintOffsetAdjustment() {
  const auto& section = ToInterface<LayoutNGTableSectionInterface>(object_);
  DCHECK(section.IsRepeatingFooterGroup());

  LayoutUnit fragment_height;
  LayoutUnit original_offset_in_flow_thread =
      context_.repeating_table_section_bounding_box.Bottom() -
      To<LayoutBox>(section.ToLayoutObject())->LogicalHeight();
  LayoutUnit original_offset_in_fragment;
  const LayoutFlowThread* flow_thread = nullptr;
  if (const auto* pagination_layer =
          context_.painting_layer->EnclosingPaginationLayer()) {
    flow_thread = &To<LayoutFlowThread>(pagination_layer->GetLayoutObject());
    // TODO(crbug.com/757947): This shouldn't be possible but happens to
    // column-spanners in nested multi-col contexts.
    if (!flow_thread->IsPageLogicalHeightKnown())
      return;

    fragment_height =
        flow_thread->PageLogicalHeightForOffset(original_offset_in_flow_thread);
    original_offset_in_fragment =
        fragment_height - flow_thread->PageRemainingLogicalHeightForOffset(
                              original_offset_in_flow_thread,
                              LayoutBox::kAssociateWithLatterPage);
  } else {
    // The containing LayoutView serves as the virtual pagination container
    // for repeating table section in paged media.
    fragment_height = object_.View()->PageLogicalHeight();
    original_offset_in_fragment =
        IntMod(original_offset_in_flow_thread, fragment_height);
  }

  const auto& table = *section.TableInterface();
  // TODO(crbug.com/798153): This keeps the existing behavior of repeating
  // footer painting in TableSectionPainter. Should change both places when
  // tweaking border-spacing for repeating footers.
  auto repeating_offset_in_fragment = fragment_height -
                                      table.RowOffsetFromRepeatingFooter() -
                                      table.VBorderSpacing();
  // We should show the whole bottom border instead of half if the table
  // collapses borders.
  if (table.ShouldCollapseBorders()) {
    repeating_offset_in_fragment -=
        To<LayoutBox>(table.ToLayoutObject())->BorderBottom();
  }

  // Similar to repeating header, this is to adjust the repeating footer from
  // its original location to the repeating location.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;
  for (auto i = context_.fragments.size(); i > 0; --i) {
    auto& fragment_context = context_.fragments[i - 1];
    fragment_context.repeating_paint_offset_adjustment = PhysicalOffset();
    // Adjust paint offsets of repeatings.
    if (i != context_.fragments.size())
      fragment_context.repeating_paint_offset_adjustment.top = adjustment;

    // Calculate the adjustment for the repeating which will appear in the
    // previous fragment.
    adjustment -= fragment_height;
    // Calculate the offset of the previous fragment in flow thread. It's used
    // to get the height of that fragment.
    fragment_offset_in_flow_thread -= fragment_height;

    if (flow_thread) {
      fragment_height = flow_thread->PageLogicalHeightForOffset(
          fragment_offset_in_flow_thread);
    }
  }
}

static LayoutUnit FragmentLogicalTopInParentFlowThread(
    const LayoutFlowThread& flow_thread,
    LayoutUnit logical_top_in_current_flow_thread) {
  const auto* parent_pagination_layer =
      flow_thread.Layer()->Parent()->EnclosingPaginationLayer();
  if (!parent_pagination_layer)
    return LayoutUnit();

  const auto* parent_flow_thread =
      &To<LayoutFlowThread>(parent_pagination_layer->GetLayoutObject());

  LayoutPoint location(LayoutUnit(), logical_top_in_current_flow_thread);
  // TODO(crbug.com/467477): Should we flip for writing-mode? For now regardless
  // of flipping, fast/multicol/vertical-rl/nested-columns.html fails.
  if (!flow_thread.IsHorizontalWritingMode())
    location = location.TransposedPoint();

  // Convert into parent_flow_thread's coordinates.
  location = flow_thread
                 .LocalToAncestorPoint(PhysicalOffsetToBeNoop(location),
                                       parent_flow_thread)
                 .ToLayoutPoint();
  if (!parent_flow_thread->IsHorizontalWritingMode())
    location = location.TransposedPoint();

  if (location.X() >= parent_flow_thread->LogicalWidth()) {
    // TODO(crbug.com/803649): We hit this condition for
    // external/wpt/css/css-multicol/multicol-height-block-child-001.xht.
    // The normal path would cause wrong FragmentClip hierarchy.
    // Return -1 to force standalone FragmentClip in the case.
    return LayoutUnit(-1);
  }

  // Return the logical top of the containing fragment in parent_flow_thread.
  return location.Y() +
         parent_flow_thread->PageRemainingLogicalHeightForOffset(
             location.Y(), LayoutBox::kAssociateWithLatterPage) -
         parent_flow_thread->PageLogicalHeightForOffset(location.Y());
}

// Find from parent contexts with matching |logical_top_in_flow_thread|, if any,
// to allow for correct property tree parenting of fragments.
PaintPropertyTreeBuilderFragmentContext
PaintPropertyTreeBuilder::ContextForFragment(
    const absl::optional<PhysicalRect>& fragment_clip,
    LayoutUnit logical_top_in_flow_thread) const {
  DCHECK(!IsInNGFragmentTraversal());
  const auto& parent_fragments = context_.fragments;
  if (parent_fragments.IsEmpty())
    return PaintPropertyTreeBuilderFragmentContext();

  // This will be used in the loop finding matching fragment from ancestor flow
  // threads after no matching from parent_fragments.
  LayoutUnit logical_top_in_containing_flow_thread;
  bool crossed_flow_thread = false;

  if (object_.IsLayoutFlowThread()) {
    const auto& flow_thread = To<LayoutFlowThread>(object_);
    // If this flow thread is under another flow thread, find the fragment in
    // the parent flow thread containing this fragment. Otherwise, the following
    // code will just match parent_contexts[0].
    logical_top_in_containing_flow_thread =
        FragmentLogicalTopInParentFlowThread(flow_thread,
                                             logical_top_in_flow_thread);
    for (const auto& parent_context : parent_fragments) {
      if (logical_top_in_containing_flow_thread ==
          parent_context.logical_top_in_flow_thread) {
        auto context = parent_context;
        context.fragment_clip = fragment_clip;
        context.logical_top_in_flow_thread = logical_top_in_flow_thread;
        return context;
      }
    }
    crossed_flow_thread = true;
  } else {
    bool parent_is_under_same_flow_thread;
    auto* pagination_layer =
        context_.painting_layer->EnclosingPaginationLayer();
    if (object_.IsColumnSpanAll()) {
      parent_is_under_same_flow_thread = false;
    } else if (object_.IsOutOfFlowPositioned()) {
      parent_is_under_same_flow_thread =
          (object_.Parent()->PaintingLayer()->EnclosingPaginationLayer() ==
           pagination_layer);
    } else {
      parent_is_under_same_flow_thread = true;
    }

    // Match against parent_fragments if the fragment and parent_fragments are
    // under the same flow thread.
    if (parent_is_under_same_flow_thread) {
      DCHECK(object_.Parent()->PaintingLayer()->EnclosingPaginationLayer() ==
             pagination_layer);
      for (const auto& parent_context : parent_fragments) {
        if (logical_top_in_flow_thread ==
            parent_context.logical_top_in_flow_thread) {
          auto context = parent_context;
          // The context inherits fragment clip from parent so we don't need
          // to issue fragment clip again.
          context.fragment_clip = absl::nullopt;
          return context;
        }
      }
    }

    logical_top_in_containing_flow_thread = logical_top_in_flow_thread;
    crossed_flow_thread = !parent_is_under_same_flow_thread;
  }

  // Found no matching parent fragment. Use parent_fragments[0] to inherit
  // transforms and effects from ancestors, and adjust the clip state.
  // TODO(crbug.com/803649): parent_fragments[0] is not always correct because
  // some ancestor transform/effect may be missing in the fragment if the
  // ancestor doesn't intersect with the first fragment of the flow thread.
  auto context = parent_fragments[0];
  context.logical_top_in_flow_thread = logical_top_in_flow_thread;
  context.fragment_clip = fragment_clip;

  // We reach here because of the following reasons:
  // 1. the parent doesn't have the corresponding fragment because the fragment
  //    overflows the parent;
  // 2. the fragment and parent_fragments are not under the same flow thread
  //    (e.g. column-span:all or some out-of-flow positioned).
  // For each case, we need to adjust context.current.clip. For now it's the
  // first parent fragment's FragmentClip which is not the correct clip for
  // object_.
  const ClipPaintPropertyNodeOrAlias* found_clip = nullptr;
  for (const auto* container = object_.Container(); container;
       container = container->Container()) {
    if (!container->FirstFragment().HasLocalBorderBoxProperties())
      continue;

    const FragmentData* container_fragment = &container->FirstFragment();
    while (container_fragment->LogicalTopInFlowThread() <
               logical_top_in_containing_flow_thread &&
           container_fragment->NextFragment())
      container_fragment = container_fragment->NextFragment();

    if (container_fragment->LogicalTopInFlowThread() ==
        logical_top_in_containing_flow_thread) {
      // Found a matching fragment in an ancestor container. Use the
      // container's content clip as the clip state.
      found_clip = &container_fragment->PostOverflowClip();
      break;
    }

    // We didn't find corresponding fragment in the container because the
    // fragment fully overflows the container. If the container has overflow
    // clip, then this fragment should be under |container_fragment|.
    // This works only when the current fragment and the overflow clip are under
    // the same flow thread. In other cases, we just leave it broken, which will
    // be fixed by LayoutNG block fragments hopefully.
    if (!crossed_flow_thread) {
      if (const auto* container_properties =
              container_fragment->PaintProperties()) {
        if (const auto* overflow_clip = container_properties->OverflowClip()) {
          context.logical_top_in_flow_thread =
              container_fragment->LogicalTopInFlowThread();
          found_clip = overflow_clip;
          break;
        }
      }
    }

    if (container->IsLayoutFlowThread()) {
      logical_top_in_containing_flow_thread =
          FragmentLogicalTopInParentFlowThread(
              To<LayoutFlowThread>(*container),
              logical_top_in_containing_flow_thread);
      crossed_flow_thread = true;
    }
  }

  // We should always find a matching ancestor fragment in the above loop
  // because logical_top_in_containing_flow_thread will be zero when we traverse
  // across the top-level flow thread and it should match the first fragment of
  // a non-fragmented ancestor container.
  DCHECK(found_clip);

  if (!crossed_flow_thread)
    context.fragment_clip = absl::nullopt;
  context.current.clip = found_clip;
  if (object_.StyleRef().GetPosition() == EPosition::kAbsolute)
    context.absolute_position.clip = found_clip;
  else if (object_.StyleRef().GetPosition() == EPosition::kFixed)
    context.fixed_position.clip = found_clip;
  return context;
}

void PaintPropertyTreeBuilder::CreateFragmentContextsInFlowThread(
    bool needs_paint_properties) {
  DCHECK(!IsInNGFragmentTraversal());
  // We need at least the fragments for all fragmented objects, which store
  // their local border box properties and paint invalidation data (such
  // as paint offset and visual rect) on each fragment.
  PaintLayer* paint_layer = context_.painting_layer;
  PaintLayer* enclosing_pagination_layer =
      paint_layer->EnclosingPaginationLayer();

  const auto& flow_thread =
      To<LayoutFlowThread>(enclosing_pagination_layer->GetLayoutObject());
  PhysicalRect object_bounding_box_in_flow_thread;
  if (context_.repeating_table_section) {
    // The object is a descendant of a repeating object. It should use the
    // repeating bounding box to repeat in the same fragments as its
    // repeating ancestor.
    object_bounding_box_in_flow_thread =
        context_.repeating_table_section_bounding_box;
  } else {
    object_bounding_box_in_flow_thread =
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer);
    if (IsRepeatingTableSection(object_)) {
      context_.repeating_table_section =
          &ToInterface<LayoutNGTableSectionInterface>(object_);
      context_.repeating_table_section_bounding_box =
          object_bounding_box_in_flow_thread;
    }
  }

  FragmentData* current_fragment_data = nullptr;
  FragmentainerIterator iterator(
      flow_thread, object_bounding_box_in_flow_thread.ToLayoutRect());
  bool fragments_changed = false;
  Vector<PaintPropertyTreeBuilderFragmentContext, 1> new_fragment_contexts;
  for (; !iterator.AtEnd(); iterator.Advance()) {
    auto pagination_offset =
        PhysicalOffsetToBeNoop(iterator.PaginationOffset());
    auto logical_top_in_flow_thread =
        iterator.FragmentainerLogicalTopInFlowThread();
    absl::optional<PhysicalRect> fragment_clip;

    if (object_.HasLayer()) {
      // 1. Compute clip in flow thread space.
      fragment_clip = PhysicalRectToBeNoop(iterator.ClipRectInFlowThread());

      // We skip empty clip fragments, since they can share the same logical top
      // with the subsequent fragments. Since we skip drawing empty fragments
      // anyway, it doesn't affect the paint output, but it allows us to use
      // logical top to uniquely identify fragments in an object.
      if (fragment_clip->IsEmpty())
        continue;

      // 2. Convert #1 to visual coordinates in the space of the flow thread.
      fragment_clip->Move(pagination_offset);
      // 3. Adjust #2 to visual coordinates in the containing "paint offset"
      // space.
      {
        DCHECK(context_.fragments[0].current.paint_offset_root);
        PhysicalOffset pagination_visual_offset =
            VisualOffsetFromPaintOffsetRoot(context_.fragments[0],
                                            enclosing_pagination_layer);
        // Adjust for paint offset of the root, which may have a subpixel
        // component. The paint offset root never has more than one fragment.
        pagination_visual_offset +=
            context_.fragments[0]
                .current.paint_offset_root->FirstFragment()
                .PaintOffset();
        fragment_clip->Move(pagination_visual_offset);
      }
    }

    // Match to parent fragments from the same containing flow thread.
    auto fragment_context =
        ContextForFragment(fragment_clip, logical_top_in_flow_thread);
    // ContextForFragment may override logical_top_in_flow_thread.
    logical_top_in_flow_thread = fragment_context.logical_top_in_flow_thread;
    // Avoid fragment with duplicated overridden logical_top_in_flow_thread.
    if (new_fragment_contexts.size() &&
        new_fragment_contexts.back().logical_top_in_flow_thread ==
            logical_top_in_flow_thread)
      break;
    new_fragment_contexts.push_back(fragment_context);

    if (current_fragment_data) {
      if (!current_fragment_data->NextFragment())
        fragments_changed = true;
      current_fragment_data = &current_fragment_data->EnsureNextFragment();
    } else {
      current_fragment_data = &object_.GetMutableForPainting().FirstFragment();
    }

    fragments_changed |= logical_top_in_flow_thread !=
                         current_fragment_data->LogicalTopInFlowThread();
    if (!fragments_changed) {
      const ClipPaintPropertyNode* old_fragment_clip = nullptr;
      if (const auto* properties = current_fragment_data->PaintProperties())
        old_fragment_clip = properties->FragmentClip();
      const absl::optional<PhysicalRect>& new_fragment_clip =
          new_fragment_contexts.back().fragment_clip;
      fragments_changed = !!old_fragment_clip != !!new_fragment_clip ||
                          (old_fragment_clip && new_fragment_clip &&
                           old_fragment_clip->PixelSnappedClipRect() !=
                               ToSnappedClipRect(*new_fragment_clip));
    }

    InitFragmentPaintPropertiesForLegacy(
        *current_fragment_data,
        needs_paint_properties || new_fragment_contexts.back().fragment_clip,
        pagination_offset, new_fragment_contexts.back());
  }

  if (!current_fragment_data) {
    // This will be an empty fragment - get rid of it?
    InitSingleFragmentFromParent(needs_paint_properties);
  } else {
    if (current_fragment_data->NextFragment())
      fragments_changed = true;
    current_fragment_data->ClearNextFragment();
    context_.fragments = std::move(new_fragment_contexts);
  }

  // Need to update subtree paint properties for the changed fragments.
  if (fragments_changed) {
    object_.GetMutableForPainting().AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kFragmentsChanged);
  }
}

bool PaintPropertyTreeBuilder::ObjectIsRepeatingTableSectionInPagedMedia()
    const {
  if (!IsRepeatingTableSection(object_))
    return false;

  // The table section repeats in the pagination layer instead of paged media.
  if (context_.painting_layer->EnclosingPaginationLayer())
    return false;

  if (!object_.View()->PageLogicalHeight())
    return false;

  // TODO(crbug.com/619094): Figure out the correct behavior for repeating
  // objects in paged media with vertical writing modes.
  if (!object_.View()->IsHorizontalWritingMode())
    return false;

  return true;
}

void PaintPropertyTreeBuilder::
    CreateFragmentContextsForRepeatingFixedPosition() {
  DCHECK(object_.IsFixedPositionObjectInPagedMedia());

  LayoutView* view = object_.View();
  auto page_height = view->PageLogicalHeight();
  int page_count = ceilf(view->DocumentRect().Height() / page_height);
  context_.fragments.resize(page_count);

  if (auto* scrollable_area = view->GetScrollableArea()) {
    context_.fragments[0].fixed_position.paint_offset.top -=
        LayoutUnit(scrollable_area->ScrollPosition().Y());
  }

  for (int page = 1; page < page_count; page++) {
    context_.fragments[page] = context_.fragments[page - 1];
    context_.fragments[page].fixed_position.paint_offset.top += page_height;
    context_.fragments[page].logical_top_in_flow_thread += page_height;
  }
}

void PaintPropertyTreeBuilder::
    CreateFragmentContextsForRepeatingTableSectionInPagedMedia() {
  DCHECK(ObjectIsRepeatingTableSectionInPagedMedia());

  // The containing LayoutView serves as the virtual pagination container
  // for repeating table section in paged media.
  LayoutView* view = object_.View();
  context_.repeating_table_section_bounding_box =
      BoundingBoxInPaginationContainer(object_, *view->Layer());
  // Convert the bounding box into the scrolling contents space.
  if (auto* scrollable_area = view->GetScrollableArea()) {
    context_.repeating_table_section_bounding_box.offset.top +=
        LayoutUnit(scrollable_area->ScrollPosition().Y());
  }

  auto page_height = view->PageLogicalHeight();
  const auto& bounding_box = context_.repeating_table_section_bounding_box;
  int first_page = floorf(bounding_box.Y() / page_height);
  int last_page = ceilf(bounding_box.Bottom() / page_height) - 1;
  if (first_page >= last_page)
    return;

  context_.fragments.resize(last_page - first_page + 1);
  for (int page = first_page; page <= last_page; page++) {
    if (page > first_page)
      context_.fragments[page - first_page] = context_.fragments[0];
    context_.fragments[page - first_page].logical_top_in_flow_thread =
        page * page_height;
  }
}

bool PaintPropertyTreeBuilder::IsRepeatingInPagedMedia() const {
  return context_.is_repeating_fixed_position ||
         (context_.repeating_table_section &&
          !context_.painting_layer->EnclosingPaginationLayer());
}

void PaintPropertyTreeBuilder::CreateFragmentDataForRepeatingInPagedMedia(
    bool needs_paint_properties) {
  DCHECK(IsRepeatingInPagedMedia());

  FragmentData* fragment_data = nullptr;
  for (auto& fragment_context : context_.fragments) {
    fragment_data = fragment_data
                        ? &fragment_data->EnsureNextFragment()
                        : &object_.GetMutableForPainting().FirstFragment();
    InitFragmentPaintPropertiesForLegacy(*fragment_data, needs_paint_properties,
                                         PhysicalOffset(), fragment_context);
  }
  DCHECK(fragment_data);
  fragment_data->ClearNextFragment();
}

bool PaintPropertyTreeBuilder::UpdateFragments() {
  bool had_paint_properties = object_.FirstFragment().PaintProperties();
  bool needs_paint_properties =
#if !DCHECK_IS_ON()
      // If DCHECK is not on, use fast path for text.
      !object_.IsText() &&
#endif
      (NeedsPaintOffsetTranslation(
           object_, context_.direct_compositing_reasons) ||
       NeedsStickyTranslation(object_) ||
       NeedsTransform(object_, context_.direct_compositing_reasons) ||
       // Note: It is important to use MayNeedClipPathClip() instead of
       // NeedsClipPathClip() which requires the clip path cache to be
       // resolved, but the clip path cache invalidation must delayed until
       // the paint offset and border box has been computed.
       MayNeedClipPathClip(object_) ||
       NeedsEffect(object_, context_.direct_compositing_reasons) ||
       NeedsTransformForSVGChild(object_,
                                 context_.direct_compositing_reasons) ||
       NeedsFilter(object_, context_) || NeedsCssClip(object_) ||
       NeedsInnerBorderRadiusClip(object_) || NeedsOverflowClip(object_) ||
       NeedsPerspective(object_) || NeedsReplacedContentTransform(object_) ||
       NeedsScrollOrScrollTranslation(object_,
                                      context_.direct_compositing_reasons));

  // If the object is a text, none of the above function should return true.
  DCHECK(!needs_paint_properties || !object_.IsText());

  // Need of fragmentation clip will be determined in CreateFragmentContexts().

  if (IsInNGFragmentTraversal()) {
    InitFragmentPaintPropertiesForNG(needs_paint_properties);
  } else {
    if (object_.IsFixedPositionObjectInPagedMedia()) {
      // This flag applies to the object itself and descendants.
      context_.is_repeating_fixed_position = true;
      CreateFragmentContextsForRepeatingFixedPosition();
    } else if (ObjectIsRepeatingTableSectionInPagedMedia()) {
      context_.repeating_table_section =
          &ToInterface<LayoutNGTableSectionInterface>(object_);
      CreateFragmentContextsForRepeatingTableSectionInPagedMedia();
    }

    if (IsRepeatingInPagedMedia()) {
      CreateFragmentDataForRepeatingInPagedMedia(needs_paint_properties);
    } else if (context_.painting_layer->ShouldFragmentCompositedBounds()) {
      CreateFragmentContextsInFlowThread(needs_paint_properties);
    } else {
      InitSingleFragmentFromParent(needs_paint_properties);
      UpdateCompositedLayerPaginationOffset();
      context_.is_repeating_fixed_position = false;
      context_.repeating_table_section = nullptr;
    }
  }

  if (object_.IsSVGHiddenContainer()) {
    // SVG resources are painted within one or more other locations in the
    // SVG during paint, and hence have their own independent paint property
    // trees, paint offset, etc.
    context_.fragments.clear();
    context_.fragments.Grow(1);
    context_.has_svg_hidden_container_ancestor = true;
    PaintPropertyTreeBuilderFragmentContext& fragment_context =
        context_.fragments[0];

    fragment_context.current.paint_offset_root =
        fragment_context.absolute_position.paint_offset_root =
            fragment_context.fixed_position.paint_offset_root = &object_;

    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();
  }

  if (object_.HasLayer()) {
    To<LayoutBoxModelObject>(object_).Layer()->SetIsUnderSVGHiddenContainer(
        context_.has_svg_hidden_container_ancestor);
  }

  if (!IsInNGFragmentTraversal())
    UpdateRepeatingTableSectionPaintOffsetAdjustment();

  return needs_paint_properties != had_paint_properties;
}

bool PaintPropertyTreeBuilder::ObjectTypeMightNeedPaintProperties() const {
  return !object_.IsText() && (object_.IsBoxModelObject() || object_.IsSVG());
}

bool PaintPropertyTreeBuilder::ObjectTypeMightNeedMultipleFragmentData() const {
  return context_.painting_layer->EnclosingPaginationLayer() ||
         context_.repeating_table_section ||
         context_.is_repeating_fixed_position;
}

void PaintPropertyTreeBuilder::UpdatePaintingLayer() {
  if (object_.HasLayer() &&
      To<LayoutBoxModelObject>(object_).HasSelfPaintingLayer()) {
    context_.painting_layer = To<LayoutBoxModelObject>(object_).Layer();
  } else if (!IsInNGFragmentTraversal() &&
             (object_.IsColumnSpanAll() ||
              object_.IsFloatingWithNonContainingBlockParent())) {
    // See LayoutObject::paintingLayer() for the special-cases of floating under
    // inline and multicolumn.
    context_.painting_layer = object_.PaintingLayer();
  }
  DCHECK(context_.painting_layer == object_.PaintingLayer());
}

PaintPropertyChangeType PaintPropertyTreeBuilder::UpdateForSelf() {
  // This is not inherited from the parent context and we always recalculate it.
  context_.direct_compositing_reasons =
      CompositingReasonFinder::DirectReasonsForPaintProperties(object_);
  context_.was_layout_shift_root =
      IsLayoutShiftRoot(object_, object_.FirstFragment());
  context_.was_main_thread_scrolling = false;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      IsA<LayoutBox>(object_)) {
    if (auto* scrollable_area = To<LayoutBox>(object_).GetScrollableArea()) {
      context_.was_main_thread_scrolling =
          scrollable_area->ShouldScrollOnMainThread();
    }
  }

  UpdatePaintingLayer();

  PaintPropertyChangeType property_changed =
      PaintPropertyChangeType::kUnchanged;
  if (ObjectTypeMightNeedPaintProperties() ||
      ObjectTypeMightNeedMultipleFragmentData()) {
    if (UpdateFragments())
      property_changed = PaintPropertyChangeType::kNodeAddedOrRemoved;
  } else {
    DCHECK_EQ(context_.direct_compositing_reasons, CompositingReason::kNone);
    if (!IsInNGFragmentTraversal())
      object_.GetMutableForPainting().FirstFragment().ClearNextFragment();
  }

  if (pre_paint_info_) {
    DCHECK_EQ(context_.fragments.size(), 1u);
    FragmentPaintPropertyTreeBuilder builder(object_, pre_paint_info_, context_,
                                             context_.fragments[0],
                                             *pre_paint_info_->fragment_data);
    builder.UpdateForSelf();
    property_changed = std::max(property_changed, builder.PropertyChanged());
  } else {
    auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
    for (auto& fragment_context : context_.fragments) {
      FragmentPaintPropertyTreeBuilder builder(
          object_, /* pre_paint_info */ nullptr, context_, fragment_context,
          *fragment_data);
      builder.UpdateForSelf();
      property_changed = std::max(property_changed, builder.PropertyChanged());
      fragment_data = fragment_data->NextFragment();
    }
    DCHECK(!fragment_data);
  }

  // We need to update property tree states of paint chunks.
  if (property_changed >= PaintPropertyChangeType::kNodeAddedOrRemoved) {
    context_.painting_layer->SetNeedsRepaint();
    if (object_.IsDocumentElement()) {
      // View background painting depends on existence of the document element's
      // paint properties (see callsite of ViewPainter::PaintRootGroup()).
      // Invalidate view background display item clients.
      // SetBackgroundNeedsFullPaintInvalidation() won't work here because we
      // have already walked the LayoutView in PrePaintTreeWalk.
      LayoutView* layout_view = object_.View();
      layout_view->Layer()->SetNeedsRepaint();
      auto reason = PaintInvalidationReason::kBackground;
      static_cast<const DisplayItemClient*>(layout_view)->Invalidate(reason);
      if (auto* scrollable_area = layout_view->GetScrollableArea()) {
        scrollable_area->GetScrollingBackgroundDisplayItemClient().Invalidate(
            reason);
      }
    }
  }

  object_.GetMutableForPainting()
      .SetShouldAssumePaintOffsetTranslationForLayoutShiftTracking(false);

  return property_changed;
}

PaintPropertyChangeType PaintPropertyTreeBuilder::UpdateForChildren() {
  PaintPropertyChangeType property_changed =
      PaintPropertyChangeType::kUnchanged;
  if (!ObjectTypeMightNeedPaintProperties())
    return property_changed;

  FragmentData* fragment_data;
  if (pre_paint_info_) {
    DCHECK_EQ(context_.fragments.size(), 1u);
    fragment_data = pre_paint_info_->fragment_data;
    DCHECK(fragment_data);
  } else {
    fragment_data = &object_.GetMutableForPainting().FirstFragment();
  }

  // For now, only consider single fragment elements as possible isolation
  // boundaries.
  // TODO(crbug.com/890932): See if this is needed.
  bool is_isolated = context_.fragments.size() == 1u;
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder builder(object_, pre_paint_info_, context_,
                                             fragment_context, *fragment_data);
    // The element establishes an isolation boundary if it has isolation nodes
    // before and after updating the children. In other words, if it didn't have
    // isolation nodes previously then we still want to do a subtree walk. If it
    // now doesn't have isolation nodes, then of course it is also not isolated.
    is_isolated &= builder.HasIsolationNodes();
    builder.UpdateForChildren();
    is_isolated &= builder.HasIsolationNodes();

    property_changed = std::max(property_changed, builder.PropertyChanged());
    fragment_data = fragment_data->NextFragment();
  }

  // With NG fragment traversal we were supplied with the right FragmentData by
  // the caller, and we only ran one lap in the loop above. Whether or not there
  // are more FragmentData objects following is irrelevant then.
  DCHECK(pre_paint_info_ || !fragment_data);

  if (object_.SubtreePaintPropertyUpdateReasons() !=
      static_cast<unsigned>(SubtreePaintPropertyUpdateReason::kNone)) {
    if (AreSubtreeUpdateReasonsIsolationPiercing(
            object_.SubtreePaintPropertyUpdateReasons())) {
      context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationPiercing;
    } else {
      context_.force_subtree_update_reasons |=
          PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    }
  }
  if (object_.CanContainAbsolutePositionObjects())
    context_.container_for_absolute_position = &object_;
  if (object_.CanContainFixedPositionObjects())
    context_.container_for_fixed_position = &object_;

  if (is_isolated) {
    context_.force_subtree_update_reasons &=
        ~PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
  }

  // We need to update property tree states of paint chunks.
  if (property_changed >= PaintPropertyChangeType::kNodeAddedOrRemoved)
    context_.painting_layer->SetNeedsRepaint();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      IsA<LayoutBox>(object_)) {
    if (auto* scrollable_area = To<LayoutBox>(object_).GetScrollableArea()) {
      if (context_.was_main_thread_scrolling !=
          scrollable_area->ShouldScrollOnMainThread())
        scrollable_area->MainThreadScrollingDidChange();
    }
  }

  return property_changed;
}

}  // namespace blink
