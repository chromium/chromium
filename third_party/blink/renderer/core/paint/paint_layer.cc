/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/paint/paint_layer.h"

#include <limits>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/hit_testing_transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/box_reflection_utils.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"

namespace blink {

namespace {

static CompositingQueryMode g_compositing_query_mode =
    kCompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases;

struct SameSizeAsPaintLayer : DisplayItemClient {
  // The bit fields may fit into the machine word of DisplayItemClient which
  // has only 8-bit data.
  unsigned bit_fields1 : 24;
  unsigned bit_fields2;
  void* pointers[11];
#if DCHECK_IS_ON()
  void* pointer;
#endif
  LayoutUnit layout_units[4];
  IntSize size;
  Persistent<PaintLayerScrollableArea> scrollable_area;
  LayoutRect previous_dirty_rect;
};

static_assert(sizeof(PaintLayer) == sizeof(SameSizeAsPaintLayer),
              "PaintLayer should stay small");

}  // namespace

using namespace HTMLNames;

PaintLayerRareData::PaintLayerRareData()
    : enclosing_pagination_layer(nullptr),
      potential_compositing_reasons_from_style(CompositingReason::kNone),
      potential_compositing_reasons_from_non_style(CompositingReason::kNone),
      compositing_reasons(CompositingReason::kNone),
      squashing_disallowed_reasons(SquashingDisallowedReason::kNone),
      grouped_mapping(nullptr) {}

PaintLayerRareData::~PaintLayerRareData() = default;

PaintLayer::PaintLayer(LayoutBoxModelObject& layout_object)
    : is_root_layer_(layout_object.IsLayoutView()),
      has_visible_content_(false),
      needs_descendant_dependent_flags_update_(true),
      has_visible_descendant_(false),
#if DCHECK_IS_ON()
      // The root layer (LayoutView) does not need position update at start
      // because its Location() is always 0.
      needs_position_update_(!IsRootLayer()),
#endif
      has3d_transformed_descendant_(false),
      contains_dirty_overlay_scrollbars_(false),
      needs_ancestor_dependent_compositing_inputs_update_(true),
      child_needs_compositing_inputs_update_(true),
      has_compositing_descendant_(false),
      should_isolate_composited_descendants_(false),
      lost_grouped_mapping_(false),
      needs_repaint_(false),
      previous_paint_result_(kFullyPainted),
      needs_paint_phase_descendant_outlines_(false),
      previous_paint_phase_descendant_outlines_was_empty_(false),
      needs_paint_phase_float_(false),
      previous_paint_phase_float_was_empty_(false),
      needs_paint_phase_descendant_block_backgrounds_(false),
      previous_paint_phase_descendant_block_backgrounds_was_empty_(false),
      has_descendant_with_clip_path_(false),
      has_non_isolated_descendant_with_blend_mode_(false),
      has_fixed_position_descendant_(false),
      has_sticky_position_descendant_(false),
      has_non_contained_absolute_position_descendant_(false),
      self_painting_status_changed_(false),
      filter_on_effect_node_dirty_(false),
      is_under_svg_hidden_container_(false),
      descendant_has_direct_or_scrolling_compositing_reason_(false),
      needs_compositing_reasons_update_(true),
      descendant_may_need_compositing_requirements_update_(false),
      needs_compositing_layer_assignment_(false),
      descendant_needs_compositing_layer_assignment_(false),
      has_self_painting_layer_descendant_(false),
      is_non_stacked_with_in_flow_stacked_descendant_(false),
      layout_object_(layout_object),
      parent_(nullptr),
      previous_(nullptr),
      next_(nullptr),
      first_(nullptr),
      last_(nullptr),
      static_inline_position_(0),
      static_block_position_(0),
      ancestor_overflow_layer_(nullptr)
#if DCHECK_IS_ON()
      ,
      stacking_parent_(nullptr)
#endif
{
  is_self_painting_layer_ = ShouldBeSelfPaintingLayer();

  UpdateScrollableArea();
}

PaintLayer::~PaintLayer() {
  if (rare_data_ && rare_data_->resource_info) {
    const ComputedStyle& style = GetLayoutObject().StyleRef();
    if (style.HasFilter())
      style.Filter().RemoveClient(*rare_data_->resource_info);
    if (auto* reference_clip =
            ToReferenceClipPathOperationOrNull(style.ClipPath()))
      reference_clip->RemoveClient(*rare_data_->resource_info);
    rare_data_->resource_info->ClearLayer();
  }
  if (GetLayoutObject().GetFrame()) {
    if (ScrollingCoordinator* scrolling_coordinator = GetScrollingCoordinator())
      scrolling_coordinator->WillDestroyLayer(this);
  }

  if (GroupedMapping()) {
    DisableCompositingQueryAsserts disabler;
    SetGroupedMapping(nullptr, kInvalidateLayerAndRemoveFromMapping);
  }

  // Child layers will be deleted by their corresponding layout objects, so
  // we don't need to delete them ourselves.

  ClearCompositedLayerMapping(true);

  if (scrollable_area_)
    scrollable_area_->Dispose();

#if DCHECK_IS_ON()
  // stacking_parent_ should be cleared because DirtyStackingContextZOrderLists
  // should have been called.
  if (!GetLayoutObject().DocumentBeingDestroyed())
    DCHECK(!stacking_parent_);
#endif
}

String PaintLayer::DebugName() const {
  return GetLayoutObject().DebugName();
}

LayoutRect PaintLayer::VisualRect() const {
  return layout_object_.FragmentsVisualRectBoundingBox();
}

PaintLayerCompositor* PaintLayer::Compositor() const {
  if (!GetLayoutObject().View())
    return nullptr;
  return GetLayoutObject().View()->Compositor();
}

void PaintLayer::ContentChanged(ContentChangeType change_type) {
  // updateLayerCompositingState will query compositingReasons for accelerated
  // overflow scrolling.  This is tripped by
  // LayoutTests/compositing/content-changed-chicken-egg.html
  DisableCompositingQueryAsserts disabler;

  if (Compositor()) {
    if (change_type == kCanvasChanged)
      SetNeedsCompositingInputsUpdate();

    if (change_type == kCanvasContextChanged) {
      SetNeedsCompositingInputsUpdate();

      // Although we're missing test coverage, we need to call
      // GraphicsLayer::SetContentsToCcLayer with the new cc::Layer for this
      // canvas. See http://crbug.com/349195
      if (HasCompositedLayerMapping()) {
        GetCompositedLayerMapping()->SetNeedsGraphicsLayerUpdate(
            kGraphicsLayerUpdateSubtree);
      }
    }
  }

  if (CompositedLayerMapping* composited_layer_mapping =
          GetCompositedLayerMapping())
    composited_layer_mapping->ContentChanged(change_type);
}

bool PaintLayer::PaintsWithFilters() const {
  if (!GetLayoutObject().HasFilterInducingProperty())
    return false;

  // https://code.google.com/p/chromium/issues/detail?id=343759
  DisableCompositingQueryAsserts disabler;
  return !GetCompositedLayerMapping() ||
         GetCompositingState() != kPaintsIntoOwnBacking;
}


LayoutSize PaintLayer::SubpixelAccumulation() const {
  return rare_data_ ? rare_data_->subpixel_accumulation : LayoutSize();
}

void PaintLayer::SetSubpixelAccumulation(const LayoutSize& size) {
  if (rare_data_ || !size.IsZero()) {
    EnsureRareData().subpixel_accumulation = size;
    if (PaintLayerScrollableArea* scrollable_area = GetScrollableArea()) {
      scrollable_area->PositionOverflowControls();
    }
  }
}

void PaintLayer::UpdateLayerPositionsAfterLayout() {
  TRACE_EVENT0("blink,benchmark",
               "PaintLayer::updateLayerPositionsAfterLayout");
  RUNTIME_CALL_TIMER_SCOPE(
      V8PerIsolateData::MainThreadIsolate(),
      RuntimeCallStats::CounterId::kUpdateLayerPositionsAfterLayout);

  ClearClipRects();
  UpdateLayerPositionRecursive();

  {
    // FIXME: Remove incremental compositing updates after fixing the
    // chicken/egg issues, https://crbug.com/343756
    DisableCompositingQueryAsserts disabler;
    UpdatePaginationRecursive(EnclosingPaginationLayer());
  }
}

void PaintLayer::UpdateLayerPositionRecursive(
    UpdateLayerPositionBehavior behavior,
    bool dirty_compositing_if_needed) {
  LayoutPoint old_location = location_;
  switch (behavior) {
    case AllLayers:
      UpdateLayerPosition();
      break;
    case OnlyStickyLayers:
      if (GetLayoutObject().StyleRef().HasStickyConstrainedPosition())
        UpdateLayerPosition();
      if (PaintLayerScrollableArea* scroller = GetScrollableArea()) {
        if (!scroller->HasStickyDescendants())
          return;
      }
      break;
    default:
      NOTREACHED();
  }

  if (dirty_compositing_if_needed && location_ != old_location)
    SetNeedsCompositingInputsUpdate();

  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->UpdateLayerPositionRecursive(behavior, dirty_compositing_if_needed);
}

bool PaintLayer::SticksToScroller() const {
  if (!GetLayoutObject().StyleRef().HasStickyConstrainedPosition())
    return false;
  return AncestorOverflowLayer()->GetScrollableArea();
}

bool PaintLayer::FixedToViewport() const {
  if (GetLayoutObject().StyleRef().GetPosition() != EPosition::kFixed)
    return false;

  // TODO(pdr): This approach of calculating the nearest scroll node is O(n).
  // An option for improving this is to cache the nearest scroll node in
  // the local border box properties.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    const auto view_border_box_properties =
        GetLayoutObject().View()->FirstFragment().LocalBorderBoxProperties();
    const auto* view_scroll = view_border_box_properties.Transform()
                                  ->NearestScrollTranslationNode()
                                  .ScrollNode();

    const auto* scroll = GetLayoutObject()
                             .FirstFragment()
                             .LocalBorderBoxProperties()
                             .Transform()
                             ->NearestScrollTranslationNode()
                             .ScrollNode();
    return scroll == view_scroll;
  }

  return GetLayoutObject().Container() == GetLayoutObject().View();
}

bool PaintLayer::ScrollsWithRespectTo(const PaintLayer* other) const {
  if (FixedToViewport() != other->FixedToViewport())
    return true;
  // If either element sticks we cannot trivially determine that the layers do
  // not scroll with respect to each other.
  if (SticksToScroller() || other->SticksToScroller())
    return true;
  return AncestorScrollingLayer() != other->AncestorScrollingLayer();
}

bool PaintLayer::IsAffectedByScrollOf(const PaintLayer* ancestor) const {
  if (this == ancestor)
    return false;

  const PaintLayer* current_layer = this;
  while (current_layer && current_layer != ancestor) {
    bool ancestor_escaped = false;
    const PaintLayer* container =
        current_layer->ContainingLayer(ancestor, &ancestor_escaped);
    if (ancestor_escaped)
      return false;
    // Workaround the bug that LayoutView is mistakenly considered
    // a fixed-pos container.
    if (current_layer->GetLayoutObject().IsFixedPositioned() &&
        container->IsRootLayer())
      return false;
    current_layer = container;
  }
  return current_layer == ancestor;
}

void PaintLayer::UpdateLayerPositionsAfterOverflowScroll() {
  if (IsRootLayer()) {
    // The root PaintLayer (i.e. the LayoutView) is special, in that scroll
    // offset is not included in clip rects. Therefore, we do not need to clear
    // them when that PaintLayer is scrolled. We also don't need to update layer
    // positions, because they also do not depend on the root's scroll offset.
    if (GetScrollableArea()->HasStickyDescendants()) {
      UpdateLayerPositionRecursive(OnlyStickyLayers,
                                   /* dirty_compositing */ false);
    }
    return;
  }
  ClearClipRects();
  UpdateLayerPositionRecursive(AllLayers, /* dirty_compositing */ false);
}

