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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/accessibility/apply_dark_mode.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_html_canvas.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
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
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/compositor_filter_operations.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

static PhysicalRect ContentsRect(const LayoutObject& layout_object) {
  if (!layout_object.IsBox())
    return PhysicalRect();
  if (layout_object.IsLayoutReplaced())
    return ToLayoutReplaced(layout_object).ReplacedContentRect();
  return ToLayoutBox(layout_object).PhysicalContentBoxRect();
}

static PhysicalRect BackgroundRect(const LayoutObject& layout_object) {
  if (!layout_object.IsBox())
    return PhysicalRect();

  const LayoutBox& box = ToLayoutBox(layout_object);
  return box.PhysicalBackgroundRect(kBackgroundClipRect);
}

static inline bool IsTextureLayerCanvas(const LayoutObject& layout_object) {
  if (layout_object.IsCanvas()) {
    auto* canvas = To<HTMLCanvasElement>(layout_object.GetNode());
    if (canvas->SurfaceLayerBridge())
      return false;
    if (CanvasRenderingContext* context = canvas->RenderingContext())
      return context->IsComposited();
  }
  return false;
}

static inline bool IsSurfaceLayerCanvas(const LayoutObject& layout_object) {
  if (layout_object.IsCanvas()) {
    auto* canvas = To<HTMLCanvasElement>(layout_object.GetNode());
    return canvas->SurfaceLayerBridge();
  }
  return false;
}

static bool HasBoxDecorationsOrBackgroundImage(const ComputedStyle& style) {
  return style.HasBoxDecorations() || style.HasBackgroundImage();
}

static bool ContentLayerSupportsDirectBackgroundComposition(
    const LayoutObject& layout_object) {
  // No support for decorations - border, border-radius or outline.
  // Only simple background - solid color or transparent.
  if (HasBoxDecorationsOrBackgroundImage(layout_object.StyleRef()))
    return false;

  // If there is no background, there is nothing to support.
  if (!layout_object.StyleRef().HasBackground())
    return true;

  // Simple background that is contained within the contents rect.
  return ContentsRect(layout_object).Contains(BackgroundRect(layout_object));
}

static WebPluginContainerImpl* GetPluginContainer(LayoutObject& layout_object) {
  if (!layout_object.IsEmbeddedObject())
    return nullptr;
  return ToLayoutEmbeddedObject(layout_object).Plugin();
}

// Returns true if the compositor will be responsible for applying the sticky
// position offset for this composited layer.
static bool UsesCompositedStickyPosition(PaintLayer& layer) {
  return layer.GetLayoutObject().StyleRef().HasStickyConstrainedPosition() &&
         layer.AncestorOverflowLayer()->NeedsCompositedScrolling();
}

// Returns the sticky position offset that should be removed from a given layer
// for use in CompositedLayerMapping.
//
// If the layer is not using composited sticky position, this will return
// FloatPoint().
static FloatPoint StickyPositionOffsetForLayer(PaintLayer& layer) {
  if (!UsesCompositedStickyPosition(layer))
    return FloatPoint();

  const StickyConstraintsMap& constraints_map = layer.AncestorOverflowLayer()
                                                    ->GetScrollableArea()
                                                    ->GetStickyConstraintsMap();
  const StickyPositionScrollingConstraints& constraints =
      constraints_map.at(&layer);

  return FloatPoint(constraints.GetOffsetForStickyPosition(constraints_map));
}

static bool NeedsDecorationOutlineLayer(const PaintLayer& paint_layer,
                                        const LayoutObject& layout_object) {
  int min_border_width = std::min(
      layout_object.StyleRef().BorderTopWidth(),
      std::min(layout_object.StyleRef().BorderLeftWidth(),
               std::min(layout_object.StyleRef().BorderRightWidth(),
                        layout_object.StyleRef().BorderBottomWidth())));

  bool could_obscure_decorations =
      (paint_layer.GetScrollableArea() &&
       paint_layer.GetScrollableArea()->UsesCompositedScrolling()) ||
      layout_object.IsCanvas() || layout_object.IsVideo();

  return could_obscure_decorations && layout_object.StyleRef().HasOutline() &&
         layout_object.StyleRef().OutlineOffset() < -min_border_width;
}

CompositedLayerMapping::CompositedLayerMapping(PaintLayer& layer)
    : owning_layer_(layer),
      pending_update_scope_(kGraphicsLayerUpdateNone),
      is_main_frame_layout_view_layer_(false),
      scrolling_contents_are_empty_(false),
      background_paints_onto_scrolling_contents_layer_(false),
      background_paints_onto_graphics_layer_(false),
      draws_background_onto_content_layer_(false) {
  if (layer.IsRootLayer() && GetLayoutObject().GetFrame()->IsMainFrame())
    is_main_frame_layout_view_layer_ = true;

  CreatePrimaryGraphicsLayer();
}

CompositedLayerMapping::~CompositedLayerMapping() {
  // Hits in compositing/squashing/squash-onto-nephew.html.
  DisableCompositingQueryAsserts disabler;

  // Do not leave the destroyed pointer dangling on any Layers that painted to
  // this mapping's squashing layer.
  for (wtf_size_t i = 0; i < squashed_layers_.size(); ++i) {
    PaintLayer* old_squashed_layer = squashed_layers_[i].paint_layer;
    // Assert on incorrect mappings between layers and groups
    DCHECK_EQ(old_squashed_layer->GroupedMapping(), this);
    if (old_squashed_layer->GroupedMapping() == this) {
      old_squashed_layer->SetGroupedMapping(
          nullptr, PaintLayer::kDoNotInvalidateLayerAndRemoveFromMapping);
      old_squashed_layer->SetLostGroupedMapping(true);
    }
  }

  UpdateOverflowControlsLayers(false, false, false);
  UpdateForegroundLayer(false);
  UpdateMaskLayer(false);
  UpdateScrollingLayers(false);
  UpdateSquashingLayers(false);
  DestroyGraphicsLayers();
}

std::unique_ptr<GraphicsLayer> CompositedLayerMapping::CreateGraphicsLayer(
    CompositingReasons reasons,
    SquashingDisallowedReasons squashing_disallowed_reasons) {
  auto graphics_layer = std::make_unique<GraphicsLayer>(*this);

  graphics_layer->SetCompositingReasons(reasons);
  graphics_layer->SetSquashingDisallowedReasons(squashing_disallowed_reasons);
  if (Node* owning_node = owning_layer_.GetLayoutObject().GetNode()) {
    graphics_layer->SetOwnerNodeId(
        static_cast<int>(DOMNodeIds::IdForNode(owning_node)));
  }

  return graphics_layer;
}

void CompositedLayerMapping::CreatePrimaryGraphicsLayer() {
  graphics_layer_ =
      CreateGraphicsLayer(owning_layer_.GetCompositingReasons(),
                          owning_layer_.GetSquashingDisallowedReasons());

  graphics_layer_->SetHitTestable(true);
}

void CompositedLayerMapping::DestroyGraphicsLayers() {
  if (graphics_layer_)
    graphics_layer_->RemoveFromParent();

  graphics_layer_ = nullptr;
  foreground_layer_ = nullptr;
  mask_layer_ = nullptr;

  scrolling_layer_ = nullptr;
  scrolling_contents_layer_ = nullptr;
}

void CompositedLayerMapping::UpdateBackgroundPaintsOntoScrollingContentsLayer(
    bool& invalidate_graphics_layer,
    bool& invalidate_scrolling_contents_layer) {
  invalidate_graphics_layer = false;
  invalidate_scrolling_contents_layer = false;
  // We can only paint the background onto the scrolling contents layer if
  // it would be visually correct and we are using composited scrolling meaning
  // we have a scrolling contents layer to paint it into.
  BackgroundPaintLocation paint_location =
      GetLayoutObject().GetBackgroundPaintLocation();
  bool should_paint_onto_scrolling_contents_layer =
      paint_location & kBackgroundPaintInScrollingContents &&
      owning_layer_.GetScrollableArea()->UsesCompositedScrolling();
  if (should_paint_onto_scrolling_contents_layer !=
      BackgroundPaintsOntoScrollingContentsLayer()) {
    background_paints_onto_scrolling_contents_layer_ =
        should_paint_onto_scrolling_contents_layer;
    // The scrolling contents layer needs to be updated for changed
    // m_backgroundPaintsOntoScrollingContentsLayer.
    if (HasScrollingLayer())
      invalidate_scrolling_contents_layer = true;
  }
  bool should_paint_onto_graphics_layer =
      !background_paints_onto_scrolling_contents_layer_ ||
      paint_location & kBackgroundPaintInGraphicsLayer;
  if (should_paint_onto_graphics_layer !=
      !!background_paints_onto_graphics_layer_) {
    background_paints_onto_graphics_layer_ = should_paint_onto_graphics_layer;
    // The graphics layer needs to be updated for changed
    // m_backgroundPaintsOntoGraphicsLayer.
    invalidate_graphics_layer = true;
  }
}

void CompositedLayerMapping::UpdateContentsOpaque() {
  // If there is a foreground layer, children paint into that layer and
  // not graphics_layer_, and so don't contribute to the opaqueness of the
  // latter.
  bool should_check_children = !foreground_layer_.get();
  if (IsTextureLayerCanvas(GetLayoutObject())) {
    CanvasRenderingContext* context =
        To<HTMLCanvasElement>(GetLayoutObject().GetNode())->RenderingContext();
    cc::Layer* layer = context ? context->CcLayer() : nullptr;
    // Determine whether the external texture layer covers the whole graphics
    // layer. This may not be the case if there are box decorations or
    // shadows.
    if (layer && layer->bounds() == graphics_layer_->CcLayer()->bounds()) {
      // Determine whether the rendering context's external texture layer is
      // opaque.
      if (!context->CreationAttributes().alpha) {
        graphics_layer_->SetContentsOpaque(true);
      } else {
        graphics_layer_->SetContentsOpaque(
            !Color(layer->background_color()).HasAlpha());
      }
    } else {
      graphics_layer_->SetContentsOpaque(false);
    }
  } else if (IsSurfaceLayerCanvas(GetLayoutObject())) {
    // TODO(crbug.com/705019): Contents could be opaque, but that cannot be
    // determined from the main thread. Or can it?
    graphics_layer_->SetContentsOpaque(false);
  } else {
    // For non-root layers, background is painted by the scrolling contents
    // layer if all backgrounds are background attachment local, otherwise
    // background is painted by the primary graphics layer.
    if (HasScrollingLayer() &&
        background_paints_onto_scrolling_contents_layer_) {
      // Backgrounds painted onto the foreground are clipped by the padding box
      // rect.
      // TODO(flackr): This should actually check the entire overflow rect
      // within the scrolling contents layer but since we currently only trigger
      // this for solid color backgrounds the answer will be the same.
      scrolling_contents_layer_->SetContentsOpaque(
          owning_layer_.BackgroundIsKnownToBeOpaqueInRect(
              ToLayoutBox(GetLayoutObject()).PhysicalPaddingBoxRect(),
              should_check_children));

      if (GetLayoutObject().GetBackgroundPaintLocation() &
          kBackgroundPaintInGraphicsLayer) {
        graphics_layer_->SetContentsOpaque(
            owning_layer_.BackgroundIsKnownToBeOpaqueInRect(
                CompositedBounds(), should_check_children));
      } else {
        // If we only paint the background onto the scrolling contents layer we
        // are going to leave a hole in the m_graphicsLayer where the background
        // is so it is not opaque.
        graphics_layer_->SetContentsOpaque(false);
      }
    } else {
      if (HasScrollingLayer())
        scrolling_contents_layer_->SetContentsOpaque(false);
      graphics_layer_->SetContentsOpaque(
          owning_layer_.BackgroundIsKnownToBeOpaqueInRect(
              CompositedBounds(), should_check_children));
    }
  }
}

