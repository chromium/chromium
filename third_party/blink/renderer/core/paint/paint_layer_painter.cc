// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_effectively_invisible.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_hint.h"
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
  if (style.HasNonInitialBackdropFilter())
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

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant())
    return kFullyPainted;

  // If this layer is totally invisible then there is nothing to paint.
  // In CompositeAfterPaint we simplify this optimization by painting even when
  // effectively invisible but skipping the painted content during layerization
  // in PaintArtifactCompositor.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      paint_layer_.PaintsWithTransparency(
          painting_info.GetGlobalPaintFlags()) &&
      PaintedOutputInvisible(paint_layer_.GetLayoutObject().StyleRef())) {
    return kFullyPainted;
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

static bool ShouldCreateSubsequence(
    const PaintLayer& paint_layer,
    const GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info) {
  // Caching is not needed during printing or painting previews.
  if (paint_layer.GetLayoutObject().GetDocument().IsPrintingOrPaintingPreview())
    return false;

  if (context.GetPaintController().IsSkippingCache())
    return false;

  if (!paint_layer.SupportsSubsequenceCaching())
    return false;

  // Don't create subsequence during special painting to avoid cache conflict
  // with normal painting.
  if (painting_info.GetGlobalPaintFlags() &
      kGlobalPaintFlattenCompositingLayers)
    return false;

  // Don't create subsequence for a composited layer because if it can be
  // cached, we can skip the whole painting in GraphicsLayer::paint() with
  // CachedDisplayItemList.  This also avoids conflict of
  // PaintLayer::previousXXX() when paintLayer is composited scrolling and is
  // painted twice for GraphicsLayers of container and scrolling contents.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      (paint_layer.GetCompositingState() == kPaintsIntoOwnBacking))
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
  if (!RuntimeEnabledFeatures::CullRectUpdateEnabled() &&
      paint_layer.PreviousCullRect() != painting_info.cull_rect) {
    if (paint_layer.PreviousPaintResult() == kMayBeClippedByCullRect)
      return true;
    // When PaintUnderInvalidationChecking is enabled, always repaint the
    // subsequence when the paint rect changes because we will strictly match
    // new and cached subsequences. Normally we can reuse the cached fully
    // painted subsequence even if we would partially paint this time.
    if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled())
      return true;
  }

  return false;
}

static bool IsUnclippedLayoutView(const PaintLayer& layer) {
  // If MainFrameClipsContent is false which means that WebPreferences::
  // record_whole_document is true, we should not cull the scrolling contents
  // of the main frame.
  if (IsA<LayoutView>(layer.GetLayoutObject())) {
    const auto* frame = layer.GetLayoutObject().GetFrame();
    if (frame && !frame->ClipsContent())
      return true;
  }
  return false;
}

bool PaintLayerPainter::ShouldUseInfiniteCullRect() {
  DCHECK(RuntimeEnabledFeatures::CullRectUpdateEnabled());
  return ShouldUseInfiniteCullRectInternal(kGlobalPaintNormalPhase,
                                           /*for_cull_rect_update*/ true);
}