void PaintLayer::UpdateTransformationMatrix() {
  if (TransformationMatrix* transform = Transform()) {
    LayoutBox* box = GetLayoutBox();
    DCHECK(box);
    transform->MakeIdentity();
    box->StyleRef().ApplyTransform(
        *transform, box->Size(), ComputedStyle::kIncludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
    MakeMatrixRenderable(
        *transform,
        Compositor() ? Compositor()->HasAcceleratedCompositing() : false);
  }
}

void PaintLayer::UpdateTransform(const ComputedStyle* old_style,
                                 const ComputedStyle& new_style) {
  // It's possible for the old and new style transform data to be equivalent
  // while hasTransform() differs, as it checks a number of conditions aside
  // from just the matrix, including but not limited to animation state.
  if (old_style && old_style->HasTransform() == new_style.HasTransform() &&
      new_style.TransformDataEquivalent(*old_style)) {
    return;
  }

  // LayoutObject::HasTransformRelatedProperty is also true when there is
  // transform-style: preserve-3d or perspective set, so check style too.
  bool has_transform = GetLayoutObject().HasTransformRelatedProperty() &&
                       new_style.HasTransform();
  bool had3d_transform = Has3DTransform();

  bool had_transform = Transform();
  if (has_transform != had_transform) {
    if (has_transform)
      EnsureRareData().transform = TransformationMatrix::Create();
    else
      rare_data_->transform.reset();

    // PaintLayers with transforms act as clip rects roots, so clear the cached
    // clip rects here.
    ClearClipRects();
  } else if (has_transform) {
    ClearClipRects(kAbsoluteClipRectsIgnoringViewportClip);
  }

  UpdateTransformationMatrix();

  if (had3d_transform != Has3DTransform()) {
    SetNeedsCompositingInputsUpdateInternal();
    MarkAncestorChainForDescendantDependentFlagsUpdate();
  }

  if (LocalFrameView* frame_view = GetLayoutObject().GetDocument().View())
    frame_view->SetNeedsUpdateGeometries();
}

static PaintLayer* EnclosingLayerForContainingBlock(PaintLayer* layer) {
  if (LayoutObject* containing_block =
          layer->GetLayoutObject().ContainingBlock())
    return containing_block->EnclosingLayer();
  return nullptr;
}

static const PaintLayer* EnclosingLayerForContainingBlock(
    const PaintLayer* layer) {
  if (const LayoutObject* containing_block =
          layer->GetLayoutObject().ContainingBlock())
    return containing_block->EnclosingLayer();
  return nullptr;
}

PaintLayer* PaintLayer::RenderingContextRoot() {
  PaintLayer* rendering_context = nullptr;

  if (ShouldPreserve3D())
    rendering_context = this;

  for (PaintLayer* current = EnclosingLayerForContainingBlock(this);
       current && current->ShouldPreserve3D();
       current = EnclosingLayerForContainingBlock(current))
    rendering_context = current;

  return rendering_context;
}

const PaintLayer* PaintLayer::RenderingContextRoot() const {
  const PaintLayer* rendering_context = nullptr;

  if (ShouldPreserve3D())
    rendering_context = this;

  for (const PaintLayer* current = EnclosingLayerForContainingBlock(this);
       current && current->ShouldPreserve3D();
       current = EnclosingLayerForContainingBlock(current))
    rendering_context = current;

  return rendering_context;
}

TransformationMatrix PaintLayer::CurrentTransform() const {
  if (TransformationMatrix* transform = Transform())
    return *transform;
  return TransformationMatrix();
}

TransformationMatrix PaintLayer::RenderableTransform(
    GlobalPaintFlags global_paint_flags) const {
  TransformationMatrix* transform = Transform();
  if (!transform)
    return TransformationMatrix();

  if (global_paint_flags & kGlobalPaintFlattenCompositingLayers) {
    TransformationMatrix matrix = *transform;
    MakeMatrixRenderable(matrix, false /* flatten 3d */);
    return matrix;
  }

  return *transform;
}

void PaintLayer::ConvertFromFlowThreadToVisualBoundingBoxInAncestor(
    const PaintLayer* ancestor_layer,
    LayoutRect& rect) const {
  PaintLayer* pagination_layer = EnclosingPaginationLayer();
  DCHECK(pagination_layer);
  LayoutFlowThread& flow_thread =
      ToLayoutFlowThread(pagination_layer->GetLayoutObject());

  // First make the flow thread rectangle relative to the flow thread, not to
  // |layer|.
  LayoutPoint offset_within_pagination_layer;
  ConvertToLayerCoords(pagination_layer, offset_within_pagination_layer);
  rect.MoveBy(offset_within_pagination_layer);

  // Then make the rectangle visual, relative to the fragmentation context.
  // Split our box up into the actual fragment boxes that layout in the
  // columns/pages and unite those together to get our true bounding box.
  rect = flow_thread.FragmentsBoundingBox(rect);

  // Finally, make the visual rectangle relative to |ancestorLayer|.
  if (ancestor_layer->EnclosingPaginationLayer() != pagination_layer) {
    rect.MoveBy(pagination_layer->VisualOffsetFromAncestor(ancestor_layer));
    return;
  }
  // The ancestor layer is inside the same pagination layer as |layer|, so we
  // need to subtract the visual distance from the ancestor layer to the
  // pagination layer.
  rect.MoveBy(-ancestor_layer->VisualOffsetFromAncestor(pagination_layer));
}

void PaintLayer::UpdatePaginationRecursive(bool needs_pagination_update) {
  if (rare_data_)
    rare_data_->enclosing_pagination_layer = nullptr;

  if (GetLayoutObject().IsLayoutFlowThread())
    needs_pagination_update = true;

  if (needs_pagination_update) {
    // Each paginated layer has to paint on its own. There is no recurring into
    // child layers. Each layer has to be checked individually and genuinely
    // know if it is going to have to split itself up when painting only its
    // contents (and not any other descendant layers). We track an
    // enclosingPaginationLayer instead of using a simple bit, since we want to
    // be able to get back to that layer easily.
    if (LayoutFlowThread* containing_flow_thread =
            GetLayoutObject().FlowThreadContainingBlock())
      EnsureRareData().enclosing_pagination_layer =
          containing_flow_thread->Layer();
  }

  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->UpdatePaginationRecursive(needs_pagination_update);
}

void PaintLayer::ClearPaginationRecursive() {
  if (rare_data_)
    rare_data_->enclosing_pagination_layer = nullptr;
  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->ClearPaginationRecursive();
}

const PaintLayer& PaintLayer::TransformAncestorOrRoot() const {
  return TransformAncestor() ? *TransformAncestor()
                             : *GetLayoutObject().View()->Layer();
}

void PaintLayer::MapPointInPaintInvalidationContainerToBacking(
    const LayoutBoxModelObject& paint_invalidation_container,
    FloatPoint& point) {
  PaintLayer* paint_invalidation_layer = paint_invalidation_container.Layer();
  if (!paint_invalidation_layer->GroupedMapping())
    return;

  LayoutBoxModelObject& transformed_ancestor =
      paint_invalidation_layer->TransformAncestorOrRoot().GetLayoutObject();

  // |paintInvalidationContainer| may have a local 2D transform on it, so take
  // that into account when mapping into the space of the transformed ancestor.
  point = paint_invalidation_container.LocalToAncestorPoint(
      point, &transformed_ancestor);
  // Don't include composited scroll offsets, since
  // SquashingOffsetFromTransformedAncestor does not.
  if (transformed_ancestor.UsesCompositedScrolling())
    point.Move(ToLayoutBox(transformed_ancestor).ScrolledContentOffset());

  point.MoveBy(-paint_invalidation_layer->GroupedMapping()
                    ->SquashingOffsetFromTransformedAncestor());
}

void PaintLayer::MapRectInPaintInvalidationContainerToBacking(
    const LayoutBoxModelObject& paint_invalidation_container,
    LayoutRect& rect) {
  PaintLayer* paint_invalidation_layer = paint_invalidation_container.Layer();
  if (!paint_invalidation_layer->GroupedMapping())
    return;

  LayoutBoxModelObject& transformed_ancestor =
      paint_invalidation_layer->TransformAncestorOrRoot().GetLayoutObject();

  // |paintInvalidationContainer| may have a local 2D transform on it, so take
  // that into account when mapping into the space of the transformed ancestor.
  rect = LayoutRect(
      paint_invalidation_container
          .LocalToAncestorQuad(FloatRect(rect), &transformed_ancestor)
          .BoundingBox());
  // Don't include composited scroll offsets, since
  // SquashingOffsetFromTransformedAncestor does not.
  if (transformed_ancestor.UsesCompositedScrolling())
    rect.Move(ToLayoutBox(transformed_ancestor).ScrolledContentOffset());

  rect.MoveBy(-paint_invalidation_layer->GroupedMapping()
                   ->SquashingOffsetFromTransformedAncestor());
}

void PaintLayer::MapRectToPaintInvalidationBacking(
    const LayoutObject& layout_object,
    const LayoutBoxModelObject& paint_invalidation_container,
    LayoutRect& rect) {
  if (!paint_invalidation_container.Layer()->GroupedMapping()) {
    layout_object.MapToVisualRectInAncestorSpace(&paint_invalidation_container,
                                                 rect);
    return;
  }

  // This code adjusts the visual rect to be in the space of the transformed
  // ancestor of the grouped (i.e. squashed) layer. This is because all layers
  // that squash together need to issue paint invalidations w.r.t. a single
  // container that is an ancestor of all of them, in order to properly take
  // into account any local transforms etc.
  // FIXME: remove this special-case code that works around the paint
  // invalidation code structure.
  layout_object.MapToVisualRectInAncestorSpace(&paint_invalidation_container,
                                               rect);

  MapRectInPaintInvalidationContainerToBacking(paint_invalidation_container,
                                               rect);
}

void PaintLayer::DirtyVisibleContentStatus() {
  MarkAncestorChainForDescendantDependentFlagsUpdate();
  // Non-self-painting layers paint into their ancestor layer, and count as part
  // of the "visible contents" of the parent, so we need to dirty it.
  if (!IsSelfPaintingLayer())
    Parent()->DirtyVisibleContentStatus();
}

void PaintLayer::MarkAncestorChainForDescendantDependentFlagsUpdate() {
  for (PaintLayer* layer = this; layer; layer = layer->Parent()) {
    if (layer->needs_descendant_dependent_flags_update_)
      break;
    layer->needs_descendant_dependent_flags_update_ = true;
    layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();
  }
}

void PaintLayer::UpdateDescendantDependentFlags() {
  if (needs_descendant_dependent_flags_update_) {
    bool old_has_non_isolated_descendant_with_blend_mode =
        has_non_isolated_descendant_with_blend_mode_;
    has_visible_descendant_ = false;
    has_non_isolated_descendant_with_blend_mode_ = false;
    has_descendant_with_clip_path_ = false;
    has_fixed_position_descendant_ = false;
    has_sticky_position_descendant_ = false;
    has_non_contained_absolute_position_descendant_ = false;
    has_self_painting_layer_descendant_ = false;
    is_non_stacked_with_in_flow_stacked_descendant_ = false;

    bool can_contain_abs =
        GetLayoutObject().CanContainAbsolutePositionObjects();

    const ComputedStyle& style = GetLayoutObject().StyleRef();
    bool needs_stacking_node = style.IsStackingContext();
    bool is_stacked = style.IsStacked();

    for (PaintLayer* child = FirstChild(); child;
         child = child->NextSibling()) {
      const ComputedStyle& child_style = child->GetLayoutObject().StyleRef();

      child->UpdateDescendantDependentFlags();

      if (child->has_visible_content_ || child->has_visible_descendant_)
        has_visible_descendant_ = true;

      has_non_isolated_descendant_with_blend_mode_ |=
          (!child->GetLayoutObject().StyleRef().IsStackingContext() &&
           child->HasNonIsolatedDescendantWithBlendMode()) ||
          child->GetLayoutObject().StyleRef().HasBlendMode();

      has_descendant_with_clip_path_ |= child->HasDescendantWithClipPath() ||
                                        child->GetLayoutObject().HasClipPath();

      has_fixed_position_descendant_ |=
          child->HasFixedPositionDescendant() ||
          child_style.GetPosition() == EPosition::kFixed;
      has_sticky_position_descendant_ |=
          child->HasStickyPositionDescendant() ||
          child_style.GetPosition() == EPosition::kSticky;

      if (!can_contain_abs) {
        has_non_contained_absolute_position_descendant_ |=
            (child->HasNonContainedAbsolutePositionDescendant() ||
             child_style.GetPosition() == EPosition::kAbsolute);
      }

      needs_stacking_node = needs_stacking_node || !child_style.IsStacked();

      has_self_painting_layer_descendant_ =
          has_self_painting_layer_descendant_ ||
          child->HasSelfPaintingLayerDescendant() ||
          child->IsSelfPaintingLayer();

      if (!is_stacked) {
        if (child->IsNonStackedWithInFlowStackedDescendant())
          is_non_stacked_with_in_flow_stacked_descendant_ = true;
        else if (child_style.IsStacked() &&
                 !child->GetLayoutObject().IsOutOfFlowPositioned())
          is_non_stacked_with_in_flow_stacked_descendant_ = true;
      }
    }

    UpdateStackingNode(needs_stacking_node);

    if (old_has_non_isolated_descendant_with_blend_mode !=
        static_cast<bool>(has_non_isolated_descendant_with_blend_mode_))
      GetLayoutObject().SetNeedsPaintPropertyUpdate();
    needs_descendant_dependent_flags_update_ = false;
  }

  bool previously_has_visible_content = has_visible_content_;
  if (GetLayoutObject().StyleRef().Visibility() == EVisibility::kVisible) {
    has_visible_content_ = true;
  } else {
    // layer may be hidden but still have some visible content, check for this
    has_visible_content_ = false;
    LayoutObject* r = GetLayoutObject().SlowFirstChild();
    while (r) {
      if (r->StyleRef().Visibility() == EVisibility::kVisible &&
          (!r->HasLayer() || !r->EnclosingLayer()->IsSelfPaintingLayer())) {
        has_visible_content_ = true;
        break;
      }
      LayoutObject* layout_object_first_child = r->SlowFirstChild();
      if (layout_object_first_child &&
          (!r->HasLayer() || !r->EnclosingLayer()->IsSelfPaintingLayer())) {
        r = layout_object_first_child;
      } else if (r->NextSibling()) {
        r = r->NextSibling();
      } else {
        do {
          r = r->Parent();
          if (r == &GetLayoutObject())
            r = nullptr;
        } while (r && !r->NextSibling());
        if (r)
          r = r->NextSibling();
      }
    }
  }

  if (HasVisibleContent() != previously_has_visible_content) {
    SetNeedsCompositingInputsUpdateInternal();
    // We need to tell layout_object_ to recheck its rect because we
    // pretend that invisible LayoutObjects have 0x0 rects. Changing
    // visibility therefore changes our rect and we need to visit
    // this LayoutObject during the PrePaintTreeWalk.
    layout_object_.SetShouldCheckForPaintInvalidation();
  }

  Update3DTransformedDescendantStatus();
}

void PaintLayer::Update3DTransformedDescendantStatus() {
  has3d_transformed_descendant_ = false;

  if (!stacking_node_)
    return;

  // Transformed or preserve-3d descendants can only be in the z-order lists,
  // not in the normal flow list, so we only need to check those.
  PaintLayerStackingNodeIterator iterator(
      *stacking_node_.get(), kPositiveZOrderChildren | kNegativeZOrderChildren);
  while (PaintLayer* child_layer = iterator.Next()) {
    bool child_has3d = false;
    // If the child lives in a 3d hierarchy, then the layer at the root of
    // that hierarchy needs the m_has3DTransformedDescendant set.
    if (child_layer->Preserves3D() &&
        (child_layer->Has3DTransform() ||
         child_layer->Has3DTransformedDescendant()))
      child_has3d = true;
    else if (child_layer->Has3DTransform())
      child_has3d = true;

    if (child_has3d) {
      has3d_transformed_descendant_ = true;
      break;
    }
  }
}

void PaintLayer::UpdateLayerPosition() {
  // LayoutBoxes will call UpdateSizeAndScrollingAfterLayout() from
  // LayoutBox::UpdateAfterLayout, but LayoutInlines will still need to update
  // their size.
  if (GetLayoutObject().IsInline() && GetLayoutObject().IsLayoutInline())
    UpdateSizeAndScrollingAfterLayout();
  LayoutPoint local_point;
  if (LayoutBox* box = GetLayoutBox()) {
    local_point.MoveBy(box->PhysicalLocation());
  }

  if (!GetLayoutObject().IsOutOfFlowPositioned() &&
      !GetLayoutObject().IsColumnSpanAll()) {
    // We must adjust our position by walking up the layout tree looking for the
    // nearest enclosing object with a layer.
    LayoutObject* curr = GetLayoutObject().Container();
    while (curr && !curr->HasLayer()) {
      if (curr->IsBox() && !curr->IsTableRow()) {
        // Rows and cells share the same coordinate space (that of the section).
        // Omit them when computing our xpos/ypos.
        local_point.MoveBy(ToLayoutBox(curr)->PhysicalLocation());
      }
      curr = curr->Container();
    }
    if (curr && curr->IsTableRow()) {
      // Put ourselves into the row coordinate space.
      local_point.MoveBy(-ToLayoutBox(curr)->PhysicalLocation());
    }
  }

  if (PaintLayer* containing_layer = ContainingLayer()) {
    if (containing_layer->GetLayoutObject().HasOverflowClip() &&
        !containing_layer->IsRootLayer()) {
      // Subtract our container's scroll offset.
      IntSize offset =
          containing_layer->GetLayoutBox()->ScrolledContentOffset();
      local_point -= offset;
    } else {
      auto& container = containing_layer->GetLayoutObject();
      if (GetLayoutObject().IsOutOfFlowPositioned() &&
          container.IsLayoutInline() &&
          container.CanContainOutOfFlowPositionedElement(
              GetLayoutObject().StyleRef().GetPosition())) {
        // Adjust offset for absolute under in-flow positioned inline.
        LayoutSize offset =
            ToLayoutInline(container).OffsetForInFlowPositionedInline(
                ToLayoutBox(GetLayoutObject()));
        local_point += offset;
      }
    }
  }

  if (GetLayoutObject().IsInFlowPositioned()) {
    LayoutSize new_offset = GetLayoutObject().OffsetForInFlowPosition();
    if (rare_data_ || !new_offset.IsZero())
      EnsureRareData().offset_for_in_flow_position = new_offset;
    local_point.Move(new_offset);
  } else if (rare_data_) {
    rare_data_->offset_for_in_flow_position = LayoutSize();
  }

  location_ = local_point;

#if DCHECK_IS_ON()
  needs_position_update_ = false;
#endif
}

bool PaintLayer::UpdateSize() {
  LayoutSize old_size = size_;
  if (IsRootLayer()) {
    size_ = LayoutSize(GetLayoutObject().GetDocument().View()->Size());
  } else if (GetLayoutObject().IsInline() &&
             GetLayoutObject().IsLayoutInline()) {
    LayoutInline& inline_flow = ToLayoutInline(GetLayoutObject());
    IntRect line_box = EnclosingIntRect(inline_flow.LinesBoundingBox());
    size_ = LayoutSize(line_box.Size());
  } else if (LayoutBox* box = GetLayoutBox()) {
    size_ = box->Size();
  }
  if (old_size != size_)
    SetNeedsCompositingInputsUpdate();

  return old_size != size_;
}

void PaintLayer::UpdateSizeAndScrollingAfterLayout() {
  bool did_resize = UpdateSize();
  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterLayout();
    if (did_resize)
      scrollable_area_->VisibleSizeChanged();
  }
}

TransformationMatrix PaintLayer::PerspectiveTransform() const {
  if (!GetLayoutObject().HasTransformRelatedProperty())
    return TransformationMatrix();

  const ComputedStyle& style = GetLayoutObject().StyleRef();
  if (!style.HasPerspective())
    return TransformationMatrix();

  TransformationMatrix t;
  t.ApplyPerspective(style.Perspective());
  return t;
}

