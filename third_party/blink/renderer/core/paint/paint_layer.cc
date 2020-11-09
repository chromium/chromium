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

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/containers/adapters.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
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
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/box_reflection_utils.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
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
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

static CompositingQueryMode g_compositing_query_mode =
    kCompositingQueriesAreOnlyAllowedInCertainDocumentLifecyclePhases;

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
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
  CullRect previous_cull_rect;
};

ASSERT_SIZE(PaintLayer, SameSizeAsPaintLayer);
#endif

inline PhysicalRect PhysicalVisualOverflowRectAllowingUnset(
    const LayoutBoxModelObject& layout_object) {
#if DCHECK_IS_ON()
  NGInkOverflow::ReadUnsetAsNoneScope read_unset_as_none;
#endif
  return layout_object.PhysicalVisualOverflowRect();
}

}  // namespace

PaintLayerRareData::PaintLayerRareData()
    : enclosing_pagination_layer(nullptr),
      potential_compositing_reasons_from_style(CompositingReason::kNone),
      potential_compositing_reasons_from_non_style(CompositingReason::kNone),
      compositing_reasons(CompositingReason::kNone),
      squashing_disallowed_reasons(SquashingDisallowedReason::kNone),
      grouped_mapping(nullptr) {}

PaintLayerRareData::~PaintLayerRareData() = default;

PaintLayer::PaintLayer(LayoutBoxModelObject& layout_object)
    : is_root_layer_(IsA<LayoutView>(layout_object)),
      has_visible_content_(false),
      needs_descendant_dependent_flags_update_(true),
      needs_visual_overflow_recalc_(true),
      has_visible_descendant_(false),
#if DCHECK_IS_ON()
      // The root layer (LayoutView) does not need position update at start
      // because its Location() is always 0.
      needs_position_update_(!IsRootLayer()),
#endif
      has3d_transformed_descendant_(false),
      needs_ancestor_dependent_compositing_inputs_update_(
          !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()),
      child_needs_compositing_inputs_update_(
          !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()),
      has_compositing_descendant_(false),
      should_isolate_composited_descendants_(false),
      lost_grouped_mapping_(false),
      self_needs_repaint_(false),
      descendant_needs_repaint_(false),
      previous_paint_result_(kFullyPainted),
      needs_paint_phase_descendant_outlines_(false),
      needs_paint_phase_float_(false),
      has_non_isolated_descendant_with_blend_mode_(false),
      has_fixed_position_descendant_(false),
      has_sticky_position_descendant_(false),
      has_non_contained_absolute_position_descendant_(false),
      has_stacked_descendant_in_current_stacking_context_(false),
      self_painting_status_changed_(false),
      filter_on_effect_node_dirty_(false),
      backdrop_filter_on_effect_node_dirty_(false),
      is_under_svg_hidden_container_(false),
      descendant_has_direct_or_scrolling_compositing_reason_(false),
      needs_compositing_reasons_update_(
          !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()),
      descendant_may_need_compositing_requirements_update_(false),
      needs_compositing_layer_assignment_(false),
      descendant_needs_compositing_layer_assignment_(false),
      has_self_painting_layer_descendant_(false),
      needs_reorder_overlay_overflow_controls_(false),
      static_inline_edge_(InlineEdge::kInlineStart),
      static_block_edge_(BlockEdge::kBlockStart),
      needs_paint_offset_translation_for_compositing_(false),
      needs_check_raster_invalidation_(false),
#if DCHECK_IS_ON()
      layer_list_mutation_allowed_(true),
#endif
      layout_object_(&layout_object),
      parent_(nullptr),
      previous_(nullptr),
      next_(nullptr),
      first_(nullptr),
      last_(nullptr),
      static_inline_position_(0),
      static_block_position_(0),
      ancestor_scroll_container_layer_(nullptr)
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
            DynamicTo<ReferenceClipPathOperation>(style.ClipPath()))
      reference_clip->RemoveClient(*rare_data_->resource_info);
    rare_data_->resource_info->ClearLayer();
  }

  if (GroupedMapping()) {
    DisableCompositingQueryAsserts disabler;
    SetGroupedMapping(nullptr, kInvalidateLayerAndRemoveFromMapping);
  }

  // Child layers will be deleted by their corresponding layout objects, so
  // we don't need to delete them ourselves.
  if (HasCompositedLayerMapping())
    ClearCompositedLayerMapping(true);

  // Reset this flag before disposing scrollable_area_ to prevent
  // PaintLayerScrollableArea::WillRemoveScrollbar() from dirtying the z-order
  // list of the stacking context. If this layer is removed from the parent,
  // the z-order list should have been invalidated in RemoveChild().
  needs_reorder_overlay_overflow_controls_ = false;

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

DOMNodeId PaintLayer::OwnerNodeId() const {
  return static_cast<const DisplayItemClient&>(GetLayoutObject()).OwnerNodeId();
}

PaintLayerCompositor* PaintLayer::Compositor() const {
  if (!GetLayoutObject().View())
    return nullptr;
  return GetLayoutObject().View()->Compositor();
}

void PaintLayer::ContentChanged(ContentChangeType change_type) {
  // Content changes in CAP are reflected in changes to what is painted, nothing
  // to do here.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  // updateLayerCompositingState will query compositingReasons for accelerated
  // overflow scrolling.  This is tripped by
  // web_tests/compositing/content-changed-chicken-egg.html
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

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // https://code.google.com/p/chromium/issues/detail?id=343759
    DisableCompositingQueryAsserts disabler;
    return !GetCompositedLayerMapping() ||
           GetCompositingState() != kPaintsIntoOwnBacking;
  } else {
    return true;
  }
}

PhysicalOffset PaintLayer::SubpixelAccumulation() const {
  return rare_data_ ? rare_data_->subpixel_accumulation : PhysicalOffset();
}

void PaintLayer::SetSubpixelAccumulation(const PhysicalOffset& accumulation) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  if (rare_data_ || !accumulation.IsZero())
    EnsureRareData().subpixel_accumulation = accumulation;
}

void PaintLayer::UpdateLayerPositionsAfterLayout() {
  TRACE_EVENT0("blink,benchmark",
               "PaintLayer::updateLayerPositionsAfterLayout");
  RUNTIME_CALL_TIMER_SCOPE(
      V8PerIsolateData::MainThreadIsolate(),
      RuntimeCallStats::CounterId::kUpdateLayerPositionsAfterLayout);

  ClearClipRects();
  UpdateLayerPositionRecursive();

  UpdatePaginationRecursive(EnclosingPaginationLayer());
}

void PaintLayer::UpdateLayerPositionRecursive() {
  auto old_location = location_without_position_offset_;
  auto old_offset_for_in_flow_rel_position = OffsetForInFlowRelPosition();
  UpdateLayerPosition();

  if (location_without_position_offset_ != old_location) {
    SetNeedsCompositingInputsUpdate();
  } else {
    // TODO(chrishtr): compute this invalidation in layout instead of here.
    auto offset_for_in_flow_rel_position =
        rare_data_ ? rare_data_->offset_for_in_flow_rel_position
                   : PhysicalOffset();
    if (offset_for_in_flow_rel_position != old_offset_for_in_flow_rel_position)
      SetNeedsCompositingInputsUpdate();
  }

  // Display-locked elements always have a PaintLayer, meaning that the
  // PaintLayer traversal won't skip locked elements. Thus, we don't have to do
  // an ancestor check, and simply skip iterating children when this element is
  // locked for child layout.
  if (GetLayoutObject().ChildLayoutBlockedByDisplayLock())
    return;

  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->UpdateLayerPositionRecursive();
}

bool PaintLayer::SticksToScroller() const {
  if (!GetLayoutObject().StyleRef().HasStickyConstrainedPosition())
    return false;
  return AncestorScrollContainerLayer()->GetScrollableArea();
}

bool PaintLayer::FixedToViewport() const {
  if (GetLayoutObject().StyleRef().GetPosition() != EPosition::kFixed)
    return false;
  return GetLayoutObject().Container() == GetLayoutObject().View();
}

bool PaintLayer::ScrollsWithRespectTo(const PaintLayer* other) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
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
        box->GetDocument().GetSettings()->GetAcceleratedCompositingEnabled());
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
      EnsureRareData().transform = std::make_unique<TransformationMatrix>();
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
    MarkAncestorChainForFlagsUpdate();
  }

  if (LocalFrameView* frame_view = GetLayoutObject().GetDocument().View())
    frame_view->SetNeedsUpdateGeometries();
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
    PhysicalRect& rect) const {
  PaintLayer* pagination_layer = EnclosingPaginationLayer();
  DCHECK(pagination_layer);
  auto& flow_thread = To<LayoutFlowThread>(pagination_layer->GetLayoutObject());

  // First make the flow thread rectangle relative to the flow thread, not to
  // |layer|.
  PhysicalOffset offset_within_pagination_layer;
  ConvertToLayerCoords(pagination_layer, offset_within_pagination_layer);
  rect.Move(offset_within_pagination_layer);

  // Then make the rectangle visual, relative to the fragmentation context.
  // Split our box up into the actual fragment boxes that layout in the
  // columns/pages and unite those together to get our true bounding box.
  rect = PhysicalRectToBeNoop(
      flow_thread.FragmentsBoundingBox(rect.ToLayoutRect()));

  // Finally, make the visual rectangle relative to |ancestorLayer|.
  if (ancestor_layer->EnclosingPaginationLayer() != pagination_layer) {
    rect.Move(pagination_layer->VisualOffsetFromAncestor(ancestor_layer));
    return;
  }
  // The ancestor layer is inside the same pagination layer as |layer|, so we
  // need to subtract the visual distance from the ancestor layer to the
  // pagination layer.
  rect.Move(-ancestor_layer->VisualOffsetFromAncestor(pagination_layer));
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

  // If this element prevents child painting, then we can skip updating
  // pagination info, since it won't be used anyway.
  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return;

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
    PhysicalOffset& point) {
  PaintLayer* paint_invalidation_layer = paint_invalidation_container.Layer();
  if (!paint_invalidation_layer->GroupedMapping())
    return;

  GraphicsLayer* squashing_layer =
      paint_invalidation_layer->GroupedMapping()->SquashingLayer(
          *paint_invalidation_layer);

  auto source_state =
      paint_invalidation_container.FirstFragment().LocalBorderBoxProperties();
  auto dest_state = squashing_layer->GetPropertyTreeState();

  // Move the point into the source_state transform space, map to dest_state
  // transform space, then move into squashing layer state.
  point += paint_invalidation_container.FirstFragment().PaintOffset();
  point = PhysicalOffset::FromFloatPointRound(
      GeometryMapper::SourceToDestinationProjection(source_state.Transform(),
                                                    dest_state.Transform())
          .MapPoint(FloatPoint(point)));
  point -= PhysicalOffset(squashing_layer->GetOffsetFromTransformNode());
}

