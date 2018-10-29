// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_table_row.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_viewport_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/css_mask_painter.h"
#include "third_party/blink/renderer/core/paint/find_paint_offset_and_visual_rect_needing_update.h"
#include "third_party/blink/renderer/core/paint/find_properties_needing_update.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/paint/svg_root_painter.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/scroll/overscroll_behavior.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
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
      supports_composited_raster_invalidation(true) {}

void VisualViewportPaintPropertyTreeBuilder::Update(
    VisualViewport& visual_viewport,
    PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.fragments.IsEmpty())
    full_context.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());

  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];

  visual_viewport.UpdatePaintPropertyNodesIfNeeded(context);

  context.current.transform = visual_viewport.GetScrollTranslationNode();
  context.absolute_position.transform =
      visual_viewport.GetScrollTranslationNode();
  context.fixed_position.transform = visual_viewport.GetScrollTranslationNode();

  context.current.scroll = visual_viewport.GetScrollNode();
  context.absolute_position.scroll = visual_viewport.GetScrollNode();
  context.fixed_position.scroll = visual_viewport.GetScrollNode();

#if DCHECK_IS_ON()
  paint_property_tree_printer::UpdateDebugNames(visual_viewport);
#endif
}

void PaintPropertyTreeBuilder::SetupContextForFrame(
    LocalFrameView& frame_view,
    PaintPropertyTreeBuilderContext& full_context) {
  if (full_context.fragments.IsEmpty())
    full_context.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());

  PaintPropertyTreeBuilderFragmentContext& context = full_context.fragments[0];
  context.current.paint_offset.MoveBy(frame_view.Location());
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
 public:
  FragmentPaintPropertyTreeBuilder(
      const LayoutObject& object,
      PaintPropertyTreeBuilderContext& full_context,
      PaintPropertyTreeBuilderFragmentContext& context,
      FragmentData& fragment_data)
      : object_(object),
        full_context_(full_context),
        context_(context),
        fragment_data_(fragment_data),
        properties_(fragment_data.PaintProperties()) {}

  ~FragmentPaintPropertyTreeBuilder() {
    if (property_added_or_removed_) {
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

  bool PropertyChanged() const { return property_changed_; }
  bool PropertyAddedOrRemoved() const { return property_added_or_removed_; }
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
  ALWAYS_INLINE void UpdatePaintOffset();
  ALWAYS_INLINE void UpdateForPaintOffsetTranslation(base::Optional<IntPoint>&);
  ALWAYS_INLINE void UpdatePaintOffsetTranslation(
      const base::Optional<IntPoint>&);
  ALWAYS_INLINE void SetNeedsPaintPropertyUpdateIfNeeded();
  ALWAYS_INLINE void UpdateForObjectLocationAndSize(
      base::Optional<IntPoint>& paint_offset_translation);
  ALWAYS_INLINE void UpdateClipPathCache();
  ALWAYS_INLINE void UpdateStickyTranslation();
  ALWAYS_INLINE void UpdateTransform();
  ALWAYS_INLINE void UpdateTransformForNonRootSVG();
  ALWAYS_INLINE bool EffectCanUseCurrentClipAsOutputClip() const;
  ALWAYS_INLINE void UpdateEffect();
  ALWAYS_INLINE void UpdateLinkHighlightEffect();
  ALWAYS_INLINE void UpdateFilter();
  ALWAYS_INLINE void UpdateFragmentClip();
  ALWAYS_INLINE void UpdateCssClip();
  ALWAYS_INLINE void UpdateClipPathClip(bool spv1_compositing_specific_pass);
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

  bool NeedsPaintPropertyUpdate() const {
    return object_.NeedsPaintPropertyUpdate() ||
           full_context_.force_subtree_update_reasons;
  }

  void OnUpdate(const ObjectPaintProperties::UpdateResult& result) {
    property_added_or_removed_ |= result.NewNodeCreated();
    property_changed_ |= !result.Unchanged();
  }
  // Like |OnUpdate| but sets |clip_changed| if the clip values change.
  void OnUpdateClip(const ObjectPaintProperties::UpdateResult& result,
                    bool only_updated_hit_test_values = false) {
    OnUpdate(result);
    full_context_.clip_changed |=
        !(result.Unchanged() || only_updated_hit_test_values);
  }
  void OnClear(bool cleared) {
    property_added_or_removed_ |= cleared;
    property_changed_ |= cleared;
  }
  void OnClearClip(bool cleared) {
    OnClear(cleared);
    full_context_.clip_changed |= cleared;
  }

  const LayoutObject& object_;
  // The tree builder context for the whole object.
  PaintPropertyTreeBuilderContext& full_context_;
  // The tree builder context for the current fragment, which is one of the
  // entries in |full_context.fragments|.
  PaintPropertyTreeBuilderFragmentContext& context_;
  FragmentData& fragment_data_;
  ObjectPaintProperties* properties_;
  bool property_changed_ = false;
  bool property_added_or_removed_ = false;
};

static bool IsRootScroller(const LayoutBox& box) {
  auto* scrollable_area = box.GetScrollableArea();
  DCHECK(scrollable_area);
  auto* layer = scrollable_area->Layer();
  return layer &&
         CompositingReasonFinder::RequiresCompositingForRootScroller(*layer);
}

static bool HasScrollsOverflow(const LayoutBox& box) {
  // TODO(crbug.com/839341): Remove ScrollTimeline check once we support
  // main-thread AnimationWorklet and don't need to promote the scroll-source.
  return box.GetScrollableArea()->ScrollsOverflow() ||
         ScrollTimeline::HasActiveScrollTimeline(box.GetNode());
}

static bool NeedsScrollNode(const LayoutObject& object) {
  if (!object.HasOverflowClip())
    return false;
  const LayoutBox& box = ToLayoutBox(object);
  // TODO(pdr): SPV2 has invalidation issues (crbug.com/732611) as well as
  // subpixel issues (crbug.com/693741) which prevent us from compositing the
  // root scroller.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return HasScrollsOverflow(box);
  return HasScrollsOverflow(box) || IsRootScroller(box);
}

static CompositingReasons CompositingReasonsForScroll(const LayoutBox& box) {
  CompositingReasons compositing_reasons = CompositingReason::kNone;

  if (IsRootScroller(box))
    compositing_reasons |= CompositingReason::kRootScroller;

  // TODO(pdr): Set other compositing reasons for scroll here, see:
  // PaintLayerScrollableArea::ComputeNeedsCompositedScrolling.
  return compositing_reasons;
}

// True if a scroll translation is needed for static scroll offset (e.g.,
// overflow hidden with scroll), or if a scroll node is needed for composited
// scrolling.
static bool NeedsScrollOrScrollTranslation(const LayoutObject& object) {
  if (!object.HasOverflowClip())
    return false;
  IntSize scroll_offset = ToLayoutBox(object).ScrolledContentOffset();
  return !scroll_offset.IsZero() || NeedsScrollNode(object);
}

static bool NeedsReplacedContentTransform(const LayoutObject& object) {
  // Quick reject.
  if (!object.IsLayoutReplaced())
    return false;

  if (object.IsSVGRoot())
    return true;

  // Only directly composited images need a transform node to scale contents
  // to the object-fit box. Note that we don't actually know whether the image
  // will be directly composited. This condition is relaxed to stay on the
  // safe side.
  // TODO(crbug.com/875110): Figure out the condition for SPv2.
  bool is_spv1_composited =
      object.HasLayer() &&
      ToLayoutBoxModelObject(object).Layer()->GetCompositedLayerMapping();
  if (object.IsImage() && is_spv1_composited)
    return true;

  return false;
}

static bool NeedsPaintOffsetTranslationForScrollbars(
    const LayoutBoxModelObject& object) {
  if (auto* area = object.GetScrollableArea()) {
    if (area->HorizontalScrollbar() || area->VerticalScrollbar())
      return true;
  }
  return false;
}

static bool NeedsIsolationNodes(const LayoutObject& object) {
  if (!object.HasLayer())
    return false;

  // Paint containment establishes isolation.
  if (object.ShouldApplyPaintContainment())
    return true;

  // Layout view establishes isolation with the exception of local roots (since
  // they are already essentially isolated).
  if (RuntimeEnabledFeatures::LayoutViewIsolationNodesEnabled() &&
      object.IsLayoutView()) {
    const auto* parent_frame = object.GetFrame()->Tree().Parent();
    return parent_frame && parent_frame->IsLocalFrame();
  }
  return false;
}

static bool NeedsStickyTranslation(const LayoutObject& object) {
  if (!object.IsBoxModelObject())
    return false;

  return object.StyleRef().HasStickyConstrainedPosition();
}

static bool NeedsPaintOffsetTranslation(const LayoutObject& object) {
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

  const LayoutBoxModelObject& box_model = ToLayoutBoxModelObject(object);

  if (box_model.IsLayoutView()) {
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
  if (NeedsScrollOrScrollTranslation(object))
    return true;
  if (NeedsStickyTranslation(object))
    return true;
  if (NeedsPaintOffsetTranslationForScrollbars(box_model))
    return true;
  if (NeedsReplacedContentTransform(object))
    return true;

  // Don't let paint offset cross composited layer boundaries, to avoid
  // unnecessary full layer paint/raster invalidation when paint offset in
  // ancestor transform node changes which should not affect the descendants
  // of the composited layer.
  // TODO(wangxianzhu): For SPv2, we also need a avoid unnecessary paint/raster
  // invalidation in composited layers when their paint offset changes.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      // For only LayoutBlocks that won't be escaped by floating objects and
      // column spans when finding their containing blocks.
      // TODO(crbug.com/780242): This can be avoided if we have fully correct
      // paint property tree states for floating objects and column spans.
      (object.IsLayoutBlock() || object.IsLayoutReplaced()) &&
      object.HasLayer() &&
      !ToLayoutBoxModelObject(object).Layer()->EnclosingPaginationLayer() &&
      object.GetCompositingState() == kPaintsIntoOwnBacking)
    return true;

  return false;
}

void FragmentPaintPropertyTreeBuilder::UpdateForPaintOffsetTranslation(
    base::Optional<IntPoint>& paint_offset_translation) {
  if (!NeedsPaintOffsetTranslation(object_))
    return;

  // We should use the same subpixel paint offset values for snapping
  // regardless of whether a transform is present. If there is a transform
  // we round the paint offset but keep around the residual fractional
  // component for the transformed content to paint with.  In spv1 this was
  // called "subpixel accumulation". For more information, see
  // PaintLayer::subpixelAccumulation() and
  // PaintLayerPainter::paintFragmentByApplyingTransform.
  paint_offset_translation = RoundedIntPoint(context_.current.paint_offset);
  LayoutPoint fractional_paint_offset =
      LayoutPoint(context_.current.paint_offset - *paint_offset_translation);
  if (fractional_paint_offset != LayoutPoint()) {
    // If the object has a non-translation transform, discard the fractional
    // paint offset which can't be transformed by the transform.
    TransformationMatrix matrix;
    object_.StyleRef().ApplyTransform(
        matrix, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
    if (!matrix.IsIdentityOrTranslation())
      fractional_paint_offset = LayoutPoint();
  }
  context_.current.paint_offset = fractional_paint_offset;
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffsetTranslation(
    const base::Optional<IntPoint>& paint_offset_translation) {
  DCHECK(properties_);

  if (paint_offset_translation) {
    TransformPaintPropertyNode::State state;
    state.matrix.Translate(paint_offset_translation->X(),
                           paint_offset_translation->Y());
    state.flattens_inherited_transform =
        context_.current.should_flatten_inherited_transform;
    state.is_identity_or_2d_translation = true;

    state.affected_by_outer_viewport_bounds_delta =
        object_.StyleRef().GetPosition() == EPosition::kFixed &&
        object_.StyleRef().IsFixedToBottom();

    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
        RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
      state.rendering_context_id = context_.current.rendering_context_id;
    OnUpdate(properties_->UpdatePaintOffsetTranslation(
        *context_.current.transform, std::move(state)));
    context_.current.transform = properties_->PaintOffsetTranslation();
    if (object_.IsLayoutView()) {
      context_.absolute_position.transform =
          properties_->PaintOffsetTranslation();
      context_.fixed_position.transform = properties_->PaintOffsetTranslation();
    }
  } else {
    OnClear(properties_->ClearPaintOffsetTranslation());
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateStickyTranslation() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsStickyTranslation(object_)) {
      const auto& box_model = ToLayoutBoxModelObject(object_);
      FloatSize sticky_offset(box_model.StickyPositionOffset());
      TransformPaintPropertyNode::State state{AffineTransform::Translation(
          sticky_offset.Width(), sticky_offset.Height())};
      state.is_identity_or_2d_translation = true;
      state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
          box_model.UniqueId(),
          CompositorElementIdNamespace::kStickyTranslation);

      auto* layer = box_model.Layer();
      const auto* scroller_properties = layer->AncestorOverflowLayer()
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
      // However there is a bug that AncestorOverflowLayer() may be computed
      // incorrectly with clip escaping involved.
      if (scroller_properties &&
          scroller_properties->Scroll() == context_.current.scroll) {
        const StickyPositionScrollingConstraints& layout_constraint =
            layer->AncestorOverflowLayer()
                ->GetScrollableArea()
                ->GetStickyConstraintsMap()
                .at(layer);
        auto constraint = std::make_unique<CompositorStickyConstraint>();
        constraint->is_sticky = true;
        constraint->is_anchored_left = layout_constraint.is_anchored_left;
        constraint->is_anchored_right = layout_constraint.is_anchored_right;
        constraint->is_anchored_top = layout_constraint.is_anchored_top;
        constraint->is_anchored_bottom = layout_constraint.is_anchored_bottom;
        constraint->left_offset = layout_constraint.left_offset;
        constraint->right_offset = layout_constraint.right_offset;
        constraint->top_offset = layout_constraint.top_offset;
        constraint->bottom_offset = layout_constraint.bottom_offset;
        constraint->constraint_box_rect =
            box_model.ComputeStickyConstrainingRect();
        constraint->scroll_container_relative_sticky_box_rect = RoundedIntRect(
            layout_constraint.scroll_container_relative_sticky_box_rect);
        constraint
            ->scroll_container_relative_containing_block_rect = RoundedIntRect(
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

static bool NeedsTransformForNonRootSVG(const LayoutObject& object) {
  // TODO(pdr): Check for the presence of a transform instead of the value.
  // Checking for an identity matrix will cause the property tree structure
  // to change during animations if the animation passes through the
  // identity matrix.
  return object.IsSVGChild() &&
         !object.LocalToSVGParentTransform().IsIdentity();
}

// SVG does not use the general transform update of |UpdateTransform|, instead
// creating a transform node for SVG-specific transforms without 3D.
void FragmentPaintPropertyTreeBuilder::UpdateTransformForNonRootSVG() {
  DCHECK(properties_);
  DCHECK(object_.IsSVGChild());
  // SVG does not use paint offset internally, except for SVGForeignObject which
  // has different SVG and HTML coordinate spaces.
  DCHECK(object_.IsSVGForeignObject() ||
         context_.current.paint_offset == LayoutPoint());

  if (NeedsPaintPropertyUpdate()) {
    AffineTransform transform = object_.LocalToSVGParentTransform();
    if (NeedsTransformForNonRootSVG(object_)) {
      // The origin is included in the local transform, so leave origin empty.
      OnUpdate(properties_->UpdateTransform(
          *context_.current.transform,
          TransformPaintPropertyNode::State{transform}));
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

static CompositingReasons CompositingReasonsForTransform(const LayoutBox& box) {
  if (!box.HasLayer())
    return CompositingReason::kNone;

  const ComputedStyle& style = box.StyleRef();
  CompositingReasons compositing_reasons = CompositingReason::kNone;
  if (CompositingReasonFinder::RequiresCompositingForTransform(box))
    compositing_reasons |= CompositingReason::k3DTransform;

  if (CompositingReasonFinder::RequiresCompositingForTransformAnimation(style))
    compositing_reasons |= CompositingReason::kActiveTransformAnimation;

  if (style.HasWillChangeCompositingHint() &&
      !style.SubtreeWillChangeContents())
    compositing_reasons |= CompositingReason::kWillChangeCompositingHint;

  if (box.HasLayer() && box.Layer()->Has3DTransformedDescendant()) {
    if (style.HasPerspective())
      compositing_reasons |= CompositingReason::kPerspectiveWith3DDescendants;
    if (style.UsedTransformStyle3D() == ETransformStyle3D::kPreserve3d)
      compositing_reasons |= CompositingReason::kPreserve3DWith3DDescendants;
  }

  return compositing_reasons;
}

static FloatPoint3D TransformOrigin(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  // Transform origin has no effect without a transform or motion path.
  if (!style.HasTransform())
    return FloatPoint3D();
  FloatSize border_box_size(box.Size());
  return FloatPoint3D(
      FloatValueForLength(style.TransformOriginX(), border_box_size.Width()),
      FloatValueForLength(style.TransformOriginY(), border_box_size.Height()),
      style.TransformOriginZ());
}

static bool NeedsTransform(const LayoutObject& object) {
  if ((RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
       RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) &&
      object.StyleRef().BackfaceVisibility() == EBackfaceVisibility::kHidden)
    return true;

  if (!object.IsBox())
    return false;
  return object.StyleRef().HasTransform() || object.StyleRef().Preserves3D() ||
         CompositingReasonsForTransform(ToLayoutBox(object)) !=
             CompositingReason::kNone;
}

void FragmentPaintPropertyTreeBuilder::UpdateTransform() {
  if (object_.IsSVGChild()) {
    UpdateTransformForNonRootSVG();
    return;
  }

  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    const ComputedStyle& style = object_.StyleRef();
    // A transform node is allocated for transforms, preserves-3d and any
    // direct compositing reason. The latter is required because this is the
    // only way to represent compositing both an element and its stacking
    // descendants.
    if (NeedsTransform(object_)) {
      TransformPaintPropertyNode::State state;

      if (object_.IsBox()) {
        auto& box = ToLayoutBox(object_);
        state.origin = TransformOrigin(box);
        style.ApplyTransform(
            state.matrix, box.Size(), ComputedStyle::kExcludeTransformOrigin,
            ComputedStyle::kIncludeMotionPath,
            ComputedStyle::kIncludeIndependentTransformProperties);

        if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
            RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
          // TODO(trchen): transform-style should only be respected if a
          // PaintLayer is created. If a node with transform-style: preserve-3d
          // does not exist in an existing rendering context, it establishes a
          // new one.
          state.rendering_context_id = context_.current.rendering_context_id;
          if (style.Preserves3D() && !state.rendering_context_id) {
            state.rendering_context_id =
                PtrHash<const LayoutObject>::GetHash(&object_);
          }
          state.direct_compositing_reasons =
              CompositingReasonsForTransform(box);
        }
      }

      state.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;

      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
        state.backface_visibility =
            object_.HasHiddenBackface()
                ? TransformPaintPropertyNode::BackfaceVisibility::kHidden
                : TransformPaintPropertyNode::BackfaceVisibility::kVisible;
        state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
            object_.UniqueId(), CompositorElementIdNamespace::kPrimary);
      }

      OnUpdate(properties_->UpdateTransform(*context_.current.transform,
                                            std::move(state)));
    } else {
      OnClear(properties_->ClearTransform());
    }
  }

  if (properties_->Transform()) {
    context_.current.transform = properties_->Transform();
    if (object_.StyleRef().Preserves3D()) {
      context_.current.rendering_context_id =
          properties_->Transform()->RenderingContextId();
      context_.current.should_flatten_inherited_transform = false;
    } else {
      context_.current.rendering_context_id = 0;
      context_.current.should_flatten_inherited_transform = true;
    }
  }
}

static bool NeedsClipPathClip(const LayoutObject& object) {
  if (!object.StyleRef().ClipPath())
    return false;

  return object.FirstFragment().ClipPathPath();
}

static bool NeedsEffect(const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();

  // For now some objects (e.g. LayoutTableCol) with stacking context style
  // don't create layer thus are not actual stacking contexts, so the HasLayer()
  // condition. TODO(crbug.com/892734): Support effects for LayoutTableCol.
  const bool is_css_isolated_group =
      object.HasLayer() && style.IsStackingContext();

  if (!is_css_isolated_group && !object.IsSVG())
    return false;

  if (object.IsSVG()) {
    if (SVGLayoutSupport::IsIsolationRequired(&object))
      return true;
    if (SVGResources* resources =
            SVGResourcesCache::CachedResourcesForLayoutObject(object)) {
      if (resources->Masker()) {
        return true;
      }
    }
  }

  if (is_css_isolated_group) {
    if (object.IsSVGRoot() && object.HasNonIsolatedBlendingDescendants())
      return true;

    const auto* layer = ToLayoutBoxModelObject(object).Layer();
    DCHECK(layer);

    if (layer->HasNonIsolatedDescendantWithBlendMode())
      return true;

    // In SPv1* a mask layer can be created for clip-path in absence of mask,
    // and a mask effect node must be created whether the clip-path is
    // path-based or not.
    if (layer->GetCompositedLayerMapping() &&
        layer->GetCompositedLayerMapping()->MaskLayer())
      return true;

    // An effect node is required by cc if the layer flattens its subtree but it
    // is treated as a 3D object by its parent.
    if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
        !layer->Preserves3D() && layer->HasSelfPaintingLayerDescendant() &&
        layer->Parent() && layer->Parent()->Preserves3D())
      return true;
  }

  SkBlendMode blend_mode = object.IsBlendingAllowed()
                               ? WebCoreCompositeToSkiaComposite(
                                     kCompositeSourceOver, style.GetBlendMode())
                               : SkBlendMode::kSrcOver;
  if (blend_mode != SkBlendMode::kSrcOver)
    return true;

  if (style.Opacity() != 1.0f || style.HasWillChangeOpacityHint())
    return true;

  if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(style))
    return true;

  if (object.StyleRef().HasMask())
    return true;

  if (object.StyleRef().ClipPath() &&
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
  DCHECK(NeedsEffect(object_));

  if (!object_.HasLayer()) {
    // An SVG object's effect never interleaves with clips.
    DCHECK(object_.IsSVG());
    return true;
  }

  const auto* layer = ToLayoutBoxModelObject(object_).Layer();
  // Out-of-flow descendants not contained by this object may escape clips.
  if (layer->HasNonContainedAbsolutePositionDescendant() &&
      object_.ContainerForAbsolutePosition()
              ->FirstFragment()
              .PostOverflowClip() != context_.current.clip)
    return false;
  if (layer->HasFixedPositionDescendant() &&
      !object_.CanContainFixedPositionObjects() &&
      object_.ContainerForFixedPosition()->FirstFragment().PostOverflowClip() !=
          context_.current.clip)
    return false;

  // Some descendants under a pagination container (e.g. composited objects
  // in SPv1 and column spanners) may escape fragment clips.
  if (layer->EnclosingPaginationLayer())
    return false;

  return true;
}

void FragmentPaintPropertyTreeBuilder::UpdateEffect() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsEffect(object_)) {
      base::Optional<IntRect> mask_clip = CSSMaskPainter::MaskBoundingBox(
          object_, context_.current.paint_offset);
      bool has_clip_path =
          style.ClipPath() && fragment_data_.ClipPathBoundingBox();
      bool is_spv1_composited =
          object_.HasLayer() &&
          ToLayoutBoxModelObject(object_).Layer()->GetCompositedLayerMapping();
      bool has_spv1_composited_clip_path = has_clip_path && is_spv1_composited;
      bool has_mask_based_clip_path =
          has_clip_path && !fragment_data_.ClipPathPath();
      base::Optional<IntRect> clip_path_clip;
      if (has_spv1_composited_clip_path || has_mask_based_clip_path) {
        clip_path_clip = fragment_data_.ClipPathBoundingBox();
      } else if (!mask_clip && is_spv1_composited &&
                 ToLayoutBoxModelObject(object_)
                     .Layer()
                     ->GetCompositedLayerMapping()
                     ->MaskLayer()) {
        // TODO(crbug.com/856818): This should never happen.
        // This is a band-aid to avoid nullptr properties on the mask layer
        // crashing the renderer, but will result in incorrect rendering.
        NOTREACHED();
        has_spv1_composited_clip_path = true;
        clip_path_clip = IntRect();
      }

      const auto* output_clip = EffectCanUseCurrentClipAsOutputClip()
                                    ? context_.current.clip
                                    : nullptr;

      if (mask_clip || clip_path_clip) {
        IntRect combined_clip = mask_clip ? *mask_clip : *clip_path_clip;
        if (mask_clip && clip_path_clip)
          combined_clip.Intersect(*clip_path_clip);

        OnUpdateClip(properties_->UpdateMaskClip(
            *context_.current.clip,
            ClipPaintPropertyNode::State{context_.current.transform,
                                         FloatRoundedRect(combined_clip)}));
        output_clip = properties_->MaskClip();
      } else {
        OnClearClip(properties_->ClearMaskClip());
      }

      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.output_clip = output_clip;
      state.opacity = style.Opacity();
      if (object_.IsBlendingAllowed()) {
        state.blend_mode = WebCoreCompositeToSkiaComposite(
            kCompositeSourceOver, style.GetBlendMode());
      }
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
        // We may begin to composite our subtree prior to an animation starts,
        // but a compositor element ID is only needed when an animation is
        // current.
        if (CompositingReasonFinder::RequiresCompositingForOpacityAnimation(
                style)) {
          state.direct_compositing_reasons =
              CompositingReason::kActiveOpacityAnimation;
        }
        state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
            object_.UniqueId(), CompositorElementIdNamespace::kPrimary);
      }
      OnUpdate(properties_->UpdateEffect(*context_.current_effect,
                                         std::move(state)));

      if (mask_clip || has_spv1_composited_clip_path) {
        EffectPaintPropertyNode::State mask_state;
        mask_state.local_transform_space = context_.current.transform;
        mask_state.output_clip = output_clip;
        mask_state.color_filter = CSSMaskPainter::MaskColorFilter(object_);
        mask_state.blend_mode = SkBlendMode::kDstIn;
        if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
            RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
          mask_state.compositor_element_id =
              CompositorElementIdFromUniqueObjectId(
                  object_.UniqueId(),
                  CompositorElementIdNamespace::kEffectMask);
        }
        OnUpdate(properties_->UpdateMask(*properties_->Effect(),
                                         std::move(mask_state)));
      } else {
        OnClear(properties_->ClearMask());
      }

      if (has_mask_based_clip_path) {
        const EffectPaintPropertyNode& parent = has_spv1_composited_clip_path
                                                    ? *properties_->Mask()
                                                    : *properties_->Effect();
        EffectPaintPropertyNode::State clip_path_state;
        clip_path_state.local_transform_space = context_.current.transform;
        clip_path_state.output_clip = output_clip;
        clip_path_state.blend_mode = SkBlendMode::kDstIn;
        if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
            RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
          clip_path_state.compositor_element_id =
              CompositorElementIdFromUniqueObjectId(
                  object_.UniqueId(),
                  CompositorElementIdNamespace::kEffectClipPath);
        }
        OnUpdate(
            properties_->UpdateClipPath(parent, std::move(clip_path_state)));
      } else {
        OnClear(properties_->ClearClipPath());
      }
    } else {
      OnClear(properties_->ClearEffect());
      OnClear(properties_->ClearMask());
      OnClear(properties_->ClearClipPath());
      OnClearClip(properties_->ClearMaskClip());
    }
  }

  if (properties_->Effect()) {
    context_.current_effect = properties_->Effect();
    if (properties_->MaskClip()) {
      context_.current.clip = context_.absolute_position.clip =
          context_.fixed_position.clip = properties_->MaskClip();
    }
  }
}

