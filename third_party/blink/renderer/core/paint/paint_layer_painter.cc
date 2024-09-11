// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include <optional>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_effectively_invisible.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {

bool PaintLayerPainter::PaintedOutputInvisible(const ComputedStyle& style) {
  if (style.HasNonInitialBackdropFilter())
    return false;

  // Always paint when 'will-change: opacity' is present. Reduces jank for
  // common animation implementation approaches, for example, an element that
  // starts with opacity zero and later begins to animate.
  if (style.HasWillChangeOpacityHint())
    return false;

  if (style.HasCurrentOpacityAnimation())
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

PhysicalRect PaintLayerPainter::ContentsVisualRect(const FragmentData& fragment,
                                                   const LayoutBox& box) {
  PhysicalRect contents_visual_rect = box.ContentsVisualOverflowRect();
  contents_visual_rect.Move(fragment.PaintOffset());
  const auto* replaced_transform =
      fragment.PaintProperties()
          ? fragment.PaintProperties()->ReplacedContentTransform()
          : nullptr;
  if (replaced_transform) {
    gfx::RectF float_contents_visual_rect(contents_visual_rect);
    GeometryMapper::SourceToDestinationRect(*replaced_transform->Parent(),
                                            *replaced_transform,
                                            float_contents_visual_rect);
    contents_visual_rect =
        PhysicalRect::EnclosingRect(float_contents_visual_rect);
  }
  return contents_visual_rect;
}

static bool ShouldCreateSubsequence(const PaintLayer& paint_layer,
                                    const GraphicsContext& context,
                                    PaintFlags paint_flags) {
  // Caching is not needed during printing or painting previews.
  if (paint_layer.GetLayoutObject().GetDocument().IsPrintingOrPaintingPreview())
    return false;

  if (context.GetPaintController().IsSkippingCache())
    return false;

  if (!paint_layer.SupportsSubsequenceCaching())
    return false;

  // Don't create subsequence during special painting to avoid cache conflict
  // with normal painting.
  if (paint_flags & PaintFlag::kOmitCompositingInfo)
    return false;

  return true;
}

static gfx::Rect FirstFragmentVisualRect(const LayoutBoxModelObject& object) {
  // We don't want to include overflowing contents.
  PhysicalRect overflow_rect =
      object.IsBox() ? To<LayoutBox>(object).SelfVisualOverflowRect()
                     : object.VisualOverflowRect();
  overflow_rect.Move(object.FirstFragment().PaintOffset());
  return ToEnclosingRect(overflow_rect);
}

PaintResult PaintLayerPainter::Paint(GraphicsContext& context,
                                     PaintFlags paint_flags) {
  const auto& object = paint_layer_.GetLayoutObject();
  if (object.NeedsLayout() && !object.ChildLayoutBlockedByDisplayLock())
      [[unlikely]] {
    // Skip if we need layout. This should never happen. See crbug.com/1423308
    // and crbug.com/330051489.
    return kFullyPainted;
  }

  if (object.GetFrameView()->ShouldThrottleRendering())
    return kFullyPainted;

  if (object.IsFragmentLessBox()) {
    return kFullyPainted;
  }

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant())
    return kFullyPainted;

  if (auto* node = DynamicTo<Element>(object.GetNode())) {
    if (node->IsInCanvasSubtree() && !DynamicTo<HTMLCanvasElement>(node)) {
      // This prevents canvas fallback content from being rendered.
      return kFullyPainted;
    }
  }

  std::optional<CheckAncestorPositionVisibilityScope>
      check_position_visibility_scope;
  if (paint_layer_.InvisibleForPositionVisibility() ||
      paint_layer_.HasAncestorInvisibleForPositionVisibility()) {
    return kFullyPainted;
  }
  if (paint_layer_.GetLayoutObject().IsStackingContext()) {
    check_position_visibility_scope.emplace(paint_layer_);
  }

  // A paint layer should always have LocalBorderBoxProperties when it's ready
  // for paint.
  if (!object.FirstFragment().HasLocalBorderBoxProperties()) {
    // TODO(crbug.com/848056): This can happen e.g. when we paint a filter
    // referencing a SVG foreign object through feImage, especially when there
    // is circular references. Should find a better solution.
    return kMayBeClippedByCullRect;
  }

  bool selection_drag_image_only =
      paint_flags & PaintFlag::kSelectionDragImageOnly;
  if (selection_drag_image_only && !object.IsSelected())
    return kFullyPainted;

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

  bool is_self_painting_layer = paint_layer_.IsSelfPaintingLayer();
  bool should_paint_content =
      paint_layer_.HasVisibleContent() &&
      // Content under a LayoutSVGHiddenContainer is auxiliary resources for
      // painting. Foreign content should never paint in this situation, as it
      // is primary, not auxiliary.
      !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer;

  PaintResult result = kFullyPainted;
  if (object.IsFragmented() ||
      // When printing, the LayoutView's background should extend infinitely
      // regardless of LayoutView's visual rect, so don't check intersection
      // between the visual rect and the cull rect (custom for each page).
      (IsA<LayoutView>(object) && object.GetDocument().Printing())) {
    result = kMayBeClippedByCullRect;
  } else {
    gfx::Rect visual_rect = FirstFragmentVisualRect(object);
    gfx::Rect cull_rect = object.FirstFragment().GetCullRect().Rect();
    bool cull_rect_intersects_self = cull_rect.Intersects(visual_rect);
    if (!cull_rect.Contains(visual_rect))
      result = kMayBeClippedByCullRect;

    bool cull_rect_intersects_contents = true;
    if (const auto* box = DynamicTo<LayoutBox>(object)) {
      PhysicalRect contents_visual_rect(
          ContentsVisualRect(object.FirstFragment(), *box));
      PhysicalRect contents_cull_rect(
          object.FirstFragment().GetContentsCullRect().Rect());
      cull_rect_intersects_contents =
          contents_cull_rect.Intersects(contents_visual_rect);
      if (!contents_cull_rect.Contains(contents_visual_rect))
        result = kMayBeClippedByCullRect;
    } else {
      cull_rect_intersects_contents = cull_rect_intersects_self;
    }

    if (!cull_rect_intersects_self && !cull_rect_intersects_contents) {
      if (paint_layer_.KnownToClipSubtreeToPaddingBox()) {
        paint_layer_.SetPreviousPaintResult(kMayBeClippedByCullRect);
        return kMayBeClippedByCullRect;
      }
      should_paint_content = false;
    }

    // The above doesn't consider clips on non-self-painting contents.
    // Will update in ScopedBoxContentsPaintState.
  }

  bool should_create_subsequence =
      should_paint_content &&
      ShouldCreateSubsequence(paint_layer_, context, paint_flags);
  std::optional<SubsequenceRecorder> subsequence_recorder;
  if (should_create_subsequence) {
    if (!paint_layer_.SelfOrDescendantNeedsRepaint() &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_)) {
      return paint_layer_.PreviousPaintResult();
    }
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  }

  std::optional<ScopedEffectivelyInvisible> effectively_invisible;
  if (PaintedOutputInvisible(object.StyleRef()))
    effectively_invisible.emplace(context.GetPaintController());

  std::optional<ScopedPaintChunkProperties> layer_chunk_properties;
  if (should_paint_content) {
    // If we will create a new paint chunk for this layer, this gives the chunk
    // a stable id.
    layer_chunk_properties.emplace(
        context.GetPaintController(),
        object.FirstFragment().LocalBorderBoxProperties(), paint_layer_,
        DisplayItem::kLayerChunk);
  }

  bool should_paint_background =
      should_paint_content && !selection_drag_image_only;
  if (should_paint_background) {
    PaintWithPhase(PaintPhase::kSelfBlockBackgroundOnly, context, paint_flags);
  }

  if (PaintChildren(kNegativeZOrderChildren, context, paint_flags) ==
      kMayBeClippedByCullRect)
    result = kMayBeClippedByCullRect;

  if (should_paint_content) {
    // If the negative-z-order children created paint chunks, this gives the
    // foreground paint chunk a stable id.
    ScopedPaintChunkProperties foreground_properties(
        context.GetPaintController(),
        object.FirstFragment().LocalBorderBoxProperties(), paint_layer_,
        DisplayItem::kLayerChunkForeground);

    if (selection_drag_image_only) {
      PaintWithPhase(PaintPhase::kSelectionDragImage, context, paint_flags);
    } else {
      PaintForegroundPhases(context, paint_flags);
    }
  }

  // Outline always needs to be painted even if we have no visible content.
  bool should_paint_self_outline =
      is_self_painting_layer && object.StyleRef().HasOutline();

  bool is_video = IsA<LayoutVideo>(object);
  if (!is_video && should_paint_self_outline)
    PaintWithPhase(PaintPhase::kSelfOutlineOnly, context, paint_flags);

  if (PaintChildren(kNormalFlowAndPositiveZOrderChildren, context,
                    paint_flags) == kMayBeClippedByCullRect)
    result = kMayBeClippedByCullRect;

  if (should_paint_content && paint_layer_.GetScrollableArea() &&
      paint_layer_.GetScrollableArea()
          ->ShouldOverflowControlsPaintAsOverlay()) {
    if (!paint_layer_.NeedsReorderOverlayOverflowControls())
      PaintOverlayOverflowControls(context, paint_flags);
    // Otherwise the overlay overflow controls will be painted after scrolling
    // children in PaintChildren().
  }
  // Overlay overflow controls of scrollers without a self-painting layer are
  // painted in the foreground paint phase. See ScrollableAreaPainter.

  if (is_video && should_paint_self_outline) {
    // We paint outlines for video later so that they aren't obscured by the
    // video controls.
    PaintWithPhase(PaintPhase::kSelfOutlineOnly, context, paint_flags);
  }

  if (should_paint_content && !selection_drag_image_only) {
    if (const auto* properties = object.FirstFragment().PaintProperties()) {
      if (properties->Mask()) {
        if (object.IsSVGForeignObject()) {
          SVGMaskPainter::Paint(context, object, object);
        } else {
          PaintWithPhase(PaintPhase::kMask, context, paint_flags);
        }
      }
      if (properties->ClipPathMask())
        ClipPathClipper::PaintClipPathAsMaskImage(context, object, object);
    }
  }

  paint_layer_.SetPreviousPaintResult(result);
  return result;
}