FloatPoint PaintLayer::PerspectiveOrigin() const {
  if (!GetLayoutObject().HasTransformRelatedProperty())
    return FloatPoint();

  const LayoutRect border_box = ToLayoutBox(GetLayoutObject()).BorderBoxRect();
  const ComputedStyle& style = GetLayoutObject().StyleRef();

  return FloatPointForLengthPoint(style.PerspectiveOrigin(),
                                  FloatSize(border_box.Size()));
}

PaintLayer* PaintLayer::ContainingLayer(const PaintLayer* ancestor,
                                        bool* skipped_ancestor) const {
  // If we have specified an ancestor, surely the caller needs to know whether
  // we skipped it.
  DCHECK(!ancestor || skipped_ancestor);
  if (skipped_ancestor)
    *skipped_ancestor = false;

  LayoutObject& layout_object = GetLayoutObject();
  if (layout_object.IsOutOfFlowPositioned()) {
    auto can_contain_this_layer =
        layout_object.IsFixedPositioned()
            ? &LayoutObject::CanContainFixedPositionObjects
            : &LayoutObject::CanContainAbsolutePositionObjects;

    PaintLayer* curr = Parent();
    while (curr && !((&curr->GetLayoutObject())->*can_contain_this_layer)()) {
      if (skipped_ancestor && curr == ancestor)
        *skipped_ancestor = true;
      curr = curr->Parent();
    }
    return curr;
  }

  // If the parent layer is not a block, there might be floating objects
  // between this layer (included) and parent layer which need to escape the
  // inline parent to find the actual containing layer through the containing
  // block chain.
  // Column span need to find the containing layer through its containing block.
  if ((!Parent() || Parent()->GetLayoutObject().IsLayoutBlock()) &&
      !layout_object.IsColumnSpanAll())
    return Parent();

  // This is a universal approach to find containing layer, but is slower than
  // the earlier code.
  base::Optional<LayoutObject::AncestorSkipInfo> skip_info;
  if (skipped_ancestor)
    skip_info.emplace(&ancestor->GetLayoutObject());
  auto* object = &layout_object;
  while (auto* container =
             object->Container(skipped_ancestor ? &*skip_info : nullptr)) {
    if (skipped_ancestor && skip_info->AncestorSkipped())
      *skipped_ancestor = true;
    if (container->HasLayer())
      return ToLayoutBoxModelObject(container)->Layer();
    object = container;
  }
  return nullptr;
}

LayoutPoint PaintLayer::ComputeOffsetFromAncestor(
    const PaintLayer& ancestor_layer) const {
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 FloatPoint());
  const LayoutBoxModelObject& ancestor_object =
      ancestor_layer.GetLayoutObject();
  GetLayoutObject().MapLocalToAncestor(&ancestor_object, transform_state, 0);
  if (ancestor_object.UsesCompositedScrolling())
    transform_state.Move(ToLayoutBox(ancestor_object).ScrolledContentOffset());
  transform_state.Flatten();
  return LayoutPoint(transform_state.LastPlanarPoint());
}

PaintLayer* PaintLayer::CompositingContainer() const {
  if (IsReplacedNormalFlowStacking())
    return Parent();
  if (!GetLayoutObject().StyleRef().IsStacked())
    return IsSelfPaintingLayer() ? Parent() : ContainingLayer();
  if (PaintLayerStackingNode* ancestor_stacking_node =
          PaintLayerStackingNode::AncestorStackingContextNode(this))
    return ancestor_stacking_node->Layer();
  return nullptr;
}

bool PaintLayer::IsPaintInvalidationContainer() const {
  return GetCompositingState() == kPaintsIntoOwnBacking ||
         GetCompositingState() == kPaintsIntoGroupedBacking;
}

// Note: enclosingCompositingLayer does not include squashed layers. Compositing
// stacking children of squashed layers receive graphics layers that are
// parented to the compositing ancestor of the squashed layer.
PaintLayer* PaintLayer::EnclosingLayerWithCompositedLayerMapping(
    IncludeSelfOrNot include_self) const {
  DCHECK(IsAllowedToQueryCompositingState());

  if ((include_self == kIncludeSelf) &&
      GetCompositingState() != kNotComposited &&
      GetCompositingState() != kPaintsIntoGroupedBacking)
    return const_cast<PaintLayer*>(this);

  for (PaintLayer* curr = CompositingContainer(); curr;
       curr = curr->CompositingContainer()) {
    if (curr->GetCompositingState() != kNotComposited &&
        curr->GetCompositingState() != kPaintsIntoGroupedBacking)
      return curr;
  }

  return nullptr;
}

// Return the enclosingCompositedLayerForPaintInvalidation for the given Layer
// including crossing frame boundaries.
PaintLayer*
PaintLayer::EnclosingLayerForPaintInvalidationCrossingFrameBoundaries() const {
  const PaintLayer* layer = this;
  PaintLayer* composited_layer = nullptr;
  while (!composited_layer) {
    composited_layer = layer->EnclosingLayerForPaintInvalidation();
    if (!composited_layer) {
      CHECK(layer->GetLayoutObject().GetFrame());
      auto* owner = layer->GetLayoutObject().GetFrame()->OwnerLayoutObject();
      if (!owner)
        break;
      layer = owner->EnclosingLayer();
    }
  }
  return composited_layer;
}

PaintLayer* PaintLayer::EnclosingLayerForPaintInvalidation() const {
  DCHECK(IsAllowedToQueryCompositingState());

  if (IsPaintInvalidationContainer())
    return const_cast<PaintLayer*>(this);

  for (PaintLayer* curr = CompositingContainer(); curr;
       curr = curr->CompositingContainer()) {
    if (curr->IsPaintInvalidationContainer())
      return curr;
  }

  return nullptr;
}

void PaintLayer::SetNeedsCompositingInputsUpdate() {
  SetNeedsCompositingInputsUpdateInternal();

  // TODO(chrishtr): These are a bit of a heavy hammer, because not all
  // things which require compositing inputs update require a descendant-
  // dependent flags udpate. Reduce call sites after SPv2 launch allows
  /// removal of CompositingInputsUpdater.
  MarkAncestorChainForDescendantDependentFlagsUpdate();
}

void PaintLayer::SetNeedsCompositingInputsUpdateInternal() {
  needs_ancestor_dependent_compositing_inputs_update_ = true;

  for (PaintLayer* current = this;
       current && !current->child_needs_compositing_inputs_update_;
       current = current->Parent())
    current->child_needs_compositing_inputs_update_ = true;

  if (Compositor()) {
    Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateAfterCompositingInputChange);
  }
}

void PaintLayer::UpdateAncestorDependentCompositingInputs(
    const AncestorDependentCompositingInputs& compositing_inputs) {
  EnsureAncestorDependentCompositingInputs() = compositing_inputs;
  needs_ancestor_dependent_compositing_inputs_update_ = false;
}

void PaintLayer::ClearChildNeedsCompositingInputsUpdate() {
  DCHECK(!NeedsCompositingInputsUpdate());
  child_needs_compositing_inputs_update_ = false;
}

bool PaintLayer::HasNonIsolatedDescendantWithBlendMode() const {
  DCHECK(!needs_descendant_dependent_flags_update_);
  if (has_non_isolated_descendant_with_blend_mode_)
    return true;
  if (GetLayoutObject().IsSVGRoot())
    return ToLayoutSVGRoot(GetLayoutObject())
        .HasNonIsolatedBlendingDescendants();
  return false;
}

void PaintLayer::SetCompositingReasons(CompositingReasons reasons,
                                       CompositingReasons mask) {
  CompositingReasons old_reasons =
      rare_data_ ? rare_data_->compositing_reasons : CompositingReason::kNone;
  if ((old_reasons & mask) == (reasons & mask))
    return;
  CompositingReasons new_reasons = (reasons & mask) | (old_reasons & ~mask);
  if (rare_data_ || new_reasons != CompositingReason::kNone)
    EnsureRareData().compositing_reasons = new_reasons;
}

void PaintLayer::SetSquashingDisallowedReasons(
    SquashingDisallowedReasons reasons) {
  SquashingDisallowedReasons old_reasons =
      rare_data_ ? rare_data_->squashing_disallowed_reasons
                 : SquashingDisallowedReason::kNone;
  if (old_reasons == reasons)
    return;
  if (rare_data_ || reasons != SquashingDisallowedReason::kNone)
    EnsureRareData().squashing_disallowed_reasons = reasons;
}

void PaintLayer::SetHasCompositingDescendant(bool has_compositing_descendant) {
  if (has_compositing_descendant_ ==
      static_cast<unsigned>(has_compositing_descendant))
    return;

  has_compositing_descendant_ = has_compositing_descendant;

  if (HasCompositedLayerMapping())
    GetCompositedLayerMapping()->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateLocal);
}

void PaintLayer::SetShouldIsolateCompositedDescendants(
    bool should_isolate_composited_descendants) {
  if (should_isolate_composited_descendants_ ==
      static_cast<unsigned>(should_isolate_composited_descendants))
    return;

  should_isolate_composited_descendants_ =
      should_isolate_composited_descendants;

  if (HasCompositedLayerMapping())
    GetCompositedLayerMapping()->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateLocal);
}

bool PaintLayer::HasAncestorWithFilterThatMovesPixels() const {
  for (const PaintLayer* curr = this; curr; curr = curr->Parent()) {
    if (curr->HasFilterThatMovesPixels())
      return true;
  }
  return false;
}

static void ExpandClipRectForDescendants(
    LayoutRect& clip_rect,
    const PaintLayer* layer,
    const PaintLayer* root_layer,
    PaintLayer::TransparencyClipBoxBehavior transparency_behavior,
    const LayoutSize& sub_pixel_accumulation,
    GlobalPaintFlags global_paint_flags) {
  // If we have a mask, then the clip is limited to the border box area (and
  // there is no need to examine child layers).
  if (!layer->GetLayoutObject().HasMask()) {
    // Note: we don't have to walk z-order lists since transparent elements
    // always establish a stacking container. This means we can just walk the
    // layer tree directly.
    for (PaintLayer* curr = layer->FirstChild(); curr;
         curr = curr->NextSibling())
      clip_rect.Unite(PaintLayer::TransparencyClipBox(
          curr, root_layer, transparency_behavior,
          PaintLayer::kDescendantsOfTransparencyClipBox, sub_pixel_accumulation,
          global_paint_flags));
  }
}

LayoutRect PaintLayer::TransparencyClipBox(
    const PaintLayer* layer,
    const PaintLayer* root_layer,
    TransparencyClipBoxBehavior transparency_behavior,
    TransparencyClipBoxMode transparency_mode,
    const LayoutSize& sub_pixel_accumulation,
    GlobalPaintFlags global_paint_flags) {
  // FIXME: Although this function completely ignores CSS-imposed clipping, we
  // did already intersect with the paintDirtyRect, and that should cut down on
  // the amount we have to paint.  Still it would be better to respect clips.

  if (root_layer != layer &&
      ((transparency_behavior == kPaintingTransparencyClipBox &&
        layer->PaintsWithTransform(global_paint_flags)) ||
       (transparency_behavior == kHitTestingTransparencyClipBox &&
        layer->HasTransformRelatedProperty()))) {
    // The best we can do here is to use enclosed bounding boxes to establish a
    // "fuzzy" enough clip to encompass the transformed layer and all of its
    // children.
    const PaintLayer* pagination_layer =
        transparency_mode == kDescendantsOfTransparencyClipBox
            ? layer->EnclosingPaginationLayer()
            : nullptr;
    const PaintLayer* root_layer_for_transform =
        pagination_layer ? pagination_layer : root_layer;
    LayoutPoint delta;
    layer->ConvertToLayerCoords(root_layer_for_transform, delta);

    delta.Move(sub_pixel_accumulation);
    IntPoint pixel_snapped_delta = RoundedIntPoint(delta);
    TransformationMatrix transform;
    transform.Translate(pixel_snapped_delta.X(), pixel_snapped_delta.Y());
    if (layer->Transform())
      transform = transform * *layer->Transform();

    // We don't use fragment boxes when collecting a transformed layer's
    // bounding box, since it always paints unfragmented.
    LayoutRect clip_rect = layer->PhysicalBoundingBox(LayoutPoint());
    ExpandClipRectForDescendants(clip_rect, layer, layer, transparency_behavior,
                                 sub_pixel_accumulation, global_paint_flags);
    LayoutRect result = EnclosingLayoutRect(
        transform.MapRect(layer->MapRectForFilter(FloatRect(clip_rect))));
    if (!pagination_layer)
      return result;

    // We have to break up the transformed extent across our columns.
    // Split our box up into the actual fragment boxes that layout in the
    // columns/pages and unite those together to get our true bounding box.
    LayoutFlowThread& enclosing_flow_thread =
        ToLayoutFlowThread(pagination_layer->GetLayoutObject());
    result = enclosing_flow_thread.FragmentsBoundingBox(result);

    LayoutPoint root_layer_delta;
    pagination_layer->ConvertToLayerCoords(root_layer, root_layer_delta);
    result.MoveBy(root_layer_delta);
    return result;
  }

  LayoutRect clip_rect = layer->ShouldFragmentCompositedBounds(root_layer)
                             ? layer->FragmentsBoundingBox(root_layer)
                             : layer->PhysicalBoundingBox(root_layer);
  ExpandClipRectForDescendants(clip_rect, layer, root_layer,
                               transparency_behavior, sub_pixel_accumulation,
                               global_paint_flags);

  // Convert clipRect into local coordinates for mapLayerRectForFilter(), and
  // convert back after.
  LayoutPoint delta;
  layer->ConvertToLayerCoords(root_layer, delta);
  clip_rect.MoveBy(-delta);
  clip_rect = layer->MapLayoutRectForFilter(clip_rect);
  clip_rect.MoveBy(delta);

  clip_rect.Move(sub_pixel_accumulation);
  return clip_rect;
}

LayoutRect PaintLayer::PaintingExtent(const PaintLayer* root_layer,
                                      const LayoutSize& sub_pixel_accumulation,
                                      GlobalPaintFlags global_paint_flags) {
  return TransparencyClipBox(this, root_layer, kPaintingTransparencyClipBox,
                             kRootOfTransparencyClipBox, sub_pixel_accumulation,
                             global_paint_flags);
}

void* PaintLayer::operator new(size_t sz) {
  return WTF::Partitions::LayoutPartition()->Alloc(
      sz, WTF_HEAP_PROFILER_TYPE_NAME(PaintLayer));
}

void PaintLayer::operator delete(void* ptr) {
  base::PartitionFree(ptr);
}

void PaintLayer::AddChild(PaintLayer* child, PaintLayer* before_child) {
  PaintLayer* prev_sibling =
      before_child ? before_child->PreviousSibling() : LastChild();
  if (prev_sibling) {
    child->SetPreviousSibling(prev_sibling);
    prev_sibling->SetNextSibling(child);
    DCHECK(prev_sibling != child);
  } else {
    SetFirstChild(child);
  }

  if (before_child) {
    before_child->SetPreviousSibling(child);
    child->SetNextSibling(before_child);
    DCHECK(before_child != child);
  } else {
    SetLastChild(child);
  }

  child->parent_ = this;

  // The ancestor overflow layer is calculated during compositing inputs update
  // and should not be set yet.
  CHECK(!child->AncestorOverflowLayer());

  SetNeedsCompositingInputsUpdate();

  const ComputedStyle& child_style = child->GetLayoutObject().StyleRef();

  if (Compositor()) {
    if (!child_style.IsStacked() && !GetLayoutObject().DocumentBeingDestroyed())
      Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  }

  if (child_style.IsStacked() || child->FirstChild()) {
    // Dirty the z-order list in which we are contained. The
    // ancestorStackingContextNode() can be null in the case where we're
    // building up generated content layers. This is ok, since the lists will
    // start off dirty in that case anyway.
    PaintLayerStackingNode::DirtyStackingContextZOrderLists(child);
    MarkAncestorChainForDescendantDependentFlagsUpdate();
  }

  // Non-self-painting children paint into this layer, so the visible contents
  // status of this layer is affected.
  if (!child->IsSelfPaintingLayer())
    DirtyVisibleContentStatus();

  MarkAncestorChainForDescendantDependentFlagsUpdate();

  // Need to force requirements update, due to change of stacking order.
  SetNeedsCompositingRequirementsUpdate();

  child->SetNeedsRepaint();
}

