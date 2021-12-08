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

#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"

#include <memory>

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/css_mask_painter.h"
#include "third_party/blink/renderer/core/paint/frame_paint_timing.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

static PhysicalRect ContentsRect(const LayoutObject& layout_object) {
  if (!layout_object.IsBox())
    return PhysicalRect();
  if (auto* replaced = DynamicTo<LayoutReplaced>(layout_object))
    return replaced->ReplacedContentRect();
  return To<LayoutBox>(layout_object).PhysicalContentBoxRect();
}

static bool HasBoxDecorationsOrBackgroundImage(const ComputedStyle& style) {
  return style.HasBoxDecorations() || style.HasBackgroundImage();
}

static WebPluginContainerImpl* GetPluginContainer(LayoutObject& layout_object) {
  if (auto* embedded_object = DynamicTo<LayoutEmbeddedObject>(layout_object))
    return embedded_object->Plugin();
  return nullptr;
}

// Returns true if the compositor will be responsible for applying the sticky
// position offset for this composited layer.
static bool UsesCompositedStickyPosition(PaintLayer& layer) {
  return layer.GetLayoutObject().StyleRef().HasStickyConstrainedPosition() &&
         layer.AncestorScrollContainerLayer()->NeedsCompositedScrolling();
}

// Returns the sticky position offset that should be removed from a given
// layer for use in CompositedLayerMapping.
//
// If the layer is not using composited sticky position, this will return
// gfx::PointF().
static gfx::PointF StickyPositionOffsetForLayer(PaintLayer& layer) {
  if (!UsesCompositedStickyPosition(layer))
    return gfx::PointF();

  const StickyConstraintsMap& constraints_map =
      layer.AncestorScrollContainerLayer()
          ->GetScrollableArea()
          ->GetStickyConstraintsMap();
  const StickyPositionScrollingConstraints* constraints =
      constraints_map.at(&layer);

  return gfx::PointF(constraints->GetOffsetForStickyPosition(constraints_map));
}

static bool NeedsDecorationOutlineLayer(const PaintLayer& paint_layer,
                                        const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  const LayoutUnit min_border_width =
      std::min(std::min(style.BorderLeftWidth(), style.BorderTopWidth()),
               std::min(style.BorderRightWidth(), style.BorderBottomWidth()));

  bool could_obscure_decorations =
      (paint_layer.GetScrollableArea() &&
       paint_layer.GetScrollableArea()->UsesCompositedScrolling()) ||
      layout_object.IsCanvas() || IsA<LayoutVideo>(layout_object);

  // Unlike normal outlines (whole width is outside of the offset), focus
  // rings can be drawn with the center of the path aligned with the offset,
  // so only 2/3 of the width is outside of the offset.
  const int outline_drawn_inside =
      style.OutlineStyleIsAuto()
          ? OutlinePainter::FocusRingWidthInsideBorderBox(style)
          : 0;

  return could_obscure_decorations && style.HasOutline() &&
         (style.OutlineOffset().ToInt() - outline_drawn_inside) <
             -min_border_width.ToInt();
}

CompositedLayerMapping::CompositedLayerMapping(PaintLayer& layer)
    : owning_layer_(&layer), pending_update_scope_(kGraphicsLayerUpdateNone) {
  CreatePrimaryGraphicsLayer();
}

CompositedLayerMapping::~CompositedLayerMapping() {
#if DCHECK_IS_ON()
  DCHECK(is_destroyed_);
#endif
}

void CompositedLayerMapping::Destroy() {
  RemoveSquashedLayers(non_scrolling_squashed_layers_);
  RemoveSquashedLayers(squashed_layers_in_scrolling_contents_);
  UpdateOverflowControlsLayers(false, false, false);
  UpdateForegroundLayer(false);
  UpdateMaskLayer(false);
  UpdateScrollingContentsLayer(false);
  UpdateSquashingLayers(false);
  if (graphics_layer_)
    graphics_layer_.Release()->Destroy();
  if (scrolling_contents_layer_)
    scrolling_contents_layer_.Release()->Destroy();
  if (mask_layer_)
    mask_layer_.Release()->Destroy();
  if (foreground_layer_)
    foreground_layer_.Release()->Destroy();
  if (layer_for_horizontal_scrollbar_)
    layer_for_horizontal_scrollbar_.Release()->Destroy();
  if (layer_for_vertical_scrollbar_)
    layer_for_vertical_scrollbar_.Release()->Destroy();
  if (layer_for_scroll_corner_)
    layer_for_scroll_corner_.Release()->Destroy();
  if (decoration_outline_layer_)
    decoration_outline_layer_.Release()->Destroy();
  if (non_scrolling_squashing_layer_)
    non_scrolling_squashing_layer_.Release()->Destroy();

#if DCHECK_IS_ON()
  is_destroyed_ = true;
#endif
}

void CompositedLayerMapping::RemoveSquashedLayers(
    HeapVector<Member<GraphicsLayerPaintInfo>>& squashed_layers) {
  // Do not leave the destroyed pointer dangling on any Layers that painted to
  // this mapping's squashing layer.
  for (auto& squashed_layer : squashed_layers) {
    PaintLayer* old_squashed_layer = squashed_layer->paint_layer;
    // Assert on incorrect mappings between layers and groups
    DCHECK_EQ(old_squashed_layer->GroupedMapping(), this);
    if (old_squashed_layer->GroupedMapping() == this) {
      old_squashed_layer->SetGroupedMapping(
          nullptr, PaintLayer::kDoNotInvalidateLayerAndRemoveFromMapping);
      old_squashed_layer->SetLostGroupedMapping(true);
    }
  }
}

Member<GraphicsLayer> CompositedLayerMapping::CreateGraphicsLayer(
    CompositingReasons reasons,
    SquashingDisallowedReasons squashing_disallowed_reasons) {
  auto* graphics_layer = MakeGarbageCollected<GraphicsLayer>(*this);

  graphics_layer->SetCompositingReasons(reasons);
  graphics_layer->SetSquashingDisallowedReasons(squashing_disallowed_reasons);
  if (Node* owning_node = owning_layer_->GetLayoutObject().GetNode()) {
    graphics_layer->SetOwnerNodeId(
        static_cast<int>(DOMNodeIds::IdForNode(owning_node)));
  }

  return graphics_layer;
}

void CompositedLayerMapping::CreatePrimaryGraphicsLayer() {
  graphics_layer_ =
      CreateGraphicsLayer(owning_layer_->GetCompositingReasons(),
                          owning_layer_->GetSquashingDisallowedReasons());

  graphics_layer_->SetHitTestable(true);
}

void CompositedLayerMapping::UpdateGraphicsLayerContentsOpaque(
    bool should_check_children) {
  if (BackgroundPaintsOntoGraphicsLayer()) {
    bool contents_opaque = owning_layer_->BackgroundIsKnownToBeOpaqueInRect(
        CompositedBounds(), should_check_children);
    graphics_layer_->CcLayer().SetContentsOpaque(contents_opaque);
    if (!contents_opaque) {
      graphics_layer_->CcLayer().SetContentsOpaqueForText(
          GetLayoutObject().TextIsKnownToBeOnOpaqueBackground());
    }
  } else {
    // If we only paint the background onto the scrolling contents layer we
    // are going to leave a hole in the m_graphicsLayer where the background
    // is so it is not opaque.
    graphics_layer_->CcLayer().SetContentsOpaque(false);
  }
}

void CompositedLayerMapping::UpdateContentsOpaque() {
  // If there is a foreground layer, children paint into that layer and
  // not graphics_layer_, and so don't contribute to the opaqueness of the
  // latter.
  bool should_check_children = !foreground_layer_;
  if (BackgroundPaintsOntoScrollingContentsLayer()) {
    DCHECK(scrolling_contents_layer_);
    // Backgrounds painted onto the foreground are clipped by the padding box
    // rect.
    // TODO(flackr): This should actually check the entire overflow rect
    // within the scrolling contents layer but since we currently only trigger
    // this for solid color backgrounds the answer will be the same.
    bool contents_opaque = owning_layer_->BackgroundIsKnownToBeOpaqueInRect(
        To<LayoutBox>(GetLayoutObject()).PhysicalPaddingBoxRect(),
        should_check_children);
    scrolling_contents_layer_->CcLayer().SetContentsOpaque(contents_opaque);
    if (!contents_opaque) {
      scrolling_contents_layer_->CcLayer().SetContentsOpaqueForText(
          GetLayoutObject().TextIsKnownToBeOnOpaqueBackground());
    }

    UpdateGraphicsLayerContentsOpaque(should_check_children);
  } else {
    DCHECK(BackgroundPaintsOntoGraphicsLayer());
    if (scrolling_contents_layer_)
      scrolling_contents_layer_->CcLayer().SetContentsOpaque(false);
    UpdateGraphicsLayerContentsOpaque(should_check_children);
  }

  if (non_scrolling_squashing_layer_) {
    non_scrolling_squashing_layer_->CcLayer().SetContentsOpaque(false);
    bool contents_opaque_for_text = true;
    for (const GraphicsLayerPaintInfo* squashed_layer :
         non_scrolling_squashed_layers_) {
      if (!squashed_layer->paint_layer->GetLayoutObject()
               .TextIsKnownToBeOnOpaqueBackground()) {
        contents_opaque_for_text = false;
        break;
      }
    }
    non_scrolling_squashing_layer_->CcLayer().SetContentsOpaqueForText(
        contents_opaque_for_text);
  }
}

void CompositedLayerMapping::UpdateCompositedBounds() {
  DCHECK_EQ(owning_layer_->Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingAssignmentsUpdate);
  // FIXME: if this is really needed for performance, it would be better to
  // store it on Layer.
  composited_bounds_ = owning_layer_->BoundingBoxForCompositing();
}

void CompositedLayerMapping::UpdateCompositingReasons() {
  // All other layers owned by this mapping will have the same compositing
  // reason for their lifetime, so they are initialized only when created.
  graphics_layer_->SetCompositingReasons(
      owning_layer_->GetCompositingReasons());
  graphics_layer_->SetSquashingDisallowedReasons(
      owning_layer_->GetSquashingDisallowedReasons());
}