PaintResult PaintLayerPainter::PaintChildren(
    PaintLayerIteration children_to_visit,
    GraphicsContext& context,
    PaintFlags paint_flags) {
  PaintResult result = kFullyPainted;
  if (!paint_layer_.HasSelfPaintingLayerDescendant())
    return result;

  if (paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock())
    return result;

  PaintLayerPaintOrderIterator iterator(&paint_layer_, children_to_visit);
  while (PaintLayer* child = iterator.Next()) {
    if (child->IsReplacedNormalFlowStacking())
      continue;

    if (PaintLayerPainter(*child).Paint(context, paint_flags) ==
        kMayBeClippedByCullRect)
      result = kMayBeClippedByCullRect;

    if (const auto* layers_painting_overlay_overflow_controls_after =
            iterator.LayersPaintingOverlayOverflowControlsAfter(child)) {
      for (auto& reparent_overflow_controls_layer :
           *layers_painting_overlay_overflow_controls_after) {
        DCHECK(reparent_overflow_controls_layer
                   ->NeedsReorderOverlayOverflowControls());
        PaintLayerPainter(*reparent_overflow_controls_layer)
            .PaintOverlayOverflowControls(context, paint_flags);
        if (reparent_overflow_controls_layer->PreviousPaintResult() ==
            kMayBeClippedByCullRect) {
          result = kMayBeClippedByCullRect;
        }
      }
    }
  }

  return result;
}

