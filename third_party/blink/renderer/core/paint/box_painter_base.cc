// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter_base.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/scoped_interpolation_quality.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void BoxPainterBase::PaintFillLayers(const PaintInfo& paint_info,
                                     const Color& c,
                                     const FillLayer& fill_layer,
                                     const PhysicalRect& rect,
                                     BackgroundImageGeometry& geometry,
                                     BackgroundBleedAvoidance bleed) {
  FillLayerOcclusionOutputList reversed_paint_list;
  bool should_draw_background_in_separate_buffer =
      CalculateFillLayerOcclusionCulling(reversed_paint_list, fill_layer);

  // TODO(trchen): We can optimize out isolation group if we have a
  // non-transparent background color and the bottom layer encloses all other
  // layers.
  GraphicsContext& context = paint_info.context;
  if (should_draw_background_in_separate_buffer)
    context.BeginLayer();

  for (auto it = reversed_paint_list.rbegin(); it != reversed_paint_list.rend();
       ++it) {
    PaintFillLayer(paint_info, c, **it, rect, bleed, geometry);
  }

  if (should_draw_background_in_separate_buffer)
    context.EndLayer();
}

void BoxPainterBase::PaintNormalBoxShadow(const PaintInfo& info,
                                          const PhysicalRect& paint_rect,
                                          const ComputedStyle& style,
                                          bool include_logical_left_edge,
                                          bool include_logical_right_edge,
                                          bool background_is_skipped) {
  if (!style.BoxShadow())
    return;
  GraphicsContext& context = info.context;

  FloatRoundedRect border = style.GetRoundedBorderFor(
      paint_rect.ToLayoutRect(), include_logical_left_edge,
      include_logical_right_edge);

  bool has_border_radius = style.HasBorderRadius();
  bool has_opaque_background =
      !background_is_skipped &&
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor()).Alpha() ==
          255;

  GraphicsContextStateSaver state_saver(context, false);

  const ShadowList* shadow_list = style.BoxShadow();
  for (wtf_size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != kNormal)
      continue;

    FloatSize shadow_offset(shadow.X(), shadow.Y());
    float shadow_blur = shadow.Blur();
    float shadow_spread = shadow.Spread();

    if (shadow_offset.IsZero() && !shadow_blur && !shadow_spread)
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(GetCSSPropertyColor()));

    FloatRect fill_rect = border.Rect();
    fill_rect.Inflate(shadow_spread);
    if (fill_rect.IsEmpty())
      continue;

    // Save the state and clip, if not already done.
    // The clip does not depend on any shadow-specific properties.
    if (!state_saver.Saved()) {
      state_saver.Save();
      if (has_border_radius) {
        FloatRoundedRect rect_to_clip_out = border;

        // If the box is opaque, it is unnecessary to clip it out. However,
        // doing so saves time when painting the shadow. On the other hand, it
        // introduces subpixel gaps along the corners. Those are avoided by
        // insetting the clipping path by one CSS pixel.
        if (has_opaque_background)
          rect_to_clip_out.InflateWithRadii(-1);

        if (!rect_to_clip_out.IsEmpty())
          context.ClipOutRoundedRect(rect_to_clip_out);
      } else {
        // This IntRect is correct even with fractional shadows, because it is
        // used for the rectangle of the box itself, which is always
        // pixel-aligned.
        FloatRect rect_to_clip_out = border.Rect();

        // If the box is opaque, it is unnecessary to clip it out. However,
        // doing so saves time when painting the shadow. On the other hand, it
        // introduces subpixel gaps along the edges if they are not
        // pixel-aligned. Those are avoided by insetting the clipping path by
        // one CSS pixel.
        if (has_opaque_background)
          rect_to_clip_out.Inflate(-1);

        if (!rect_to_clip_out.IsEmpty())
          context.ClipOut(rect_to_clip_out);
      }
    }

    // Draw only the shadow.
    context.SetShadow(shadow_offset, shadow_blur, shadow_color,
                      DrawLooperBuilder::kShadowRespectsTransforms,
                      DrawLooperBuilder::kShadowIgnoresAlpha, kDrawShadowOnly);

    if (has_border_radius) {
      FloatRoundedRect rounded_fill_rect = border;
      rounded_fill_rect.Inflate(shadow_spread);

      if (shadow_spread >= 0)
        rounded_fill_rect.ExpandRadii(shadow_spread);
      else
        rounded_fill_rect.ShrinkRadii(-shadow_spread);
      if (!rounded_fill_rect.IsRenderable())
        rounded_fill_rect.AdjustRadii();
      rounded_fill_rect.ConstrainRadii();
      context.FillRoundedRect(rounded_fill_rect, Color::kBlack);
    } else {
      context.FillRect(fill_rect, Color::kBlack);
    }
  }
}