void PaintLayer::DirtyVisibleContentStatus() {
  MarkAncestorChainForFlagsUpdate();
  // Non-self-painting layers paint into their ancestor layer, and count as part
  // of the "visible contents" of the parent, so we need to dirty it.
  if (!IsSelfPaintingLayer())
    Parent()->DirtyVisibleContentStatus();
}

void PaintLayer::MarkAncestorChainForFlagsUpdate(
    DescendantDependentFlagsUpdateFlag flag) {
#if DCHECK_IS_ON()
  DCHECK(flag == DoesNotNeedDescendantDependentUpdate ||
         !layout_object_->GetDocument()
              .View()
              ->IsUpdatingDescendantDependentFlags());
#endif
  for (PaintLayer* layer = this; layer; layer = layer->Parent()) {
    if (layer->needs_descendant_dependent_flags_update_ &&
        layer->GetLayoutObject().NeedsPaintPropertyUpdate())
      break;
    if (flag == NeedsDescendantDependentUpdate)
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
    has_fixed_position_descendant_ = false;
    has_sticky_position_descendant_ = false;
    has_non_contained_absolute_position_descendant_ = false;
    has_stacked_descendant_in_current_stacking_context_ = false;
    has_self_painting_layer_descendant_ = false;

    bool can_contain_abs =
        GetLayoutObject().CanContainAbsolutePositionObjects();

    auto* first_child = [this]() -> PaintLayer* {
      if (GetLayoutObject().ChildPrePaintBlockedByDisplayLock()) {
        GetLayoutObject()
            .GetDisplayLockContext()
            ->NotifyCompositingDescendantDependentFlagUpdateWasBlocked();
        return nullptr;
      }
      return FirstChild();
    }();

    for (PaintLayer* child = first_child; child; child = child->NextSibling()) {
      const ComputedStyle& child_style = child->GetLayoutObject().StyleRef();

      child->UpdateDescendantDependentFlags();

      if (child->has_visible_content_ || child->has_visible_descendant_)
        has_visible_descendant_ = true;

      has_non_isolated_descendant_with_blend_mode_ |=
          (!child->GetLayoutObject().IsStackingContext() &&
           child->HasNonIsolatedDescendantWithBlendMode()) ||
          child_style.HasBlendMode();

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

      if (!has_stacked_descendant_in_current_stacking_context_) {
        if (child->GetLayoutObject().IsStacked()) {
          has_stacked_descendant_in_current_stacking_context_ = true;
        } else if (!child->GetLayoutObject().IsStackingContext()) {
          has_stacked_descendant_in_current_stacking_context_ =
              child->has_stacked_descendant_in_current_stacking_context_;
        }
      }

      has_self_painting_layer_descendant_ =
          has_self_painting_layer_descendant_ ||
          child->HasSelfPaintingLayerDescendant() ||
          child->IsSelfPaintingLayer();
    }

    UpdateStackingNode();

    if (old_has_non_isolated_descendant_with_blend_mode !=
        static_cast<bool>(has_non_isolated_descendant_with_blend_mode_)) {
      // The LayoutView DisplayItemClient owns painting of the background
      // of the HTML element. When blending isolation of the HTML element's
      // descendants change, there will be an addition or removal of an
      // isolation effect node for the HTML element to add (or remove)
      // isolated blending, and that case we need to re-paint the LayoutView.
      if (Parent() && Parent()->IsRootLayer())
        GetLayoutObject().View()->SetBackgroundNeedsFullPaintInvalidation();
      GetLayoutObject().SetNeedsPaintPropertyUpdate();
    }
    needs_descendant_dependent_flags_update_ = false;

    if (IsSelfPaintingLayer() && needs_visual_overflow_recalc_) {
      PhysicalRect old_visual_rect =
          PhysicalVisualOverflowRectAllowingUnset(GetLayoutObject());
      GetLayoutObject().RecalcVisualOverflow();
      if (old_visual_rect != GetLayoutObject().PhysicalVisualOverflowRect()) {
        SetNeedsCompositingInputsUpdateInternal();
        MarkAncestorChainForFlagsUpdate(DoesNotNeedDescendantDependentUpdate);
      }
    }
    needs_visual_overflow_recalc_ = false;
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
    layout_object_->SetShouldCheckForPaintInvalidation();
  }

  Update3DTransformedDescendantStatus();
}

void PaintLayer::Update3DTransformedDescendantStatus() {
  has3d_transformed_descendant_ = false;

  // Transformed or preserve-3d descendants can only be in the z-order lists,
  // not in the normal flow list, so we only need to check those.
  PaintLayerPaintOrderIterator iterator(*this, kStackedChildren);
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
  if (GetLayoutObject().IsLayoutInline())
    UpdateSize();
  PhysicalOffset local_point;
  if (LayoutBox* box = GetLayoutBox()) {
    local_point += box->PhysicalLocation();
  }

  if (!GetLayoutObject().IsOutOfFlowPositioned() &&
      !GetLayoutObject().IsColumnSpanAll()) {
    // We must adjust our position by walking up the layout tree looking for the
    // nearest enclosing object with a layer.
    LayoutObject* curr = GetLayoutObject().Container();
    while (curr && !curr->HasLayer()) {
      if (curr->IsBox() && !curr->IsLegacyTableRow()) {
        // Rows and cells share the same coordinate space (that of the section).
        // Omit them when computing our xpos/ypos.
        local_point += To<LayoutBox>(curr)->PhysicalLocation();
      }
      curr = curr->Container();
    }
    if (curr && curr->IsLegacyTableRow()) {
      // Put ourselves into the row coordinate space.
      local_point -= To<LayoutBox>(curr)->PhysicalLocation();
    }
  }

  if (PaintLayer* containing_layer = ContainingLayer()) {
    auto& container = containing_layer->GetLayoutObject();
    if (GetLayoutObject().IsOutOfFlowPositioned() &&
        container.IsLayoutInline() &&
        container.CanContainOutOfFlowPositionedElement(
            GetLayoutObject().StyleRef().GetPosition())) {
      // Adjust offset for absolute under in-flow positioned inline.
      PhysicalOffset offset =
          ToLayoutInline(container).OffsetForInFlowPositionedInline(
              To<LayoutBox>(GetLayoutObject()));
      local_point += offset;
    }
  }

  if (GetLayoutObject().IsInFlowPositioned() &&
      GetLayoutObject().IsRelPositioned()) {
    auto new_offset = GetLayoutObject().OffsetForInFlowPosition();
    if (rare_data_ || !new_offset.IsZero())
      EnsureRareData().offset_for_in_flow_rel_position = new_offset;
  } else if (rare_data_) {
    rare_data_->offset_for_in_flow_rel_position = PhysicalOffset();
  }
  location_without_position_offset_ = local_point;

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
    IntRect line_box = EnclosingIntRect(inline_flow.PhysicalLinesBoundingBox());
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
      return To<LayoutBoxModelObject>(container)->Layer();
    object = container;
  }
  return nullptr;
}

PhysicalOffset PaintLayer::ComputeOffsetFromAncestor(
    const PaintLayer& ancestor_layer) const {
  const LayoutBoxModelObject& ancestor_object =
      ancestor_layer.GetLayoutObject();
  PhysicalOffset result = GetLayoutObject().LocalToAncestorPoint(
      PhysicalOffset(), &ancestor_object, kIgnoreTransforms);
  if (ancestor_object.UsesCompositedScrolling()) {
    result += PhysicalOffset(
        To<LayoutBox>(ancestor_object).PixelSnappedScrolledContentOffset());
  }
  return result;
}

PaintLayer* PaintLayer::CompositingContainer() const {
  if (IsReplacedNormalFlowStacking())
    return Parent();
  if (!GetLayoutObject().IsStacked()) {
    if (IsSelfPaintingLayer() || GetLayoutObject().IsColumnSpanAll())
      return Parent();
    return ContainingLayer();
  }
  return AncestorStackingContext();
}