void PaintLayerPainter::PaintOverlayOverflowControls(GraphicsContext& context,
                                                     PaintFlags paint_flags) {
  DCHECK(paint_layer_.GetScrollableArea());
  DCHECK(
      paint_layer_.GetScrollableArea()->ShouldOverflowControlsPaintAsOverlay());
  PaintWithPhase(PaintPhase::kOverlayOverflowControls, context, paint_flags);
}

void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const FragmentData& fragment_data,
    wtf_size_t fragment_data_idx,
    const PhysicalBoxFragment* physical_fragment,
    GraphicsContext& context,
    PaintFlags paint_flags) {
  DCHECK(paint_layer_.IsSelfPaintingLayer() ||
         phase == PaintPhase::kOverlayOverflowControls);

  CullRect cull_rect = fragment_data.GetCullRect();
  if (cull_rect.Rect().IsEmpty())
    return;

  auto chunk_properties = fragment_data.LocalBorderBoxProperties();
  if (phase == PaintPhase::kMask) {
    const auto* properties = fragment_data.PaintProperties();
    DCHECK(properties);
    DCHECK(properties->Mask());
    DCHECK(properties->Mask()->OutputClip());
    chunk_properties.SetEffect(*properties->Mask());
    chunk_properties.SetClip(*properties->Mask()->OutputClip());
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  PaintInfo paint_info(
      context, cull_rect, phase,
      paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock(),
      paint_flags);

  if (physical_fragment) {
    BoxFragmentPainter(*physical_fragment).Paint(paint_info);
  } else if (const auto* layout_inline =
                 DynamicTo<LayoutInline>(&paint_layer_.GetLayoutObject())) {
    InlineBoxFragmentPainter::PaintAllFragments(*layout_inline, fragment_data,
                                                fragment_data_idx, paint_info);
  } else {
    // We are about to enter legacy paint code. Set the right FragmentData
    // object, to use the right paint offset.
    paint_info.SetFragmentDataOverride(&fragment_data);
    paint_layer_.GetLayoutObject().Paint(paint_info);
  }
}

void PaintLayerPainter::PaintWithPhase(PaintPhase phase,
                                       GraphicsContext& context,
                                       PaintFlags paint_flags) {
  const auto* layout_box_with_fragments =
      paint_layer_.GetLayoutBoxWithBlockFragments();
  wtf_size_t fragment_idx = 0u;

  // The NG paint code guards against painting multiple fragments for content
  // that doesn't support it, but the legacy paint code has no such guards.
  // TODO(crbug.com/1229581): Remove this when everything is handled by NG.
  bool multiple_fragments_allowed =
      layout_box_with_fragments ||
      CanPaintMultipleFragments(paint_layer_.GetLayoutObject());

  for (const FragmentData& fragment :
       FragmentDataIterator(paint_layer_.GetLayoutObject())) {
    const PhysicalBoxFragment* physical_fragment = nullptr;
    if (layout_box_with_fragments) {
      physical_fragment =
          layout_box_with_fragments->GetPhysicalFragment(fragment_idx);
      DCHECK(physical_fragment);
    }

    std::optional<ScopedDisplayItemFragment> scoped_display_item_fragment;
    if (fragment_idx)
      scoped_display_item_fragment.emplace(context, fragment_idx);

    PaintFragmentWithPhase(phase, fragment, fragment_idx, physical_fragment,
                           context, paint_flags);

    if (!multiple_fragments_allowed)
      break;

    fragment_idx++;
  }
}

void PaintLayerPainter::PaintForegroundPhases(GraphicsContext& context,
                                              PaintFlags paint_flags) {
  PaintWithPhase(PaintPhase::kDescendantBlockBackgroundsOnly, context,
                 paint_flags);

  if (paint_layer_.GetLayoutObject().GetDocument().InForcedColorsMode()) {
    PaintWithPhase(PaintPhase::kForcedColorsModeBackplate, context,
                   paint_flags);
  }

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseFloat()) {
    PaintWithPhase(PaintPhase::kFloat, context, paint_flags);
  }

  PaintWithPhase(PaintPhase::kForeground, context, paint_flags);

  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      paint_layer_.NeedsPaintPhaseDescendantOutlines()) {
    PaintWithPhase(PaintPhase::kDescendantOutlinesOnly, context, paint_flags);
  }
}

}  // namespace blink