void BoxPainterBase::PaintInsetBoxShadowWithBorderRect(
    const PaintInfo& info,
    const PhysicalRect& border_rect,
    const ComputedStyle& style,
    bool include_logical_left_edge,
    bool include_logical_right_edge) {
  if (!style.BoxShadow())
    return;
  auto bounds = style.GetRoundedInnerBorderFor(border_rect.ToLayoutRect(),
                                               include_logical_left_edge,
                                               include_logical_right_edge);
  PaintInsetBoxShadow(info, bounds, style, include_logical_left_edge,
                      include_logical_right_edge);
}

void BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
    const PaintInfo& info,
    const PhysicalRect& inner_rect,
    const ComputedStyle& style) {
  if (!style.BoxShadow())
    return;
  auto bounds = style.GetRoundedInnerBorderFor(inner_rect.ToLayoutRect(),
                                               LayoutRectOutsets());
  PaintInsetBoxShadow(info, bounds, style);
}

void BoxPainterBase::PaintInsetBoxShadow(const PaintInfo& info,
                                         const FloatRoundedRect& bounds,
                                         const ComputedStyle& style,
                                         bool include_logical_left_edge,
                                         bool include_logical_right_edge) {
  GraphicsContext& context = info.context;
  bool is_horizontal = style.IsHorizontalWritingMode();
  GraphicsContextStateSaver state_saver(context, false);

  const ShadowList* shadow_list = style.BoxShadow();
  for (wtf_size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != kInset)
      continue;

    FloatSize shadow_offset(shadow.X(), shadow.Y());
    float shadow_blur = shadow.Blur();
    float shadow_spread = shadow.Spread();

    if (shadow_offset.IsZero() && !shadow_blur && !shadow_spread)
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(GetCSSPropertyColor()));

    // The inset shadow case.
    GraphicsContext::Edges clipped_edges = GraphicsContext::kNoEdge;
    if (!include_logical_left_edge) {
      if (is_horizontal)
        clipped_edges |= GraphicsContext::kLeftEdge;
      else
        clipped_edges |= GraphicsContext::kTopEdge;
    }
    if (!include_logical_right_edge) {
      if (is_horizontal)
        clipped_edges |= GraphicsContext::kRightEdge;
      else
        clipped_edges |= GraphicsContext::kBottomEdge;
    }
    context.DrawInnerShadow(bounds, shadow_color, shadow_offset, shadow_blur,
                            shadow_spread, clipped_edges);
  }
}

bool BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(
    const Document& document,
    const ComputedStyle& style) {
  return document.Printing() &&
         style.PrintColorAdjust() == EPrintColorAdjust::kEconomy &&
         (!document.GetSettings() ||
          !document.GetSettings()->GetShouldPrintBackgrounds());
}

bool BoxPainterBase::CalculateFillLayerOcclusionCulling(
    FillLayerOcclusionOutputList& reversed_paint_list,
    const FillLayer& fill_layer) {
  bool is_non_associative = false;
  for (auto* current_layer = &fill_layer; current_layer;
       current_layer = current_layer->Next()) {
    reversed_paint_list.push_back(current_layer);
    // Stop traversal when an opaque layer is encountered.
    // FIXME : It would be possible for the following occlusion culling test to
    // be more aggressive on layers with no repeat by testing whether the image
    // covers the layout rect.  Testing that here would imply duplicating a lot
    // of calculations that are currently done in
    // LayoutBoxModelObject::paintFillLayer. A more efficient solution might be
    // to move the layer recursion into paintFillLayer, or to compute the layer
    // geometry here and pass it down.

    // TODO(trchen): Need to check compositing mode as well.
    if (current_layer->GetBlendMode() != BlendMode::kNormal)
      is_non_associative = true;

    // TODO(trchen): A fill layer cannot paint if the calculated tile size is
    // empty.  This occlusion check can be wrong.
    if (current_layer->ClipOccludesNextLayers() &&
        current_layer->ImageOccludesNextLayers(*document_, style_)) {
      if (current_layer->Clip() == EFillBox::kBorder)
        is_non_associative = false;
      break;
    }
  }
  return is_non_associative;
}