static bool NeedsLinkHighlightEffect(const LayoutObject& object) {
  auto* page = object.GetFrame()->GetPage();
  return page->GetLinkHighlights().NeedsHighlightEffect(object);
}

void FragmentPaintPropertyTreeBuilder::UpdateLinkHighlightEffect() {
  if (NeedsPaintPropertyUpdate()) {
    if (NeedsLinkHighlightEffect(object_)) {
      // While the link highlight uses the current transform space for
      // positioning, it's parent effect is the root so that it is not affected
      // by enclosing filters.
      const auto& parent = EffectPaintPropertyNode::Root();
      EffectPaintPropertyNode::State link_highlight_state;
      link_highlight_state.local_transform_space = context_.current.transform;
      link_highlight_state.compositor_element_id =
          object_.GetFrame()->GetPage()->GetLinkHighlights().element_id(
              object_);
      link_highlight_state.direct_compositing_reasons =
          CompositingReason::kActiveOpacityAnimation;
      // Unlike other property nodes, link highlight effect nodes are guaranteed
      // to be leaf nodes and do not require subtree invalidation, so we do not
      // call |OnUpdate| here.
      properties_->UpdateLinkHighlightEffect(parent,
                                             std::move(link_highlight_state));
    } else {
      // Unlike other property nodes, link highlight effect nodes are guaranteed
      // to be leaf nodes and do not require subtree invalidation, so we do not
      // call |OnClear| here.
      properties_->ClearLinkHighlightEffect();
    }
  }
}