bool CompositedLayerMapping::UpdateGraphicsLayerConfiguration(
    const PaintLayer* compositing_container) {
  DCHECK_EQ(owning_layer_->Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingAssignmentsUpdate);

  // Note carefully: here we assume that the compositing state of all
  // descendants have been updated already, so it is legitimate to compute and
  // cache the composited bounds for this layer.
  UpdateCompositedBounds();

  PaintLayerCompositor* compositor = Compositor();
  LayoutObject& layout_object = GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();

  bool layer_config_changed = false;

  if (UpdateForegroundLayer(
          compositor->NeedsContentsCompositingLayer(owning_layer_)))
    layer_config_changed = true;

  if (UpdateScrollingContentsLayer(owning_layer_->NeedsCompositedScrolling()))
    layer_config_changed = true;

  // If the outline needs to draw over the composited scrolling contents layer
  // or scrollbar layers (or video or webgl) it needs to be drawn into a
  // separate layer.
  bool needs_decoration_outline_layer =
      NeedsDecorationOutlineLayer(*owning_layer_, layout_object);

  if (UpdateDecorationOutlineLayer(needs_decoration_outline_layer))
    layer_config_changed = true;

  if (UpdateOverflowControlsLayers(RequiresHorizontalScrollbarLayer(),
                                   RequiresVerticalScrollbarLayer(),
                                   RequiresScrollCornerLayer()))
    layer_config_changed = true;

  if (UpdateSquashingLayers(!non_scrolling_squashed_layers_.IsEmpty()))
    layer_config_changed = true;

  bool has_mask =
      CSSMaskPainter::MaskBoundingBox(GetLayoutObject(), PhysicalOffset())
          .has_value();
  bool has_mask_based_clip_path =
      ClipPathClipper::ShouldUseMaskBasedClip(GetLayoutObject());
  if (UpdateMaskLayer(has_mask || has_mask_based_clip_path))
    layer_config_changed = true;

  if (layer_config_changed)
    UpdateInternalHierarchy();

  if (layout_object.IsLayoutEmbeddedContent()) {
    if (WebPluginContainerImpl* plugin = GetPluginContainer(layout_object)) {
      graphics_layer_->SetContentsToCcLayer(plugin->CcLayer());
    } else if (auto* frame_owner =
                   DynamicTo<HTMLFrameOwnerElement>(layout_object.GetNode())) {
      if (auto* remote = DynamicTo<RemoteFrame>(frame_owner->ContentFrame())) {
        graphics_layer_->SetContentsToCcLayer(remote->GetCcLayer());
      }
    }
  } else if (IsA<LayoutVideo>(layout_object)) {
    auto* media_element = To<HTMLMediaElement>(layout_object.GetNode());
    graphics_layer_->SetContentsToCcLayer(media_element->CcLayer());
  } else if (layout_object.IsCanvas()) {
    graphics_layer_->SetContentsToCcLayer(
        To<HTMLCanvasElement>(layout_object.GetNode())->ContentsCcLayer());
    layer_config_changed = true;
  }

  if (layer_config_changed) {
    // Changes to either the internal hierarchy or the mask layer have an
    // impact on painting phases, so we need to update when either are
    // updated.
    UpdatePaintingPhases();
  }

  UpdateElementId();

  if (style.Preserves3D() && style.HasOpacity() &&
      owning_layer_->Has3DTransformedDescendant()) {
    UseCounter::Count(layout_object.GetDocument(),
                      WebFeature::kOpacityWithPreserve3DQuirk);
  }

  return layer_config_changed;
}

static PhysicalOffset ComputeOffsetFromCompositedAncestor(
    const PaintLayer* layer,
    const PaintLayer* composited_ancestor,
    const PhysicalOffset& local_representative_point_for_fragmentation,
    const gfx::PointF& offset_for_sticky_position) {
  // Add in the offset of the composited bounds from the coordinate space of
  // the PaintLayer, since visualOffsetFromAncestor() requires the pre-offset
  // input to be in the space of the PaintLayer. We also need to add in this
  // offset before computation of visualOffsetFromAncestor(), because it
  // affects fragmentation offset if compositedAncestor crosses a pagination
  // boundary.
  //
  // Currently, visual fragmentation for composited layers is not implemented.
  // For fragmented contents, we paint in the logical coordinates of the flow
  // thread, then split the result by fragment boundary and paste each part
  // into each fragment's physical position.
  // Since composited layers don't support visual fragmentation, we have to
  // choose a "representative" fragment to position the painted contents. This
  // is where localRepresentativePointForFragmentation comes into play.
  // The fragment that the representative point resides in will be chosen as
  // the representative fragment for layer position purpose.
  // For layers that are not fragmented, the point doesn't affect behavior as
  // there is one and only one fragment.
  PhysicalOffset offset = layer->VisualOffsetFromAncestor(
      composited_ancestor, local_representative_point_for_fragmentation);
  if (composited_ancestor)
    offset += composited_ancestor->SubpixelAccumulation();
  offset -= local_representative_point_for_fragmentation;
  offset -= PhysicalOffset::FromPointFRound(offset_for_sticky_position);
  return offset;
}

PhysicalOffset ComputeSubpixelAccumulation(
    const PhysicalOffset& offset_from_composited_ancestor,
    const PaintLayer& layer,
    gfx::Point& snapped_offset_from_composited_ancestor) {
  snapped_offset_from_composited_ancestor =
      ToRoundedPoint(offset_from_composited_ancestor);
  PhysicalOffset subpixel_accumulation =
      offset_from_composited_ancestor -
      PhysicalOffset(snapped_offset_from_composited_ancestor);
  if (subpixel_accumulation.IsZero()) {
    return subpixel_accumulation;
  }
  if (layer.GetCompositingReasons() &
      CompositingReason::kPreventingSubpixelAccumulationReasons) {
    return PhysicalOffset();
  }
  if (layer.GetCompositingReasons() &
      CompositingReason::kActiveTransformAnimation) {
    if (const Element* element =
            To<Element>(layer.GetLayoutObject().GetNode())) {
      DCHECK(element->GetElementAnimations());
      if (element->GetElementAnimations()->IsIdentityOrTranslation())
        return subpixel_accumulation;
    }
    return PhysicalOffset();
  }
  if (!layer.Transform() || layer.Transform()->IsIdentityOrTranslation()) {
    return subpixel_accumulation;
  }
  return PhysicalOffset();
}

void CompositedLayerMapping::ComputeBoundsOfOwningLayer(
    const PaintLayer* composited_ancestor,
    gfx::Rect& local_bounds,
    gfx::Point& snapped_offset_from_composited_ancestor) {
  // HACK(chrishtr): adjust for position of inlines.
  PhysicalOffset local_representative_point_for_fragmentation;
  if (owning_layer_->GetLayoutObject().IsLayoutInline()) {
    local_representative_point_for_fragmentation =
        To<LayoutInline>(owning_layer_->GetLayoutObject())
            .FirstLineBoxTopLeft();
  }
  // Blink will already have applied any necessary offset for sticky
  // positioned elements. If the compositor is handling sticky offsets for
  // this layer, we need to remove the Blink-side offset to avoid
  // double-counting.
  gfx::PointF offset_for_sticky_position =
      StickyPositionOffsetForLayer(*owning_layer_);
  PhysicalOffset offset_from_composited_ancestor =
      ComputeOffsetFromCompositedAncestor(
          owning_layer_, composited_ancestor,
          local_representative_point_for_fragmentation,
          offset_for_sticky_position);
  PhysicalOffset subpixel_accumulation = ComputeSubpixelAccumulation(
      offset_from_composited_ancestor, *owning_layer_,
      snapped_offset_from_composited_ancestor);

  // Invalidate the whole layer when subpixel accumulation changes, since
  // the previous subpixel accumulation is baked into the display list.
  // However, don't do so for directly composited layers, to avoid impacting
  // performance.
  if (subpixel_accumulation != owning_layer_->SubpixelAccumulation()) {
    // Always invalidate if under-invalidation checking is on, to avoid
    // false positives.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
        !(owning_layer_->GetCompositingReasons() &
          CompositingReason::kComboAllDirectReasons))
      GetLayoutObject().SetShouldCheckForPaintInvalidation();
  }

  // Otherwise discard the sub-pixel remainder because paint offset can't be
  // transformed by a non-translation transform.
  owning_layer_->SetSubpixelAccumulation(subpixel_accumulation);

  absl::optional<gfx::Rect> mask_bounding_box =
      CSSMaskPainter::MaskBoundingBox(GetLayoutObject(), subpixel_accumulation);
  absl::optional<gfx::RectF> clip_path_bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(GetLayoutObject());
  if (clip_path_bounding_box)
    clip_path_bounding_box->Offset(gfx::Vector2dF(subpixel_accumulation));

  // Override graphics layer size to the bound of mask layer, this is because
  // the compositor implementation requires mask layer bound to match its
  // host layer.
  if (mask_bounding_box) {
    local_bounds = *mask_bounding_box;
    if (clip_path_bounding_box)
      local_bounds.Intersect(gfx::ToEnclosingRect(*clip_path_bounding_box));
  } else if (clip_path_bounding_box) {
    local_bounds = gfx::ToEnclosingRect(*clip_path_bounding_box);
  } else {
    // Move the bounds by the subpixel accumulation so that it pixel-snaps
    // relative to absolute pixels instead of local coordinates.
    PhysicalRect local_raw_compositing_bounds = CompositedBounds();
    local_raw_compositing_bounds.Move(subpixel_accumulation);
    local_bounds = ToPixelSnappedRect(local_raw_compositing_bounds);
  }
}

void CompositedLayerMapping::UpdateSquashingLayerGeometry(
    const PaintLayer* compositing_container,
    const gfx::Point& snapped_offset_from_composited_ancestor,
    HeapVector<Member<GraphicsLayerPaintInfo>>& layers,
    HeapVector<Member<PaintLayer>>& layers_needing_paint_invalidation) {
  if (!non_scrolling_squashing_layer_)
    return;

  gfx::Point graphics_layer_parent_location;
  ComputeGraphicsLayerParentLocation(compositing_container,
                                     graphics_layer_parent_location);

  PhysicalOffset compositing_container_offset_from_parent_graphics_layer =
      -PhysicalOffset(graphics_layer_parent_location);
  if (compositing_container) {
    compositing_container_offset_from_parent_graphics_layer +=
        compositing_container->SubpixelAccumulation();
  }

  const PaintLayer* common_transform_ancestor = nullptr;
  if (compositing_container && compositing_container->Transform()) {
    common_transform_ancestor = compositing_container;
  } else if (compositing_container) {
    common_transform_ancestor =
        &compositing_container->TransformAncestorOrRoot();
  } else {
    common_transform_ancestor = owning_layer_->Root();
  }

  // FIXME: Cache these offsets.
  PhysicalOffset compositing_container_offset_from_transformed_ancestor;
  if (compositing_container) {
    compositing_container_offset_from_transformed_ancestor =
        compositing_container->ComputeOffsetFromAncestor(
            *common_transform_ancestor);
  }

  PhysicalRect total_squash_bounds;
  for (wtf_size_t i = 0; i < layers.size(); ++i) {
    PhysicalRect squashed_bounds =
        layers[i]->paint_layer->BoundingBoxForCompositing();

    // Store the local bounds of the Layer subtree before applying the offset.
    layers[i]->composited_bounds = squashed_bounds;

    PhysicalOffset squashed_layer_offset_from_transformed_ancestor =
        layers[i]->paint_layer->ComputeOffsetFromAncestor(
            *common_transform_ancestor);
    PhysicalOffset squashed_layer_offset_from_compositing_container =
        squashed_layer_offset_from_transformed_ancestor -
        compositing_container_offset_from_transformed_ancestor;

    squashed_bounds.Move(squashed_layer_offset_from_compositing_container);
    total_squash_bounds.Unite(squashed_bounds);
  }

  // The totalSquashBounds is positioned with respect to compositingContainer.
  // But the squashingLayer needs to be positioned with respect to the
  // graphicsLayerParent.  The conversion between compositingContainer and the
  // graphicsLayerParent is already computed as
  // compositingContainerOffsetFromParentGraphicsLayer.
  total_squash_bounds.Move(
      compositing_container_offset_from_parent_graphics_layer);
  const gfx::Rect squash_layer_bounds = ToEnclosingRect(total_squash_bounds);
  const gfx::Point squash_layer_origin = squash_layer_bounds.origin();
  const PhysicalOffset squash_layer_origin_in_compositing_container_space =
      PhysicalOffset(squash_layer_origin) -
      compositing_container_offset_from_parent_graphics_layer;

  // Now that the squashing bounds are known, we can convert the PaintLayer
  // painting offsets from compositingContainer space to the squashing layer
  // space.
  //
  // The painting offset we want to compute for each squashed PaintLayer is
  // essentially the position of the squashed PaintLayer described w.r.t.
  // compositingContainer's origin.  So we just need to convert that point
  // from compositingContainer space to the squashing layer's space. This is
  // done by subtracting squashLayerOriginInCompositingContainerSpace, but
  // then the offset overall needs to be negated because that's the direction
  // that the painting code expects the offset to be.
  for (auto& layer : layers) {
    const PhysicalOffset squashed_layer_offset_from_transformed_ancestor =
        layer->paint_layer->ComputeOffsetFromAncestor(
            *common_transform_ancestor);
    const PhysicalOffset offset_from_squash_layer_origin =
        (squashed_layer_offset_from_transformed_ancestor -
         compositing_container_offset_from_transformed_ancestor) -
        squash_layer_origin_in_compositing_container_space;

    gfx::Vector2d new_offset_from_layout_object =
        -ToRoundedVector2d(offset_from_squash_layer_origin);
    PhysicalOffset subpixel_accumulation =
        offset_from_squash_layer_origin +
        PhysicalOffset(new_offset_from_layout_object);
    if (layer->offset_from_layout_object_set &&
        layer->offset_from_layout_object != new_offset_from_layout_object) {
      layers_needing_paint_invalidation.push_back(layer->paint_layer);
    }
    layer->offset_from_layout_object = new_offset_from_layout_object;
    layer->offset_from_layout_object_set = true;
    layer->paint_layer->SetSubpixelAccumulation(subpixel_accumulation);
  }

  non_scrolling_squashing_layer_->SetSize(squash_layer_bounds.size());
  // We can't non_scrolling_squashing_layer_->SetOffsetFromLayoutObject().
  // Squashing layer has special paint and invalidation logic that already
  // compensated for compositing bounds, setting it here would end up
  // double adjustment.
  auto new_offset = squash_layer_bounds.origin() -
                    snapped_offset_from_composited_ancestor +
                    graphics_layer_parent_location.OffsetFromOrigin();
  if (new_offset != non_scrolling_squashing_layer_offset_from_layout_object_) {
    non_scrolling_squashing_layer_offset_from_layout_object_ = new_offset;
    // Need to update squashing LayerState according to the new offset.
    // GraphicsLayerUpdater does this.
    layers_needing_paint_invalidation.push_back(owning_layer_);
  }

  for (auto& layer : layers)
    UpdateLocalClipRectForSquashedLayer(*owning_layer_, layers, *layer);
}