BoxPainterBase::FillLayerInfo::FillLayerInfo(
    const Document& doc,
    const ComputedStyle& style,
    bool has_overflow_clip,
    Color bg_color,
    const FillLayer& layer,
    BackgroundBleedAvoidance bleed_avoidance,
    bool include_left,
    bool include_right,
    bool is_inline)
    : image(layer.GetImage()),
      color(bg_color),
      include_left_edge(include_left),
      include_right_edge(include_right),
      is_bottom_layer(!layer.Next()),
      is_border_fill(layer.Clip() == EFillBox::kBorder),
      is_clipped_with_local_scrolling(
          has_overflow_clip && layer.Attachment() == EFillAttachment::kLocal) {
  // When printing backgrounds is disabled or using economy mode,
  // change existing background colors and images to a solid white background.
  // If there's no bg color or image, leave it untouched to avoid affecting
  // transparency.  We don't try to avoid loading the background images,
  // because this style flag is only set when printing, and at that point
  // we've already loaded the background images anyway. (To avoid loading the
  // background images we'd have to do this check when applying styles rather
  // than while layout.)
  if (BoxPainterBase::ShouldForceWhiteBackgroundForPrintEconomy(doc, style)) {
    // Note that we can't reuse this variable below because the bgColor might
    // be changed.
    bool should_paint_background_color = is_bottom_layer && color.Alpha();
    if (image || should_paint_background_color) {
      color = Color::kWhite;
      image = nullptr;
    }
  }

  // Background images are not allowed at the inline level in forced colors
  // mode when forced-color-adjust is auto. This ensures that the inline images
  // are not painted on top of the forced colors mode backplate.
  if (doc.InForcedColorsMode() && is_inline &&
      style.ForcedColorAdjust() != EForcedColorAdjust::kNone)
    image = nullptr;

  const bool has_rounded_border =
      style.HasBorderRadius() && (include_left_edge || include_right_edge);
  // BorderFillBox radius clipping is taken care of by
  // BackgroundBleedClip{Only,Layer}
  is_rounded_fill =
      has_rounded_border &&
      !(is_border_fill && BleedAvoidanceIsClipping(bleed_avoidance));

  should_paint_image = image && image->CanRender();
  should_paint_color =
      is_bottom_layer && color.Alpha() &&
      (!should_paint_image || !layer.ImageOccludesNextLayers(doc, style));
}

