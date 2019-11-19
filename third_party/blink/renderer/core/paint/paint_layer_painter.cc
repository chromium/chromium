// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void PaintLayerPainter::Paint(GraphicsContext& context,
                              const CullRect& cull_rect,
                              const GlobalPaintFlags global_paint_flags,
                              PaintLayerFlags paint_flags) {
  PaintLayerPaintingInfo painting_info(&paint_layer_, cull_rect,
                                       global_paint_flags, PhysicalOffset());
  if (!paint_layer_.PaintsIntoOwnOrGroupedBacking(global_paint_flags))
    Paint(context, painting_info, paint_flags);
}

static ShouldRespectOverflowClipType ShouldRespectOverflowClip(
    PaintLayerFlags paint_flags,
    const LayoutObject& layout_object) {
  return (paint_flags & kPaintLayerPaintingOverflowContents)
             ? kIgnoreOverflowClip
             : kRespectOverflowClip;
}

bool PaintLayerPainter::PaintedOutputInvisible(const ComputedStyle& style) {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  if (style.HasBackdropFilter())
    return false;

  // Always paint when 'will-change: opacity' is present. Reduces jank for
  // common animation implementation approaches, for example, an element that
  // starts with opacity zero and later begins to animate.
  if (style.HasWillChangeOpacityHint())
    return false;

  // 0.0004f < 1/2048. With 10-bit color channels (only available on the
  // newest Macs; otherwise it's 8-bit), we see that an alpha of 1/2048 or
  // less leads to a color output of less than 0.5 in all channels, hence
  // not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (style.Opacity() < kMinimumVisibleOpacity)
    return true;

  return false;
}

PaintResult PaintLayerPainter::Paint(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  if (paint_layer_.GetLayoutObject().GetFrameView()->ShouldThrottleRendering())
    return kFullyPainted;

  // https://code.google.com/p/chromium/issues/detail?id=343772
  DisableCompositingQueryAsserts disabler;

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant())
    return kFullyPainted;

  // If this layer is totally invisible then there is nothing to paint. In CAP
  // we simplify this optimization by painting even when effectively invisible
  // but skipping the painted content during layerization in
  // PaintArtifactCompositor.
  if (paint_layer_.PaintsWithTransparency(
          painting_info.GetGlobalPaintFlags())) {
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        PaintedOutputInvisible(paint_layer_.GetLayoutObject().StyleRef()))
      return kFullyPainted;

    paint_flags |= kPaintLayerHaveTransparency;
  }

  // If the transform can't be inverted, then don't paint anything.
  if (paint_layer_.PaintsWithTransform(painting_info.GetGlobalPaintFlags()) &&
      !paint_layer_.RenderableTransform(painting_info.GetGlobalPaintFlags())
           .IsInvertible()) {
    return kFullyPainted;
  }

  paint_flags |= kPaintLayerPaintingCompositingAllPhases;
  return PaintLayerContents(context, painting_info, paint_flags);
}

static bool ShouldCreateSubsequence(const PaintLayer& paint_layer,
                                    const GraphicsContext& context,
                                    const PaintLayerPaintingInfo& painting_info,
                                    PaintLayerFlags paint_flags) {
  // Caching is not needed during printing or painting previews.
  if (context.Printing() || context.IsPaintingPreview())
    return false;

  if (context.GetPaintController().IsSkippingCache())
    return false;

  if (!paint_layer.SupportsSubsequenceCaching())
    return false;

  // Don't create subsequence for a composited layer because if it can be
  // cached, we can skip the whole painting in GraphicsLayer::paint() with
  // CachedDisplayItemList.  This also avoids conflict of
  // PaintLayer::previousXXX() when paintLayer is composited scrolling and is
  // painted twice for GraphicsLayers of container and scrolling contents.
  if (paint_layer.GetCompositingState() == kPaintsIntoOwnBacking)
    return false;

  // Don't create subsequence during special painting to avoid cache conflict
  // with normal painting.
  if (painting_info.GetGlobalPaintFlags() &
      kGlobalPaintFlattenCompositingLayers)
    return false;

  if (paint_flags & kPaintLayerPaintingOverlayOverflowControls)
    return false;

  return true;
}