bool PaintLayerPainter::ShouldUseInfiniteCullRectInternal(
    GlobalPaintFlags global_flags,
    bool for_cull_rect_update) {
  bool is_printing = paint_layer_.GetLayoutObject().GetDocument().Printing();
  if (IsUnclippedLayoutView(paint_layer_) && !is_printing)
    return true;

  // Cull rects and clips can't be propagated across a filter which moves
  // pixels, since the input of the filter may be outside the cull rect /
  // clips yet still result in painted output.
  // TODO(wangxianzhu): With CullRectUpdate, we can let CullRect support
  // mapping for pixel moving filters to avoid this infinite cull rect.
  if (paint_layer_.HasFilterThatMovesPixels() &&
      // However during printing, we don't want filter outset to cross page
      // boundaries. This also avoids performance issue because the PDF renderer
      // is super slow for big filters. Otherwise all filtered contents would
      // appear in the painted result of every page.
      // TODO(crbug.com/1098995): For now we don't adjust cull rect for clips.
      // When we do, we need to check if we are painting under a real clip.
      // This won't be a problem when we use block fragments for printing.
      !is_printing)
    return true;

  // Cull rect mapping doesn't work under perspective in some cases.
  // See http://crbug.com/887558 for details.
  if (paint_layer_.GetLayoutObject().StyleRef().HasPerspective())
    return true;

  if (const auto* properties =
          paint_layer_.GetLayoutObject().FirstFragment().PaintProperties()) {
    if (properties->Perspective())
      return true;
    if (for_cull_rect_update) {
      // A CSS transform can also have perspective like
      // "transform: perspective(100px) rotateY(45deg)".
      if (const auto* transform = properties->Transform()) {
        if (!transform->IsIdentityOr2DTranslation() &&
            transform->Matrix().HasPerspective())
          return true;
      }
    }
  }

  // We do not apply cull rect optimizations across transforms for two
  // reasons:
  //   1) Performance: We can optimize transform changes by not repainting.
  //   2) Complexity: Difficulty updating clips when ancestor transforms
  //      change.
  // For these reasons, we use an infinite dirty rect here.
  // The reasons don't apply for CullRectUpdate.
  if (!for_cull_rect_update && paint_layer_.PaintsWithTransform(global_flags) &&
      // The reasons don't apply for printing though, because when we enter and
      // leaving printing mode, full invalidations occur.
      !is_printing)
    return true;

  return false;
}