namespace {

// Given the |size| that the whole image should draw at, and the input phase
// requested by the content, and the space between repeated tiles, return a
// rectangle with |size| and a location that respects the phase but is no more
// than one size + space in magnitude. In practice, this means that if there is
// no repeating the returned rect would contain the destination_offset
// location. The destination_offset passed here must exactly match the location
// of the subset in a following call to ComputeSubsetForBackground.
FloatRect ComputePhaseForBackground(const FloatPoint& destination_offset,
                                    const FloatSize& size,
                                    const FloatPoint& phase,
                                    const FloatSize& spacing) {
  const FloatSize step_per_tile(size + spacing);
  return FloatRect(
      FloatPoint(
          destination_offset.X() + fmodf(-phase.X(), step_per_tile.Width()),
          destination_offset.Y() + fmodf(-phase.Y(), step_per_tile.Height())),
      size);
}

// Compute the image subset, in intrinsic image coordinates, that gets mapped
// onto the |subset|, when the whole image would be drawn with phase and size
// given by |phase_and_size|. Assumes |phase_and_size| contains |subset|. The
// location of the requested subset should be the painting snapped location, or
// whatever was used as a destination_offset in ComputePhaseForBackground.
//
// It is used to undo the offset added in ComputePhaseForBackground. The size
// of requested subset should be the unsnapped size so that the computed
// scale and location in the source image can be correctly determined.
FloatRect ComputeSubsetForBackground(const FloatRect& phase_and_size,
                                     const FloatRect& subset,
                                     const FloatSize& intrinsic_size) {
  // TODO(schenney): Re-enable this after determining why it fails for
  // CAP, and maybe other cases.
  // DCHECK(phase_and_size.Contains(subset));

  const FloatSize scale(phase_and_size.Width() / intrinsic_size.Width(),
                        phase_and_size.Height() / intrinsic_size.Height());
  return FloatRect((subset.X() - phase_and_size.X()) / scale.Width(),
                   (subset.Y() - phase_and_size.Y()) / scale.Height(),
                   subset.Width() / scale.Width(),
                   subset.Height() / scale.Height());
}

// The unsnapped_subset_size should be the target painting area implied by the
//   content, without any snapping applied. It is necessary to correctly
//   compute the subset of the source image to paint into the destination.
// The snapped_paint_rect should be the target destination for painting into.
// The phase is never snapped.
// The tile_size is the total image size. The mapping from this size
//   to the unsnapped_dest_rect size defines the scaling of the image for
//   sprite computation.
void DrawTiledBackground(GraphicsContext& context,
                         Image* image,
                         const FloatSize& unsnapped_subset_size,
                         const FloatRect& snapped_paint_rect,
                         const FloatPoint& phase,
                         const FloatSize& tile_size,
                         SkBlendMode op,
                         const FloatSize& repeat_spacing,
                         bool has_filter_property) {
  DCHECK(!tile_size.IsEmpty());

  // Use the intrinsic size of the image if it has one, otherwise force the
  // generated image to be the tile size.
  FloatSize intrinsic_tile_size(image->Size());
  FloatSize scale(1, 1);
  if (!image->HasIntrinsicSize()) {
    intrinsic_tile_size = tile_size;
  } else {
    scale = FloatSize(tile_size.Width() / intrinsic_tile_size.Width(),
                      tile_size.Height() / intrinsic_tile_size.Height());
  }

  const FloatRect one_tile_rect = ComputePhaseForBackground(
      snapped_paint_rect.Location(), tile_size, phase, repeat_spacing);

  // Check and see if a single draw of the image can cover the entire area we
  // are supposed to tile. The dest_rect_for_subset must use the same
  // location that was used in ComputePhaseForBackground and the unsnapped
  // destination rect in order to correctly evaluate the subset size and
  // location in the presence of border snapping and zoom.
  FloatRect dest_rect_for_subset(snapped_paint_rect.Location(),
                                 unsnapped_subset_size);
  if (one_tile_rect.Contains(dest_rect_for_subset)) {
    FloatRect visible_src_rect = ComputeSubsetForBackground(
        one_tile_rect, dest_rect_for_subset, intrinsic_tile_size);
    // Round to avoid filtering pulling in neighboring pixels, for the
    // common case of sprite maps.
    // TODO(schenney): Snapping at this level is a problem for cases where we
    // might be animating background-position to pan over an image. Ideally we
    // would either snap only if close to integral, or move snapping
    // calculations up the stack.
    visible_src_rect = FloatRect(RoundedIntRect(visible_src_rect));
    context.DrawImage(image, Image::kSyncDecode, snapped_paint_rect,
                      &visible_src_rect, has_filter_property, op,
                      kDoNotRespectImageOrientation);
    return;
  }

  // Note that this tile rect the image's pre-scaled size.
  FloatRect tile_rect(FloatPoint(), intrinsic_tile_size);
  // This call takes the unscaled image, applies the given scale, and paints
  // it into the snapped_dest_rect using phase from one_tile_rect and the
  // given repeat spacing. Note the phase is already scaled.
  context.DrawImageTiled(image, snapped_paint_rect, tile_rect, scale,
                         one_tile_rect.Location(), repeat_spacing, op);
}

inline bool PaintFastBottomLayer(Node* node,
                                 const PaintInfo& paint_info,
                                 const BoxPainterBase::FillLayerInfo& info,
                                 const PhysicalRect& rect,
                                 const FloatRoundedRect& border_rect,
                                 BackgroundImageGeometry& geometry,
                                 Image* image,
                                 SkBlendMode composite_op) {
  // Painting a background image from an ancestor onto a cell is a complex case.
  if (geometry.CellUsingContainerBackground())
    return false;
  // Complex cases not handled on the fast path.
  if (!info.is_bottom_layer || !info.is_border_fill)
    return false;

  // Transparent layer, nothing to paint.
  if (!info.should_paint_color && !info.should_paint_image)
    return true;

  // Compute the destination rect for painting the color here because we may
  // need it for computing the image painting rect for optimization.
  GraphicsContext& context = paint_info.context;
  FloatRoundedRect color_border =
      info.is_rounded_fill ? border_rect
                           : FloatRoundedRect(PixelSnappedIntRect(rect));
  // When the layer has an image, figure out whether it is covered by a single
  // tile. The border for painting images may not be the same as the color due
  // to optimizations for the image painting destination that avoid painting
  // under the border.
  FloatRect image_tile;
  FloatRoundedRect image_border;
  if (info.should_paint_image) {
    // Avoid image shaders when printing (poorly supported in PDF).
    if (info.is_rounded_fill && paint_info.IsPrinting())
      return false;

    // Compute the dest rect we will be using for images.
    image_border =
        info.is_rounded_fill
            ? color_border
            : FloatRoundedRect(FloatRect(geometry.SnappedDestRect()));

    if (!image_border.Rect().IsEmpty()) {
      // We cannot optimize if the tile is too small.
      if (geometry.TileSize().Width() < image_border.Rect().Width() ||
          geometry.TileSize().Height() < image_border.Rect().Height())
        return false;

      // Phase calculation uses the actual painted location, given by the
      // border-snapped destination rect.
      image_tile = ComputePhaseForBackground(
          FloatPoint(geometry.SnappedDestRect().offset),
          FloatSize(geometry.TileSize()), geometry.Phase(),
          FloatSize(geometry.SpaceSize()));

      // Force the image tile to LayoutUnit precision, which is the precision
      // it was calculated in. This avoids bleeding due to values very close to
      // integers.
      // The test images/sprite-no-bleed.html fails on two of the sub-cases
      // due to this rounding still not being enough to make the Contains check
      // pass. The best way to fix this would be to remove the paint rect offset
      // from the tile computation, because we effectively add it in
      // ComputePhaseForBackground then remove it in ComputeSubsetForBackground.
      image_tile = FloatRect(LayoutRect(image_tile));
      // We cannot optimize if the tile is misaligned.
      if (!image_tile.Contains(image_border.Rect()))
        return false;
    }
  }

  // At this point we're committed to the fast path: the destination (r)rect
  // fits within a single tile, and we can paint it using direct draw(R)Rect()
  // calls.
  base::Optional<RoundedInnerRectClipper> clipper;
  if (info.is_rounded_fill && !color_border.IsRenderable()) {
    // When the rrect is not renderable, we resort to clipping.
    // RoundedInnerRectClipper handles this case via discrete, corner-wise
    // clipping.
    clipper.emplace(context, rect, color_border);
    color_border.SetRadii(FloatRoundedRect::Radii());
  }

  // Paint the color if needed.
  if (info.should_paint_color)
    context.FillRoundedRect(color_border, info.color);

  // Paint the image if needed.
  if (!info.should_paint_image || !image || image_tile.IsEmpty())
    return true;

  // Generated images will be created at the desired tile size, so assume their
  // intrinsic size is the requested tile size.
  bool has_intrinsic_size = image->HasIntrinsicSize();
  const FloatSize intrinsic_tile_size =
      !has_intrinsic_size ? image_tile.Size() : FloatSize(image->Size());
  // Subset computation needs the same location as was used with
  // ComputePhaseForBackground above, but needs the unsnapped destination
  // size to correctly calculate sprite subsets in the presence of zoom. But if
  // this is a generated image sized according to the tile size (which is a
  // snapped value), use the snapped dest rect instead.
  FloatRect dest_rect_for_subset(
      FloatPoint(geometry.SnappedDestRect().offset),
      !has_intrinsic_size ? FloatSize(geometry.SnappedDestRect().size)
                          : FloatSize(geometry.UnsnappedDestRect().size));
  // Content providers almost always choose source pixels at integer locations,
  // so snap to integers. This is particuarly important for sprite maps.
  // Calculation up to this point, in LayoutUnits, can lead to small variations
  // from integer size, so it is safe to round without introducing major issues.
  const FloatRect unrounded_subset = ComputeSubsetForBackground(
      image_tile, dest_rect_for_subset, intrinsic_tile_size);
  FloatRect src_rect = FloatRect(RoundedIntRect(unrounded_subset));

  // If we have rounded the image size to 0, revert the rounding.
  if (src_rect.IsEmpty())
    src_rect = unrounded_subset;

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
               "data",
               inspector_paint_image_event::Data(
                   node, *info.image, FloatRect(image->Rect()),
                   FloatRect(image_border.Rect())));

