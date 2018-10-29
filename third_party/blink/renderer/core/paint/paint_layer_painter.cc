// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
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

static inline bool ShouldSuppressPaintingLayer(const PaintLayer& layer) {
  // Avoid painting descendants of the root layer when stylesheets haven't
  // loaded. This avoids some FOUC.  It's ok not to draw, because later on, when
  // all the stylesheets do load, Document::styleResolverMayHaveChanged() will
  // invalidate all painted output via a call to
  // LayoutView::invalidatePaintForViewAndCompositedLayers().  We also avoid
  // caching subsequences in this mode; see shouldCreateSubsequence().
  return layer.GetLayoutObject()
             .GetDocument()
             .DidLayoutWithPendingStylesheets() &&
         !layer.IsRootLayer() && !layer.GetLayoutObject().IsDocumentElement();
}

void PaintLayerPainter::Paint(GraphicsContext& context,
                              const LayoutRect& damage_rect,
                              const GlobalPaintFlags global_paint_flags,
                              PaintLayerFlags paint_flags) {
  PaintLayerPaintingInfo painting_info(
      &paint_layer_, LayoutRect(EnclosingIntRect(damage_rect)),
      global_paint_flags, LayoutSize());
  if (!paint_layer_.PaintsIntoOwnOrGroupedBacking(global_paint_flags))
    Paint(context, painting_info, paint_flags);
}

static ShouldRespectOverflowClipType ShouldRespectOverflowClip(
    PaintLayerFlags paint_flags,
    const LayoutObject& layout_object) {
  return (paint_flags & kPaintLayerPaintingOverflowContents ||
          (paint_flags & kPaintLayerPaintingChildClippingMaskPhase &&
           layout_object.HasClipPath()))
             ? kIgnoreOverflowClip
             : kRespectOverflowClip;
}

bool PaintLayerPainter::PaintedOutputInvisible(
    const ComputedStyle& style,
    GlobalPaintFlags global_paint_flags) const {
  if (style.HasBackdropFilter())
    return false;

  // Always paint when 'will-change: opacity' is present. Reduces jank for
  // common animation implementation approaches, for example, an element that
  // starts with opacity zero and later begins to animate.
  if (style.HasWillChangeOpacityHint())
    return false;

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (style.Opacity())
      return false;

    const auto* effect = paint_layer_.GetLayoutObject()
                             .FirstFragment()
                             .PaintProperties()
                             ->Effect();
    if (effect && effect->RequiresCompositingForAnimation()) {
      return false;
    }
  }

  // 0.0004f < 1/2048. With 10-bit color channels (only available on the
  // newest Macs; otherwise it's 8-bit), we see that an alpha of 1/2048 or
  // less leads to a color output of less than 0.5 in all channels, hence
  // not visible.
  static const float kMinimumVisibleOpacity = 0.0004f;
  if (paint_layer_.PaintsWithTransparency(global_paint_flags)) {
    if (style.Opacity() < kMinimumVisibleOpacity) {
      return true;
    }
  }
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

  if (paint_layer_.GetCompositingState() != kNotComposited) {
    if (painting_info.GetGlobalPaintFlags() &
        kGlobalPaintFlattenCompositingLayers) {
      // FIXME: ok, but what about GlobalPaintFlattenCompositingLayers? That's
      // for printing and drag-image.
      // FIXME: why isn't the code here global, as opposed to being set on each
      // paint() call?
      paint_flags |= kPaintLayerUncachedClipRects;
    }
  }

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant())
    return kFullyPainted;

  if (ShouldSuppressPaintingLayer(paint_layer_))
    return kFullyPainted;

  // If this layer is totally invisible then there is nothing to paint. In SPv2
  // we simplify this optimization by painting even when effectively invisible
  // but skipping the painted content during layerization in
  // PaintArtifactCompositor.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      PaintedOutputInvisible(paint_layer_.GetLayoutObject().StyleRef(),
                             painting_info.GetGlobalPaintFlags())) {
    return kFullyPainted;
  }

  // If the transform can't be inverted, then don't paint anything.
  if (paint_layer_.PaintsWithTransform(painting_info.GetGlobalPaintFlags()) &&
      !paint_layer_.RenderableTransform(painting_info.GetGlobalPaintFlags())
           .IsInvertible()) {
    return kFullyPainted;
  }

  if (paint_layer_.PaintsWithTransparency(painting_info.GetGlobalPaintFlags()))
    paint_flags |= kPaintLayerHaveTransparency;

  paint_flags |= kPaintLayerPaintingCompositingAllPhases;
  return PaintLayerContents(context, painting_info, paint_flags);
}