static bool ShouldRepaintSubsequence(
    PaintLayer& paint_layer,
    const PaintLayerPaintingInfo& painting_info) {
  // Repaint subsequence if the layer is marked for needing repaint.
  if (paint_layer.SelfOrDescendantNeedsRepaint())
    return true;

  // Repaint if previously the layer may be clipped by cull rect, and cull rect
  // changes.
  if ((paint_layer.PreviousPaintResult() == kMayBeClippedByCullRect ||
       // When PaintUnderInvalidationChecking is enabled, always repaint the
       // subsequence when the paint rect changes because we will strictly match
       // new and cached subsequences. Normally we can reuse the cached fully
       // painted subsequence even if we would partially paint this time.
       RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) &&
      paint_layer.PreviousCullRect() != painting_info.cull_rect)
    return true;

  return false;
}

static bool ShouldUseInfiniteCullRect(const GraphicsContext& context,
                                      const PaintLayer& layer,
                                      PaintLayerPaintingInfo& painting_info) {
  // Cull rects and clips can't be propagated across a filter which moves
  // pixels, since the input of the filter may be outside the cull rect /
  // clips yet still result in painted output.
  if (layer.HasFilterThatMovesPixels())
    return true;

  // Cull rect mapping doesn't work under perspective in some cases.
  // See http://crbug.com/887558 for details.
  if (painting_info.root_layer->GetLayoutObject().StyleRef().HasPerspective())
    return true;

  // We do not apply cull rect optimizations across transforms for two
  // reasons:
  //   1) Performance: We can optimize transform changes by not repainting.
  //   2) Complexity: Difficulty updating clips when ancestor transforms
  //      change.
  // For these reasons, we use an infinite dirty rect here.
  if (layer.PaintsWithTransform(painting_info.GetGlobalPaintFlags()) &&
      // The reasons don't apply for printing though, because when we enter and
      // leaving printing mode, full invalidations occur.
      !context.Printing())
    return true;

  return false;
}

static bool IsMainFrameNotClippingContents(const PaintLayer& layer) {
  // If MainFrameClipsContent is false which means that WebPreferences::
  // record_whole_document is true, we should not cull the scrolling contents
  // of the main frame.
  if (layer.GetLayoutObject().IsLayoutView()) {
    const auto* frame = layer.GetLayoutObject().GetFrame();
    if (frame && frame->IsMainFrame() && !frame->ClipsContent())
      return true;
  }
  return false;
}

