// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_painter_base.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/css/background_color_paint_image_generator.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/nine_piece_image_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
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

namespace {

void ApplySpreadToShadowShape(FloatRoundedRect& shadow_shape, float spread) {
  if (spread == 0)
    return;

  if (spread >= 0)
    shadow_shape.ExpandRadii(spread);
  else
    shadow_shape.ShrinkRadii(-spread);

  if (!shadow_shape.IsRenderable())
    shadow_shape.AdjustRadii();
  shadow_shape.ConstrainRadii();
}

}  // namespace

void BoxPainterBase::PaintNormalBoxShadow(const PaintInfo& info,
                                          const PhysicalRect& paint_rect,
                                          const ComputedStyle& style,
                                          PhysicalBoxSides sides_to_include,
                                          bool background_is_skipped) {
  if (!style.BoxShadow())
    return;
  GraphicsContext& context = info.context;

  FloatRoundedRect border = RoundedBorderGeometry::PixelSnappedRoundedBorder(
      style, paint_rect, sides_to_include);

  bool has_border_radius = style.HasBorderRadius();
  bool has_opaque_background =
      !background_is_skipped &&
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor()).Alpha() ==
          255;

  GraphicsContextStateSaver state_saver(context, false);

  const ShadowList* shadow_list = style.BoxShadow();
  for (wtf_size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != ShadowStyle::kNormal)
      continue;

    FloatSize shadow_offset(shadow.X(), shadow.Y());
    float shadow_blur = shadow.Blur();
    float shadow_spread = shadow.Spread();

    if (shadow_offset.IsZero() && !shadow_blur && !shadow_spread)
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(GetCSSPropertyColor()),
        style.UsedColorScheme());

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

    // Draw only the shadow. If the color of the shadow is transparent we will
    // set an empty draw looper.
    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(shadow_offset, shadow_blur, shadow_color,
                                  DrawLooperBuilder::kShadowRespectsTransforms,
                                  DrawLooperBuilder::kShadowIgnoresAlpha);
    context.SetDrawLooper(draw_looper_builder.DetachDrawLooper());

    if (has_border_radius) {
      FloatRoundedRect rounded_fill_rect = border;
      rounded_fill_rect.Inflate(shadow_spread);
      ApplySpreadToShadowShape(rounded_fill_rect, shadow_spread);
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
    PhysicalBoxSides sides_to_include) {
  if (!style.BoxShadow())
    return;
  auto bounds = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
      style, border_rect, sides_to_include);
  PaintInsetBoxShadow(info, bounds, style, sides_to_include);
}

void BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
    const PaintInfo& info,
    const PhysicalRect& inner_rect,
    const ComputedStyle& style) {
  if (!style.BoxShadow())
    return;
  auto bounds = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
      style, inner_rect, LayoutRectOutsets());
  PaintInsetBoxShadow(info, bounds, style);
}

namespace {

inline FloatRect AreaCastingShadowInHole(const FloatRect& hole_rect,
                                         const ShadowData& shadow) {
  FloatRect bounds(hole_rect);
  bounds.Inflate(shadow.Blur());

  if (shadow.Spread() < 0)
    bounds.Inflate(-shadow.Spread());

  FloatRect offset_bounds = bounds;
  offset_bounds.MoveBy(-shadow.Location());
  return UnionRect(bounds, offset_bounds);
}

void AdjustInnerRectForSideClipping(FloatRect& inner_rect,
                                    const ShadowData& shadow,
                                    PhysicalBoxSides sides_to_include) {
  if (!sides_to_include.left) {
    float extend_by = std::max(shadow.X(), 0.0f) + shadow.Blur();
    inner_rect.Move(-extend_by, 0);
    inner_rect.SetWidth(inner_rect.Width() + extend_by);
  }
  if (!sides_to_include.top) {
    float extend_by = std::max(shadow.Y(), 0.0f) + shadow.Blur();
    inner_rect.Move(0, -extend_by);
    inner_rect.SetHeight(inner_rect.Height() + extend_by);
  }
  if (!sides_to_include.right) {
    float shrink_by = std::min(shadow.X(), 0.0f) - shadow.Blur();
    inner_rect.SetWidth(inner_rect.Width() - shrink_by);
  }
  if (!sides_to_include.bottom) {
    float shrink_by = std::min(shadow.Y(), 0.0f) - shadow.Blur();
    inner_rect.SetHeight(inner_rect.Height() - shrink_by);
  }
}

}  // namespace