void CompositedLayerMapping::UpdateRasterizationPolicy() {
  bool transformed_rasterization_allowed =
      !(owning_layer_.GetCompositingReasons() &
        CompositingReason::kComboAllDirectReasons);
  graphics_layer_->CcLayer()->SetTransformedRasterizationAllowed(
      transformed_rasterization_allowed);
  if (squashing_layer_)
    squashing_layer_->CcLayer()->SetTransformedRasterizationAllowed(true);
}

void CompositedLayerMapping::UpdateCompositedBounds() {
  DCHECK_EQ(owning_layer_.Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingUpdate);
  // FIXME: if this is really needed for performance, it would be better to
  // store it on Layer.
  composited_bounds_ = owning_layer_.BoundingBoxForCompositing();
}

GraphicsLayer* CompositedLayerMapping::FrameContentsGraphicsLayer() const {
  Node* node = GetLayoutObject().GetNode();
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node);
  if (!frame_owner)
    return nullptr;

  Document* document = frame_owner->contentDocument();
  if (!document)
    return nullptr;

  LayoutView* layoutView = document->GetLayoutView();
  if (!layoutView)
    return nullptr;

  DCHECK(layoutView->HasLayer());
  PaintLayer* layer = layoutView->Layer();

  // PaintLayerCompositor updates child frames before parents, so in general
  // it is safe to read the child's compositing state here, and to position its
  // main GraphicsLayer in UpdateAfterPartResize.

  // If the child is not yet in compositing mode, there's nothing to do for now.
  // If it becomes composited later, it will mark the parent frame for another
  // compositing update (see PaintLayerCompositor::AttachRootLayer).

  // If the child's rendering is throttled, its lifecycle state may not permit
  // compositing queries.  But in that case, it has not yet entered compositing
  // mode (see above).

  if (layoutView->GetFrameView()->ShouldThrottleRendering())
    return nullptr;
  DCHECK(layer->IsAllowedToQueryCompositingState());
  if (!layer->HasCompositedLayerMapping())
    return nullptr;

  return layer->GetCompositedLayerMapping()->MainGraphicsLayer();
}

void CompositedLayerMapping::UpdateAfterPartResize() {
  if (GetLayoutObject().IsLayoutEmbeddedContent()) {
    if (GraphicsLayer* document_layer = FrameContentsGraphicsLayer()) {
      document_layer->SetPosition(
          FloatPoint(RoundedIntPoint(ContentsBox().offset)));
    }
  }
}

void CompositedLayerMapping::UpdateCompositingReasons() {
  // All other layers owned by this mapping will have the same compositing
  // reason for their lifetime, so they are initialized only when created.
  graphics_layer_->SetCompositingReasons(owning_layer_.GetCompositingReasons());
  graphics_layer_->SetSquashingDisallowedReasons(
      owning_layer_.GetSquashingDisallowedReasons());
}

bool CompositedLayerMapping::UpdateGraphicsLayerConfiguration(
    const PaintLayer* compositing_container) {
  DCHECK_EQ(owning_layer_.Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingUpdate);

  // Note carefully: here we assume that the compositing state of all
  // descendants have been updated already, so it is legitimate to compute and
  // cache the composited bounds for this layer.
  UpdateCompositedBounds();

  PaintLayerCompositor* compositor = Compositor();
  LayoutObject& layout_object = GetLayoutObject();
  const ComputedStyle& style = layout_object.StyleRef();

  bool layer_config_changed = false;

  if (UpdateForegroundLayer(
          compositor->NeedsContentsCompositingLayer(&owning_layer_)))
    layer_config_changed = true;

  if (UpdateScrollingLayers(owning_layer_.NeedsCompositedScrolling()))
    layer_config_changed = true;

  // If the outline needs to draw over the composited scrolling contents layer
  // or scrollbar layers (or video or webgl) it needs to be drawn into a
  // separate layer.
  bool needs_decoration_outline_layer =
      NeedsDecorationOutlineLayer(owning_layer_, layout_object);

  if (UpdateDecorationOutlineLayer(needs_decoration_outline_layer))
    layer_config_changed = true;

  if (UpdateOverflowControlsLayers(RequiresHorizontalScrollbarLayer(),
                                   RequiresVerticalScrollbarLayer(),
                                   RequiresScrollCornerLayer()))
    layer_config_changed = true;

  if (UpdateSquashingLayers(!squashed_layers_.IsEmpty()))
    layer_config_changed = true;

  if (layer_config_changed)
    UpdateInternalHierarchy();

  // A mask layer is not part of the hierarchy proper, it's an auxiliary layer
  // that's plugged into another GraphicsLayer that is part of the hierarchy.
  // It has no parent or child GraphicsLayer. For that reason, we process it
  // here, after the hierarchy has been updated.
  bool has_mask =
      CSSMaskPainter::MaskBoundingBox(GetLayoutObject(), PhysicalOffset())
          .has_value();
  bool has_clip_path =
      ClipPathClipper::LocalClipPathBoundingBox(GetLayoutObject()).has_value();
  if (UpdateMaskLayer(has_mask || has_clip_path)) {
    graphics_layer_->SetMaskLayer(mask_layer_.get());
    layer_config_changed = true;
  }

  UpdateBackgroundColor();

  if (layout_object.IsImage()) {
    if (IsDirectlyCompositedImage()) {
      UpdateImageContents();
    } else if (graphics_layer_->HasContentsLayer()) {
      graphics_layer_->SetContentsToImage(nullptr, Image::kUnspecifiedDecode);
    }
  }

  if (WebPluginContainerImpl* plugin = GetPluginContainer(layout_object)) {
    graphics_layer_->SetContentsToCcLayer(
        plugin->CcLayer(), plugin->PreventContentsOpaqueChangesToCcLayer());
  } else {
    auto* frame_owner =
        DynamicTo<HTMLFrameOwnerElement>(layout_object.GetNode());
    if (frame_owner && frame_owner->ContentFrame()) {
      Frame* frame = frame_owner->ContentFrame();
      if (auto* remote = DynamicTo<RemoteFrame>(frame)) {
        cc::Layer* layer = remote->GetCcLayer();
        graphics_layer_->SetContentsToCcLayer(
            layer, remote->WebLayerHasFixedContentsOpaque());
      }
    } else if (layout_object.IsVideo()) {
      HTMLMediaElement* media_element =
          ToHTMLMediaElement(layout_object.GetNode());
      graphics_layer_->SetContentsToCcLayer(
          media_element->CcLayer(),
          /*prevent_contents_opaque_changes=*/true);
    } else if (layout_object.IsCanvas()) {
      graphics_layer_->SetContentsToCcLayer(
          To<HTMLCanvasElement>(layout_object.GetNode())->ContentsCcLayer(),
          /*prevent_contents_opaque_changes=*/false);
      layer_config_changed = true;
    }
  }
  if (layout_object.IsLayoutEmbeddedContent()) {
    if (PaintLayerCompositor::AttachFrameContentLayersToIframeLayer(
            ToLayoutEmbeddedContent(layout_object)))
      layer_config_changed = true;
  }

  if (layer_config_changed) {
    // Changes to either the internal hierarchy or the mask layer have an impact
    // on painting phases, so we need to update when either are updated.
    UpdatePaintingPhases();
    // Need to update paint LayerState of the changed GraphicsLayers.
    // The pre-paint tree walk does this.
    layout_object.SetNeedsPaintPropertyUpdate();
  }

  UpdateElementId();

  graphics_layer_->SetHasWillChangeTransformHint(
      style.HasWillChangeTransformHint());

  if (style.Preserves3D() && style.HasOpacity() &&
      owning_layer_.Has3DTransformedDescendant()) {
    UseCounter::Count(layout_object.GetDocument(),
                      WebFeature::kOpacityWithPreserve3DQuirk);
  }

  return layer_config_changed;
}

static PhysicalOffset ComputeOffsetFromCompositedAncestor(
    const PaintLayer* layer,
    const PaintLayer* composited_ancestor,
    const PhysicalOffset& local_representative_point_for_fragmentation,
    const FloatPoint& offset_for_sticky_position) {
  // Add in the offset of the composited bounds from the coordinate space of
  // the PaintLayer, since visualOffsetFromAncestor() requires the pre-offset
  // input to be in the space of the PaintLayer. We also need to add in this
  // offset before computation of visualOffsetFromAncestor(), because it affects
  // fragmentation offset if compositedAncestor crosses a pagination boundary.
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
  offset -= PhysicalOffset::FromFloatPointRound(offset_for_sticky_position);
  return offset;
}

void CompositedLayerMapping::ComputeBoundsOfOwningLayer(
    const PaintLayer* composited_ancestor,
    IntRect& local_bounds,
    IntRect& compositing_bounds_relative_to_composited_ancestor,
    PhysicalOffset& offset_from_composited_ancestor,
    IntPoint& snapped_offset_from_composited_ancestor) {
  // HACK(chrishtr): adjust for position of inlines.
  PhysicalOffset local_representative_point_for_fragmentation;
  if (owning_layer_.GetLayoutObject().IsLayoutInline()) {
    local_representative_point_for_fragmentation =
        ToLayoutInline(owning_layer_.GetLayoutObject()).FirstLineBoxTopLeft();
  }
  // Blink will already have applied any necessary offset for sticky positioned
  // elements. If the compositor is handling sticky offsets for this layer, we
  // need to remove the Blink-side offset to avoid double-counting.
  FloatPoint offset_for_sticky_position =
      StickyPositionOffsetForLayer(owning_layer_);
  offset_from_composited_ancestor = ComputeOffsetFromCompositedAncestor(
      &owning_layer_, composited_ancestor,
      local_representative_point_for_fragmentation, offset_for_sticky_position);
  snapped_offset_from_composited_ancestor =
      RoundedIntPoint(offset_from_composited_ancestor);

  PhysicalOffset subpixel_accumulation;
  if (!owning_layer_.Transform() ||
      owning_layer_.Transform()->IsIdentityOrTranslation()) {
    subpixel_accumulation =
        offset_from_composited_ancestor -
        PhysicalOffset(snapped_offset_from_composited_ancestor);
  }

  // Invalidate the whole layer when subpixel accumulation changes, since
  // the previous subpixel accumulation is baked into the dispay list.
  // However, don't do so for directly composited layers, to avoid impacting
  // performance.
  if (subpixel_accumulation != owning_layer_.SubpixelAccumulation()) {
    // Always invalidate if under-invalidation checking is on, to avoid
    // false positives.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
      SetContentsNeedDisplay();
    else if (!(owning_layer_.GetCompositingReasons() &
               CompositingReason::kComboAllDirectReasons))
      SetContentsNeedDisplay();
  }

  // Otherwise discard the sub-pixel remainder because paint offset can't be
  // transformed by a non-translation transform.
  owning_layer_.SetSubpixelAccumulation(subpixel_accumulation);

  base::Optional<IntRect> mask_bounding_box =
      CSSMaskPainter::MaskBoundingBox(GetLayoutObject(), subpixel_accumulation);
  base::Optional<FloatRect> clip_path_bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(GetLayoutObject());
  if (clip_path_bounding_box)
    clip_path_bounding_box->MoveBy(FloatPoint(subpixel_accumulation));

  // Override graphics layer size to the bound of mask layer, this is because
  // the compositor implementation requires mask layer bound to match its
  // host layer.
  if (mask_bounding_box) {
    local_bounds = *mask_bounding_box;
    if (clip_path_bounding_box)
      local_bounds.Intersect(EnclosingIntRect(*clip_path_bounding_box));
  } else if (clip_path_bounding_box) {
    local_bounds = EnclosingIntRect(*clip_path_bounding_box);
  } else {
    // Move the bounds by the subpixel accumulation so that it pixel-snaps
    // relative to absolute pixels instead of local coordinates.
    PhysicalRect local_raw_compositing_bounds = CompositedBounds();
    local_raw_compositing_bounds.Move(subpixel_accumulation);
    local_bounds = PixelSnappedIntRect(local_raw_compositing_bounds);
  }

  compositing_bounds_relative_to_composited_ancestor = local_bounds;
  compositing_bounds_relative_to_composited_ancestor.MoveBy(
      snapped_offset_from_composited_ancestor);
}