PaintLayer* PaintLayer::RemoveChild(PaintLayer* old_child) {
  old_child->MarkCompositingContainerChainForNeedsRepaint();

  if (old_child->PreviousSibling())
    old_child->PreviousSibling()->SetNextSibling(old_child->NextSibling());
  if (old_child->NextSibling())
    old_child->NextSibling()->SetPreviousSibling(old_child->PreviousSibling());

  if (first_ == old_child)
    first_ = old_child->NextSibling();
  if (last_ == old_child)
    last_ = old_child->PreviousSibling();

  const ComputedStyle& old_child_style =
      old_child->GetLayoutObject().StyleRef();

  if (!GetLayoutObject().DocumentBeingDestroyed()) {
    if (Compositor()) {
      if (!old_child_style.IsStacked())
        Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    }
    // Dirty the z-order list in which we are contained.
    PaintLayerStackingNode::DirtyStackingContextZOrderLists(old_child);
    SetNeedsCompositingInputsUpdate();
  }

  if (GetLayoutObject().StyleRef().Visibility() != EVisibility::kVisible)
    DirtyVisibleContentStatus();

  old_child->SetPreviousSibling(nullptr);
  old_child->SetNextSibling(nullptr);
  old_child->parent_ = nullptr;

  // Remove any ancestor overflow layers which descended into the removed child.
  if (old_child->AncestorOverflowLayer())
    old_child->RemoveAncestorOverflowLayer(old_child->AncestorOverflowLayer());

  if (old_child->has_visible_content_ || old_child->has_visible_descendant_)
    MarkAncestorChainForDescendantDependentFlagsUpdate();

  if (old_child->EnclosingPaginationLayer())
    old_child->ClearPaginationRecursive();

  return old_child;
}

void PaintLayer::ClearClipRects(ClipRectsCacheSlot cache_slot) {
  Clipper(PaintLayer::kDoNotUseGeometryMapper)
      .ClearClipRectsIncludingDescendants(cache_slot);
}

void PaintLayer::RemoveOnlyThisLayerAfterStyleChange(
    const ComputedStyle* old_style) {
  if (!parent_)
    return;

  if (old_style && old_style->IsStacked()) {
    PaintLayerStackingNode::DirtyStackingContextZOrderLists(this);
    MarkAncestorChainForDescendantDependentFlagsUpdate();
  }

  bool did_set_paint_invalidation = false;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // Destructing PaintLayer would cause CompositedLayerMapping and composited
    // layers to be destructed and detach from layer tree immediately. Layers
    // could have dangling scroll/clip parent if compositing update were
    // omitted.
    if (LocalFrameView* frame_view = layout_object_.GetDocument().View())
      frame_view->SetNeedsForcedCompositingUpdate();

    // We need the current compositing status.
    DisableCompositingQueryAsserts disabler;
    if (IsPaintInvalidationContainer()) {
      // Our children will be reparented and contained by a new paint
      // invalidation container, so need paint invalidation. CompositingUpdate
      // can't see this layer (which has been removed) so won't do this for us.
      ObjectPaintInvalidator(GetLayoutObject())
          .InvalidatePaintIncludingNonCompositingDescendants();
      GetLayoutObject().SetSubtreeShouldDoFullPaintInvalidation();
      did_set_paint_invalidation = true;
    }
  }

  if (!did_set_paint_invalidation && IsSelfPaintingLayer()) {
    if (PaintLayer* enclosing_self_painting_layer =
            parent_->EnclosingSelfPaintingLayer())
      enclosing_self_painting_layer->MergeNeedsPaintPhaseFlagsFrom(*this);
  }

  ClearClipRects();

  PaintLayer* next_sib = NextSibling();

  // Now walk our kids and reattach them to our parent.
  PaintLayer* current = first_;
  while (current) {
    PaintLayer* next = current->NextSibling();
    RemoveChild(current);
    parent_->AddChild(current, next_sib);

    // FIXME: We should call a specialized version of this function.
    current->UpdateLayerPositionsAfterLayout();
    current = next;
  }

  // Remove us from the parent.
  parent_->RemoveChild(this);
  layout_object_.DestroyLayer();
}

void PaintLayer::InsertOnlyThisLayerAfterStyleChange() {
  if (!parent_ && GetLayoutObject().Parent()) {
    // We need to connect ourselves when our layoutObject() has a parent.
    // Find our enclosingLayer and add ourselves.
    PaintLayer* parent_layer = GetLayoutObject().Parent()->EnclosingLayer();
    DCHECK(parent_layer);
    PaintLayer* before_child = GetLayoutObject().Parent()->FindNextLayer(
        parent_layer, &GetLayoutObject());
    parent_layer->AddChild(this, before_child);
  }

  // Remove all descendant layers from the hierarchy and add them to the new
  // position.
  for (LayoutObject* curr = GetLayoutObject().SlowFirstChild(); curr;
       curr = curr->NextSibling())
    curr->MoveLayers(parent_, this);

  // If the previous paint invalidation container is not a stacking context and
  // this object is stacked content, creating this layer may cause this object
  // and its descendants to change paint invalidation container.
  bool did_set_paint_invalidation = false;
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      !GetLayoutObject().IsLayoutView() && GetLayoutObject().IsRooted() &&
      GetLayoutObject().StyleRef().IsStacked()) {
    const LayoutBoxModelObject& previous_paint_invalidation_container =
        GetLayoutObject().Parent()->ContainerForPaintInvalidation();
    if (!previous_paint_invalidation_container.StyleRef().IsStackingContext()) {
      ObjectPaintInvalidator(GetLayoutObject())
          .InvalidatePaintIncludingNonSelfPaintingLayerDescendants();
      // Set needsRepaint along the original compositingContainer chain.
      GetLayoutObject().Parent()->EnclosingLayer()->SetNeedsRepaint();
      did_set_paint_invalidation = true;
    }
  }

  if (!did_set_paint_invalidation && IsSelfPaintingLayer() && parent_) {
    if (PaintLayer* enclosing_self_painting_layer =
            parent_->EnclosingSelfPaintingLayer())
      MergeNeedsPaintPhaseFlagsFrom(*enclosing_self_painting_layer);
  }

  // Clear out all the clip rects.
  ClearClipRects();
}

// Returns the layer reached on the walk up towards the ancestor.
static inline const PaintLayer* AccumulateOffsetTowardsAncestor(
    const PaintLayer* layer,
    const PaintLayer* ancestor_layer,
    LayoutPoint& location) {
  DCHECK(ancestor_layer != layer);

  const LayoutBoxModelObject& layout_object = layer->GetLayoutObject();

  if (layout_object.IsFixedPositioned() &&
      (!ancestor_layer || ancestor_layer == layout_object.View()->Layer())) {
    // If the fixed layer's container is the root, just add in the offset of the
    // view. We can obtain this by calling localToAbsolute() on the LayoutView.
    FloatPoint abs_pos = layout_object.LocalToAbsolute();
    location += LayoutSize(abs_pos.X(), abs_pos.Y());
    return ancestor_layer;
  }

  bool found_ancestor_first;
  PaintLayer* containing_layer =
      layer->ContainingLayer(ancestor_layer, &found_ancestor_first);

  if (found_ancestor_first) {
    // Found ancestorLayer before the containing layer, so compute offset of
    // both relative to the container and subtract.
    LayoutPoint this_coords;
    layer->ConvertToLayerCoords(containing_layer, this_coords);

    LayoutPoint ancestor_coords;
    ancestor_layer->ConvertToLayerCoords(containing_layer, ancestor_coords);

    location += (this_coords - ancestor_coords);
    return ancestor_layer;
  }

  if (!containing_layer)
    return nullptr;

  location += layer->Location();
  return containing_layer;
}

void PaintLayer::ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                                      LayoutPoint& location) const {
  if (ancestor_layer == this)
    return;

  const PaintLayer* curr_layer = this;
  while (curr_layer && curr_layer != ancestor_layer)
    curr_layer =
        AccumulateOffsetTowardsAncestor(curr_layer, ancestor_layer, location);
}

void PaintLayer::ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                                      LayoutRect& rect) const {
  LayoutPoint delta;
  ConvertToLayerCoords(ancestor_layer, delta);
  rect.MoveBy(delta);
}

LayoutPoint PaintLayer::VisualOffsetFromAncestor(
    const PaintLayer* ancestor_layer,
    LayoutPoint offset) const {
  if (ancestor_layer == this)
    return offset;
  PaintLayer* pagination_layer = EnclosingPaginationLayer();
  if (pagination_layer == this)
    pagination_layer = Parent()->EnclosingPaginationLayer();
  if (!pagination_layer) {
    ConvertToLayerCoords(ancestor_layer, offset);
    return offset;
  }

  LayoutFlowThread& flow_thread =
      ToLayoutFlowThread(pagination_layer->GetLayoutObject());
  ConvertToLayerCoords(pagination_layer, offset);
  offset = flow_thread.FlowThreadPointToVisualPoint(offset);
  if (ancestor_layer == pagination_layer)
    return offset;

  if (ancestor_layer->EnclosingPaginationLayer() != pagination_layer) {
    offset.MoveBy(pagination_layer->VisualOffsetFromAncestor(ancestor_layer));
  } else {
    // The ancestor layer is also inside the pagination layer, so we need to
    // subtract the visual distance from the ancestor layer to the pagination
    // layer.
    offset.MoveBy(-ancestor_layer->VisualOffsetFromAncestor(pagination_layer));
  }
  return offset;
}

void PaintLayer::DidUpdateScrollsOverflow() {
  UpdateSelfPaintingLayer();
}

void PaintLayer::UpdateStackingNode(bool needs_stacking_node) {
  if (needs_stacking_node != !!stacking_node_) {
    if (needs_stacking_node)
      stacking_node_ = std::make_unique<PaintLayerStackingNode>(this);
    else
      stacking_node_ = nullptr;
  }

  if (stacking_node_)
    stacking_node_->UpdateZOrderLists();
}

bool PaintLayer::RequiresScrollableArea() const {
  if (!GetLayoutBox())
    return false;
  if (GetLayoutObject().HasOverflowClip())
    return true;
  // Iframes with the resize property can be resized. This requires
  // scroll corner painting, which is implemented, in part, by
  // PaintLayerScrollableArea.
  if (GetLayoutBox()->CanResize())
    return true;
  return false;
}

void PaintLayer::UpdateScrollableArea() {
  if (RequiresScrollableArea() && !scrollable_area_) {
    scrollable_area_ = PaintLayerScrollableArea::Create(*this);
    Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  } else if (!RequiresScrollableArea() && scrollable_area_) {
    scrollable_area_->Dispose();
    scrollable_area_.Clear();
    Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  }
}

bool PaintLayer::HasOverflowControls() const {
  return scrollable_area_ &&
         (scrollable_area_->HasScrollbar() ||
          scrollable_area_->ScrollCorner() ||
          GetLayoutObject().StyleRef().Resize() != EResize::kNone);
}

void PaintLayer::AppendSingleFragmentIgnoringPagination(
    PaintLayerFragments& fragments,
    const PaintLayer* root_layer,
    const LayoutRect* dirty_rect,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const LayoutPoint* offset_from_root,
    const LayoutSize& sub_pixel_accumulation) const {
  PaintLayerFragment fragment;
  ClipRectsContext clip_rects_context(
      root_layer, &root_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, overlay_scrollbar_clip_behavior,
      respect_overflow_clip, sub_pixel_accumulation);
  Clipper(kUseGeometryMapper)
      .CalculateRects(clip_rects_context, &GetLayoutObject().FirstFragment(),
                      dirty_rect, fragment.layer_bounds,
                      fragment.background_rect, fragment.foreground_rect,
                      offset_from_root);
  fragment.root_fragment_data = &root_layer->GetLayoutObject().FirstFragment();
  fragment.fragment_data = &GetLayoutObject().FirstFragment();
  fragments.push_back(fragment);
}

bool PaintLayer::ShouldFragmentCompositedBounds(
    const PaintLayer* compositing_layer) const {
  if (!EnclosingPaginationLayer())
    return false;
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return true;
  if (PaintsWithTransform(kGlobalPaintNormalPhase))
    return true;
  if (!compositing_layer) {
    compositing_layer =
        EnclosingLayerForPaintInvalidationCrossingFrameBoundaries();
  }
  if (!compositing_layer)
    return true;
  // Composited layers may not be fragmented.
  return !compositing_layer->EnclosingPaginationLayer();
}

void PaintLayer::CollectFragments(
    PaintLayerFragments& fragments,
    const PaintLayer* root_layer,
    const LayoutRect* dirty_rect,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const LayoutPoint* offset_from_root,
    const LayoutSize& sub_pixel_accumulation) const {
  PaintLayerFragment fragment;
  const auto& first_fragment_data = GetLayoutObject().FirstFragment();
  const auto& first_root_fragment_data =
      root_layer->GetLayoutObject().FirstFragment();

  // If both |this| and |root_layer| are fragmented and are inside the same
  // pagination container, then try to match fragments from |root_layer| to
  // |this|, so that any fragment clip for |root_layer|'s fragment matches
  // |this|'s. Note we check both ShouldFragmentCompositedBounds() and next
  // fragment here because the former may return false even if |this| is
  // fragmented, e.g. for fixed-position objects in paged media, and the next
  // fragment can be null even if the first fragment is actually in a fragmented
  // context when the current layer appears in only one of the multiple
  // fragments of the pagination container.
  bool is_fragmented =
      ShouldFragmentCompositedBounds() || first_fragment_data.NextFragment();
  bool should_match_fragments =
      is_fragmented &&
      root_layer->EnclosingPaginationLayer() == EnclosingPaginationLayer();

  // The inherited offset_from_root does not include any pagination offsets.
  // In the presence of fragmentation, we cannot use it.
  bool offset_from_root_can_be_used = offset_from_root && !is_fragmented;
  for (auto* fragment_data = &first_fragment_data; fragment_data;
       fragment_data = fragment_data->NextFragment()) {
    const FragmentData* root_fragment_data;
    if (root_layer == this) {
      root_fragment_data = fragment_data;
    } else if (should_match_fragments) {
      for (root_fragment_data = &first_root_fragment_data; root_fragment_data;
           root_fragment_data = root_fragment_data->NextFragment()) {
        if (root_fragment_data->LogicalTopInFlowThread() ==
            fragment_data->LogicalTopInFlowThread())
          break;
      }
    } else {
      root_fragment_data = &first_root_fragment_data;
    }

    bool cant_find_fragment = !root_fragment_data;
    if (cant_find_fragment) {
      DCHECK(should_match_fragments);
      // Fall back to the first fragment, in order to have
      // PaintLayerClipper at least compute |fragment.layer_bounds|.
      root_fragment_data = &first_root_fragment_data;
    }

    ClipRectsContext clip_rects_context(
        root_layer, root_fragment_data, kUncachedClipRects,
        overlay_scrollbar_clip_behavior, respect_overflow_clip,
        sub_pixel_accumulation);

    base::Optional<LayoutRect> fragment_dirty_rect;
    if (dirty_rect) {
      // |dirty_rect| is in the coordinate space of |root_layer| (i.e. the
      // space of |root_layer|'s first fragment). Map the rect to the space of
      // the current root fragment.
      fragment_dirty_rect = *dirty_rect;
      first_root_fragment_data.MapRectToFragment(*root_fragment_data,
                                                 *fragment_dirty_rect);
    }

    Clipper(kUseGeometryMapper)
        .CalculateRects(
            clip_rects_context, fragment_data,
            fragment_dirty_rect ? &*fragment_dirty_rect : nullptr,
            fragment.layer_bounds, fragment.background_rect,
            fragment.foreground_rect,
            offset_from_root_can_be_used ? offset_from_root : nullptr);

    if (cant_find_fragment) {
      // If we couldn't find a matching fragment when |should_match_fragments|
      // was true, then fall back to no clip.
      fragment.background_rect.Reset();
      fragment.foreground_rect.Reset();
    }

    fragment.root_fragment_data = root_fragment_data;
    fragment.fragment_data = fragment_data;

    fragments.push_back(fragment);
  }
}