void BoxPainterBase::PaintInsetBoxShadow(const PaintInfo& info,
                                         const FloatRoundedRect& bounds,
                                         const ComputedStyle& style,
                                         PhysicalBoxSides sides_to_include) {
  GraphicsContext& context = info.context;

  const ShadowList* shadow_list = style.BoxShadow();
  for (wtf_size_t i = shadow_list->Shadows().size(); i--;) {
    const ShadowData& shadow = shadow_list->Shadows()[i];
    if (shadow.Style() != ShadowStyle::kInset)
      continue;
    if (!shadow.X() && !shadow.Y() && !shadow.Blur() && !shadow.Spread())
      continue;

    const Color& shadow_color = shadow.GetColor().Resolve(
        style.VisitedDependentColor(GetCSSPropertyColor()),
        style.UsedColorScheme());

    FloatRect inner_rect(bounds.Rect());
    inner_rect.Inflate(-shadow.Spread());
    if (inner_rect.IsEmpty()) {
      context.FillRoundedRect(bounds, shadow_color);
      continue;
    }
    AdjustInnerRectForSideClipping(inner_rect, shadow, sides_to_include);

    FloatRoundedRect inner_rounded_rect(inner_rect, bounds.GetRadii());
    GraphicsContextStateSaver state_saver(context);
    if (bounds.IsRounded()) {
      context.ClipRoundedRect(bounds);
      ApplySpreadToShadowShape(inner_rounded_rect, -shadow.Spread());
    } else {
      context.Clip(bounds.Rect());
    }

    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(ToFloatSize(shadow.Location()), shadow.Blur(),
                                  shadow_color,
                                  DrawLooperBuilder::kShadowRespectsTransforms,
                                  DrawLooperBuilder::kShadowIgnoresAlpha);
    context.SetDrawLooper(draw_looper_builder.DetachDrawLooper());

    Color fill_color(shadow_color.Red(), shadow_color.Green(),
                     shadow_color.Blue());
    FloatRect outer_rect = AreaCastingShadowInHole(bounds.Rect(), shadow);
    context.FillRectWithRoundedHole(outer_rect, inner_rounded_rect, fill_color);
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
    bool is_scroll_container,
    Color bg_color,
    const FillLayer& layer,
    BackgroundBleedAvoidance bleed_avoidance,
    RespectImageOrientationEnum respect_image_orientation,
    PhysicalBoxSides sides_to_include,
    bool is_inline,
    bool is_painting_scrolling_background)
    : image(layer.GetImage()),
      color(bg_color),
      respect_image_orientation(respect_image_orientation),
      sides_to_include(sides_to_include),
      is_bottom_layer(!layer.Next()),
      is_border_fill(layer.Clip() == EFillBox::kBorder),
      is_clipped_with_local_scrolling(is_scroll_container &&
                                      layer.Attachment() ==
                                          EFillAttachment::kLocal) {
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
      style.HasBorderRadius() && !sides_to_include.IsEmpty();
  // BorderFillBox radius clipping is taken care of by
  // BackgroundBleedClip{Only,Layer}
  is_rounded_fill =
      has_rounded_border && !is_painting_scrolling_background &&
      !(is_border_fill && BleedAvoidanceIsClipping(bleed_avoidance));
  is_printing = doc.Printing();

  should_paint_image = image && image->CanRender();
  bool composite_bgcolor_animation =
      RuntimeEnabledFeatures::CompositeBGColorAnimationEnabled() &&
      style.HasCurrentBackgroundColorAnimation();
  // When background color animation is running on the compositor thread, we
  // need to trigger repaint even if the background is transparent to collect
  // artifacts in order to run the animation on the compositor.
  should_paint_color =
      is_bottom_layer && (color.Alpha() || composite_bgcolor_animation) &&
      (!should_paint_image || !layer.ImageOccludesNextLayers(doc, style));
  should_paint_color_with_paint_worklet_image =
      should_paint_color && composite_bgcolor_animation;
}

namespace {

FloatRect SnapSourceRectIfNearIntegral(const FloatRect src_rect) {
  // Round to avoid filtering pulling in neighboring pixels, for the
  // common case of sprite maps, but only if we're close to an integral size.
  // "Close" in this context means we will allow floating point inaccuracy,
  // when converted to layout units, to be at most one LayoutUnit::Epsilon and
  // still snap.
  if (std::abs(std::round(src_rect.X()) - src_rect.X()) <=
          LayoutUnit::Epsilon() &&
      std::abs(std::round(src_rect.Y()) - src_rect.Y()) <=
          LayoutUnit::Epsilon() &&
      std::abs(std::round(src_rect.MaxX()) - src_rect.MaxX()) <=
          LayoutUnit::Epsilon() &&
      std::abs(std::round(src_rect.MaxY()) - src_rect.MaxY()) <=
          LayoutUnit::Epsilon()) {
    IntRect rounded_src_rect = RoundedIntRect(src_rect);
    // If we have snapped the image size to 0, revert the rounding.
    if (rounded_src_rect.IsEmpty())
      return src_rect;
    return FloatRect(rounded_src_rect);
  }
  return src_rect;
}

absl::optional<FloatRect> OptimizeToSingleTileDraw(
    const BackgroundImageGeometry& geometry,
    const PhysicalRect& dest_rect,
    Image* image,
    RespectImageOrientationEnum respect_orientation) {
  const PhysicalOffset dest_phase = geometry.ComputeDestPhase();

  // Phase calculation uses the actual painted location, given by the
  // border-snapped destination rect.
  const PhysicalRect one_tile_rect(dest_phase, geometry.TileSize());

  // We cannot optimize if the tile is misaligned.
  if (!one_tile_rect.Contains(dest_rect))
    return absl::nullopt;

  const PhysicalOffset offset_in_tile =
      geometry.SnappedDestRect().offset - dest_phase;
  if (!image->HasIntrinsicSize()) {
    // This is a generated image sized according to the tile size so we can use
    // the snapped dest rect directly.
    const PhysicalRect offset_tile(offset_in_tile,
                                   geometry.SnappedDestRect().size);
    return FloatRect(offset_tile);
  }

  // Compute the image subset, in intrinsic image coordinates, that gets mapped
  // onto the |dest_rect|, when the whole image would be drawn with phase and
  // size given by |one_tile_rect|. Assumes |one_tile_rect| contains
  // |dest_rect|. The location of the requested subset should be the painting
  // snapped location.
  //
  // The size of requested subset should be the unsnapped size so that the
  // computed scale and location in the source image can be correctly
  // determined.
  //
  // image-resolution information is baked into the given parameters, but we
  // need oriented size.
  const FloatSize intrinsic_tile_size = image->SizeAsFloat(respect_orientation);

  // Subset computation needs the same location as was used above, but needs the
  // unsnapped destination size to correctly calculate sprite subsets in the
  // presence of zoom.
  // TODO(schenney): Re-enable this after determining why it fails for
  // CAP, and maybe other cases.
  // DCHECK(one_tile_rect.Contains(dest_rect_for_subset));
  const FloatSize scale(
      geometry.TileSize().width / intrinsic_tile_size.Width(),
      geometry.TileSize().height / intrinsic_tile_size.Height());
  FloatRect visible_src_rect(
      offset_in_tile.left / scale.Width(), offset_in_tile.top / scale.Height(),
      geometry.UnsnappedDestRect().Width() / scale.Width(),
      geometry.UnsnappedDestRect().Height() / scale.Height());

  // Content providers almost always choose source pixels at integer locations,
  // so snap to integers. This is particularly important for sprite maps.
  // Calculation up to this point, in LayoutUnits, can lead to small variations
  // from integer size, so it is safe to round without introducing major issues.
  visible_src_rect = SnapSourceRectIfNearIntegral(visible_src_rect);

  // When respecting image orientation, the drawing code expects the source
  // rect to be in the unrotated image space, but we have computed it here in
  // the rotated space in order to position and size the background. Undo the
  // src rect rotation if necessary.
  if (respect_orientation && !image->HasDefaultOrientation()) {
    visible_src_rect = image->CorrectSrcRectForImageOrientation(
        intrinsic_tile_size, visible_src_rect);
  }
  return visible_src_rect;
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
                         const BackgroundImageGeometry& geometry,
                         SkBlendMode op,
                         bool has_filter_property,
                         RespectImageOrientationEnum respect_orientation) {
  DCHECK(!geometry.TileSize().IsEmpty());

  // Check and see if a single draw of the image can cover the entire area we
  // are supposed to tile. The dest_rect_for_subset must use the same
  // location that was used in ComputePhaseForBackground and the unsnapped
  // destination rect in order to correctly evaluate the subset size and
  // location in the presence of border snapping and zoom.
  const PhysicalRect dest_rect_for_subset(geometry.SnappedDestRect().offset,
                                          geometry.UnsnappedDestRect().size);
  if (absl::optional<FloatRect> single_tile_src = OptimizeToSingleTileDraw(
          geometry, dest_rect_for_subset, image, respect_orientation)) {
    context.DrawImage(image, Image::kSyncDecode,
                      FloatRect(geometry.SnappedDestRect()), &*single_tile_src,
                      has_filter_property, op, respect_orientation);
    return;
  }

  // At this point we have decided to tile the image to fill the dest rect.

  // Use the intrinsic size of the image if it has one, otherwise force the
  // generated image to be the tile size.
  // image-resolution information is baked into the given parameters, but we
  // need oriented size. That requires explicitly applying orientation here.
  Image::SizeConfig size_config;
  size_config.apply_orientation = respect_orientation;
  const FloatSize intrinsic_tile_size =
      image->SizeWithConfigAsFloat(size_config);

  // Note that this tile rect uses the image's pre-scaled size.
  ImageTilingInfo tiling_info;
  tiling_info.image_rect.SetSize(intrinsic_tile_size);
  tiling_info.phase = FloatPoint(geometry.ComputeDestPhase());
  tiling_info.spacing = FloatSize(geometry.SpaceSize());

  // Farther down the pipeline we will use the scaled tile size to determine
  // which dimensions to clamp or repeat in. We do not want to repeat when the
  // tile size rounds to match the dest in a given dimension, to avoid having
  // a single row or column repeated when the developer almost certainly
  // intended the image to not repeat (this generally occurs under zoom).
  //
  // So detect when we do not want to repeat and set the scale to round the
  // values in that dimension.
  const PhysicalSize tile_dest_diff =
      geometry.TileSize() - geometry.SnappedDestRect().size;
  const LayoutUnit ref_tile_width = tile_dest_diff.width.Abs() <= 0.5f
                                        ? geometry.SnappedDestRect().Width()
                                        : geometry.TileSize().width;
  const LayoutUnit ref_tile_height = tile_dest_diff.height.Abs() <= 0.5f
                                         ? geometry.SnappedDestRect().Height()
                                         : geometry.TileSize().height;
  tiling_info.scale = {ref_tile_width / tiling_info.image_rect.Width(),
                       ref_tile_height / tiling_info.image_rect.Height()};

  // This call takes the unscaled image, applies the given scale, and paints
  // it into the snapped_dest_rect using phase from one_tile_rect and the
  // given repeat spacing. Note the phase is already scaled.
  context.DrawImageTiled(image, FloatRect(geometry.SnappedDestRect()),
                         tiling_info, has_filter_property, op,
                         respect_orientation);
}

scoped_refptr<Image> GetBGColorPaintWorkletImage(const Document* document,
                                                 Node* node,
                                                 const FloatSize& image_size) {
  LocalFrame* frame = document->GetFrame();
  if (!frame)
    return nullptr;
  BackgroundColorPaintImageGenerator* generator =
      frame->GetBackgroundColorPaintImageGenerator();
  // The generator can be null in testing environment.
  if (!generator)
    return nullptr;
  Vector<Color> animated_colors;
  Vector<double> offsets;
  absl::optional<double> progress;
  if (!generator->GetBGColorPaintWorkletParams(node, &animated_colors, &offsets,
                                               &progress)) {
    return nullptr;
  }
  return generator->Paint(image_size, node, animated_colors, offsets, progress);
}

// Returns true if the background color was painted by the paint worklet.
bool PaintBGColorWithPaintWorklet(const Document* document,
                                  const BoxPainterBase::FillLayerInfo& info,
                                  Node* node,
                                  const FloatRoundedRect& dest_rect,
                                  GraphicsContext& context) {
  if (!info.should_paint_color_with_paint_worklet_image)
    return false;
  scoped_refptr<Image> paint_worklet_image =
      GetBGColorPaintWorkletImage(document, node, dest_rect.Rect().Size());
  if (!paint_worklet_image)
    return false;
  FloatRect src_rect(FloatPoint(), dest_rect.Rect().Size());
  context.DrawImageRRect(paint_worklet_image.get(), Image::kSyncDecode,
                         dest_rect, src_rect,
                         node && node->ComputedStyleRef().DisableForceDark());
  return true;
}

void DidDrawImage(
    Node* node,
    const Image& image,
    const StyleImage& style_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const FloatRect& image_rect) {
  if (!node || !style_image.IsImageResource())
    return;
  const IntRect enclosing_rect = EnclosingIntRect(image_rect);
  PaintTimingDetector::NotifyBackgroundImagePaint(
      *node, image, To<StyleFetchedImage>(style_image),
      current_paint_chunk_properties, enclosing_rect);

  LocalDOMWindow* window = node->GetDocument().domWindow();
  DCHECK(window);
  ImageElementTiming::From(*window).NotifyBackgroundImagePainted(
      *node, To<StyleFetchedImage>(style_image), current_paint_chunk_properties,
      enclosing_rect);
}

inline bool PaintFastBottomLayer(const Document* document,
                                 Node* node,
                                 GraphicsContext& context,
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
  FloatRoundedRect color_border =
      info.is_rounded_fill ? border_rect
                           : FloatRoundedRect(PixelSnappedIntRect(rect));
  // When the layer has an image, figure out whether it is covered by a single
  // tile. The border for painting images may not be the same as the color due
  // to optimizations for the image painting destination that avoid painting
  // under the border.
  FloatRect src_rect;
  FloatRoundedRect image_border;
  if (info.should_paint_image && image) {
    // Avoid image shaders when printing (poorly supported in PDF).
    if (info.is_rounded_fill && info.is_printing)
      return false;

    // Compute the dest rect we will be using for images.
    image_border =
        info.is_rounded_fill
            ? color_border
            : FloatRoundedRect(FloatRect(geometry.SnappedDestRect()));

    const FloatRect& image_rect = image_border.Rect();
    if (!image_rect.IsEmpty()) {
      // We cannot optimize if the tile is too small.
      if (geometry.TileSize().width < image_rect.Width() ||
          geometry.TileSize().height < image_rect.Height())
        return false;

      // Use FastAndLossyFromFloatRect when converting the image border rect.
      // At this point it should have been derived from a snapped rectangle, so
      // the conversion from float should be as precise as it can be.
      const PhysicalRect dest_rect =
          PhysicalRect::FastAndLossyFromFloatRect(image_rect);

      absl::optional<FloatRect> single_tile_src = OptimizeToSingleTileDraw(
          geometry, dest_rect, image, info.respect_image_orientation);
      if (!single_tile_src)
        return false;
      src_rect = *single_tile_src;
    }
  }

  // At this point we're committed to the fast path: the destination (r)rect
  // fits within a single tile, and we can paint it using direct draw(R)Rect()
  // calls. Furthermore, if an image should be painted, |src_rect| has been
  // updated to account for positioning and size parameters by
  // OptimizeToSingleTileDraw() in the above code block.
  absl::optional<RoundedInnerRectClipper> clipper;
  if (info.is_rounded_fill && !color_border.IsRenderable()) {
    // When the rrect is not renderable, we resort to clipping.
    // RoundedInnerRectClipper handles this case via discrete, corner-wise
    // clipping.
    clipper.emplace(context, rect, color_border);
    color_border.SetRadii(FloatRoundedRect::Radii());
    image_border.SetRadii(FloatRoundedRect::Radii());
  }

  // Paint the color if needed.
  if (info.should_paint_color) {
    // Try to paint the background with a paint worklet first in case it will be
    // animated. Otherwise, paint it directly into the context.
    if (!PaintBGColorWithPaintWorklet(document, info, node, color_border,
                                      context)) {
      context.FillRoundedRect(color_border, info.color);
    }
  }

  // Paint the image if needed.
  if (!info.should_paint_image || src_rect.IsEmpty())
    return true;

  DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
      inspector_paint_image_event::Data, node, *info.image,
      FloatRect(image->Rect()), FloatRect(image_border.Rect()));

  // Since there is no way for the developer to specify decode behavior, use
  // kSync by default.
  context.DrawImageRRect(image, Image::kSyncDecode, image_border, src_rect,
                         node && node->ComputedStyleRef().DisableForceDark(),
                         composite_op, info.respect_image_orientation);

  DidDrawImage(node, *image, *info.image,
               context.GetPaintController().CurrentPaintChunkProperties(),
               image_border.Rect());
  return true;
}

// Inset the background rect by a "safe" amount: 1/2 border-width for opaque
// border styles, 1/6 border-width for double borders.
FloatRoundedRect BackgroundRoundedRectAdjustedForBleedAvoidance(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    bool object_has_multiple_boxes,
    PhysicalBoxSides sides_to_include,
    FloatRoundedRect background_rounded_rect) {
  // TODO(fmalita): we should be able to fold these parameters into
  // BoxBorderInfo or BoxDecorationData and avoid calling getBorderEdgeInfo
  // redundantly here.
  BorderEdge edges[4];
  style.GetBorderEdgeInfo(edges, sides_to_include);

  // Use the most conservative inset to avoid mixed-style corner issues.
  float fractional_inset = 1.0f / 2;
  for (auto& edge : edges) {
    if (edge.BorderStyle() == EBorderStyle::kDouble) {
      fractional_inset = 1.0f / 6;
      break;
    }
  }

  FloatRectOutsets insets(
      -fractional_inset *
          edges[static_cast<unsigned>(BoxSide::kTop)].UsedWidth(),
      -fractional_inset *
          edges[static_cast<unsigned>(BoxSide::kRight)].UsedWidth(),
      -fractional_inset *
          edges[static_cast<unsigned>(BoxSide::kBottom)].UsedWidth(),
      -fractional_inset *
          edges[static_cast<unsigned>(BoxSide::kLeft)].UsedWidth());

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

  FloatRoundedRect border = RoundedBorderGeometry::PixelSnappedRoundedBorder(
      style, rect, info.sides_to_include);
  if (object_has_multiple_boxes) {
    FloatRoundedRect segment_border =
        RoundedBorderGeometry::PixelSnappedRoundedBorder(
            style,
            PhysicalRect(PhysicalOffset(),
                         PhysicalSize(FlooredIntSize(flow_box_size))),
            info.sides_to_include);
    border.SetRadii(segment_border.GetRadii());
  }

  if (info.is_border_fill &&
      bleed_avoidance == kBackgroundBleedShrinkBackground) {
    border = BackgroundRoundedRectAdjustedForBleedAvoidance(
        style, rect, object_has_multiple_boxes, info.sides_to_include, border);
  }

  // Clip to the padding or content boxes as necessary.
  // Use FastAndLossyFromFloatRect because we know it has been pixel snapped.
  PhysicalRect border_rect =
      PhysicalRect::FastAndLossyFromFloatRect(border.Rect());
  if (bg_layer.Clip() == EFillBox::kContent) {
    border = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
        style, border_rect, border_padding_insets, info.sides_to_include);
  } else if (bg_layer.Clip() == EFillBox::kPadding) {
    border = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
        style, border_rect, info.sides_to_include);
  }
  return border;
}