static bool ShouldCreateSubsequence(const PaintLayer& paint_layer,
                                    const GraphicsContext& context,
                                    const PaintLayerPaintingInfo& painting_info,
                                    PaintLayerFlags paint_flags) {
  // Caching is not needed during printing.
  if (context.Printing())
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
  if (paint_flags &
      (kPaintLayerPaintingOverlayScrollbars | kPaintLayerUncachedClipRects))
    return false;

  // When in FOUC-avoidance mode, don't cache any subsequences, to avoid having
  // to invalidate all of them when leaving this mode. There is an early-out in
  // BlockPainter::paintContents that may result in nothing getting painted in
  // this mode, in addition to early-out logic in PaintLayerPainter.
  if (paint_layer.GetLayoutObject()
          .GetDocument()
          .DidLayoutWithPendingStylesheets())
    return false;

  return true;
}

static bool ShouldRepaintSubsequence(
    PaintLayer& paint_layer,
    const PaintLayerPaintingInfo& painting_info,
    ShouldRespectOverflowClipType respect_overflow_clip,
    bool& should_clear_empty_paint_phase_flags) {
  bool needs_repaint = false;

  // We should set shouldResetEmptyPaintPhaseFlags if some previously unpainted
  // objects may begin to be painted, causing a previously empty paint phase to
  // become non-empty.

  // Repaint subsequence if the layer is marked for needing repaint.
  // We don't set needsResetEmptyPaintPhase here, but clear the empty paint
  // phase flags in PaintLayer::setNeedsPaintPhaseXXX(), to ensure that we won't
  // clear previousPaintPhaseXXXEmpty flags when unrelated things changed which
  // won't cause the paint phases to become non-empty.
  if (paint_layer.NeedsRepaint())
    needs_repaint = true;

  // Repaint if previously the layer might be clipped by paintDirtyRect and
  // paintDirtyRect changes.
  if ((paint_layer.PreviousPaintResult() == kMayBeClippedByPaintDirtyRect ||
       // When PaintUnderInvalidationChecking is enabled, always repaint the
       // subsequence when the paint rect changes because we will strictly match
       // new and cached subsequences. Normally we can reuse the cached fully
       // painted subsequence even if we would partially paint this time.
       RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) &&
      paint_layer.PreviousPaintDirtyRect() != painting_info.paint_dirty_rect) {
    needs_repaint = true;
    should_clear_empty_paint_phase_flags = true;
  }
  paint_layer.SetPreviousPaintDirtyRect(painting_info.paint_dirty_rect);

  return needs_repaint;
}

static bool ShouldUseInfiniteDirtyRect(const GraphicsContext& context,
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
  if (layer.PaintsWithTransform(painting_info.GetGlobalPaintFlags())) {
    // The reasons don't apply for printing though, because when we enter and
    // leaving printing mode, full invalidations occur.
    return !context.Printing();
  }

  return false;
}