void PaintLayerPainter::AdjustForPaintProperties(
    const GraphicsContext& context,
    PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags& paint_flags) {
  const auto& first_fragment = paint_layer_.GetLayoutObject().FirstFragment();

  bool is_main_frame_not_clipping_contents =
      IsMainFrameNotClippingContents(paint_layer_);
  bool should_use_infinite_cull_rect =
      is_main_frame_not_clipping_contents ||
      ShouldUseInfiniteCullRect(context, paint_layer_, painting_info);
  if (should_use_infinite_cull_rect) {
    painting_info.cull_rect = CullRect::Infinite();
    // Avoid clipping during CollectFragments.
    if (is_main_frame_not_clipping_contents)
      paint_flags |= kPaintLayerPaintingOverflowContents;
  }

  if (painting_info.root_layer == &paint_layer_)
    return;

  if (!should_use_infinite_cull_rect) {
    // painting_info.cull_rect is currently in |painting_info.root_layer|'s
    // pixel-snapped border box space. We need to adjust it into
    // |paint_layer_|'s space. This handles the following cases:
    // - The current layer has PaintOffsetTranslation;
    // - The current layer's transform state escapes the root layers contents
    //   transform, e.g. a fixed-position layer;
    // - Scroll offsets.
    const auto& first_root_fragment =
        painting_info.root_layer->GetLayoutObject().FirstFragment();
    const auto* source_transform =
        &first_root_fragment.LocalBorderBoxProperties().Transform();
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        IsMainFrameNotClippingContents(*painting_info.root_layer)) {
      // Use PostScrollTranslation as the source transform to avoid clipping of
      // the scrolling contents in CullRect::ApplyTransforms().
      source_transform = &first_root_fragment.PostScrollTranslation();
    }
    const auto& destination_transform =
        first_fragment.LocalBorderBoxProperties().Transform();
    if (source_transform == &destination_transform)
      return;

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      auto& cull_rect = painting_info.cull_rect;
      // CullRect::ApplyTransforms() requires the cull rect in the source
      // transform space. Convert cull_rect from the root layer's local space.
      cull_rect.MoveBy(RoundedIntPoint(first_root_fragment.PaintOffset()));
      base::Optional<CullRect> old_cull_rect;
      if (!paint_layer_.SelfOrDescendantNeedsRepaint()) {
        old_cull_rect = paint_layer_.PreviousCullRect();
        // Convert old_cull_rect into the layer's transform space.
        old_cull_rect->MoveBy(RoundedIntPoint(first_fragment.PaintOffset()));
      }

      // Don't clip to the view scrolling container when printing because we
      // need to print the whole document.
      bool clip_to_scroll_container =
          !(context.Printing() &&
            (paint_flags & kPaintLayerPaintingOverflowContents));
      cull_rect.ApplyTransforms(*source_transform, destination_transform,
                                old_cull_rect, clip_to_scroll_container);
      // Convert cull_rect from the layer's transform space to the layer's local
      // space.
      cull_rect.MoveBy(-RoundedIntPoint(first_fragment.PaintOffset()));
    } else if (!painting_info.cull_rect.IsInfinite()) {
      auto rect = painting_info.cull_rect.Rect();
      first_root_fragment.MapRectToFragment(first_fragment, rect);
      painting_info.cull_rect = CullRect(rect);
    }
  }

  // We reach here if the layer requires infinite cull rect or has different
  // transform space from the current root layer. Use the current layer as
  // the new root layer.
  painting_info.root_layer = &paint_layer_;
  // These flags no longer apply for the new root layer.
  paint_flags &= ~kPaintLayerPaintingSkipRootBackground;
  paint_flags &= ~kPaintLayerPaintingOverflowContents;
  paint_flags &= ~kPaintLayerPaintingCompositingScrollingPhase;

  if (first_fragment.PaintProperties() &&
      first_fragment.PaintProperties()->PaintOffsetTranslation()) {
    painting_info.sub_pixel_accumulation = first_fragment.PaintOffset();
  }
}