static bool NeedsFilter(const LayoutObject& object) {
  // TODO(trchen): SVG caches filters in SVGResources. Implement it.
  return (object.IsBoxModelObject() && ToLayoutBoxModelObject(object).Layer() &&
          (object.StyleRef().HasFilter() || object.HasReflection() ||
           CompositingReasonFinder::RequiresCompositingForFilterAnimation(
               object.StyleRef())));
}

void FragmentPaintPropertyTreeBuilder::UpdateFilter() {
  DCHECK(properties_);
  const ComputedStyle& style = object_.StyleRef();

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsFilter(object_)) {
      EffectPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.filters_origin = FloatPoint(context_.current.paint_offset);

      if (auto* layer = ToLayoutBoxModelObject(object_).Layer()) {
        // Try to use the cached filter.
        if (properties_->Filter())
          state.filter = properties_->Filter()->Filter();

        layer->UpdateCompositorFilterOperationsForFilter(state.filter);
        layer->ClearFilterOnEffectNodeDirty();
      }

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
      state.output_clip = context_.current.clip;

      // TODO(trchen): A filter may contain spatial operations such that an
      // output pixel may depend on an input pixel outside of the output clip.
      // We should generate a special clip node to represent this expansion.

      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
        // We may begin to composite our subtree prior to an animation starts,
        // but a compositor element ID is only needed when an animation is
        // current.
        state.direct_compositing_reasons =
            CompositingReasonFinder::RequiresCompositingForFilterAnimation(
                style)
                ? CompositingReason::kActiveFilterAnimation
                : CompositingReason::kNone;
        DCHECK(!style.HasCurrentFilterAnimation() ||
               state.direct_compositing_reasons != CompositingReason::kNone);

        state.compositor_element_id = CompositorElementIdFromUniqueObjectId(
            object_.UniqueId(), CompositorElementIdNamespace::kEffectFilter);
      }

      OnUpdate(properties_->UpdateFilter(*context_.current_effect,
                                         std::move(state)));
    } else {
      OnClear(properties_->ClearFilter());
    }
  }

  if (properties_->Filter()) {
    context_.current_effect = properties_->Filter();
    // TODO(trchen): Change input clip to expansion hint once implemented.
    const ClipPaintPropertyNode* input_clip =
        properties_->Filter()->OutputClip();
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = input_clip;
  }
}

static FloatRoundedRect ToClipRect(const LayoutRect& rect) {
  return FloatRoundedRect(FloatRect(PixelSnappedIntRect(rect)));
}

void FragmentPaintPropertyTreeBuilder::UpdateFragmentClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (context_.fragment_clip) {
      OnUpdateClip(properties_->UpdateFragmentClip(
          *context_.current.clip,
          ClipPaintPropertyNode::State{context_.current.transform,
                                       ToClipRect(*context_.fragment_clip)}));
    } else {
      OnClearClip(properties_->ClearFragmentClip());
    }
  }

  if (properties_->FragmentClip())
    context_.current.clip = properties_->FragmentClip();
}