void PaintLayerPainter::AdjustForPaintProperties(
    const GraphicsContext& context,
    PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags& paint_flags) {
  const auto& first_fragment = paint_layer_.GetLayoutObject().FirstFragment();

  bool is_using_infinite_dirty_rect = painting_info.paint_dirty_rect ==
                                      LayoutRect(LayoutRect::InfiniteIntRect());
  bool should_use_infinite_dirty_rect =
      ShouldUseInfiniteDirtyRect(context, paint_layer_, painting_info);
  if (!is_using_infinite_dirty_rect && should_use_infinite_dirty_rect) {
    painting_info.paint_dirty_rect = LayoutRect(LayoutRect::InfiniteIntRect());
    is_using_infinite_dirty_rect = true;
  }

  if (painting_info.root_layer == &paint_layer_)
    return;

  const auto& first_root_fragment =
      painting_info.root_layer->GetLayoutObject().FirstFragment();
  bool transform_changed =
      first_root_fragment.LocalBorderBoxProperties().Transform() !=
      first_fragment.LocalBorderBoxProperties().Transform();

  // Will use the current layer as the new root layer if the layer requires
  // infinite dirty rect or has different transform space from the current
  // root layer.
  if (!should_use_infinite_dirty_rect && !transform_changed)
    return;

  if (!is_using_infinite_dirty_rect && transform_changed) {
    // painting_info.paint_dirty_rect is currently in
    // |painting_info.root_layer|'s pixel-snapped border box space. We need to
    // adjust it into |paint_layer_|'s space.
    // This handles the following cases:
    // - The current layer has PaintOffsetTranslation;
    // - The current layer's transform state escapes the root layers contents
    //   transform, e.g. a fixed-position layer;
    // - Scroll offsets.
    first_root_fragment.MapRectToFragment(first_fragment,
                                          painting_info.paint_dirty_rect);
  }

  // Make the current layer the new root layer.
  painting_info.root_layer = &paint_layer_;
  // These flags no longer apply for the new root layer.
  paint_flags &= ~kPaintLayerPaintingSkipRootBackground;
  paint_flags &= ~kPaintLayerPaintingOverflowContents;
  paint_flags &= ~kPaintLayerPaintingCompositingScrollingPhase;

  if (first_fragment.PaintProperties() &&
      first_fragment.PaintProperties()->PaintOffsetTranslation()) {
    painting_info.sub_pixel_accumulation =
        ToLayoutSize(first_fragment.PaintOffset());
  }
}