void CompositedLayerMapping::UpdateGraphicsLayerGeometry(
    const PaintLayer* compositing_container,
    HeapVector<Member<PaintLayer>>& layers_needing_paint_invalidation) {
  DCHECK_EQ(owning_layer_->Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingAssignmentsUpdate);

  gfx::Rect local_compositing_bounds;
  gfx::Point snapped_offset_from_composited_ancestor;
  ComputeBoundsOfOwningLayer(compositing_container, local_compositing_bounds,
                             snapped_offset_from_composited_ancestor);

  UpdateMainGraphicsLayerGeometry(local_compositing_bounds);
  UpdateSquashingLayerGeometry(
      compositing_container, snapped_offset_from_composited_ancestor,
      non_scrolling_squashed_layers_, layers_needing_paint_invalidation);

  UpdateMaskLayerGeometry();
  UpdateDecorationOutlineLayerGeometry(local_compositing_bounds.size());
  UpdateScrollingContentsLayerGeometry(layers_needing_paint_invalidation);
  UpdateForegroundLayerGeometry();

  if (owning_layer_->GetScrollableArea() &&
      owning_layer_->GetScrollableArea()->ScrollsOverflow())
    owning_layer_->GetScrollableArea()->PositionOverflowControls();

  UpdateContentsRect();
  UpdateDrawsContentAndPaintsHitTest();
  UpdateElementId();
  UpdateContentsOpaque();
  UpdateCompositingReasons();
}

void CompositedLayerMapping::UpdateMainGraphicsLayerGeometry(
    const gfx::Rect& local_compositing_bounds) {
  graphics_layer_->SetOffsetFromLayoutObject(
      local_compositing_bounds.OffsetFromOrigin());
  graphics_layer_->SetSize(local_compositing_bounds.size());

  // m_graphicsLayer is the corresponding GraphicsLayer for this PaintLayer
  // and its non-compositing descendants. So, the visibility flag for
  // m_graphicsLayer should be true if there are any non-compositing visible
  // layers.
  bool contents_visible = owning_layer_->HasVisibleContent() ||
                          HasVisibleNonCompositingDescendant(owning_layer_);
  // TODO(sunxd): Investigate and possibly implement computing hit test
  // regions in PaintTouchActionRects code path, so that cc has correct
  // pointer-events information. For now, there is no need to set
  // graphics_layer_'s hit testable bit here, because it is always hit
  // testable from cc's perspective.
  graphics_layer_->SetContentsVisible(contents_visible);
}

void CompositedLayerMapping::ComputeGraphicsLayerParentLocation(
    const PaintLayer* compositing_container,
    gfx::Point& graphics_layer_parent_location) {
  if (compositing_container) {
    graphics_layer_parent_location = gfx::PointAtOffsetFromOrigin(
        compositing_container->GetCompositedLayerMapping()
            ->ParentForSublayers()
            ->OffsetFromLayoutObject());
  } else if (!GetLayoutObject().GetFrame()->IsLocalRoot()) {  // TODO(oopif)
    DCHECK(!compositing_container);
    graphics_layer_parent_location = gfx::Point();
  }

  if (compositing_container &&
      compositing_container->NeedsCompositedScrolling()) {
    auto& layout_box = To<LayoutBox>(compositing_container->GetLayoutObject());
    gfx::Vector2d scroll_offset =
        layout_box.PixelSnappedScrolledContentOffset();
    gfx::Point scroll_origin =
        compositing_container->GetScrollableArea()->ScrollOrigin();
    scroll_origin -= layout_box.OriginAdjustmentForScrollbars();
    scroll_origin.Offset(-layout_box.BorderLeft().ToInt(),
                         -layout_box.BorderTop().ToInt());
    graphics_layer_parent_location = gfx::PointAtOffsetFromOrigin(
        -(scroll_origin.OffsetFromOrigin() + scroll_offset));
  }
}

void CompositedLayerMapping::UpdateMaskLayerGeometry() {
  if (!mask_layer_)
    return;

  mask_layer_->SetSize(graphics_layer_->Size());
  mask_layer_->SetOffsetFromLayoutObject(
      graphics_layer_->OffsetFromLayoutObject());
}

void CompositedLayerMapping::UpdateScrollingContentsLayerGeometry(
    HeapVector<Member<PaintLayer>>& layers_needing_paint_invalidation) {
  if (!scrolling_contents_layer_) {
    DCHECK(squashed_layers_in_scrolling_contents_.IsEmpty());
    return;
  }

  DCHECK(scrolling_contents_layer_);
  auto& layout_box = To<LayoutBox>(GetLayoutObject());
  gfx::Rect overflow_clip_rect = ToPixelSnappedRect(
      layout_box.OverflowClipRect(owning_layer_->SubpixelAccumulation()));

  bool scroll_container_size_changed =
      previous_scroll_container_size_ != overflow_clip_rect.size();
  if (scroll_container_size_changed)
    previous_scroll_container_size_ = overflow_clip_rect.size();

  PaintLayerScrollableArea* scrollable_area =
      owning_layer_->GetScrollableArea();
  gfx::Size scroll_size = scrollable_area->PixelSnappedContentsSize(
      owning_layer_->SubpixelAccumulation());

  // Ensure scrolling contents are at least as large as the scroll clip
  scroll_size.SetToMin(overflow_clip_rect.size());

  auto* scrolling_coordinator = owning_layer_->GetScrollingCoordinator();
  scrolling_coordinator->UpdateCompositorScrollOffset(*layout_box.GetFrame(),
                                                      *scrollable_area);

  if (scroll_size != scrolling_contents_layer_->Size() ||
      scroll_container_size_changed) {
    scrolling_coordinator->ScrollableAreaScrollLayerDidChange(scrollable_area);
  }

  scrolling_contents_layer_->SetSize(scroll_size);

  scrolling_contents_layer_->SetOffsetFromLayoutObject(
      overflow_clip_rect.origin() - scrollable_area->ScrollOrigin());

  for (auto& layer : squashed_layers_in_scrolling_contents_) {
    layer->composited_bounds = layer->paint_layer->BoundingBoxForCompositing();
    PhysicalOffset offset_from_scrolling_contents_layer =
        layer->paint_layer->ComputeOffsetFromAncestor(*owning_layer_) +
        owning_layer_->SubpixelAccumulation() -
        PhysicalOffset(scrolling_contents_layer_->OffsetFromLayoutObject());
    gfx::Vector2d new_offset_from_layout_object =
        -ToRoundedVector2d(offset_from_scrolling_contents_layer);
    PhysicalOffset subpixel_accumulation =
        offset_from_scrolling_contents_layer +
        PhysicalOffset(new_offset_from_layout_object);

    if (layer->offset_from_layout_object_set &&
        layer->offset_from_layout_object != new_offset_from_layout_object) {
      layers_needing_paint_invalidation.push_back(layer->paint_layer);
    }
    layer->offset_from_layout_object = new_offset_from_layout_object;
    layer->offset_from_layout_object_set = true;
    layer->paint_layer->SetSubpixelAccumulation(subpixel_accumulation);
  }
  for (auto& layer : squashed_layers_in_scrolling_contents_) {
    UpdateLocalClipRectForSquashedLayer(
        *owning_layer_, squashed_layers_in_scrolling_contents_, *layer);
  }
}

bool CompositedLayerMapping::RequiresHorizontalScrollbarLayer() const {
  return owning_layer_->GetScrollableArea() &&
         owning_layer_->GetScrollableArea()->HorizontalScrollbar();
}

bool CompositedLayerMapping::RequiresVerticalScrollbarLayer() const {
  return owning_layer_->GetScrollableArea() &&
         owning_layer_->GetScrollableArea()->VerticalScrollbar();
}

bool CompositedLayerMapping::RequiresScrollCornerLayer() const {
  return owning_layer_->GetScrollableArea() &&
         !owning_layer_->GetScrollableArea()
              ->ScrollCornerAndResizerRect()
              .IsEmpty();
}

void CompositedLayerMapping::UpdateForegroundLayerGeometry() {
  if (!foreground_layer_)
    return;

  // Should be equivalent to local_compositing_bounds.
  gfx::Rect compositing_bounds(
      gfx::PointAtOffsetFromOrigin(graphics_layer_->OffsetFromLayoutObject()),
      graphics_layer_->Size());
  if (scrolling_contents_layer_) {
    // Override compositing bounds to include full overflow if composited
    // scrolling is used.
    compositing_bounds =
        gfx::Rect(gfx::PointAtOffsetFromOrigin(
                      scrolling_contents_layer_->OffsetFromLayoutObject()),
                  scrolling_contents_layer_->Size());
  }

  foreground_layer_->SetOffsetFromLayoutObject(
      compositing_bounds.OffsetFromOrigin());
  foreground_layer_->SetSize(compositing_bounds.size());
}