void PaintLayerPainter::AdjustForPaintProperties(
    const GraphicsContext& context,
    PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags& paint_flags) {
  const auto& first_fragment = paint_layer_.GetLayoutObject().FirstFragment();

  bool should_use_infinite_cull_rect =
      ShouldUseInfiniteCullRectInternal(painting_info.GetGlobalPaintFlags(),
                                        /*for_cull_rect_update*/ false);
  if (should_use_infinite_cull_rect) {
    painting_info.cull_rect = CullRect::Infinite();
    // Avoid clipping during CollectFragments.
    if (IsUnclippedLayoutView(paint_layer_))
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
    const auto& destination_transform =
        first_fragment.LocalBorderBoxProperties().Transform();
    if (source_transform == &destination_transform)
      return;

    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      DCHECK(RuntimeEnabledFeatures::CullRectUpdateEnabled());
    } else if (!painting_info.cull_rect.IsInfinite()) {
      auto rect = painting_info.cull_rect.Rect();
      const FragmentData& primary_stitching_fragment =
          paint_layer_.GetLayoutObject().PrimaryStitchingFragment();
      const FragmentData& primary_root_stitching_fragment =
          painting_info.root_layer->GetLayoutObject()
              .PrimaryStitchingFragment();
      primary_root_stitching_fragment.MapRectToFragment(
          primary_stitching_fragment, rect);
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

static IntRect FirstFragmentVisualRect(const LayoutBoxModelObject& object) {
  // We don't want to include overflowing contents.
  PhysicalRect overflow_rect =
      object.IsBox() ? To<LayoutBox>(object).PhysicalSelfVisualOverflowRect()
                     : object.PhysicalVisualOverflowRect();
  overflow_rect.Move(object.FirstFragment().PaintOffset());
  return EnclosingIntRect(overflow_rect);
}

PaintResult PaintLayerPainter::PaintLayerContents(
    GraphicsContext& context,
    const PaintLayerPaintingInfo& painting_info_arg,
    PaintLayerFlags paint_flags_arg) {
  DCHECK(paint_layer_.IsSelfPaintingLayer() ||
         paint_layer_.HasSelfPaintingLayerDescendant());

  const auto& object = paint_layer_.GetLayoutObject();
  PaintResult result = kFullyPainted;
  if (object.GetFrameView()->ShouldThrottleRendering())
    return result;

  // A paint layer should always have LocalBorderBoxProperties when it's ready
  // for paint.
  if (!object.FirstFragment().HasLocalBorderBoxProperties()) {
    // TODO(crbug.com/848056): This can happen e.g. when we paint a filter
    // referencing a SVG foreign object through feImage, especially when there
    // is circular references. Should find a better solution.
    if (!RuntimeEnabledFeatures::CullRectUpdateEnabled())
      paint_layer_.SetPreviousCullRect(CullRect());
    return kMayBeClippedByCullRect;
  }

  bool selection_drag_image_only = painting_info_arg.GetGlobalPaintFlags() &
                                   kGlobalPaintSelectionDragImageOnly;
  if (selection_drag_image_only && !object.IsSelected())
    return result;

  IgnorePaintTimingScope ignore_paint_timing;
  if (object.StyleRef().Opacity() == 0.0f) {
    IgnorePaintTimingScope::IncrementIgnoreDepth();
  }
  // Explicitly compute opacity of documentElement, as it is special-cased in
  // Largest Contentful Paint.
  bool is_document_element_invisible = false;
  if (const auto* document_element = object.GetDocument().documentElement()) {
    if (document_element->GetLayoutObject() &&
        document_element->GetLayoutObject()->StyleRef().Opacity() == 0.0f) {
      is_document_element_invisible = true;
    }
  }
  IgnorePaintTimingScope::SetIsDocumentElementInvisible(
      is_document_element_invisible);

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
      is_painting_composited_decoration && object.StyleRef().HasOutline();

  PhysicalOffset subpixel_accumulation =
      (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
       !(painting_info.GetGlobalPaintFlags() &
         kGlobalPaintFlattenCompositingLayers) &&
       paint_layer_.GetCompositingState() == kPaintsIntoOwnBacking)
          ? paint_layer_.SubpixelAccumulation()
          : painting_info.sub_pixel_accumulation;

  ShouldRespectOverflowClipType respect_overflow_clip =
      ShouldRespectOverflowClip(paint_flags, object);

  bool should_paint_content =
      paint_layer_.HasVisibleContent() &&
      // Content under a LayoutSVGHiddenContainer is auxiliary resources for
      // painting. Foreign content should never paint in this situation, as it
      // is primary, not auxiliary.
      !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer &&
      !is_painting_overlay_overflow_controls;

  // TODO(paint-dev): Become block fragmentation aware. Unconditionally using
  // the first fragment doesn't seem right.
  PhysicalOffset offset_from_root = object.FirstFragment().PaintOffset();
  if (const PaintLayer* root = painting_info.root_layer)
    offset_from_root -= root->GetLayoutObject().FirstFragment().PaintOffset();
  offset_from_root += subpixel_accumulation;

  IntRect visual_rect = FirstFragmentVisualRect(object);
  if (RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
    if (object.FirstFragment().NextFragment()) {
      result = kMayBeClippedByCullRect;
    } else {
      IntRect cull_rect = object.FirstFragment().GetCullRect().Rect();
      bool cull_rect_intersects_self = cull_rect.Intersects(visual_rect);
      if (!cull_rect.Contains(visual_rect))
        result = kMayBeClippedByCullRect;

      bool cull_rect_intersects_contents = true;
      if (const auto* box = DynamicTo<LayoutBox>(object)) {
        PhysicalRect contents_visual_rect =
            box->PhysicalContentsVisualOverflowRect();
        contents_visual_rect.Move(object.FirstFragment().PaintOffset());
        PhysicalRect contents_cull_rect(
            object.FirstFragment().GetContentsCullRect().Rect());
        cull_rect_intersects_contents =
            contents_cull_rect.Intersects(contents_visual_rect);
        if (!contents_cull_rect.Contains(contents_visual_rect))
          result = kMayBeClippedByCullRect;
      } else {
        cull_rect_intersects_contents = cull_rect_intersects_self;
      }

      if (!cull_rect_intersects_self && !cull_rect_intersects_contents)
        should_paint_content = false;

      // The above doesn't consider clips on non-self-painting contents.
      // Will update in ScopedBoxContentsPaintState.
    }
  } else {
    PhysicalRect bounds = paint_layer_.PhysicalBoundingBox(offset_from_root);
    if (!PhysicalRect(painting_info.cull_rect.Rect()).Contains(bounds))
      result = kMayBeClippedByCullRect;
  }

  PaintLayerPaintingInfo local_painting_info(painting_info);
  local_painting_info.sub_pixel_accumulation = subpixel_accumulation;

  PaintLayerFragments layer_fragments;

  if (should_paint_content || should_paint_self_outline ||
      is_painting_overlay_overflow_controls) {
    // Collect the fragments. If CullRectUpdate is enabled, this will just
    // create a light-weight adapter from FragmentData to PaintLayerFragment
    // and we'll remove the adapter in the future. Otherwise this will compute
    // the clip rectangles and paint offsets for each layer fragment.
    paint_layer_.CollectFragments(
        layer_fragments, local_painting_info.root_layer,
        &local_painting_info.cull_rect, kIgnoreOverlayScrollbarSize,
        respect_overflow_clip, &offset_from_root,
        local_painting_info.sub_pixel_accumulation);

    if (!RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
      // PaintLayer::CollectFragments depends on the paint dirty rect in
      // complicated ways. For now, always assume a partially painted output
      // for fragmented content.
      if (layer_fragments.size() > 1)
        result = kMayBeClippedByCullRect;

      if (should_paint_content) {
        should_paint_content = AtLeastOneFragmentIntersectsDamageRect(
            layer_fragments, local_painting_info, paint_flags,
            offset_from_root);
        if (!should_paint_content)
          result = kMayBeClippedByCullRect;
      }
    }
  }

  bool should_create_subsequence =
      should_paint_content &&
      ShouldCreateSubsequence(paint_layer_, context, painting_info);
  absl::optional<SubsequenceRecorder> subsequence_recorder;
  if (should_create_subsequence) {
    if (!ShouldRepaintSubsequence(paint_layer_, painting_info) &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_)) {
      return paint_layer_.PreviousPaintResult();
    }
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  }

  bool is_painting_root_layer = (&paint_layer_) == painting_info.root_layer;
  bool should_paint_background =
      should_paint_content && !selection_drag_image_only &&
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
  bool is_video = IsA<LayoutVideo>(object);

  absl::optional<ScopedEffectivelyInvisible> effectively_invisible;
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
      PaintedOutputInvisible(object.StyleRef()))
    effectively_invisible.emplace(context.GetPaintController());

  absl::optional<ScopedPaintChunkHint> paint_chunk_hint;
  if (should_paint_content) {
    paint_chunk_hint.emplace(context.GetPaintController(),
                             object.FirstFragment().LocalBorderBoxProperties(),
                             paint_layer_, DisplayItem::kLayerChunk,
                             visual_rect);
  }

  if (should_paint_background) {
    PaintBackgroundForFragmentsWithPhase(PaintPhase::kSelfBlockBackgroundOnly,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
  }

  if (should_paint_neg_z_order_list) {
    if (PaintChildren(kNegativeZOrderChildren, context, painting_info,
                      paint_flags) == kMayBeClippedByCullRect)
      result = kMayBeClippedByCullRect;
  }

  if (should_paint_own_contents) {
    absl::optional<ScopedPaintChunkHint> paint_chunk_hint_foreground;
    if (paint_chunk_hint && paint_chunk_hint->HasCreatedPaintChunk()) {
      // Hint a foreground chunk if we have created any chunks, to give the
      // paint chunk after the previous forced paint chunks a stable id.
      paint_chunk_hint_foreground.emplace(
          context.GetPaintController(), paint_layer_,
          DisplayItem::kLayerChunkForeground, visual_rect);
    }
    if (selection_drag_image_only) {
      PaintForegroundForFragmentsWithPhase(PaintPhase::kSelectionDragImage,
                                           layer_fragments, context,
                                           local_painting_info, paint_flags);
    } else {
      PaintForegroundForFragments(layer_fragments, context, local_painting_info,
                                  paint_flags);
    }
  }

  if (!is_video && should_paint_self_outline) {
    PaintBackgroundForFragmentsWithPhase(PaintPhase::kSelfOutlineOnly,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
  }

  if (should_paint_normal_flow_and_pos_z_order_lists) {
    if (PaintChildren(kNormalFlowAndPositiveZOrderChildren, context,
                      painting_info, paint_flags) == kMayBeClippedByCullRect)
      result = kMayBeClippedByCullRect;
  }

  if (paint_layer_.GetScrollableArea() &&
      paint_layer_.GetScrollableArea()
          ->ShouldOverflowControlsPaintAsOverlay()) {
    if (is_painting_overlay_overflow_controls ||
        !paint_layer_.NeedsReorderOverlayOverflowControls()) {
      PaintOverlayOverflowControlsForFragments(
          layer_fragments, context, local_painting_info, paint_flags);
    }
  }

  if (is_video && should_paint_self_outline) {
    // We paint outlines for video later so that they aren't obscured by the
    // video controls.
    PaintBackgroundForFragmentsWithPhase(PaintPhase::kSelfOutlineOnly,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
  }

  if (is_painting_mask && should_paint_content && !selection_drag_image_only) {
    if (const auto* properties = object.FirstFragment().PaintProperties()) {
      if (properties->Mask()) {
        PaintBackgroundForFragmentsWithPhase(PaintPhase::kMask, layer_fragments,
                                             context, local_painting_info,
                                             paint_flags);
      }
      if (properties->ClipPathMask()) {
        PhysicalOffset visual_offset_from_root =
            paint_layer_.EnclosingPaginationLayer()
                ? paint_layer_.VisualOffsetFromAncestor(
                      local_painting_info.root_layer, subpixel_accumulation)
                : offset_from_root;
        ClipPathClipper::PaintClipPathAsMaskImage(context, object, object,
                                                  visual_offset_from_root);
      }
    }
  }

  paint_layer_.SetPreviousPaintResult(result);
  if (!RuntimeEnabledFeatures::CullRectUpdateEnabled())
    paint_layer_.SetPreviousCullRect(local_painting_info.cull_rect);
  return result;
}

bool PaintLayerPainter::AtLeastOneFragmentIntersectsDamageRect(
    PaintLayerFragments& fragments,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags local_paint_flags,
    const PhysicalOffset& offset_from_root) {
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());

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
    absl::optional<ScopedDisplayItemFragment> scoped_display_item_fragment;
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

  if (paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return result;

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
  DCHECK(
      paint_layer_.GetScrollableArea() &&
      paint_layer_.GetScrollableArea()->ShouldOverflowControlsPaintAsOverlay());

  // We don't need to paint composited overflow controls.
  if (paint_layer_.GetScrollableArea()->HasLayerForHorizontalScrollbar() ||
      paint_layer_.GetScrollableArea()->HasLayerForVerticalScrollbar() ||
      paint_layer_.GetScrollableArea()->HasLayerForScrollCorner())
    return;

  PaintBackgroundForFragmentsWithPhase(PaintPhase::kOverlayOverflowControls,
                                       layer_fragments, context, painting_info,
                                       paint_flags);
}

void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const PaintLayerFragment& fragment,
    GraphicsContext& context,
    const CullRect& cull_rect,
    const PaintLayerPaintingInfo& painting_info,
    PaintLayerFlags paint_flags) {
  DCHECK(paint_layer_.IsSelfPaintingLayer());

  auto chunk_properties = fragment.fragment_data->LocalBorderBoxProperties();
  if (phase == PaintPhase::kMask) {
    const auto* properties = fragment.fragment_data->PaintProperties();
    DCHECK(properties);
    DCHECK(properties->Mask());
    chunk_properties.SetEffect(*properties->Mask());
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  PaintInfo paint_info(context, cull_rect, phase,
                       painting_info.GetGlobalPaintFlags(), paint_flags,
                       &painting_info.root_layer->GetLayoutObject());
  if (paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock())
    paint_info.SetDescendantPaintingBlocked(true);

  if (fragment.physical_fragment) {
    NGBoxFragmentPainter(*fragment.physical_fragment).Paint(paint_info);
  } else {
    if (fragment.fragment_data)
      paint_info.SetFragmentID(fragment.fragment_data->FragmentID());
    paint_layer_.GetLayoutObject().Paint(paint_info);
  }
}

static CullRect LegacyCullRect(const PaintLayerFragment& fragment,
                               const ClipRect& clip_rect) {
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());
  PhysicalRect new_cull_rect(clip_rect.Rect());
  // Now |new_cull_rect| is in the pixel-snapped border box space of
  // |fragment.root_fragment_data|. Adjust it to the containing transform node's
  // space in which we will paint.
  new_cull_rect.Move(PhysicalOffset(
      RoundedIntPoint(fragment.root_fragment_data->PaintOffset())));
  return CullRect(PixelSnappedIntRect(new_cull_rect));
}

void PaintLayerPainter::PaintBackgroundForFragmentsWithPhase(
    PaintPhase phase,
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  ForAllFragments(
      context, layer_fragments, [&](const PaintLayerFragment& fragment) {
        CullRect cull_rect =
            RuntimeEnabledFeatures::CullRectUpdateEnabled()
                ? fragment.fragment_data->GetCullRect()
                : LegacyCullRect(fragment, fragment.background_rect);
        if (!cull_rect.Rect().IsEmpty()) {
          PaintFragmentWithPhase(phase, fragment, context, cull_rect,
                                 local_painting_info, paint_flags);
        }
      });
}

void PaintLayerPainter::PaintForegroundForFragments(
    const PaintLayerFragments& layer_fragments,
    GraphicsContext& context,
    const PaintLayerPaintingInfo& local_painting_info,
    PaintLayerFlags paint_flags) {
  PaintForegroundForFragmentsWithPhase(
      PaintPhase::kDescendantBlockBackgroundsOnly, layer_fragments, context,
      local_painting_info, paint_flags);

  if (paint_layer_.GetLayoutObject().GetDocument().InForcedColorsMode()) {
    PaintForegroundForFragmentsWithPhase(PaintPhase::kForcedColorsModeBackplate,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
  }

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseFloat()) {
    PaintForegroundForFragmentsWithPhase(PaintPhase::kFloat, layer_fragments,
                                         context, local_painting_info,
                                         paint_flags);
  }

  PaintForegroundForFragmentsWithPhase(PaintPhase::kForeground, layer_fragments,
                                       context, local_painting_info,
                                       paint_flags);

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseDescendantOutlines()) {
    PaintForegroundForFragmentsWithPhase(PaintPhase::kDescendantOutlinesOnly,
                                         layer_fragments, context,
                                         local_painting_info, paint_flags);
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
        CullRect cull_rect;
        if (RuntimeEnabledFeatures::CullRectUpdateEnabled()) {
          // In CullRectUpdate, this function is the same as
          // PaintBackgroundForFragmentsWithPhase(). The contents cull rect will
          // be applied by ScopedBoxContentsPaintState.
          cull_rect = fragment.fragment_data->GetCullRect();
        } else {
          cull_rect = LegacyCullRect(fragment, fragment.foreground_rect);
        }
        if (!cull_rect.Rect().IsEmpty()) {
          PaintFragmentWithPhase(phase, fragment, context, cull_rect,
                                 local_painting_info, paint_flags);
        }
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

}  // namespace blink