PaintResult PaintLayerPainter::PaintLayerContents(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info_arg,
    PaintLayerFlags paint_flags_arg) {
  PaintLayerFlags paint_flags = paint_flags_arg;
  PaintResult result = kFullyPainted;

  if (paint_layer_.GetLayoutObject().GetFrameView()->ShouldThrottleRendering())
    return result;

  // A paint layer should always have LocalBorderBoxProperties when it's ready
  // for paint.
  if (!paint_layer_.GetLayoutObject()
           .FirstFragment()
           .HasLocalBorderBoxProperties()) {
    // TODO(crbug.com/848056): This can happen e.g. when we paint a filter
    // referencing a SVG foreign object through feImage, especially when there
    // is circular references. Should find a better solution.
    paint_layer_.SetPreviousPaintDirtyRect(LayoutRect());
    return kMayBeClippedByPaintDirtyRect;
  }

  DCHECK(paint_layer_.IsSelfPaintingLayer() ||
         paint_layer_.HasSelfPaintingLayerDescendant());

  bool is_self_painting_layer = paint_layer_.IsSelfPaintingLayer();
  bool is_painting_overlay_scrollbars =
      paint_flags & kPaintLayerPaintingOverlayScrollbars;
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
      is_self_painting_layer && !is_painting_overlay_scrollbars &&
      (is_painting_composited_decoration ||
       (!is_painting_overflow_contents && !is_painting_mask)) &&
      paint_layer_.GetLayoutObject().StyleRef().HasOutline();

  LayoutSize subpixel_accumulation =
      paint_layer_.GetCompositingState() == kPaintsIntoOwnBacking
          ? paint_layer_.SubpixelAccumulation()
          : painting_info_arg.sub_pixel_accumulation;

  PaintLayerPaintingInfo painting_info = painting_info_arg;
  AdjustForPaintProperties(context, painting_info, paint_flags);

  ShouldRespectOverflowClipType respect_overflow_clip =
      ShouldRespectOverflowClip(paint_flags, paint_layer_.GetLayoutObject());

  bool should_create_subsequence = ShouldCreateSubsequence(
      paint_layer_, context, painting_info_arg, paint_flags);

  base::Optional<SubsequenceRecorder> subsequence_recorder;
  bool should_clear_empty_paint_phase_flags = false;
  if (should_create_subsequence) {
    if (!ShouldRepaintSubsequence(paint_layer_, painting_info,
                                  respect_overflow_clip,
                                  should_clear_empty_paint_phase_flags) &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_))
      return result;
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  } else {
    should_clear_empty_paint_phase_flags = true;
  }

  if (should_clear_empty_paint_phase_flags) {
    paint_layer_.SetPreviousPaintPhaseDescendantOutlinesEmpty(false);
    paint_layer_.SetPreviousPaintPhaseFloatEmpty(false);
    paint_layer_.SetPreviousPaintPhaseDescendantBlockBackgroundsEmpty(false);
  }

  LayoutPoint offset_from_root;
  paint_layer_.ConvertToLayerCoords(painting_info.root_layer, offset_from_root);
  offset_from_root.Move(subpixel_accumulation);

  LayoutRect bounds = paint_layer_.PhysicalBoundingBox(offset_from_root);
  if (!painting_info.paint_dirty_rect.Contains(bounds))
    result = kMayBeClippedByPaintDirtyRect;

  // These helpers output clip and compositing operations using a RAII pattern.
  // Stack-allocated-varibles are destructed in the reverse order of
  // construction, so they are nested properly.
  base::Optional<ClipPathClipper> clip_path_clipper;
  bool should_paint_clip_path =
      is_painting_mask && paint_layer_.GetLayoutObject().HasClipPath();
  if (should_paint_clip_path) {
    LayoutPoint visual_offset_from_root =
        paint_layer_.EnclosingPaginationLayer()
            ? paint_layer_.VisualOffsetFromAncestor(
                  painting_info.root_layer, LayoutPoint(subpixel_accumulation))
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
      !is_painting_overlay_scrollbars;

  PaintLayerFragments layer_fragments;

  if (should_paint_content || should_paint_self_outline ||
      is_painting_overlay_scrollbars) {
    // Collect the fragments. This will compute the clip rectangles and paint
    // offsets for each layer fragment.
    LayoutPoint offset_to_clipper;
    const PaintLayer* paint_layer_for_fragments = &paint_layer_;
    if (paint_flags & kPaintLayerPaintingAncestorClippingMaskPhase) {
      // Compute fragments and their clips with respect to the outermost
      // clipping container. This handles nested border radius by including
      // all of them in the mask.
      //
      // The paint rect is in this layer's space, so convert it to the clipper's
      // layer's space. The root_layer is also changed to the clipper's layer to
      // simplify coordinate system adjustments. The change to root_layer must
      // persist to correctly record the clips.
      paint_layer_for_fragments =
          paint_layer_.EnclosingLayerWithCompositedLayerMapping(kExcludeSelf);
      local_painting_info.root_layer = paint_layer_for_fragments;
      paint_layer_.ConvertToLayerCoords(local_painting_info.root_layer,
                                        offset_to_clipper);
      local_painting_info.paint_dirty_rect.MoveBy(offset_to_clipper);
      // Overflow clip of the compositing container is irrelevant.
      respect_overflow_clip = kIgnoreOverflowClip;
    }

    paint_layer_for_fragments->CollectFragments(
        layer_fragments, local_painting_info.root_layer,
        &local_painting_info.paint_dirty_rect,
        kIgnorePlatformOverlayScrollbarSize, respect_overflow_clip,
        &offset_from_root, local_painting_info.sub_pixel_accumulation);

    // PaintLayer::CollectFragments depends on the paint dirty rect in
    // complicated ways. For now, always assume a partially painted output
    // for fragmented content.
    if (layer_fragments.size() > 1)
      result = kMayBeClippedByPaintDirtyRect;

    if (paint_flags & kPaintLayerPaintingAncestorClippingMaskPhase) {
      // Fragment offsets have been computed in the clipping container's
      // layer's coordinate system, but for the rest of painting we need
      // them in the layer coordinate. So move them and the
      // foreground/background rects that are also in the clipper's space.
      LayoutSize negative_offset(-offset_to_clipper.X(),
                                 -offset_to_clipper.Y());
      for (auto& fragment : layer_fragments) {
        fragment.background_rect.Move(negative_offset);
        fragment.foreground_rect.Move(negative_offset);
      }
    } else if (should_paint_content) {
      should_paint_content = AtLeastOneFragmentIntersectsDamageRect(
          layer_fragments, local_painting_info, paint_flags, offset_from_root);
      if (!should_paint_content)
        result = kMayBeClippedByPaintDirtyRect;
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
        (is_painting_scrolling_content && is_painting_overflow_contents) ||
        (!is_painting_scrolling_content && is_painting_composited_background);
    bool should_paint_own_contents =
        is_painting_composited_foreground && should_paint_content;
    bool should_paint_normal_flow_and_pos_z_order_lists =
        is_painting_composited_foreground;
    bool should_paint_overlay_scrollbars = is_painting_overlay_scrollbars;

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
      PaintBackgroundForFragments(layer_fragments, context,
                                  local_painting_info, paint_flags);
    }

    if (should_paint_neg_z_order_list) {
      if (subsequence_forced_chunk_properties) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkNegativeZOrderChildren);
      }
      if (PaintChildren(kNegativeZOrderChildren, context, painting_info,
                        paint_flags) == kMayBeClippedByPaintDirtyRect)
        result = kMayBeClippedByPaintDirtyRect;
    }

    if (should_paint_own_contents) {
      PaintForegroundForFragments(
          layer_fragments, context, local_painting_info, selection_only,
          !!subsequence_forced_chunk_properties, paint_flags);
    }

    if (should_paint_self_outline) {
      PaintSelfOutlineForFragments(layer_fragments, context,
                                   local_painting_info, paint_flags);
    }

    if (should_paint_normal_flow_and_pos_z_order_lists) {
      if (subsequence_forced_chunk_properties) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_,
            DisplayItem::kLayerChunkNormalFlowAndPositiveZOrderChildren);
      }
      if (PaintChildren(kNormalFlowChildren | kPositiveZOrderChildren, context,
                        painting_info,
                        paint_flags) == kMayBeClippedByPaintDirtyRect)
        result = kMayBeClippedByPaintDirtyRect;
    }

    if (should_paint_overlay_scrollbars) {
      PaintOverflowControlsForFragments(layer_fragments, context,
                                        local_painting_info, paint_flags);
    }

    if (!is_painting_overlay_scrollbars && paint_layer_.PaintsWithFilters() &&
        display_item_list_size_before_painting ==
            context.GetPaintController().NewDisplayItemList().size()) {
      // If a layer with filters painted nothing, we need to issue a no-op
      // display item to ensure the filters won't be ignored.
      PaintEmptyContentForFilters(context);
    }
  }  // FilterPainter block

  bool should_paint_mask = is_painting_mask && should_paint_content &&
                           paint_layer_.GetLayoutObject().HasMask() &&
                           !selection_only;
  if (should_paint_mask) {
    PaintMaskForFragments(layer_fragments, context, local_painting_info,
                          paint_flags);
  } else if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
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
    state.SetEffect(properties->Mask());
    if (properties && properties->ClipPathClip()) {
      DCHECK_EQ(properties->ClipPathClip()->Parent(), properties->MaskClip());
      state.SetClip(properties->ClipPathClip());
    }
    ScopedPaintChunkProperties path_based_clip_path_scope(
        context.GetPaintController(), state,
        *paint_layer_.GetCompositedLayerMapping()->MaskLayer(),
        DisplayItem::kClippingMask);

    const GraphicsLayer* mask_layer =
        paint_layer_.GetCompositedLayerMapping()->MaskLayer();
    ClipRect layer_rect = LayoutRect(
        LayoutPoint(LayoutSize(mask_layer->OffsetFromLayoutObject())),
        LayoutSize(IntSize(mask_layer->Size())));
    FillMaskingFragment(context, layer_rect, *mask_layer);
  }

  clip_path_clipper = base::nullopt;

  if (should_paint_content && !selection_only) {
    // Paint the border radius mask for the fragments.
    if (paint_flags & kPaintLayerPaintingAncestorClippingMaskPhase) {
      // |layer_fragments| comes from the compositing container which doesn't
      // have multiple fragments.
      DCHECK_EQ(1u, layer_fragments.size());
      PaintAncestorClippingMask(layer_fragments[0], context,
                                local_painting_info, paint_flags);
    }
    if (paint_flags & kPaintLayerPaintingChildClippingMaskPhase) {
      PaintChildClippingMaskForFragments(layer_fragments, context,
                                         local_painting_info, paint_flags);
    }
  }

  if (subsequence_recorder)
    paint_layer_.SetPreviousPaintResult(result);
  return result;
}