void PaintFillLayerBackground(const Document* document,
                              GraphicsContext& context,
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
  if (info.should_paint_color) {
    IntRect background_rect(PixelSnappedIntRect(scrolled_paint_rect));
    // Try to paint the background with a paint worklet first in case it will be
    // animated. Otherwise, paint it directly into the context.
    if (!PaintBGColorWithPaintWorklet(
            document, info, node, FloatRoundedRect(background_rect), context)) {
      context.FillRect(background_rect, info.color);
    }
  }

  // No progressive loading of the background image.
  // NOTE: This method can be called with no image in situations when a bad
  // resource locator is given such as "//:0", so still check for image.
  if (info.should_paint_image && !geometry.SnappedDestRect().IsEmpty() &&
      !geometry.TileSize().IsEmpty() && image) {
    DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
        inspector_paint_image_event::Data, node, *info.image,
        FloatRect(image->Rect()), FloatRect(scrolled_paint_rect));
    DrawTiledBackground(context, image, geometry, composite_op,
                        node && node->ComputedStyleRef().DisableForceDark(),
                        info.respect_image_orientation);
    DidDrawImage(node, *image, *info.image,
                 context.GetPaintController().CurrentPaintChunkProperties(),
                 FloatRect(geometry.SnappedDestRect()));
  }
}