void CompositedLayerMapping::UpdateDecorationOutlineLayerGeometry(
    const gfx::Size& relative_compositing_bounds_size) {
  if (!decoration_outline_layer_)
    return;
  decoration_outline_layer_->SetSize(relative_compositing_bounds_size);
  decoration_outline_layer_->SetOffsetFromLayoutObject(
      graphics_layer_->OffsetFromLayoutObject());
}

void CompositedLayerMapping::UpdateInternalHierarchy() {
  // foreground_layer_ has to be inserted in the correct order with child
  // layers in SetSubLayers(), so it's not inserted here.
  graphics_layer_->RemoveFromParent();

  bool overflow_controls_after_scrolling_contents =
      owning_layer_->IsRootLayer() ||
      (owning_layer_->GetScrollableArea() &&
       owning_layer_->GetScrollableArea()->HasOverlayOverflowControls());
  if (overflow_controls_after_scrolling_contents && scrolling_contents_layer_)
    graphics_layer_->AddChild(scrolling_contents_layer_);

  if (layer_for_horizontal_scrollbar_)
    graphics_layer_->AddChild(layer_for_horizontal_scrollbar_);
  if (layer_for_vertical_scrollbar_)
    graphics_layer_->AddChild(layer_for_vertical_scrollbar_);
  if (layer_for_scroll_corner_)
    graphics_layer_->AddChild(layer_for_scroll_corner_);

  if (!overflow_controls_after_scrolling_contents && scrolling_contents_layer_)
    graphics_layer_->AddChild(scrolling_contents_layer_);

  if (decoration_outline_layer_)
    graphics_layer_->AddChild(decoration_outline_layer_);

  if (mask_layer_)
    graphics_layer_->AddChild(mask_layer_);

  if (non_scrolling_squashing_layer_)
    graphics_layer_->AddChild(non_scrolling_squashing_layer_);
}

void CompositedLayerMapping::UpdatePaintingPhases() {
  graphics_layer_->SetPaintingPhase(PaintingPhaseForPrimaryLayer());
  if (scrolling_contents_layer_) {
    GraphicsLayerPaintingPhase paint_phase =
        kGraphicsLayerPaintOverflowContents |
        kGraphicsLayerPaintCompositedScroll;
    if (!foreground_layer_)
      paint_phase |= kGraphicsLayerPaintForeground;
    scrolling_contents_layer_->SetPaintingPhase(paint_phase);
  }
  if (foreground_layer_) {
    GraphicsLayerPaintingPhase paint_phase = kGraphicsLayerPaintForeground;
    if (scrolling_contents_layer_)
      paint_phase |= kGraphicsLayerPaintOverflowContents;
    foreground_layer_->SetPaintingPhase(paint_phase);
  }
}

void CompositedLayerMapping::UpdateContentsRect() {
  graphics_layer_->SetContentsRect(ToPixelSnappedRect(ContentsBox()));
}

void CompositedLayerMapping::UpdateDrawsContentAndPaintsHitTest() {
  bool in_overlay_fullscreen_video = false;
  if (IsA<LayoutVideo>(GetLayoutObject())) {
    auto* video_element = To<HTMLVideoElement>(GetLayoutObject().GetNode());
    if (video_element->IsFullscreen() &&
        video_element->UsesOverlayFullscreenVideo())
      in_overlay_fullscreen_video = true;
  }
  bool has_painted_content =
      in_overlay_fullscreen_video ? false : ContainsPaintedContent();
  graphics_layer_->SetDrawsContent(has_painted_content);

  // |has_painted_content| is conservative (e.g., will be true if any descendant
  // paints content, regardless of whether the descendant content is a hit test)
  // but an exhaustive check of descendants that paint hit tests would be too
  // expensive.
  bool paints_hit_test = has_painted_content ||
                         GetLayoutObject().HasEffectiveAllowedTouchAction() ||
                         GetLayoutObject().InsideBlockingWheelEventHandler();
  bool paints_scroll_hit_test =
      ((owning_layer_->GetScrollableArea() &&
        owning_layer_->GetScrollableArea()->ScrollsOverflow()) ||
       (GetPluginContainer(GetLayoutObject()) &&
        GetPluginContainer(GetLayoutObject())->WantsWheelEvents()));
  graphics_layer_->SetPaintsHitTest(paints_hit_test || paints_scroll_hit_test);

  if (scrolling_contents_layer_) {
    // scrolling_contents_layer_ only needs backing store if the scrolled
    // contents need to paint.
    bool has_painted_scrolling_contents =
        !squashed_layers_in_scrolling_contents_.IsEmpty() ||
        (owning_layer_->HasVisibleContent() &&
         (GetLayoutObject().StyleRef().HasBackground() ||
          GetLayoutObject().HasNonInitialBackdropFilter() || PaintsChildren()));
    scrolling_contents_layer_->SetDrawsContent(has_painted_scrolling_contents);
    scrolling_contents_layer_->SetPaintsHitTest(paints_hit_test);
  }

  if (has_painted_content && GetLayoutObject().IsCanvas() &&
      To<LayoutHTMLCanvas>(GetLayoutObject())
          .DrawsBackgroundOntoContentLayer()) {
    has_painted_content = false;
  }

  // FIXME: we could refine this to only allocate backings for one of these
  // layers if possible.
  if (foreground_layer_) {
    foreground_layer_->SetDrawsContent(has_painted_content);
    foreground_layer_->SetPaintsHitTest(paints_hit_test);
  }

  if (decoration_outline_layer_)
    decoration_outline_layer_->SetDrawsContent(true);

  if (mask_layer_)
    mask_layer_->SetDrawsContent(true);
}

bool CompositedLayerMapping::ToggleScrollbarLayerIfNeeded(
    Member<GraphicsLayer>& layer,
    bool needs_layer,
    CompositingReasons reason) {
  if (needs_layer == !!layer)
    return false;
  if (layer)
    layer.Release()->Destroy();
  else
    layer = CreateGraphicsLayer(reason);

  if (PaintLayerScrollableArea* scrollable_area =
          owning_layer_->GetScrollableArea()) {
    if (ScrollingCoordinator* scrolling_coordinator =
            owning_layer_->GetScrollingCoordinator()) {
      if (reason == CompositingReason::kLayerForHorizontalScrollbar) {
        scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
            scrollable_area, kHorizontalScrollbar);
      } else if (reason == CompositingReason::kLayerForVerticalScrollbar) {
        scrolling_coordinator->ScrollableAreaScrollbarLayerDidChange(
            scrollable_area, kVerticalScrollbar);
      }
    }
  }
  return true;
}

bool CompositedLayerMapping::UpdateOverflowControlsLayers(
    bool needs_horizontal_scrollbar_layer,
    bool needs_vertical_scrollbar_layer,
    bool needs_scroll_corner_layer) {
  if (PaintLayerScrollableArea* scrollable_area =
          owning_layer_->GetScrollableArea()) {
    // If the scrollable area is marked as needing a new scrollbar layer,
    // destroy the layer now so that it will be created again below.
    if (layer_for_horizontal_scrollbar_ && needs_horizontal_scrollbar_layer &&
        scrollable_area->ShouldRebuildHorizontalScrollbarLayer()) {
      ToggleScrollbarLayerIfNeeded(
          layer_for_horizontal_scrollbar_, false,
          CompositingReason::kLayerForHorizontalScrollbar);
    }
    if (layer_for_vertical_scrollbar_ && needs_vertical_scrollbar_layer &&
        scrollable_area->ShouldRebuildVerticalScrollbarLayer()) {
      ToggleScrollbarLayerIfNeeded(
          layer_for_vertical_scrollbar_, false,
          CompositingReason::kLayerForVerticalScrollbar);
    }
    scrollable_area->ResetRebuildScrollbarLayerFlags();
  }

  // If the subtree is invisible, we don't actually need scrollbar layers.
  // Only do this check if at least one of the bits is currently true.
  // This is important because this method is called during the destructor
  // of CompositedLayerMapping, which may happen during style recalc,
  // and therefore visible content status may be invalid.
  if (needs_horizontal_scrollbar_layer || needs_vertical_scrollbar_layer ||
      needs_scroll_corner_layer) {
    bool invisible = owning_layer_->SubtreeIsInvisible();
    needs_horizontal_scrollbar_layer &= !invisible;
    needs_vertical_scrollbar_layer &= !invisible;
    needs_scroll_corner_layer &= !invisible;
  }

  bool horizontal_scrollbar_layer_changed = ToggleScrollbarLayerIfNeeded(
      layer_for_horizontal_scrollbar_, needs_horizontal_scrollbar_layer,
      CompositingReason::kLayerForHorizontalScrollbar);
  bool vertical_scrollbar_layer_changed = ToggleScrollbarLayerIfNeeded(
      layer_for_vertical_scrollbar_, needs_vertical_scrollbar_layer,
      CompositingReason::kLayerForVerticalScrollbar);
  bool scroll_corner_layer_changed = ToggleScrollbarLayerIfNeeded(
      layer_for_scroll_corner_, needs_scroll_corner_layer,
      CompositingReason::kLayerForScrollCorner);

  return horizontal_scrollbar_layer_changed ||
         vertical_scrollbar_layer_changed || scroll_corner_layer_changed;
}

void CompositedLayerMapping::PositionOverflowControlsLayers() {
  if (GraphicsLayer* layer = LayerForHorizontalScrollbar()) {
    Scrollbar* h_bar =
        owning_layer_->GetScrollableArea()->HorizontalScrollbar();
    if (h_bar) {
      gfx::Rect frame_rect = h_bar->FrameRect();
      layer->SetOffsetFromLayoutObject(frame_rect.OffsetFromOrigin());
      layer->SetSize(frame_rect.size());
      if (layer->HasContentsLayer())
        layer->SetContentsRect(gfx::Rect(frame_rect.size()));
    }
    bool h_bar_visible = h_bar && !layer->HasContentsLayer();
    layer->SetDrawsContent(h_bar_visible);
    layer->SetHitTestable(h_bar_visible);
  }

  if (GraphicsLayer* layer = LayerForVerticalScrollbar()) {
    Scrollbar* v_bar = owning_layer_->GetScrollableArea()->VerticalScrollbar();
    if (v_bar) {
      gfx::Rect frame_rect = v_bar->FrameRect();
      layer->SetOffsetFromLayoutObject(frame_rect.OffsetFromOrigin());
      layer->SetSize(frame_rect.size());
      if (layer->HasContentsLayer())
        layer->SetContentsRect(gfx::Rect(frame_rect.size()));
    }
    bool v_bar_visible = v_bar && !layer->HasContentsLayer();
    layer->SetDrawsContent(v_bar_visible);
    layer->SetHitTestable(v_bar_visible);
  }

  if (GraphicsLayer* layer = LayerForScrollCorner()) {
    const gfx::Rect& scroll_corner_and_resizer =
        owning_layer_->GetScrollableArea()->ScrollCornerAndResizerRect();
    layer->SetOffsetFromLayoutObject(
        scroll_corner_and_resizer.OffsetFromOrigin());
    layer->SetSize(scroll_corner_and_resizer.size());
    layer->SetDrawsContent(!scroll_corner_and_resizer.IsEmpty());
    layer->SetHitTestable(!scroll_corner_and_resizer.IsEmpty());
  }
}