bool PaintLayerPainter::AtLeastOneFragmentIntersectsDamageRect(
    PaintLayerFragments& fragments,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags local_paint_flags,
    const LayoutPoint& offset_from_root) {
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
    unsigned children_to_visit,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  PaintResult result = kFullyPainted;
  if (!paint_layer_.HasSelfPaintingLayerDescendant())
    return result;
  if (!paint_layer_.StackingNode())
    return result;
#if DCHECK_IS_ON()
  LayerListMutationDetector mutation_checker(paint_layer_.StackingNode());
#endif

  PaintLayerStackingNodeIterator iterator(*paint_layer_.StackingNode(),
                                          children_to_visit);
  PaintLayer* child = iterator.Next();
  if (!child)
    return result;

  for (; child; child = iterator.Next()) {
    // If this Layer should paint into its own backing or a grouped backing,
    // that will be done via CompositedLayerMapping::PaintContents() and
    // CompositedLayerMapping::DoPaintTask().
    if (child->PaintsIntoOwnOrGroupedBacking(
            painting_info.GetGlobalPaintFlags()))
      continue;

    if (child->IsReplacedNormalFlowStacking())
      continue;

    if (PaintLayerPainter(*child).Paint(context, painting_info, paint_flags) ==
        kMayBeClippedByPaintDirtyRect)
      result = kMayBeClippedByPaintDirtyRect;
  }

  return result;
}