static bool NeedsCssClip(const LayoutObject& object) {
  return object.HasClip();
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
      OnUpdateClip(properties_->UpdateCssClip(
          *context_.current.clip,
          ClipPaintPropertyNode::State{context_.current.transform,
                                       ToClipRect(ToLayoutBox(object_).ClipRect(
                                           context_.current.paint_offset))}));
    } else {
      OnClearClip(properties_->ClearCssClip());
    }
  }

  if (properties_->CssClip())
    context_.current.clip = properties_->CssClip();
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathClip(
    bool spv1_compositing_specific_pass) {
  // In SPv1*, composited path-based clip-path applies to a mask paint chunk
  // instead of actual contents. We have to delay until mask clip node has been
  // created first so we can parent under it.
  bool is_spv1_composited =
      object_.HasLayer() &&
      ToLayoutBoxModelObject(object_).Layer()->GetCompositedLayerMapping();
  if (is_spv1_composited != spv1_compositing_specific_pass)
    return;

  if (NeedsPaintPropertyUpdate()) {
    if (!NeedsClipPathClip(object_)) {
      OnClearClip(properties_->ClearClipPathClip());
    } else {
      ClipPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.clip_rect =
          FloatRoundedRect(FloatRect(*fragment_data_.ClipPathBoundingBox()));
      state.clip_path = fragment_data_.ClipPathPath();
      OnUpdateClip(properties_->UpdateClipPathClip(*context_.current.clip,
                                                   std::move(state)));
    }
  }

  if (properties_->ClipPathClip() && !spv1_compositing_specific_pass) {
    context_.current.clip = context_.absolute_position.clip =
        context_.fixed_position.clip = properties_->ClipPathClip();
  }
}

// Returns true if we are printing which was initiated by the frame. We should
// ignore clipping and scroll transform on contents. WebLocalFrameImpl will
// issue artificial page clip for each page, and always print from the origin
// of the contents for which no scroll offset should be applied.
static bool IsPrintingRootLayoutView(const LayoutObject& object) {
  if (!object.IsLayoutView())
    return false;

  const auto& frame = *object.GetFrame();
  if (!frame.GetDocument()->Printing())
    return false;

  const auto* parent_frame = frame.Tree().Parent();
  if (!parent_frame)
    return true;
  // TODO(crbug.com/455764): The local frame may be not the root frame of
  // printing when it's printing under a remote frame.
  if (!parent_frame->IsLocalFrame())
    return true;

  // If the parent frame is printing, this frame should clip normally.
  return !ToLocalFrame(parent_frame)->GetDocument()->Printing();
}

static bool NeedsOverflowClipForReplacedContents(
    const LayoutReplaced& replaced) {
  // <svg> may optionally allow overflow. If an overflow clip is required,
  // always create it without checking whether the actual content overflows.
  if (replaced.IsSVGRoot())
    return ToLayoutSVGRoot(replaced).ShouldApplyViewportClip();

  if (replaced.StyleRef().HasBorderRadius())
    return true;

  // Non-composited images have a micro-optimization to embed clip rects into
  // the drawings instead of using a clip node.
  bool is_spv1_composited =
      replaced.HasLayer() && replaced.Layer()->GetCompositedLayerMapping();
  if (replaced.IsImage() && !is_spv1_composited)
    return false;

  // Embedded objects are always sized to fit the content rect.
  if (replaced.IsLayoutEmbeddedContent())
    return false;

  return true;
}

static bool NeedsOverflowClip(const LayoutObject& object) {
  if (object.IsLayoutReplaced())
    return NeedsOverflowClipForReplacedContents(ToLayoutReplaced(object));

  if (object.IsSVGViewportContainer() &&
      SVGLayoutSupport::IsOverflowHidden(object))
    return true;

  return object.IsBox() && ToLayoutBox(object).ShouldClipOverflow() &&
         !IsPrintingRootLayoutView(object);
}

void FragmentPaintPropertyTreeBuilder::UpdateLocalBorderBoxContext() {
  if (!NeedsPaintPropertyUpdate())
    return;

  if (!object_.HasLayer() && !NeedsPaintOffsetTranslation(object_) &&
      !NeedsFilter(object_) && !NeedsOverflowClip(object_)) {
    fragment_data_.ClearLocalBorderBoxProperties();
  } else {
    PropertyTreeState local_border_box =
        PropertyTreeState(context_.current.transform, context_.current.clip,
                          context_.current_effect);

    if (!fragment_data_.HasLocalBorderBoxProperties() ||
        local_border_box != fragment_data_.LocalBorderBoxProperties())
      property_added_or_removed_ = true;

    fragment_data_.SetLocalBorderBoxProperties(std::move(local_border_box));
  }
}

bool FragmentPaintPropertyTreeBuilder::NeedsOverflowControlsClip() const {
  if (!object_.HasOverflowClip())
    return false;

  const auto& box = ToLayoutBox(object_);
  const auto* scrollable_area = box.GetScrollableArea();
  IntRect scroll_controls_bounds =
      scrollable_area->ScrollCornerAndResizerRect();
  if (const auto* scrollbar = scrollable_area->HorizontalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  if (const auto* scrollbar = scrollable_area->VerticalScrollbar())
    scroll_controls_bounds.Unite(scrollbar->FrameRect());
  auto pixel_snapped_border_box_rect = box.PixelSnappedBorderBoxRect(
      ToLayoutSize(context_.current.paint_offset));
  pixel_snapped_border_box_rect.SetLocation(IntPoint());
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

static LayoutPoint VisualOffsetFromPaintOffsetRoot(
    const PaintPropertyTreeBuilderFragmentContext& context,
    const PaintLayer* child) {
  const LayoutObject* paint_offset_root = context.current.paint_offset_root;
  PaintLayer* painting_layer = paint_offset_root->PaintingLayer();
  LayoutPoint result = child->VisualOffsetFromAncestor(painting_layer);
  if (!paint_offset_root->HasLayer() ||
      ToLayoutBoxModelObject(paint_offset_root)->Layer() != painting_layer) {
    result.Move(-paint_offset_root->OffsetFromAncestor(
        &painting_layer->GetLayoutObject()));
  }

  // Convert the result into the space of the scrolling contents space.
  if (const auto* properties =
          paint_offset_root->FirstFragment().PaintProperties()) {
    if (const auto* scroll_translation = properties->ScrollTranslation()) {
      DCHECK(scroll_translation->Matrix().IsIdentityOr2DTranslation());
      result += -LayoutSize(scroll_translation->Matrix().To2DTranslation());
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
    // OnUpdateClip() because this clip doesn't affect descendants.
    properties_->UpdateOverflowControlsClip(
        *context_.current.clip,
        ClipPaintPropertyNode::State{
            context_.current.transform,
            ToClipRect(LayoutRect(context_.current.paint_offset,
                                  ToLayoutBox(object_).Size()))});
  } else {
    properties_->ClearOverflowControlsClip();
  }

  // No need to set force_subtree_update_reasons and clip_changed because
  // OverflowControlsClip applies to overflow controls only, not descendants.
  // We also don't walk into custom scrollbars in PrePaintTreeWalk and
  // LayoutObjects under custom scrollbars don't support paint properties.
}

void FragmentPaintPropertyTreeBuilder::UpdateInnerBorderRadiusClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsInnerBorderRadiusClip(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
      ClipPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;
      state.clip_rect = box.StyleRef().GetRoundedInnerBorderFor(
          LayoutRect(context_.current.paint_offset, box.Size()));
      OnUpdateClip(properties_->UpdateInnerBorderRadiusClip(
          *context_.current.clip, std::move(state)));
    } else {
      OnClearClip(properties_->ClearInnerBorderRadiusClip());
    }
  }

  if (auto* border_radius_clip = properties_->InnerBorderRadiusClip())
    context_.current.clip = border_radius_clip;
}

static bool CanOmitOverflowClip(const LayoutObject& object) {
  DCHECK(NeedsOverflowClip(object));
  // Some non-block boxes and SVG objects have special overflow rules.
  if (!object.IsLayoutBlock() || object.IsSVG())
    return false;

  const auto& block = ToLayoutBlock(object);
  // This is a heuristic to avoid costly paint property subtree rebuild on
  // CanOmitOverflowClip() changes, e.g. on selection. This also avoids omitting
  // overflow clip when there is any self-painting descendant which is not
  // covered by ContentsVisualOverflowRect().
  if (block.HasLayer() && block.Layer()->FirstChild())
    return false;
  // Selection may overflow.
  if (block.IsSelected())
    return false;
  // Other cases that the contents may overflow. The conditions are copied from
  // BlockPainter for SPv1 clip. TODO(wangxianzhu): clean up.
  if (block.HasControlClip() || block.ShouldPaintCarets())
    return false;

  if (object.IsLayoutReplaced()) {
    const LayoutReplaced& replaced = ToLayoutReplaced(object);
    if (replaced.StyleRef().HasBorderRadius())
      return false;
    LayoutRect replaced_content_rect = replaced.ReplacedContentRect();
    return replaced_content_rect.IsEmpty() ||
           replaced.PhysicalContentBoxRect().Contains(replaced_content_rect);
  }

  // We need OverflowClip for hit-testing if the clip rect excluding overlay
  // scrollbars is different from the normal clip rect.
  auto clip_rect = block.OverflowClipRect(LayoutPoint());
  auto clip_rect_excluding_overlay_scrollbars = block.OverflowClipRect(
      LayoutPoint(), kExcludeOverlayScrollbarSizeForHitTesting);
  if (clip_rect != clip_rect_excluding_overlay_scrollbars)
    return false;
  return clip_rect.Contains(block.ContentsVisualOverflowRect());
}

void FragmentPaintPropertyTreeBuilder::UpdateOverflowClip() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsOverflowClip(object_) && !CanOmitOverflowClip(object_)) {
      ClipPaintPropertyNode::State state;
      state.local_transform_space = context_.current.transform;

      if (object_.IsLayoutReplaced()) {
        const LayoutReplaced& replaced = ToLayoutReplaced(object_);
        // LayoutReplaced clips the foreground by rounded content box.
        state.clip_rect = replaced.StyleRef().GetRoundedInnerBorderFor(
            LayoutRect(context_.current.paint_offset, replaced.Size()),
            LayoutRectOutsets(
                -(replaced.PaddingTop() + replaced.BorderTop()),
                -(replaced.PaddingRight() + replaced.BorderRight()),
                -(replaced.PaddingBottom() + replaced.BorderBottom()),
                -(replaced.PaddingLeft() + replaced.BorderLeft())));
        if (replaced.IsLayoutEmbeddedContent()) {
          // Embedded objects are always sized to fit the content rect, but
          // they could overflow by 1px due to pre-snapping. Adjust clip rect
          // to match pre-snapped box as a special case.
          FloatRect adjusted_rect = state.clip_rect.Rect();
          adjusted_rect.SetSize(
              FloatSize(replaced.ReplacedContentRect().Size()));
          state.clip_rect.SetRect(adjusted_rect);
        }
      } else if (object_.IsBox()) {
        state.clip_rect = ToClipRect(ToLayoutBox(object_).OverflowClipRect(
            context_.current.paint_offset));
        state.clip_rect_excluding_overlay_scrollbars =
            ToClipRect(ToLayoutBox(object_).OverflowClipRect(
                context_.current.paint_offset,
                kExcludeOverlayScrollbarSizeForHitTesting));
      } else {
        DCHECK(object_.IsSVGViewportContainer());
        const auto& viewport_container = ToLayoutSVGViewportContainer(object_);
        state.clip_rect = FloatRoundedRect(
            viewport_container.LocalToSVGParentTransform().Inverse().MapRect(
                viewport_container.Viewport()));
      }

      const ClipPaintPropertyNode* existing = properties_->OverflowClip();
      bool equal_ignoring_hit_test_rects =
          !!existing &&
          existing->EqualIgnoringHitTestRects(context_.current.clip, state);
      OnUpdateClip(properties_->UpdateOverflowClip(*context_.current.clip,
                                                   std::move(state)),
                   equal_ignoring_hit_test_rects);
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
      TransformPaintPropertyNode::State state;
      state.matrix.ApplyPerspective(style.Perspective());
      state.origin = PerspectiveOrigin(ToLayoutBox(object_)) +
                     ToLayoutSize(context_.current.paint_offset);
      state.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
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

static bool ImageWasTransposed(const LayoutImage& layout_image,
                               const Image& image) {
  return LayoutObject::ShouldRespectImageOrientation(&layout_image) ==
             kRespectImageOrientation &&
         image.IsBitmapImage() &&
         ToBitmapImage(image).CurrentFrameOrientation().UsesWidthAsHeight();
}

static AffineTransform RectToRect(const FloatRect& src_rect,
                                  const FloatRect& dst_rect) {
  float x_scale = dst_rect.Width() / src_rect.Width();
  float y_scale = dst_rect.Height() / src_rect.Height();
  float x_offset = dst_rect.X() - src_rect.X() * x_scale;
  float y_offset = dst_rect.Y() - src_rect.Y() * y_scale;
  return AffineTransform(x_scale, 0.f, 0.f, y_scale, x_offset, y_offset);
}

void FragmentPaintPropertyTreeBuilder::UpdateReplacedContentTransform() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate() && !NeedsReplacedContentTransform(object_)) {
    OnClear(properties_->ClearReplacedContentTransform());
  } else if (NeedsPaintPropertyUpdate()) {
    AffineTransform content_to_parent_space;
    if (object_.IsSVGRoot()) {
      content_to_parent_space =
          SVGRootPainter(ToLayoutSVGRoot(object_))
              .TransformToPixelSnappedBorderBox(context_.current.paint_offset);
    } else if (object_.IsImage()) {
      const LayoutImage& layout_image = ToLayoutImage(object_);
      LayoutRect layout_replaced_rect = layout_image.ReplacedContentRect();
      layout_replaced_rect.MoveBy(context_.current.paint_offset);
      IntRect replaced_rect = PixelSnappedIntRect(layout_replaced_rect);
      scoped_refptr<Image> image = layout_image.ImageResource()->GetImage(
          LayoutSize(replaced_rect.Size()));
      if (image && !image->IsNull()) {
        IntRect src_rect = image->Rect();
        if (ImageWasTransposed(layout_image, *image))
          src_rect = src_rect.TransposedRect();
        content_to_parent_space =
            RectToRect(FloatRect(src_rect), FloatRect(replaced_rect));
      }
    } else {
      NOTREACHED();
    }
    if (!content_to_parent_space.IsIdentity()) {
      OnUpdate(properties_->UpdateReplacedContentTransform(
          *context_.current.transform,
          TransformPaintPropertyNode::State{content_to_parent_space}));
    } else {
      OnClear(properties_->ClearReplacedContentTransform());
    }
  }

  if (object_.IsSVGRoot()) {
    // SVG painters don't use paint offset. The paint offset is baked into
    // the transform node instead.
    context_.current.paint_offset = LayoutPoint();

    // Only <svg> paints its subtree as replaced contents. Other replaced
    // element type may have shadow DOM that should not be affected by the
    // replaced object fit.
    if (properties_->ReplacedContentTransform()) {
      context_.current.transform = properties_->ReplacedContentTransform();
      context_.current.should_flatten_inherited_transform = false;
      context_.current.rendering_context_id = 0;
    }
  }
}