template <typename Function>
static void ApplyToGraphicsLayers(const CompositedLayerMapping* mapping,
                                  const Function& function) {
  auto null_checking_function = [&function](GraphicsLayer* layer) {
    if (layer)
      function(layer);
  };

  null_checking_function(mapping->MainGraphicsLayer());
  null_checking_function(mapping->ScrollingContentsLayer());
  null_checking_function(mapping->ForegroundLayer());
  null_checking_function(mapping->MaskLayer());
  null_checking_function(mapping->DecorationOutlineLayer());
  null_checking_function(mapping->NonScrollingSquashingLayer());
  null_checking_function(mapping->LayerForHorizontalScrollbar());
  null_checking_function(mapping->LayerForVerticalScrollbar());
  null_checking_function(mapping->LayerForScrollCorner());
}

// You receive an element id if you have an animation, or you're a scroller
// (and might impl animate).
//
// The element id for the scroll layers is assigned when they're constructed,
// since this is unconditional. However, the element id for the primary layer
// may change according to the rules above so we update those values here.
void CompositedLayerMapping::UpdateElementId() {
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      owning_layer_->GetLayoutObject().UniqueId(),
      CompositorElementIdNamespace::kPrimary);

  graphics_layer_->SetElementId(element_id);
}

bool CompositedLayerMapping::UpdateForegroundLayer(
    bool needs_foreground_layer) {
  bool layer_changed = false;
  if (needs_foreground_layer) {
    if (!foreground_layer_) {
      foreground_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForForeground);
      foreground_layer_->SetHitTestable(true);
      layer_changed = true;
    }
  } else if (foreground_layer_) {
    foreground_layer_->RemoveFromParent();
    foreground_layer_.Release()->Destroy();
    layer_changed = true;
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateDecorationOutlineLayer(
    bool needs_decoration_outline_layer) {
  bool layer_changed = false;
  if (needs_decoration_outline_layer) {
    if (!decoration_outline_layer_) {
      decoration_outline_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForDecoration);
      decoration_outline_layer_->SetPaintingPhase(
          kGraphicsLayerPaintDecoration);
      layer_changed = true;
    }
  } else if (decoration_outline_layer_) {
    decoration_outline_layer_.Release()->Destroy();
    layer_changed = true;
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateMaskLayer(bool needs_mask_layer) {
  bool layer_changed = false;
  if (needs_mask_layer) {
    if (!mask_layer_) {
      mask_layer_ = CreateGraphicsLayer(CompositingReason::kLayerForMask);
      mask_layer_->SetPaintingPhase(kGraphicsLayerPaintMask);
      CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
          GetLayoutObject().UniqueId(),
          CompositorElementIdNamespace::kEffectMask);
      mask_layer_->SetElementId(element_id);
      if (GetLayoutObject().HasNonInitialBackdropFilter())
        mask_layer_->CcLayer().SetIsBackdropFilterMask(true);
      mask_layer_->SetHitTestable(true);
      layer_changed = true;
    }
  } else if (mask_layer_) {
    mask_layer_.Release()->Destroy();
    layer_changed = true;
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateScrollingContentsLayer(
    bool needs_scrolling_contents_layer) {
  ScrollingCoordinator* scrolling_coordinator =
      owning_layer_->GetScrollingCoordinator();

  auto* scrollable_area = owning_layer_->GetScrollableArea();
  if (scrollable_area)
    scrollable_area->SetUsesCompositedScrolling(needs_scrolling_contents_layer);

  bool layer_changed = false;
  if (needs_scrolling_contents_layer) {
    if (!scrolling_contents_layer_) {
      // Inner layer which renders the content that scrolls.
      scrolling_contents_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForScrollingContents);
      scrolling_contents_layer_->SetHitTestable(true);

      DCHECK(scrollable_area);
      auto element_id = scrollable_area->GetScrollElementId();
      scrolling_contents_layer_->SetElementId(element_id);

      layer_changed = true;
      if (scrolling_coordinator) {
        scrolling_coordinator->ScrollableAreaScrollLayerDidChange(
            scrollable_area);
      }
    }
  } else if (scrolling_contents_layer_) {
    scrolling_contents_layer_.Release()->Destroy();
    layer_changed = true;
    if (scrolling_coordinator && scrollable_area) {
      scrolling_coordinator->ScrollableAreaScrollLayerDidChange(
          scrollable_area);
    }
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateSquashingLayers(
    bool needs_squashing_layers) {
  bool layers_changed = false;

  if (needs_squashing_layers) {
    if (!non_scrolling_squashing_layer_) {
      non_scrolling_squashing_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForSquashingContents);
      non_scrolling_squashing_layer_->SetDrawsContent(true);
      non_scrolling_squashing_layer_->SetHitTestable(true);
      layers_changed = true;
    }
    DCHECK(non_scrolling_squashing_layer_);
  } else {
    if (non_scrolling_squashing_layer_) {
      non_scrolling_squashing_layer_->RemoveFromParent();
      non_scrolling_squashing_layer_.Release()->Destroy();
      layers_changed = true;
    }
    DCHECK(!non_scrolling_squashing_layer_);
  }

  return layers_changed;
}

GraphicsLayerPaintingPhase
CompositedLayerMapping::PaintingPhaseForPrimaryLayer() const {
  unsigned phase = kGraphicsLayerPaintBackground;
  if (!foreground_layer_)
    phase |= kGraphicsLayerPaintForeground;
  if (!mask_layer_)
    phase |= kGraphicsLayerPaintMask;
  if (!decoration_outline_layer_)
    phase |= kGraphicsLayerPaintDecoration;

  if (scrolling_contents_layer_) {
    phase &= ~kGraphicsLayerPaintForeground;
    phase |= kGraphicsLayerPaintCompositedScroll;
  }

  return static_cast<GraphicsLayerPaintingPhase>(phase);
}

bool CompositedLayerMapping::PaintsChildren() const {
  if (owning_layer_->HasVisibleContent() &&
      owning_layer_->HasNonEmptyChildLayoutObjects())
    return true;

  if (HasVisibleNonCompositingDescendant(owning_layer_))
    return true;

  return false;
}

static bool IsCompositedPlugin(LayoutObject& layout_object) {
  return layout_object.IsEmbeddedObject() &&
         layout_object.AdditionalCompositingReasons();
}

bool CompositedLayerMapping::HasVisibleNonCompositingDescendant(
    PaintLayer* parent) {
  if (!parent->HasVisibleDescendant())
    return false;

  PaintLayerPaintOrderIterator iterator(parent, kAllChildren);
  while (PaintLayer* child_layer = iterator.Next()) {
    if (child_layer->HasCompositedLayerMapping())
      continue;
    if (child_layer->HasVisibleContent() ||
        HasVisibleNonCompositingDescendant(child_layer))
      return true;
  }

  return false;
}

bool CompositedLayerMapping::ContainsPaintedContent() const {
  if (CompositedBounds().IsEmpty())
    return false;

  LayoutObject& layout_object = GetLayoutObject();
  // FIXME: we could optimize cases where the image, video or canvas is known
  // to fill the border box entirely, and set background color on the layer in
  // that case, instead of allocating backing store and painting.
  auto* layout_video = DynamicTo<LayoutVideo>(layout_object);
  if (layout_video && layout_video->GetDisplayMode() == LayoutVideo::kVideo)
    return owning_layer_->HasBoxDecorationsOrBackground();

  if (layout_object.GetNode() && layout_object.GetNode()->IsDocumentNode()) {
    if (owning_layer_->NeedsCompositedScrolling())
      return BackgroundPaintsOntoGraphicsLayer();

    // Look to see if the root object has a non-simple background
    LayoutObject* root_object =
        layout_object.GetDocument().documentElement()
            ? layout_object.GetDocument().documentElement()->GetLayoutObject()
            : nullptr;
    // Reject anything that has a border, a border-radius or outline,
    // or is not a simple background (no background, or solid color).
    if (root_object &&
        HasBoxDecorationsOrBackgroundImage(root_object->StyleRef()))
      return true;

    // Now look at the body's LayoutObject.
    HTMLElement* body = layout_object.GetDocument().FirstBodyElement();
    LayoutObject* body_object = body ? body->GetLayoutObject() : nullptr;
    if (body_object &&
        HasBoxDecorationsOrBackgroundImage(body_object->StyleRef()))
      return true;
  }

  if (owning_layer_->HasVisibleBoxDecorations())
    return true;

  if (layout_object.HasMask())  // masks require special treatment
    return true;

  if (layout_object.IsAtomicInlineLevel() && !IsCompositedPlugin(layout_object))
    return true;

  if (layout_object.IsLayoutMultiColumnSet())
    return true;

  // FIXME: it's O(n^2). A better solution is needed.
  return PaintsChildren();
}

// Return the offset from the top-left of this compositing layer at which the
// LayoutObject's contents are painted.
PhysicalOffset CompositedLayerMapping::ContentOffsetInCompositingLayer() const {
  return owning_layer_->SubpixelAccumulation() -
         PhysicalOffset(graphics_layer_->OffsetFromLayoutObject());
}

PhysicalRect CompositedLayerMapping::ContentsBox() const {
  PhysicalRect contents_box = ContentsRect(GetLayoutObject());
  contents_box.Move(ContentOffsetInCompositingLayer());
  return contents_box;
}

bool CompositedLayerMapping::NeedsToReparentOverflowControls() const {
  return owning_layer_->NeedsReorderOverlayOverflowControls();
}

wtf_size_t CompositedLayerMapping::MoveOverflowControlLayersInto(
    GraphicsLayerVector& vector,
    wtf_size_t position) {
  wtf_size_t count = 0;
  auto move_layer = [&](GraphicsLayer* layer) {
    if (!layer)
      return;
    layer->RemoveFromParent();
    vector.insert(position++, layer);
    count++;
  };
  move_layer(layer_for_horizontal_scrollbar_);
  move_layer(layer_for_vertical_scrollbar_);
  move_layer(layer_for_scroll_corner_);
  return count;
}

GraphicsLayer* CompositedLayerMapping::ParentForSublayers() const {
  if (scrolling_contents_layer_)
    return scrolling_contents_layer_;

  return graphics_layer_;
}

void CompositedLayerMapping::SetSublayers(GraphicsLayerVector sublayers) {
  GraphicsLayer* parent = ParentForSublayers();

  // TODO(szager): Remove after diagnosing crash crbug.com/1092673
  CHECK(parent);

  // The caller should have inserted |foreground_layer_| into |sublayers|.
  DCHECK(!foreground_layer_ || sublayers.Contains(foreground_layer_));

  if (parent == graphics_layer_) {
    // SetChildren() below will clobber all layers in |parent|, so we need to
    // add layers that should stay in the children list into |sublayers|.
    if (!NeedsToReparentOverflowControls()) {
      if (layer_for_horizontal_scrollbar_)
        sublayers.insert(0, layer_for_horizontal_scrollbar_);
      if (layer_for_vertical_scrollbar_)
        sublayers.insert(0, layer_for_vertical_scrollbar_);
      if (layer_for_scroll_corner_)
        sublayers.insert(0, layer_for_scroll_corner_);
    }

    if (decoration_outline_layer_)
      sublayers.push_back(decoration_outline_layer_);
    if (mask_layer_)
      sublayers.push_back(mask_layer_);
    if (non_scrolling_squashing_layer_)
      sublayers.push_back(non_scrolling_squashing_layer_);
  }

  parent->SetChildren(sublayers);
}

GraphicsLayerUpdater::UpdateType CompositedLayerMapping::UpdateTypeForChildren(
    GraphicsLayerUpdater::UpdateType update_type) const {
  if (pending_update_scope_ >= kGraphicsLayerUpdateSubtree)
    return GraphicsLayerUpdater::kForceUpdate;
  return update_type;
}

GraphicsLayer* CompositedLayerMapping::SquashingLayer(
    const PaintLayer& squashed_layer) const {
#if DCHECK_IS_ON()
  AssertInSquashedLayersVector(squashed_layer);
#endif
  if (MayBeSquashedIntoScrollingContents(squashed_layer)) {
    DCHECK(ScrollingContentsLayer());
    return ScrollingContentsLayer();
  }
  DCHECK(NonScrollingSquashingLayer());
  return NonScrollingSquashingLayer();
}

void CompositedLayerMapping::SetNeedsCheckRasterInvalidation() {
  ApplyToGraphicsLayers(this, [](GraphicsLayer* graphics_layer) {
    if (graphics_layer->DrawsContent())
      graphics_layer->SetNeedsCheckRasterInvalidation();
  });
}

const GraphicsLayerPaintInfo* CompositedLayerMapping::ContainingSquashedLayer(
    const LayoutObject* layout_object,
    const HeapVector<Member<GraphicsLayerPaintInfo>>& layers,
    unsigned max_squashed_layer_index) {
  if (!layout_object)
    return nullptr;
  for (wtf_size_t i = 0; i < layers.size() && i < max_squashed_layer_index;
       ++i) {
    if (layout_object->IsDescendantOf(
            &layers[i]->paint_layer->GetLayoutObject()))
      return layers[i];
  }
  return nullptr;
}

const GraphicsLayerPaintInfo*
CompositedLayerMapping::ContainingSquashedLayerInSquashingLayer(
    const LayoutObject* layout_object,
    unsigned max_squashed_layer_index) const {
  return ContainingSquashedLayer(layout_object, non_scrolling_squashed_layers_,
                                 max_squashed_layer_index);
}

void CompositedLayerMapping::UpdateLocalClipRectForSquashedLayer(
    const PaintLayer& reference_layer,
    const HeapVector<Member<GraphicsLayerPaintInfo>>& layers,
    GraphicsLayerPaintInfo& paint_info) {
  const LayoutObject* clipping_container =
      paint_info.paint_layer->ClippingContainer();
  if (clipping_container == reference_layer.ClippingContainer() ||
      // When squashing into scrolling contents without other clips.
      clipping_container == &reference_layer.GetLayoutObject()) {
    paint_info.local_clip_rect_for_squashed_layer =
        ClipRect(PhysicalRect(LayoutRect::InfiniteIntRect()));
    paint_info.offset_from_clip_rect_root = PhysicalOffset();
    paint_info.local_clip_rect_root = paint_info.paint_layer;
    return;
  }

  DCHECK(clipping_container);

  const GraphicsLayerPaintInfo* ancestor_paint_info =
      ContainingSquashedLayer(clipping_container, layers, layers.size());
  // Must be there, otherwise
  // CompositingLayerAssigner::GetReasonsPreventingSquashing() would have
  // disallowed squashing.
  DCHECK(ancestor_paint_info);

  const PaintLayer* ancestor_layer = ancestor_paint_info->paint_layer;
  ClipRectsContext clip_rects_context(
      ancestor_layer,
      &ancestor_layer->GetLayoutObject().PrimaryStitchingFragment());
  ClipRect parent_clip_rect;
  paint_info.paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kUseGeometryMapper)
      .CalculateBackgroundClipRect(clip_rects_context, parent_clip_rect);

  // Convert from ancestor to local coordinates.
  gfx::Vector2d ancestor_to_local_offset =
      paint_info.offset_from_layout_object -
      ancestor_paint_info->offset_from_layout_object;
  parent_clip_rect.Move(PhysicalOffset(ancestor_to_local_offset));
  paint_info.local_clip_rect_for_squashed_layer = parent_clip_rect;
  paint_info.offset_from_clip_rect_root =
      PhysicalOffset(ancestor_to_local_offset);
  paint_info.local_clip_rect_root = ancestor_paint_info->paint_layer;
}

void CompositedLayerMapping::DoPaintTask(
    const GraphicsLayerPaintInfo& paint_info,
    const GraphicsLayer& graphics_layer,
    PaintLayerFlags paint_layer_flags,
    GraphicsContext& context,
    const gfx::Rect& clip /* In the coords of rootLayer */) const {
  FontCachePurgePreventer font_cache_purge_preventer;

  gfx::Vector2d offset = paint_info.offset_from_layout_object;
  // The dirtyRect is in the coords of the painting root.
  gfx::Rect dirty_rect(clip);
  dirty_rect.Offset(offset);

  if (paint_layer_flags & (kPaintLayerPaintingOverflowContents)) {
    dirty_rect.Offset(
        ToRoundedVector2d(paint_info.paint_layer->SubpixelAccumulation()));
  } else {
    PhysicalRect bounds = paint_info.composited_bounds;
    bounds.Move(paint_info.paint_layer->SubpixelAccumulation());
    dirty_rect.Intersect(ToPixelSnappedRect(bounds));
  }

#if DCHECK_IS_ON()
  if (!GetLayoutObject().View()->GetFrame() ||
      !GetLayoutObject().View()->GetFrame()->ShouldThrottleRendering())
    paint_info.paint_layer->GetLayoutObject().AssertSubtreeIsLaidOut();
#endif

  float device_scale_factor = blink::DeviceScaleFactorDeprecated(
      paint_info.paint_layer->GetLayoutObject().GetFrame());
  context.SetDeviceScaleFactor(device_scale_factor);

  // As a composited layer may be painted directly, we need to traverse the
  // effect tree starting from the current node all the way up through the
  // parents to determine which effects are opacity 0, for the purposes of
  // correctly computing paint metrics such as First Contentful Paint and
  // Largest Contentful Paint. For the latter we special-case the nodes where
  // the opacity:0 depth is 1, so we need to only compute up to the first two
  // opacity:0 effects in here and can ignore the rest.
  absl::optional<IgnorePaintTimingScope> ignore_paint_timing_scope;
  int num_ignores = 0;
  DCHECK_EQ(IgnorePaintTimingScope::IgnoreDepth(), 0);
  for (const auto* effect_node = &paint_info.paint_layer->GetLayoutObject()
                                      .FirstFragment()
                                      .PreEffect()
                                      .Unalias();
       effect_node && num_ignores < 2;
       effect_node = effect_node->UnaliasedParent()) {
    if (effect_node->Opacity() == 0.0f) {
      if (!ignore_paint_timing_scope)
        ignore_paint_timing_scope.emplace();
      IgnorePaintTimingScope::IncrementIgnoreDepth();
      ++num_ignores;
    }
  }

  if (paint_info.paint_layer->GetCompositingState() !=
      kPaintsIntoGroupedBacking) {
    // FIXME: GraphicsLayers need a way to split for multicol.
    PaintLayerPaintingInfo painting_info(
        paint_info.paint_layer, CullRect(dirty_rect), kGlobalPaintNormalPhase,
        paint_info.paint_layer->SubpixelAccumulation());
    PaintLayerPainter(*paint_info.paint_layer)
        .PaintLayerContents(context, painting_info, paint_layer_flags);
  } else {
    PaintLayerPaintingInfo painting_info(
        paint_info.paint_layer, CullRect(dirty_rect), kGlobalPaintNormalPhase,
        paint_info.paint_layer->SubpixelAccumulation());
    PaintLayerPainter(*paint_info.paint_layer)
        .Paint(context, painting_info, paint_layer_flags);
  }
}

// TODO(eseckler): Make recording distance configurable, e.g. for use in
// headless, where we would like to record an exact area.
static const int kPixelDistanceToRecord = 4000;

gfx::Rect CompositedLayerMapping::RecomputeInterestRect(
    const GraphicsLayer* graphics_layer) const {
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());

  gfx::Rect graphics_layer_bounds(graphics_layer->Size());

  FloatClipRect mapping_rect((gfx::RectF(graphics_layer_bounds)));

  auto source_state = graphics_layer->GetPropertyTreeState();

  LayoutView* root_view = owning_layer_->GetLayoutObject().View();
  while (root_view->GetFrame()->OwnerLayoutObject())
    root_view = root_view->GetFrame()->OwnerLayoutObject()->View();

  auto root_view_state = root_view->FirstFragment().LocalBorderBoxProperties();

  // 1. Move into local transform space.
  mapping_rect.Move(
      gfx::Vector2dF(graphics_layer->GetOffsetFromTransformNode()));
  // 2. Map into visible space of the root LayoutView.
  GeometryMapper::LocalToAncestorVisualRect(source_state, root_view_state,
                                            mapping_rect);

  gfx::RectF visible_content_rect =
      gfx::RectF(gfx::ToEnclosingRect(mapping_rect.Rect()));

  gfx::RectF local_interest_rect;
  // If the visible content rect is empty, then it makes no sense to map it
  // back since there is nothing to map.
  if (!visible_content_rect.IsEmpty()) {
    local_interest_rect = visible_content_rect;
    // 3. Map the visible content rect from root view space to local graphics
    // layer space.
    GeometryMapper::SourceToDestinationRect(root_view_state.Transform(),
                                            source_state.Transform(),
                                            local_interest_rect);
    local_interest_rect.Offset(-graphics_layer->GetOffsetFromTransformNode());

    // TODO(chrishtr): the code below is a heuristic. Instead we should detect
    // and return whether the mapping failed.  In some cases,
    // absoluteToLocalQuad can fail to map back to the local space, due to
    // passing through non-invertible transforms or floating-point accuracy
    // issues. Examples include rotation near 90 degrees or perspective. In
    // such cases, fall back to painting the first kPixelDistanceToRecord
    // pixels in each direction.

    // Note that since the interest rect mapping above can produce extremely
    // large numbers in cases of perspective, try our best to "normalize" the
    // result by ensuring that none of the rect dimensions exceed some large,
    // but reasonable, limit.
    const float reasonable_pixel_limit = std::numeric_limits<int>::max() / 2.f;
    auto unpadded_intersection = local_interest_rect;

    // Note that by clamping X and Y, we are effectively moving the rect right
    // / down. However, this will at most make us paint more content, which is
    // better than erroneously deciding that the rect produced here is far
    // offscreen.
    if (unpadded_intersection.x() < -reasonable_pixel_limit)
      unpadded_intersection.set_x(-reasonable_pixel_limit);
    if (unpadded_intersection.y() < -reasonable_pixel_limit)
      unpadded_intersection.set_y(-reasonable_pixel_limit);
    if (unpadded_intersection.right() > reasonable_pixel_limit) {
      unpadded_intersection.set_width(reasonable_pixel_limit -
                                      unpadded_intersection.x());
    }
    if (unpadded_intersection.bottom() > reasonable_pixel_limit) {
      unpadded_intersection.set_height(reasonable_pixel_limit -
                                       unpadded_intersection.y());
    }

    unpadded_intersection.Intersect(gfx::RectF(graphics_layer_bounds));
    // If our unpadded intersection is not empty, then use that before
    // padding, since it can produce more stable results, and it would not
    // produce any smaller area than if we used the original local interest
    // rect.
    if (!unpadded_intersection.IsEmpty())
      local_interest_rect = unpadded_intersection;

    // Expand by interest rect padding amount, scaled by the approximate scale
    // of the GraphicsLayer relative to screen pixels. If width or height
    // are zero or nearly zero, fall back to kPixelDistanceToRecord.
    // This is the same as the else clause below.
    float x_scale =
        visible_content_rect.width() > std::numeric_limits<float>::epsilon()
            ? local_interest_rect.width() / visible_content_rect.width()
            : 1.0f;
    float y_scale =
        visible_content_rect.height() > std::numeric_limits<float>::epsilon()
            ? local_interest_rect.height() / visible_content_rect.height()
            : 1.0f;
    // Take the max, to account for situations like rotation transforms, which
    // swap x and y.
    // Since at this point we can also have an extremely large scale due to
    // perspective (see the comments above), cap it to something reasonable.
    float scale = std::min(std::max(x_scale, y_scale),
                           reasonable_pixel_limit / kPixelDistanceToRecord);
    local_interest_rect.Outset(kPixelDistanceToRecord * scale);
  } else {
    // Expand by interest rect padding amount.
    local_interest_rect.Outset(kPixelDistanceToRecord);
  }
  return gfx::IntersectRects(gfx::ToEnclosingRect(local_interest_rect),
                             graphics_layer_bounds);
}

static const int kMinimumDistanceBeforeRepaint = 512;

bool CompositedLayerMapping::InterestRectChangedEnoughToRepaint(
    const gfx::Rect& previous_interest_rect,
    const gfx::Rect& new_interest_rect,
    const gfx::Size& layer_size) {
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());

  if (previous_interest_rect.IsEmpty() && new_interest_rect.IsEmpty())
    return false;

  // Repaint when going from empty to not-empty, to cover cases where the
  // layer is painted for the first time, or otherwise becomes visible.
  if (previous_interest_rect.IsEmpty())
    return true;

  // Repaint if the new interest rect includes area outside of a skirt around
  // the existing interest rect.
  gfx::Rect expanded_previous_interest_rect(previous_interest_rect);
  expanded_previous_interest_rect.Outset(kMinimumDistanceBeforeRepaint);
  if (!expanded_previous_interest_rect.Contains(new_interest_rect))
    return true;

  // Even if the new interest rect doesn't include enough new area to satisfy
  // the condition above, repaint anyway if it touches a layer edge not
  // touched by the existing interest rect.  Because it's impossible to expose
  // more area in the direction, repainting cannot be deferred until the
  // exposed new area satisfies the condition above.
  if (new_interest_rect.x() == 0 && previous_interest_rect.x() != 0)
    return true;
  if (new_interest_rect.y() == 0 && previous_interest_rect.y() != 0)
    return true;
  if (new_interest_rect.right() == layer_size.width() &&
      previous_interest_rect.right() != layer_size.width())
    return true;
  if (new_interest_rect.bottom() == layer_size.height() &&
      previous_interest_rect.bottom() != layer_size.height())
    return true;

  return false;
}