PaintLayer::HitTestRecursionData::HitTestRecursionData(
    const LayoutRect& rect_arg,
    const HitTestLocation& location_arg,
    const HitTestLocation& original_location_arg)
    : rect(rect_arg),
      location(location_arg),
      original_location(original_location_arg),
      intersects_location(location_arg.Intersects(rect_arg)) {}

bool PaintLayer::HitTest(const HitTestLocation& hit_test_location,
                         HitTestResult& result,
                         const LayoutRect& hit_test_area) {
  DCHECK(IsSelfPaintingLayer() || HasSelfPaintingLayerDescendant());

  // LayoutView should make sure to update layout before entering hit testing
  DCHECK(!GetLayoutObject().GetFrame()->View()->LayoutPending());
  DCHECK(!GetLayoutObject().GetDocument().GetLayoutView()->NeedsLayout());

  const HitTestRequest& request = result.GetHitTestRequest();

  HitTestRecursionData recursion_data(hit_test_area, hit_test_location,
                                      hit_test_location);
  PaintLayer* inside_layer =
      HitTestLayer(this, nullptr, result, recursion_data, false);
  if (!inside_layer && IsRootLayer()) {
    bool fallback = false;
    // If we didn't hit any layers but are still inside the document
    // bounds, then we should fallback to hitting the document.
    // For rect-based hit test, we do the fallback only when the hit-rect
    // is totally within the document bounds.
    if (hit_test_area.Contains(hit_test_location.BoundingBox())) {
      fallback = true;

      // Mouse dragging outside the main document should also be
      // delivered to the document.
      // TODO(miletus): Capture behavior inconsistent with iframes
      // crbug.com/522109.
      // TODO(majidvp): This should apply more consistently across different
      // event types and we should not use RequestType for it. Perhaps best for
      // it to be done at a higher level. See http://crbug.com/505825
    } else if ((request.Active() || request.Release()) &&
               !request.IsChildFrameHitTest()) {
      fallback = true;
    }
    if (fallback) {
      GetLayoutObject().UpdateHitTestResult(
          result, ToLayoutView(GetLayoutObject())
                      .FlipForWritingMode(hit_test_location.Point()));
      inside_layer = this;

      // Don't cache this result since it really wasn't a true hit.
      result.SetCacheable(false);
    }
  }

  // Now determine if the result is inside an anchor - if the urlElement isn't
  // already set.
  Node* node = result.InnerNode();
  if (node && !result.URLElement())
    result.SetURLElement(node->EnclosingLinkEventParentOrSelf());

  // Now return whether we were inside this layer (this will always be true for
  // the root layer).
  return inside_layer;
}

Node* PaintLayer::EnclosingNode() const {
  for (LayoutObject* r = &GetLayoutObject(); r; r = r->Parent()) {
    if (Node* e = r->GetNode())
      return e;
  }
  NOTREACHED();
  return nullptr;
}