static MainThreadScrollingReasons GetMainThreadScrollingReasons(
    const LayoutObject& object,
    MainThreadScrollingReasons ancestor_reasons) {
  // The current main thread scrolling reasons implementation only changes
  // reasons at frame boundaries, so we can early-out when not at a LayoutView.
  // TODO(pdr): Need to find a solution to the style-related main thread
  // scrolling reasons such as opacity and transform which violate this.
  if (!object.IsLayoutView())
    return ancestor_reasons;

  auto reasons = ancestor_reasons;
  if (!object.GetFrame()->GetSettings()->GetThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;
  if (object.GetFrameView()->HasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  return reasons;
}

void FragmentPaintPropertyTreeBuilder::UpdateScrollAndScrollTranslation() {
  DCHECK(properties_);

  if (NeedsPaintPropertyUpdate()) {
    if (NeedsScrollNode(object_)) {
      const LayoutBox& box = ToLayoutBox(object_);
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

      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
        state.compositor_element_id = scrollable_area->GetCompositorElementId();

      state.overscroll_behavior = OverscrollBehavior(
          static_cast<OverscrollBehavior::OverscrollBehaviorType>(
              box.StyleRef().OverscrollBehaviorX()),
          static_cast<OverscrollBehavior::OverscrollBehaviorType>(
              box.StyleRef().OverscrollBehaviorY()));

      auto* snap_coordinator = box.GetDocument().GetSnapCoordinator();
      if (snap_coordinator) {
        state.snap_container_data = snap_coordinator->GetSnapContainerData(box);
      }

      OnUpdate(properties_->UpdateScroll(*context_.current.scroll,
                                         std::move(state)));

      if (scrollable_area->VerticalScrollbar() ||
          scrollable_area->HasLayerForVerticalScrollbar()) {
        EffectPaintPropertyNode::State state;
        state.local_transform_space = context_.current.transform;
        state.direct_compositing_reasons =
            CompositingReason::kActiveOpacityAnimation;
        state.compositor_element_id = scrollable_area->GetScrollbarElementId(
            ScrollbarOrientation::kVerticalScrollbar);
        OnUpdate(properties_->UpdateVerticalScrollbarEffect(
            *context_.current_effect, std::move(state)));
      } else {
        OnClear(properties_->ClearVerticalScrollbarEffect());
      }

      if (scrollable_area->HorizontalScrollbar() ||
          scrollable_area->HasLayerForHorizontalScrollbar()) {
        EffectPaintPropertyNode::State state;
        state.local_transform_space = context_.current.transform;
        state.direct_compositing_reasons =
            CompositingReason::kActiveOpacityAnimation;
        state.compositor_element_id = scrollable_area->GetScrollbarElementId(
            ScrollbarOrientation::kHorizontalScrollbar);
        OnUpdate(properties_->UpdateHorizontalScrollbarEffect(
            *context_.current_effect, std::move(state)));
      } else {
        OnClear(properties_->ClearHorizontalScrollbarEffect());
      }
    } else {
      OnClear(properties_->ClearScroll());
      OnClear(properties_->ClearVerticalScrollbarEffect());
      OnClear(properties_->ClearHorizontalScrollbarEffect());
    }

    // A scroll translation node is created for static offset (e.g., overflow
    // hidden with scroll offset) or cases that scroll and have a scroll node.
    if (NeedsScrollOrScrollTranslation(object_)) {
      const auto& box = ToLayoutBox(object_);
      TransformPaintPropertyNode::State state;
      // Bake ScrollOrigin into ScrollTranslation. See comments for
      // ScrollTranslation in object_paint_properties.h for details.
      auto scroll_position = box.ScrollOrigin() + box.ScrolledContentOffset();
      state.matrix.Translate(-scroll_position.X(), -scroll_position.Y());
      state.flattens_inherited_transform =
          context_.current.should_flatten_inherited_transform;
      state.is_identity_or_2d_translation = true;
      state.direct_compositing_reasons = CompositingReasonsForScroll(box);
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
          RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
        state.rendering_context_id = context_.current.rendering_context_id;
      }
      state.scroll = properties_->Scroll();
      OnUpdate(properties_->UpdateScrollTranslation(*context_.current.transform,
                                                    std::move(state)));
    } else {
      OnClear(properties_->ClearScrollTranslation());
    }
  }

  if (properties_->Scroll())
    context_.current.scroll = properties_->Scroll();

  if (properties_->ScrollTranslation()) {
    context_.current.transform = properties_->ScrollTranslation();
    // See comments for ScrollTranslation in object_paint_properties.h for the
    // reason of adding ScrollOrigin().
    context_.current.paint_offset += ToLayoutBox(object_).ScrollOrigin();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateOutOfFlowContext() {
  if (!object_.IsBoxModelObject() && !properties_)
    return;

  if (object_.IsLayoutBlock())
    context_.paint_offset_for_float = context_.current.paint_offset;

  if (object_.CanContainAbsolutePositionObjects())
    context_.absolute_position = context_.current;

  if (object_.IsLayoutView()) {
    const auto* initial_fixed_transform = context_.fixed_position.transform;
    const auto* initial_fixed_scroll = context_.fixed_position.scroll;

    context_.fixed_position = context_.current;
    context_.fixed_position.fixed_position_children_fixed_to_root = true;

    // Fixed position transform and scroll nodes should not be affected.
    context_.fixed_position.transform = initial_fixed_transform;
    context_.fixed_position.scroll = initial_fixed_scroll;
    if (properties_->ScrollTranslation()) {
      // Also undo the ScrollOrigin part in paint offset that was added when
      // ScrollTranslation was updated.
      context_.fixed_position.paint_offset -=
          ToLayoutBox(object_).ScrollOrigin();
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
            ClipPaintPropertyNode::State{css_clip->LocalTransformSpace(),
                                         css_clip->ClipRect()}));
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
          *context_.current.transform, TransformPaintPropertyNode::State{},
          true /* is_parent_alias */));
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
      OnUpdate(properties_->UpdateEffectIsolationNode(
          *context_.current_effect, EffectPaintPropertyNode::State{},
          true /* is_parent_alias */));
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
      OnUpdate(properties_->UpdateClipIsolationNode(
          *context_.current.clip, ClipPaintPropertyNode::State{},
          true /* is_parent_alias */));
    } else {
      OnClear(properties_->ClearClipIsolationNode());
    }
  }
  if (properties_->ClipIsolationNode())
    context_.current.clip = properties_->ClipIsolationNode();
}