gfx::Rect CompositedLayerMapping::ComputeInterestRect(
    const GraphicsLayer* graphics_layer,
    const gfx::Rect& previous_interest_rect) const {
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());

  // Use the previous interest rect if it covers the whole layer.
  gfx::Rect whole_layer_rect(graphics_layer->Size());
  if (!NeedsRepaint(*graphics_layer) &&
      previous_interest_rect == whole_layer_rect)
    return previous_interest_rect;

  if (graphics_layer != graphics_layer_ &&
      graphics_layer != non_scrolling_squashing_layer_ &&
      graphics_layer != scrolling_contents_layer_)
    return whole_layer_rect;

  gfx::Rect new_interest_rect = RecomputeInterestRect(graphics_layer);
  if (NeedsRepaint(*graphics_layer) ||
      InterestRectChangedEnoughToRepaint(
          previous_interest_rect, new_interest_rect, graphics_layer->Size()))
    return new_interest_rect;
  return previous_interest_rect;
}

gfx::Rect CompositedLayerMapping::PaintableRegion(
    const GraphicsLayer* graphics_layer) const {
  DCHECK(RuntimeEnabledFeatures::CullRectUpdateEnabled());
  const auto& fragment =
      OwningLayer().GetLayoutObject().PrimaryStitchingFragment();
  CullRect cull_rect =
      graphics_layer == scrolling_contents_layer_ ||
              (graphics_layer == foreground_layer_ && scrolling_contents_layer_)
          ? fragment.GetContentsCullRect()
          : fragment.GetCullRect();
  gfx::Rect layer_rect(graphics_layer->Size());
  if (cull_rect.IsInfinite())
    return layer_rect;
  cull_rect.Move(-graphics_layer->GetOffsetFromTransformNode());
  return IntersectRects(cull_rect.Rect(), layer_rect);
}