PaintResult PaintLayerPainter::PaintLayerContents(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info_arg,
    PaintLayerFlags paint_flags_arg) {
  DCHECK(paint_layer_.IsSelfPaintingLayer() ||
         paint_layer_.HasSelfPaintingLayerDescendant());

  PaintResult result = kFullyPainted;
  if (paint_layer_.GetLayoutObject().GetFrameView()->ShouldThrottleRendering())
    return result;

  // If we're blocked from painting by the display lock, return early.
  if (paint_layer_.GetLayoutObject().PaintBlockedByDisplayLock(
          DisplayLockLifecycleTarget::kSelf)) {
    return result;
  }

  // TODO(vmpstr): This should be called after paint succeeds, but due to
  // multiple early outs this is more convenient. We should use RAII here.
  paint_layer_.GetLayoutObject().NotifyDisplayLockDidPaint(
      DisplayLockLifecycleTarget::kSelf);

  // A paint layer should always have LocalBorderBoxProperties when it's ready
  // for paint.
  if (!paint_layer_.GetLayoutObject()
           .FirstFragment()
           .HasLocalBorderBoxProperties()) {
    // TODO(crbug.com/848056): This can happen e.g. when we paint a filter
    // referencing a SVG foreign object through feImage, especially when there
    // is circular references. Should find a better solution.
    paint_layer_.SetPreviousCullRect(CullRect());
    return kMayBeClippedByCullRect;
  }

  PaintLayerFlags paint_flags = paint_flags_arg;
  PaintLayerPaintingInfo painting_info = painting_info_arg;
  AdjustForPaintProperties(context, painting_info, paint_flags);

  bool is_self_painting_layer = paint_layer_.IsSelfPaintingLayer();
  bool is_painting_overlay_overflow_controls =
      paint_flags & kPaintLayerPaintingOverlayOverflowControls;
  bool is_painting_scrolling_content =
      paint_flags & kPaintLayerPaintingCompositingScrollingPhase;
  bool is_painting_composited_foreground =
      paint_flags & kPaintLayerPaintingCompositingForegroundPhase;
  bool is_painting_composited_background =
      paint_flags & kPaintLayerPaintingCompositingBackgroundPhase;
  bool is_painting_composited_decoration =
      paint_flags & kPaintLayerPaintingCompositingDecorationPhase;
  bool is_painting_overflow_contents =
      paint_flags & kPaintLayerPaintingOverflowContents;
  bool is_painting_mask = paint_flags & kPaintLayerPaintingCompositingMaskPhase;

  // Outline always needs to be painted even if we have no visible content.
  // It is painted as part of the decoration phase which paints content that
  // is not scrolled and should be above scrolled content.
  bool should_paint_self_outline =
      is_self_painting_layer && !is_painting_overlay_overflow_controls &&
      (is_painting_composited_decoration ||
       (!is_painting_overflow_contents && !is_painting_mask)) &&
      paint_layer_.GetLayoutObject().StyleRef().HasOutline();

  PhysicalOffset subpixel_accumulation =
      paint_layer_.GetCompositingState() == kPaintsIntoOwnBacking
          ? paint_layer_.SubpixelAccumulation()
          : painting_info.sub_pixel_accumulation;

  ShouldRespectOverflowClipType respect_overflow_clip =
      ShouldRespectOverflowClip(paint_flags, paint_layer_.GetLayoutObject());

  bool should_create_subsequence = ShouldCreateSubsequence(
      paint_layer_, context, painting_info, paint_flags);

  base::Optional<SubsequenceRecorder> subsequence_recorder;
  if (should_create_subsequence) {
    if (!ShouldRepaintSubsequence(paint_layer_, painting_info) &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_)) {
      return paint_layer_.PreviousPaintResult();
    }
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  }

  PhysicalOffset offset_from_root;
  paint_layer_.ConvertToLayerCoords(painting_info.root_layer, offset_from_root);
  offset_from_root += subpixel_accumulation;

  PhysicalRect bounds = paint_layer_.PhysicalBoundingBox(offset_from_root);
  if (!PhysicalRect(painting_info.cull_rect.Rect()).Contains(bounds))
    result = kMayBeClippedByCullRect;

  // These helpers output clip and compositing operations using a RAII pattern.
  // Stack-allocated-varibles are destructed in the reverse order of
  // construction, so they are nested properly.
  base::Optional<ClipPathClipper> clip_path_clipper;
  bool should_paint_clip_path =
      is_painting_mask && paint_layer_.GetLayoutObject().HasClipPath();
  if (should_paint_clip_path) {
    PhysicalOffset visual_offset_from_root =
        paint_layer_.EnclosingPaginationLayer()
            ? paint_layer_.VisualOffsetFromAncestor(painting_info.root_layer,
                                                    subpixel_accumulation)
            : offset_from_root;
    clip_path_clipper.emplace(context, paint_layer_.GetLayoutObject(),
                              visual_offset_from_root);
  }

  PaintLayerPaintingInfo local_painting_info(painting_info);
  local_painting_info.sub_pixel_accumulation = subpixel_accumulation;

  bool should_paint_content =
      paint_layer_.HasVisibleContent() &&
      // Content under a LayoutSVGHiddenContainer is auxiliary resources for
      // painting. Foreign content should never paint in this situation, as it
      // is primary, not auxiliary.
      !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer &&
      !is_painting_overlay_overflow_controls;

  PaintLayerFragments layer_fragments;

  if (should_paint_content || should_paint_self_outline ||
      is_painting_overlay_overflow_controls) {
    // Collect the fragments. This will compute the clip rectangles and paint
    // offsets for each layer fragment.
    paint_layer_.CollectFragments(
        layer_fragments, local_painting_info.root_layer,
        &local_painting_info.cull_rect, kIgnorePlatformOverlayScrollbarSize,
        respect_overflow_clip, &offset_from_root,
        local_painting_info.sub_pixel_accumulation);

    // PaintLayer::CollectFragments depends on the paint dirty rect in
    // complicated ways. For now, always assume a partially painted output
    // for fragmented content.
    if (layer_fragments.size() > 1)
      result = kMayBeClippedByCullRect;

    if (should_paint_content) {
      should_paint_content = AtLeastOneFragmentIntersectsDamageRect(
          layer_fragments, local_painting_info, paint_flags, offset_from_root);
      if (!should_paint_content)
        result = kMayBeClippedByCullRect;
    }
  }

  bool selection_only =
      local_painting_info.GetGlobalPaintFlags() & kGlobalPaintSelectionOnly;

  {  // Begin block for the lifetime of any filter.
    size_t display_item_list_size_before_painting =
        context.GetPaintController().NewDisplayItemList().size();

    bool is_painting_root_layer = (&paint_layer_) == painting_info.root_layer;
    bool should_paint_background =
        should_paint_content && !selection_only &&
        (is_painting_composited_background ||
         (is_painting_root_layer &&
          !(paint_flags & kPaintLayerPaintingSkipRootBackground)));
    bool should_paint_neg_z_order_list =
        !is_painting_overlay_overflow_controls &&
        (is_painting_scrolling_content ? is_painting_overflow_contents
                                       : is_painting_composited_background);
    bool should_paint_own_contents =
        is_painting_composited_foreground && should_paint_content;
    bool should_paint_normal_flow_and_pos_z_order_lists =
        is_painting_composited_foreground &&
        !is_painting_overlay_overflow_controls;
    bool is_video = paint_layer_.GetLayoutObject().IsVideo();

    base::Optional<ScopedPaintChunkProperties>
        subsequence_forced_chunk_properties;
    if (subsequence_recorder && paint_layer_.HasSelfPaintingLayerDescendant()) {
      // Prepare for forced paint chunks to ensure chunk id stability to avoid
      // unnecessary full chunk raster invalidations on changed chunk ids.
      // TODO(crbug.com/834606): This may be unnecessary after we refactor
      // raster invalidation not to depend on chunk ids too much.
      subsequence_forced_chunk_properties.emplace(
          context.GetPaintController(),
          paint_layer_.GetLayoutObject()
              .FirstFragment()
              .LocalBorderBoxProperties(),
          paint_layer_, DisplayItem::kUninitializedType);
    }

    if (should_paint_background) {
      if (subsequence_forced_chunk_properties) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkBackground);
      }
      PaintBackgroundForFragments(layer_fragments, context, local_painting_info,
                                  paint_flags);
    }

    if (should_paint_neg_z_order_list) {
      if (subsequence_forced_chunk_properties) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkNegativeZOrderChildren);
      }
      if (PaintChildren(kNegativeZOrderChildren, context, painting_info,
                        paint_flags) == kMayBeClippedByCullRect)
        result = kMayBeClippedByCullRect;
    }

    if (should_paint_own_contents) {
      PaintForegroundForFragments(
          layer_fragments, context, local_painting_info, selection_only,
          !!subsequence_forced_chunk_properties, paint_flags);
    }

    if (!is_video && should_paint_self_outline) {
      PaintSelfOutlineForFragments(layer_fragments, context,
                                   local_painting_info, paint_flags);
    }

    if (should_paint_normal_flow_and_pos_z_order_lists) {
      if (subsequence_forced_chunk_properties) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_,
            DisplayItem::kLayerChunkNormalFlowAndPositiveZOrderChildren);
      }
      if (PaintChildren(kNormalFlowAndPositiveZOrderChildren, context,
                        painting_info, paint_flags) == kMayBeClippedByCullRect)
        result = kMayBeClippedByCullRect;
    }

    if (paint_layer_.GetScrollableArea() &&
        paint_layer_.GetScrollableArea()->HasOverlayOverflowControls()) {
      if (is_painting_overlay_overflow_controls ||
          !paint_layer_.NeedsReorderOverlayOverflowControls()) {
        PaintOverlayOverflowControlsForFragments(
            layer_fragments, context, local_painting_info, paint_flags);
      }
    }

    if (is_video && should_paint_self_outline) {
      // We paint outlines for video later so that they aren't obscured by the
      // video controls.
      PaintSelfOutlineForFragments(layer_fragments, context,
                                   local_painting_info, paint_flags);
    }

    if (!is_painting_overlay_overflow_controls) {
      // For filters, if the layer painted nothing, we need to issue a no-op
      // display item to ensure the filters won't be ignored. For backdrop
      // filters, we issue the display item regardless of other paintings to
      // ensure correct bounds of the composited layer for the backdrop filter.
      if ((paint_layer_.PaintsWithFilters() &&
           display_item_list_size_before_painting ==
               context.GetPaintController().NewDisplayItemList().size()) ||
          paint_layer_.GetLayoutObject().HasBackdropFilter()) {
        PaintEmptyContentForFilters(context);
      }
    }
  }  // FilterPainter block

  bool should_paint_mask = is_painting_mask && should_paint_content &&
                           paint_layer_.GetLayoutObject().HasMask() &&
                           !selection_only;
  if (should_paint_mask) {
    PaintMaskForFragments(layer_fragments, context, local_painting_info,
                          paint_flags);
  } else if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
             is_painting_mask &&
             !(painting_info.GetGlobalPaintFlags() &
               kGlobalPaintFlattenCompositingLayers) &&
             paint_layer_.GetCompositedLayerMapping() &&
             paint_layer_.GetCompositedLayerMapping()->MaskLayer()) {
    // In SPv1 it is possible for CompositedLayerMapping to create a mask layer
    // for just CSS clip-path but without a CSS mask. In that case we need to
    // paint a fully filled mask (which will subsequently clipped by the
    // clip-path), otherwise the mask layer will be empty.
    const auto& fragment_data = paint_layer_.GetLayoutObject().FirstFragment();
    auto state = fragment_data.LocalBorderBoxProperties();
    const auto* properties = fragment_data.PaintProperties();
    DCHECK(properties && properties->Mask());
    state.SetEffect(*properties->Mask());
    if (properties && properties->ClipPathClip()) {
      DCHECK_EQ(properties->ClipPathClip()->Parent(), properties->MaskClip());
      state.SetClip(*properties->ClipPathClip());
    }
    ScopedPaintChunkProperties path_based_clip_path_scope(
        context.GetPaintController(), state,
        *paint_layer_.GetCompositedLayerMapping()->MaskLayer(),
        DisplayItem::kClippingMask);

    const GraphicsLayer* mask_layer =
        paint_layer_.GetCompositedLayerMapping()->MaskLayer();
    ClipRect layer_rect =
        PhysicalRect(PhysicalOffset(mask_layer->OffsetFromLayoutObject()),
                     PhysicalSize(IntSize(mask_layer->Size())));
    FillMaskingFragment(context, layer_rect, *mask_layer);
  }

  clip_path_clipper = base::nullopt;

  paint_layer_.SetPreviousPaintResult(result);
  paint_layer_.SetPreviousCullRect(local_painting_info.cull_rect);
  return result;
}