bool PaintLayer::IsInTopLayer() const {
  Node* node = GetLayoutObject().GetNode();
  return node && node->IsElementNode() && ToElement(node)->IsInTopLayer();
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding
// where that ray intersects target, and computing the z delta between those two
// points.
static double ComputeZOffset(const HitTestingTransformState& transform_state) {
  // We got an affine transform, so no z-offset
  if (transform_state.accumulated_transform_.IsAffine())
    return 0;

  // Flatten the point into the target plane
  FloatPoint target_point = transform_state.MappedPoint();

  // Now map the point back through the transform, which computes Z.
  FloatPoint3D backmapped_point =
      transform_state.accumulated_transform_.MapPoint(
          FloatPoint3D(target_point));
  return backmapped_point.Z();
}

HitTestingTransformState PaintLayer::CreateLocalTransformState(
    PaintLayer* root_layer,
    PaintLayer* container_layer,
    const HitTestRecursionData& recursion_data,
    const HitTestingTransformState* container_transform_state,
    const LayoutPoint& translation_offset) const {
  // If we're already computing transform state, then it's relative to the
  // container (which we know is non-null).
  // If this is the first time we need to make transform state, then base it
  // off of hitTestLocation, which is relative to rootLayer.
  HitTestingTransformState transform_state =
      container_transform_state
          ? *container_transform_state
          : HitTestingTransformState(recursion_data.location.TransformedPoint(),
                                     recursion_data.location.TransformedRect(),
                                     FloatQuad(FloatRect(recursion_data.rect)));

  LayoutPoint offset;
  if (container_transform_state)
    ConvertToLayerCoords(container_layer, offset);
  else
    ConvertToLayerCoords(root_layer, offset);

  offset.MoveBy(translation_offset);

  LayoutObject* container_layout_object =
      container_layer ? &container_layer->GetLayoutObject() : nullptr;
  if (GetLayoutObject().ShouldUseTransformFromContainer(
          container_layout_object)) {
    TransformationMatrix container_transform;
    GetLayoutObject().GetTransformFromContainer(
        container_layout_object, ToLayoutSize(offset), container_transform);
    transform_state.ApplyTransform(
        container_transform, HitTestingTransformState::kAccumulateTransform);
  } else {
    transform_state.Translate(offset.X().ToInt(), offset.Y().ToInt(),
                              HitTestingTransformState::kAccumulateTransform);
  }

  return transform_state;
}

static bool IsHitCandidate(const PaintLayer* hit_layer,
                           bool can_depth_sort,
                           double* z_offset,
                           const HitTestingTransformState* transform_state) {
  if (!hit_layer)
    return false;

  // The hit layer is depth-sorting with other layers, so just say that it was
  // hit.
  if (can_depth_sort)
    return true;

  // We need to look at z-depth to decide if this layer was hit.
  if (z_offset) {
    DCHECK(transform_state);
    // This is actually computing our z, but that's OK because the hitLayer is
    // coplanar with us.
    double child_z_offset = ComputeZOffset(*transform_state);
    if (child_z_offset > *z_offset) {
      *z_offset = child_z_offset;
      return true;
    }
    return false;
  }

  return true;
}

// hitTestLocation and hitTestRect are relative to rootLayer.
// A 'flattening' layer is one preserves3D() == false.
// transformState.m_accumulatedTransform holds the transform from the containing
// flattening layer.
// transformState.m_lastPlanarPoint is the hitTestLocation in the plane of the
// containing flattening layer.
// transformState.m_lastPlanarQuad is the hitTestRect as a quad in the plane of
// the containing flattening layer.
//
// If zOffset is non-null (which indicates that the caller wants z offset
// information), *zOffset on return is the z offset of the hit point relative to
// the containing flattening layer.
PaintLayer* PaintLayer::HitTestLayer(PaintLayer* root_layer,
                                     PaintLayer* container_layer,
                                     HitTestResult& result,
                                     const HitTestRecursionData& recursion_data,
                                     bool applied_transform,
                                     HitTestingTransformState* transform_state,
                                     double* z_offset) {
  const LayoutObject& layout_object = GetLayoutObject();
  DCHECK_GE(layout_object.GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kCompositingClean);

  if (!IsSelfPaintingLayer() && !HasSelfPaintingLayerDescendant())
    return nullptr;

  if ((result.GetHitTestRequest().GetType() &
       HitTestRequest::kIgnoreZeroOpacityObjects) &&
      !layout_object.HasNonZeroEffectiveOpacity()) {
    return nullptr;
  }

  ShouldRespectOverflowClipType clip_behavior = kRespectOverflowClip;
  if (result.GetHitTestRequest().IgnoreClipping())
    clip_behavior = kIgnoreOverflowClip;

  // We can only reach an SVG foreign object's PaintLayer from
  // LayoutSVGForeignObject::NodeAtFloatPoint (because
  // IsReplacedNormalFlowStacking() true for LayoutSVGForeignObject),
  // where the hit_test_rect has already been transformed to local coordinates.
  bool use_transform = Transform() && !layout_object.IsSVGForeignObject();

  // Apply a transform if we have one.
  if (use_transform && !applied_transform) {
    if (EnclosingPaginationLayer()) {
      return HitTestTransformedLayerInFragments(
          root_layer, container_layer, result, recursion_data, transform_state,
          z_offset, clip_behavior);
    }

    // Make sure the parent's clip rects have been calculated.
    if (Parent()) {
      ClipRect clip_rect;
      Clipper(PaintLayer::kUseGeometryMapper)
          .CalculateBackgroundClipRect(
              ClipRectsContext(
                  root_layer, &root_layer->GetLayoutObject().FirstFragment(),
                  kUncachedClipRects, kExcludeOverlayScrollbarSizeForHitTesting,
                  clip_behavior),
              clip_rect);
      // Go ahead and test the enclosing clip now.
      if (!clip_rect.Intersects(recursion_data.location))
        return nullptr;
    }

    return HitTestLayerByApplyingTransform(root_layer, container_layer, result,
                                           recursion_data, transform_state,
                                           z_offset);
  }

  if (layout_object.HasClipPath() &&
      HitTestClippedOutByClipPath(root_layer, recursion_data.location))
    return nullptr;

  // The natural thing would be to keep HitTestingTransformState on the stack,
  // but it's big, so we heap-allocate.
  HitTestingTransformState* local_transform_state = nullptr;
  base::Optional<HitTestingTransformState> storage;

  if (applied_transform) {
    // We computed the correct state in the caller (above code), so just
    // reference it.
    DCHECK(transform_state);
    local_transform_state = transform_state;
  } else if (transform_state || has3d_transformed_descendant_ ||
             Preserves3D()) {
    // We need transform state for the first time, or to offset the container
    // state, so create it here.
    storage = CreateLocalTransformState(root_layer, container_layer,
                                        recursion_data, transform_state);
    local_transform_state = &*storage;
  }

  // Check for hit test on backface if backface-visibility is 'hidden'
  if (local_transform_state && layout_object.StyleRef().BackfaceVisibility() ==
                                   EBackfaceVisibility::kHidden) {
    TransformationMatrix inverted_matrix =
        local_transform_state->accumulated_transform_.Inverse();
    // If the z-vector of the matrix is negative, the back is facing towards the
    // viewer.
    if (inverted_matrix.M33() < 0)
      return nullptr;
  }

  HitTestingTransformState* unflattened_transform_state = local_transform_state;
  base::Optional<HitTestingTransformState> unflattened_storage;
  if (local_transform_state && !Preserves3D()) {
    // Keep a copy of the pre-flattening state, for computing z-offsets for the
    // container
    unflattened_storage.emplace(*local_transform_state);
    unflattened_transform_state = &*unflattened_storage;
    // This layer is flattening, so flatten the state passed to descendants.
    local_transform_state->Flatten();
  }

  // The following are used for keeping track of the z-depth of the hit point of
  // 3d-transformed descendants.
  double local_z_offset = -std::numeric_limits<double>::infinity();
  double* z_offset_for_descendants_ptr = nullptr;
  double* z_offset_for_contents_ptr = nullptr;

  bool depth_sort_descendants = false;
  if (Preserves3D()) {
    depth_sort_descendants = true;
    // Our layers can depth-test with our container, so share the z depth
    // pointer with the container, if it passed one down.
    z_offset_for_descendants_ptr = z_offset ? z_offset : &local_z_offset;
    z_offset_for_contents_ptr = z_offset ? z_offset : &local_z_offset;
  } else if (z_offset) {
    z_offset_for_descendants_ptr = nullptr;
    // Container needs us to give back a z offset for the hit layer.
    z_offset_for_contents_ptr = z_offset;
  }

  // This variable tracks which layer the mouse ends up being inside.
  PaintLayer* candidate_layer = nullptr;

  // Begin by walking our list of positive layers from highest z-index down to
  // the lowest z-index.
  PaintLayer* hit_layer = HitTestChildren(
      kPositiveZOrderChildren, root_layer, result, recursion_data,
      local_transform_state, z_offset_for_descendants_ptr, z_offset,
      unflattened_transform_state, depth_sort_descendants);
  if (hit_layer) {
    if (!depth_sort_descendants)
      return hit_layer;
    candidate_layer = hit_layer;
  }

  // Now check our overflow objects.
  hit_layer = HitTestChildren(
      kNormalFlowChildren, root_layer, result, recursion_data,
      local_transform_state, z_offset_for_descendants_ptr, z_offset,
      unflattened_transform_state, depth_sort_descendants);
  if (hit_layer) {
    if (!depth_sort_descendants)
      return hit_layer;
    candidate_layer = hit_layer;
  }

  // Collect the fragments. This will compute the clip rectangles for each
  // layer fragment.
  base::Optional<PaintLayerFragments> layer_fragments;
  LayoutPoint offset;
  if (recursion_data.intersects_location) {
    layer_fragments.emplace();
    if (applied_transform) {
      DCHECK(root_layer == this);
      LayoutPoint ignored;
      AppendSingleFragmentIgnoringPagination(
          *layer_fragments, root_layer, nullptr,
          kExcludeOverlayScrollbarSizeForHitTesting, clip_behavior, &ignored);
    } else {
      CollectFragments(*layer_fragments, root_layer, nullptr,
                       kExcludeOverlayScrollbarSizeForHitTesting,
                       clip_behavior);
    }

    if (scrollable_area_ && scrollable_area_->HitTestResizerInFragments(
                                *layer_fragments, recursion_data.location)) {
      layout_object.UpdateHitTestResult(result,
                                        recursion_data.location.Point());
      return this;
    }

    // Next we want to see if the mouse pos is inside the child LayoutObjects of
    // the layer. Check every fragment in reverse order.
    if (IsSelfPaintingLayer()) {
      offset = -LayoutBoxLocation();
      // Hit test with a temporary HitTestResult, because we only want to commit
      // to 'result' if we know we're frontmost.
      HitTestResult temp_result(result.GetHitTestRequest(),
                                recursion_data.original_location);
      bool inside_fragment_foreground_rect = false;

      if (HitTestContentsForFragments(
              *layer_fragments, offset, temp_result, recursion_data.location,
              kHitTestDescendants, inside_fragment_foreground_rect) &&
          IsHitCandidate(this, false, z_offset_for_contents_ptr,
                         unflattened_transform_state)) {
        if (result.GetHitTestRequest().ListBased())
          result.Append(temp_result);
        else
          result = temp_result;
        if (!depth_sort_descendants)
          return this;
        // Foreground can depth-sort with descendant layers, so keep this as a
        // candidate.
        candidate_layer = this;
      } else if (inside_fragment_foreground_rect &&
                 result.GetHitTestRequest().ListBased()) {
        result.Append(temp_result);
      }
    }
  }

  // Now check our negative z-index children.
  hit_layer = HitTestChildren(
      kNegativeZOrderChildren, root_layer, result, recursion_data,
      local_transform_state, z_offset_for_descendants_ptr, z_offset,
      unflattened_transform_state, depth_sort_descendants);
  if (hit_layer) {
    if (!depth_sort_descendants)
      return hit_layer;
    candidate_layer = hit_layer;
  }

  // If we found a layer, return. Child layers, and foreground always render
  // in front of background.
  if (candidate_layer)
    return candidate_layer;

  if (recursion_data.intersects_location && IsSelfPaintingLayer()) {
    HitTestResult temp_result(result.GetHitTestRequest(),
                              recursion_data.original_location);
    bool inside_fragment_background_rect = false;
    if (HitTestContentsForFragments(*layer_fragments, offset, temp_result,
                                    recursion_data.location, kHitTestSelf,
                                    inside_fragment_background_rect) &&
        IsHitCandidate(this, false, z_offset_for_contents_ptr,
                       unflattened_transform_state)) {
      if (result.GetHitTestRequest().ListBased())
        result.Append(temp_result);
      else
        result = temp_result;
      return this;
    }
    if (inside_fragment_background_rect &&
        result.GetHitTestRequest().ListBased())
      result.Append(temp_result);
  }

  return nullptr;
}

bool PaintLayer::HitTestContentsForFragments(
    const PaintLayerFragments& layer_fragments,
    const LayoutPoint& offset,
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    HitTestFilter hit_test_filter,
    bool& inside_clip_rect) const {
  if (layer_fragments.IsEmpty())
    return false;

  for (int i = layer_fragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layer_fragments.at(i);
    if ((hit_test_filter == kHitTestSelf &&
         !fragment.background_rect.Intersects(hit_test_location)) ||
        (hit_test_filter == kHitTestDescendants &&
         !fragment.foreground_rect.Intersects(hit_test_location)))
      continue;
    inside_clip_rect = true;
    LayoutPoint fragment_offset = offset;
    fragment_offset.MoveBy(fragment.layer_bounds.Location());
    if (HitTestContents(result, fragment_offset, hit_test_location,
                        hit_test_filter))
      return true;
  }

  return false;
}

PaintLayer* PaintLayer::HitTestTransformedLayerInFragments(
    PaintLayer* root_layer,
    PaintLayer* container_layer,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* transform_state,
    double* z_offset,
    ShouldRespectOverflowClipType clip_behavior) {
  PaintLayerFragments enclosing_pagination_fragments;
  // FIXME: We're missing a sub-pixel offset here crbug.com/348728

  EnclosingPaginationLayer()->CollectFragments(
      enclosing_pagination_fragments, root_layer, nullptr,
      kExcludeOverlayScrollbarSizeForHitTesting, clip_behavior, nullptr,
      LayoutSize());

  for (const auto& fragment : enclosing_pagination_fragments) {
    // Apply the page/column clip for this fragment, as well as any clips
    // established by layers in between us and the enclosing pagination layer.
    LayoutRect clip_rect = fragment.background_rect.Rect();
    if (!recursion_data.location.Intersects(clip_rect))
      continue;

    PaintLayer* hit_layer = HitTestLayerByApplyingTransform(
        root_layer, container_layer, result, recursion_data, transform_state,
        z_offset, fragment.fragment_data->PaginationOffset());
    if (hit_layer)
      return hit_layer;
  }

  return nullptr;
}

PaintLayer* PaintLayer::HitTestLayerByApplyingTransform(
    PaintLayer* root_layer,
    PaintLayer* container_layer,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* transform_state,
    double* z_offset,
    const LayoutPoint& translation_offset) {
  // Create a transform state to accumulate this transform.
  HitTestingTransformState new_transform_state =
      CreateLocalTransformState(root_layer, container_layer, recursion_data,
                                transform_state, translation_offset);

  // If the transform can't be inverted, then don't hit test this layer at all.
  if (!new_transform_state.accumulated_transform_.IsInvertible())
    return nullptr;

  // Compute the point and the hit test rect in the coords of this layer by
  // using the values from the transformState, which store the point and quad in
  // the coords of the last flattened layer, and the accumulated transform which
  // lets up map through preserve-3d layers.
  //
  // We can't just map hitTestLocation and hitTestRect because they may have
  // been flattened (losing z) by our container.
  FloatPoint local_point = new_transform_state.MappedPoint();
  FloatQuad local_point_quad = new_transform_state.MappedQuad();
  LayoutRect bounds_of_mapped_area = new_transform_state.BoundsOfMappedArea();
  base::Optional<HitTestLocation> new_location;
  if (recursion_data.location.IsRectBasedTest())
    new_location.emplace(local_point, local_point_quad);
  else
    new_location.emplace(local_point);
  HitTestRecursionData new_recursion_data(bounds_of_mapped_area, *new_location,
                                          recursion_data.original_location);

  // Now do a hit test with the root layer shifted to be us.
  return HitTestLayer(this, container_layer, result, new_recursion_data, true,
                      &new_transform_state, z_offset);
}

bool PaintLayer::HitTestContents(HitTestResult& result,
                                 const LayoutPoint& fragment_offset,
                                 const HitTestLocation& hit_test_location,
                                 HitTestFilter hit_test_filter) const {
  DCHECK(IsSelfPaintingLayer() || HasSelfPaintingLayerDescendant());
  if (!GetLayoutObject().HitTestAllPhases(result, hit_test_location,
                                          fragment_offset, hit_test_filter)) {
    // It's wrong to set innerNode, but then claim that you didn't hit anything,
    // unless it is a list-based test.
    DCHECK(!result.InnerNode() || (result.GetHitTestRequest().ListBased() &&
                                   result.ListBasedTestResult().size()));
    return false;
  }

  if (!result.InnerNode()) {
    // We hit something anonymous, and we didn't find a DOM node ancestor in
    // this layer.

    if (GetLayoutObject().IsLayoutFlowThread()) {
      // For a flow thread it's safe to just say that we didn't hit anything.
      // That means that we'll continue as normally, and eventually hit a column
      // set sibling instead. Column sets are also anonymous, but, unlike flow
      // threads, they don't establish layers, so we'll fall back and hit the
      // multicol container parent (which should have a DOM node).
      return false;
    }

    Node* e = EnclosingNode();
    // FIXME: should be a call to result.setNodeAndPosition. What we would
    // really want to do here is to return and look for the nearest
    // non-anonymous ancestor, and ignore aunts and uncles on our way. It's bad
    // to look for it manually like we do here, and give up on setting a local
    // point in the result, because that has bad implications for text selection
    // and caretRangeFromPoint(). See crbug.com/461791
    // This code path only ever hits in fullscreen tests.
    result.SetInnerNode(e);
  }
  return true;
}

bool PaintLayer::IsReplacedNormalFlowStacking() const {
  return GetLayoutObject().IsSVGForeignObject();
}

void PaintLayer::SetNeedsCompositingLayerAssignment() {
  needs_compositing_layer_assignment_ = true;

  for (PaintLayer* curr = CompositingContainer();
       curr && !curr->StackingDescendantNeedsCompositingLayerAssignment();
       curr = curr->CompositingContainer()) {
    curr->descendant_needs_compositing_layer_assignment_ = true;
  }
}

void PaintLayer::ClearNeedsCompositingLayerAssignment() {
  needs_compositing_layer_assignment_ = false;
  descendant_needs_compositing_layer_assignment_ = false;
}

void PaintLayer::SetNeedsCompositingRequirementsUpdate() {
  for (PaintLayer* curr = this;
       curr && !curr->DescendantMayNeedCompositingRequirementsUpdate();
       curr = curr->Parent()) {
    curr->descendant_may_need_compositing_requirements_update_ = true;
  }
}

PaintLayer* PaintLayer::HitTestChildren(
    ChildrenIteration childrento_visit,
    PaintLayer* root_layer,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* transform_state,
    double* z_offset_for_descendants,
    double* z_offset,
    HitTestingTransformState* unflattened_transform_state,
    bool depth_sort_descendants) {
  if (!HasSelfPaintingLayerDescendant())
    return nullptr;

  if (!stacking_node_)
    return nullptr;

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  PaintLayer* stop_layer = stop_node ? stop_node->PaintingLayer() : nullptr;

  PaintLayer* result_layer = nullptr;
  PaintLayerStackingNodeReverseIterator iterator(*stacking_node_,
                                                 childrento_visit);
  while (PaintLayer* child_layer = iterator.Next()) {
    if (child_layer->IsReplacedNormalFlowStacking())
      continue;

    // Calling IsDescendantOf is sad (slow), but it's the only way to tell
    // whether the child layer is a descendant of the stop node.
    if (stop_layer == this &&
        child_layer->GetLayoutObject().IsDescendantOf(stop_node)) {
      continue;
    }

    PaintLayer* hit_layer = nullptr;
    HitTestResult temp_result(result.GetHitTestRequest(),
                              recursion_data.original_location);
    hit_layer = child_layer->HitTestLayer(
        root_layer, this, temp_result, recursion_data, false, transform_state,
        z_offset_for_descendants);

    // If it is a list-based test, we can safely append the temporary result
    // since it might had hit nodes but not necesserily had hitLayer set.
    if (result.GetHitTestRequest().ListBased())
      result.Append(temp_result);

    if (IsHitCandidate(hit_layer, depth_sort_descendants, z_offset,
                       unflattened_transform_state)) {
      result_layer = hit_layer;
      if (!result.GetHitTestRequest().ListBased())
        result = temp_result;
      if (!depth_sort_descendants)
        break;
    }
  }

  return result_layer;
}

FloatRect PaintLayer::FilterReferenceBox(const FilterOperations& filter,
                                         float zoom) const {
  if (!filter.HasReferenceFilter())
    return FloatRect();

  FloatRect reference_box(PhysicalBoundingBoxIncludingStackingChildren(
      LayoutPoint(), PaintLayer::CalculateBoundsOptions::
                         kIncludeTransformsAndCompositedChildLayers));
  if (zoom != 1)
    reference_box.Scale(1 / zoom);
  return reference_box;
}

bool PaintLayer::HitTestClippedOutByClipPath(
    PaintLayer* root_layer,
    const HitTestLocation& hit_test_location) const {
  DCHECK(GetLayoutObject().HasClipPath());
  DCHECK(IsSelfPaintingLayer());
  DCHECK(root_layer);

  LayoutRect origin;
  if (EnclosingPaginationLayer())
    ConvertFromFlowThreadToVisualBoundingBoxInAncestor(root_layer, origin);
  else
    ConvertToLayerCoords(root_layer, origin);

  FloatPoint point(hit_test_location.Point() - origin.Location());
  FloatRect reference_box(
      ClipPathClipper::LocalReferenceBox(GetLayoutObject()));

  ClipPathOperation* clip_path_operation =
      GetLayoutObject().StyleRef().ClipPath();
  DCHECK(clip_path_operation);
  if (clip_path_operation->GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation* clip_path =
        ToShapeClipPathOperation(clip_path_operation);
    return !clip_path->GetPath(reference_box).Contains(point);
  }
  DCHECK_EQ(clip_path_operation->GetType(), ClipPathOperation::REFERENCE);
  SVGResource* resource =
      ToReferenceClipPathOperation(*clip_path_operation).Resource();
  LayoutSVGResourceContainer* container =
      resource ? resource->ResourceContainer() : nullptr;
  if (!container || container->ResourceType() != kClipperResourceType)
    return false;
  auto* clipper = ToLayoutSVGResourceClipper(container);
  // If the clipPath is using "userspace on use" units, then the origin of
  // the coordinate system is the top-left of the reference box, so adjust
  // the point accordingly.
  if (clipper->ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeUserspaceonuse)
    point.MoveBy(-reference_box.Location());
  // Unzoom the point and the reference box, since the <clipPath> geometry is
  // not zoomed.
  float inverse_zoom = 1 / GetLayoutObject().StyleRef().EffectiveZoom();
  point.Scale(inverse_zoom, inverse_zoom);
  reference_box.Scale(inverse_zoom);
  HitTestLocation location(point);
  return !clipper->HitTestClipContent(reference_box, location);
}

bool PaintLayer::IntersectsDamageRect(
    const LayoutRect& layer_bounds,
    const LayoutRect& damage_rect,
    const LayoutPoint& offset_from_root) const {
  // Always examine the canvas and the root.
  // FIXME: Could eliminate the isDocumentElement() check if we fix background
  // painting so that the LayoutView paints the root's background.
  if (IsRootLayer() || GetLayoutObject().IsDocumentElement())
    return true;

  // If we aren't an inline flow, and our layer bounds do intersect the damage
  // rect, then we can go ahead and return true.
  LayoutView* view = GetLayoutObject().View();
  DCHECK(view);
  if (view && !GetLayoutObject().IsLayoutInline()) {
    if (layer_bounds.Intersects(damage_rect))
      return true;
  }

  // Otherwise we need to compute the bounding box of this single layer and see
  // if it intersects the damage rect.
  return PhysicalBoundingBox(offset_from_root).Intersects(damage_rect);
}

LayoutRect PaintLayer::LogicalBoundingBox() const {
  LayoutRect rect = GetLayoutObject().VisualOverflowRect();

  if (GetLayoutObject().IsEffectiveRootScroller() || IsRootLayer()) {
    rect.Unite(LayoutRect(rect.Location(),
                          GetLayoutObject().View()->ViewRect().Size()));
  }

  return rect;
}

static inline LayoutRect FlippedLogicalBoundingBox(
    LayoutRect bounding_box,
    LayoutObject& layout_object) {
  LayoutRect result = bounding_box;
  if (layout_object.IsBox())
    ToLayoutBox(layout_object).FlipForWritingMode(result);
  else
    layout_object.ContainingBlock()->FlipForWritingMode(result);
  return result;
}

LayoutRect PaintLayer::PhysicalBoundingBox(
    const PaintLayer* ancestor_layer) const {
  LayoutPoint offset_from_root;
  ConvertToLayerCoords(ancestor_layer, offset_from_root);
  return PhysicalBoundingBox(offset_from_root);
}

LayoutRect PaintLayer::PhysicalBoundingBox(
    const LayoutPoint& offset_from_root) const {
  LayoutRect result =
      FlippedLogicalBoundingBox(LogicalBoundingBox(), GetLayoutObject());
  result.MoveBy(offset_from_root);
  return result;
}

LayoutRect PaintLayer::FragmentsBoundingBox(
    const PaintLayer* ancestor_layer) const {
  if (!EnclosingPaginationLayer())
    return PhysicalBoundingBox(ancestor_layer);

  LayoutRect result =
      FlippedLogicalBoundingBox(LogicalBoundingBox(), GetLayoutObject());
  ConvertFromFlowThreadToVisualBoundingBoxInAncestor(ancestor_layer, result);
  return result;
}

LayoutRect PaintLayer::BoundingBoxForCompositingOverlapTest() const {
  // Apply NeverIncludeTransformForAncestorLayer, because the geometry map in
  // CompositingInputsUpdater will take care of applying the transform of |this|
  // (== the ancestorLayer argument to boundingBoxForCompositing).
  // TODO(trchen): Layer fragmentation is inhibited across compositing boundary.
  // Should we return the unfragmented bounds for overlap testing? Or perhaps
  // assume fragmented layers always overlap?
  return OverlapBoundsIncludeChildren()
             ? BoundingBoxForCompositingInternal(
                   *this, nullptr, kNeverIncludeTransformForAncestorLayer)
             : FragmentsBoundingBox(this);
}

bool PaintLayer::OverlapBoundsIncludeChildren() const {
  return HasFilterThatMovesPixels();
}

void PaintLayer::ExpandRectForStackingChildren(
    const PaintLayer& composited_layer,
    LayoutRect& result,
    PaintLayer::CalculateBoundsOptions options) const {
  if (!StackingNode())
    return;

  DCHECK(GetLayoutObject().StyleRef().IsStackingContext() ||
         !StackingNode()->HasPositiveZOrderList());

#if DCHECK_IS_ON()
  LayerListMutationDetector mutation_checker(
      const_cast<PaintLayer*>(this)->StackingNode());
#endif

  PaintLayerStackingNodeIterator iterator(*StackingNode(), kAllChildren);
  while (PaintLayer* child_layer = iterator.Next()) {
    // Here we exclude both directly composited layers and squashing layers
    // because those Layers don't paint into the graphics layer
    // for this Layer. For example, the bounds of squashed Layers
    // will be included in the computation of the appropriate squashing
    // GraphicsLayer.
    if (options != PaintLayer::CalculateBoundsOptions::
                       kIncludeTransformsAndCompositedChildLayers &&
        child_layer->GetCompositingState() != kNotComposited)
      continue;
    result.Unite(child_layer->BoundingBoxForCompositingInternal(
        composited_layer, this, options));
  }
}

LayoutRect PaintLayer::PhysicalBoundingBoxIncludingStackingChildren(
    const LayoutPoint& offset_from_root,
    CalculateBoundsOptions options) const {
  LayoutRect result = PhysicalBoundingBox(LayoutPoint());
  ExpandRectForStackingChildren(*this, result, options);

  result.MoveBy(offset_from_root);
  return result;
}

LayoutRect PaintLayer::BoundingBoxForCompositing() const {
  return BoundingBoxForCompositingInternal(
      *this, nullptr, kMaybeIncludeTransformForAncestorLayer);
}

LayoutRect PaintLayer::BoundingBoxForCompositingInternal(
    const PaintLayer& composited_layer,
    const PaintLayer* stacking_parent,
    CalculateBoundsOptions options) const {
  if (!IsSelfPaintingLayer())
    return LayoutRect();

  // FIXME: This could be improved to do a check like
  // hasVisibleNonCompositingDescendantLayers() (bug 92580).
  if (this != &composited_layer && !HasVisibleContent() &&
      !HasVisibleDescendant())
    return LayoutRect();

  if (GetLayoutObject().IsEffectiveRootScroller() || IsRootLayer()) {
    // In root layer scrolling mode, the main GraphicsLayer is the size of the
    // layout viewport. In non-RLS mode, it is the union of the layout viewport
    // and the document's layout overflow rect.
    IntRect result = IntRect();
    if (LocalFrameView* frame_view = GetLayoutObject().GetFrameView())
      result = IntRect(IntPoint(), frame_view->Size());
    return LayoutRect(result);
  }

  // The layer created for the LayoutFlowThread is just a helper for painting
  // and hit-testing, and should not contribute to the bounding box. The
  // LayoutMultiColumnSets will contribute the correct size for the layout
  // content of the multicol container.
  if (GetLayoutObject().IsLayoutFlowThread())
    return LayoutRect();

  // If there is a clip applied by an ancestor to this PaintLayer but below or
  // equal to |ancestorLayer|, apply that clip.
  LayoutRect result = Clipper(PaintLayer::kDoNotUseGeometryMapper)
                          .LocalClipRect(composited_layer);

  result.Intersect(PhysicalBoundingBox(LayoutPoint()));

  ExpandRectForStackingChildren(composited_layer, result, options);

  // Only enlarge by the filter outsets if we know the filter is going to be
  // rendered in software.  Accelerated filters will handle their own outsets.
  if (PaintsWithFilters())
    result = MapLayoutRectForFilter(result);

  if (Transform() && (options == kIncludeTransformsAndCompositedChildLayers ||
                      ((PaintsWithTransform(kGlobalPaintNormalPhase) &&
                        (this != &composited_layer ||
                         options == kMaybeIncludeTransformForAncestorLayer)))))
    result = Transform()->MapRect(result);

  if (ShouldFragmentCompositedBounds(&composited_layer)) {
    ConvertFromFlowThreadToVisualBoundingBoxInAncestor(&composited_layer,
                                                       result);
    return result;
  }

  if (stacking_parent) {
    LayoutPoint delta;
    ConvertToLayerCoords(stacking_parent, delta);
    result.MoveBy(delta);
  }
  return result;
}

CompositingState PaintLayer::GetCompositingState() const {
  DCHECK(IsAllowedToQueryCompositingState());

  // This is computed procedurally so there is no redundant state variable that
  // can get out of sync from the real actual compositing state.

  if (GroupedMapping()) {
    DCHECK(!GetCompositedLayerMapping());
    return kPaintsIntoGroupedBacking;
  }

  if (!GetCompositedLayerMapping())
    return kNotComposited;

  return kPaintsIntoOwnBacking;
}

bool PaintLayer::IsAllowedToQueryCompositingState() const {
  if (g_compositing_query_mode == kCompositingQueriesAreAllowed ||
      RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return true;
  return GetLayoutObject().GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kInCompositingUpdate;
}

CompositedLayerMapping* PaintLayer::GetCompositedLayerMapping() const {
  DCHECK(IsAllowedToQueryCompositingState());
  return rare_data_ ? rare_data_->composited_layer_mapping.get() : nullptr;
}

GraphicsLayer* PaintLayer::GraphicsLayerBacking(const LayoutObject* obj) const {
  switch (GetCompositingState()) {
    case kNotComposited:
      return nullptr;
    case kPaintsIntoGroupedBacking:
      return GroupedMapping()->SquashingLayer();
    default:
      return (obj != &GetLayoutObject() &&
              GetCompositedLayerMapping()->ScrollingContentsLayer())
                 ? GetCompositedLayerMapping()->ScrollingContentsLayer()
                 : GetCompositedLayerMapping()->MainGraphicsLayer();
  }
}

void PaintLayer::EnsureCompositedLayerMapping() {
  if (rare_data_ && rare_data_->composited_layer_mapping)
    return;

  EnsureRareData().composited_layer_mapping =
      std::make_unique<CompositedLayerMapping>(*this);
  rare_data_->composited_layer_mapping->SetNeedsGraphicsLayerUpdate(
      kGraphicsLayerUpdateSubtree);

  if (PaintLayerResourceInfo* resource_info = ResourceInfo())
    resource_info->InvalidateFilterChain();
}

void PaintLayer::ClearCompositedLayerMapping(bool layer_being_destroyed) {
  if (!layer_being_destroyed) {
    // We need to make sure our decendants get a geometry update. In principle,
    // we could call setNeedsGraphicsLayerUpdate on our children, but that would
    // require walking the z-order lists to find them. Instead, we
    // over-invalidate by marking our parent as needing a geometry update.
    if (PaintLayer* compositing_parent =
            EnclosingLayerWithCompositedLayerMapping(kExcludeSelf))
      compositing_parent->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
  }

  if (rare_data_)
    rare_data_->composited_layer_mapping.reset();

  if (layer_being_destroyed)
    return;

  if (PaintLayerResourceInfo* resource_info = ResourceInfo())
    resource_info->InvalidateFilterChain();
}

void PaintLayer::SetGroupedMapping(CompositedLayerMapping* grouped_mapping,
                                   SetGroupMappingOptions options) {
  CompositedLayerMapping* old_grouped_mapping = GroupedMapping();
  if (grouped_mapping == old_grouped_mapping)
    return;

  if (options == kInvalidateLayerAndRemoveFromMapping && old_grouped_mapping) {
    old_grouped_mapping->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateSubtree);
    old_grouped_mapping->RemoveLayerFromSquashingGraphicsLayer(this);
  }
  if (rare_data_ || grouped_mapping)
    EnsureRareData().grouped_mapping = grouped_mapping;
#if DCHECK_IS_ON()
  DCHECK(!grouped_mapping ||
         grouped_mapping->VerifyLayerInSquashingVector(this));
#endif
  if (options == kInvalidateLayerAndRemoveFromMapping && grouped_mapping)
    grouped_mapping->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
}

bool PaintLayer::NeedsCompositedScrolling() const {
  return scrollable_area_ && scrollable_area_->NeedsCompositedScrolling();
}

bool PaintLayer::PaintsWithTransform(
    GlobalPaintFlags global_paint_flags) const {
  return Transform() && !PaintsIntoOwnBacking(global_paint_flags);
}

bool PaintLayer::PaintsIntoOwnBacking(
    GlobalPaintFlags global_paint_flags) const {
  return !(global_paint_flags & kGlobalPaintFlattenCompositingLayers) &&
         GetCompositingState() == kPaintsIntoOwnBacking;
}

bool PaintLayer::PaintsIntoOwnOrGroupedBacking(
    GlobalPaintFlags global_paint_flags) const {
  return !(global_paint_flags & kGlobalPaintFlattenCompositingLayers) &&
         GetCompositingState() != kNotComposited;
}

bool PaintLayer::SupportsSubsequenceCaching() const {
  if (EnclosingPaginationLayer())
    return false;

  // SVG paints atomically.
  if (GetLayoutObject().IsSVGRoot())
    return true;

  // Create subsequence for only stacking contexts whose painting are atomic.
  return GetLayoutObject().StyleRef().IsStackingContext();
}

ScrollingCoordinator* PaintLayer::GetScrollingCoordinator() {
  Page* page = GetLayoutObject().GetFrame()->GetPage();
  return (!page) ? nullptr : page->GetScrollingCoordinator();
}

bool PaintLayer::CompositesWithTransform() const {
  return TransformAncestor() || Transform();
}

bool PaintLayer::CompositesWithOpacity() const {
  return OpacityAncestor() || GetLayoutObject().StyleRef().HasOpacity();
}

bool PaintLayer::BackgroundIsKnownToBeOpaqueInRect(
    const LayoutRect& local_rect) const {
  if (PaintsWithTransparency(kGlobalPaintNormalPhase))
    return false;

  // We can't use hasVisibleContent(), because that will be true if our
  // layoutObject is hidden, but some child is visible and that child doesn't
  // cover the entire rect.
  if (GetLayoutObject().StyleRef().Visibility() != EVisibility::kVisible)
    return false;

  if (GetLayoutObject().HasMask() || GetLayoutObject().HasClipPath())
    return false;

  if (PaintsWithFilters() &&
      GetLayoutObject().StyleRef().Filter().HasFilterThatAffectsOpacity())
    return false;

  // FIXME: Handle simple transforms.
  if (Transform() && GetCompositingState() != kPaintsIntoOwnBacking)
    return false;

  if (!RuntimeEnabledFeatures::CompositeOpaqueFixedPositionEnabled() &&
      GetLayoutObject().StyleRef().GetPosition() == EPosition::kFixed &&
      GetCompositingState() != kPaintsIntoOwnBacking)
    return false;

  // FIXME: We currently only check the immediate layoutObject,
  // which will miss many cases where additional layout objects paint
  // into this layer.
  if (GetLayoutObject().BackgroundIsKnownToBeOpaqueInRect(local_rect))
    return true;

  // We can't consult child layers if we clip, since they might cover
  // parts of the rect that are clipped out.
  if (GetLayoutObject().HasClipRelatedProperty())
    return false;

  // TODO(schenney): This could be improved by unioning the opaque regions of
  // all the children.  That would require a refactoring because currently
  // children just check they at least cover the given rect, but a unioning
  // method would require children to compute and report their rects.
  return ChildBackgroundIsKnownToBeOpaqueInRect(local_rect);
}

bool PaintLayer::ChildBackgroundIsKnownToBeOpaqueInRect(
    const LayoutRect& local_rect) const {
  if (!stacking_node_)
    return false;

  PaintLayerStackingNodeReverseIterator reverse_iterator(
      *stacking_node_,
      kPositiveZOrderChildren | kNormalFlowChildren | kNegativeZOrderChildren);
  while (PaintLayer* child_layer = reverse_iterator.Next()) {
    // Stop at composited paint boundaries and non-self-painting layers.
    if (child_layer->IsPaintInvalidationContainer())
      continue;

    if (!child_layer->CanUseConvertToLayerCoords())
      continue;

    LayoutPoint child_offset;
    LayoutRect child_local_rect(local_rect);
    child_layer->ConvertToLayerCoords(this, child_offset);
    child_local_rect.MoveBy(-child_offset);

    if (child_layer->BackgroundIsKnownToBeOpaqueInRect(child_local_rect))
      return true;
  }
  return false;
}

bool PaintLayer::ShouldBeSelfPaintingLayer() const {
  // TODO(crbug.com/839341): Remove ScrollTimeline check once we support
  // main-thread AnimationWorklet and don't need to promote the scroll-source.
  return GetLayoutObject().LayerTypeRequired() == kNormalPaintLayer ||
         (scrollable_area_ && scrollable_area_->HasOverlayScrollbars()) ||
         ScrollsOverflow() ||
         ScrollTimeline::HasActiveScrollTimeline(GetLayoutObject().GetNode());
}

void PaintLayer::UpdateSelfPaintingLayer() {
  bool is_self_painting_layer = ShouldBeSelfPaintingLayer();
  if (IsSelfPaintingLayer() == is_self_painting_layer)
    return;

  // Invalidate the old subsequences which may no longer contain some
  // descendants of this layer because of the self painting status change.
  SetNeedsRepaint();
  is_self_painting_layer_ = is_self_painting_layer;
  self_painting_status_changed_ = true;
  // Self-painting change can change the compositing container chain;
  // invalidate the new chain in addition to the old one.
  MarkCompositingContainerChainForNeedsRepaint();

  if (PaintLayer* parent = Parent()) {
    parent->MarkAncestorChainForDescendantDependentFlagsUpdate();

    if (PaintLayer* enclosing_self_painting_layer =
            parent->EnclosingSelfPaintingLayer()) {
      if (is_self_painting_layer)
        MergeNeedsPaintPhaseFlagsFrom(*enclosing_self_painting_layer);
      else
        enclosing_self_painting_layer->MergeNeedsPaintPhaseFlagsFrom(*this);
    }
  }
}

PaintLayer* PaintLayer::EnclosingSelfPaintingLayer() {
  PaintLayer* layer = this;
  while (layer && !layer->IsSelfPaintingLayer())
    layer = layer->Parent();
  return layer;
}

bool PaintLayer::HasNonEmptyChildLayoutObjects() const {
  // Some HTML can cause whitespace text nodes to have layoutObjects, like:
  // <div>
  // <img src=...>
  // </div>
  // so test for 0x0 LayoutTexts here
  for (LayoutObject* child = GetLayoutObject().SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->HasLayer()) {
      if (child->IsLayoutInline() || !child->IsBox())
        return true;

      if (ToLayoutBox(child)->Size().Width() > 0 ||
          ToLayoutBox(child)->Size().Height() > 0)
        return true;
    }
  }
  return false;
}

