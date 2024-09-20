/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@dbaron.org>
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

#include "base/containers/adapters.h"
#include "build/build_config.h"
#include "cc/input/scroll_snap_data.h"
#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/fragmentainer_iterator.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/box_reflection_utils.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/hit_testing_transform_state.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/transform_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/reference_offset_path_operation.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
struct SameSizeAsPaintLayer : GarbageCollected<PaintLayer>, DisplayItemClient {
  // The bit fields may fit into the machine word of DisplayItemClient which
  // has only 8-bit data.
  unsigned bit_fields1 : 24;
  unsigned bit_fields2 : 24;
#if DCHECK_IS_ON()
  bool is_destroyed;
#endif
  Member<void*> members[9];
  LayoutUnit layout_units[2];
  std::unique_ptr<void*> pointer;
};

ASSERT_SIZE(PaintLayer, SameSizeAsPaintLayer);
#endif

inline PhysicalRect PhysicalVisualOverflowRectAllowingUnset(
    const LayoutBoxModelObject& layout_object) {
#if DCHECK_IS_ON()
  InkOverflow::ReadUnsetAsNoneScope read_unset_as_none;
#endif
  return layout_object.VisualOverflowRect();
}

PaintLayer* SlowContainingLayer(LayoutObject& layout_object) {
  // This is a universal approach to find the containing layer, but it is
  // slower.
  auto* container = layout_object.Container(nullptr);
  while (container) {
    if (container->HasLayer())
      return To<LayoutBoxModelObject>(container)->Layer();
    container = container->Container(nullptr);
  }
  return nullptr;
}

std::optional<gfx::SizeF> ComputeFilterViewport(const PaintLayer& layer) {
  if (const auto* layout_inline =
          DynamicTo<LayoutInline>(layer.GetLayoutObject())) {
    return gfx::SizeF(layout_inline->PhysicalLinesBoundingBox().size);
  }
  const auto* box = layer.GetLayoutBox();
  if (box->IsSVGForeignObject()) {
    return std::nullopt;
  }
  return gfx::SizeF(box->Size());
}

}  // namespace

PaintLayer::PaintLayer(LayoutBoxModelObject* layout_object)
    : is_root_layer_(IsA<LayoutView>(layout_object)),
      has_visible_content_(false),
      needs_descendant_dependent_flags_update_(true),
      needs_visual_overflow_recalc_(true),
      has_visible_self_painting_descendant_(false),
      has3d_transformed_descendant_(false),
      self_needs_repaint_(false),
      descendant_needs_repaint_(false),
      needs_cull_rect_update_(false),
      forces_children_cull_rect_update_(false),
      descendant_needs_cull_rect_update_(false),
      previous_paint_result_(kMayBeClippedByCullRect),
      needs_paint_phase_descendant_outlines_(false),
      needs_paint_phase_float_(false),
      has_non_isolated_descendant_with_blend_mode_(false),
      has_fixed_position_descendant_(false),
      has_non_contained_absolute_position_descendant_(false),
      has_stacked_descendant_in_current_stacking_context_(false),
      filter_on_effect_node_dirty_(false),
      backdrop_filter_on_effect_node_dirty_(false),
      has_filter_that_moves_pixels_(false),
      is_under_svg_hidden_container_(false),
      has_self_painting_layer_descendant_(false),
      needs_reorder_overlay_overflow_controls_(false),
      static_inline_edge_(InlineEdge::kInlineStart),
      static_block_edge_(BlockEdge::kBlockStart),
#if DCHECK_IS_ON()
      layer_list_mutation_allowed_(true),
#endif
      layout_object_(layout_object),
      parent_(nullptr),
      previous_(nullptr),
      next_(nullptr),
      first_(nullptr),
      last_(nullptr),
      static_inline_position_(0),
      static_block_position_(0) {
  is_self_painting_layer_ = ShouldBeSelfPaintingLayer();

  UpdateScrollableArea();
}

PaintLayer::~PaintLayer() {
#if DCHECK_IS_ON()
  DCHECK(is_destroyed_);
#endif
}

void PaintLayer::Destroy() {
#if DCHECK_IS_ON()
  DCHECK(!is_destroyed_);
#endif
  if (resource_info_) {
    const ComputedStyle& style = GetLayoutObject().StyleRef();
    if (style.HasFilter())
      style.Filter().RemoveClient(*resource_info_);
    if (style.HasBackdropFilter()) {
      style.BackdropFilter().RemoveClient(*resource_info_);
    }
    if (auto* reference_clip =
            DynamicTo<ReferenceClipPathOperation>(style.ClipPath()))
      reference_clip->RemoveClient(*resource_info_);
    if (auto* reference_offset =
            DynamicTo<ReferenceOffsetPathOperation>(style.OffsetPath())) {
      reference_offset->RemoveClient(*resource_info_);
    }
    resource_info_->ClearLayer();
  }

  // Reset this flag before disposing scrollable_area_ to prevent
  // PaintLayerScrollableArea::WillRemoveScrollbar() from dirtying the z-order
  // list of the stacking context. If this layer is removed from the parent,
  // the z-order list should have been invalidated in RemoveChild().
  needs_reorder_overlay_overflow_controls_ = false;

  if (scrollable_area_)
    scrollable_area_->Dispose();

#if DCHECK_IS_ON()
  is_destroyed_ = true;
#endif
}

String PaintLayer::DebugName() const {
  return GetLayoutObject().DebugName();
}

DOMNodeId PaintLayer::OwnerNodeId() const {
  return static_cast<const DisplayItemClient&>(GetLayoutObject()).OwnerNodeId();
}

bool PaintLayer::PaintsWithFilters() const {
  if (!GetLayoutObject().HasFilterInducingProperty())
    return false;
  return true;
}

const PaintLayer* PaintLayer::ContainingScrollContainerLayer(
    bool* is_fixed_to_view) const {
  bool is_fixed = GetLayoutObject().IsFixedPositioned();
  for (const PaintLayer* container = ContainingLayer(); container;
       container = container->ContainingLayer()) {
    if (container->GetLayoutObject().IsScrollContainer()) {
      if (is_fixed_to_view)
        *is_fixed_to_view = is_fixed && container->IsRootLayer();
      DCHECK(container->GetScrollableArea());
      return container;
    }
    is_fixed = container->GetLayoutObject().IsFixedPositioned();
  }
  DCHECK(IsRootLayer());
  if (is_fixed_to_view)
    *is_fixed_to_view = true;
  return nullptr;
}