  // Since there is no way for the developer to specify decode behavior, use
  // kSync by default.
  context.DrawImageRRect(
      image, Image::kSyncDecode, image_border, src_rect,
      node && node->ComputedStyleRef().HasFilterInducingProperty(),
      composite_op);

  if (info.image && info.image->IsImageResource()) {
    PaintTimingDetector::NotifyBackgroundImagePaint(
        node, image, To<StyleFetchedImage>(info.image.Get()),
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
  }
  if (node && info.image && info.image->IsImageResource()) {
    LocalDOMWindow* window = node->GetDocument().domWindow();
    DCHECK(window);
    ImageElementTiming::From(*window).NotifyBackgroundImagePainted(
        node, To<StyleFetchedImage>(info.image.Get()),
        context.GetPaintController().CurrentPaintChunkProperties());
  }
  return true;
}

// Inset the background rect by a "safe" amount: 1/2 border-width for opaque
// border styles, 1/6 border-width for double borders.
FloatRoundedRect BackgroundRoundedRectAdjustedForBleedAvoidance(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    bool object_has_multiple_boxes,
    bool include_logical_left_edge,
    bool include_logical_right_edge,
    FloatRoundedRect background_rounded_rect) {
  // TODO(fmalita): we should be able to fold these parameters into
  // BoxBorderInfo or BoxDecorationData and avoid calling getBorderEdgeInfo
  // redundantly here.
  BorderEdge edges[4];
  style.GetBorderEdgeInfo(edges, include_logical_left_edge,
                          include_logical_right_edge);

  // Use the most conservative inset to avoid mixed-style corner issues.
  float fractional_inset = 1.0f / 2;
  for (auto& edge : edges) {
    if (edge.BorderStyle() == EBorderStyle::kDouble) {
      fractional_inset = 1.0f / 6;
      break;
    }
  }

  FloatRectOutsets insets(
      -fractional_inset * edges[static_cast<unsigned>(BoxSide::kTop)].Width(),
      -fractional_inset * edges[static_cast<unsigned>(BoxSide::kRight)].Width(),
      -fractional_inset *
          edges[static_cast<unsigned>(BoxSide::kBottom)].Width(),
      -fractional_inset * edges[static_cast<unsigned>(BoxSide::kLeft)].Width());

  FloatRect inset_rect(background_rounded_rect.Rect());
  inset_rect.Expand(insets);
  FloatRoundedRect::Radii inset_radii(background_rounded_rect.GetRadii());
  inset_radii.Shrink(-insets.Top(), -insets.Bottom(), -insets.Left(),
                     -insets.Right());
  return FloatRoundedRect(inset_rect, inset_radii);
}

FloatRoundedRect RoundedBorderRectForClip(
    const ComputedStyle& style,
    const BoxPainterBase::FillLayerInfo& info,
    const FillLayer& bg_layer,
    const PhysicalRect& rect,
    bool object_has_multiple_boxes,
    const PhysicalSize& flow_box_size,
    BackgroundBleedAvoidance bleed_avoidance,
    LayoutRectOutsets border_padding_insets) {
  if (!info.is_rounded_fill)
    return FloatRoundedRect();

  FloatRoundedRect border = style.GetRoundedBorderFor(
      rect.ToLayoutRect(), info.include_left_edge, info.include_right_edge);
  if (object_has_multiple_boxes) {
    FloatRoundedRect segment_border = style.GetRoundedBorderFor(
        LayoutRect(LayoutPoint(), LayoutSize(FlooredIntSize(flow_box_size))),
        info.include_left_edge, info.include_right_edge);
    border.SetRadii(segment_border.GetRadii());
  }

  if (info.is_border_fill &&
      bleed_avoidance == kBackgroundBleedShrinkBackground) {
    border = BackgroundRoundedRectAdjustedForBleedAvoidance(
        style, rect, object_has_multiple_boxes, info.include_left_edge,
        info.include_right_edge, border);
  }

  // Clip to the padding or content boxes as necessary.
  if (bg_layer.Clip() == EFillBox::kContent) {
    border = style.GetRoundedInnerBorderFor(
        LayoutRect(border.Rect()), border_padding_insets,
        info.include_left_edge, info.include_right_edge);
  } else if (bg_layer.Clip() == EFillBox::kPadding) {
    border = style.GetRoundedInnerBorderFor(LayoutRect(border.Rect()),
                                            info.include_left_edge,
                                            info.include_right_edge);
  }
  return border;
}

void PaintFillLayerBackground(GraphicsContext& context,
                              const BoxPainterBase::FillLayerInfo& info,
                              Node* node,
                              Image* image,
                              SkBlendMode composite_op,
                              const BackgroundImageGeometry& geometry,
                              const PhysicalRect& scrolled_paint_rect) {
  // Paint the color first underneath all images, culled if background image
  // occludes it.
  // TODO(trchen): In the !bgLayer.hasRepeatXY() case, we could improve the
  // culling test by verifying whether the background image covers the entire
  // painting area.
  if (info.is_bottom_layer && info.color.Alpha() && info.should_paint_color) {
    IntRect background_rect(PixelSnappedIntRect(scrolled_paint_rect));
    context.FillRect(background_rect, info.color);
  }

  // No progressive loading of the background image.
  // NOTE: This method can be called with no image in situations when a bad
  // resource locator is given such as "//:0", so still check for image.
  if (info.should_paint_image && !geometry.SnappedDestRect().IsEmpty() &&
      !geometry.TileSize().IsEmpty() && image) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
                 "data",
                 inspector_paint_image_event::Data(
                     node, *info.image, FloatRect(image->Rect()),
                     FloatRect(scrolled_paint_rect)));
    DrawTiledBackground(
        context, image, FloatSize(geometry.UnsnappedDestRect().size),
        FloatRect(geometry.SnappedDestRect()), geometry.Phase(),
        FloatSize(geometry.TileSize()), composite_op,
        FloatSize(geometry.SpaceSize()),
        node && node->ComputedStyleRef().HasFilterInducingProperty());
    if (info.image && info.image->IsImageResource()) {
      PaintTimingDetector::NotifyBackgroundImagePaint(
          node, image, To<StyleFetchedImage>(info.image.Get()),
          context.GetPaintController().CurrentPaintChunkProperties());
    }
    if (node && info.image && info.image->IsImageResource()) {
      LocalDOMWindow* window = node->GetDocument().domWindow();
      DCHECK(window);
      ImageElementTiming::From(*window).NotifyBackgroundImagePainted(
          node, To<StyleFetchedImage>(info.image.Get()),
          context.GetPaintController().CurrentPaintChunkProperties());
    }
  }
}