bool PaintLayer::HasBoxDecorationsOrBackground() const {
  return GetLayoutObject().StyleRef().HasBoxDecorations() ||
         GetLayoutObject().StyleRef().HasBackground();
}

bool PaintLayer::HasVisibleBoxDecorations() const {
  if (!HasVisibleContent())
    return false;

  return HasBoxDecorationsOrBackground() || HasOverflowControls();
}

void PaintLayer::UpdateFilters(const ComputedStyle* old_style,
                               const ComputedStyle& new_style) {
  if (!filter_on_effect_node_dirty_) {
    filter_on_effect_node_dirty_ =
        old_style ? !old_style->FilterDataEquivalent(new_style) ||
                        !old_style->ReflectionDataEquivalent(new_style)
                  : new_style.HasFilterInducingProperty();
  }

  if (!new_style.HasFilterInducingProperty() &&
      (!old_style || !old_style->HasFilterInducingProperty()))
    return;

  const bool had_resource_info = ResourceInfo();
  if (new_style.HasFilterInducingProperty())
    new_style.Filter().AddClient(EnsureResourceInfo());
  if (had_resource_info && old_style)
    old_style->Filter().RemoveClient(*ResourceInfo());
  if (PaintLayerResourceInfo* resource_info = ResourceInfo())
    resource_info->InvalidateFilterChain();
}

void PaintLayer::UpdateClipPath(const ComputedStyle* old_style,
                                const ComputedStyle& new_style) {
  ClipPathOperation* new_clip = new_style.ClipPath();
  ClipPathOperation* old_clip = old_style ? old_style->ClipPath() : nullptr;
  if (!new_clip && !old_clip)
    return;
  const bool had_resource_info = ResourceInfo();
  if (auto* reference_clip = ToReferenceClipPathOperationOrNull(new_clip))
    reference_clip->AddClient(EnsureResourceInfo());
  if (had_resource_info) {
    if (auto* old_reference_clip = ToReferenceClipPathOperationOrNull(old_clip))
      old_reference_clip->RemoveClient(*ResourceInfo());
  }
}

