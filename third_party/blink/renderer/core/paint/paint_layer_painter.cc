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
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_display_item_fragment.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_effectively_invisible.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/paint/subsequence_recorder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
  PhysicalRect contents_visual_rect = box.PhysicalContentsVisualOverflowRect();
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

PaintResult PaintLayerPainter::Paint(GraphicsContext& context,
                                     PaintFlags paint_flags) {
  const LayoutObject& layout_object = paint_layer_.GetLayoutObject();
  if (UNLIKELY(layout_object.NeedsLayout() &&
               !layout_object.ChildLayoutBlockedByDisplayLock())) {
    // Skip if we need layout. This should never happen. See crbug.com/1244130
    NOTREACHED();
    return kFullyPainted;
  }

  if (layout_object.GetFrameView()->ShouldThrottleRendering())
    return kFullyPainted;

  // Non self-painting layers without self-painting descendants don't need to be
  // painted as their layoutObject() should properly paint itself.
  if (!paint_layer_.IsSelfPaintingLayer() &&
      !paint_layer_.HasSelfPaintingLayerDescendant())
    return kFullyPainted;

  return PaintLayerContents(context, paint_flags);
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

static bool IsUnclippedLayoutView(const PaintLayer& layer) {
  if (IsA<LayoutView>(layer.GetLayoutObject())) {
    const auto* frame = layer.GetLayoutObject().GetFrame();
    if (frame && !frame->ClipsContent())
      return true;
  }
  return false;
}

bool PaintLayerPainter::ShouldUseInfiniteCullRect() {
  if (RuntimeEnabledFeatures::InfiniteCullRectEnabled())
    return true;

  bool is_printing = paint_layer_.GetLayoutObject().GetDocument().Printing();
  if (IsUnclippedLayoutView(paint_layer_) && !is_printing)
    return true;

  // Cull rects and clips can't be propagated across a filter which moves
  // pixels, since the input of the filter may be outside the cull rect /
  // clips yet still result in painted output.
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
    // Cull rect mapping doesn't work under perspective in some cases.
    // See http://crbug.com/887558 for details.
    if (properties->Perspective())
      return true;

    if (const auto* transform = properties->Transform()) {
      // A CSS transform can also have perspective like
      // "transform: perspective(100px) rotateY(45deg)". In these cases, we
      // also want to skip cull rect mapping. See http://crbug.com/887558 for
      // details.
      if (!transform->IsIdentityOr2DTranslation() &&
          transform->Matrix().HasPerspective()) {
        return true;
      }

      // Ensure content under animating transforms is not culled out.
      if (transform->HasActiveTransformAnimation())
        return true;

      // As an optimization, skip cull rect updating for non-composited
      // transforms which have already been painted. This is because the cull
      // rect update, which needs to do complex mapping of the cull rect, can
      // be more expensive than over-painting.
      if (!transform->HasDirectCompositingReasons() &&
          paint_layer_.PreviousPaintResult() == kFullyPainted) {
        return true;
      }
    }
  }

  return false;
}

static gfx::Rect FirstFragmentVisualRect(const LayoutBoxModelObject& object) {
  // We don't want to include overflowing contents.
  PhysicalRect overflow_rect =
      object.IsBox() ? To<LayoutBox>(object).PhysicalSelfVisualOverflowRect()
                     : object.PhysicalVisualOverflowRect();
  overflow_rect.Move(object.FirstFragment().PaintOffset());
  return ToEnclosingRect(overflow_rect);
}

PaintResult PaintLayerPainter::PaintLayerContents(GraphicsContext& context,
                                                  PaintFlags paint_flags) {
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
    return kMayBeClippedByCullRect;
  }

  bool selection_drag_image_only =
      paint_flags & PaintFlag::kSelectionDragImageOnly;
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

  bool is_self_painting_layer = paint_layer_.IsSelfPaintingLayer();
  bool is_unclipped_layout_view = IsUnclippedLayoutView(paint_layer_);

  bool should_paint_content =
      paint_layer_.HasVisibleContent() &&
      // Content under a LayoutSVGHiddenContainer is auxiliary resources for
      // painting. Foreign content should never paint in this situation, as it
      // is primary, not auxiliary.
      !paint_layer_.IsUnderSVGHiddenContainer() && is_self_painting_layer;

  if (object.FirstFragment().NextFragment() || is_unclipped_layout_view) {
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
      if (!is_unclipped_layout_view && paint_layer_.KnownToClipSubtree()) {
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
  absl::optional<SubsequenceRecorder> subsequence_recorder;
  if (should_create_subsequence) {
    if (!paint_layer_.SelfOrDescendantNeedsRepaint() &&
        SubsequenceRecorder::UseCachedSubsequenceIfPossible(context,
                                                            paint_layer_)) {
      return paint_layer_.PreviousPaintResult();
    }
    DCHECK(paint_layer_.SupportsSubsequenceCaching());
    subsequence_recorder.emplace(context, paint_layer_);
  }

  absl::optional<ScopedEffectivelyInvisible> effectively_invisible;
  if (PaintedOutputInvisible(object.StyleRef()))
    effectively_invisible.emplace(context.GetPaintController());

  absl::optional<ScopedPaintChunkProperties> layer_chunk_properties;
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
        PaintWithPhase(PaintPhase::kMask, context, paint_flags);
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
    const NGPhysicalBoxFragment* physical_fragment,
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
    chunk_properties.SetEffect(*properties->Mask());
  }
  ScopedPaintChunkProperties fragment_paint_chunk_properties(
      context.GetPaintController(), chunk_properties, paint_layer_,
      DisplayItem::PaintPhaseToDrawingType(phase));

  PaintInfo paint_info(context, cull_rect, phase, paint_flags);
  if (paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock())
    paint_info.SetDescendantPaintingBlocked(true);

  if (physical_fragment) {
    NGBoxFragmentPainter(*physical_fragment).Paint(paint_info);
  } else {
    paint_info.SetFragmentID(fragment_data.FragmentID());
    paint_layer_.GetLayoutObject().Paint(paint_info);
  }
}

void PaintLayerPainter::PaintWithPhase(PaintPhase phase,
                                       GraphicsContext& context,
                                       PaintFlags paint_flags) {
  const auto* layout_box_with_fragments =
      paint_layer_.GetLayoutBoxWithBlockFragments();
  wtf_size_t fragment_idx = 0u;
  for (const auto* fragment = &paint_layer_.GetLayoutObject().FirstFragment();
       fragment; fragment = fragment->NextFragment(), ++fragment_idx) {
    const NGPhysicalBoxFragment* physical_fragment = nullptr;
    if (layout_box_with_fragments) {
      physical_fragment =
          layout_box_with_fragments->GetPhysicalFragment(fragment_idx);
      DCHECK(physical_fragment);
    }

    absl::optional<ScopedDisplayItemFragment> scoped_display_item_fragment;
    if (fragment_idx)
      scoped_display_item_fragment.emplace(context, fragment_idx);

    PaintFragmentWithPhase(phase, *fragment, physical_fragment, context,
                           paint_flags);
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