void PaintLayerPainter::PaintOverflowControlsForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  PaintLayerScrollableArea* scrollable_area = paint_layer_.GetScrollableArea();
  if (!scrollable_area)
    return;

  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        ScopedPaintChunkProperties fragment_paint_chunk_properties(
            context.GetPaintController(),
            fragment.fragment_data->LocalBorderBoxProperties(), paint_layer_,
            DisplayItem::kOverflowControls);

        // We need to apply the same clips and transforms that
        // paintFragmentWithPhase would have.
        LayoutRect cull_rect = fragment.background_rect.Rect();
        PaintInfo paint_info(
            context, PixelSnappedIntRect(cull_rect),
            PaintPhase::kSelfBlockBackgroundOnly,
            painting_info.GetGlobalPaintFlags(), paint_flags,
            &painting_info.root_layer->GetLayoutObject(),
            fragment.fragment_data
                ? fragment.fragment_data->LogicalTopInFlowThread()
                : LayoutUnit());
        // We pass IntPoint() as the paint offset here, because
        // ScrollableArea::paintOverflowControls just ignores it and uses the
        // offset found in a previous pass.
        ScrollableAreaPainter(*scrollable_area)
            .PaintOverflowControls(paint_info, IntPoint(),
                                   true /* painting_overlay_controls */);
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
    chunk_properties.SetEffect(properties->Mask());
    // Special case for SPv1 composited mask layer. Path-based clip-path
    // is only applies to the mask chunk, but not to the layer property
    // or local box box property.
    if (properties->ClipPathClip() &&
        properties->ClipPathClip()->Parent() == properties->MaskClip()) {
      chunk_properties.SetClip(properties->ClipPathClip());
    }
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  LayoutRect new_cull_rect(clip_rect.Rect());
  // Now |new_cull_rect| is in the pixel-snapped border box space of
  // |fragment.root_fragment_data|. Adjust it to the containing transform node's
  // space in which we will paint.
  new_cull_rect.MoveBy(
      RoundedIntPoint(fragment.root_fragment_data->PaintOffset()));

  // If we had pending stylesheets, we should avoid painting descendants of
  // layout view to avoid FOUC.
  bool suppress_painting_descendants = paint_layer_.GetLayoutObject()
                                           .GetDocument()
                                           .DidLayoutWithPendingStylesheets();
  PaintInfo paint_info(context, PixelSnappedIntRect(new_cull_rect), phase,
                       painting_info.GetGlobalPaintFlags(), paint_flags,
                       &painting_info.root_layer->GetLayoutObject(),
                       fragment.fragment_data
                           ? fragment.fragment_data->LogicalTopInFlowThread()
                           : LayoutUnit(),
                       suppress_painting_descendants);

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
      size_t size_before =
          context.GetPaintController().NewDisplayItemList().size();
      PaintForegroundForFragmentsWithPhase(
          PaintPhase::kDescendantBlockBackgroundsOnly, layer_fragments, context,
          local_painting_info, paint_flags);
      // Don't set the empty flag if we are not painting the whole background.
      if (!(paint_flags & kPaintLayerPaintingSkipRootBackground)) {
        bool phase_is_empty =
            context.GetPaintController().NewDisplayItemList().size() ==
            size_before;
        DCHECK(phase_is_empty ||
               paint_layer_.NeedsPaintPhaseDescendantBlockBackgrounds());
        paint_layer_.SetPreviousPaintPhaseDescendantBlockBackgroundsEmpty(
            phase_is_empty);
      }
    }

    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
        paint_layer_.NeedsPaintPhaseFloat()) {
      if (force_paint_chunks) {
        context.GetPaintController().ForceNewChunk(
            paint_layer_, DisplayItem::kLayerChunkFloat);
      }
      size_t size_before =
          context.GetPaintController().NewDisplayItemList().size();
      PaintForegroundForFragmentsWithPhase(PaintPhase::kFloat, layer_fragments,
                                           context, local_painting_info,
                                           paint_flags);
      bool phase_is_empty =
          context.GetPaintController().NewDisplayItemList().size() ==
          size_before;
      DCHECK(phase_is_empty || paint_layer_.NeedsPaintPhaseFloat());
      paint_layer_.SetPreviousPaintPhaseFloatEmpty(phase_is_empty);
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
      size_t size_before =
          context.GetPaintController().NewDisplayItemList().size();
      PaintForegroundForFragmentsWithPhase(PaintPhase::kDescendantOutlinesOnly,
                                           layer_fragments, context,
                                           local_painting_info, paint_flags);
      bool phase_is_empty =
          context.GetPaintController().NewDisplayItemList().size() ==
          size_before;
      DCHECK(phase_is_empty ||
             paint_layer_.NeedsPaintPhaseDescendantOutlines());
      paint_layer_.SetPreviousPaintPhaseDescendantOutlinesEmpty(phase_is_empty);
    }
  }
}