PaintLayer* PaintLayer::AncestorStackingContext() const {
  for (PaintLayer* ancestor = Parent(); ancestor;
       ancestor = ancestor->Parent()) {
    if (ancestor->GetLayoutObject().IsStackingContext())
      return ancestor;
  }
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

bool PaintLayer::CanBeCompositedForDirectReasons() const {
  return DirectCompositingReasons() && IsSelfPaintingLayer();
}

PaintLayer*
PaintLayer::EnclosingDirectlyCompositableLayerCrossingFrameBoundaries() const {
  const PaintLayer* layer = this;
  PaintLayer* composited_layer = nullptr;
  while (!composited_layer) {
    composited_layer = layer->EnclosingDirectlyCompositableLayer(kIncludeSelf);
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

PaintLayer* PaintLayer::EnclosingDirectlyCompositableLayer(
    IncludeSelfOrNot include_self_or_not) const {
  DCHECK(IsAllowedToQueryCompositingInputs());
  if (include_self_or_not == kIncludeSelf && CanBeCompositedForDirectReasons())
    return const_cast<PaintLayer*>(this);

  for (PaintLayer* curr = CompositingContainer(); curr;
       curr = curr->CompositingContainer()) {
    if (curr->CanBeCompositedForDirectReasons())
      return curr;
  }

  return nullptr;
}

void PaintLayer::SetNeedsCompositingInputsUpdate(bool mark_ancestor_flags) {
  SetNeedsCompositingInputsUpdateInternal();

  // TODO(chrishtr): These are a bit of a heavy hammer, because not all
  // things which require compositing inputs update require a descendant-
  // dependent flags update. Reduce call sites after CAP launch allows
  /// removal of CompositingInputsUpdater.
  if (mark_ancestor_flags)
    MarkAncestorChainForFlagsUpdate(NeedsDescendantDependentUpdate);
}

void PaintLayer::SetNeedsGraphicsLayerRebuild() {
  if (Compositor())
    Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
}

void PaintLayer::SetNeedsCheckRasterInvalidation() {
  DCHECK_EQ(GetLayoutObject().GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  needs_check_raster_invalidation_ = true;
  // We need to mark |this| as needing layer assignment also, because
  // CompositingLayerAssigner is where we transfer the raster invalidation
  // checking bit from PaintLayer to GraphicsLayer.
  SetNeedsCompositingLayerAssignment();
}

void PaintLayer::SetNeedsVisualOverflowRecalc() {
  DCHECK(IsSelfPaintingLayer());
  needs_visual_overflow_recalc_ = true;
  MarkAncestorChainForFlagsUpdate();
}

void PaintLayer::SetChildNeedsCompositingInputsUpdateUpToAncestor(
    PaintLayer* ancestor) {
  DCHECK(ancestor);

  for (auto* layer = this; layer && layer != ancestor; layer = layer->Parent())
    layer->child_needs_compositing_inputs_update_ = true;

  ancestor->child_needs_compositing_inputs_update_ = true;
}

const IntRect PaintLayer::ClippedAbsoluteBoundingBox() const {
  if (RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    PhysicalRect mapping_rect = BoundingBoxForCompositingOverlapTest();
    GetLayoutObject().MapToVisualRectInAncestorSpace(
        GetLayoutObject().View(), mapping_rect, kUseGeometryMapper);
    return EnclosingIntRect(mapping_rect);
  } else {
    return GetAncestorDependentCompositingInputs()
        .clipped_absolute_bounding_box;
  }
}
const IntRect PaintLayer::UnclippedAbsoluteBoundingBox() const {
  if (RuntimeEnabledFeatures::CompositingOptimizationsEnabled()) {
    return EnclosingIntRect(GetLayoutObject().LocalToAbsoluteRect(
        BoundingBoxForCompositingOverlapTest(),
        kUseGeometryMapperMode | kIgnoreScrollOffset));
  } else {
    return GetAncestorDependentCompositingInputs()
        .unclipped_absolute_bounding_box;
  }
}

void PaintLayer::SetNeedsCompositingInputsUpdateInternal() {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  needs_ancestor_dependent_compositing_inputs_update_ = true;

  // We might call this function on a locked element. Now, locked elements might
  // have a persistent dirty child bit, meaning that the below loop won't mark
  // the breakcrumb bit further up the chain (since this element appears to
  // already have a breadcrumb). However, since the element itself needs an
  // ancestor dependent update, we need to force the propagation at least one
  // level to the parent. This ensures that the real dirty bit
  // (|needs_ancestor_dependent_compositing_inputs_update_|) can be discovered
  // by the compositing update walk.
  bool child_flag_may_persist_after_update =
      GetLayoutObject().ChildPrePaintBlockedByDisplayLock();

  PaintLayer* initial_layer = child_needs_compositing_inputs_update_ &&
                                      child_flag_may_persist_after_update
                                  ? Parent()
                                  : this;

  PaintLayer* last_ancestor = nullptr;
  for (PaintLayer* current = initial_layer;
       current && !current->child_needs_compositing_inputs_update_;
       current = current->Parent()) {
    last_ancestor = current;
    current->child_needs_compositing_inputs_update_ = true;
    if (Compositor() &&
        (current != initial_layer ||
         !current->GetLayoutObject().IsStickyPositioned()) &&
        current->GetLayoutObject().ShouldApplyStrictContainment())
      break;
  }

  if (Compositor()) {
    Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateAfterCompositingInputChange);

    if (last_ancestor)
      Compositor()->UpdateCompositingInputsRoot(last_ancestor);
  }
}

void PaintLayer::UpdateAncestorDependentCompositingInputs(
    const AncestorDependentCompositingInputs& compositing_inputs) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  EnsureAncestorDependentCompositingInputs() = compositing_inputs;
  needs_ancestor_dependent_compositing_inputs_update_ = false;
}

void PaintLayer::ClearChildNeedsCompositingInputsUpdate() {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(!NeedsCompositingInputsUpdate());
  child_needs_compositing_inputs_update_ = false;
}

bool PaintLayer::HasNonIsolatedDescendantWithBlendMode() const {
  DCHECK(!needs_descendant_dependent_flags_update_);
  if (has_non_isolated_descendant_with_blend_mode_)
    return true;
  if (GetLayoutObject().IsSVGRoot()) {
    return To<LayoutSVGRoot>(GetLayoutObject())
        .HasNonIsolatedBlendingDescendants();
  }
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

void* PaintLayer::operator new(size_t sz) {
  return WTF::Partitions::LayoutPartition()->Alloc(
      sz, WTF_HEAP_PROFILER_TYPE_NAME(PaintLayer));
}

void PaintLayer::operator delete(void* ptr) {
  WTF::Partitions::LayoutPartition()->Free(ptr);
}

void PaintLayer::AddChild(PaintLayer* child, PaintLayer* before_child) {
#if DCHECK_IS_ON()
  DCHECK(layer_list_mutation_allowed_);
#endif

  PaintLayer* prev_sibling =
      before_child ? before_child->PreviousSibling() : LastChild();
  if (prev_sibling) {
    child->SetPreviousSibling(prev_sibling);
    prev_sibling->SetNextSibling(child);
    DCHECK_NE(prev_sibling, child);
  } else {
    SetFirstChild(child);
  }

  if (before_child) {
    before_child->SetPreviousSibling(child);
    child->SetNextSibling(before_child);
    DCHECK_NE(before_child, child);
  } else {
    SetLastChild(child);
  }

  child->parent_ = this;

  // The ancestor scroll container layer is calculated during compositing inputs
  // update and should not be set yet.
  CHECK(!child->AncestorScrollContainerLayer());

  SetNeedsCompositingInputsUpdate();

  if (Compositor()) {
    if (!child->GetLayoutObject().IsStacked() &&
        !GetLayoutObject().DocumentBeingDestroyed())
      Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  }

  if (child->GetLayoutObject().IsStacked() || child->FirstChild()) {
    // Dirty the z-order list in which we are contained. The
    // ancestorStackingContextNode() can be null in the case where we're
    // building up generated content layers. This is ok, since the lists will
    // start off dirty in that case anyway.
    child->DirtyStackingContextZOrderLists();
  }

  // Non-self-painting children paint into this layer, so the visible contents
  // status of this layer is affected.
  if (!child->IsSelfPaintingLayer())
    DirtyVisibleContentStatus();

  MarkAncestorChainForFlagsUpdate();

  // Need to force requirements update, due to change of stacking order.
  SetNeedsCompositingRequirementsUpdate();

  child->SetNeedsRepaint();
}

void PaintLayer::RemoveChild(PaintLayer* old_child) {
#if DCHECK_IS_ON()
  DCHECK(layer_list_mutation_allowed_);
#endif

  old_child->MarkCompositingContainerChainForNeedsRepaint();

  if (old_child->PreviousSibling())
    old_child->PreviousSibling()->SetNextSibling(old_child->NextSibling());
  if (old_child->NextSibling())
    old_child->NextSibling()->SetPreviousSibling(old_child->PreviousSibling());

  if (first_ == old_child)
    first_ = old_child->NextSibling();
  if (last_ == old_child)
    last_ = old_child->PreviousSibling();

  if (!GetLayoutObject().DocumentBeingDestroyed()) {
    if (Compositor()) {
      if (!old_child->GetLayoutObject().IsStacked())
        Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);

      if (Compositor()->GetCompositingInputsRoot() == old_child)
        Compositor()->ClearCompositingInputsRoot();
    }
    // Dirty the z-order list in which we are contained.
    old_child->DirtyStackingContextZOrderLists();
    SetNeedsCompositingInputsUpdate();
  }

  if (GetLayoutObject().StyleRef().Visibility() != EVisibility::kVisible)
    DirtyVisibleContentStatus();

  old_child->SetPreviousSibling(nullptr);
  old_child->SetNextSibling(nullptr);
  old_child->parent_ = nullptr;

  // Remove any ancestor scroll container layers which descended into the
  // removed child.
  if (old_child->AncestorScrollContainerLayer()) {
    old_child->RemoveAncestorScrollContainerLayer(
        old_child->AncestorScrollContainerLayer());
  }

  if (old_child->has_visible_content_ || old_child->has_visible_descendant_)
    MarkAncestorChainForFlagsUpdate();

  if (old_child->EnclosingPaginationLayer())
    old_child->ClearPaginationRecursive();
}

void PaintLayer::ClearClipRects(ClipRectsCacheSlot cache_slot) {
  Clipper(GeometryMapperOption::kDoNotUseGeometryMapper)
      .ClearClipRectsIncludingDescendants(cache_slot);
}

void PaintLayer::RemoveOnlyThisLayerAfterStyleChange(
    const ComputedStyle* old_style) {
  if (!parent_)
    return;

  if (old_style && GetLayoutObject().IsStacked(*old_style))
    DirtyStackingContextZOrderLists();

  bool did_set_paint_invalidation = false;
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // Destructing PaintLayer would cause CompositedLayerMapping and composited
    // layers to be destructed and detach from layer tree immediately. Layers
    // could have dangling scroll/clip parent if compositing update were
    // omitted.
    if (LocalFrameView* frame_view = layout_object_->GetDocument().View())
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
  layout_object_->DestroyLayer();
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

  if (IsSelfPaintingLayer() && parent_) {
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
    PhysicalOffset& location) {
  DCHECK(ancestor_layer != layer);

  const LayoutBoxModelObject& layout_object = layer->GetLayoutObject();

  if (layout_object.IsFixedPositioned() &&
      (!ancestor_layer || ancestor_layer == layout_object.View()->Layer())) {
    // If the fixed layer's container is the root, just add in the offset of the
    // view. We can obtain this by calling localToAbsolute() on the LayoutView.
    location +=
        layout_object.LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms);
    return ancestor_layer;
  }

  bool found_ancestor_first = false;
  PaintLayer* containing_layer =
      ancestor_layer
          ? layer->ContainingLayer(ancestor_layer, &found_ancestor_first)
          : layer->ContainingLayer(ancestor_layer, nullptr);

  if (found_ancestor_first) {
    // Found ancestorLayer before the containing layer, so compute offset of
    // both relative to the container and subtract.
    PhysicalOffset this_coords;
    layer->ConvertToLayerCoords(containing_layer, this_coords);

    PhysicalOffset ancestor_coords;
    ancestor_layer->ConvertToLayerCoords(containing_layer, ancestor_coords);

    location += (this_coords - ancestor_coords);
    return ancestor_layer;
  }

  if (!containing_layer)
    return nullptr;

  location += layer->LocationWithoutPositionOffset();
  if (layer->GetLayoutObject().IsRelPositioned()) {
    location += layer->OffsetForInFlowRelPosition();
  } else if (layer->GetLayoutObject().IsInFlowPositioned()) {
    location += layer->GetLayoutObject().OffsetForInFlowPosition();
  }
  location -=
      PhysicalOffset(containing_layer->PixelSnappedScrolledContentOffset());

  return containing_layer;
}

void PaintLayer::ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                                      PhysicalOffset& location) const {
  if (ancestor_layer == this)
    return;

  const PaintLayer* curr_layer = this;
  while (curr_layer && curr_layer != ancestor_layer)
    curr_layer =
        AccumulateOffsetTowardsAncestor(curr_layer, ancestor_layer, location);
}

void PaintLayer::ConvertToLayerCoords(const PaintLayer* ancestor_layer,
                                      PhysicalRect& rect) const {
  PhysicalOffset delta;
  ConvertToLayerCoords(ancestor_layer, delta);
  rect.Move(delta);
}

PhysicalOffset PaintLayer::VisualOffsetFromAncestor(
    const PaintLayer* ancestor_layer,
    PhysicalOffset offset) const {
  if (ancestor_layer == this)
    return offset;
  PaintLayer* pagination_layer = EnclosingPaginationLayer();
  if (pagination_layer == this)
    pagination_layer = Parent()->EnclosingPaginationLayer();
  if (!pagination_layer) {
    ConvertToLayerCoords(ancestor_layer, offset);
    return offset;
  }

  auto& flow_thread = To<LayoutFlowThread>(pagination_layer->GetLayoutObject());
  ConvertToLayerCoords(pagination_layer, offset);
  offset = PhysicalOffsetToBeNoop(
      flow_thread.FlowThreadPointToVisualPoint(offset.ToLayoutPoint()));
  if (ancestor_layer == pagination_layer)
    return offset;

  if (ancestor_layer->EnclosingPaginationLayer() != pagination_layer) {
    offset += pagination_layer->VisualOffsetFromAncestor(ancestor_layer);
  } else {
    // The ancestor layer is also inside the pagination layer, so we need to
    // subtract the visual distance from the ancestor layer to the pagination
    // layer.
    offset -= ancestor_layer->VisualOffsetFromAncestor(pagination_layer);
  }
  return offset;
}

void PaintLayer::DidUpdateScrollsOverflow() {
  UpdateSelfPaintingLayer();
}

void PaintLayer::UpdateStackingNode() {
#if DCHECK_IS_ON()
  DCHECK(layer_list_mutation_allowed_);
#endif

  bool needs_stacking_node =
      has_stacked_descendant_in_current_stacking_context_ &&
      GetLayoutObject().IsStackingContext();

  if (needs_stacking_node != !!stacking_node_) {
    if (needs_stacking_node)
      stacking_node_ = std::make_unique<PaintLayerStackingNode>(*this);
    else
      stacking_node_ = nullptr;
  }

  if (stacking_node_)
    stacking_node_->UpdateZOrderLists();
}

bool PaintLayer::RequiresScrollableArea() const {
  if (!GetLayoutBox())
    return false;
  if (GetLayoutObject().IsScrollContainer())
    return true;
  // Iframes with the resize property can be resized. This requires
  // scroll corner painting, which is implemented, in part, by
  // PaintLayerScrollableArea.
  if (GetLayoutBox()->CanResize())
    return true;
  // When scrollbar-gutter is "force" we need a PaintLayerScrollableArea
  // in order to calculate the size of scrollbar gutters.
  if (GetLayoutObject().StyleRef().IsScrollbarGutterForce())
    return true;
  return false;
}

void PaintLayer::UpdateScrollableArea() {
  if (RequiresScrollableArea() && !scrollable_area_) {
    scrollable_area_ = MakeGarbageCollected<PaintLayerScrollableArea>(*this);
    if (Compositor()) {
      Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    }
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
  } else if (!RequiresScrollableArea() && scrollable_area_) {
    scrollable_area_->Dispose();
    scrollable_area_.Clear();
    if (Compositor()) {
      Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    }
    GetLayoutObject().SetBackgroundPaintLocation(
        kBackgroundPaintInGraphicsLayer);
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
  }
}

bool PaintLayer::HasOverflowControls() const {
  return scrollable_area_ && (scrollable_area_->HasScrollbar() ||
                              scrollable_area_->ScrollCorner() ||
                              GetLayoutObject().StyleRef().HasResize());
}

void PaintLayer::AppendSingleFragmentIgnoringPagination(
    PaintLayerFragments& fragments,
    const PaintLayer* root_layer,
    const CullRect* cull_rect,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const PhysicalOffset* offset_from_root,
    const PhysicalOffset& sub_pixel_accumulation) const {
  PaintLayerFragment fragment;
  ClipRectsContext clip_rects_context(
      root_layer, &root_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects, overlay_scrollbar_clip_behavior,
      respect_overflow_clip, sub_pixel_accumulation);
  Clipper(GeometryMapperOption::kUseGeometryMapper)
      .CalculateRects(clip_rects_context, &GetLayoutObject().FirstFragment(),
                      cull_rect, fragment.layer_bounds,
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
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return true;
  if (Transform() &&
      !PaintsWithDirectReasonIntoOwnBacking(kGlobalPaintNormalPhase))
    return true;
  if (!compositing_layer) {
    compositing_layer =
        EnclosingDirectlyCompositableLayerCrossingFrameBoundaries();
  }
  if (!compositing_layer)
    return true;
  // Composited layers may not be fragmented.
  return !compositing_layer->EnclosingPaginationLayer();
}

void PaintLayer::CollectFragments(
    PaintLayerFragments& fragments,
    const PaintLayer* root_layer,
    const CullRect* cull_rect,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const PhysicalOffset* offset_from_root,
    const PhysicalOffset& sub_pixel_accumulation) const {
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
  wtf_size_t physical_fragment_idx = 0u;
  for (auto* fragment_data = &first_fragment_data; fragment_data;
       fragment_data = fragment_data->NextFragment(), physical_fragment_idx++) {
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

    base::Optional<CullRect> fragment_cull_rect;
    if (cull_rect) {
      // |cull_rect| is in the coordinate space of |root_layer| (i.e. the
      // space of |root_layer|'s first fragment). Map the rect to the space of
      // the current root fragment.
      auto rect = cull_rect->Rect();
      first_root_fragment_data.MapRectToFragment(*root_fragment_data, rect);
      fragment_cull_rect.emplace(rect);
    }

    Clipper(GeometryMapperOption::kUseGeometryMapper)
        .CalculateRects(
            clip_rects_context, fragment_data,
            fragment_cull_rect ? &*fragment_cull_rect : nullptr,
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

    if (GetLayoutObject().CanTraversePhysicalFragments()) {
      if (const auto* layout_box = GetLayoutBox()) {
        fragment.physical_fragment =
            layout_box->GetPhysicalFragment(physical_fragment_idx);
        DCHECK(fragment.physical_fragment);
      }
    }

    fragments.push_back(fragment);
  }
}

PaintLayer::HitTestRecursionData::HitTestRecursionData(
    const PhysicalRect& rect_arg,
    const HitTestLocation& location_arg,
    const HitTestLocation& original_location_arg)
    : rect(rect_arg),
      location(location_arg),
      original_location(original_location_arg),
      intersects_location(location_arg.Intersects(rect_arg)) {}

bool PaintLayer::HitTest(const HitTestLocation& hit_test_location,
                         HitTestResult& result,
                         const PhysicalRect& hit_test_area) {
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
      GetLayoutObject().UpdateHitTestResult(result, hit_test_location.Point());
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
  auto* element = DynamicTo<Element>(GetLayoutObject().GetNode());
  return element && element->IsInTopLayer();
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
    const PhysicalOffset& translation_offset) const {
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

  PhysicalOffset offset;
  if (container_transform_state)
    ConvertToLayerCoords(container_layer, offset);
  else
    ConvertToLayerCoords(root_layer, offset);

  offset += translation_offset;

  LayoutObject* container_layout_object =
      container_layer ? &container_layer->GetLayoutObject() : nullptr;
  if (GetLayoutObject().ShouldUseTransformFromContainer(
          container_layout_object)) {
    TransformationMatrix container_transform;
    GetLayoutObject().GetTransformFromContainer(container_layout_object, offset,
                                                container_transform);
    transform_state.ApplyTransform(
        container_transform, HitTestingTransformState::kAccumulateTransform);
  } else {
    transform_state.Translate(offset.left.ToInt(), offset.top.ToInt(),
                              HitTestingTransformState::kAccumulateTransform);
  }

  return transform_state;
}

static bool IsHitCandidateForDepthOrder(
    const PaintLayer* hit_layer,
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

// Calling IsDescendantOf is sad (slow), but it's the only way to tell
// whether a hit test candidate is a descendant of the stop node.
static bool IsHitCandidateForStopNode(const LayoutObject& candidate,
                                      const LayoutObject* stop_node) {
  return !stop_node || (&candidate == stop_node) ||
         !candidate.IsDescendantOf(stop_node);
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
                                     double* z_offset,
                                     bool check_resizer_only) {
  const LayoutObject& layout_object = GetLayoutObject();
  DCHECK_GE(layout_object.GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

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

  // For the global root scroller, hit test the layout viewport scrollbars
  // first, as they are visually presented on top of the content.
  if (layout_object.IsGlobalRootScroller()) {
    // There are a number of early outs below that don't apply to the the
    // global root scroller.
    DCHECK(!Transform());
    DCHECK(!Preserves3D());
    DCHECK(!layout_object.HasClipPath());
    if (scrollable_area_) {
      IntPoint point = scrollable_area_->ConvertFromRootFrameToVisualViewport(
          RoundedIntPoint(recursion_data.location.Point()));

      DCHECK(GetLayoutBox());
      if (GetLayoutBox()->HitTestOverflowControl(result, HitTestLocation(point),
                                                 PhysicalOffset()))
        return this;
    }
  }

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
          z_offset, check_resizer_only, clip_behavior);
    }

    // Make sure the parent's clip rects have been calculated.
    if (Parent()) {
      ClipRect clip_rect;
      Clipper(GeometryMapperOption::kUseGeometryMapper)
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
                                           z_offset, check_resizer_only);
  }

  // Don't hit test the clip-path area when checking for occlusion. This is
  // necessary because SVG doesn't support rect-based hit testing, so
  // HitTestClippedOutByClipPath may erroneously return true for a rect-based
  // hit test).
  bool is_occlusion_test = result.GetHitTestRequest().GetType() &
                           HitTestRequest::kHitTestVisualOverflow;
  if (!is_occlusion_test && layout_object.HasClipPath() &&
      HitTestClippedOutByClipPath(root_layer, recursion_data.location)) {
    return nullptr;
  }

  // The natural thing would be to keep HitTestingTransformState on the stack,
  // but it's big, so we heap-allocate.
  HitTestingTransformState* local_transform_state = nullptr;
  STACK_UNINITIALIZED base::Optional<HitTestingTransformState> storage;

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
    STACK_UNINITIALIZED TransformationMatrix inverted_matrix =
        local_transform_state->accumulated_transform_.Inverse();
    // If the z-vector of the matrix is negative, the back is facing towards the
    // viewer.
    if (inverted_matrix.M33() < 0)
      return nullptr;
  }

  HitTestingTransformState* unflattened_transform_state = local_transform_state;
  STACK_UNINITIALIZED base::Optional<HitTestingTransformState>
      unflattened_storage;
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

  // Collect the fragments. This will compute the clip rectangles for each
  // layer fragment.
  STACK_UNINITIALIZED base::Optional<PaintLayerFragments> layer_fragments;
  if (recursion_data.intersects_location) {
    layer_fragments.emplace();
    if (applied_transform) {
      DCHECK(root_layer == this);
      PhysicalOffset ignored;
      AppendSingleFragmentIgnoringPagination(
          *layer_fragments, root_layer, nullptr,
          kExcludeOverlayScrollbarSizeForHitTesting, clip_behavior, &ignored);
    } else {
      CollectFragments(*layer_fragments, root_layer, nullptr,
                       kExcludeOverlayScrollbarSizeForHitTesting,
                       clip_behavior);
    }

    // See if the hit test pos is inside the resizer of current layer. This
    // should be done before walking child layers to avoid that the resizer
    // clickable area is obscured by the positive child layers.
    if (scrollable_area_ && scrollable_area_->HitTestResizerInFragments(
                                *layer_fragments, recursion_data.location)) {
      if (Node* node_for_resizer = layout_object.NodeForHitTest())
        result.SetInnerNode(node_for_resizer);
      return this;
    }
  }

  if (check_resizer_only)
    return nullptr;

  // See if the hit test pos is inside the resizer of the child layers which
  // has reordered the painting of the overlay overflow controls.
  if (stacking_node_) {
    for (auto* layer : base::Reversed(
             stacking_node_->OverlayOverflowControlsReorderedList())) {
      if (layer->HitTestLayer(
              root_layer, nullptr, result, recursion_data,
              false /*applied_transform*/, local_transform_state,
              z_offset_for_descendants_ptr, true /*check_resizer_only*/)) {
        return layer;
      }
    }
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

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  PhysicalOffset offset;
  if (recursion_data.intersects_location) {
    // Next we want to see if the mouse pos is inside the child LayoutObjects of
    // the layer. Check every fragment in reverse order.
    if (IsSelfPaintingLayer() &&
        !layout_object.ChildPaintBlockedByDisplayLock()) {
      // Hit test with a temporary HitTestResult, because we only want to commit
      // to 'result' if we know we're frontmost.
      STACK_UNINITIALIZED HitTestResult temp_result(
          result.GetHitTestRequest(), recursion_data.original_location);
      temp_result.SetInertNode(result.InertNode());
      bool inside_fragment_foreground_rect = false;

      if (HitTestContentsForFragments(
              *layer_fragments, offset, temp_result, recursion_data.location,
              kHitTestDescendants, inside_fragment_foreground_rect) &&
          IsHitCandidateForDepthOrder(this, false, z_offset_for_contents_ptr,
                                      unflattened_transform_state) &&
          IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
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
                 result.GetHitTestRequest().ListBased() &&
                 IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
        result.Append(temp_result);
      } else if (result.GetHitTestRequest().RetargetForInert() &&
                 IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
        result.SetInertNode(temp_result.InertNode());
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
    STACK_UNINITIALIZED HitTestResult temp_result(
        result.GetHitTestRequest(), recursion_data.original_location);
    temp_result.SetInertNode(result.InertNode());
    bool inside_fragment_background_rect = false;
    if (HitTestContentsForFragments(*layer_fragments, offset, temp_result,
                                    recursion_data.location, kHitTestSelf,
                                    inside_fragment_background_rect) &&
        IsHitCandidateForDepthOrder(this, false, z_offset_for_contents_ptr,
                                    unflattened_transform_state) &&
        IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
      if (result.GetHitTestRequest().ListBased())
        result.Append(temp_result);
      else
        result = temp_result;
      return this;
    } else if (result.GetHitTestRequest().RetargetForInert() &&
               IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
      result.SetInertNode(temp_result.InertNode());
    }
    if (inside_fragment_background_rect &&
        result.GetHitTestRequest().ListBased() &&
        IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
      result.Append(temp_result);
    }
  }

  return nullptr;
}

bool PaintLayer::HitTestContentsForFragments(
    const PaintLayerFragments& layer_fragments,
    const PhysicalOffset& offset,
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
    PhysicalOffset fragment_offset = offset;
    fragment_offset += fragment.layer_bounds.offset;

    if (UNLIKELY(layer_fragments.size() > 1 &&
                 GetLayoutObject().IsLayoutInline() &&
                 GetLayoutObject().CanTraversePhysicalFragments())) {
      // When hit-testing a relatively positioned inline, we'll search for it in
      // each fragment of the containing block. Each fragment has its own
      // offset, and we need to do one fragment at a time.
      HitTestLocation location_for_fragment(hit_test_location, i);
      if (HitTestContents(result, fragment.physical_fragment, fragment_offset,
                          location_for_fragment, hit_test_filter))
        return true;
    } else if (HitTestContents(result, fragment.physical_fragment,
                               fragment_offset, hit_test_location,
                               hit_test_filter)) {
      return true;
    }
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
    bool check_resizer_only,
    ShouldRespectOverflowClipType clip_behavior) {
  PaintLayerFragments enclosing_pagination_fragments;
  // FIXME: We're missing a sub-pixel offset here crbug.com/348728

  EnclosingPaginationLayer()->CollectFragments(
      enclosing_pagination_fragments, root_layer, nullptr,
      kExcludeOverlayScrollbarSizeForHitTesting, clip_behavior, nullptr,
      PhysicalOffset());

  for (const auto& fragment : enclosing_pagination_fragments) {
    // Apply the page/column clip for this fragment, as well as any clips
    // established by layers in between us and the enclosing pagination layer.
    PhysicalRect clip_rect = fragment.background_rect.Rect();
    if (!recursion_data.location.Intersects(clip_rect))
      continue;

    PaintLayer* hit_layer = HitTestLayerByApplyingTransform(
        root_layer, container_layer, result, recursion_data, transform_state,
        z_offset, check_resizer_only,
        fragment.fragment_data->LegacyPaginationOffset());
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
    bool check_resizer_only,
    const PhysicalOffset& translation_offset) {
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
  PhysicalRect bounds_of_mapped_area = new_transform_state.BoundsOfMappedArea();
  base::Optional<HitTestLocation> new_location;
  if (recursion_data.location.IsRectBasedTest())
    new_location.emplace(local_point, new_transform_state.MappedQuad());
  else
    new_location.emplace(local_point, new_transform_state.BoundsOfMappedQuad());
  HitTestRecursionData new_recursion_data(bounds_of_mapped_area, *new_location,
                                          recursion_data.original_location);

  // Now do a hit test with the root layer shifted to be us.
  return HitTestLayer(this, container_layer, result, new_recursion_data, true,
                      &new_transform_state, z_offset, check_resizer_only);
}

bool PaintLayer::HitTestContents(HitTestResult& result,
                                 const NGPhysicalBoxFragment* physical_fragment,
                                 const PhysicalOffset& fragment_offset,
                                 const HitTestLocation& hit_test_location,
                                 HitTestFilter hit_test_filter) const {
  DCHECK(IsSelfPaintingLayer() || HasSelfPaintingLayerDescendant());

  bool did_hit;
  if (physical_fragment) {
    did_hit = NGBoxFragmentPainter(*physical_fragment)
                  .HitTestAllPhases(result, hit_test_location, fragment_offset,
                                    hit_test_filter);
  } else {
    did_hit = GetLayoutObject().HitTestAllPhases(
        result, hit_test_location, fragment_offset, hit_test_filter);
  }

  if (!did_hit) {
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
  PropagateDescendantNeedsCompositingLayerAssignment();
}

void PaintLayer::PropagateDescendantNeedsCompositingLayerAssignment() {
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
    PaintLayerIteration children_to_visit,
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

  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return nullptr;

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  PaintLayer* stop_layer = stop_node ? stop_node->PaintingLayer() : nullptr;

  PaintLayer* result_layer = nullptr;
  PaintLayerPaintOrderReverseIterator iterator(*this, children_to_visit);
  while (PaintLayer* child_layer = iterator.Next()) {
    if (child_layer->IsReplacedNormalFlowStacking())
      continue;

    // Avoid the call to child_layer->HitTestLayer() if possible.
    if (stop_layer == this &&
        !IsHitCandidateForStopNode(child_layer->GetLayoutObject(), stop_node)) {
      continue;
    }

    PaintLayer* hit_layer = nullptr;
    STACK_UNINITIALIZED HitTestResult temp_result(
        result.GetHitTestRequest(), recursion_data.original_location);
    temp_result.SetInertNode(result.InertNode());
    hit_layer = child_layer->HitTestLayer(
        root_layer, this, temp_result, recursion_data, false, transform_state,
        z_offset_for_descendants);

    // If it is a list-based test, we can safely append the temporary result
    // since it might had hit nodes but not necesserily had hitLayer set.
    if (result.GetHitTestRequest().ListBased())
      result.Append(temp_result);

    if (IsHitCandidateForDepthOrder(hit_layer, depth_sort_descendants, z_offset,
                                    unflattened_transform_state)) {
      result_layer = hit_layer;
      if (!result.GetHitTestRequest().ListBased())
        result = temp_result;
      if (!depth_sort_descendants)
        break;
    } else if (result.GetHitTestRequest().RetargetForInert()) {
      result.SetInertNode(temp_result.InertNode());
    }
  }

  return result_layer;
}

void PaintLayer::UpdateFilterReferenceBox() {
  if (!HasFilterThatMovesPixels())
    return;
  PhysicalRect result = LocalBoundingBox();
  ExpandRectForStackingChildren(
      *this, result, PaintLayer::kIncludeTransformsAndCompositedChildLayers);
  FloatRect reference_box = FloatRect(result);

  float zoom = GetLayoutObject().StyleRef().EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(1 / zoom);
  if (!ResourceInfo() || ResourceInfo()->FilterReferenceBox() != reference_box)
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
  EnsureResourceInfo().SetFilterReferenceBox(reference_box);
}

FloatRect PaintLayer::FilterReferenceBox() const {
#if DCHECK_IS_ON()
  DCHECK(GetLayoutObject().GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kInPrePaint);
#endif
  if (ResourceInfo())
    return ResourceInfo()->FilterReferenceBox();
  return FloatRect();
}

FloatRect PaintLayer::BackdropFilterReferenceBox() const {
  FloatRect reference_box(GetLayoutObject().BorderBoundingBox());
  float zoom = GetLayoutObject().StyleRef().EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(1 / zoom);
  return reference_box;
}

gfx::RRectF PaintLayer::BackdropFilterBounds(
    const FloatRect& reference_box) const {
  auto& style = GetLayoutObject().StyleRef();
  FloatRect scaled_reference_box(reference_box);
  scaled_reference_box.Scale(style.EffectiveZoom());
  gfx::RRectF backdrop_filter_bounds =
      gfx::RRectF(RoundedBorderGeometry::PixelSnappedRoundedBorder(
          style, PhysicalRect::EnclosingRect(scaled_reference_box)));
  return backdrop_filter_bounds;
}
bool PaintLayer::HitTestClippedOutByClipPath(
    PaintLayer* root_layer,
    const HitTestLocation& hit_test_location) const {
  DCHECK(GetLayoutObject().HasClipPath());
  DCHECK(IsSelfPaintingLayer());
  DCHECK(root_layer);

  PhysicalRect origin;
  if (EnclosingPaginationLayer())
    ConvertFromFlowThreadToVisualBoundingBoxInAncestor(root_layer, origin);
  else
    ConvertToLayerCoords(root_layer, origin);

  FloatPoint point(hit_test_location.Point() - origin.offset);
  FloatRect reference_box(
      ClipPathClipper::LocalReferenceBox(GetLayoutObject()));

  ClipPathOperation* clip_path_operation =
      GetLayoutObject().StyleRef().ClipPath();
  DCHECK(clip_path_operation);
  if (clip_path_operation->GetType() == ClipPathOperation::SHAPE) {
    ShapeClipPathOperation* clip_path =
        To<ShapeClipPathOperation>(clip_path_operation);
    return !clip_path
                ->GetPath(reference_box,
                          GetLayoutObject().StyleRef().EffectiveZoom())
                .Contains(point);
  }
  DCHECK_EQ(clip_path_operation->GetType(), ClipPathOperation::REFERENCE);
  LayoutSVGResourceClipper* clipper = GetSVGResourceAsType(clip_path_operation);
  if (!clipper)
    return false;
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
    const PhysicalRect& layer_bounds,
    const PhysicalRect& damage_rect,
    const PhysicalOffset& offset_from_root) const {
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

PhysicalRect PaintLayer::LocalBoundingBox() const {
  PhysicalRect rect = GetLayoutObject().PhysicalVisualOverflowRect();
  if (GetLayoutObject().IsEffectiveRootScroller() || IsRootLayer()) {
    rect.Unite(
        PhysicalRect(rect.offset, GetLayoutObject().View()->ViewRect().size));
  }
  return rect;
}

PhysicalRect PaintLayer::PhysicalBoundingBox(
    const PaintLayer* ancestor_layer) const {
  PhysicalOffset offset_from_root;
  ConvertToLayerCoords(ancestor_layer, offset_from_root);
  return PhysicalBoundingBox(offset_from_root);
}

PhysicalRect PaintLayer::PhysicalBoundingBox(
    const PhysicalOffset& offset_from_root) const {
  PhysicalRect result = LocalBoundingBox();
  result.Move(offset_from_root);
  return result;
}

PhysicalRect PaintLayer::FragmentsBoundingBox(
    const PaintLayer* ancestor_layer) const {
  if (!EnclosingPaginationLayer())
    return PhysicalBoundingBox(ancestor_layer);

  PhysicalRect result = LocalBoundingBox();
  ConvertFromFlowThreadToVisualBoundingBoxInAncestor(ancestor_layer, result);
  return result;
}

PhysicalRect PaintLayer::BoundingBoxForCompositingOverlapTest() const {
  // Apply NeverIncludeTransformForAncestorLayer, because the geometry map in
  // CompositingInputsUpdater will take care of applying the transform of |this|
  // (== the ancestorLayer argument to boundingBoxForCompositing).
  // TODO(trchen): Layer fragmentation is inhibited across compositing boundary.
  // Should we return the unfragmented bounds for overlap testing? Or perhaps
  // assume fragmented layers always overlap?
  PhysicalRect bounding_box = FragmentsBoundingBox(this);
  const ComputedStyle& style = GetLayoutObject().StyleRef();

  if (PaintsWithFilters())
    bounding_box = MapRectForFilter(bounding_box);

  if (style.HasBackdropFilter() &&
      style.BackdropFilter().HasFilterThatMovesPixels()) {
    bounding_box = PhysicalRect::EnclosingRect(
        style.BackdropFilter().MapRect(FloatRect(bounding_box)));
  }

  if (FixedToViewport() && !bounding_box.IsEmpty()) {
    DCHECK_EQ(style.GetPosition(), EPosition::kFixed);
    // Note that we only expand the bounding box for overlap testing when the
    // fixed's containing block is the viewport. This keeps us from expanding
    // the bounds when the fixed is a child of an ancestor with transform,
    // filters, etc. and the fixed is no longer scroll position dependent.

    // Expand the bounding box by the amount that scrolling the
    // viewport can expand the area that this fixed-pos element could
    // cover. Compute how much we could still scroll in each direction.
    // |max_scroll_delta| is the amount we could still scroll in
    // increasing offset direction. |min_scroll_delta| is the amount we
    // can still scroll in a decreasing scroll offset direction.
    PaintLayerScrollableArea* scrollable_area =
        GetLayoutObject().View()->GetScrollableArea();
    ScrollOffset current_scroll_offset = scrollable_area->GetScrollOffset();
    ScrollOffset max_scroll_delta =
        scrollable_area->MaximumScrollOffset() - current_scroll_offset;
    ScrollOffset min_scroll_delta =
        current_scroll_offset - scrollable_area->MinimumScrollOffset();
    bounding_box.Expand(
        LayoutRectOutsets(min_scroll_delta.Height(), max_scroll_delta.Width(),
                          max_scroll_delta.Height(), min_scroll_delta.Width()));
  }
  return bounding_box;
}

void PaintLayer::ExpandRectForStackingChildren(
    const PaintLayer& composited_layer,
    PhysicalRect& result,
    PaintLayer::CalculateBoundsOptions options) const {
  // If we're locked, th en the subtree does not contribute painted output.
  // Furthermore, we might not have up-to-date sizing and position information
  // in the subtree, so skip recursing into the subtree.
  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return;

  PaintLayerPaintOrderIterator iterator(*this, kAllChildren);
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

PhysicalRect PaintLayer::BoundingBoxForCompositing() const {
  return BoundingBoxForCompositingInternal(
      *this, nullptr, kIncludeClipsAndMaybeIncludeTransformForAncestorLayer);
}

bool PaintLayer::ShouldApplyTransformToBoundingBox(
    const PaintLayer& composited_layer,
    CalculateBoundsOptions options) const {
  if (!Transform())
    return false;
  if (options == kIncludeTransformsAndCompositedChildLayers)
    return true;
  if (PaintsWithTransform(kGlobalPaintNormalPhase)) {
    if (this != &composited_layer)
      return true;
    if (options == kIncludeClipsAndMaybeIncludeTransformForAncestorLayer)
      return true;
  }
  return false;
}

PhysicalRect PaintLayer::BoundingBoxForCompositingInternal(
    const PaintLayer& composited_layer,
    const PaintLayer* stacking_parent,
    CalculateBoundsOptions options) const {
  DCHECK_GE(GetLayoutObject().GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  if (!IsSelfPaintingLayer())
    return PhysicalRect();

  // FIXME: This could be improved to do a check like
  // hasVisibleNonCompositingDescendantLayers() (bug 92580).
  if (this != &composited_layer && !HasVisibleContent() &&
      !HasVisibleDescendant())
    return PhysicalRect();

  if (GetLayoutObject().IsEffectiveRootScroller() || IsRootLayer()) {
    // In root layer scrolling mode, the main GraphicsLayer is the size of the
    // layout viewport. In non-RLS mode, it is the union of the layout viewport
    // and the document's layout overflow rect.
    IntRect result = IntRect();
    if (LocalFrameView* frame_view = GetLayoutObject().GetFrameView())
      result = IntRect(IntPoint(), frame_view->Size());
    return PhysicalRect(result);
  }

  // The layer created for the LayoutFlowThread is just a helper for painting
  // and hit-testing, and should not contribute to the bounding box. The
  // LayoutMultiColumnSets will contribute the correct size for the layout
  // content of the multicol container.
  if (GetLayoutObject().IsLayoutFlowThread())
    return PhysicalRect();

  PhysicalRect result;
  if (options == kIncludeClipsAndMaybeIncludeTransformForAncestorLayer) {
    // If there is a clip applied by an ancestor to this PaintLayer but below or
    // equal to |ancestorLayer|, apply that clip. This optimizes the size
    // of the composited layer to exclude clipped-out regions of descendants.
    result = Clipper((GetLayoutObject().GetDocument().Lifecycle().GetState() ==
                      DocumentLifecycle::kInCompositingAssignmentsUpdate)
                         ? GeometryMapperOption::kUseGeometryMapper
                         : GeometryMapperOption::kUseGeometryMapper)
                 .LocalClipRect(composited_layer);

    result.Intersect(LocalBoundingBox());
  } else {
    result = LocalBoundingBox();
  }

  ExpandRectForStackingChildren(composited_layer, result, options);

  // Only enlarge by the filter outsets if we know the filter is going to be
  // rendered in software.  Accelerated filters will handle their own outsets.
  if (PaintsWithFilters())
    result = MapRectForFilter(result);

  if (ShouldApplyTransformToBoundingBox(composited_layer, options)) {
    result =
        PhysicalRect::EnclosingRect(Transform()->MapRect(FloatRect(result)));
  }

  if (ShouldFragmentCompositedBounds(&composited_layer)) {
    ConvertFromFlowThreadToVisualBoundingBoxInAncestor(&composited_layer,
                                                       result);
    return result;
  }

  if (stacking_parent) {
    PhysicalOffset delta;
    ConvertToLayerCoords(stacking_parent, delta);
    result.Move(delta);
  }
  return result;
}

CompositingState PaintLayer::GetCompositingState() const {
#if DCHECK_IS_ON()
  DCHECK(IsAllowedToQueryCompositingState())
      << " " << GetLayoutObject().GetDocument().Lifecycle().ToString();
#endif

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
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return true;
  if (!GetLayoutObject().GetFrameView()->IsUpdatingLifecycle())
    return true;
  return GetLayoutObject().GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kInCompositingAssignmentsUpdate;
}

bool PaintLayer::IsAllowedToQueryCompositingInputs() const {
  if (g_compositing_query_mode == kCompositingQueriesAreAllowed ||
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return true;
  return GetLayoutObject().GetDocument().Lifecycle().GetState() >=
         DocumentLifecycle::kCompositingInputsClean;
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
      return GroupedMapping()->SquashingLayer(*this);
    default:
      return (obj != &GetLayoutObject() &&
              GetCompositedLayerMapping()->ScrollingContentsLayer())
                 ? GetCompositedLayerMapping()->ScrollingContentsLayer()
                 : GetCompositedLayerMapping()->MainGraphicsLayer();
  }
}

void PaintLayer::EnsureCompositedLayerMapping() {
  if (HasCompositedLayerMapping())
    return;

  EnsureRareData().composited_layer_mapping =
      std::make_unique<CompositedLayerMapping>(*this);
  rare_data_->composited_layer_mapping->SetNeedsGraphicsLayerUpdate(
      kGraphicsLayerUpdateSubtree);
}

void PaintLayer::ClearCompositedLayerMapping(bool layer_being_destroyed) {
  DCHECK(HasCompositedLayerMapping());
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  DisableCompositingQueryAsserts disabler;
  if (!layer_being_destroyed) {
    // We need to make sure our descendants get a geometry update. In principle,
    // we could call setNeedsGraphicsLayerUpdate on our children, but that would
    // require walking the z-order lists to find them. Instead, we
    // over-invalidate by marking our parent as needing a geometry update.
    if (PaintLayer* compositing_parent =
            EnclosingLayerWithCompositedLayerMapping(kExcludeSelf))
      compositing_parent->GetCompositedLayerMapping()
          ->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
  }
  DCHECK(rare_data_);
  rare_data_->composited_layer_mapping.reset();
}

void PaintLayer::SetGroupedMapping(CompositedLayerMapping* grouped_mapping,
                                   SetGroupMappingOptions options) {
  CompositedLayerMapping* old_grouped_mapping = GroupedMapping();
  if (grouped_mapping == old_grouped_mapping)
    return;

  if (options == kInvalidateLayerAndRemoveFromMapping && old_grouped_mapping) {
    old_grouped_mapping->SetNeedsGraphicsLayerUpdate(
        kGraphicsLayerUpdateSubtree);
    old_grouped_mapping->RemoveLayerFromSquashingGraphicsLayer(*this);
  }
  if (rare_data_ || grouped_mapping)
    EnsureRareData().grouped_mapping = grouped_mapping;
#if DCHECK_IS_ON()
  if (grouped_mapping)
    grouped_mapping->AssertInSquashedLayersVector(*this);
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

bool PaintLayer::PaintsWithDirectReasonIntoOwnBacking(
    GlobalPaintFlags global_paint_flags) const {
  return !(global_paint_flags & kGlobalPaintFlattenCompositingLayers) &&
         CanBeCompositedForDirectReasons();
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

  // Don't create subsequence for the document element because the subsequence
  // for LayoutView serves the same purpose. This can avoid unnecessary paint
  // chunks that would otherwise be forced by the subsequence.
  if (GetLayoutObject().IsDocumentElement())
    return false;

  // Create subsequence for only stacking contexts whose painting are atomic.
  return GetLayoutObject().IsStackingContext();
}

ScrollingCoordinator* PaintLayer::GetScrollingCoordinator() {
  Page* page = GetLayoutObject().GetFrame()->GetPage();
  return (!page) ? nullptr : page->GetScrollingCoordinator();
}

bool PaintLayer::CompositesWithTransform() const {
  return TransformAncestor() || Transform();
}

bool PaintLayer::BackgroundIsKnownToBeOpaqueInRect(
    const PhysicalRect& local_rect,
    bool should_check_children) const {
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

  if (GetLayoutObject().StyleRef().GetPosition() == EPosition::kFixed &&
      GetCompositingState() != kPaintsIntoOwnBacking)
    return false;

  // FIXME: We currently only check the immediate layoutObject,
  // which will miss many cases where additional layout objects paint
  // into this layer.
  if (GetLayoutObject().BackgroundIsKnownToBeOpaqueInRect(local_rect))
    return true;

  if (!should_check_children)
    return false;

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
    const PhysicalRect& local_rect) const {
  PaintLayerPaintOrderReverseIterator reverse_iterator(*this, kAllChildren);
  while (PaintLayer* child_layer = reverse_iterator.Next()) {
    // Stop at composited paint boundaries and non-self-painting layers.
    if (child_layer->IsPaintInvalidationContainer())
      continue;

    if (!child_layer->CanUseConvertToLayerCoords())
      continue;

    if (child_layer->PaintsWithTransparency(kGlobalPaintNormalPhase))
      continue;

    PhysicalOffset child_offset;
    PhysicalRect child_local_rect(local_rect);
    child_layer->ConvertToLayerCoords(this, child_offset);
    child_local_rect.Move(-child_offset);

    if (child_layer->BackgroundIsKnownToBeOpaqueInRect(child_local_rect, true))
      return true;
  }
  return false;
}

bool PaintLayer::ShouldBeSelfPaintingLayer() const {
  return GetLayoutObject().LayerTypeRequired() == kNormalPaintLayer ||
         (scrollable_area_ && scrollable_area_->HasOverlayOverflowControls()) ||
         ScrollsOverflow() ||
         (RuntimeEnabledFeatures::CompositeSVGEnabled() &&
          GetLayoutObject().IsSVGRoot() &&
          To<LayoutSVGRoot>(GetLayoutObject())
              .HasDescendantCompositingReasons());
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

  if (is_self_painting_layer)
    SetNeedsVisualOverflowRecalc();

  if (PaintLayer* parent = Parent()) {
    parent->MarkAncestorChainForFlagsUpdate();

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
  for (const auto* child = GetLayoutObject().SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->HasLayer()) {
      if (child->IsLayoutInline() || !child->IsBox())
        return true;

      const auto* box = To<LayoutBox>(child);
      if (!box->Size().IsZero() || box->HasVisualOverflow())
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
}

void PaintLayer::UpdateBackdropFilters(const ComputedStyle* old_style,
                                       const ComputedStyle& new_style) {
  if (!backdrop_filter_on_effect_node_dirty_) {
    backdrop_filter_on_effect_node_dirty_ =
        old_style ? !old_style->BackdropFilterDataEquivalent(new_style)
                  : new_style.HasBackdropFilter();
  }
}

void PaintLayer::UpdateClipPath(const ComputedStyle* old_style,
                                const ComputedStyle& new_style) {
  ClipPathOperation* new_clip = new_style.ClipPath();
  ClipPathOperation* old_clip = old_style ? old_style->ClipPath() : nullptr;
  if (!new_clip && !old_clip)
    return;
  const bool had_resource_info = ResourceInfo();
  if (auto* reference_clip = DynamicTo<ReferenceClipPathOperation>(new_clip))
    reference_clip->AddClient(EnsureResourceInfo());
  if (had_resource_info) {
    if (auto* old_reference_clip =
            DynamicTo<ReferenceClipPathOperation>(old_clip))
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

  // If a transform changed, we can't use the fast path.
  if (diff.TransformChanged())
    return false;

  // We composite transparent Layers differently from non-transparent
  // Layers even when the non-transparent Layers are already a
  // stacking context.
  if (diff.OpacityChanged() &&
      layout_object_->StyleRef().HasOpacity() != old_style->HasOpacity())
    return false;

  // Changes in pointer-events affect hit test visibility of the scrollable
  // area and its |m_scrollsOverflow| value which determines if the layer
  // requires composited scrolling or not.
  if (scrollable_area_ &&
      layout_object_->StyleRef().PointerEvents() != old_style->PointerEvents())
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

  if (PaintLayerStackingNode::StyleDidChange(*this, old_style))
    MarkAncestorChainForFlagsUpdate();

  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterStyleChange(old_style);
  }

  // Overlay scrollbars can make this layer self-painting so we need
  // to recompute the bit once scrollbars have been updated.
  UpdateSelfPaintingLayer();

  if (!diff.CompositingReasonsChanged() &&
      !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // For querying stale GetCompositingState().
    DisableCompositingQueryAsserts disable;

    // Compositing inputs update is required when the PaintLayer is currently
    // composited. This is because even style changes as simple as background
    // color change, or pointer-events state change, can update compositing
    // state.
    if (old_style && GetCompositingState() == kPaintsIntoOwnBacking)
      SetNeedsCompositingInputsUpdate();
  }

  // HasAlphaChanged can affect whether a composited layer is opaque.
  if (diff.NeedsLayout() || diff.HasAlphaChanged())
    SetNeedsCompositingInputsUpdate();

  // A scroller that changes background color might become opaque or not
  // opaque, which in turn affects whether it can be composited on low-DPI
  // screens.
  if (GetScrollableArea() && GetScrollableArea()->ScrollsOverflow() &&
      diff.HasDifference()) {
    SetNeedsCompositingInputsUpdate();
  }

  // See also |LayoutObject::SetStyle| which handles these invalidations if a
  // PaintLayer is not present.
  if (diff.TransformChanged() || diff.OpacityChanged() ||
      diff.ZIndexChanged() || diff.FilterChanged() || diff.CssClipChanged() ||
      diff.BlendModeChanged() || diff.MaskChanged() ||
      diff.CompositingReasonsChanged()) {
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
    SetNeedsCompositingInputsUpdate();
  }

  const ComputedStyle& new_style = GetLayoutObject().StyleRef();
  // HasNonContainedAbsolutePositionDescendant depends on position changes.
  if (!old_style || old_style->GetPosition() != new_style.GetPosition())
    MarkAncestorChainForFlagsUpdate();

  UpdateTransform(old_style, new_style);
  UpdateFilters(old_style, new_style);
  UpdateBackdropFilters(old_style, new_style);
  UpdateClipPath(old_style, new_style);

  if (!SelfNeedsRepaint()) {
    if (diff.ZIndexChanged()) {
      // We don't need to invalidate paint of objects when paint order
      // changes. However, we do need to repaint the containing stacking
      // context, in order to generate new paint chunks in the correct order.
      // Raster invalidation will be issued if needed during paint.
      SetNeedsRepaint();
    } else if (old_style &&
               !RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // Change of PaintedOutputInvisible() will affect existence of paint
      // chunks, so needs repaint.
      if (PaintLayerPainter::PaintedOutputInvisible(*old_style) !=
          PaintLayerPainter::PaintedOutputInvisible(new_style))
        SetNeedsRepaint();
    }
  }
}

LayoutSize PaintLayer::PixelSnappedScrolledContentOffset() const {
  if (GetLayoutObject().IsScrollContainer())
    return GetLayoutBox()->PixelSnappedScrolledContentOffset();
  return LayoutSize();
}

PaintLayerClipper PaintLayer::Clipper(
    GeometryMapperOption geometry_mapper_option) const {
  return PaintLayerClipper(*this, geometry_mapper_option ==
                                      GeometryMapperOption::kUseGeometryMapper);
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
        MakeGarbageCollected<BoxReflectFilterOperation>(reflection));
  }
  return filter_operations;
}

void PaintLayer::UpdateCompositorFilterOperationsForFilter(
    CompositorFilterOperations& operations) const {
  auto filter = FilterOperationsIncludingReflection();
  FloatRect reference_box = FilterReferenceBox();
  if (!operations.IsEmpty() && !filter_on_effect_node_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;
  float zoom = GetLayoutObject().StyleRef().EffectiveZoom();
  operations =
      FilterEffectBuilder(reference_box, zoom).BuildFilterOperations(filter);
}

void PaintLayer::UpdateCompositorFilterOperationsForBackdropFilter(
    CompositorFilterOperations& operations,
    base::Optional<gfx::RRectF>* backdrop_filter_bounds) const {
  DCHECK(backdrop_filter_bounds);
  const auto& style = GetLayoutObject().StyleRef();
  if (style.BackdropFilter().IsEmpty()) {
    operations.Clear();
    backdrop_filter_bounds->reset();
    return;
  }
  FloatRect reference_box = BackdropFilterReferenceBox();
  *backdrop_filter_bounds = BackdropFilterBounds(reference_box);
  if (operations.IsEmpty() || backdrop_filter_on_effect_node_dirty_ ||
      reference_box != operations.ReferenceBox()) {
    operations = CreateCompositorFilterOperationsForBackdropFilter();
  }
}

CompositorFilterOperations
PaintLayer::CreateCompositorFilterOperationsForBackdropFilter() const {
  const auto& style = GetLayoutObject().StyleRef();
  CompositorFilterOperations return_value;
  if (style.BackdropFilter().IsEmpty()) {
    return return_value;
  }
  float zoom = style.EffectiveZoom();
  FloatRect reference_box = BackdropFilterReferenceBox();
  // Tack on regular filter values here - they need to be applied to the
  // backdrop image as well, in addition to being applied to the painted content
  // and children of the element. This is a bit of a hack - according to the
  // spec, filters should apply to the entire render pass as a whole, including
  // the backdrop-filtered content. However, because in the case that we have
  // both filters and backdrop-filters on a single element, we create two effect
  // nodes, and two render surfaces, and the backdrop-filter node comes first.
  // To get around that, we add the "regular" filters to the backdrop filters to
  // approximate.
  FilterOperations filter_operations = style.BackdropFilter();
  filter_operations.Operations().AppendVector(style.Filter().Operations());
  // Use kClamp tile mode to avoid pixel moving filters bringing in black
  // transparent pixels from the viewport edge.
  return_value = FilterEffectBuilder(reference_box, zoom, nullptr, nullptr,
                                     SkBlurImageFilter::kClamp_TileMode)
                     .BuildFilterOperations(filter_operations);
  // Note that return_value may be empty here, if the |filter_operations| list
  // contains only invalid filters (e.g. invalid reference filters). See
  // https://crbug.com/983157 for details.
  return return_value;
}

PaintLayerResourceInfo& PaintLayer::EnsureResourceInfo() {
  PaintLayerRareData& rare_data = EnsureRareData();
  if (!rare_data.resource_info) {
    rare_data.resource_info =
        MakeGarbageCollected<PaintLayerResourceInfo>(this);
  }
  return *rare_data.resource_info;
}

void PaintLayer::RemoveAncestorScrollContainerLayer(
    const PaintLayer* removed_layer) {
  // If the current scroll container layer does not match the removed layer
  // the ancestor overflow layer has changed so we can stop searching.
  if (AncestorScrollContainerLayer() &&
      AncestorScrollContainerLayer() != removed_layer) {
    return;
  }

  if (AncestorScrollContainerLayer()) {
    // If the previous AncestorScrollContainerLayer is the root and this object
    // is a sticky viewport constrained object, it is no longer known to be
    // constrained by the root.
    if (AncestorScrollContainerLayer()->IsRootLayer() &&
        GetLayoutObject().StyleRef().HasStickyConstrainedPosition()) {
      if (LocalFrameView* frame_view = GetLayoutObject().GetFrameView()) {
        frame_view->RemoveViewportConstrainedObject(
            GetLayoutObject(),
            LocalFrameView::ViewportConstrainedType::kSticky);
      }
    }

    if (PaintLayerScrollableArea* ancestor_scrollable_area =
            AncestorScrollContainerLayer()->GetScrollableArea()) {
      // TODO(pdr): When CompositeAfterPaint is enabled, we will need to
      // invalidate the scroll paint property subtree for this so main thread
      // scroll reasons are recomputed.
      ancestor_scrollable_area->InvalidateStickyConstraintsFor(this);
    }
  }
  UpdateAncestorScrollContainerLayer(nullptr);
  PaintLayer* current = first_;
  while (current) {
    current->RemoveAncestorScrollContainerLayer(removed_layer);
    current = current->NextSibling();
  }
}

FloatRect PaintLayer::MapRectForFilter(const FloatRect& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;
  return FilterOperationsIncludingReflection().MapRect(rect);
}

PhysicalRect PaintLayer::MapRectForFilter(const PhysicalRect& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;
  return PhysicalRect::EnclosingRect(MapRectForFilter(FloatRect(rect)));
}

bool PaintLayer::HasFilterThatMovesPixels() const {
  if (!HasFilterInducingProperty())
    return false;
  const ComputedStyle& style = GetLayoutObject().StyleRef();
  if (style.HasFilter() && style.Filter().HasFilterThatMovesPixels())
    return true;
  if (GetLayoutObject().HasReflection())
    return true;
  return false;
}

void PaintLayer::SetNeedsRepaint() {
  SetSelfNeedsRepaint();

  LocalFrameView* frame_view = GetLayoutObject().GetDocument().View();
  if (frame_view) {
    // If you need repaint, then you might do layerization and isue raster
    // invalidations. In CompositeAfterPaint mode, and in CompositeSVG mode for
    // SVG roots, we do these in PAC::Update(). TODO(paint-team): distinguish
    // requirements for layerization and non-geometry raster invalidations.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
        (RuntimeEnabledFeatures::CompositeSVGEnabled() &&
         GetLayoutObject().IsSVGRoot()))
      frame_view->SetPaintArtifactCompositorNeedsUpdate();
  }

  // Do this unconditionally to ensure container chain is marked when
  // compositing status of the layer changes.
  MarkCompositingContainerChainForNeedsRepaint();
}

void PaintLayer::SetSelfNeedsRepaint() {
  self_needs_repaint_ = true;
  // Invalidate as a display item client.
  static_cast<DisplayItemClient*>(this)->Invalidate();
}

void PaintLayer::MarkCompositingContainerChainForNeedsRepaint() {
  PaintLayer* layer = this;
  while (true) {
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // Need to access compositingState(). We've ensured correct flag setting
      // when compositingState() changes.
      DisableCompositingQueryAsserts disabler;
      if (layer->GetCompositingState() == kPaintsIntoOwnBacking)
        return;
      if (CompositedLayerMapping* grouped_mapping = layer->GroupedMapping()) {
        // TODO(wkorman): As we clean up the CompositedLayerMapping needsRepaint
        // logic to delegate to scrollbars, we may be able to remove the line
        // below as well.
        grouped_mapping->OwningLayer().SetNeedsRepaint();
        return;
      }
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

    // If the container already needs descendants repaint, break out of the
    // loop. Also, if the layer doesn't need painting itself (which means we're
    // propagating a bit from its children) and it blocks child painting via
    // display lock, then stop propagating the dirty bit.
    if (container->descendant_needs_repaint_ ||
        (!layer->SelfNeedsRepaint() &&
         layer->GetLayoutObject().ChildPaintBlockedByDisplayLock())) {
      break;
    }

    container->descendant_needs_repaint_ = true;
    layer = container;
  }
}

void PaintLayer::ClearNeedsRepaintRecursively() {
  self_needs_repaint_ = false;

  // Don't clear dirty bits in a display-locked subtree.
  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return;

  for (PaintLayer* child = FirstChild(); child; child = child->NextSibling())
    child->ClearNeedsRepaintRecursively();
  descendant_needs_repaint_ = false;
}

const PaintLayer* PaintLayer::CommonAncestor(const PaintLayer* other) const {
  DCHECK(other);
  if (this == other)
    return this;

  int this_depth = 0;
  for (auto* layer = this; layer; layer = layer->Parent()) {
    if (layer == other)
      return layer;
    this_depth++;
  }
  int other_depth = 0;
  for (auto* layer = other; layer; layer = layer->Parent()) {
    if (layer == this)
      return layer;
    other_depth++;
  }

  const PaintLayer* this_iterator = this;
  const PaintLayer* other_iterator = other;
  for (; this_depth > other_depth; this_depth--)
    this_iterator = this_iterator->Parent();
  for (; other_depth > this_depth; other_depth--)
    other_iterator = other_iterator->Parent();

  while (this_iterator) {
    if (this_iterator == other_iterator)
      return this_iterator;
    this_iterator = this_iterator->Parent();
    other_iterator = other_iterator->Parent();
  }

  DCHECK(!this_iterator);
  DCHECK(!other_iterator);
  return nullptr;
}

void PaintLayer::DirtyStackingContextZOrderLists() {
  auto* stacking_context = AncestorStackingContext();
  if (!stacking_context)
    return;

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // This invalidation code intentionally refers to stale state.
    DisableCompositingQueryAsserts disabler;

    // Changes of stacking may result in graphics layers changing size
    // due to new contents painting into them.
    if (auto* mapping = stacking_context->GetCompositedLayerMapping())
      mapping->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);
  }

  if (stacking_context->StackingNode())
    stacking_context->StackingNode()->DirtyZOrderLists();

  MarkAncestorChainForFlagsUpdate();
}

DisableCompositingQueryAsserts::DisableCompositingQueryAsserts()
    : disabler_(&g_compositing_query_mode, kCompositingQueriesAreAllowed) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
}

}  // namespace blink

#if DCHECK_IS_ON()
void showLayerTree(const blink::PaintLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showLayerTree. Root is (nil)";
    return;
  }

  base::Optional<blink::DisableCompositingQueryAsserts> disabler;
  if (!blink::RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    disabler.emplace();

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
    LOG(INFO) << output.Utf8();
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