bool PaintLayer::AttemptDirectCompositingUpdate(
    const StyleDifference& diff,
    const ComputedStyle* old_style) {
  CompositingReasons old_potential_compositing_reasons_from_style =
      PotentialCompositingReasonsFromStyle();
  if (Compositor() &&
      (diff.HasDifference() || needs_compositing_reasons_update_))
    Compositor()->UpdatePotentialCompositingReasonsFromStyle(*this);
  needs_compositing_reasons_update_ = false;

  // This function implements an optimization for transforms and opacity.
  // A common pattern is for a touchmove handler to update the transform
  // and/or an opacity of an element every frame while the user moves their
  // finger across the screen. The conditions below recognize when the
  // compositing state is set up to receive a direct transform or opacity
  // update.

  if (!diff.HasAtMostPropertySpecificDifferences(
          StyleDifference::kTransformChanged |
          StyleDifference::kOpacityChanged))
    return false;
  // The potentialCompositingReasonsFromStyle could have changed without
  // a corresponding StyleDifference if an animation started or ended.
  if (PotentialCompositingReasonsFromStyle() !=
      old_potential_compositing_reasons_from_style)
    return false;
  if (!rare_data_ || !rare_data_->composited_layer_mapping)
    return false;

  // To cut off almost all the work in the compositing update for
  // this case, we treat inline transforms has having assumed overlap
  // (similar to how we treat animated transforms). Notice that we read
  // CompositingReasonInlineTransform from the m_compositingReasons, which
  // means that the inline transform actually triggered assumed overlap in
  // the overlap map.
  if (diff.TransformChanged() &&
      (!rare_data_ || !(rare_data_->compositing_reasons &
                        CompositingReason::kInlineTransform)))
    return false;

  // We composite transparent Layers differently from non-transparent
  // Layers even when the non-transparent Layers are already a
  // stacking context.
  if (diff.OpacityChanged() &&
      layout_object_.StyleRef().HasOpacity() != old_style->HasOpacity())
    return false;

  // Changes in pointer-events affect hit test visibility of the scrollable
  // area and its |m_scrollsOverflow| value which determines if the layer
  // requires composited scrolling or not.
  if (scrollable_area_ &&
      layout_object_.StyleRef().PointerEvents() != old_style->PointerEvents())
    return false;

  UpdateTransform(old_style, GetLayoutObject().StyleRef());

  // FIXME: Consider introducing a smaller graphics layer update scope
  // that just handles transforms and opacity. GraphicsLayerUpdateLocal
  // will also program bounds, clips, and many other properties that could
  // not possibly have changed.
  rare_data_->composited_layer_mapping->SetNeedsGraphicsLayerUpdate(
      kGraphicsLayerUpdateLocal);
  if (Compositor()) {
    Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateAfterGeometryChange);
  }

  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterStyleChange(old_style);
  }

  return true;
}

void PaintLayer::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  UpdateScrollableArea();

  if (AttemptDirectCompositingUpdate(diff, old_style)) {
    if (diff.HasDifference())
      GetLayoutObject().SetNeedsPaintPropertyUpdate();
    return;
  }

  if (PaintLayerStackingNode::StyleDidChange(this, old_style))
    MarkAncestorChainForDescendantDependentFlagsUpdate();

  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterStyleChange(old_style);
  }

  // Overlay scrollbars can make this layer self-painting so we need
  // to recompute the bit once scrollbars have been updated.
  UpdateSelfPaintingLayer();

  const ComputedStyle& new_style = GetLayoutObject().StyleRef();

  if (diff.CompositingReasonsChanged()) {
    SetNeedsCompositingInputsUpdate();
  } else {
    // For querying stale GetCompositingState().
    DisableCompositingQueryAsserts disable;

    // Compositing inputs update is required when the PaintLayer is currently
    // composited. This is because even style changes as simple as background
    // color change, or pointer-events state change, can update compositing
    // state.
    if (old_style && GetCompositingState() == kPaintsIntoOwnBacking)
      SetNeedsCompositingInputsUpdate();
  }

  if (diff.NeedsLayout())
    SetNeedsCompositingInputsUpdate();

  // A scroller that changes background color might become opaque or not
  // opaque, which in turn affects whether it can be composited on low-DPI
  // screens.
  if (GetScrollableArea() && GetScrollableArea()->ScrollsOverflow() &&
      diff.HasDifference()) {
    SetNeedsCompositingInputsUpdate();
  }

  if (diff.TransformChanged() || diff.OpacityChanged() ||
      diff.ZIndexChanged() || diff.FilterChanged() ||
      diff.BackdropFilterChanged() || diff.CssClipChanged() ||
      diff.BlendModeChanged() || diff.MaskChanged()) {
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
    SetNeedsCompositingInputsUpdate();
  }

  // HasNonContainedAbsolutePositionDescendant depends on position changes.
  if (!old_style || old_style->GetPosition() != new_style.GetPosition())
    MarkAncestorChainForDescendantDependentFlagsUpdate();

  UpdateTransform(old_style, new_style);
  UpdateFilters(old_style, new_style);
  UpdateClipPath(old_style, new_style);

  if (!NeedsRepaint()) {
    if (diff.ZIndexChanged()) {
      // We don't need to invalidate paint of objects when paint order
      // changes. However, we do need to repaint the containing stacking
      // context, in order to generate new paint chunks in the correct order.
      // Raster invalidation will be issued if needed during paint.
      SetNeedsRepaint();
    } else if (old_style) {
      // Change of PaintedOutputInvisible() will affect existence of paint
      // chunks, so needs repaint.
      PaintLayerPainter painter(*this);
      // It's fine for PaintedOutputInvisible() to access the current
      // compositing state.
      DisableCompositingQueryAsserts disable;
      if (painter.PaintedOutputInvisible(*old_style) !=
          painter.PaintedOutputInvisible(new_style))
        SetNeedsRepaint();
    }
  }
}

LayoutPoint PaintLayer::LocationInternal() const {
  LayoutPoint result(location_);
  PaintLayer* containing_layer = ContainingLayer();
  if (containing_layer && containing_layer->IsRootLayer() &&
      containing_layer->GetLayoutObject().HasOverflowClip()) {
    result -= containing_layer->GetLayoutBox()->ScrolledContentOffset();
  }
  return result;
}

PaintLayerClipper PaintLayer::Clipper(
    GeometryMapperOption geometry_mapper_option) const {
  return PaintLayerClipper(*this, geometry_mapper_option == kUseGeometryMapper);
}

bool PaintLayer::ScrollsOverflow() const {
  if (PaintLayerScrollableArea* scrollable_area = GetScrollableArea())
    return scrollable_area->ScrollsOverflow();

  return false;
}

FilterOperations PaintLayer::FilterOperationsIncludingReflection() const {
  const auto& style = GetLayoutObject().StyleRef();
  FilterOperations filter_operations = style.Filter();
  if (GetLayoutObject().HasReflection() && GetLayoutObject().IsBox()) {
    BoxReflection reflection = BoxReflectionForPaintLayer(*this, style);
    filter_operations.Operations().push_back(
        BoxReflectFilterOperation::Create(reflection));
  }
  return filter_operations;
}

void PaintLayer::UpdateCompositorFilterOperationsForFilter(
    CompositorFilterOperations& operations) const {
  const auto& style = GetLayoutObject().StyleRef();
  float zoom = style.EffectiveZoom();
  auto filter = FilterOperationsIncludingReflection();
  FloatRect reference_box = FilterReferenceBox(filter, zoom);
  if (!operations.IsEmpty() && !filter_on_effect_node_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;

  operations =
      FilterEffectBuilder(reference_box, zoom).BuildFilterOperations(filter);
}

CompositorFilterOperations
PaintLayer::CreateCompositorFilterOperationsForBackdropFilter() const {
  const auto& style = GetLayoutObject().StyleRef();
  float zoom = style.EffectiveZoom();
  FloatRect reference_box = FilterReferenceBox(style.BackdropFilter(), zoom);
  return FilterEffectBuilder(reference_box, zoom)
      .BuildFilterOperations(style.BackdropFilter());
}

PaintLayerResourceInfo& PaintLayer::EnsureResourceInfo() {
  PaintLayerRareData& rare_data = EnsureRareData();
  if (!rare_data.resource_info)
    rare_data.resource_info = new PaintLayerResourceInfo(this);
  return *rare_data.resource_info;
}

void PaintLayer::RemoveAncestorOverflowLayer(const PaintLayer* removed_layer) {
  // If the current ancestor overflow layer does not match the removed layer
  // the ancestor overflow layer has changed so we can stop searching.
  if (AncestorOverflowLayer() && AncestorOverflowLayer() != removed_layer)
    return;

  if (AncestorOverflowLayer()) {
    // If the previous AncestorOverflowLayer is the root and this object is a
    // sticky viewport constrained object, it is no longer known to be
    // constrained by the root.
    if (AncestorOverflowLayer()->IsRootLayer() &&
        GetLayoutObject().StyleRef().HasStickyConstrainedPosition()) {
      if (LocalFrameView* frame_view = GetLayoutObject().GetFrameView())
        frame_view->RemoveViewportConstrainedObject(GetLayoutObject());
    }

    if (PaintLayerScrollableArea* ancestor_scrollable_area =
            AncestorOverflowLayer()->GetScrollableArea()) {
      // TODO(pdr): When slimming paint v2 is enabled, we will need to
      // invalidate the scroll paint property subtree for this so main
      // thread scroll reasons are recomputed.
      ancestor_scrollable_area->InvalidateStickyConstraintsFor(this);
    }
  }
  UpdateAncestorOverflowLayer(nullptr);
  PaintLayer* current = first_;
  while (current) {
    current->RemoveAncestorOverflowLayer(removed_layer);
    current = current->NextSibling();
  }
}

FilterEffect* PaintLayer::LastFilterEffect() const {
  // TODO(chrishtr): ensure (and assert) that compositing is clean here.
  if (!PaintsWithFilters())
    return nullptr;
  PaintLayerResourceInfo* resource_info = ResourceInfo();
  DCHECK(resource_info);

  if (resource_info->LastEffect())
    return resource_info->LastEffect();

  const auto& style = GetLayoutObject().StyleRef();
  float zoom = style.EffectiveZoom();
  FilterEffectBuilder builder(FilterReferenceBox(style.Filter(), zoom), zoom);
  resource_info->SetLastEffect(
      builder.BuildFilterEffect(FilterOperationsIncludingReflection()));
  return resource_info->LastEffect();
}

FloatRect PaintLayer::MapRectForFilter(const FloatRect& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;

  // Ensure the filter-chain is refreshed wrt reference filters.
  // TODO(fs): Avoid having this side-effect inducing call.
  LastFilterEffect();

  return FilterOperationsIncludingReflection().MapRect(rect);
}

LayoutRect PaintLayer::MapLayoutRectForFilter(const LayoutRect& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;
  return EnclosingLayoutRect(MapRectForFilter(FloatRect(rect)));
}

bool PaintLayer::HasFilterThatMovesPixels() const {
  if (!HasFilterInducingProperty())
    return false;
  const ComputedStyle& style = GetLayoutObject().StyleRef();
  if (style.HasFilter() && style.Filter().HasFilterThatMovesPixels())
    return true;
  if (style.HasBoxReflect())
    return true;
  return false;
}

void PaintLayer::AddLayerHitTestRects(
    LayerHitTestRects& rects,
    TouchAction supported_fast_actions) const {
  ComputeSelfHitTestRects(rects, supported_fast_actions);
  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->AddLayerHitTestRects(rects, supported_fast_actions);
}

void PaintLayer::ComputeSelfHitTestRects(
    LayerHitTestRects& rects,
    TouchAction supported_fast_actions) const {
  if (!Size().IsEmpty()) {
    Vector<HitTestRect> rect;
    TouchAction whitelisted_touch_action =
        GetLayoutObject().StyleRef().GetEffectiveTouchAction() &
        supported_fast_actions;

    if (GetLayoutBox() && GetLayoutBox()->ScrollsOverflow()) {
      // For scrolling layers, rects are taken to be in the space of the
      // contents.  We need to include the bounding box of the layer in the
      // space of its parent (eg. for border / scroll bars) and if it's
      // composited then the entire contents as well as they may be on another
      // composited layer. Skip reporting contents for non-composited layers as
      // they'll get projected to the same layer as the bounding box.
      if (GetCompositingState() != kNotComposited && scrollable_area_) {
        rect.push_back(HitTestRect(scrollable_area_->OverflowRect(),
                                   whitelisted_touch_action));
      }

      rects.Set(this, rect);
      if (const PaintLayer* parent_layer = Parent()) {
        LayerHitTestRects::iterator iter = rects.find(parent_layer);
        if (iter == rects.end()) {
          rects.insert(parent_layer, Vector<HitTestRect>())
              .stored_value->value.push_back(HitTestRect(
                  PhysicalBoundingBox(parent_layer), whitelisted_touch_action));
        } else {
          iter->value.push_back(HitTestRect(PhysicalBoundingBox(parent_layer),
                                            whitelisted_touch_action));
        }
      }
    } else {
      rect.push_back(
          HitTestRect(LogicalBoundingBox(), whitelisted_touch_action));
      rects.Set(this, rect);
    }
  }
}

void PaintLayer::SetNeedsRepaint() {
  SetNeedsRepaintInternal();

  // Do this unconditionally to ensure container chain is marked when
  // compositing status of the layer changes.
  MarkCompositingContainerChainForNeedsRepaint();
}

void PaintLayer::SetNeedsRepaintInternal() {
  needs_repaint_ = true;
  // Invalidate as a display item client.
  static_cast<DisplayItemClient*>(this)->Invalidate();
}

void PaintLayer::MarkCompositingContainerChainForNeedsRepaint() {
  // Need to access compositingState(). We've ensured correct flag setting when
  // compositingState() changes.
  DisableCompositingQueryAsserts disabler;

  PaintLayer* layer = this;
  while (true) {
    if (layer->GetCompositingState() == kPaintsIntoOwnBacking)
      return;
    if (CompositedLayerMapping* grouped_mapping = layer->GroupedMapping()) {
      // TODO(wkorman): As we clean up the CompositedLayerMapping needsRepaint
      // logic to delegate to scrollbars, we may be able to remove the line
      // below as well.
      grouped_mapping->OwningLayer().SetNeedsRepaint();
      return;
    }

    // For a non-self-painting layer having self-painting descendant, the
    // descendant will be painted through this layer's Parent() instead of
    // this layer's Container(), so in addition to the CompositingContainer()
    // chain, we also need to mark NeedsRepaint for Parent().
    // TODO(crbug.com/828103): clean up this.
    if (layer->Parent() && !layer->IsSelfPaintingLayer())
      layer->Parent()->SetNeedsRepaint();

    PaintLayer* container = layer->CompositingContainer();
    if (!container) {
      auto* owner = layer->GetLayoutObject().GetFrame()->OwnerLayoutObject();
      if (!owner)
        break;
      container = owner->EnclosingLayer();
    }

    if (container->needs_repaint_)
      break;

    container->SetNeedsRepaintInternal();
    layer = container;
  }
}

void PaintLayer::ClearNeedsRepaintRecursively() {
  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->ClearNeedsRepaintRecursively();
  needs_repaint_ = false;
}

DisableCompositingQueryAsserts::DisableCompositingQueryAsserts()
    : disabler_(&g_compositing_query_mode, kCompositingQueriesAreAllowed) {}

}  // namespace blink

#if DCHECK_IS_ON()
void showLayerTree(const blink::PaintLayer* layer) {
  blink::DisableCompositingQueryAsserts disabler;
  if (!layer) {
    LOG(ERROR) << "Cannot showLayerTree. Root is (nil)";
    return;
  }

  if (blink::LocalFrame* frame = layer->GetLayoutObject().GetFrame()) {
    WTF::String output =
        ExternalRepresentation(frame,
                               blink::kLayoutAsTextShowAllLayers |
                                   blink::kLayoutAsTextShowLayerNesting |
                                   blink::kLayoutAsTextShowCompositedLayers |
                                   blink::kLayoutAsTextShowAddresses |
                                   blink::kLayoutAsTextShowIDAndClass |
                                   blink::kLayoutAsTextDontUpdateLayout |
                                   blink::kLayoutAsTextShowLayoutState |
                                   blink::kLayoutAsTextShowPaintProperties,
                               layer);
    LOG(ERROR) << output.Utf8().data();
  }
}

void showLayerTree(const blink::LayoutObject* layoutObject) {
  if (!layoutObject) {
    LOG(ERROR) << "Cannot showLayerTree. Root is (nil)";
    return;
  }
  showLayerTree(layoutObject->EnclosingLayer());
}
#endif