bool PaintLayerPainter::AtLeastOneFragmentIntersectsDamageRect(
    PaintLayerFragments& fragments,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags local_paint_flags,
    const PhysicalOffset& offset_from_root) {
  if (&paint_layer_ == local_painting_info.root_layer &&
      (local_paint_flags & kPaintLayerPaintingOverflowContents))
    return true;

  // Skip the optimization if the layer is fragmented to avoid complexity
  // about overflows in fragments. LayoutObject painters will do cull rect
  // optimization later.
  if (paint_layer_.EnclosingPaginationLayer() || fragments.size() > 1)
    return true;

  return paint_layer_.IntersectsDamageRect(fragments[0].layer_bounds,
                                           fragments[0].background_rect.Rect(),
                                           offset_from_root);
}

template <typename Function>
static void ForAllFragments(GraphicsContext& context,
                            const PaintLayerFragments& fragments,
                            const Function& function) {
  for (wtf_size_t i = 0; i < fragments.size(); ++i) {
    base::Optional<ScopedDisplayItemFragment> scoped_display_item_fragment;
    if (i)
      scoped_display_item_fragment.emplace(context, i);
    function(fragments[i]);
  }
}

PaintResult PaintLayerPainter::PaintChildren(
    PaintLayerIteration children_to_visit,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  PaintResult result = kFullyPainted;
  if (!paint_layer_.HasSelfPaintingLayerDescendant())
    return result;

  if (paint_layer_.GetLayoutObject().PaintBlockedByDisplayLock(
          DisplayLockLifecycleTarget::kChildren)) {
    return result;
  }

  PaintLayerPaintOrderIterator iterator(paint_layer_, children_to_visit);
  while (PaintLayer* child = iterator.Next()) {
    // If this Layer should paint into its own backing or a grouped backing,
    // that will be done via CompositedLayerMapping::PaintContents() and
    // CompositedLayerMapping::DoPaintTask().
    if (child->PaintsIntoOwnOrGroupedBacking(
            painting_info.GetGlobalPaintFlags()))
      continue;

    if (child->IsReplacedNormalFlowStacking())
      continue;

    if (PaintLayerPainter(*child).Paint(context, painting_info, paint_flags) ==
        kMayBeClippedByCullRect)
      result = kMayBeClippedByCullRect;

    if (const auto* layers_painting_overlay_overflow_controls_after =
            iterator.LayersPaintingOverlayOverflowControlsAfter(child)) {
      for (auto* reparent_overflow_controls_layer :
           *layers_painting_overlay_overflow_controls_after) {
        DCHECK(reparent_overflow_controls_layer
                   ->NeedsReorderOverlayOverflowControls());
        if (PaintLayerPainter(*reparent_overflow_controls_layer)
                .Paint(context, painting_info,
                       kPaintLayerPaintingOverlayOverflowControls) ==
            kMayBeClippedByCullRect)
          result = kMayBeClippedByCullRect;
      }
    }
  }

  return result;
}