static LayoutRect MapLocalRectToAncestorLayer(
    const LayoutBox& box,
    const LayoutRect& local_rect,
    const PaintLayer& ancestor_layer) {
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint(local_rect.Location()));
  box.MapLocalToAncestor(&ancestor_layer.GetLayoutObject(), transform_state,
                         kApplyContainerFlip);
  transform_state.Flatten();
  return LayoutRect(LayoutPoint(transform_state.LastPlanarPoint()),
                    local_rect.Size());
}

static bool IsRepeatingTableSection(const LayoutObject& object) {
  if (!object.IsTableSection())
    return false;
  const auto& section = ToLayoutTableSection(object);
  return section.IsRepeatingHeaderGroup() || section.IsRepeatingFooterGroup();
}

static LayoutRect BoundingBoxInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // The special path for fragmented layers ensures that the bounding box also
  // covers contents visual overflow, so that the fragments will cover all
  // fragments of contents except for self-painting layers, because we initiate
  // fragment painting of contents from the layer.
  if (object.HasLayer() &&
      ToLayoutBoxModelObject(object)
          .Layer()
          ->ShouldFragmentCompositedBounds() &&
      // Table section may repeat, and doesn't need the special layer path
      // because it doesn't have contents visual overflow.
      !object.IsTableSection()) {
    return ToLayoutBoxModelObject(object).Layer()->PhysicalBoundingBox(
        &enclosing_pagination_layer);
  }

  // Non-boxes paint in the space of their containing block.
  if (!object.IsBox()) {
    const LayoutBox& containining_block = *object.ContainingBlock();
    LayoutRect bounds_rect;
    // For non-SVG we can get a more accurate result with LocalVisualRect,
    // instead of falling back to the bounds of the enclosing block.
    if (!object.IsSVG()) {
      bounds_rect = object.LocalVisualRect();
      containining_block.FlipForWritingMode(bounds_rect);
    } else {
      bounds_rect = LayoutRect(SVGLayoutSupport::LocalVisualRect(object));
    }

    return MapLocalRectToAncestorLayer(containining_block, bounds_rect,
                                       enclosing_pagination_layer);
  }

  // Compute the bounding box without transforms.
  // The object is guaranteed to be a box due to the logic above.
  const LayoutBox& box = ToLayoutBox(object);
  auto bounding_box = MapLocalRectToAncestorLayer(box, box.BorderBoxRect(),
                                                  enclosing_pagination_layer);

  if (!IsRepeatingTableSection(object))
    return bounding_box;

  const auto& section = ToLayoutTableSection(object);
  const auto& table = *section.Table();

  if (section.IsRepeatingHeaderGroup()) {
    // Now bounding_box covers the original header. Expand it to intersect
    // with all fragments containing the original and repeatings, i.e. to
    // intersect any fragment containing any row.
    if (const auto* bottom_section = table.BottomNonEmptySection()) {
      bounding_box.Unite(MapLocalRectToAncestorLayer(
          *bottom_section, bottom_section->BorderBoxRect(),
          enclosing_pagination_layer));
    }
    return bounding_box;
  }

  DCHECK(section.IsRepeatingFooterGroup());
  // Similar to repeating header, expand bounding_box to intersect any
  // fragment containing any row first.
  if (const auto* top_section = table.TopNonEmptySection()) {
    bounding_box.Unite(MapLocalRectToAncestorLayer(*top_section,
                                                   top_section->BorderBoxRect(),
                                                   enclosing_pagination_layer));
    // However, the first fragment intersecting the expanded bounding_box may
    // not have enough space to contain the repeating footer. Exclude the
    // total height of the first row and repeating footers from the top of
    // bounding_box to exclude the first fragment without enough space.
    auto top_exclusion = table.RowOffsetFromRepeatingFooter();
    if (top_section != section) {
      top_exclusion +=
          top_section->FirstRow()->LogicalHeight() + table.VBorderSpacing();
    }
    // Subtract 1 to ensure overlap of 1 px for a fragment that has exactly
    // one row plus space for the footer.
    if (top_exclusion)
      top_exclusion -= 1;
    bounding_box.ShiftYEdgeTo(bounding_box.Y() + top_exclusion);
  }
  return bounding_box;
}

static LayoutPoint PaintOffsetInPaginationContainer(
    const LayoutObject& object,
    const PaintLayer& enclosing_pagination_layer) {
  // Non-boxes use their containing blocks' paint offset.
  if (!object.IsBox() && !object.HasLayer()) {
    return PaintOffsetInPaginationContainer(*object.ContainingBlock(),
                                            enclosing_pagination_layer);
  }

  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint());
  object.MapLocalToAncestor(&enclosing_pagination_layer.GetLayoutObject(),
                            transform_state, kApplyContainerFlip);
  transform_state.Flatten();
  return LayoutPoint(transform_state.LastPlanarPoint());
}

void FragmentPaintPropertyTreeBuilder::UpdatePaintOffset() {
  // Paint offsets for fragmented content are computed from scratch.
  const auto* enclosing_pagination_layer =
      full_context_.painting_layer->EnclosingPaginationLayer();
  if (enclosing_pagination_layer &&
      // Except if the paint_offset_root is below the pagination container,
      // in which case fragmentation offsets are already baked into the paint
      // offset transform for paint_offset_root.
      !context_.current.paint_offset_root->PaintingLayer()
           ->EnclosingPaginationLayer()) {
    // Set fragment visual paint offset.
    LayoutPoint paint_offset =
        PaintOffsetInPaginationContainer(object_, *enclosing_pagination_layer);

    paint_offset.MoveBy(fragment_data_.PaginationOffset());
    paint_offset.Move(context_.repeating_paint_offset_adjustment);
    paint_offset.MoveBy(
        VisualOffsetFromPaintOffsetRoot(context_, enclosing_pagination_layer));

    // The paint offset root can have a subpixel paint offset adjustment.
    // The paint offset root always has one fragment.
    const auto& paint_offset_root_fragment =
        context_.current.paint_offset_root->FirstFragment();
    paint_offset.MoveBy(paint_offset_root_fragment.PaintOffset());

    context_.current.paint_offset = paint_offset;
    return;
  }

  if (object_.IsFloating())
    context_.current.paint_offset = context_.paint_offset_for_float;

  // Multicolumn spanners are painted starting at the multicolumn container (but
  // still inherit properties in layout-tree order) so reset the paint offset.
  if (object_.IsColumnSpanAll()) {
    context_.current.paint_offset =
        object_.Container()->FirstFragment().PaintOffset();
  }

  if (object_.IsBoxModelObject()) {
    const LayoutBoxModelObject& box_model_object =
        ToLayoutBoxModelObject(object_);
    switch (box_model_object.StyleRef().GetPosition()) {
      case EPosition::kStatic:
        break;
      case EPosition::kRelative:
        context_.current.paint_offset +=
            box_model_object.OffsetForInFlowPosition();
        break;
      case EPosition::kAbsolute: {
        DCHECK(full_context_.container_for_absolute_position ==
               box_model_object.Container());
        context_.current = context_.absolute_position;

        // Absolutely positioned content in an inline should be positioned
        // relative to the inline.
        const auto* container = full_context_.container_for_absolute_position;
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainAbsolutePositionObjects());
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              ToLayoutInline(container)->OffsetForInFlowPositionedInline(
                  ToLayoutBox(box_model_object));
        }
        break;
      }
      case EPosition::kSticky:
        break;
      case EPosition::kFixed: {
        DCHECK(full_context_.container_for_fixed_position ==
               box_model_object.Container());
        context_.current = context_.fixed_position;
        // Fixed-position elements that are fixed to the vieport have a
        // transform above the scroll of the LayoutView. Child content is
        // relative to that transform, and hence the fixed-position element.
        if (context_.fixed_position.fixed_position_children_fixed_to_root)
          context_.current.paint_offset_root = &box_model_object;

        const auto* container = full_context_.container_for_fixed_position;
        if (container && container->IsLayoutInline()) {
          DCHECK(container->CanContainFixedPositionObjects());
          DCHECK(box_model_object.IsBox());
          context_.current.paint_offset +=
              ToLayoutInline(container)->OffsetForInFlowPositionedInline(
                  ToLayoutBox(box_model_object));
        }
        break;
      }
      default:
        NOTREACHED();
    }
  }

  if (object_.IsBox()) {
    // TODO(pdr): Several calls in this function walk back up the tree to
    // calculate containers (e.g., physicalLocation, offsetForInFlowPosition*).
    // The containing block and other containers can be stored on
    // PaintPropertyTreeBuilderFragmentContext instead of recomputing them.
    context_.current.paint_offset.MoveBy(
        ToLayoutBox(object_).PhysicalLocation());

    // This is a weird quirk that table cells paint as children of table rows,
    // but their location have the row's location baked-in.
    // Similar adjustment is done in LayoutTableCell::offsetFromContainer().
    if (object_.IsTableCell()) {
      LayoutObject* parent_row = object_.Parent();
      DCHECK(parent_row && parent_row->IsTableRow());
      context_.current.paint_offset.MoveBy(
          -ToLayoutBox(parent_row)->PhysicalLocation());
    }
  }

  context_.current.paint_offset.Move(
      context_.repeating_paint_offset_adjustment);
}

void FragmentPaintPropertyTreeBuilder::SetNeedsPaintPropertyUpdateIfNeeded() {
  if (!object_.IsBox())
    return;

  const LayoutBox& box = ToLayoutBox(object_);

  if (NeedsOverflowClip(box)) {
    bool had_overflow_clip = properties_ && properties_->OverflowClip();
    if (had_overflow_clip == CanOmitOverflowClip(box))
      box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }

  if (box.IsLayoutReplaced() &&
      box.PreviousPhysicalContentBoxRect() != box.PhysicalContentBoxRect())
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();

  if (box.Size() == box.PreviousSize())
    return;

  // CSS mask and clip-path comes with an implicit clip to the border box.
  // Currently only SPv2 generate and take advantage of those.
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

  if (box.HasClipPath())
    box.GetMutableForPainting().InvalidateClipPathCache();

  // The filter generated for reflection depends on box size.
  if (box.HasReflection()) {
    DCHECK(box.HasLayer());
    box.Layer()->SetFilterOnEffectNodeDirty();
    box.GetMutableForPainting().SetNeedsPaintPropertyUpdate();
  }
}

void FragmentPaintPropertyTreeBuilder::UpdateForObjectLocationAndSize(
    base::Optional<IntPoint>& paint_offset_translation) {
  context_.old_paint_offset = fragment_data_.PaintOffset();
  UpdatePaintOffset();
  UpdateForPaintOffsetTranslation(paint_offset_translation);

  if (fragment_data_.PaintOffset() != context_.current.paint_offset) {
    // Many paint properties depend on paint offset so we force an update of
    // the entire subtree on paint offset changes.
    // However, they are blocked by isolation.
    full_context_.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;

    object_.GetMutableForPainting().SetShouldCheckForPaintInvalidation();
    fragment_data_.SetPaintOffset(context_.current.paint_offset);
    fragment_data_.InvalidateClipPathCache();

    object_.GetFrameView()->SetIntersectionObservationState(
        LocalFrameView::kDesired);
  }

  if (paint_offset_translation)
    context_.current.paint_offset_root = &ToLayoutBoxModelObject(object_);
}