LayoutSize CompositedLayerMapping::SubpixelAccumulation() const {
  return owning_layer_->SubpixelAccumulation().ToLayoutSize();
}

bool CompositedLayerMapping::NeedsRepaint(
    const GraphicsLayer& graphics_layer) const {
  return IsScrollableAreaLayerWhichNeedsRepaint(&graphics_layer) ||
         owning_layer_->SelfOrDescendantNeedsRepaint();
}

bool CompositedLayerMapping::AdjustForCompositedScrolling(
    const GraphicsLayer* graphics_layer,
    gfx::Vector2d& offset) const {
  if (graphics_layer == scrolling_contents_layer_ ||
      graphics_layer == foreground_layer_) {
    if (PaintLayerScrollableArea* scrollable_area =
            owning_layer_->GetScrollableArea()) {
      if (scrollable_area->UsesCompositedScrolling()) {
        // Note: this is the offset from the beginning of flow of the block,
        // not the offset from the top/left of the overflow rect.
        // offsetFromLayoutObject adds the origin offset from top/left to the
        // beginning of flow.
        ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
        offset -= gfx::ToFlooredVector2d(scroll_offset);
        return true;
      }
    }
  }
  return false;
}

static constexpr PaintLayerFlags PaintLayerFlagsFromGraphicsLayerPaintingPhase(
    GraphicsLayerPaintingPhase graphics_layer_painting_phase) {
  PaintLayerFlags paint_layer_flags = 0;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintBackground)
    paint_layer_flags |= kPaintLayerPaintingCompositingBackgroundPhase;
  else
    paint_layer_flags |= kPaintLayerPaintingSkipRootBackground;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintForeground)
    paint_layer_flags |= kPaintLayerPaintingCompositingForegroundPhase;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintMask)
    paint_layer_flags |= kPaintLayerPaintingCompositingMaskPhase;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintOverflowContents)
    paint_layer_flags |= kPaintLayerPaintingOverflowContents;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintCompositedScroll)
    paint_layer_flags |= kPaintLayerPaintingCompositingScrollingPhase;
  if (graphics_layer_painting_phase & kGraphicsLayerPaintDecoration)
    paint_layer_flags |= kPaintLayerPaintingCompositingDecorationPhase;
  return paint_layer_flags;
}

// Always paint all phases for squashed layers.
static constexpr PaintLayerFlags kPaintLayerFlagsForSquashedLayer =
    PaintLayerFlagsFromGraphicsLayerPaintingPhase(
        kGraphicsLayerPaintAllWithOverflowClip);

void CompositedLayerMapping::PaintContents(
    const GraphicsLayer* graphics_layer,
    GraphicsContext& context,
    GraphicsLayerPaintingPhase graphics_layer_painting_phase,
    const gfx::Rect& interest_rect_arg) const {
  gfx::Rect interest_rect = RuntimeEnabledFeatures::CullRectUpdateEnabled()
                                ? PaintableRegion(graphics_layer)
                                : interest_rect_arg;

  FramePaintTiming frame_paint_timing(context, GetLayoutObject().GetFrame());

#if DCHECK_IS_ON()
  // FIXME: once the state machine is ready, this can be removed and we can
  // refer to that instead.
  if (Page* page = GetLayoutObject().GetFrame()->GetPage())
    page->SetIsPainting(true);
#endif

  DCHECK(owning_layer_->GetLayoutObject()
             .GetFrameView()
             ->LocalFrameTreeAllowsThrottling());

  DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
      "devtools.timeline,rail", "Paint", inspector_paint_event::Data,
      owning_layer_->GetLayoutObject().GetFrame(),
      &owning_layer_->GetLayoutObject(),
      owning_layer_->GetLayoutObject().LocalToAbsoluteQuad(
          FloatQuad(interest_rect),
          kTraverseDocumentBoundaries | kUseGeometryMapperMode),
      graphics_layer->CcLayer().id());

  PaintLayerFlags paint_layer_flags =
      PaintLayerFlagsFromGraphicsLayerPaintingPhase(
          graphics_layer_painting_phase);

  if (graphics_layer == graphics_layer_ ||
      graphics_layer == foreground_layer_ || graphics_layer == mask_layer_ ||
      graphics_layer == scrolling_contents_layer_ ||
      graphics_layer == decoration_outline_layer_) {
    if (BackgroundPaintsOntoScrollingContentsLayer()) {
      if (graphics_layer == scrolling_contents_layer_)
        paint_layer_flags &= ~kPaintLayerPaintingSkipRootBackground;
      else if (!BackgroundPaintsOntoGraphicsLayer())
        paint_layer_flags |= kPaintLayerPaintingSkipRootBackground;
    }

    GraphicsLayerPaintInfo paint_info;
    paint_info.paint_layer = owning_layer_;
    paint_info.composited_bounds = CompositedBounds();
    paint_info.offset_from_layout_object =
        graphics_layer->OffsetFromLayoutObject();
    AdjustForCompositedScrolling(graphics_layer,
                                 paint_info.offset_from_layout_object);

    // We have to use the same root as for hit testing, because both methods
    // can compute and cache clipRects.
    DoPaintTask(paint_info, *graphics_layer, paint_layer_flags, context,
                interest_rect);

    if (graphics_layer == scrolling_contents_layer_ &&
        !squashed_layers_in_scrolling_contents_.IsEmpty()) {
      // We have squashed_layers_in_scrolling_contents_ only if owning_layer_
      // is not a stacking context, thus doesn't have foreground_layer_.
      // (Otherwise we would need to squash into foreground_layer_.)
      DCHECK(!foreground_layer_);
      for (auto& squashed_layer : squashed_layers_in_scrolling_contents_) {
        DoPaintTask(*squashed_layer, *graphics_layer,
                    kPaintLayerFlagsForSquashedLayer, context, interest_rect);
      }
    }
  } else if (graphics_layer == non_scrolling_squashing_layer_) {
    DCHECK_EQ(kPaintLayerFlagsForSquashedLayer, paint_layer_flags);
    for (auto& squashed_layer : non_scrolling_squashed_layers_) {
      DoPaintTask(*squashed_layer, *graphics_layer, paint_layer_flags, context,
                  interest_rect);
    }
  } else if (IsScrollableAreaLayer(graphics_layer)) {
    PaintScrollableArea(graphics_layer, context, interest_rect);
  }

#if DCHECK_IS_ON()
  if (Page* page = GetLayoutObject().GetFrame()->GetPage())
    page->SetIsPainting(false);
#endif
}