LayoutRectOutsets AdjustOutsetsForEdgeInclusion(
    const LayoutRectOutsets outsets,
    const BoxPainterBase::FillLayerInfo& info) {
  LayoutRectOutsets adjusted = outsets;
  if (!info.include_right_edge)
    adjusted.SetRight(LayoutUnit());
  if (!info.include_left_edge)
    adjusted.SetLeft(LayoutUnit());
  return adjusted;
}

bool ShouldApplyBlendOperation(const BoxPainterBase::FillLayerInfo& info,
                               const FillLayer& layer) {
  // For a mask layer, don't use the operator if this is the bottom layer.
  return !info.is_bottom_layer || layer.GetType() != EFillLayerType::kMask;
}

}  // anonymous namespace

LayoutRectOutsets BoxPainterBase::AdjustedBorderOutsets(
    const FillLayerInfo& info) const {
  return AdjustOutsetsForEdgeInclusion(ComputeBorders(), info);
}

void BoxPainterBase::PaintFillLayer(const PaintInfo& paint_info,
                                    const Color& color,
                                    const FillLayer& bg_layer,
                                    const PhysicalRect& rect,
                                    BackgroundBleedAvoidance bleed_avoidance,
                                    BackgroundImageGeometry& geometry,
                                    bool object_has_multiple_boxes,
                                    const PhysicalSize& flow_box_size) {
  GraphicsContext& context = paint_info.context;
  if (rect.IsEmpty())
    return;

  const FillLayerInfo info = GetFillLayerInfo(color, bg_layer, bleed_avoidance);
  // If we're not actually going to paint anything, abort early.
  if (!info.should_paint_image && !info.should_paint_color)
    return;

  GraphicsContextStateSaver clip_with_scrolling_state_saver(
      context, info.is_clipped_with_local_scrolling);
  auto scrolled_paint_rect =
      AdjustRectForScrolledContent(paint_info, info, rect);
  const auto did_adjust_paint_rect = scrolled_paint_rect != rect;

  scoped_refptr<Image> image;
  SkBlendMode composite_op = SkBlendMode::kSrcOver;
  base::Optional<ScopedInterpolationQuality> interpolation_quality_context;
  if (info.should_paint_image) {
    geometry.Calculate(paint_info.PaintContainer(), paint_info.phase,
                       paint_info.GetGlobalPaintFlags(), bg_layer,
                       scrolled_paint_rect);
    image = info.image->GetImage(
        geometry.ImageClient(), geometry.ImageDocument(), geometry.ImageStyle(),
        FloatSize(geometry.TileSize()));
    interpolation_quality_context.emplace(context,
                                          geometry.ImageInterpolationQuality());

    if (bg_layer.MaskSourceType() == EMaskSourceType::kLuminance)
      context.SetColorFilter(kColorFilterLuminanceToAlpha);

    if (ShouldApplyBlendOperation(info, bg_layer)) {
      composite_op = WebCoreCompositeToSkiaComposite(bg_layer.Composite(),
                                                     bg_layer.GetBlendMode());
    }
  }

  LayoutRectOutsets border = ComputeBorders();
  LayoutRectOutsets padding = ComputePadding();
  LayoutRectOutsets border_padding_insets = -(border + padding);
  FloatRoundedRect border_rect = RoundedBorderRectForClip(
      style_, info, bg_layer, rect, object_has_multiple_boxes, flow_box_size,
      bleed_avoidance, border_padding_insets);

  // Fast path for drawing simple color backgrounds. Do not use the fast
  // path with images if the dest rect has been adjusted for scrolling
  // backgrounds because correcting the dest rect for scrolling reduces the
  // accuracy of the destination rects. Also disable the fast path for images
  // if we are shrinking the background for bleed avoidance, because this
  // adjusts the border rects in a way that breaks the optimization.
  bool disable_fast_path =
      info.should_paint_image &&
      (bleed_avoidance == kBackgroundBleedShrinkBackground ||
       did_adjust_paint_rect);
  if (!disable_fast_path &&
      PaintFastBottomLayer(node_, paint_info, info, rect, border_rect, geometry,
                           image.get(), composite_op)) {
    return;
  }

  base::Optional<RoundedInnerRectClipper> clip_to_border;
  if (info.is_rounded_fill)
    clip_to_border.emplace(context, rect, border_rect);

  if (bg_layer.Clip() == EFillBox::kText) {
    PaintFillLayerTextFillBox(context, info, image.get(), composite_op,
                              geometry, rect, scrolled_paint_rect,
                              object_has_multiple_boxes);
    return;
  }

  GraphicsContextStateSaver background_clip_state_saver(context, false);
  switch (bg_layer.Clip()) {
    case EFillBox::kPadding:
    case EFillBox::kContent: {
      if (info.is_rounded_fill)
        break;

      // Clip to the padding or content boxes as necessary.
      PhysicalRect clip_rect = scrolled_paint_rect;
      clip_rect.Contract(AdjustOutsetsForEdgeInclusion(border, info));
      if (bg_layer.Clip() == EFillBox::kContent)
        clip_rect.Contract(AdjustOutsetsForEdgeInclusion(padding, info));
      background_clip_state_saver.Save();
      // TODO(chrishtr): this should be pixel-snapped.
      context.Clip(FloatRect(clip_rect));
      break;
    }
    case EFillBox::kBorder:
      break;
    case EFillBox::kText:  // fall through
    default:
      NOTREACHED();
      break;
  }

  PaintFillLayerBackground(context, info, node_, image.get(), composite_op,
                           geometry, scrolled_paint_rect);
}