void FragmentPaintPropertyTreeBuilder::UpdateClipPathCache() {
  if (fragment_data_.IsClipPathCacheValid())
    return;

  if (!object_.StyleRef().ClipPath())
    return;

  base::Optional<FloatRect> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(object_);
  if (!bounding_box) {
    fragment_data_.SetClipPathCache(base::nullopt, nullptr);
    return;
  }
  bounding_box->MoveBy(FloatPoint(fragment_data_.PaintOffset()));

  bool is_valid = false;
  base::Optional<Path> path = ClipPathClipper::PathBasedClip(
      object_, object_.IsSVGChild(),
      ClipPathClipper::LocalReferenceBox(object_), is_valid);
  DCHECK(is_valid);
  if (path)
    path->Translate(ToFloatSize(FloatPoint(fragment_data_.PaintOffset())));
  fragment_data_.SetClipPathCache(
      EnclosingIntRect(*bounding_box),
      path ? AdoptRef(new RefCountedPath(std::move(*path))) : nullptr);
}

void FragmentPaintPropertyTreeBuilder::UpdateForSelf() {
#if DCHECK_IS_ON()
  FindPaintOffsetNeedingUpdateScope check_paint_offset(
      object_, fragment_data_, full_context_.is_actually_needed);
#endif

  // This is not in FindObjectPropertiesNeedingUpdateScope because paint offset
  // can change without NeedsPaintPropertyUpdate.
  base::Optional<IntPoint> paint_offset_translation;
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
    UpdateClipPathClip(false);
    UpdateEffect();
    UpdateLinkHighlightEffect();
    UpdateClipPathClip(true);  // Special pass for SPv1 composited clip-path.
    UpdateCssClip();
    UpdateFilter();
    UpdateOverflowControlsClip();
  }
  UpdateLocalBorderBoxContext();
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

#if DCHECK_IS_ON()
  if (properties_)
    properties_->Validate();
#endif
}

}  // namespace

void PaintPropertyTreeBuilder::InitFragmentPaintProperties(
    FragmentData& fragment,
    bool needs_paint_properties,
    const LayoutPoint& pagination_offset,
    LayoutUnit logical_top_in_flow_thread) {
  if (needs_paint_properties) {
    fragment.EnsurePaintProperties();
  } else if (fragment.PaintProperties()) {
    // Tree topology changes are blocked by isolation.
    context_.force_subtree_update_reasons |=
        PaintPropertyTreeBuilderContext::kSubtreeUpdateIsolationBlocked;
    fragment.ClearPaintProperties();
  }
  fragment.SetPaginationOffset(pagination_offset);
  fragment.SetLogicalTopInFlowThread(logical_top_in_flow_thread);
}

void PaintPropertyTreeBuilder::InitSingleFragmentFromParent(
    bool needs_paint_properties) {
  FragmentData& first_fragment =
      object_.GetMutableForPainting().FirstFragment();
  first_fragment.ClearNextFragment();
  InitFragmentPaintProperties(first_fragment, needs_paint_properties);
  if (context_.fragments.IsEmpty()) {
    context_.fragments.push_back(PaintPropertyTreeBuilderFragmentContext());
  } else {
    context_.fragments.resize(1);
    context_.fragments[0].fragment_clip.reset();
    context_.fragments[0].logical_top_in_flow_thread = LayoutUnit();
  }

  // Column-span:all skips pagination container in the tree hierarchy, so it
  // should also skip any fragment clip created by the skipped pagination
  // container. We also need to skip fragment clip if the object is a paint
  // invalidation container which doesn't allow fragmentation.
  if (object_.IsColumnSpanAll() ||
      (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
       object_.IsPaintInvalidationContainer() &&
       ToLayoutBoxModelObject(object_).Layer()->EnclosingPaginationLayer())) {
    if (const auto* pagination_layer_in_tree_hierarchy =
            object_.Parent()->EnclosingLayer()->EnclosingPaginationLayer()) {
      const auto* properties =
          pagination_layer_in_tree_hierarchy->GetLayoutObject()
              .FirstFragment()
              .PaintProperties();
      if (properties && properties->FragmentClip()) {
        context_.fragments[0].current.clip =
            properties->FragmentClip()->Parent();
      }
    }
  }
}

void PaintPropertyTreeBuilder::UpdateCompositedLayerPaginationOffset() {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

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
  bool is_paint_invalidation_container = object_.IsPaintInvalidationContainer();
  const auto* parent_composited_layer =
      context_.painting_layer->EnclosingLayerWithCompositedLayerMapping(
          is_paint_invalidation_container ? kExcludeSelf : kIncludeSelf);
  if (is_paint_invalidation_container &&
      (!parent_composited_layer ||
       !parent_composited_layer->EnclosingPaginationLayer())) {
    // |object_| establishes the top level composited layer under the
    // pagination layer.
    FragmentainerIterator iterator(
        ToLayoutFlowThread(enclosing_pagination_layer->GetLayoutObject()),
        BoundingBoxInPaginationContainer(object_, *enclosing_pagination_layer));
    if (!iterator.AtEnd()) {
      first_fragment.SetPaginationOffset(
          ToLayoutPoint(iterator.PaginationOffset()));
      first_fragment.SetLogicalTopInFlowThread(
          iterator.FragmentainerLogicalTopInFlowThread());
    }
  } else if (parent_composited_layer) {
    // All objects under the composited layer use the same pagination offset.
    const auto& fragment =
        parent_composited_layer->GetLayoutObject().FirstFragment();
    first_fragment.SetPaginationOffset(fragment.PaginationOffset());
    first_fragment.SetLogicalTopInFlowThread(fragment.LogicalTopInFlowThread());
  }
}