void PaintLayerPainter::PaintOverlayOverflowControlsForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  DCHECK(paint_layer_.GetScrollableArea() &&
         paint_layer_.GetScrollableArea()->HasOverlayOverflowControls());

  // We don't need to paint composited overflow controls.
  if (paint_layer_.GetScrollableArea()->HasLayerForHorizontalScrollbar() ||
      paint_layer_.GetScrollableArea()->HasLayerForVerticalScrollbar() ||
      paint_layer_.GetScrollableArea()->HasLayerForScrollCorner())
    return;

  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        if (!fragment.background_rect.IsEmpty()) {
          PaintFragmentWithPhase(PaintPhase::kOverlayOverflowControls, fragment,
                                 context, fragment.background_rect,
                                 painting_info, paint_flags);
        }
      });
}

void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const PaintLayerFragment& fragment,
    GraphicsContext& context,
    const ClipRect& clip_rect,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  DCHECK(paint_layer_.IsSelfPaintingLayer());

  auto chunk_properties = fragment.fragment_data->LocalBorderBoxProperties();
  if (phase == PaintPhase::kMask) {
    const auto* properties = fragment.fragment_data->PaintProperties();
    DCHECK(properties && properties->Mask());
    chunk_properties.SetEffect(*properties->Mask());
    // Special case for SPv1 composited mask layer. Path-based clip-path
    // is only applies to the mask chunk, but not to the layer property
    // or local box box property.
    if (properties->ClipPathClip() &&
        properties->ClipPathClip()->Parent() == properties->MaskClip()) {
      chunk_properties.SetClip(*properties->ClipPathClip());
    }
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  PhysicalRect new_cull_rect(clip_rect.Rect());
  // Now |new_cull_rect| is in the pixel-snapped border box space of
  // |fragment.root_fragment_data|. Adjust it to the containing transform node's
  // space in which we will paint.
  new_cull_rect.Move(PhysicalOffset(
      RoundedIntPoint(fragment.root_fragment_data->PaintOffset())));

  PaintInfo paint_info(context, PixelSnappedIntRect(new_cull_rect), phase,
                       painting_info.GetGlobalPaintFlags(), paint_flags,
                       &painting_info.root_layer->GetLayoutObject(),
                       fragment.fragment_data
                           ? fragment.fragment_data->LogicalTopInFlowThread()
                           : LayoutUnit());
  if (UNLIKELY(paint_layer_.GetLayoutObject().PaintBlockedByDisplayLock(
          DisplayLockLifecycleTarget::kChildren))) {
    paint_info.SetDescendantPaintingBlocked(true);
  }
  paint_layer_.GetLayoutObject().Paint(paint_info);
}