void BoxPainterBase::PaintFillLayerTextFillBox(
    GraphicsContext& context,
    const BoxPainterBase::FillLayerInfo& info,
    Image* image,
    SkBlendMode composite_op,
    const BackgroundImageGeometry& geometry,
    const PhysicalRect& rect,
    const PhysicalRect& scrolled_paint_rect,
    bool object_has_multiple_boxes) {
  // First figure out how big the mask has to be. It should be no bigger
  // than what we need to actually render, so we should intersect the dirty
  // rect with the border box of the background.
  IntRect mask_rect = PixelSnappedIntRect(rect);

  // We draw the background into a separate layer, to be later masked with
  // yet another layer holding the text content.
  GraphicsContextStateSaver background_clip_state_saver(context, false);
  background_clip_state_saver.Save();
  context.Clip(mask_rect);
  context.BeginLayer(1, composite_op);

  PaintFillLayerBackground(context, info, node_, image, SkBlendMode::kSrcOver,
                           geometry, scrolled_paint_rect);

  // Create the text mask layer and draw the text into the mask. We do this by
  // painting using a special paint phase that signals to InlineTextBoxes that
  // they should just add their contents to the clip.
  context.BeginLayer(1, SkBlendMode::kDstIn);

  PaintTextClipMask(context, mask_rect, scrolled_paint_rect.offset,
                    object_has_multiple_boxes);

  context.EndLayer();  // Text mask layer.
  context.EndLayer();  // Background layer.
}