void PaintPropertyTreeBuilder::
    UpdateRepeatingTableSectionPaintOffsetAdjustment() {
  if (!context_.repeating_table_section)
    return;

  if (object_ == context_.repeating_table_section) {
    if (ToLayoutTableSection(object_).IsRepeatingHeaderGroup())
      UpdateRepeatingTableHeaderPaintOffsetAdjustment();
    else if (ToLayoutTableSection(object_).IsRepeatingFooterGroup())
      UpdateRepeatingTableFooterPaintOffsetAdjustment();
  } else if (!context_.painting_layer->EnclosingPaginationLayer()) {
    // When repeating a table section in paged media, paint_offset is inherited
    // by descendants, so we only need to adjust point offset for the table
    // section.
    for (auto& fragment_context : context_.fragments) {
      fragment_context.repeating_paint_offset_adjustment = LayoutSize();
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
  const auto& section = ToLayoutTableSection(object_);
  DCHECK(section.IsRepeatingHeaderGroup());

  LayoutUnit fragment_height;
  LayoutUnit original_offset_in_flow_thread =
      context_.repeating_table_section_bounding_box.Y();
  LayoutUnit original_offset_in_fragment;
  const LayoutFlowThread* flow_thread = nullptr;
  if (const auto* pagination_layer =
          context_.painting_layer->EnclosingPaginationLayer()) {
    flow_thread = &ToLayoutFlowThread(pagination_layer->GetLayoutObject());
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

  // This is total height of repeating headers seen by the table - height of
  // this header (which is the lowest repeating header seen by this table.
  auto repeating_offset_in_fragment =
      section.Table()->RowOffsetFromRepeatingHeader() - section.LogicalHeight();

  // For a repeating table header, the original location (which may be in the
  // middle of the fragment) and repeated locations (which should be always,
  // together with repeating headers of outer tables, aligned to the top of
  // the fragments) may be different. Therefore, for fragments other than the
  // first, adjust by |alignment_offset|.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;
  for (wtf_size_t i = 0; i < context_.fragments.size(); ++i) {
    auto& fragment_context = context_.fragments[i];
    fragment_context.repeating_paint_offset_adjustment = LayoutSize();
    // Adjust paint offsets of repeatings (not including the original).
    if (i)
      fragment_context.repeating_paint_offset_adjustment.SetHeight(adjustment);

    // Calculate the adjustment for the repeating which will appear in the next
    // fragment.
    adjustment += fragment_height;
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
  const auto& section = ToLayoutTableSection(object_);
  DCHECK(section.IsRepeatingFooterGroup());

  LayoutUnit fragment_height;
  LayoutUnit original_offset_in_flow_thread =
      context_.repeating_table_section_bounding_box.MaxY() -
      section.LogicalHeight();
  LayoutUnit original_offset_in_fragment;
  const LayoutFlowThread* flow_thread = nullptr;
  if (const auto* pagination_layer =
          context_.painting_layer->EnclosingPaginationLayer()) {
    flow_thread = &ToLayoutFlowThread(pagination_layer->GetLayoutObject());
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

  const auto& table = *section.Table();
  // TODO(crbug.com/798153): This keeps the existing behavior of repeating
  // footer painting in TableSectionPainter. Should change both places when
  // tweaking border-spacing for repeating footers.
  auto repeating_offset_in_fragment = fragment_height -
                                      table.RowOffsetFromRepeatingFooter() -
                                      table.VBorderSpacing();
  // We should show the whole bottom border instead of half if the table
  // collapses borders.
  if (table.ShouldCollapseBorders())
    repeating_offset_in_fragment -= table.BorderBottom();

  // Similar to repeating header, this is to adjust the repeating footer from
  // its original location to the repeating location.
  auto adjustment = repeating_offset_in_fragment - original_offset_in_fragment;

  auto fragment_offset_in_flow_thread =
      original_offset_in_flow_thread - original_offset_in_fragment;
  for (auto i = context_.fragments.size(); i > 0; --i) {
    auto& fragment_context = context_.fragments[i - 1];
    fragment_context.repeating_paint_offset_adjustment = LayoutSize();
    // Adjust paint offsets of repeatings.
    if (i != context_.fragments.size())
      fragment_context.repeating_paint_offset_adjustment.SetHeight(adjustment);

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
      &ToLayoutFlowThread(parent_pagination_layer->GetLayoutObject());

  LayoutPoint location(LayoutUnit(), logical_top_in_current_flow_thread);
  // TODO(crbug.com/467477): Should we flip for writing-mode? For now regardless
  // of flipping, fast/multicol/vertical-rl/nested-columns.html fails.
  if (!flow_thread.IsHorizontalWritingMode())
    location = location.TransposedPoint();

  // Convert into parent_flow_thread's coordinates.
  location = LayoutPoint(flow_thread.LocalToAncestorPoint(FloatPoint(location),
                                                          parent_flow_thread));
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
    const base::Optional<LayoutRect>& fragment_clip,
    LayoutUnit logical_top_in_flow_thread) const {
  const auto& parent_fragments = context_.fragments;
  if (parent_fragments.IsEmpty())
    return PaintPropertyTreeBuilderFragmentContext();

  // This will be used in the loop finding matching fragment from ancestor flow
  // threads after no matching from parent_fragments.
  LayoutUnit logical_top_in_containing_flow_thread;

  if (object_.IsLayoutFlowThread()) {
    const auto& flow_thread = ToLayoutFlowThread(object_);
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
          context.fragment_clip = base::nullopt;
          return context;
        }
      }
    }

    logical_top_in_containing_flow_thread = logical_top_in_flow_thread;
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
  for (const auto* container = object_.Container(); container;
       container = container->Container()) {
    if (!container->FirstFragment().HasLocalBorderBoxProperties())
      continue;

    for (const auto* fragment = &container->FirstFragment(); fragment;
         fragment = fragment->NextFragment()) {
      if (fragment->LogicalTopInFlowThread() ==
          logical_top_in_containing_flow_thread) {
        // Found a matching fragment in an ancestor container. Use the
        // container's content clip as the clip state.
        DCHECK(fragment->PostOverflowClip());
        context.current.clip = fragment->PostOverflowClip();
        return context;
      }
    }

    if (container->IsLayoutFlowThread()) {
      logical_top_in_containing_flow_thread =
          FragmentLogicalTopInParentFlowThread(
              ToLayoutFlowThread(*container),
              logical_top_in_containing_flow_thread);
    }
  }

  // We should always find a matching ancestor fragment in the above loop
  // because logical_top_in_containing_flow_thread will be zero when we traverse
  // across the top-level flow thread and it should match the first fragment of
  // a non-fragmented ancestor container.
  NOTREACHED();
  return context;
}

void PaintPropertyTreeBuilder::CreateFragmentContextsInFlowThread(
    bool needs_paint_properties) {
  // We need at least the fragments for all fragmented objects, which store
  // their local border box properties and paint invalidation data (such
  // as paint offset and visual rect) on each fragment.
  PaintLayer* paint_layer = context_.painting_layer;
  PaintLayer* enclosing_pagination_layer =
      paint_layer->EnclosingPaginationLayer();

  const auto& flow_thread =
      ToLayoutFlowThread(enclosing_pagination_layer->GetLayoutObject());
  LayoutRect object_bounding_box_in_flow_thread;
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
      context_.repeating_table_section = &ToLayoutTableSection(object_);
      context_.repeating_table_section_bounding_box =
          object_bounding_box_in_flow_thread;
    }
  }

  FragmentData* current_fragment_data = nullptr;
  FragmentainerIterator iterator(flow_thread,
                                 object_bounding_box_in_flow_thread);
  bool fragments_changed = false;
  Vector<PaintPropertyTreeBuilderFragmentContext, 1> new_fragment_contexts;
  for (; !iterator.AtEnd(); iterator.Advance()) {
    auto pagination_offset = ToLayoutPoint(iterator.PaginationOffset());
    auto logical_top_in_flow_thread =
        iterator.FragmentainerLogicalTopInFlowThread();
    base::Optional<LayoutRect> fragment_clip;

    if (object_.HasLayer()) {
      // 1. Compute clip in flow thread space.
      fragment_clip = iterator.ClipRectInFlowThread();

      // We skip empty clip fragments, since they can share the same logical top
      // with the subsequent fragments. Since we skip drawing empty fragments
      // anyway, it doesn't affect the paint output, but it allows us to use
      // logical top to uniquely identify fragments in an object.
      if (fragment_clip->IsEmpty())
        continue;

      // 2. Convert #1 to visual coordinates in the space of the flow thread.
      fragment_clip->MoveBy(pagination_offset);
      // 3. Adjust #2 to visual coordinates in the containing "paint offset"
      // space.
      {
        DCHECK(context_.fragments[0].current.paint_offset_root);
        LayoutPoint pagination_visual_offset = VisualOffsetFromPaintOffsetRoot(
            context_.fragments[0], enclosing_pagination_layer);
        // Adjust for paint offset of the root, which may have a subpixel
        // component. The paint offset root never has more than one fragment.
        pagination_visual_offset.MoveBy(
            context_.fragments[0]
                .current.paint_offset_root->FirstFragment()
                .PaintOffset());
        fragment_clip->MoveBy(pagination_visual_offset);
      }
    }

    // Match to parent fragments from the same containing flow thread.
    new_fragment_contexts.push_back(
        ContextForFragment(fragment_clip, logical_top_in_flow_thread));

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
      const base::Optional<LayoutRect>& new_fragment_clip =
          new_fragment_contexts.back().fragment_clip;
      fragments_changed =
          !!old_fragment_clip != !!new_fragment_clip ||
          (old_fragment_clip && new_fragment_clip &&
           old_fragment_clip->ClipRect() != ToClipRect(*new_fragment_clip));
    }

    InitFragmentPaintProperties(
        *current_fragment_data,
        needs_paint_properties || new_fragment_contexts.back().fragment_clip,
        pagination_offset, logical_top_in_flow_thread);
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

  context_.fragments[0].fixed_position.paint_offset.Move(LayoutUnit(),
                                                         -view->ScrollTop());
  for (int page = 1; page < page_count; page++) {
    context_.fragments[page] = context_.fragments[page - 1];
    context_.fragments[page].fixed_position.paint_offset.Move(LayoutUnit(),
                                                              page_height);
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
  context_.repeating_table_section_bounding_box.Move(LayoutUnit(),
                                                     view->ScrollTop());

  auto page_height = view->PageLogicalHeight();
  const auto& bounding_box = context_.repeating_table_section_bounding_box;
  int first_page = floorf(bounding_box.Y() / page_height);
  int last_page = ceilf(bounding_box.MaxY() / page_height) - 1;
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
    InitFragmentPaintProperties(*fragment_data, needs_paint_properties,
                                LayoutPoint(),
                                fragment_context.logical_top_in_flow_thread);
  }
  DCHECK(fragment_data);
  fragment_data->ClearNextFragment();
}

bool PaintPropertyTreeBuilder::UpdateFragments() {
  bool had_paint_properties = object_.FirstFragment().PaintProperties();
  // Note: It is important to short-circuit on object_.StyleRef().ClipPath()
  // because NeedsClipPathClip() and NeedsEffect() requires the clip path
  // cache to be resolved, but the clip path cache invalidation must delayed
  // until the paint offset and border box has been computed.
  bool needs_paint_properties =
      object_.StyleRef().ClipPath() || NeedsPaintOffsetTranslation(object_) ||
      NeedsStickyTranslation(object_) || NeedsTransform(object_) ||
      NeedsClipPathClip(object_) || NeedsEffect(object_) ||
      NeedsLinkHighlightEffect(object_) ||
      NeedsTransformForNonRootSVG(object_) || NeedsFilter(object_) ||
      NeedsCssClip(object_) || NeedsInnerBorderRadiusClip(object_) ||
      NeedsOverflowClip(object_) || NeedsPerspective(object_) ||
      NeedsReplacedContentTransform(object_) ||
      NeedsScrollOrScrollTranslation(object_);
  // Need of fragmentation clip will be determined in CreateFragmentContexts().

  if (object_.IsFixedPositionObjectInPagedMedia()) {
    // This flag applies to the object itself and descendants.
    context_.is_repeating_fixed_position = true;
    CreateFragmentContextsForRepeatingFixedPosition();
  } else if (ObjectIsRepeatingTableSectionInPagedMedia()) {
    context_.repeating_table_section = &ToLayoutTableSection(object_);
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
    ToLayoutBoxModelObject(object_).Layer()->SetIsUnderSVGHiddenContainer(
        context_.has_svg_hidden_container_ancestor);
  }

  UpdateRepeatingTableSectionPaintOffsetAdjustment();

  return needs_paint_properties != had_paint_properties;
}

bool PaintPropertyTreeBuilder::ObjectTypeMightNeedPaintProperties() const {
  return object_.IsBoxModelObject() || object_.IsSVG() ||
         context_.painting_layer->EnclosingPaginationLayer() ||
         context_.repeating_table_section ||
         context_.is_repeating_fixed_position;
}

void PaintPropertyTreeBuilder::UpdatePaintingLayer() {
  bool changed_painting_layer = false;
  if (object_.HasLayer() &&
      ToLayoutBoxModelObject(object_).HasSelfPaintingLayer()) {
    context_.painting_layer = ToLayoutBoxModelObject(object_).Layer();
    changed_painting_layer = true;
  } else if (object_.IsColumnSpanAll() ||
             object_.IsFloatingWithNonContainingBlockParent()) {
    // See LayoutObject::paintingLayer() for the special-cases of floating under
    // inline and multicolumn.
    context_.painting_layer = object_.PaintingLayer();
    changed_painting_layer = true;
  }
  DCHECK(context_.painting_layer == object_.PaintingLayer());
}

bool PaintPropertyTreeBuilder::UpdateForSelf() {
  UpdatePaintingLayer();

  bool property_added_or_removed = false;
  if (ObjectTypeMightNeedPaintProperties())
    property_added_or_removed = UpdateFragments();
  else
    object_.GetMutableForPainting().FirstFragment().ClearNextFragment();

  bool property_changed = false;
  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder builder(object_, context_,
                                             fragment_context, *fragment_data);
    builder.UpdateForSelf();
    property_changed |= builder.PropertyChanged();
    property_added_or_removed |= builder.PropertyAddedOrRemoved();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);

  // We need to update property tree states of paint chunks.
  if (property_added_or_removed)
    context_.painting_layer->SetNeedsRepaint();

  if (!context_.supports_composited_raster_invalidation)
    return property_changed || property_added_or_removed;

  return property_changed;
}

bool PaintPropertyTreeBuilder::UpdateForChildren() {
  if (!ObjectTypeMightNeedPaintProperties())
    return false;

  bool property_changed = false;
  bool property_added_or_removed = false;
  auto* fragment_data = &object_.GetMutableForPainting().FirstFragment();
  // For now, only consider single fragment elements as possible isolation
  // boundaries.
  // TODO(crbug.com/890932): See if this is needed.
  bool is_isolated = context_.fragments.size() == 1u;
  for (auto& fragment_context : context_.fragments) {
    FragmentPaintPropertyTreeBuilder builder(object_, context_,
                                             fragment_context, *fragment_data);
    // The element establishes an isolation boundary if it has isolation nodes
    // before and after updating the children. In other words, if it didn't have
    // isolation nodes previously then we still want to do a subtree walk. If it
    // now doesn't have isolation nodes, then of course it is also not isolated.
    is_isolated &= builder.HasIsolationNodes();
    builder.UpdateForChildren();
    is_isolated &= builder.HasIsolationNodes();

    property_changed |= builder.PropertyChanged();
    property_added_or_removed |= builder.PropertyAddedOrRemoved();
    fragment_data = fragment_data->NextFragment();
  }
  DCHECK(!fragment_data);
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
  if (property_added_or_removed)
    context_.painting_layer->SetNeedsRepaint();

  return property_changed;
}

}  // namespace blink