LayoutRectOutsets AdjustOutsetsForEdgeInclusion(
    const LayoutRectOutsets outsets,
    const BoxPainterBase::FillLayerInfo& info) {
  LayoutRectOutsets adjusted = outsets;
  if (!info.sides_to_include.top)
    adjusted.SetTop(LayoutUnit());
  if (!info.sides_to_include.right)
    adjusted.SetRight(LayoutUnit());
  if (!info.sides_to_include.bottom)
    adjusted.SetBottom(LayoutUnit());
  if (!info.sides_to_include.left)
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
  if (rect.IsEmpty())
    return;

  const FillLayerInfo info =
      GetFillLayerInfo(color, bg_layer, bleed_avoidance,
                       IsPaintingScrollingBackground(paint_info));
  // If we're not actually going to paint anything, abort early.
  if (!info.should_paint_image && !info.should_paint_color)
    return;

  GraphicsContext& context = paint_info.context;
  GraphicsContextStateSaver clip_with_scrolling_state_saver(
      context, info.is_clipped_with_local_scrolling);
  auto scrolled_paint_rect =
      AdjustRectForScrolledContent(paint_info, info, rect);
  const auto did_adjust_paint_rect = scrolled_paint_rect != rect;

  scoped_refptr<Image> image;
  SkBlendMode composite_op = SkBlendMode::kSrcOver;
  absl::optional<ScopedInterpolationQuality> interpolation_quality_context;
  if (info.should_paint_image) {
    geometry.Calculate(paint_info.PaintContainer(), paint_info.phase, bg_layer,
                       scrolled_paint_rect);
    image = info.image->GetImage(
        geometry.ImageClient(), geometry.ImageDocument(),
        geometry.ImageStyle(style_), FloatSize(geometry.TileSize()));
    interpolation_quality_context.emplace(context,
                                          geometry.ImageInterpolationQuality());

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
      PaintFastBottomLayer(document_, node_, context, info, rect, border_rect,
                           geometry, image.get(), composite_op)) {
    return;
  }

  absl::optional<RoundedInnerRectClipper> clip_to_border;
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
      context.Clip(PixelSnappedIntRect(clip_rect));
      break;
    }
    case EFillBox::kBorder:
      break;
    case EFillBox::kText:  // fall through
    default:
      NOTREACHED();
      break;
  }

  PaintFillLayerBackground(document_, context, info, node_, image.get(),
                           composite_op, geometry, scrolled_paint_rect);
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

  PaintFillLayerBackground(document_, context, info, node_, image,
                           SkBlendMode::kSrcOver, geometry,
                           scrolled_paint_rect);

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
                                 PhysicalBoxSides sides_to_include) {
  // border-image is not affected by border-radius.
  if (NinePieceImagePainter::Paint(info.context, obj, document, node, rect,
                                   style, style.BorderImage())) {
    return;
  }

  BoxBorderPainter::PaintBorder(info.context, rect, style, bleed_avoidance,
                                sides_to_include);
}