void PaintLayer::UpdateTransform() {
  if (gfx::Transform* transform = Transform()) {
    const LayoutBox* box = GetLayoutBox();
    DCHECK(box);
    transform->MakeIdentity();
    const PhysicalRect reference_box = ComputeReferenceBox(*box);
    box->StyleRef().ApplyTransform(
        *transform, box, reference_box,
        ComputedStyle::kIncludeTransformOperations,
        ComputedStyle::kIncludeTransformOrigin,
        ComputedStyle::kIncludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
  }
}

void PaintLayer::UpdateTransformAfterStyleChange(
    StyleDifference diff,
    const ComputedStyle* old_style,
    const ComputedStyle& new_style) {
  // It's possible for the old and new style transform data to be equivalent
  // while HasTransform() differs, as it checks a number of conditions aside
  // from just the matrix, including but not limited to animation state.
  bool had_transform = Transform();
  bool has_transform = GetLayoutObject().HasTransform();
  if (had_transform == has_transform && old_style &&
      !diff.TransformDataChanged()) {
    return;
  }
  bool had_3d_transform = Has3DTransform();

  if (has_transform != had_transform) {
    if (has_transform)
      transform_ = std::make_unique<gfx::Transform>();
    else
      transform_.reset();
  }

  UpdateTransform();

  if (had_3d_transform != Has3DTransform())
    MarkAncestorChainForFlagsUpdate();

  if (LocalFrameView* frame_view = GetLayoutObject().GetDocument().View())
    frame_view->SetNeedsUpdateGeometries();
}

gfx::Transform PaintLayer::CurrentTransform() const {
  if (gfx::Transform* transform = Transform())
    return *transform;
  return gfx::Transform();
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
  DCHECK(flag == kDoesNotNeedDescendantDependentUpdate ||
         !layout_object_->GetDocument()
              .View()
              ->IsUpdatingDescendantDependentFlags());
#endif
  for (PaintLayer* layer = this; layer; layer = layer->Parent()) {
    if (layer->needs_descendant_dependent_flags_update_ &&
        layer->GetLayoutObject().NeedsPaintPropertyUpdate())
      break;
    if (flag == kNeedsDescendantDependentUpdate)
      layer->needs_descendant_dependent_flags_update_ = true;
    layer->GetLayoutObject().SetNeedsPaintPropertyUpdate();
  }
}

void PaintLayer::SetNeedsDescendantDependentFlagsUpdate() {
  for (PaintLayer* layer = this; layer; layer = layer->Parent()) {
    if (layer->needs_descendant_dependent_flags_update_)
      break;
    layer->needs_descendant_dependent_flags_update_ = true;
  }
}

void PaintLayer::UpdateDescendantDependentFlags() {
  if (needs_descendant_dependent_flags_update_) {
    bool old_has_non_isolated_descendant_with_blend_mode =
        has_non_isolated_descendant_with_blend_mode_;
    has_visible_self_painting_descendant_ = false;
    has_non_isolated_descendant_with_blend_mode_ = false;
    has_fixed_position_descendant_ = false;
    has_non_contained_absolute_position_descendant_ = false;
    has_stacked_descendant_in_current_stacking_context_ = false;
    has_self_painting_layer_descendant_ = false;
    descendant_needs_check_position_visibility_ = false;

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

      if ((child->has_visible_content_ && child->IsSelfPaintingLayer()) ||
          child->has_visible_self_painting_descendant_)
        has_visible_self_painting_descendant_ = true;

      has_non_isolated_descendant_with_blend_mode_ |=
          (!child->GetLayoutObject().IsStackingContext() &&
           child->HasNonIsolatedDescendantWithBlendMode()) ||
          child_style.HasBlendMode();

      has_fixed_position_descendant_ |=
          child->HasFixedPositionDescendant() ||
          child_style.GetPosition() == EPosition::kFixed;

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

    // See SetInvisibleForPositionVisibility() for explanation for
    // descendant_needs_check_position_visibility_.
    if (InvisibleForPositionVisibility() &&
        !GetLayoutObject().IsStackingContext() &&
        has_self_painting_layer_descendant_) {
      AncestorStackingContext()->descendant_needs_check_position_visibility_ =
          true;
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
      if (old_visual_rect != GetLayoutObject().VisualOverflowRect()) {
        MarkAncestorChainForFlagsUpdate(kDoesNotNeedDescendantDependentUpdate);
      }
    }
    GetLayoutObject().DeprecatedInvalidateIntersectionObserverCachedRects();
    needs_visual_overflow_recalc_ = false;
  }

  bool previously_has_visible_content = has_visible_content_;
  if (GetLayoutObject().StyleRef().UsedVisibility() == EVisibility::kVisible) {
    has_visible_content_ = true;
  } else {
    // layer may be hidden but still have some visible content, check for this
    has_visible_content_ = false;
    LayoutObject* r = GetLayoutObject().SlowFirstChild();
    while (r) {
      if (r->StyleRef().UsedVisibility() == EVisibility::kVisible &&
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
    // We need to tell layout_object_ to recheck its rect because we pretend
    // that invisible LayoutObjects have 0x0 rects. Changing visibility
    // therefore changes our rect and we need to visit this LayoutObject during
    // the PrePaintTreeWalk.
    layout_object_->SetShouldCheckForPaintInvalidation();
  }

  Update3DTransformedDescendantStatus();
}

void PaintLayer::Update3DTransformedDescendantStatus() {
  has3d_transformed_descendant_ = false;

  // Transformed or preserve-3d descendants can only be in the z-order lists,
  // not in the normal flow list, so we only need to check those.
  PaintLayerPaintOrderIterator iterator(this, kStackedChildren);
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

void PaintLayer::UpdateScrollingAfterLayout() {
  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterLayout();
    LayoutBox* layout_box = GetLayoutBox();
    if (layout_box->ScrollableAreaSizeChanged()) {
      scrollable_area_->VisibleSizeChanged();
      layout_box->SetScrollableAreaSizeChanged(false);
    }
  }
}

PaintLayer* PaintLayer::ContainingLayer() const {
  LayoutObject& layout_object = GetLayoutObject();
  if (layout_object.IsOutOfFlowPositioned()) {
    // In NG, the containing block chain goes directly from a column spanner to
    // the multi-column container. Thus, for an OOF nested inside a spanner, we
    // need to find its containing layer through its containing block to handle
    // this case correctly. Therefore, we technically only need to take this
    // path for OOFs inside an NG spanner. However, doing so for all OOF
    // descendants of a multicol container is reasonable enough.
    if (layout_object.IsInsideFlowThread())
      return SlowContainingLayer(layout_object);
    auto can_contain_this_layer =
        layout_object.IsFixedPositioned()
            ? &LayoutObject::CanContainFixedPositionObjects
            : &LayoutObject::CanContainAbsolutePositionObjects;

    PaintLayer* curr = Parent();
    while (curr && !((&curr->GetLayoutObject())->*can_contain_this_layer)()) {
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

  return SlowContainingLayer(layout_object);
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

void PaintLayer::SetNeedsCompositingInputsUpdate() {
  // TODO(chrishtr): These are a bit of a heavy hammer, because not all
  // things which require compositing inputs update require a descendant-
  // dependent flags update. Reduce call sites after CAP launch allows
  /// removal of CompositingInputsUpdater.
  MarkAncestorChainForFlagsUpdate();
}

void PaintLayer::ScrollContainerStatusChanged() {
  SetNeedsCompositingInputsUpdate();
}

void PaintLayer::SetNeedsVisualOverflowRecalc() {
  DCHECK(IsSelfPaintingLayer());
#if DCHECK_IS_ON()
  GetLayoutObject().InvalidateVisualOverflowForDCheck();
#endif
  needs_visual_overflow_recalc_ = true;
  // |MarkAncestorChainForFlagsUpdate| will cause a paint property update which
  // is only needed if visual overflow actually changes. To avoid this, only
  // mark this as needing a descendant dependent flags update, which will
  // cause a paint property update if needed (see:
  // PaintLayer::UpdateDescendantDependentFlags).
  SetNeedsDescendantDependentFlagsUpdate();
}

bool PaintLayer::HasNonIsolatedDescendantWithBlendMode() const {
  DCHECK(!needs_descendant_dependent_flags_update_);
  if (has_non_isolated_descendant_with_blend_mode_) {
    return true;
  }
  if (GetLayoutObject().IsSVGRoot()) {
    return To<LayoutSVGRoot>(GetLayoutObject())
        .HasNonIsolatedBlendingDescendants();
  }
  return false;
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

  if (child->SelfNeedsRepaint())
    MarkCompositingContainerChainForNeedsRepaint();
  else
    child->SetNeedsRepaint();

  if (child->NeedsCullRectUpdate()) {
    SetDescendantNeedsCullRectUpdate();
  } else {
    child->SetNeedsCullRectUpdate();
  }
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
    // Dirty the z-order list in which we are contained.
    old_child->DirtyStackingContextZOrderLists();
    MarkAncestorChainForFlagsUpdate();
  }

  if (GetLayoutObject().StyleRef().UsedVisibility() != EVisibility::kVisible) {
    DirtyVisibleContentStatus();
  }

  old_child->SetPreviousSibling(nullptr);
  old_child->SetNextSibling(nullptr);
  old_child->parent_ = nullptr;

  if (old_child->has_visible_content_ ||
      old_child->has_visible_self_painting_descendant_)
    MarkAncestorChainForFlagsUpdate();
}

void PaintLayer::RemoveOnlyThisLayerAfterStyleChange(
    const ComputedStyle* old_style) {
  if (!parent_)
    return;

  if (old_style) {
    if (GetLayoutObject().IsStacked(*old_style))
      DirtyStackingContextZOrderLists();

    if (PaintLayerPainter::PaintedOutputInvisible(*old_style)) {
      // PaintedOutputInvisible() was true because opacity was near zero, and
      // this layer is to be removed because opacity becomes 1. Do the same as
      // StyleDidChange() on change of PaintedOutputInvisible().
      GetLayoutObject().SetSubtreeShouldDoFullPaintInvalidation();
    }
  }

  if (IsSelfPaintingLayer()) {
    if (PaintLayer* enclosing_self_painting_layer =
            parent_->EnclosingSelfPaintingLayer())
      enclosing_self_painting_layer->MergeNeedsPaintPhaseFlagsFrom(*this);
  }

  PaintLayer* next_sib = NextSibling();

  // Now walk our kids and reattach them to our parent.
  PaintLayer* current = first_;
  while (current) {
    PaintLayer* next = current->NextSibling();
    RemoveChild(current);
    parent_->AddChild(current, next_sib);
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
    if (needs_stacking_node) {
      stacking_node_ = MakeGarbageCollected<PaintLayerStackingNode>(this);
    } else {
      stacking_node_.Clear();
    }
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
  return false;
}

void PaintLayer::UpdateScrollableArea() {
  if (RequiresScrollableArea() == !!scrollable_area_)
    return;

  if (!scrollable_area_) {
    scrollable_area_ = MakeGarbageCollected<PaintLayerScrollableArea>(*this);
    const ComputedStyle& style = GetLayoutObject().StyleRef();
    // A newly created snap container may need to be made aware of snap areas
    // within it which are targeted or contain a targeted element. Such a
    // container may also change the snap areas associated with snap containers
    // higher in the DOM.
    if (!style.GetScrollSnapType().is_none) {
      if (Element* css_target = GetLayoutObject().GetDocument().CssTarget()) {
        css_target->SetTargetedSnapAreaIdsForSnapContainers();
      }
    }
  } else {
    scrollable_area_->Dispose();
    scrollable_area_.Clear();
  }

  GetLayoutObject().SetNeedsPaintPropertyUpdate();
  // To clear z-ordering information of overlay overflow controls.
  if (NeedsReorderOverlayOverflowControls())
    DirtyStackingContextZOrderLists();
}

void PaintLayer::AppendSingleFragmentForHitTesting(
    PaintLayerFragments& fragments,
    const PaintLayerFragment* container_fragment,
    ShouldRespectOverflowClipType respect_overflow_clip) const {
  PaintLayerFragment fragment;
  if (container_fragment) {
    fragment = *container_fragment;
  } else {
    fragment.fragment_data = &GetLayoutObject().FirstFragment();
    if (GetLayoutObject().CanTraversePhysicalFragments()) {
      // Make sure that we actually traverse the fragment tree, by providing a
      // physical fragment. Otherwise we'd fall back to LayoutObject traversal.
      if (const auto* layout_box = GetLayoutBox())
        fragment.physical_fragment = layout_box->GetPhysicalFragment(0);
    }
    fragment.fragment_idx = 0;
  }

  ClipRectsContext clip_rects_context(this, fragment.fragment_data,
                                      kExcludeOverlayScrollbarSizeForHitTesting,
                                      respect_overflow_clip);
  Clipper().CalculateRects(clip_rects_context, *fragment.fragment_data,
                           fragment.layer_offset, fragment.background_rect,
                           fragment.foreground_rect);

  fragments.push_back(fragment);
}

const LayoutBox* PaintLayer::GetLayoutBoxWithBlockFragments() const {
  const LayoutBox* layout_box = GetLayoutBox();
  if (!layout_box || !layout_box->CanTraversePhysicalFragments()) {
    return nullptr;
  }
  DCHECK(!layout_box->IsFragmentLessBox());
  return layout_box;
}

void PaintLayer::CollectFragments(
    PaintLayerFragments& fragments,
    const PaintLayer* root_layer,
    ShouldRespectOverflowClipType respect_overflow_clip,
    const FragmentData* root_fragment_arg) const {
  PaintLayerFragment fragment;
  const auto& first_root_fragment_data =
      root_layer->GetLayoutObject().FirstFragment();

  const LayoutBox* layout_box_with_fragments = GetLayoutBoxWithBlockFragments();

  // The NG hit-testing code guards against painting multiple fragments for
  // content that doesn't support it, but the legacy hit-testing code has no
  // such guards.
  // TODO(crbug.com/1229581): Remove this when everything is handled by NG.
  bool multiple_fragments_allowed =
      layout_box_with_fragments || CanPaintMultipleFragments(GetLayoutObject());

  // The inherited offset_from_root does not include any pagination offsets.
  // In the presence of fragmentation, we cannot use it.
  wtf_size_t physical_fragment_idx = 0u;
  for (FragmentDataIterator iterator(GetLayoutObject()); !iterator.IsDone();
       ++iterator, physical_fragment_idx++) {
    const FragmentData* fragment_data = iterator.GetFragmentData();
    const FragmentData* root_fragment_data = nullptr;
    if (root_fragment_arg) {
      DCHECK(this != root_layer);
      if (!root_fragment_arg->ContentsProperties().Transform().IsAncestorOf(
              fragment_data->LocalBorderBoxProperties().Transform())) {
        // We only want to collect fragments that are descendants of
        // |root_fragment_arg|.
        continue;
      }
      root_fragment_data = root_fragment_arg;
    } else if (root_layer == this) {
      root_fragment_data = fragment_data;
    } else {
      root_fragment_data = &first_root_fragment_data;
    }

    ClipRectsContext clip_rects_context(
        root_layer, root_fragment_data,
        kExcludeOverlayScrollbarSizeForHitTesting, respect_overflow_clip,
        PhysicalOffset());

    Clipper().CalculateRects(clip_rects_context, *fragment_data,
                             fragment.layer_offset, fragment.background_rect,
                             fragment.foreground_rect);

    fragment.fragment_data = fragment_data;

    if (layout_box_with_fragments) {
      fragment.physical_fragment =
          layout_box_with_fragments->GetPhysicalFragment(physical_fragment_idx);
      DCHECK(fragment.physical_fragment);
    }

    fragment.fragment_idx = physical_fragment_idx;

    fragments.push_back(fragment);

    if (!multiple_fragments_allowed)
      break;
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
  // The root PaintLayer of HitTest must contain all descendants.
  DCHECK(GetLayoutObject().CanContainFixedPositionObjects());
  DCHECK(GetLayoutObject().CanContainAbsolutePositionObjects());

  // LayoutView should make sure to update layout before entering hit testing
  DCHECK(!GetLayoutObject().GetFrame()->View()->LayoutPending());
  DCHECK(!GetLayoutObject().GetDocument().GetLayoutView()->NeedsLayout());

  const HitTestRequest& request = result.GetHitTestRequest();

  HitTestRecursionData recursion_data(hit_test_area, hit_test_location,
                                      hit_test_location);
  PaintLayer* inside_layer = HitTestLayer(*this, /*container_fragment*/ nullptr,
                                          result, recursion_data);
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
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool PaintLayer::IsInTopOrViewTransitionLayer() const {
  return GetLayoutObject().IsInTopOrViewTransitionLayer();
}

// Compute the z-offset of the point in the transformState.
// This is effectively projecting a ray normal to the plane of ancestor, finding
// where that ray intersects target, and computing the z delta between those two
// points.
static double ComputeZOffset(const HitTestingTransformState& transform_state) {
  // We got an affine transform, so no z-offset
  if (transform_state.AccumulatedTransform().Is2dTransform())
    return 0;

  // Flatten the point into the target plane
  gfx::PointF target_point = transform_state.MappedPoint();

  // Now map the point back through the transform, which computes Z.
  gfx::Point3F backmapped_point =
      transform_state.AccumulatedTransform().MapPoint(
          gfx::Point3F(target_point));
  return backmapped_point.z();
}

HitTestingTransformState PaintLayer::CreateLocalTransformState(
    const PaintLayer& transform_container,
    const FragmentData& transform_container_fragment,
    const FragmentData& local_fragment,
    const HitTestRecursionData& recursion_data,
    const HitTestingTransformState* container_transform_state) const {
  // If we're already computing transform state, then it's relative to the
  // container (which we know is non-null).
  // If this is the first time we need to make transform state, then base it
  // off of hitTestLocation, which is relative to rootLayer.
  HitTestingTransformState transform_state =
      container_transform_state
          ? *container_transform_state
          : HitTestingTransformState(
                recursion_data.location.TransformedPoint(),
                recursion_data.location.TransformedRect(),
                gfx::QuadF(gfx::RectF(recursion_data.rect)));

  if (&transform_container == this) {
    DCHECK(!container_transform_state);
    return transform_state;
  }

  if (container_transform_state &&
      (!transform_container.Preserves3D() ||
       &transform_container.GetLayoutObject() !=
           GetLayoutObject().NearestAncestorForElement())) {
    // The transform container layer doesn't preserve 3d, or its preserve-3d
    // doesn't apply to this layer because our element is not a child of the
    // transform container layer's element.
    transform_state.Flatten();
  }

  DCHECK_NE(&transform_container_fragment, &local_fragment);

  const auto* container_transform =
      &transform_container_fragment.LocalBorderBoxProperties().Transform();
  if (const auto* properties = transform_container_fragment.PaintProperties()) {
    if (const auto* perspective = properties->Perspective()) {
      transform_state.ApplyTransform(*perspective);
      container_transform = perspective;
    }
  }

  transform_state.Translate(
      gfx::Vector2dF(-transform_container_fragment.PaintOffset()));
  transform_state.ApplyTransform(GeometryMapper::SourceToDestinationProjection(
      local_fragment.PreTransform(), *container_transform));
  transform_state.Translate(gfx::Vector2dF(local_fragment.PaintOffset()));

  if (const auto* properties = local_fragment.PaintProperties()) {
    for (const TransformPaintPropertyNode* transform :
         properties->AllCSSTransformPropertiesOutsideToInside()) {
      if (transform)
        transform_state.ApplyTransform(*transform);
    }
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
  //
  // See comment in PaintLayer::HitTestLayer regarding SVG
  // foreignObject; if it weren't for that case we could test z_offset
  // and then DCHECK(transform_state) inside of it.
  DCHECK(!z_offset || transform_state ||
         hit_layer->GetLayoutObject().IsSVGForeignObject());
  if (z_offset && transform_state) {
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

// recursion_data.location and rect are relative to |transform_container|.
// A 'flattening' layer is one preserves3D() == false.
// transform_state.AccumulatedTransform() holds the transform from the
// containing flattening layer.
// transform_state.last_planar_point_ is the hit test location in the plane of
// the containing flattening layer.
// transform_state.last_planar_quad_ is the hit test rect as a quad in the
// plane of the containing flattening layer.
//
// If z_offset is non-null (which indicates that the caller wants z offset
// information), *z_offset on return is the z offset of the hit point relative
// to the containing flattening layer.
//
// If |container_fragment| is null, we'll hit test all fragments. Otherwise it
// points to a fragment of |transform_container|, and descendants should hit
// test their fragments that are descendants of |container_fragment|.
PaintLayer* PaintLayer::HitTestLayer(
    const PaintLayer& transform_container,
    const PaintLayerFragment* container_fragment,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    bool applied_transform,
    HitTestingTransformState* container_transform_state,
    double* z_offset,
    bool overflow_controls_only) {
  const FragmentData* container_fragment_data =
      container_fragment ? container_fragment->fragment_data : nullptr;
  const auto& container_layout_object = transform_container.GetLayoutObject();
  DCHECK(container_layout_object.CanContainFixedPositionObjects());
  DCHECK(container_layout_object.CanContainAbsolutePositionObjects());

  const LayoutObject& layout_object = GetLayoutObject();
  DCHECK_GE(layout_object.GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  if (layout_object.NeedsLayout() &&
      !layout_object.ChildLayoutBlockedByDisplayLock()) [[unlikely]] {
    // Skip if we need layout. This should never happen. See crbug.com/1423308
    // and crbug.com/330051489.
    return nullptr;
  }

  if (layout_object.IsFragmentLessBox()) {
    return nullptr;
  }

  if (!IsSelfPaintingLayer() && !HasSelfPaintingLayerDescendant())
    return nullptr;

  if ((result.GetHitTestRequest().GetType() &
       HitTestRequest::kIgnoreZeroOpacityObjects) &&
      !layout_object.HasNonZeroEffectiveOpacity()) {
    return nullptr;
  }

  std::optional<CheckAncestorPositionVisibilityScope>
      check_position_visibility_scope;
  if (InvisibleForPositionVisibility() ||
      HasAncestorInvisibleForPositionVisibility()) {
    return nullptr;
  }
  if (GetLayoutObject().IsStackingContext()) {
    check_position_visibility_scope.emplace(*this);
  }

  // TODO(vmpstr): We need to add a simple document flag which says whether
  // there is an ongoing transition, since this may be too heavy of a check for
  // each hit test.
  if (auto* transition =
          ViewTransitionUtils::GetTransition(layout_object.GetDocument())) {
    // This means that the contents of the object are drawn elsewhere.
    if (transition->IsRepresentedViaPseudoElements(layout_object))
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
      gfx::Point point = scrollable_area_->ConvertFromRootFrameToVisualViewport(
          ToRoundedPoint(recursion_data.location.Point()));

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
  bool use_transform = false;
  if (!layout_object.IsSVGForeignObject() &&
      // Only a layer that can contain all descendants can become a transform
      // container. This excludes layout objects having transform nodes created
      // for animating opacity etc. or for backface-visibility:hidden.
      layout_object.CanContainFixedPositionObjects()) {
    DCHECK(layout_object.CanContainAbsolutePositionObjects());
    if (const auto* properties =
            layout_object.FirstFragment().PaintProperties()) {
      if (properties->HasCSSTransformPropertyNode() ||
          properties->Perspective())
        use_transform = true;
    }
  }

  // Apply a transform if we have one.
  if (use_transform && !applied_transform) {
    return HitTestTransformedLayerInFragments(
        transform_container, container_fragment, result, recursion_data,
        container_transform_state, z_offset, overflow_controls_only,
        clip_behavior);
  }

  // Don't hit test the clip-path area when checking for occlusion. This is
  // necessary because SVG doesn't support rect-based hit testing, so
  // HitTestClippedOutByClipPath may erroneously return true for a rect-based
  // hit test).
  bool is_occlusion_test = result.GetHitTestRequest().GetType() &
                           HitTestRequest::kHitTestVisualOverflow;
  if (!is_occlusion_test && layout_object.HasClipPath() &&
      HitTestClippedOutByClipPath(transform_container,
                                  recursion_data.location)) {
    return nullptr;
  }

  HitTestingTransformState* local_transform_state = nullptr;
  STACK_UNINITIALIZED std::optional<HitTestingTransformState> storage;

  if (applied_transform) {
    // We computed the correct state in the caller (above code), so just
    // reference it.
    DCHECK(container_transform_state);
    local_transform_state = container_transform_state;
  } else if (container_transform_state || has3d_transformed_descendant_) {
    DCHECK(!Preserves3D());
    // We need transform state for the first time, or to offset the container
    // state, so create it here.
    FragmentDataIterator iterator(layout_object);
    const FragmentData* local_fragment_for_transform_state =
        iterator.GetFragmentData();
    const FragmentData* container_fragment_for_transform_state;
    if (container_fragment_data) {
      container_fragment_for_transform_state = container_fragment_data;
      const auto& container_transform =
          container_fragment_data->ContentsProperties().Transform();
      while (!iterator.IsDone()) {
        // Find the first local fragment that is a descendant of
        // container_fragment.
        if (container_transform.IsAncestorOf(
                local_fragment_for_transform_state->LocalBorderBoxProperties()
                    .Transform())) {
          break;
        }
        ++iterator;
        local_fragment_for_transform_state = iterator.GetFragmentData();
      }
      if (!local_fragment_for_transform_state)
        return nullptr;
    } else {
      container_fragment_for_transform_state =
          &container_layout_object.FirstFragment();
    }
    storage = CreateLocalTransformState(
        transform_container, *container_fragment_for_transform_state,
        *local_fragment_for_transform_state, recursion_data,
        container_transform_state);
    local_transform_state = &*storage;
  }

  // Check for hit test on backface if backface-visibility is 'hidden'
  if (local_transform_state &&
      layout_object.StyleRef().BackfaceVisibility() ==
          EBackfaceVisibility::kHidden &&
      local_transform_state->AccumulatedTransform().IsBackFaceVisible()) {
    return nullptr;
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
  PaintLayerFragments layer_fragments;
  ClearCollectionScope<PaintLayerFragments> scope(&layer_fragments);
  if (recursion_data.intersects_location) {
    if (applied_transform) {
      DCHECK_EQ(&transform_container, this);
      AppendSingleFragmentForHitTesting(layer_fragments, container_fragment,
                                        clip_behavior);
    } else {
      CollectFragments(layer_fragments, &transform_container, clip_behavior,
                       container_fragment_data);
    }

    // See if the hit test pos is inside the overflow controls of current layer.
    // This should be done before walking child layers to avoid that the
    // overflow controls are obscured by the positive child layers.
    if (scrollable_area_ &&
        layer_fragments[0].background_rect.Intersects(
            recursion_data.location) &&
        GetLayoutBox()->HitTestOverflowControl(
            result, recursion_data.location, layer_fragments[0].layer_offset)) {
      return this;
    }
  }

  if (overflow_controls_only)
    return nullptr;

  // This variable tracks which layer the mouse ends up being inside.
  PaintLayer* candidate_layer = nullptr;

  // Begin by walking our list of positive layers from highest z-index down to
  // the lowest z-index.
  PaintLayer* hit_layer = HitTestChildren(
      kPositiveZOrderChildren, transform_container, container_fragment, result,
      recursion_data, container_transform_state, z_offset_for_descendants_ptr,
      z_offset, local_transform_state, depth_sort_descendants);
  if (hit_layer) {
    if (!depth_sort_descendants)
      return hit_layer;
    candidate_layer = hit_layer;
  }

  // Now check our overflow objects.
  hit_layer = HitTestChildren(
      kNormalFlowChildren, transform_container, container_fragment, result,
      recursion_data, container_transform_state, z_offset_for_descendants_ptr,
      z_offset, local_transform_state, depth_sort_descendants);
  if (hit_layer) {
    if (!depth_sort_descendants)
      return hit_layer;
    candidate_layer = hit_layer;
  }

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  if (recursion_data.intersects_location) {
    // Next we want to see if the mouse pos is inside the child LayoutObjects of
    // the layer. Check every fragment in reverse order.
    if (IsSelfPaintingLayer() &&
        !layout_object.ChildPaintBlockedByDisplayLock()) {
      // Hit test with a temporary HitTestResult, because we only want to commit
      // to 'result' if we know we're frontmost.
      STACK_UNINITIALIZED HitTestResult temp_result(
          result.GetHitTestRequest(), recursion_data.original_location);
      bool inside_fragment_foreground_rect = false;

      if (HitTestForegroundForFragments(layer_fragments, temp_result,
                                        recursion_data.location,
                                        inside_fragment_foreground_rect) &&
          IsHitCandidateForDepthOrder(this, false, z_offset_for_contents_ptr,
                                      local_transform_state) &&
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
      }
    }
  }

  // Now check our negative z-index children.
  hit_layer = HitTestChildren(
      kNegativeZOrderChildren, transform_container, container_fragment, result,
      recursion_data, container_transform_state, z_offset_for_descendants_ptr,
      z_offset, local_transform_state, depth_sort_descendants);
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
    bool inside_fragment_background_rect = false;
    if (HitTestFragmentsWithPhase(layer_fragments, temp_result,
                                  recursion_data.location,
                                  HitTestPhase::kSelfBlockBackground,
                                  inside_fragment_background_rect) &&
        IsHitCandidateForDepthOrder(this, false, z_offset_for_contents_ptr,
                                    local_transform_state) &&
        IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
      if (result.GetHitTestRequest().ListBased())
        result.Append(temp_result);
      else
        result = temp_result;
      return this;
    }
    if (inside_fragment_background_rect &&
        result.GetHitTestRequest().ListBased() &&
        IsHitCandidateForStopNode(GetLayoutObject(), stop_node)) {
      result.Append(temp_result);
    }
  }

  return nullptr;
}

bool PaintLayer::HitTestForegroundForFragments(
    const PaintLayerFragments& layer_fragments,
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    bool& inside_clip_rect) const {
  if (HitTestFragmentsWithPhase(layer_fragments, result, hit_test_location,
                                HitTestPhase::kForeground, inside_clip_rect)) {
    return true;
  }
  if (inside_clip_rect &&
      HitTestFragmentsWithPhase(layer_fragments, result, hit_test_location,
                                HitTestPhase::kFloat, inside_clip_rect)) {
    return true;
  }
  if (inside_clip_rect &&
      HitTestFragmentsWithPhase(layer_fragments, result, hit_test_location,
                                HitTestPhase::kDescendantBlockBackgrounds,
                                inside_clip_rect)) {
    return true;
  }
  return false;
}

bool PaintLayer::HitTestFragmentsWithPhase(
    const PaintLayerFragments& layer_fragments,
    HitTestResult& result,
    const HitTestLocation& hit_test_location,
    HitTestPhase phase,
    bool& inside_clip_rect) const {
  if (layer_fragments.empty())
    return false;

  for (int i = layer_fragments.size() - 1; i >= 0; --i) {
    const PaintLayerFragment& fragment = layer_fragments.at(i);
    const ClipRect& bounds = phase == HitTestPhase::kSelfBlockBackground
                                 ? fragment.background_rect
                                 : fragment.foreground_rect;
    if (!bounds.Intersects(hit_test_location))
      continue;

    inside_clip_rect = true;

    if (GetLayoutObject().IsLayoutInline() &&
        GetLayoutObject().CanTraversePhysicalFragments()) [[unlikely]] {
      // When hit-testing an inline that has a layer, we'll search for it in
      // each fragment of the containing block. Each fragment has its own
      // offset, and we need to do one fragment at a time. If the inline uses a
      // transform, though, we'll only have one PaintLayerFragment in the list
      // at this point (we iterate over them further up on the stack, and pass a
      // "list" of one fragment at a time from there instead).
      DCHECK(fragment.fragment_idx != WTF::kNotFound);
      HitTestLocation location_for_fragment(hit_test_location,
                                            fragment.fragment_idx);
      if (HitTestFragmentWithPhase(result, fragment.physical_fragment,
                                   fragment.layer_offset, location_for_fragment,
                                   phase))
        return true;
    } else if (HitTestFragmentWithPhase(result, fragment.physical_fragment,
                                        fragment.layer_offset,
                                        hit_test_location, phase)) {
      return true;
    }
  }

  return false;
}

PaintLayer* PaintLayer::HitTestTransformedLayerInFragments(
    const PaintLayer& transform_container,
    const PaintLayerFragment* container_fragment,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* container_transform_state,
    double* z_offset,
    bool overflow_controls_only,
    ShouldRespectOverflowClipType clip_behavior) {
  const FragmentData* container_fragment_data =
      container_fragment ? container_fragment->fragment_data : nullptr;
  PaintLayerFragments fragments;
  ClearCollectionScope<PaintLayerFragments> scope(&fragments);

  CollectFragments(fragments, &transform_container, clip_behavior,
                   container_fragment_data);

  for (const auto& fragment : fragments) {
    // Apply any clips established by layers in between us and the root layer.
    if (!fragment.background_rect.Intersects(recursion_data.location))
      continue;

    PaintLayer* hit_layer = HitTestLayerByApplyingTransform(
        transform_container, container_fragment, fragment, result,
        recursion_data, container_transform_state, z_offset,
        overflow_controls_only);
    if (hit_layer)
      return hit_layer;
  }

  return nullptr;
}

PaintLayer* PaintLayer::HitTestLayerByApplyingTransform(
    const PaintLayer& transform_container,
    const PaintLayerFragment* container_fragment,
    const PaintLayerFragment& local_fragment,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* root_transform_state,
    double* z_offset,
    bool overflow_controls_only,
    const PhysicalOffset& translation_offset) {
  // Create a transform state to accumulate this transform.
  HitTestingTransformState new_transform_state = CreateLocalTransformState(
      transform_container,
      container_fragment
          ? *container_fragment->fragment_data
          : transform_container.GetLayoutObject().FirstFragment(),
      *local_fragment.fragment_data, recursion_data, root_transform_state);

  // If the transform can't be inverted, then don't hit test this layer at all.
  if (!new_transform_state.AccumulatedTransform().IsInvertible())
    return nullptr;

  // Compute the point and the hit test rect in the coords of this layer by
  // using the values from new_transform_state, which store the point and quad
  // in the coords of the last flattened layer, and the accumulated transform
  // which lets up map through preserve-3d layers.
  //
  // We can't just map HitTestLocation and HitTestRect because they may have
  // been flattened (losing z) by our container.
  gfx::PointF local_point = new_transform_state.MappedPoint();
  PhysicalRect bounds_of_mapped_area = new_transform_state.BoundsOfMappedArea();
  std::optional<HitTestLocation> new_location;
  if (recursion_data.location.IsRectBasedTest())
    new_location.emplace(local_point, new_transform_state.MappedQuad());
  else
    new_location.emplace(local_point, new_transform_state.BoundsOfMappedQuad());
  HitTestRecursionData new_recursion_data(bounds_of_mapped_area, *new_location,
                                          recursion_data.original_location);

  // Now do a hit test with the transform container shifted to this layer.
  // As an optimization, pass nullptr as the new container_fragment if this
  // layer has only one fragment.
  const auto* new_container_fragment =
      GetLayoutObject().IsFragmented() ? &local_fragment : nullptr;
  return HitTestLayer(*this, new_container_fragment, result, new_recursion_data,
                      /*applied_transform*/ true, &new_transform_state,
                      z_offset, overflow_controls_only);
}

bool PaintLayer::HitTestFragmentWithPhase(
    HitTestResult& result,
    const PhysicalBoxFragment* physical_fragment,
    const PhysicalOffset& fragment_offset,
    const HitTestLocation& hit_test_location,
    HitTestPhase phase) const {
  DCHECK(IsSelfPaintingLayer() || HasSelfPaintingLayerDescendant());

  bool did_hit;
  if (physical_fragment) {
    if (!physical_fragment->MayIntersect(result, hit_test_location,
                                         fragment_offset)) {
      did_hit = false;
    } else {
      did_hit =
          BoxFragmentPainter(*physical_fragment)
              .NodeAtPoint(result, hit_test_location, fragment_offset, phase);
    }
  } else {
    did_hit = GetLayoutObject().NodeAtPoint(result, hit_test_location,
                                            fragment_offset, phase);
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

PaintLayer* PaintLayer::HitTestChildren(
    PaintLayerIteration children_to_visit,
    const PaintLayer& transform_container,
    const PaintLayerFragment* container_fragment,
    HitTestResult& result,
    const HitTestRecursionData& recursion_data,
    HitTestingTransformState* container_transform_state,
    double* z_offset_for_descendants,
    double* z_offset,
    HitTestingTransformState* local_transform_state,
    bool depth_sort_descendants) {
  if (!HasSelfPaintingLayerDescendant())
    return nullptr;

  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return nullptr;

  const LayoutObject* stop_node = result.GetHitTestRequest().GetStopNode();
  PaintLayer* stop_layer = stop_node ? stop_node->PaintingLayer() : nullptr;

  PaintLayer* result_layer = nullptr;
  PaintLayerPaintOrderReverseIterator iterator(this, children_to_visit);

  // Returns true if the caller should break the loop.
  auto hit_test_child = [&](PaintLayer* child_layer,
                            bool overflow_controls_only) -> bool {
    if (child_layer->IsReplacedNormalFlowStacking())
      return false;

    // Avoid the call to child_layer.HitTestLayer() if possible.
    if (stop_layer == this &&
        !IsHitCandidateForStopNode(child_layer->GetLayoutObject(), stop_node)) {
      return false;
    }

    STACK_UNINITIALIZED HitTestResult temp_result(
        result.GetHitTestRequest(), recursion_data.original_location);
    PaintLayer* hit_layer = child_layer->HitTestLayer(
        transform_container, container_fragment, temp_result, recursion_data,
        /*applied_transform*/ false, container_transform_state,
        z_offset_for_descendants, overflow_controls_only);

    // If it is a list-based test, we can safely append the temporary result
    // since it might had hit nodes but not necessarily had hit_layer set.
    if (result.GetHitTestRequest().ListBased()) {
      result.Append(temp_result);
    }

    if (IsHitCandidateForDepthOrder(hit_layer, depth_sort_descendants, z_offset,
                                    local_transform_state)) {
      result_layer = hit_layer;
      if (!result.GetHitTestRequest().ListBased())
        result = temp_result;
      if (!depth_sort_descendants) {
        return true;
      }
    }
    return false;
  };

  while (PaintLayer* child_layer = iterator.Next()) {
    if (stacking_node_) {
      if (const auto* layers_painting_overlay_overflow_controls_after =
              stacking_node_->LayersPaintingOverlayOverflowControlsAfter(
                  child_layer)) {
        bool break_loop = false;
        for (auto& reparent_overflow_controls_layer :
             base::Reversed(*layers_painting_overlay_overflow_controls_after)) {
          DCHECK(reparent_overflow_controls_layer
                     ->NeedsReorderOverlayOverflowControls());
          if (hit_test_child(reparent_overflow_controls_layer, true)) {
            break_loop = true;
            break;
          }
        }
        if (break_loop) {
          break;
        }
      }
    }

    if (hit_test_child(child_layer, false)) {
      break;
    }
  }

  return result_layer;
}

void PaintLayer::UpdateFilterReferenceBox() {
  if (!HasFilterThatMovesPixels())
    return;
  gfx::RectF reference_box(LocalBoundingBoxIncludingSelfPaintingDescendants());
  std::optional<gfx::SizeF> viewport(ComputeFilterViewport(*this));
  if (!ResourceInfo() ||
      ResourceInfo()->FilterReferenceBox() != reference_box ||
      ResourceInfo()->FilterViewport() != viewport) {
    if (GetLayoutObject().GetDocument().Lifecycle().GetState() ==
        DocumentLifecycle::kInPrePaint) {
      GetLayoutObject()
          .GetMutableForPainting()
          .SetOnlyThisNeedsPaintPropertyUpdate();
    } else {
      GetLayoutObject().SetNeedsPaintPropertyUpdate();
    }
    if (ResourceInfo() && ResourceInfo()->FilterViewport() != viewport) {
      filter_on_effect_node_dirty_ = true;
    }
  }
  auto& resource_info = EnsureResourceInfo();
  resource_info.SetFilterReferenceBox(reference_box);
  resource_info.SetFilterViewport(viewport);
}

gfx::RectF PaintLayer::FilterReferenceBox() const {
#if DCHECK_IS_ON()
  DCHECK_GE(GetLayoutObject().GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
#endif
  if (ResourceInfo())
    return ResourceInfo()->FilterReferenceBox();
  return gfx::RectF();
}

std::optional<gfx::SizeF> PaintLayer::FilterViewport() const {
  DCHECK_GE(GetLayoutObject().GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kInPrePaint);
  if (ResourceInfo()) {
    return ResourceInfo()->FilterViewport();
  }
  return std::nullopt;
}

gfx::RectF PaintLayer::BackdropFilterReferenceBox() const {
  if (const auto* layout_inline = DynamicTo<LayoutInline>(GetLayoutObject())) {
    return gfx::RectF(
        gfx::SizeF(layout_inline->PhysicalLinesBoundingBox().size));
  }
  return gfx::RectF(GetLayoutBox()->PhysicalBorderBoxRect());
}

gfx::RRectF PaintLayer::BackdropFilterBounds() const {
  gfx::RRectF backdrop_filter_bounds(
      SkRRect(RoundedBorderGeometry::PixelSnappedRoundedBorder(
          GetLayoutObject().StyleRef(),
          PhysicalRect::EnclosingRect(BackdropFilterReferenceBox()))));
  return backdrop_filter_bounds;
}

bool PaintLayer::HitTestClippedOutByClipPath(
    const PaintLayer& root_layer,
    const HitTestLocation& hit_test_location) const {
  // TODO(crbug.com/1270522): Support LayoutNGBlockFragmentation.
  DCHECK(GetLayoutObject().HasClipPath());
  DCHECK(IsSelfPaintingLayer());

  PhysicalOffset origin = GetLayoutObject().LocalToAncestorPoint(
      PhysicalOffset(), &root_layer.GetLayoutObject());

  const HitTestLocation location_in_layer(hit_test_location, -origin);
  return !ClipPathClipper::HitTest(GetLayoutObject(), location_in_layer);
}

PhysicalRect PaintLayer::LocalBoundingBox() const {
  PhysicalRect rect = GetLayoutObject().VisualOverflowRect();
  if (GetLayoutObject().IsEffectiveRootScroller() || IsRootLayer()) {
    rect.Unite(
        PhysicalRect(rect.offset, GetLayoutObject().View()->ViewRect().size));
  }
  return rect;
}

void PaintLayer::ExpandRectForSelfPaintingDescendants(
    PhysicalRect& result) const {
  // If we're locked, then the subtree does not contribute painted output.
  // Furthermore, we might not have up-to-date sizing and position information
  // in the subtree, so skip recursing into the subtree.
  if (GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return;

  DCHECK_EQ(result, LocalBoundingBox());
  // The input |result| is based on LayoutObject::PhysicalVisualOverflowRect()
  // which already includes bounds non-self-painting descendants.
  if (!HasSelfPaintingLayerDescendant())
    return;

  // If the layer is known to clip the whole subtree, then we don't need to
  // expand for children. The clip of the current layer is always applied.
  if (KnownToClipSubtreeToPaddingBox())
    return;

  PaintLayerPaintOrderIterator iterator(this, kAllChildren);
  while (PaintLayer* child_layer = iterator.Next()) {
    if (!child_layer->IsSelfPaintingLayer())
      continue;

    PhysicalRect added_rect = child_layer->LocalBoundingBox();
    child_layer->ExpandRectForSelfPaintingDescendants(added_rect);

    // Only enlarge by the filter outsets if we know the filter is going to be
    // rendered in software.  Accelerated filters will handle their own outsets.
    if (child_layer->PaintsWithFilters())
      added_rect = child_layer->MapRectForFilter(added_rect);

    if (child_layer->Transform()) {
      added_rect = PhysicalRect::EnclosingRect(
          child_layer->Transform()->MapRect(gfx::RectF(added_rect)));
    }

    PhysicalOffset delta = child_layer->GetLayoutObject().LocalToAncestorPoint(
        PhysicalOffset(), &GetLayoutObject(), kIgnoreTransforms);
    added_rect.Move(delta);

    result.Unite(added_rect);
  }
}

bool PaintLayer::KnownToClipSubtreeToPaddingBox() const {
  if (const auto* box = GetLayoutBox()) {
    if (!box->ShouldClipOverflowAlongBothAxis())
      return false;
    if (HasNonContainedAbsolutePositionDescendant())
      return false;
    if (HasFixedPositionDescendant() && !box->CanContainFixedPositionObjects())
      return false;
    if (box->StyleRef().OverflowClipMargin())
      return false;
    // The root frame's clip is special at least in Android WebView.
    if (is_root_layer_ && box->GetFrame()->IsLocalRoot())
      return false;
    return true;
  }
  return false;
}

PhysicalRect PaintLayer::LocalBoundingBoxIncludingSelfPaintingDescendants()
    const {
  PhysicalRect result = LocalBoundingBox();
  ExpandRectForSelfPaintingDescendants(result);
  return result;
}

bool PaintLayer::SupportsSubsequenceCaching() const {
  if (const LayoutBox* box = GetLayoutBox()) {
    // TODO(crbug.com/1253797): Revisit this when implementing correct paint
    // order of fragmented stacking contexts.
    if (box->PhysicalFragmentCount() > 1)
      return false;

    // SVG root and SVG foreign object paint atomically.
    if (box->IsSVGRoot() || box->IsSVGForeignObject()) {
      return true;
    }

    // Don't create subsequence for the document element because the subsequence
    // for LayoutView serves the same purpose. This can avoid unnecessary paint
    // chunks that would otherwise be forced by the subsequence.
    if (box->IsDocumentElement())
      return false;
  }

  // Create subsequence for only stacked objects whose paintings are atomic.
  return GetLayoutObject().IsStacked();
}

bool PaintLayer::ShouldBeSelfPaintingLayer() const {
  return GetLayoutObject().LayerTypeRequired() == kNormalPaintLayer;
}

void PaintLayer::UpdateSelfPaintingLayer() {
  bool is_self_painting_layer = ShouldBeSelfPaintingLayer();
  if (IsSelfPaintingLayer() == is_self_painting_layer)
    return;

  // Invalidate the old subsequences which may no longer contain some
  // descendants of this layer because of the self painting status change.
  SetNeedsRepaint();
  is_self_painting_layer_ = is_self_painting_layer;
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

void PaintLayer::UpdateFilters(StyleDifference diff,
                               const ComputedStyle* old_style,
                               const ComputedStyle& new_style) {
  if (!filter_on_effect_node_dirty_) {
    filter_on_effect_node_dirty_ = old_style
                                       ? diff.FilterChanged()
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
        old_style ? old_style->BackdropFilter() != new_style.BackdropFilter()
                  : new_style.HasBackdropFilter();
  }

  if (!new_style.HasBackdropFilter() &&
      (!old_style || !old_style->HasBackdropFilter())) {
    return;
  }

  const bool had_resource_info = ResourceInfo();
  if (new_style.HasBackdropFilter()) {
    new_style.BackdropFilter().AddClient(EnsureResourceInfo());
  }
  if (had_resource_info && old_style) {
    old_style->BackdropFilter().RemoveClient(*ResourceInfo());
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

void PaintLayer::UpdateOffsetPath(const ComputedStyle* old_style,
                                  const ComputedStyle& new_style) {
  OffsetPathOperation* new_offset = new_style.OffsetPath();
  OffsetPathOperation* old_offset =
      old_style ? old_style->OffsetPath() : nullptr;
  if (!new_offset && !old_offset) {
    return;
  }
  const bool had_resource_info = ResourceInfo();
  if (auto* reference_offset =
          DynamicTo<ReferenceOffsetPathOperation>(new_offset)) {
    reference_offset->AddClient(EnsureResourceInfo());
  }
  if (had_resource_info) {
    if (auto* old_reference_offset =
            DynamicTo<ReferenceOffsetPathOperation>(old_offset)) {
      old_reference_offset->RemoveClient(*ResourceInfo());
    }
  }
}

void PaintLayer::StyleDidChange(StyleDifference diff,
                                const ComputedStyle* old_style) {
  UpdateScrollableArea();

  bool had_filter_that_moves_pixels = has_filter_that_moves_pixels_;
  has_filter_that_moves_pixels_ = ComputeHasFilterThatMovesPixels();
  if (had_filter_that_moves_pixels != has_filter_that_moves_pixels_) {
    // The compositor cannot easily track the filters applied within a layer
    // (i.e. composited filters) and is unable to expand the damage rect.
    // Force paint invalidation to update any potentially affected animations.
    // See |CompositorMayHaveIncorrectDamageRect|.
    GetLayoutObject().SetSubtreeShouldDoFullPaintInvalidation();
  }

  if (PaintLayerStackingNode::StyleDidChange(*this, old_style)) {
    // The compositing container (see: |PaintLayer::CompositingContainer()|) may
    // have changed so we need to ensure |descendant_needs_repaint_| is
    // propagated up the new compositing chain.
    if (SelfOrDescendantNeedsRepaint())
      MarkCompositingContainerChainForNeedsRepaint();

    MarkAncestorChainForFlagsUpdate();
  }

  if (RequiresScrollableArea()) {
    DCHECK(scrollable_area_);
    scrollable_area_->UpdateAfterStyleChange(old_style);
  }

  // Overlay scrollbars can make this layer self-painting so we need
  // to recompute the bit once scrollbars have been updated.
  UpdateSelfPaintingLayer();

  // A scroller that changes background color might become opaque or not
  // opaque, which in turn affects whether it can be composited on low-DPI
  // screens.
  if (GetScrollableArea() && GetScrollableArea()->ScrollsOverflow() &&
      diff.HasDifference()) {
    MarkAncestorChainForFlagsUpdate();
  }

  bool needs_full_transform_update = diff.TransformChanged();
  if (needs_full_transform_update) {
    // If only the transform property changed, without other related properties
    // changing, try to schedule a deferred transform node update.
    if (!diff.OtherTransformPropertyChanged() &&
        PaintPropertyTreeBuilder::ScheduleDeferredTransformNodeUpdate(
            GetLayoutObject())) {
      needs_full_transform_update = false;
      SetNeedsDescendantDependentFlagsUpdate();
    }
  }

  bool needs_full_opacity_update = diff.OpacityChanged();
  if (needs_full_opacity_update) {
    if (PaintPropertyTreeBuilder::ScheduleDeferredOpacityNodeUpdate(
            GetLayoutObject())) {
      needs_full_opacity_update = false;
      SetNeedsDescendantDependentFlagsUpdate();
    }
  }

  // See also |LayoutObject::SetStyle| which handles these invalidations if a
  // PaintLayer is not present.
  if (needs_full_transform_update || needs_full_opacity_update ||
      diff.ZIndexChanged() || diff.FilterChanged() || diff.CssClipChanged() ||
      diff.BlendModeChanged() || diff.MaskChanged() ||
      diff.CompositingReasonsChanged()) {
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
    MarkAncestorChainForFlagsUpdate();
  }

  // HasNonContainedAbsolutePositionDescendant depends on position changes.
  const ComputedStyle& new_style = GetLayoutObject().StyleRef();
  if (!old_style || old_style->GetPosition() != new_style.GetPosition())
    MarkAncestorChainForFlagsUpdate();

  UpdateTransformAfterStyleChange(diff, old_style, new_style);
  UpdateFilters(diff, old_style, new_style);
  UpdateBackdropFilters(old_style, new_style);
  UpdateClipPath(old_style, new_style);
  UpdateOffsetPath(old_style, new_style);

  if (diff.ZIndexChanged()) {
    // We don't need to invalidate paint of objects when paint order
    // changes. However, we do need to repaint the containing stacking
    // context, in order to generate new paint chunks in the correct order.
    // Raster invalidation will be issued if needed during paint.
    if (auto* stacking_context = AncestorStackingContext())
      stacking_context->SetNeedsRepaint();
  }

  if (old_style) {
    bool new_painted_output_invisible =
        PaintLayerPainter::PaintedOutputInvisible(new_style);
    if (PaintLayerPainter::PaintedOutputInvisible(*old_style) !=
        new_painted_output_invisible) {
      // Force repaint of the subtree for two purposes:
      // 1. To ensure FCP/LCP will be reported. See crbug.com/1184903.
      // 2. To update effectively_invisible flags of PaintChunks.
      // TODO(crbug.com/1104218): Optimize this.
      GetLayoutObject().SetSubtreeShouldDoFullPaintInvalidation();
    }
  }
}

gfx::Vector2d PaintLayer::PixelSnappedScrolledContentOffset() const {
  if (GetLayoutObject().IsScrollContainer())
    return GetLayoutBox()->PixelSnappedScrolledContentOffset();
  return gfx::Vector2d();
}

PaintLayerClipper PaintLayer::Clipper() const {
  return PaintLayerClipper(this);
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
    CompositorFilterOperations& operations) {
  auto filter = FilterOperationsIncludingReflection();
  gfx::RectF reference_box = FilterReferenceBox();

  // CompositorFilter needs the reference box to be unzoomed.
  const ComputedStyle& style = GetLayoutObject().StyleRef();
  float zoom = style.EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(1 / zoom);

  // Use the existing |operations| if there is no change.
  if (!operations.IsEmpty() && !filter_on_effect_node_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;

  operations =
      FilterEffectBuilder(reference_box, FilterViewport(), zoom,
                          style.VisitedDependentColor(GetCSSPropertyColor()),
                          style.UsedColorScheme())
          .BuildFilterOperations(filter);
  filter_on_effect_node_dirty_ = false;
}

void PaintLayer::UpdateCompositorFilterOperationsForBackdropFilter(
    CompositorFilterOperations& operations,
    gfx::RRectF& backdrop_filter_bounds) {
  const auto& style = GetLayoutObject().StyleRef();
  if (style.BackdropFilter().IsEmpty()) {
    operations.Clear();
    backdrop_filter_on_effect_node_dirty_ = false;
    return;
  }

  gfx::RectF reference_box = BackdropFilterReferenceBox();
  backdrop_filter_bounds = BackdropFilterBounds();
  // CompositorFilter needs the reference box to be unzoomed.
  float zoom = style.EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(1 / zoom);

  // Use the existing |operations| if there is no change.
  if (!operations.IsEmpty() && !backdrop_filter_on_effect_node_dirty_ &&
      reference_box == operations.ReferenceBox())
    return;

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
  // NOTE: Backdrop filters will have their input cropped to the their layer
  // bounds with a mirror edge mode, but this is the responsibility of the
  // compositor to apply, regardless of the actual filter operations added here.
  operations =
      FilterEffectBuilder(reference_box, FilterViewport(), zoom,
                          style.VisitedDependentColor(GetCSSPropertyColor()),
                          style.UsedColorScheme(), nullptr, nullptr)
          .BuildFilterOperations(filter_operations);
  // Note that |operations| may be empty here, if the |filter_operations| list
  // contains only invalid filters (e.g. invalid reference filters). See
  // https://crbug.com/983157 for details.
  backdrop_filter_on_effect_node_dirty_ = false;
}

PaintLayerResourceInfo& PaintLayer::EnsureResourceInfo() {
  if (!resource_info_) {
    resource_info_ = MakeGarbageCollected<PaintLayerResourceInfo>(this);
  }
  return *resource_info_;
}

void PaintLayer::SetNeedsReorderOverlayOverflowControls(bool b) {
  if (b != needs_reorder_overlay_overflow_controls_) {
    SetNeedsRepaint();
    needs_reorder_overlay_overflow_controls_ = b;
  }
}

gfx::RectF PaintLayer::MapRectForFilter(const gfx::RectF& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;
  return FilterOperationsIncludingReflection().MapRect(rect);
}

PhysicalRect PaintLayer::MapRectForFilter(const PhysicalRect& rect) const {
  if (!HasFilterThatMovesPixels())
    return rect;
  return PhysicalRect::EnclosingRect(MapRectForFilter(gfx::RectF(rect)));
}

bool PaintLayer::ComputeHasFilterThatMovesPixels() const {
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
  if (self_needs_repaint_)
    return;
  self_needs_repaint_ = true;
  // Invalidate as a display item client.
  static_cast<DisplayItemClient*>(this)->Invalidate();
  MarkCompositingContainerChainForNeedsRepaint();
}

void PaintLayer::SetDescendantNeedsRepaint() {
  if (descendant_needs_repaint_)
    return;
  descendant_needs_repaint_ = true;
  MarkCompositingContainerChainForNeedsRepaint();
}

void PaintLayer::MarkCompositingContainerChainForNeedsRepaint() {
  PaintLayer* layer = this;
  while (true) {
    // For a non-self-painting layer having self-painting descendant, the
    // descendant will be painted through this layer's Parent() instead of
    // this layer's Container(), so in addition to the CompositingContainer()
    // chain, we also need to mark NeedsRepaint for Parent().
    // TODO(crbug.com/828103): clean up this.
    if (layer->Parent() && !layer->IsSelfPaintingLayer())
      layer->Parent()->SetNeedsRepaint();

    // Don't mark across frame boundary here. LocalFrameView::PaintTree() will
    // propagate child frame NeedsRepaint flag into the owning frame.
    PaintLayer* container = layer->CompositingContainer();
    if (!container || container->descendant_needs_repaint_)
      break;

    // If the layer doesn't need painting itself (which means we're propagating
    // a bit from its children) and it blocks child painting via display lock,
    // then stop propagating the dirty bit.
    if (!layer->SelfNeedsRepaint() &&
        layer->GetLayoutObject().ChildPaintBlockedByDisplayLock())
      break;

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

void PaintLayer::SetNeedsCullRectUpdate() {
  if (needs_cull_rect_update_)
    return;
  needs_cull_rect_update_ = true;
  if (Parent()) {
    Parent()->SetDescendantNeedsCullRectUpdate();
  }
}

void PaintLayer::SetForcesChildrenCullRectUpdate() {
  if (forces_children_cull_rect_update_)
    return;
  forces_children_cull_rect_update_ = true;
  descendant_needs_cull_rect_update_ = true;
  if (Parent()) {
    Parent()->SetDescendantNeedsCullRectUpdate();
  }
}

void PaintLayer::SetDescendantNeedsCullRectUpdate() {
  for (auto* layer = this; layer; layer = layer->Parent()) {
    if (layer->descendant_needs_cull_rect_update_)
      break;
    layer->descendant_needs_cull_rect_update_ = true;
    // Only propagate the dirty bit up to the display locked ancestor.
    if (layer->GetLayoutObject().ChildPrePaintBlockedByDisplayLock())
      break;
  }
}

void PaintLayer::DirtyStackingContextZOrderLists() {
  auto* stacking_context = AncestorStackingContext();
  if (!stacking_context)
    return;
  if (stacking_context->StackingNode())
    stacking_context->StackingNode()->DirtyZOrderLists();

  MarkAncestorChainForFlagsUpdate();
}

void PaintLayer::SetPreviousPaintResult(PaintResult result) {
  if (CullRectUpdater::IsOverridingCullRects())
    return;
  previous_paint_result_ = static_cast<unsigned>(result);
  DCHECK(previous_paint_result_ == static_cast<unsigned>(result));
}

void PaintLayer::SetInvisibleForPositionVisibility(
    LayerPositionVisibility visibility,
    bool invisible) {
  bool already_invisible = InvisibleForPositionVisibility();
  if (invisible) {
    invisible_for_position_visibility_ |= static_cast<int>(visibility);
    // This will fail if subtree_invisible_for_position_visibility_ doesn't
    // have enough bits.
    CHECK(InvisibleForPositionVisibility());
  } else {
    invisible_for_position_visibility_ &= ~static_cast<int>(visibility);
  }
  if (InvisibleForPositionVisibility() != already_invisible) {
    SetNeedsRepaint();
    // If this layer is not a stacking context, during paint, self-painting
    // descendants need to check their ancestor chain to know if they need to
    // hide due to the position visibility hidden flag on this layer.
    if (!already_invisible && !GetLayoutObject().IsStackingContext() &&
        // If needs_descendant_dependent_flags_update_ is set, we can't call
        // HasSelfPaintingLayerDescendants() now, but will update
        // descendants_need_check_position_visibility_hidden_ during
        // UpdateDescendantDependentFlags().
        !needs_descendant_dependent_flags_update_ &&
        HasSelfPaintingLayerDescendant()) {
      // This flag is cleared during UpdateDescendantDependentFlags() only, so
      // it may have false-positives which affects performance only in rare
      // cases.
      AncestorStackingContext()->descendant_needs_check_position_visibility_ =
          true;
    }
  }
}

bool PaintLayer::HasAncestorInvisibleForPositionVisibility() const {
  if (!CheckAncestorPositionVisibilityScope::ShouldCheck()) {
    return false;
  }
  for (auto* layer = Parent();
       layer && !layer->GetLayoutObject().IsStackingContext();
       layer = layer->Parent()) {
    if (layer->InvisibleForPositionVisibility()) {
      return true;
    }
  }
  return false;
}

void PaintLayer::Trace(Visitor* visitor) const {
  visitor->Trace(layout_object_);
  visitor->Trace(parent_);
  visitor->Trace(previous_);
  visitor->Trace(next_);
  visitor->Trace(first_);
  visitor->Trace(last_);
  visitor->Trace(scrollable_area_);
  visitor->Trace(stacking_node_);
  visitor->Trace(resource_info_);
  DisplayItemClient::Trace(visitor);
}

bool CheckAncestorPositionVisibilityScope::should_check_ = false;

}  // namespace blink

#if DCHECK_IS_ON()
void ShowLayerTree(const blink::PaintLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showLayerTree. Root is (nil)";
    return;
  }

  if (blink::LocalFrame* frame = layer->GetLayoutObject().GetFrame()) {
    WTF::String output =
        ExternalRepresentation(frame,
                               blink::kLayoutAsTextShowLayerNesting |
                                   blink::kLayoutAsTextShowAddresses |
                                   blink::kLayoutAsTextShowIDAndClass |
                                   blink::kLayoutAsTextDontUpdateLayout |
                                   blink::kLayoutAsTextShowLayoutState |
                                   blink::kLayoutAsTextShowPaintProperties,
                               layer);
    LOG(INFO) << output.Utf8();
  }
}

void ShowLayerTree(const blink::LayoutObject* layoutObject) {
  if (!layoutObject) {
    LOG(ERROR) << "Cannot showLayerTree. Root is (nil)";
    return;
  }
  ShowLayerTree(layoutObject->EnclosingLayer());
}
#endif