void PaintLayerPainter::PaintBackgroundForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        PaintFragmentWithPhase(PaintPhase::kSelfBlockBackgroundOnly, fragment,
                               context, fragment.background_rect,
                               local_painting_info, paint_flags);
      });
}

void PaintLayerPainter::PaintForegroundForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    bool selection_only,
    bool force_paint_chunks,
    PaintLayerFlags paint_flags) {
  if (selection_only) {
    PaintForegroundForFragmentsWithPhase(PaintPhase::kSelection,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
  } else {
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
        paint_layer_.NeedsPaintPhaseDescendantBlockBackgrounds()) {
      if (force_paint_chunks) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkDescendantBackgrounds);
      }
      PaintForegroundForFragmentsWithPhase(
          PaintPhase::kDescendantBlockBackgroundsOnly, layer_fragments, context,
          local_painting_info, paint_flags);
    }

    if (paint_layer_.GetLayoutObject().GetDocument().InForcedColorsMode()) {
      PaintForegroundForFragmentsWithPhase(
          PaintPhase::kForcedColorsModeBackplate, layer_fragments, context,
          local_painting_info, paint_flags);
    }

    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
        paint_layer_.NeedsPaintPhaseFloat()) {
      if (force_paint_chunks) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkFloat);
      }
      PaintForegroundForFragmentsWithPhase(PaintPhase::kFloat, layer_fragments,
                                           context, local_painting_info,
                                           paint_flags);
    }

    if (force_paint_chunks) {
      context.GetPaintController().ForceNewChunk(
          paint_layer_, DisplayItem::kLayerChunkForeground);
    }

    PaintForegroundForFragmentsWithPhase(PaintPhase::kForeground,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);

    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
        paint_layer_.NeedsPaintPhaseDescendantOutlines()) {
      PaintForegroundForFragmentsWithPhase(PaintPhase::kDescendantOutlinesOnly,
                                           layer_fragments, context,
                                           local_painting_info, paint_flags);
    }
  }
}