void CompositedLayerMapping::UpdateSquashingLayerGeometry(
    const IntPoint& graphics_layer_parent_location,
    const PaintLayer* compositing_container,
    const IntPoint& snapped_offset_from_composited_ancestor,
    Vector<GraphicsLayerPaintInfo>& layers,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  if (!squashing_layer_)
    return;

  PhysicalOffset compositing_container_offset_from_parent_graphics_layer(
      -graphics_layer_parent_location);
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
    common_transform_ancestor = owning_layer_.Root();
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
        layers[i].paint_layer->BoundingBoxForCompositing();

    // Store the local bounds of the Layer subtree before applying the offset.
    layers[i].composited_bounds = squashed_bounds;

    PhysicalOffset squashed_layer_offset_from_transformed_ancestor =
        layers[i].paint_layer->ComputeOffsetFromAncestor(
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
  const IntRect squash_layer_bounds = EnclosingIntRect(total_squash_bounds);
  const IntPoint squash_layer_origin = squash_layer_bounds.Location();
  const PhysicalOffset squash_layer_origin_in_compositing_container_space =
      PhysicalOffset(squash_layer_origin) -
      compositing_container_offset_from_parent_graphics_layer;

  // Now that the squashing bounds are known, we can convert the PaintLayer
  // painting offsets from compositingContainer space to the squashing layer
  // space.
  //
  // The painting offset we want to compute for each squashed PaintLayer is
  // essentially the position of the squashed PaintLayer described w.r.t.
  // compositingContainer's origin.  So we just need to convert that point from
  // compositingContainer space to the squashing layer's space. This is done by
  // subtracting squashLayerOriginInCompositingContainerSpace, but then the
  // offset overall needs to be negated because that's the direction that the
  // painting code expects the offset to be.
  for (wtf_size_t i = 0; i < layers.size(); ++i) {
    const PhysicalOffset squashed_layer_offset_from_transformed_ancestor =
        layers[i].paint_layer->ComputeOffsetFromAncestor(
            *common_transform_ancestor);
    const PhysicalOffset offset_from_squash_layer_origin =
        (squashed_layer_offset_from_transformed_ancestor -
         compositing_container_offset_from_transformed_ancestor) -
        squash_layer_origin_in_compositing_container_space;

    IntSize new_offset_from_layout_object =
        -ToIntSize(RoundedIntPoint(offset_from_squash_layer_origin));
    PhysicalOffset subpixel_accumulation =
        offset_from_squash_layer_origin +
        PhysicalOffset(new_offset_from_layout_object);
    if (layers[i].offset_from_layout_object_set &&
        layers[i].offset_from_layout_object != new_offset_from_layout_object) {
      ObjectPaintInvalidator(layers[i].paint_layer->GetLayoutObject())
          .InvalidatePaintIncludingNonCompositingDescendants();
      layers_needing_paint_invalidation.push_back(layers[i].paint_layer);
    }
    layers[i].offset_from_layout_object = new_offset_from_layout_object;
    layers[i].offset_from_layout_object_set = true;

    layers[i].paint_layer->SetSubpixelAccumulation(subpixel_accumulation);
  }

  squashing_layer_->SetPosition(FloatPoint(squash_layer_bounds.Location()));
  squashing_layer_->SetSize(gfx::Size(squash_layer_bounds.Size()));
  // We can't squashing_layer_->SetOffsetFromLayoutObject().
  // Squashing layer has special paint and invalidation logic that already
  // compensated for compositing bounds, setting it here would end up
  // double adjustment.
  auto new_offset = squash_layer_bounds.Location() -
                    snapped_offset_from_composited_ancestor +
                    ToIntSize(graphics_layer_parent_location);
  if (new_offset != squashing_layer_offset_from_layout_object_) {
    squashing_layer_offset_from_layout_object_ = new_offset;
    // Need to update squashing LayerState according to the new offset.
    // The pre-paint tree walk does this.
    GetLayoutObject().SetNeedsPaintPropertyUpdate();
  }

  for (wtf_size_t i = 0; i < layers.size(); ++i) {
    LocalClipRectForSquashedLayer(owning_layer_, layers, layers[i]);
  }
}

void CompositedLayerMapping::UpdateGraphicsLayerGeometry(
    const PaintLayer* compositing_container,
    const PaintLayer* compositing_stacking_context,
    Vector<PaintLayer*>& layers_needing_paint_invalidation,
    GraphicsLayerUpdater::UpdateContext& update_context) {
  DCHECK_EQ(owning_layer_.Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingUpdate);

  IntRect local_compositing_bounds;
  IntRect relative_compositing_bounds;
  PhysicalOffset offset_from_composited_ancestor;
  IntPoint snapped_offset_from_composited_ancestor;
  ComputeBoundsOfOwningLayer(compositing_container, local_compositing_bounds,
                             relative_compositing_bounds,
                             offset_from_composited_ancestor,
                             snapped_offset_from_composited_ancestor);

  IntPoint graphics_layer_parent_location;
  ComputeGraphicsLayerParentLocation(compositing_container,
                                     graphics_layer_parent_location);

  IntSize contents_size(relative_compositing_bounds.Size());

  UpdateMainGraphicsLayerGeometry(
      relative_compositing_bounds, local_compositing_bounds,
      graphics_layer_parent_location, update_context);
  UpdateOverflowControlsHostLayerGeometry(compositing_stacking_context,
                                          compositing_container,
                                          graphics_layer_parent_location);
  UpdateSquashingLayerGeometry(
      graphics_layer_parent_location, compositing_container,
      snapped_offset_from_composited_ancestor, squashed_layers_,
      layers_needing_paint_invalidation);

  UpdateMaskLayerGeometry();
  // TODO(yigu): Currently the decoration layer uses the same contentSize
  // as the foreground layer. There are scenarios where the sizes could be
  // different so the decoration layer size should be calculated separately.
  UpdateDecorationOutlineLayerGeometry(contents_size);
  UpdateScrollingLayerGeometry(local_compositing_bounds);
  UpdateForegroundLayerGeometry();

  if (owning_layer_.GetScrollableArea() &&
      owning_layer_.GetScrollableArea()->ScrollsOverflow())
    owning_layer_.GetScrollableArea()->PositionOverflowControls();

  UpdateContentsRect();
  UpdateBackgroundColor();

  bool invalidate_graphics_layer;
  bool invalidate_scrolling_contents_layer;
  UpdateBackgroundPaintsOntoScrollingContentsLayer(
      invalidate_graphics_layer, invalidate_scrolling_contents_layer);

  // This depends on background_paints_onto_graphics_layer_.
  UpdateDrawsContentAndPaintsHitTest();

  // These invalidations need to happen after
  // |UpdateDrawsContentAndPaintsHitTest|.
  if (invalidate_graphics_layer)
    graphics_layer_->SetNeedsDisplay();
  if (invalidate_scrolling_contents_layer)
    scrolling_contents_layer_->SetNeedsDisplay();

  UpdateElementId();
  UpdateContentsOpaque();
  UpdateRasterizationPolicy();
  UpdateAfterPartResize();
  UpdateCompositingReasons();
}

void CompositedLayerMapping::UpdateMainGraphicsLayerGeometry(
    const IntRect& relative_compositing_bounds,
    const IntRect& local_compositing_bounds,
    const IntPoint& graphics_layer_parent_location,
    GraphicsLayerUpdater::UpdateContext& update_context) {
  FloatPoint old_position(graphics_layer_->GetPosition());
  IntSize old_size(graphics_layer_->Size());

  FloatPoint new_position = FloatPoint(relative_compositing_bounds.Location() -
                                       graphics_layer_parent_location);
  IntSize new_size = relative_compositing_bounds.Size();
  IntSize new_object_offset = -ToIntSize(local_compositing_bounds.Location());
  const LayoutObject& layout_object = GetLayoutObject();

  // An iframe's main GraphicsLayer is positioned by the CLM for the <iframe>
  // element in the parent frame's DOM.
  bool is_iframe_doc =
      layout_object.IsLayoutView() && !layout_object.GetFrame()->IsLocalRoot();
  if (new_position != old_position && !is_iframe_doc)
    graphics_layer_->SetPosition(new_position);
  graphics_layer_->SetOffsetFromLayoutObject(-new_object_offset);

  if (old_size != new_size)
    graphics_layer_->SetSize(gfx::Size(new_size));

  // m_graphicsLayer is the corresponding GraphicsLayer for this PaintLayer and
  // its non-compositing descendants. So, the visibility flag for
  // m_graphicsLayer should be true if there are any non-compositing visible
  // layers.
  bool contents_visible = owning_layer_.HasVisibleContent() ||
                          HasVisibleNonCompositingDescendant(&owning_layer_);
  // TODO(sunxd): Investigate and possibly implement computing hit test regions
  // in PaintTouchActionRects code path, so that cc has correct pointer-events
  // information.
  // For now, there is no need to set graphics_layer_'s hit testable bit here,
  // because it is always hit testable from cc's perspective.
  graphics_layer_->SetContentsVisible(contents_visible);
}

void CompositedLayerMapping::ComputeGraphicsLayerParentLocation(
    const PaintLayer* compositing_container,
    IntPoint& graphics_layer_parent_location) {
  if (compositing_container) {
    graphics_layer_parent_location =
        IntPoint(compositing_container->GetCompositedLayerMapping()
                     ->ParentForSublayers()
                     ->OffsetFromLayoutObject());
  } else if (!GetLayoutObject().GetFrame()->IsLocalRoot()) {  // TODO(oopif)
    DCHECK(!compositing_container);
    graphics_layer_parent_location = IntPoint();
  }

  if (compositing_container &&
      compositing_container->NeedsCompositedScrolling()) {
    LayoutBox& layout_box =
        ToLayoutBox(compositing_container->GetLayoutObject());
    IntSize scroll_offset = FlooredIntSize(layout_box.ScrolledContentOffset());
    IntPoint scroll_origin =
        compositing_container->GetScrollableArea()->ScrollOrigin();
    scroll_origin.Move(-layout_box.OriginAdjustmentForScrollbars());
    scroll_origin.Move(-layout_box.BorderLeft().ToInt(),
                       -layout_box.BorderTop().ToInt());
    graphics_layer_parent_location = -(scroll_origin + scroll_offset);
  }
}

void CompositedLayerMapping::UpdateOverflowControlsHostLayerGeometry(
    const PaintLayer* compositing_stacking_context,
    const PaintLayer* compositing_container,
    IntPoint graphics_layer_parent_location) {
  if (!overflow_controls_host_layer_)
    return;

  // To position and clip the scrollbars correctly,
  // overflow_controls_host_layer_ should match our border box rect, which is at
  // the origin of our LayoutObject. Its position is computed in various ways
  // depending on who its parent GraphicsLayer is going to be.
  PhysicalOffset host_layer_position;

  if (NeedsToReparentOverflowControls()) {
    // This should never be true, but for some reason it is.
    // See https://crbug.com/880930.
    if (!compositing_stacking_context)
      return;

    CompositedLayerMapping* stacking_clm =
        compositing_stacking_context->GetCompositedLayerMapping();
    DCHECK(stacking_clm);

    // overflow_controls_host_layer_ will be a child of the main GraphicsLayer
    // of the compositing stacking context.
    IntSize stacking_offset_from_layout_object =
        stacking_clm->MainGraphicsLayer()->OffsetFromLayoutObject();

    // The controls are in the same 2D space as the compositing container, so
    // we can map them into the space of the container.
    host_layer_position = owning_layer_.GetLayoutObject().LocalToAncestorPoint(
        PhysicalOffset(), &compositing_stacking_context->GetLayoutObject(),
        kIgnoreTransforms);
    if (PaintLayerScrollableArea* scrollable_area =
            compositing_stacking_context->GetScrollableArea()) {
      host_layer_position += PhysicalOffset::FromFloatPointRound(
          scrollable_area->ScrollPosition());
    }
    host_layer_position -= PhysicalOffset(stacking_offset_from_layout_object);
  } else {
    host_layer_position -=
        PhysicalOffset(graphics_layer_->OffsetFromLayoutObject());
  }

  overflow_controls_host_layer_->SetPosition(FloatPoint(host_layer_position));

  const IntRect border_box =
      owning_layer_.GetLayoutBox()->PixelSnappedBorderBoxRect(
          owning_layer_.SubpixelAccumulation());
  overflow_controls_host_layer_->SetSize(gfx::Size(border_box.Size()));
  overflow_controls_host_layer_->SetMasksToBounds(true);
}

void CompositedLayerMapping::UpdateMaskLayerGeometry() {
  if (!mask_layer_)
    return;

  if (mask_layer_->Size() != graphics_layer_->Size()) {
    mask_layer_->SetSize(graphics_layer_->Size());
    mask_layer_->SetNeedsDisplay();
  }
  mask_layer_->SetPosition(FloatPoint());
  mask_layer_->SetOffsetFromLayoutObject(
      graphics_layer_->OffsetFromLayoutObject());
}

void CompositedLayerMapping::UpdateScrollingLayerGeometry(
    const IntRect& local_compositing_bounds) {
  if (!scrolling_layer_)
    return;

  DCHECK(scrolling_contents_layer_);
  LayoutBox& layout_box = ToLayoutBox(GetLayoutObject());
  IntRect overflow_clip_rect = PixelSnappedIntRect(
      layout_box.OverflowClipRect(owning_layer_.SubpixelAccumulation()));

  // When a m_childTransformLayer exists, local content offsets for the
  // m_scrollingLayer have already been applied. Otherwise, we apply them here.
  IntSize local_content_offset(0, 0);
  local_content_offset = RoundedIntPoint(owning_layer_.SubpixelAccumulation()) -
                         local_compositing_bounds.Location();
  scrolling_layer_->SetPosition(
      FloatPoint(overflow_clip_rect.Location() + local_content_offset));

  auto old_scroll_container_size = scrolling_layer_->Size();
  scrolling_layer_->SetSize(gfx::Size(overflow_clip_rect.Size()));
  bool scroll_container_size_changed =
      old_scroll_container_size != scrolling_layer_->Size();

  scrolling_layer_->SetOffsetFromLayoutObject(
      ToIntSize(overflow_clip_rect.Location()));

  PaintLayerScrollableArea* scrollable_area = owning_layer_.GetScrollableArea();
  IntSize scroll_size = scrollable_area->PixelSnappedContentsSize(
      owning_layer_.SubpixelAccumulation());

  // Ensure scrolling contents are at least as large as the scroll clip
  scroll_size = scroll_size.ExpandedTo(overflow_clip_rect.Size());

  auto* scrolling_coordinator = owning_layer_.GetScrollingCoordinator();
  scrolling_coordinator->UpdateCompositedScrollOffset(scrollable_area);

  if (gfx::Size(scroll_size) != scrolling_contents_layer_->Size() ||
      scroll_container_size_changed) {
    scrolling_coordinator->ScrollableAreaScrollLayerDidChange(scrollable_area);
    scrolling_contents_layer_->SetPosition(FloatPoint());
  }

  scrolling_contents_layer_->SetSize(gfx::Size(scroll_size));

  scrolling_contents_layer_->SetOffsetFromLayoutObject(
      overflow_clip_rect.Location() - scrollable_area->ScrollOrigin());
}

bool CompositedLayerMapping::RequiresHorizontalScrollbarLayer() const {
  return owning_layer_.GetScrollableArea() &&
         owning_layer_.GetScrollableArea()->HorizontalScrollbar();
}

bool CompositedLayerMapping::RequiresVerticalScrollbarLayer() const {
  return owning_layer_.GetScrollableArea() &&
         owning_layer_.GetScrollableArea()->VerticalScrollbar();
}

bool CompositedLayerMapping::RequiresScrollCornerLayer() const {
  return owning_layer_.GetScrollableArea() &&
         !owning_layer_.GetScrollableArea()
              ->ScrollCornerAndResizerRect()
              .IsEmpty();
}

void CompositedLayerMapping::UpdateForegroundLayerGeometry() {
  if (!foreground_layer_)
    return;

  // Should be equivalent to local_compositing_bounds.
  IntRect compositing_bounds(
      IntPoint(graphics_layer_->OffsetFromLayoutObject()),
      IntSize(graphics_layer_->Size()));
  if (scrolling_layer_) {
    // Override compositing bounds to include full overflow if composited
    // scrolling is used.
    compositing_bounds =
        IntRect(IntPoint(scrolling_contents_layer_->OffsetFromLayoutObject()),
                IntSize(scrolling_contents_layer_->Size()));
  }

  IntRect old_compositing_bounds(
      IntPoint(foreground_layer_->OffsetFromLayoutObject()),
      IntSize(foreground_layer_->Size()));
  if (compositing_bounds != old_compositing_bounds) {
    foreground_layer_->SetOffsetFromLayoutObject(
        ToIntSize(compositing_bounds.Location()));
    foreground_layer_->SetSize(gfx::Size(compositing_bounds.Size()));
    foreground_layer_->SetNeedsDisplay();
  }
  IntPoint parent_location(ParentForSublayers()->OffsetFromLayoutObject());
  foreground_layer_->SetPosition(
      FloatPoint(compositing_bounds.Location() - parent_location));
}

void CompositedLayerMapping::UpdateDecorationOutlineLayerGeometry(
    const IntSize& relative_compositing_bounds_size) {
  if (!decoration_outline_layer_)
    return;
  const auto& decoration_size = relative_compositing_bounds_size;
  decoration_outline_layer_->SetPosition(FloatPoint());
  if (gfx::Size(decoration_size) != decoration_outline_layer_->Size()) {
    decoration_outline_layer_->SetSize(gfx::Size(decoration_size));
    decoration_outline_layer_->SetNeedsDisplay();
  }
  decoration_outline_layer_->SetOffsetFromLayoutObject(
      graphics_layer_->OffsetFromLayoutObject());
}

void CompositedLayerMapping::UpdateInternalHierarchy() {
  // m_foregroundLayer has to be inserted in the correct order with child
  // layers, so it's not inserted here.
  graphics_layer_->RemoveFromParent();

  // Layer to which children should be attached as we build the hierarchy.
  GraphicsLayer* bottom_layer = graphics_layer_.get();
  auto update_bottom_layer = [&bottom_layer](GraphicsLayer* layer) {
    if (layer) {
      bottom_layer->AddChild(layer);
      bottom_layer = layer;
    }
  };

  update_bottom_layer(scrolling_layer_.get());

  // Now constructing the subtree for the overflow controls.
  bottom_layer = graphics_layer_.get();
  update_bottom_layer(overflow_controls_host_layer_.get());
  if (layer_for_horizontal_scrollbar_) {
    overflow_controls_host_layer_->AddChild(
        layer_for_horizontal_scrollbar_.get());
  }
  if (layer_for_vertical_scrollbar_) {
    overflow_controls_host_layer_->AddChild(
        layer_for_vertical_scrollbar_.get());
  }
  if (layer_for_scroll_corner_)
    overflow_controls_host_layer_->AddChild(layer_for_scroll_corner_.get());

  // Now add the DecorationOutlineLayer as a subtree to GraphicsLayer
  if (decoration_outline_layer_.get())
    graphics_layer_->AddChild(decoration_outline_layer_.get());

  // The squashing containment layer, if it exists, becomes a no-op parent.
  if (squashing_layer_) {
    if (squashing_containment_layer_) {
      squashing_containment_layer_->RemoveAllChildren();
      squashing_containment_layer_->AddChild(graphics_layer_.get());
      squashing_containment_layer_->AddChild(squashing_layer_.get());
    }
  }
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
  graphics_layer_->SetContentsRect(PixelSnappedIntRect(ContentsBox()));
}

void CompositedLayerMapping::UpdateDrawsContentAndPaintsHitTest() {
  bool in_overlay_fullscreen_video = false;
  if (GetLayoutObject().IsVideo()) {
    HTMLVideoElement* video_element =
        ToHTMLVideoElement(GetLayoutObject().GetNode());
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
  bool paints_hit_test =
      has_painted_content || GetLayoutObject().HasEffectiveAllowedTouchAction();
  bool paints_scroll_hit_test =
      ((owning_layer_.GetScrollableArea() &&
        owning_layer_.GetScrollableArea()->ScrollsOverflow()) ||
       (GetPluginContainer(GetLayoutObject()) &&
        GetPluginContainer(GetLayoutObject())->WantsWheelEvents()));
  graphics_layer_->SetPaintsHitTest(paints_hit_test || paints_scroll_hit_test);

  if (scrolling_layer_) {
    // m_scrollingLayer never has backing store.
    // m_scrollingContentsLayer only needs backing store if the scrolled
    // contents need to paint.
    scrolling_contents_are_empty_ =
        !owning_layer_.HasVisibleContent() ||
        !(GetLayoutObject().StyleRef().HasBackground() ||
          GetLayoutObject().HasBackdropFilter() || PaintsChildren());
    scrolling_contents_layer_->SetDrawsContent(!scrolling_contents_are_empty_);
    scrolling_contents_layer_->SetPaintsHitTest(paints_hit_test);
  }

  draws_background_onto_content_layer_ = false;

  if (has_painted_content && IsTextureLayerCanvas(GetLayoutObject())) {
    CanvasRenderingContext* context =
        To<HTMLCanvasElement>(GetLayoutObject().GetNode())->RenderingContext();
    // Content layer may be null if context is lost.
    if (cc::Layer* content_layer = context->CcLayer()) {
      Color bg_color(Color::kTransparent);
      if (ContentLayerSupportsDirectBackgroundComposition(GetLayoutObject())) {
        bg_color = LayoutObjectBackgroundColor();
        has_painted_content = false;
        draws_background_onto_content_layer_ = true;
      }
      content_layer->SetBackgroundColor(bg_color.Rgb());
    }
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
    std::unique_ptr<GraphicsLayer>& layer,
    bool needs_layer,
    CompositingReasons reason) {
  if (needs_layer == !!layer)
    return false;
  layer = needs_layer ? CreateGraphicsLayer(reason) : nullptr;

  if (PaintLayerScrollableArea* scrollable_area =
          owning_layer_.GetScrollableArea()) {
    if (ScrollingCoordinator* scrolling_coordinator =
            owning_layer_.GetScrollingCoordinator()) {
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
          owning_layer_.GetScrollableArea()) {
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

    if (scrolling_contents_layer_ &&
        scrollable_area->NeedsShowScrollbarLayers()) {
      scrolling_contents_layer_->CcLayer()->ShowScrollbars();
      scrollable_area->DidShowScrollbarLayers();
    }
  }

  // If the subtree is invisible, we don't actually need scrollbar layers.
  // Only do this check if at least one of the bits is currently true.
  // This is important because this method is called during the destructor
  // of CompositedLayerMapping, which may happen during style recalc,
  // and therefore visible content status may be invalid.
  if (needs_horizontal_scrollbar_layer || needs_vertical_scrollbar_layer ||
      needs_scroll_corner_layer) {
    bool invisible = owning_layer_.SubtreeIsInvisible();
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

  bool needs_overflow_controls_host_layer = needs_horizontal_scrollbar_layer ||
                                            needs_vertical_scrollbar_layer ||
                                            needs_scroll_corner_layer;
  ToggleScrollbarLayerIfNeeded(
      overflow_controls_host_layer_, needs_overflow_controls_host_layer,
      CompositingReason::kLayerForOverflowControlsHost);

  return horizontal_scrollbar_layer_changed ||
         vertical_scrollbar_layer_changed || scroll_corner_layer_changed;
}

void CompositedLayerMapping::PositionOverflowControlsLayers() {
  if (GraphicsLayer* layer = LayerForHorizontalScrollbar()) {
    Scrollbar* h_bar = owning_layer_.GetScrollableArea()->HorizontalScrollbar();
    if (h_bar) {
      IntRect frame_rect = h_bar->FrameRect();
      layer->SetPosition(FloatPoint(frame_rect.Location()));
      layer->SetOffsetFromLayoutObject(ToIntSize(frame_rect.Location()));
      layer->SetSize(gfx::Size(frame_rect.Size()));
      if (layer->HasContentsLayer())
        layer->SetContentsRect(IntRect(IntPoint(), frame_rect.Size()));
    }
    bool h_bar_visible = h_bar && !layer->HasContentsLayer();
    layer->SetDrawsContent(h_bar_visible);
    layer->SetHitTestable(h_bar_visible);
  }

  if (GraphicsLayer* layer = LayerForVerticalScrollbar()) {
    Scrollbar* v_bar = owning_layer_.GetScrollableArea()->VerticalScrollbar();
    if (v_bar) {
      IntRect frame_rect = v_bar->FrameRect();
      layer->SetPosition(FloatPoint(frame_rect.Location()));
      layer->SetOffsetFromLayoutObject(ToIntSize(frame_rect.Location()));
      layer->SetSize(gfx::Size(frame_rect.Size()));
      if (layer->HasContentsLayer())
        layer->SetContentsRect(IntRect(IntPoint(), frame_rect.Size()));
    }
    bool v_bar_visible = v_bar && !layer->HasContentsLayer();
    layer->SetDrawsContent(v_bar_visible);
    layer->SetHitTestable(v_bar_visible);
  }

  if (GraphicsLayer* layer = LayerForScrollCorner()) {
    const IntRect& scroll_corner_and_resizer =
        owning_layer_.GetScrollableArea()->ScrollCornerAndResizerRect();
    layer->SetPosition(FloatPoint(scroll_corner_and_resizer.Location()));
    layer->SetOffsetFromLayoutObject(
        ToIntSize(scroll_corner_and_resizer.Location()));
    layer->SetSize(gfx::Size(scroll_corner_and_resizer.Size()));
    layer->SetDrawsContent(!scroll_corner_and_resizer.IsEmpty());
    layer->SetHitTestable(!scroll_corner_and_resizer.IsEmpty());
  }
}

enum ApplyToGraphicsLayersModeFlags {
  kApplyToLayersAffectedByPreserve3D = (1 << 0),
  kApplyToSquashingLayer = (1 << 1),
  kApplyToScrollbarLayers = (1 << 2),
  kApplyToMaskLayers = (1 << 3),
  kApplyToContentLayers = (1 << 4),
  kApplyToChildContainingLayers =
      (1 << 5),  // layers between m_graphicsLayer and children
  kApplyToNonScrollingContentLayers = (1 << 6),
  kApplyToScrollingContentLayers = (1 << 7),
  kApplyToDecorationOutlineLayer = (1 << 8),
  kApplyToAllGraphicsLayers =
      (kApplyToSquashingLayer | kApplyToScrollbarLayers | kApplyToMaskLayers |
       kApplyToLayersAffectedByPreserve3D | kApplyToContentLayers |
       kApplyToScrollingContentLayers | kApplyToDecorationOutlineLayer)
};
typedef unsigned ApplyToGraphicsLayersMode;

// Flags to layers mapping matrix:
//                  bit 0 1 2 3 4 5 6 7 8
// ChildTransform       *         *
// Main                 *       *   *
// Clipping             *         *
// Scrolling            *         *
// ScrollingContents    *       * *   *
// Foreground           *       *     *
// Squashing              *
// Mask                       * *   *
// HorizontalScrollbar      *
// VerticalScrollbar        *
// ScrollCorner             *
// DecorationOutline                *   *
template <typename Func>
static void ApplyToGraphicsLayers(const CompositedLayerMapping* mapping,
                                  const Func& f,
                                  ApplyToGraphicsLayersMode mode) {
  DCHECK(mode);

  if (((mode & kApplyToLayersAffectedByPreserve3D) ||
       (mode & kApplyToContentLayers) ||
       (mode & kApplyToNonScrollingContentLayers)) &&
      mapping->MainGraphicsLayer())
    f(mapping->MainGraphicsLayer());
  if (((mode & kApplyToLayersAffectedByPreserve3D) ||
       (mode & kApplyToChildContainingLayers)) &&
      mapping->ScrollingLayer())
    f(mapping->ScrollingLayer());
  if (((mode & kApplyToLayersAffectedByPreserve3D) ||
       (mode & kApplyToContentLayers) ||
       (mode & kApplyToChildContainingLayers) ||
       (mode & kApplyToScrollingContentLayers)) &&
      mapping->ScrollingContentsLayer())
    f(mapping->ScrollingContentsLayer());
  if (((mode & kApplyToLayersAffectedByPreserve3D) ||
       (mode & kApplyToContentLayers) ||
       (mode & kApplyToScrollingContentLayers)) &&
      mapping->ForegroundLayer())
    f(mapping->ForegroundLayer());

  if ((mode & kApplyToSquashingLayer) && mapping->SquashingLayer())
    f(mapping->SquashingLayer());

  if (((mode & kApplyToMaskLayers) || (mode & kApplyToContentLayers) ||
       (mode & kApplyToNonScrollingContentLayers)) &&
      mapping->MaskLayer())
    f(mapping->MaskLayer());

  if ((mode & kApplyToScrollbarLayers) &&
      mapping->LayerForHorizontalScrollbar())
    f(mapping->LayerForHorizontalScrollbar());
  if ((mode & kApplyToScrollbarLayers) && mapping->LayerForVerticalScrollbar())
    f(mapping->LayerForVerticalScrollbar());
  if ((mode & kApplyToScrollbarLayers) && mapping->LayerForScrollCorner())
    f(mapping->LayerForScrollCorner());

  if (((mode & kApplyToDecorationOutlineLayer) ||
       (mode & kApplyToNonScrollingContentLayers)) &&
      mapping->DecorationOutlineLayer())
    f(mapping->DecorationOutlineLayer());
}

struct AnimatingData {
  STACK_ALLOCATED();

 public:
  Persistent<Node> owning_node = nullptr;
  Persistent<Element> animating_element = nullptr;
  const ComputedStyle* animating_style = nullptr;
};

// You receive an element id if you have an animation, or you're a scroller (and
// might impl animate).
//
// The element id for the scroll layers is assigned when they're constructed,
// since this is unconditional. However, the element id for the primary layer
// may change according to the rules above so we update those values here.
void CompositedLayerMapping::UpdateElementId() {
  CompositorElementId element_id = CompositorElementIdFromUniqueObjectId(
      owning_layer_.GetLayoutObject().UniqueId(),
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
    foreground_layer_ = nullptr;
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
    decoration_outline_layer_ = nullptr;
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
      if (GetLayoutObject().HasBackdropFilter())
        mask_layer_->CcLayer()->SetIsBackdropFilterMask(true);
      layer_changed = true;
    }
  } else if (mask_layer_) {
    mask_layer_ = nullptr;
    layer_changed = true;
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateScrollingLayers(
    bool needs_scrolling_layers) {
  ScrollingCoordinator* scrolling_coordinator =
      owning_layer_.GetScrollingCoordinator();

  auto* scrollable_area = owning_layer_.GetScrollableArea();
  if (scrollable_area)
    scrollable_area->SetUsesCompositedScrolling(needs_scrolling_layers);

  bool layer_changed = false;
  if (needs_scrolling_layers) {
    if (!scrolling_layer_) {
      // Outer layer which corresponds with the scroll view.
      scrolling_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForScrollingContainer);
      scrolling_layer_->SetDrawsContent(false);
      scrolling_layer_->SetHitTestable(false);
      scrolling_layer_->SetMasksToBounds(true);

      // Inner layer which renders the content that scrolls.
      scrolling_contents_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForScrollingContents);
      scrolling_contents_layer_->SetHitTestable(true);

      auto element_id = scrollable_area->GetCompositorElementId();
      scrolling_contents_layer_->SetElementId(element_id);

      scrolling_layer_->AddChild(scrolling_contents_layer_.get());

      layer_changed = true;
      if (scrolling_coordinator && scrollable_area) {
        scrolling_coordinator->ScrollableAreaScrollLayerDidChange(
            scrollable_area);
        const auto& object = GetLayoutObject();
        if (object.IsLayoutView())
          ToLayoutView(object).GetFrameView()->ScrollableAreasDidChange();
      }
    }
  } else if (scrolling_layer_) {
    scrolling_layer_ = nullptr;
    scrolling_contents_layer_ = nullptr;
    layer_changed = true;
    if (scrolling_coordinator && scrollable_area) {
      scrolling_coordinator->ScrollableAreaScrollLayerDidChange(
          scrollable_area);
      const auto& object = GetLayoutObject();
      if (object.IsLayoutView())
        ToLayoutView(object).GetFrameView()->ScrollableAreasDidChange();
    }
  }

  return layer_changed;
}

bool CompositedLayerMapping::UpdateSquashingLayers(
    bool needs_squashing_layers) {
  bool layers_changed = false;

  if (needs_squashing_layers) {
    if (!squashing_layer_) {
      squashing_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForSquashingContents);
      squashing_layer_->SetDrawsContent(true);
      squashing_layer_->SetHitTestable(true);
      layers_changed = true;
    }
    if (!squashing_containment_layer_) {
      squashing_containment_layer_ =
          CreateGraphicsLayer(CompositingReason::kLayerForSquashingContainer);
      layers_changed = true;
    }
    DCHECK(squashing_layer_);
  } else {
    if (squashing_layer_) {
      squashing_layer_->RemoveFromParent();
      squashing_layer_ = nullptr;
      layers_changed = true;
    }
    if (squashing_containment_layer_) {
      squashing_containment_layer_->RemoveFromParent();
      squashing_containment_layer_ = nullptr;
      layers_changed = true;
    }
    DCHECK(!squashing_layer_);
    DCHECK(!squashing_containment_layer_);
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

Color CompositedLayerMapping::LayoutObjectBackgroundColor() const {
  const auto& object = GetLayoutObject();
  auto background_color = object.ResolveColor(GetCSSPropertyBackgroundColor());
  if (object.IsLayoutView() && object.GetDocument().IsInMainFrame()) {
    return ToLayoutView(object).GetFrameView()->BaseBackgroundColor().Blend(
        background_color);
  }
  return background_color;
}

void CompositedLayerMapping::UpdateBackgroundColor() {
  auto color = LayoutObjectBackgroundColor().Rgb();
  graphics_layer_->SetBackgroundColor(color);
  if (scrolling_contents_layer_)
    scrolling_contents_layer_->SetBackgroundColor(color);
}

bool CompositedLayerMapping::PaintsChildren() const {
  if (owning_layer_.HasVisibleContent() &&
      owning_layer_.HasNonEmptyChildLayoutObjects())
    return true;

  if (HasVisibleNonCompositingDescendant(&owning_layer_))
    return true;

  return false;
}

static bool IsCompositedPlugin(LayoutObject& layout_object) {
  return layout_object.IsEmbeddedObject() &&
         ToLayoutEmbeddedObject(layout_object).RequiresAcceleratedCompositing();
}

bool CompositedLayerMapping::HasVisibleNonCompositingDescendant(
    PaintLayer* parent) {
  if (!parent->HasVisibleDescendant())
    return false;

  PaintLayerPaintOrderIterator iterator(*parent, kAllChildren);
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

  if (GetLayoutObject().IsImage() && IsDirectlyCompositedImage())
    return false;

  LayoutObject& layout_object = GetLayoutObject();
  // FIXME: we could optimize cases where the image, video or canvas is known to
  // fill the border box entirely, and set background color on the layer in that
  // case, instead of allocating backing store and painting.
  if (layout_object.IsVideo() &&
      ToLayoutVideo(layout_object).ShouldDisplayVideo())
    return owning_layer_.HasBoxDecorationsOrBackground();

  if (layout_object.GetNode() && layout_object.GetNode()->IsDocumentNode()) {
    if (owning_layer_.NeedsCompositedScrolling())
      return background_paints_onto_graphics_layer_;

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

    // Now look at the body's layoutObject.
    HTMLElement* body = layout_object.GetDocument().body();
    LayoutObject* body_object =
        IsA<HTMLBodyElement>(body) ? body->GetLayoutObject() : nullptr;
    if (body_object &&
        HasBoxDecorationsOrBackgroundImage(body_object->StyleRef()))
      return true;
  }

  if (owning_layer_.HasVisibleBoxDecorations())
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

// An image can be directly composited if it's the sole content of the layer,
// and has no box decorations or clipping that require painting. Direct
// compositing saves a backing store.
bool CompositedLayerMapping::IsDirectlyCompositedImage() const {
  DCHECK(GetLayoutObject().IsImage());

  if (base::FeatureList::IsEnabled(features::kDisableDirectlyCompositedImages))
    return false;

  LayoutImage& image_layout_object = ToLayoutImage(GetLayoutObject());

  if (owning_layer_.HasBoxDecorationsOrBackground() ||
      image_layout_object.HasClip() || image_layout_object.HasClipPath() ||
      image_layout_object.HasObjectFit())
    return false;

  if (ImageResourceContent* cached_image = image_layout_object.CachedImage()) {
    if (!cached_image->HasImage())
      return false;

    Image* image = cached_image->GetImage();
    if (!image->IsBitmapImage())
      return false;

    UseCounter::Count(GetLayoutObject().GetDocument(),
                      WebFeature::kDirectlyCompositedImage);
    return true;
  }

  return false;
}

void CompositedLayerMapping::ContentChanged(ContentChangeType change_type) {
  if ((change_type == kImageChanged) && GetLayoutObject().IsImage() &&
      IsDirectlyCompositedImage()) {
    SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateLocal);
    Compositor()->SetNeedsCompositingUpdate(
        kCompositingUpdateAfterGeometryChange);
    return;
  }

  if (change_type == kCanvasChanged &&
      IsTextureLayerCanvas(GetLayoutObject())) {
    graphics_layer_->SetContentsNeedsDisplay();
    return;
  }
}

void CompositedLayerMapping::UpdateImageContents() {
  DCHECK_EQ(owning_layer_.Compositor()->Lifecycle().GetState(),
            DocumentLifecycle::kInCompositingUpdate);

  DCHECK(GetLayoutObject().IsImage());
  LayoutImage& image_layout_object = ToLayoutImage(GetLayoutObject());

  ImageResourceContent* cached_image = image_layout_object.CachedImage();
  if (!cached_image)
    return;

  Image* image = cached_image->GetImage();
  if (!image)
    return;

  Node* node = image_layout_object.GetNode();
  Image::ImageDecodingMode decode_mode =
      IsHTMLImageElement(node)
          ? ToHTMLImageElement(node)->GetDecodingModeForPainting(
                image->paint_image_id())
          : Image::kUnspecifiedDecode;

  // This is a no-op if the layer doesn't have an inner layer for the image.
  graphics_layer_->SetContentsToImage(
      image, decode_mode,
      LayoutObject::ShouldRespectImageOrientation(&image_layout_object));

  graphics_layer_->SetFilterQuality(
      GetLayoutObject().StyleRef().ImageRendering() ==
              EImageRendering::kPixelated
          ? kNone_SkFilterQuality
          : kLow_SkFilterQuality);

  // Prevent double-drawing: https://bugs.webkit.org/show_bug.cgi?id=58632
  UpdateDrawsContentAndPaintsHitTest();

  // Image animation is "lazy", in that it automatically stops unless someone is
  // drawing the image. So we have to kick the animation each time; this has the
  // downside that the image will keep animating, even if its layer is not
  // visible.
  image->StartAnimation();
}

FloatPoint3D CompositedLayerMapping::ComputeTransformOrigin(
    const IntRect& border_box) const {
  const ComputedStyle& style = GetLayoutObject().StyleRef();

  FloatPoint3D origin;
  origin.SetX(
      FloatValueForLength(style.TransformOriginX(), border_box.Width()));
  origin.SetY(
      FloatValueForLength(style.TransformOriginY(), border_box.Height()));
  origin.SetZ(style.TransformOriginZ());

  return origin;
}

// Return the offset from the top-left of this compositing layer at which the
// LayoutObject's contents are painted.
PhysicalOffset CompositedLayerMapping::ContentOffsetInCompositingLayer() const {
  return owning_layer_.SubpixelAccumulation() -
         PhysicalOffset(graphics_layer_->OffsetFromLayoutObject());
}

PhysicalRect CompositedLayerMapping::ContentsBox() const {
  PhysicalRect contents_box = ContentsRect(GetLayoutObject());
  contents_box.Move(ContentOffsetInCompositingLayer());
  return contents_box;
}

bool CompositedLayerMapping::NeedsToReparentOverflowControls() const {
  return owning_layer_.NeedsReorderOverlayOverflowControls();
}

GraphicsLayer* CompositedLayerMapping::DetachLayerForOverflowControls() {
  if (overflow_controls_host_layer_)
    overflow_controls_host_layer_->RemoveFromParent();
  return overflow_controls_host_layer_.get();
}

GraphicsLayer* CompositedLayerMapping::DetachLayerForDecorationOutline() {
  if (!decoration_outline_layer_.get())
    return nullptr;
  decoration_outline_layer_->RemoveFromParent();
  return decoration_outline_layer_.get();
}

GraphicsLayer* CompositedLayerMapping::ParentForSublayers() const {
  if (scrolling_contents_layer_)
    return scrolling_contents_layer_.get();

  return graphics_layer_.get();
}

void CompositedLayerMapping::SetSublayers(
    const GraphicsLayerVector& sublayers) {
  GraphicsLayer* overflow_controls_container =
      overflow_controls_host_layer_.get();
  GraphicsLayer* parent = ParentForSublayers();
  bool needs_overflow_controls_reattached =
      overflow_controls_container &&
      overflow_controls_container->Parent() == parent;

  parent->SetChildren(sublayers);

  // If we have scrollbars, but are not using composited scrolling, then
  // parentForSublayers may return m_graphicsLayer.  In that case, the above
  // call to setChildren has clobbered the overflow controls host layer, so we
  // need to reattach it.
  if (needs_overflow_controls_reattached)
    parent->AddChild(overflow_controls_container);
}

GraphicsLayer* CompositedLayerMapping::ChildForSuperlayers() const {
  if (squashing_containment_layer_)
    return squashing_containment_layer_.get();

  return graphics_layer_.get();
}

GraphicsLayerUpdater::UpdateType CompositedLayerMapping::UpdateTypeForChildren(
    GraphicsLayerUpdater::UpdateType update_type) const {
  if (pending_update_scope_ >= kGraphicsLayerUpdateSubtree)
    return GraphicsLayerUpdater::kForceUpdate;
  return update_type;
}

struct SetContentsNeedsDisplayFunctor {
  void operator()(GraphicsLayer* layer) const {
    if (layer->PaintsContentOrHitTest())
      layer->SetNeedsDisplay();
  }
};

void CompositedLayerMapping::SetSquashingContentsNeedDisplay() {
  ApplyToGraphicsLayers(this, SetContentsNeedsDisplayFunctor(),
                        kApplyToSquashingLayer);
}

void CompositedLayerMapping::SetContentsNeedDisplay() {
  // FIXME: need to split out paint invalidations for the background.
  ApplyToGraphicsLayers(this, SetContentsNeedsDisplayFunctor(),
                        kApplyToContentLayers);
}

void CompositedLayerMapping::SetNeedsCheckRasterInvalidation() {
  ApplyToGraphicsLayers(
      this,
      [](GraphicsLayer* graphics_layer) {
        if (graphics_layer->DrawsContent())
          graphics_layer->SetNeedsCheckRasterInvalidation();
      },
      kApplyToAllGraphicsLayers);
}

const GraphicsLayerPaintInfo* CompositedLayerMapping::ContainingSquashedLayer(
    const LayoutObject* layout_object,
    const Vector<GraphicsLayerPaintInfo>& layers,
    unsigned max_squashed_layer_index) {
  if (!layout_object)
    return nullptr;
  for (wtf_size_t i = 0; i < layers.size() && i < max_squashed_layer_index;
       ++i) {
    if (layout_object->IsDescendantOf(
            &layers[i].paint_layer->GetLayoutObject()))
      return &layers[i];
  }
  return nullptr;
}

const GraphicsLayerPaintInfo* CompositedLayerMapping::ContainingSquashedLayer(
    const LayoutObject* layout_object,
    unsigned max_squashed_layer_index) {
  return CompositedLayerMapping::ContainingSquashedLayer(
      layout_object, squashed_layers_, max_squashed_layer_index);
}

void CompositedLayerMapping::LocalClipRectForSquashedLayer(
    const PaintLayer& reference_layer,
    const Vector<GraphicsLayerPaintInfo>& layers,
    GraphicsLayerPaintInfo& paint_info) {
  const LayoutObject* clipping_container =
      paint_info.paint_layer->ClippingContainer();
  if (clipping_container == reference_layer.ClippingContainer()) {
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
  // CompositingLayerAssigner::canSquashIntoCurrentSquashingOwner would have
  // disallowed squashing.
  DCHECK(ancestor_paint_info);

  // FIXME: this is a potential performance issue. We should consider caching
  // these clip rects or otherwise optimizing.
  ClipRectsContext clip_rects_context(
      ancestor_paint_info->paint_layer,
      &ancestor_paint_info->paint_layer->GetLayoutObject().FirstFragment(),
      kUncachedClipRects);
  ClipRect parent_clip_rect;
  paint_info.paint_layer
      ->Clipper(PaintLayer::GeometryMapperOption::kDoNotUseGeometryMapper)
      .CalculateBackgroundClipRect(clip_rects_context, parent_clip_rect);

  // Convert from ancestor to local coordinates.
  IntSize ancestor_to_local_offset =
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
    const IntRect& clip /* In the coords of rootLayer */) const {
  FontCachePurgePreventer font_cache_purge_preventer;

  IntSize offset = paint_info.offset_from_layout_object;
  // The dirtyRect is in the coords of the painting root.
  IntRect dirty_rect(clip);
  dirty_rect.Move(offset);

  if (paint_layer_flags & (kPaintLayerPaintingOverflowContents)) {
    dirty_rect.MoveBy(
        RoundedIntPoint(paint_info.paint_layer->SubpixelAccumulation()));
  } else {
    PhysicalRect bounds = paint_info.composited_bounds;
    bounds.Move(paint_info.paint_layer->SubpixelAccumulation());
    dirty_rect.Intersect(PixelSnappedIntRect(bounds));
  }

#if DCHECK_IS_ON()
  if (!GetLayoutObject().View()->GetFrame() ||
      !GetLayoutObject().View()->GetFrame()->ShouldThrottleRendering())
    paint_info.paint_layer->GetLayoutObject().AssertSubtreeIsLaidOut();
#endif

  float device_scale_factor = blink::DeviceScaleFactorDeprecated(
      paint_info.paint_layer->GetLayoutObject().GetFrame());
  context.SetDeviceScaleFactor(device_scale_factor);

  Settings* settings = GetLayoutObject().GetFrame()->GetSettings();
  context.SetDarkMode(
      BuildDarkModeSettings(*settings, *GetLayoutObject().View()));

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
// Note however that the minimum value for this constant is the size of a
// raster tile. This is because the raster system is not able to raster a
// tile that is not completely covered by a display list. If the constant
// were less than the size of a tile, then a tile which partially overlaps
// the screen may not be rastered.
static const int kPixelDistanceToRecord = 4000;

IntRect CompositedLayerMapping::RecomputeInterestRect(
    const GraphicsLayer* graphics_layer) const {
  IntRect graphics_layer_bounds(IntPoint(), IntSize(graphics_layer->Size()));

  FloatClipRect mapping_rect((FloatRect(graphics_layer_bounds)));

  PropertyTreeState source_state = graphics_layer->GetPropertyTreeState();

  LayoutView* root_view = owning_layer_.GetLayoutObject().View();
  while (root_view->GetFrame()->OwnerLayoutObject())
    root_view = root_view->GetFrame()->OwnerLayoutObject()->View();

  PropertyTreeState root_view_contents_state =
      root_view->FirstFragment().ContentsProperties();
  PropertyTreeState root_view_border_box_state =
      root_view->FirstFragment().LocalBorderBoxProperties();

  // 1. Move into local transform space.
  mapping_rect.MoveBy(FloatPoint(graphics_layer->GetOffsetFromTransformNode()));
  // 2. Map into contents space of the root LayoutView.
  GeometryMapper::LocalToAncestorVisualRect(
      source_state, root_view_contents_state, mapping_rect);

  FloatRect visible_content_rect = mapping_rect.Rect();

  // 3. Move into local border box transform space of the root LayoutView.
  // Note that the overflow clip has *not* been applied.
  GeometryMapper::SourceToDestinationRect(
      root_view_contents_state.Transform(),
      root_view_border_box_state.Transform(), visible_content_rect);

  // 4. Apply overflow clip, or adjusted version if necessary.
  root_view->GetFrameView()->ClipPaintRect(&visible_content_rect);

  FloatRect local_interest_rect;
  // If the visible content rect is empty, then it makes no sense to map it back
  // since there is nothing to map.
  if (!visible_content_rect.IsEmpty()) {
    local_interest_rect = visible_content_rect;
    // 5. Map the visible content rect from root view space to local graphics
    // layer space.
    GeometryMapper::SourceToDestinationRect(
        root_view_border_box_state.Transform(), source_state.Transform(),
        local_interest_rect);
    local_interest_rect.MoveBy(
        -FloatPoint(graphics_layer->GetOffsetFromTransformNode()));

    // TODO(chrishtr): the code below is a heuristic. Instead we should detect
    // and return whether the mapping failed.  In some cases,
    // absoluteToLocalQuad can fail to map back to the local space, due to
    // passing through non-invertible transforms or floating-point accuracy
    // issues. Examples include rotation near 90 degrees or perspective. In such
    // cases, fall back to painting the first kPixelDistanceToRecord pixels in
    // each direction.

    // Note that since the interest rect mapping above can produce extremely
    // large numbers in cases of perspective, try our best to "normalize" the
    // result by ensuring that none of the rect dimensions exceed some large,
    // but reasonable, limit.
    const float reasonable_pixel_limit = std::numeric_limits<int>::max() / 2.f;
    auto unpadded_intersection = local_interest_rect;

    // Note that by clamping X and Y, we are effectively moving the rect right /
    // down. However, this will at most make us paint more content, which is
    // better than erroneously deciding that the rect produced here is far
    // offscreen.
    if (unpadded_intersection.X() < -reasonable_pixel_limit)
      unpadded_intersection.SetX(-reasonable_pixel_limit);
    if (unpadded_intersection.Y() < -reasonable_pixel_limit)
      unpadded_intersection.SetY(-reasonable_pixel_limit);
    if (unpadded_intersection.MaxX() > reasonable_pixel_limit) {
      unpadded_intersection.SetWidth(reasonable_pixel_limit -
                                     unpadded_intersection.X());
    }
    if (unpadded_intersection.MaxY() > reasonable_pixel_limit) {
      unpadded_intersection.SetHeight(reasonable_pixel_limit -
                                      unpadded_intersection.Y());
    }

    unpadded_intersection.Intersect(FloatRect(graphics_layer_bounds));
    // If our unpadded intersection is not empty, then use that before padding,
    // since it can produce more stable results, and it would not produce any
    // smaller area than if we used the original local interest rect.
    if (!unpadded_intersection.IsEmpty())
      local_interest_rect = unpadded_intersection;

    // Expand by interest rect padding amount, scaled by the approximate scale
    // of the GraphicsLayer relative to screen pixels. If width or height
    // are zero or nearly zero, fall back to kPixelDistanceToRecord.
    // This is the same as the else clause below.
    float x_scale =
        visible_content_rect.Width() > std::numeric_limits<float>::epsilon()
            ? local_interest_rect.Width() / visible_content_rect.Width()
            : 1.0f;
    float y_scale =
        visible_content_rect.Height() > std::numeric_limits<float>::epsilon()
            ? local_interest_rect.Height() / visible_content_rect.Height()
            : 1.0f;
    // Take the max, to account for situations like rotation transforms, which
    // swap x and y.
    // Since at this point we can also have an extremely large scale due to
    // perspective (see the comments above), cap it to something reasonable.
    float scale = std::min(std::max(x_scale, y_scale),
                           reasonable_pixel_limit / kPixelDistanceToRecord);
    local_interest_rect.Inflate(kPixelDistanceToRecord * scale);
  } else {
    // Expand by interest rect padding amount.
    local_interest_rect.Inflate(kPixelDistanceToRecord);
  }
  return Intersection(EnclosingIntRect(local_interest_rect),
                      graphics_layer_bounds);
}

static const int kMinimumDistanceBeforeRepaint = 512;

bool CompositedLayerMapping::InterestRectChangedEnoughToRepaint(
    const IntRect& previous_interest_rect,
    const IntRect& new_interest_rect,
    const IntSize& layer_size) {
  if (previous_interest_rect.IsEmpty() && new_interest_rect.IsEmpty())
    return false;

  // Repaint when going from empty to not-empty, to cover cases where the layer
  // is painted for the first time, or otherwise becomes visible.
  if (previous_interest_rect.IsEmpty())
    return true;

  // Repaint if the new interest rect includes area outside of a skirt around
  // the existing interest rect.
  IntRect expanded_previous_interest_rect(previous_interest_rect);
  expanded_previous_interest_rect.Inflate(kMinimumDistanceBeforeRepaint);
  if (!expanded_previous_interest_rect.Contains(new_interest_rect))
    return true;

  // Even if the new interest rect doesn't include enough new area to satisfy
  // the condition above, repaint anyway if it touches a layer edge not touched
  // by the existing interest rect.  Because it's impossible to expose more area
  // in the direction, repainting cannot be deferred until the exposed new area
  // satisfies the condition above.
  if (new_interest_rect.X() == 0 && previous_interest_rect.X() != 0)
    return true;
  if (new_interest_rect.Y() == 0 && previous_interest_rect.Y() != 0)
    return true;
  if (new_interest_rect.MaxX() == layer_size.Width() &&
      previous_interest_rect.MaxX() != layer_size.Width())
    return true;
  if (new_interest_rect.MaxY() == layer_size.Height() &&
      previous_interest_rect.MaxY() != layer_size.Height())
    return true;

  return false;
}

IntRect CompositedLayerMapping::ComputeInterestRect(
    const GraphicsLayer* graphics_layer,
    const IntRect& previous_interest_rect) const {
  // Use the previous interest rect if it covers the whole layer.
  IntRect whole_layer_rect =
      IntRect(IntPoint(), IntSize(graphics_layer->Size()));
  if (!NeedsRepaint(*graphics_layer) &&
      previous_interest_rect == whole_layer_rect)
    return previous_interest_rect;

  if (graphics_layer != graphics_layer_.get() &&
      graphics_layer != squashing_layer_.get() &&
      graphics_layer != scrolling_contents_layer_.get())
    return whole_layer_rect;

  IntRect new_interest_rect = RecomputeInterestRect(graphics_layer);
  if (NeedsRepaint(*graphics_layer) ||
      InterestRectChangedEnoughToRepaint(previous_interest_rect,
                                         new_interest_rect,
                                         IntSize(graphics_layer->Size())))
    return new_interest_rect;
  return previous_interest_rect;
}

LayoutSize CompositedLayerMapping::SubpixelAccumulation() const {
  return owning_layer_.SubpixelAccumulation().ToLayoutSize();
}

bool CompositedLayerMapping::NeedsRepaint(
    const GraphicsLayer& graphics_layer) const {
  return IsScrollableAreaLayer(&graphics_layer)
             ? true
             : owning_layer_.SelfOrDescendantNeedsRepaint();
}

bool CompositedLayerMapping::AdjustForCompositedScrolling(
    const GraphicsLayer* graphics_layer,
    IntSize& offset) const {
  if (graphics_layer == scrolling_contents_layer_.get() ||
      graphics_layer == foreground_layer_.get()) {
    if (PaintLayerScrollableArea* scrollable_area =
            owning_layer_.GetScrollableArea()) {
      if (scrollable_area->UsesCompositedScrolling()) {
        // Note: this is the offset from the beginning of flow of the block, not
        // the offset from the top/left of the overflow rect.
        // offsetFromLayoutObject adds the origin offset from top/left to the
        // beginning of flow.
        ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
        offset.Expand(-scroll_offset.Width(), -scroll_offset.Height());
        return true;
      }
    }
  }
  return false;
}

void CompositedLayerMapping::PaintContents(
    const GraphicsLayer* graphics_layer,
    GraphicsContext& context,
    GraphicsLayerPaintingPhase graphics_layer_painting_phase,
    const IntRect& interest_rect) const {
  FramePaintTiming frame_paint_timing(context, GetLayoutObject().GetFrame());

  // https://code.google.com/p/chromium/issues/detail?id=343772
  DisableCompositingQueryAsserts disabler;
  // Allow throttling to make sure no painting paths (e.g.,
  // GraphicsLayer::PaintContents) try to paint throttled content.
  DocumentLifecycle::AllowThrottlingScope allow_throttling(
      owning_layer_.GetLayoutObject().GetDocument().Lifecycle());
#if DCHECK_IS_ON()
  // FIXME: once the state machine is ready, this can be removed and we can
  // refer to that instead.
  if (Page* page = GetLayoutObject().GetFrame()->GetPage())
    page->SetIsPainting(true);
#endif

  TRACE_EVENT1(
      "devtools.timeline,rail", "Paint", "data",
      inspector_paint_event::Data(&owning_layer_.GetLayoutObject(),
                                  PhysicalRect(interest_rect), graphics_layer));

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

  if (graphics_layer == graphics_layer_.get() ||
      graphics_layer == foreground_layer_.get() ||
      graphics_layer == mask_layer_.get() ||
      graphics_layer == scrolling_contents_layer_.get() ||
      graphics_layer == decoration_outline_layer_.get()) {
    if (background_paints_onto_scrolling_contents_layer_) {
      if (graphics_layer == scrolling_contents_layer_.get())
        paint_layer_flags &= ~kPaintLayerPaintingSkipRootBackground;
      else if (!background_paints_onto_graphics_layer_)
        paint_layer_flags |= kPaintLayerPaintingSkipRootBackground;
    }

    GraphicsLayerPaintInfo paint_info;
    paint_info.paint_layer = &owning_layer_;
    paint_info.composited_bounds = CompositedBounds();
    paint_info.offset_from_layout_object =
        graphics_layer->OffsetFromLayoutObject();
    AdjustForCompositedScrolling(graphics_layer,
                                 paint_info.offset_from_layout_object);

    // We have to use the same root as for hit testing, because both methods can
    // compute and cache clipRects.
    DoPaintTask(paint_info, *graphics_layer, paint_layer_flags, context,
                interest_rect);
  } else if (graphics_layer == squashing_layer_.get()) {
    for (wtf_size_t i = 0; i < squashed_layers_.size(); ++i) {
      DoPaintTask(squashed_layers_[i], *graphics_layer, paint_layer_flags,
                  context, interest_rect);
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
    const IntRect& interest_rect) const {
  // cull_rect is in the space of the containing scrollable area in which
  // Scrollbar::Paint() will paint the scrollbar.
  CullRect cull_rect(interest_rect);
  cull_rect.Move(graphics_layer->OffsetFromLayoutObject());
  PaintLayerScrollableArea* scrollable_area = owning_layer_.GetScrollableArea();
  if (graphics_layer == LayerForHorizontalScrollbar()) {
    if (const Scrollbar* scrollbar = scrollable_area->HorizontalScrollbar()) {
      if (cull_rect.Intersects(scrollbar->FrameRect()))
        scrollbar->Paint(context);
    }
  } else if (graphics_layer == LayerForVerticalScrollbar()) {
    if (const Scrollbar* scrollbar = scrollable_area->VerticalScrollbar()) {
      if (cull_rect.Intersects(scrollbar->FrameRect()))
        scrollbar->Paint(context);
    }
  } else if (graphics_layer == LayerForScrollCorner()) {
    ScrollableAreaPainter painter(*scrollable_area);
    painter.PaintScrollCorner(context, IntPoint(), cull_rect);
    painter.PaintResizer(context, IntPoint(), cull_rect);
  }
}

bool CompositedLayerMapping::IsScrollableAreaLayer(
    const GraphicsLayer* graphics_layer) const {
  return graphics_layer == LayerForHorizontalScrollbar() ||
         graphics_layer == LayerForVerticalScrollbar() ||
         graphics_layer == LayerForScrollCorner();
}

bool CompositedLayerMapping::ShouldThrottleRendering() const {
  return GetLayoutObject().GetFrame()->ShouldThrottleRendering();
}

bool CompositedLayerMapping::IsUnderSVGHiddenContainer() const {
  return owning_layer_.IsUnderSVGHiddenContainer();
}

bool CompositedLayerMapping::IsTrackingRasterInvalidations() const {
  return GetLayoutObject().GetFrameView()->IsTrackingPaintInvalidations();
}

void CompositedLayerMapping::GraphicsLayersDidChange() {
  LocalFrameView* frame_view = GetLayoutObject().GetFrameView();
  DCHECK(frame_view);
  frame_view->SetForeignLayerListNeedsUpdate();
}

bool CompositedLayerMapping::PaintBlockedByDisplayLockIncludingAncestors(
    DisplayLockContextLifecycleTarget target) const {
  auto* node = GetLayoutObject().GetNode();
  if (node) {
    auto* element = DynamicTo<Element>(node);
    if (target == DisplayLockContextLifecycleTarget::kSelf && element) {
      if (auto* context = element->GetDisplayLockContext()) {
        if (!context->ShouldPaint(DisplayLockLifecycleTarget::kSelf))
          return true;
      }
    }
  }
  return DisplayLockUtilities::NearestLockedExclusiveAncestor(
      GetLayoutObject());
}

void CompositedLayerMapping::NotifyDisplayLockNeedsGraphicsLayerCollection() {
  if (auto* locked_element =
          DisplayLockUtilities::NearestLockedInclusiveAncestor(
              GetLayoutObject())) {
    locked_element->GetDisplayLockContext()
        ->NotifyNeedsGraphicsLayerCollection();
  }
}

#if DCHECK_IS_ON()
void CompositedLayerMapping::VerifyNotPainting() {
  DCHECK(!GetLayoutObject().GetFrame()->GetPage() ||
         !GetLayoutObject().GetFrame()->GetPage()->IsPainting());
}
#endif

// Only used for performance benchmark testing. Intended to be a
// sufficiently-unique element id name to allow picking out the target element
// for invalidation.
static const char kTestPaintInvalidationTargetName[] =
    "blinkPaintInvalidationTarget";

void CompositedLayerMapping::InvalidateTargetElementForTesting() {
  // The below is an artificial construct formed intentionally to focus a
  // microbenchmark on the cost of paint with a partial invalidation.
  Element* target_element =
      owning_layer_.GetLayoutObject().GetDocument().getElementById(
          AtomicString(kTestPaintInvalidationTargetName));
  // TODO(wkorman): If we don't find the expected target element, we could
  // consider walking to the first leaf node so that the partial-invalidation
  // benchmark mode still provides some value when running on generic pages.
  if (!target_element)
    return;
  LayoutObject* target_object = target_element->GetLayoutObject();
  if (!target_object)
    return;
  target_object->EnclosingLayer()->SetNeedsRepaint();
  // TODO(wkorman): Consider revising the below to invalidate all
  // non-compositing descendants as well.
  target_object->InvalidateDisplayItemClients(
      PaintInvalidationReason::kForTesting);
}

bool CompositedLayerMapping::InvalidateLayerIfNoPrecedingEntry(
    wtf_size_t index_to_clear) {
  PaintLayer* layer_to_remove = squashed_layers_[index_to_clear].paint_layer;
  wtf_size_t previous_index = 0;
  for (; previous_index < index_to_clear; ++previous_index) {
    if (squashed_layers_[previous_index].paint_layer == layer_to_remove)
      break;
  }
  if (previous_index == index_to_clear &&
      layer_to_remove->GroupedMapping() == this) {
    Compositor()->PaintInvalidationOnCompositingChange(layer_to_remove);
    return true;
  }
  return false;
}

bool CompositedLayerMapping::UpdateSquashingLayerAssignment(
    PaintLayer* squashed_layer,
    wtf_size_t next_squashed_layer_index) {
  GraphicsLayerPaintInfo paint_info;
  paint_info.paint_layer = squashed_layer;
  // NOTE: composited bounds are updated elsewhere
  // NOTE: offsetFromLayoutObject is updated elsewhere

  // Change tracking on squashing layers: at the first sign of something
  // changed, just invalidate the layer.
  // FIXME: Perhaps we can find a tighter more clever mechanism later.
  if (next_squashed_layer_index < squashed_layers_.size()) {
    if (paint_info.paint_layer ==
        squashed_layers_[next_squashed_layer_index].paint_layer)
      return false;

    // Must invalidate before adding the squashed layer to the mapping.
    Compositor()->PaintInvalidationOnCompositingChange(squashed_layer);

    // If the layer which was previously at |nextSquashedLayerIndex| is not
    // earlier in the grouped mapping, invalidate its current backing now, since
    // it will move later or be removed from the squashing layer.
    InvalidateLayerIfNoPrecedingEntry(next_squashed_layer_index);

    squashed_layers_.insert(next_squashed_layer_index, paint_info);
  } else {
    // Must invalidate before adding the squashed layer to the mapping.
    Compositor()->PaintInvalidationOnCompositingChange(squashed_layer);
    squashed_layers_.push_back(paint_info);
  }
  squashed_layer->SetGroupedMapping(
      this, PaintLayer::kInvalidateLayerAndRemoveFromMapping);

  return true;
}

void CompositedLayerMapping::RemoveLayerFromSquashingGraphicsLayer(
    const PaintLayer* layer) {
  wtf_size_t layer_index = 0;
  for (; layer_index < squashed_layers_.size(); ++layer_index) {
    if (squashed_layers_[layer_index].paint_layer == layer)
      break;
  }

  // Assert on incorrect mappings between layers and groups
  DCHECK_LT(layer_index, squashed_layers_.size());
  if (layer_index == squashed_layers_.size())
    return;

  squashed_layers_.EraseAt(layer_index);
}

#if DCHECK_IS_ON()
bool CompositedLayerMapping::VerifyLayerInSquashingVector(
    const PaintLayer* layer) {
  for (wtf_size_t layer_index = 0; layer_index < squashed_layers_.size();
       ++layer_index) {
    if (squashed_layers_[layer_index].paint_layer == layer)
      return true;
  }

  return false;
}
#endif

void CompositedLayerMapping::FinishAccumulatingSquashingLayers(
    wtf_size_t next_squashed_layer_index,
    Vector<PaintLayer*>& layers_needing_paint_invalidation) {
  if (next_squashed_layer_index < squashed_layers_.size()) {
    // Any additional squashed Layers in the array no longer belong here, but
    // they might have been added already at an earlier index. Clear pointers on
    // those that do not appear in the valid set before removing all the extra
    // entries.
    for (wtf_size_t i = next_squashed_layer_index; i < squashed_layers_.size();
         ++i) {
      if (InvalidateLayerIfNoPrecedingEntry(i)) {
        squashed_layers_[i].paint_layer->SetGroupedMapping(
            nullptr, PaintLayer::kDoNotInvalidateLayerAndRemoveFromMapping);
        squashed_layers_[i].paint_layer->SetLostGroupedMapping(true);
      }
      layers_needing_paint_invalidation.push_back(
          squashed_layers_[i].paint_layer);
    }

    squashed_layers_.EraseAt(
        next_squashed_layer_index,
        squashed_layers_.size() - next_squashed_layer_index);
  }
}

String CompositedLayerMapping::DebugName(
    const GraphicsLayer* graphics_layer) const {
  String name;
  if (graphics_layer == graphics_layer_.get()) {
    name = owning_layer_.DebugName();
  } else if (graphics_layer == squashing_containment_layer_.get()) {
    name = "Squashing Containment Layer";
  } else if (graphics_layer == squashing_layer_.get()) {
    name = "Squashing Layer (first squashed layer: " +
           (squashed_layers_.size() > 0
                ? squashed_layers_[0].paint_layer->DebugName()
                : "") +
           ")";
  } else if (graphics_layer == foreground_layer_.get()) {
    name = owning_layer_.DebugName() + " (foreground) Layer";
  } else if (graphics_layer == mask_layer_.get()) {
    name = "Mask Layer";
  } else if (graphics_layer == layer_for_horizontal_scrollbar_.get()) {
    name = "Horizontal Scrollbar Layer";
  } else if (graphics_layer == layer_for_vertical_scrollbar_.get()) {
    name = "Vertical Scrollbar Layer";
  } else if (graphics_layer == layer_for_scroll_corner_.get()) {
    name = "Scroll Corner Layer";
  } else if (graphics_layer == overflow_controls_host_layer_.get()) {
    name = "Overflow Controls Host Layer";
  } else if (graphics_layer == scrolling_layer_.get()) {
    name = "Scrolling Layer";
  } else if (graphics_layer == scrolling_contents_layer_.get()) {
    name = "Scrolling Contents Layer";
  } else if (graphics_layer == decoration_outline_layer_.get()) {
    name = "Decoration Layer";
  } else {
    NOTREACHED();
  }

  return name;
}

const ScrollableArea* CompositedLayerMapping::GetScrollableAreaForTesting(
    const GraphicsLayer* layer) const {
  if (layer == scrolling_contents_layer_.get())
    return owning_layer_.GetScrollableArea();
  return nullptr;
}

}  // namespace blink