void PaintLayerPainter::PaintForegroundForFragmentsWithPhase(
    PaintPhase phase,
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        if (!fragment.foreground_rect.IsEmpty()) {
          PaintFragmentWithPhase(phase, fragment, context,
                                 fragment.foreground_rect, local_painting_info,
                                 paint_flags);
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

void PaintLayerPainter::PaintAncestorClippingMask(
    const PaintLayerFragment& fragment,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  const DisplayItemClient& client =
      *paint_layer_.GetCompositedLayerMapping()->AncestorClippingMaskLayer();
  const auto& layer_fragment = paint_layer_.GetLayoutObject().FirstFragment();
  auto state = layer_fragment.PreEffectProperties();
  // This is a hack to incorporate mask-based clip-path.
  // See CompositingLayerPropertyUpdater.cpp about AncestorClippingMaskLayer.
  state.SetEffect(layer_fragment.PreFilter());
  ScopedPaintChunkProperties properties(context.GetPaintController(), state,
                                        client, DisplayItem::kClippingMask);
  ClipRect mask_rect = fragment.background_rect;
  mask_rect.MoveBy(layer_fragment.PaintOffset());
  FillMaskingFragment(context, mask_rect, client);
}

void PaintLayerPainter::PaintChildClippingMaskForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  const DisplayItemClient& client =
      *paint_layer_.GetCompositedLayerMapping()->ChildClippingMaskLayer();
  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        // Use the LocalBorderboxProperties as a starting point to ensure that
        // we don't include the scroll offset when painting the mask layer.
        auto state = fragment.fragment_data->LocalBorderBoxProperties();
        // This is a hack to incorporate mask-based clip-path.
        // See CompositingLayerPropertyUpdater.cpp about
        // ChildClippingMaskLayer.
        state.SetEffect(fragment.fragment_data->PreFilter());
        // Update the clip to be the ContentsProperties clip, since it
        // includes the InnerBorderRadiusClip.
        state.SetClip(fragment.fragment_data->ContentsProperties().Clip());
        ScopedPaintChunkProperties fragment_paint_chunk_properties(
            context.GetPaintController(), state, client,
            DisplayItem::kClippingMask);
        ClipRect mask_rect = fragment.background_rect;
        FillMaskingFragment(context, mask_rect, client);
      });
}

void PaintLayerPainter::PaintOverlayScrollbars(
    GraphicsContext& context,
    const LayoutRect& damage_rect,
    const GlobalPaintFlags paint_flags) {
  if (!paint_layer_.ContainsDirtyOverlayScrollbars())
    return;

  PaintLayerPaintingInfo painting_info(
      &paint_layer_, LayoutRect(EnclosingIntRect(damage_rect)), paint_flags,
      LayoutSize());
  Paint(context, painting_info, kPaintLayerPaintingOverlayScrollbars);

  paint_layer_.SetContainsDirtyOverlayScrollbars(false);
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
  DCHECK(paint_layer_.PaintsWithFilters());

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