void PaintLayerPainter::PaintForegroundForFragmentsWithPhase(
    PaintPhase phase,
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(context, layer_fragments,
                  [&](const PaintLayerFragment& fragment) {
                    if (!fragment.foreground_rect.IsEmpty()) {
                      PaintFragmentWithPhase(phase, fragment, context,
                                             fragment.foreground_rect,
                                             local_painting_info, paint_flags);
                    }
                  });
}

void PaintLayerPainter::PaintSelfOutlineForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        if (!fragment.background_rect.IsEmpty()) {
          PaintFragmentWithPhase(PaintPhase::kSelfOutlineOnly, fragment,
                                 context, fragment.background_rect,
                                 local_painting_info, paint_flags);
        }
      });
}

void PaintLayerPainter::PaintMaskForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(context, layer_fragments,
                  [&](const PaintLayerFragment& fragment) {
                    PaintFragmentWithPhase(PaintPhase::kMask, fragment, context,
                                           fragment.background_rect,
                                           local_painting_info, paint_flags);
                  });
}

void PaintLayerPainter::PaintOverlayOverflowControls(
    GraphicsContext& context,
    const CullRect& cull_rect,
    const GlobalPaintFlags paint_flags) {
  PaintLayerPaintingInfo painting_info(&paint_layer_, cull_rect, paint_flags,
                                       PhysicalOffset());
  Paint(context, painting_info, kPaintLayerPaintingOverlayOverflowControls);
}

void PaintLayerPainter::FillMaskingFragment(GraphicsContext& context,
                                            const ClipRect& clip_rect,
                                            const DisplayItemClient& client) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, client,
                                                  DisplayItem::kClippingMask))
    return;

  DrawingRecorder recorder(context, client, DisplayItem::kClippingMask);
  IntRect snapped_clip_rect = PixelSnappedIntRect(clip_rect.Rect());
  context.FillRect(snapped_clip_rect, Color::kBlack);
}

// Generate a no-op DrawingDisplayItem to ensure a non-empty chunk for the
// filter without content.
void PaintLayerPainter::PaintEmptyContentForFilters(GraphicsContext& context) {
  DCHECK(paint_layer_.PaintsWithFilters() ||
         paint_layer_.GetLayoutObject().HasBackdropFilter());

  ScopedPaintChunkProperties paint_chunk_properties(
      context.GetPaintController(),
      paint_layer_.GetLayoutObject().FirstFragment().LocalBorderBoxProperties(),
      paint_layer_, DisplayItem::kEmptyContentForFilters);
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          context, paint_layer_, DisplayItem::kEmptyContentForFilters))
    return;
  DrawingRecorder recorder(context, paint_layer_,
                           DisplayItem::kEmptyContentForFilters);
}

}  // namespace blink