void BoxPainterBase::PaintMaskImages(const PaintInfo& paint_info,
                                     const PhysicalRect& paint_rect,
                                     const ImageResourceObserver& obj,
                                     BackgroundImageGeometry& geometry,
                                     PhysicalBoxSides sides_to_include) {
  if (!style_.HasMask() || style_.Visibility() != EVisibility::kVisible)
    return;

  PaintFillLayers(paint_info, Color::kTransparent, style_.MaskLayers(),
                  paint_rect, geometry);
  NinePieceImagePainter::Paint(paint_info.context, obj, *document_, node_,
                               paint_rect, style_, style_.MaskBoxImage(),
                               sides_to_include);
}

bool BoxPainterBase::ShouldSkipPaintUnderInvalidationChecking(
    const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled());

  // Disable paint under-invalidation checking for cases that under-invalidation
  // is intensional and/or harmless.

  // A box having delayed-invalidation may change before it's actually
  // invalidated. Note that we still report harmless under-invalidation of
  // non-delayed-invalidation animated background, which should be ignored.
  if (box.ShouldDelayFullPaintInvalidation())
    return true;

  // We always paint a MediaSliderPart using the latest data (buffered ranges,
  // current time and duration) which may be different from the cached data.
  if (box.StyleRef().EffectiveAppearance() == kMediaSliderPart)
    return true;

  // We paint an indeterminate progress based on the position calculated from
  // the animation progress. Harmless under-invalidatoin may happen during a
  // paint that is not scheduled for animation.
  if (box.IsProgress() && !To<LayoutProgress>(box).IsDeterminate())
    return true;

  return false;
}

}  // namespace blink