void BoxPainterBase::PaintBorder(const ImageResourceObserver& obj,
                                 const Document& document,
                                 Node* node,
                                 const PaintInfo& info,
                                 const PhysicalRect& rect,
                                 const ComputedStyle& style,
                                 BackgroundBleedAvoidance bleed_avoidance,
                                 bool include_logical_left_edge,
                                 bool include_logical_right_edge) {
  // border-image is not affected by border-radius.
  if (NinePieceImagePainter::Paint(info.context, obj, document, node, rect,
                                   style, style.BorderImage())) {
    return;
  }

  const BoxBorderPainter border_painter(rect, style, bleed_avoidance,
                                        include_logical_left_edge,
                                        include_logical_right_edge);
  border_painter.PaintBorder(info, rect);
}

void BoxPainterBase::PaintMaskImages(const PaintInfo& paint_info,
                                     const PhysicalRect& paint_rect,
                                     const ImageResourceObserver& obj,
                                     BackgroundImageGeometry& geometry,
                                     bool include_logical_left_edge,
                                     bool include_logical_right_edge) {
  if (!style_.HasMask() || style_.Visibility() != EVisibility::kVisible)
    return;

  PaintFillLayers(paint_info, Color::kTransparent, style_.MaskLayers(),
                  paint_rect, geometry);
  NinePieceImagePainter::Paint(paint_info.context, obj, *document_, node_,
                               paint_rect, style_, style_.MaskBoxImage(),
                               include_logical_left_edge,
                               include_logical_right_edge);
}

}  // namespace blink