void CompositedLayerMapping::PaintScrollableArea(
    const GraphicsLayer* graphics_layer,
    GraphicsContext& context,
    const gfx::Rect& interest_rect) const {
  if (GetLayoutObject().StyleRef().Visibility() != EVisibility::kVisible)
    return;

  // cull_rect is in the space of the containing scrollable area in which
  // Scrollbar::Paint() will paint the scrollbar.
  CullRect cull_rect(interest_rect);
  cull_rect.Move(graphics_layer->OffsetFromLayoutObject());
  PaintLayerScrollableArea* scrollable_area =
      owning_layer_->GetScrollableArea();
  ScrollableAreaPainter painter(*scrollable_area);
  if (graphics_layer == LayerForHorizontalScrollbar()) {
    if (Scrollbar* scrollbar = scrollable_area->HorizontalScrollbar())
      painter.PaintScrollbar(context, *scrollbar, gfx::Vector2d(), cull_rect);
  } else if (graphics_layer == LayerForVerticalScrollbar()) {
    if (Scrollbar* scrollbar = scrollable_area->VerticalScrollbar())
      painter.PaintScrollbar(context, *scrollbar, gfx::Vector2d(), cull_rect);
  } else if (graphics_layer == LayerForScrollCorner()) {
    painter.PaintScrollCorner(context, gfx::Vector2d(), cull_rect);
    painter.PaintResizer(context, gfx::Vector2d(), cull_rect);
  }
}

bool CompositedLayerMapping::IsScrollableAreaLayer(
    const GraphicsLayer* graphics_layer) const {
  return graphics_layer == LayerForHorizontalScrollbar() ||
         graphics_layer == LayerForVerticalScrollbar() ||
         graphics_layer == LayerForScrollCorner();
}

bool CompositedLayerMapping::IsScrollableAreaLayerWhichNeedsRepaint(
    const GraphicsLayer* graphics_layer) const {
  if (PaintLayerScrollableArea* scrollable_area =
          owning_layer_->GetScrollableArea()) {
    if (graphics_layer == LayerForHorizontalScrollbar())
      return scrollable_area->HorizontalScrollbarNeedsPaintInvalidation();

    if (graphics_layer == LayerForVerticalScrollbar())
      return scrollable_area->VerticalScrollbarNeedsPaintInvalidation();

    if (graphics_layer == LayerForScrollCorner())
      return scrollable_area->ScrollCornerNeedsPaintInvalidation();
  }

  return false;
}

bool CompositedLayerMapping::ShouldSkipPaintingSubtree() const {
  return GetLayoutObject().GetFrame()->ShouldThrottleRendering() ||
         owning_layer_->IsUnderSVGHiddenContainer() ||
         DisplayLockUtilities::LockedAncestorPreventingPaint(GetLayoutObject());
}

bool CompositedLayerMapping::IsTrackingRasterInvalidations() const {
  return GetLayoutObject()
      .GetFrame()
      ->LocalFrameRoot()
      .View()
      ->IsTrackingRasterInvalidations();
}

void CompositedLayerMapping::GraphicsLayersDidChange() {
  LocalFrameView* frame_view = GetLayoutObject().GetFrameView();
  DCHECK(frame_view);
  frame_view->SetPaintArtifactCompositorNeedsUpdate();
}

PaintArtifactCompositor* CompositedLayerMapping::GetPaintArtifactCompositor() {
  LocalFrameView* frame_view = GetLayoutObject().GetFrameView();
  DCHECK(frame_view);
  return frame_view->GetPaintArtifactCompositor();
}

void CompositedLayerMapping::Trace(Visitor* visitor) const {
  visitor->Trace(owning_layer_);
  visitor->Trace(graphics_layer_);
  visitor->Trace(scrolling_contents_layer_);
  visitor->Trace(mask_layer_);
  visitor->Trace(foreground_layer_);
  visitor->Trace(layer_for_horizontal_scrollbar_);
  visitor->Trace(layer_for_vertical_scrollbar_);
  visitor->Trace(layer_for_scroll_corner_);
  visitor->Trace(decoration_outline_layer_);
  visitor->Trace(non_scrolling_squashing_layer_);
  visitor->Trace(non_scrolling_squashed_layers_);
  visitor->Trace(squashed_layers_in_scrolling_contents_);
  GraphicsLayerClient::Trace(visitor);
}

#if DCHECK_IS_ON()
void CompositedLayerMapping::VerifyNotPainting() {
  DCHECK(!GetLayoutObject().GetFrame()->GetPage() ||
         !GetLayoutObject().GetFrame()->GetPage()->IsPainting());
}
#endif

bool CompositedLayerMapping::UpdateSquashingLayerAssignmentInternal(
    HeapVector<Member<GraphicsLayerPaintInfo>>& squashed_layers,
    PaintLayer& squashed_layer,
    wtf_size_t next_squashed_layer_index) {
  GraphicsLayerPaintInfo* paint_info =
      MakeGarbageCollected<GraphicsLayerPaintInfo>();
  paint_info->paint_layer = &squashed_layer;
  // NOTE: composited bounds are updated elsewhere
  // NOTE: offsetFromLayoutObject is updated elsewhere

  // Change tracking on squashing layers: at the first sign of something
  // changed, just invalidate the layer.
  // FIXME: Perhaps we can find a tighter more clever mechanism later.
  if (next_squashed_layer_index < squashed_layers.size()) {
    if (paint_info->paint_layer ==
        squashed_layers[next_squashed_layer_index]->paint_layer)
      return false;
    squashed_layers.insert(next_squashed_layer_index, paint_info);
  } else {
    squashed_layers.push_back(paint_info);
  }
  // Must invalidate before adding the squashed layer to the mapping.
  Compositor()->PaintInvalidationOnCompositingChange(&squashed_layer);
  squashed_layer.SetGroupedMapping(
      this, PaintLayer::kInvalidateLayerAndRemoveFromMapping);

  return true;
}

bool CompositedLayerMapping::UpdateSquashingLayerAssignment(
    PaintLayer& squashed_layer,
    wtf_size_t next_squashed_layer_in_non_scrolling_squashing_layer_index,
    wtf_size_t next_squashed_layer_in_scrolling_contents_index) {
  if (MayBeSquashedIntoScrollingContents(squashed_layer)) {
    return UpdateSquashingLayerAssignmentInternal(
        squashed_layers_in_scrolling_contents_, squashed_layer,
        next_squashed_layer_in_scrolling_contents_index);
  }
  return UpdateSquashingLayerAssignmentInternal(
      non_scrolling_squashed_layers_, squashed_layer,
      next_squashed_layer_in_non_scrolling_squashing_layer_index);
}

void CompositedLayerMapping::RemoveLayerFromSquashingGraphicsLayer(
    const PaintLayer& layer) {
  // We must try to remove the layer from both vectors because
  // MayBeSquashedIntoScrollingContents() may not reflect the previous status.
  for (wtf_size_t i = 0; i < non_scrolling_squashed_layers_.size(); ++i) {
    if (non_scrolling_squashed_layers_[i]->paint_layer == &layer) {
      non_scrolling_squashed_layers_.EraseAt(i);
      return;
    }
  }
  for (wtf_size_t i = 0; i < squashed_layers_in_scrolling_contents_.size();
       ++i) {
    if (squashed_layers_in_scrolling_contents_[i]->paint_layer == &layer) {
      squashed_layers_in_scrolling_contents_.EraseAt(i);
      return;
    }
  }

  // Assert on incorrect mappings between layers and groups
  NOTREACHED();
}

static bool LayerInSquashedLayersVector(
    const HeapVector<Member<GraphicsLayerPaintInfo>>& squashed_layers,
    const PaintLayer& layer) {
  for (auto& squashed_layer : squashed_layers) {
    if (squashed_layer->paint_layer == &layer)
      return true;
  }
  return false;
}

#if DCHECK_IS_ON()
void CompositedLayerMapping::AssertInSquashedLayersVector(
    const PaintLayer& squashed_layer) const {
  auto* in = &non_scrolling_squashed_layers_;
  auto* out = &squashed_layers_in_scrolling_contents_;
  if (MayBeSquashedIntoScrollingContents(squashed_layer))
    std::swap(in, out);
  DCHECK(LayerInSquashedLayersVector(*in, squashed_layer));
  DCHECK(!LayerInSquashedLayersVector(*out, squashed_layer));
}
#endif

static void RemoveExtraSquashedLayers(
    HeapVector<Member<GraphicsLayerPaintInfo>>& squashed_layers,
    wtf_size_t new_count,
    HeapVector<Member<PaintLayer>>& layers_needing_paint_invalidation) {
  DCHECK_GE(squashed_layers.size(), new_count);
  if (squashed_layers.size() == new_count)
    return;
  for (auto i = new_count; i < squashed_layers.size(); i++) {
    layers_needing_paint_invalidation.push_back(
        squashed_layers[i]->paint_layer);
  }
  squashed_layers.Shrink(new_count);
}

void CompositedLayerMapping::FinishAccumulatingSquashingLayers(
    wtf_size_t new_non_scrolling_squashed_layer_count,
    wtf_size_t new_squashed_layer_in_scrolling_contents_count,
    HeapVector<Member<PaintLayer>>& layers_needing_paint_invalidation) {
  wtf_size_t first_removed_layer = layers_needing_paint_invalidation.size();
  RemoveExtraSquashedLayers(non_scrolling_squashed_layers_,
                            new_non_scrolling_squashed_layer_count,
                            layers_needing_paint_invalidation);
  RemoveExtraSquashedLayers(squashed_layers_in_scrolling_contents_,
                            new_squashed_layer_in_scrolling_contents_count,
                            layers_needing_paint_invalidation);
  for (auto i = first_removed_layer;
       i < layers_needing_paint_invalidation.size(); i++) {
    PaintLayer* layer = layers_needing_paint_invalidation[i];
    // Deal with layers that are no longer squashed. Need to check both
    // vectors to exclude the layers that are still squashed. A layer may
    // change from scrolling to non-scrolling or vice versa and still be
    // squashed.
    if (!LayerInSquashedLayersVector(non_scrolling_squashed_layers_, *layer) &&
        !LayerInSquashedLayersVector(squashed_layers_in_scrolling_contents_,
                                     *layer)) {
      Compositor()->PaintInvalidationOnCompositingChange(layer);
      layer->SetGroupedMapping(
          nullptr, PaintLayer::kDoNotInvalidateLayerAndRemoveFromMapping);
      layer->SetLostGroupedMapping(true);
    }
  }
}

String CompositedLayerMapping::DebugName(
    const GraphicsLayer* graphics_layer) const {
  String name;
  if (graphics_layer == graphics_layer_) {
    name = owning_layer_->DebugName();
  } else if (graphics_layer == non_scrolling_squashing_layer_) {
    name = "Squashing Layer (first squashed layer: " +
           (non_scrolling_squashed_layers_.size() > 0
                ? non_scrolling_squashed_layers_[0]->paint_layer->DebugName()
                : "") +
           ")";
  } else if (graphics_layer == foreground_layer_) {
    name = owning_layer_->DebugName() + " (foreground) Layer";
  } else if (graphics_layer == mask_layer_) {
    name = "Mask Layer";
  } else if (graphics_layer == layer_for_horizontal_scrollbar_) {
    name = "Horizontal Scrollbar Layer";
  } else if (graphics_layer == layer_for_vertical_scrollbar_) {
    name = "Vertical Scrollbar Layer";
  } else if (graphics_layer == layer_for_scroll_corner_) {
    name = "Scroll Corner Layer";
  } else if (graphics_layer == scrolling_contents_layer_) {
    name = "Scrolling Contents Layer";
  } else if (graphics_layer == decoration_outline_layer_) {
    name = "Decoration Layer";
  } else {
    NOTREACHED();
  }

  return name;
}

const ScrollableArea* CompositedLayerMapping::GetScrollableAreaForTesting(
    const GraphicsLayer* layer) const {
  if (layer == scrolling_contents_layer_)
    return owning_layer_->GetScrollableArea();
  return nullptr;
}

}  // namespace blink
