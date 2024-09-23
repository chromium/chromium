// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_mask_painter.h"

#include "base/containers/adapters.h"
#include "cc/paint/color_filter.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/background_image_geometry.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/svg_background_paint_context.h"
#include "third_party/blink/renderer/core/style/style_mask_source_image.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/graphics/scoped_image_rendering_settings.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

AffineTransform MaskToContentTransform(const LayoutSVGResourceMasker& masker,
                                       const gfx::RectF& reference_box,
                                       float zoom) {
  AffineTransform content_transformation;
  if (masker.MaskContentUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    content_transformation.Translate(reference_box.x(), reference_box.y());
    content_transformation.ScaleNonUniform(reference_box.width(),
                                           reference_box.height());
  } else if (zoom != 1) {
    content_transformation.Scale(zoom);
  }
  return content_transformation;
}

LayoutSVGResourceMasker* ResolveElementReference(
    const StyleMaskSourceImage& mask_source,
    const ImageResourceObserver& observer) {
  SVGResource* mask_resource = mask_source.GetSVGResource();
  SVGResourceClient* client = mask_source.GetSVGResourceClient(observer);
  // The client should only be null if the resource is null.
  if (!client) {
    CHECK(!mask_resource);
    return nullptr;
  }
  auto* masker =
      GetSVGResourceAsType<LayoutSVGResourceMasker>(*client, mask_resource);
  if (!masker) {
    return nullptr;
  }
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*masker)) {
    return nullptr;
  }
  SECURITY_CHECK(!masker->SelfNeedsFullLayout());
  masker->ClearInvalidationMask();
  return masker;
}

class ScopedMaskLuminanceLayer {
  STACK_ALLOCATED();

 public:
  ScopedMaskLuminanceLayer(GraphicsContext& context, SkBlendMode composite_op)
      : context_(context) {
    context.BeginLayer(cc::ColorFilter::MakeLuma(), &composite_op);
  }
  ~ScopedMaskLuminanceLayer() { context_.EndLayer(); }

 private:
  GraphicsContext& context_;
};

const StyleMaskSourceImage* ToMaskSourceIfSVGMask(
    const StyleImage& style_image) {
  const auto* mask_source = DynamicTo<StyleMaskSourceImage>(style_image);
  if (!mask_source || !mask_source->HasSVGMask()) {
    return nullptr;
  }
  return mask_source;
}

void PaintMaskLayer(const FillLayer& layer,
                    const LayoutObject& object,
                    const SVGBackgroundPaintContext& bg_paint_context,
                    GraphicsContext& context) {
  const StyleImage* style_image = layer.GetImage();
  if (!style_image) {
    return;
  }

  std::optional<ScopedMaskLuminanceLayer> mask_luminance_scope;
  SkBlendMode composite_op = SkBlendMode::kSrcOver;
  // Don't use the operator if this is the bottom layer.
  if (layer.Next()) {
    composite_op = WebCoreCompositeToSkiaComposite(layer.Composite(),
                                                   layer.GetBlendMode());
  }

  if (layer.MaskMode() == EFillMaskMode::kLuminance) {
    mask_luminance_scope.emplace(context, composite_op);
    composite_op = SkBlendMode::kSrcOver;
  }

  const ComputedStyle& style = bg_paint_context.Style();
  const ImageResourceObserver& observer = object;
  const bool uses_zoomed_coordinates = object.IsSVGForeignObject();
  GraphicsContextStateSaver saver(context, false);

  // If the "image" referenced by the FillLayer is an SVG <mask> reference (and
  // this is a layer for a mask), then repeat, position, clip, origin and size
  // should have no effect.
  if (const auto* mask_source = ToMaskSourceIfSVGMask(*style_image)) {
    const float zoom = uses_zoomed_coordinates ? style.EffectiveZoom() : 1;
    gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
        object, GeometryBox::kFillBox,
        SVGResources::ForeignObjectQuirk::kDisabled);
    reference_box.Scale(zoom);

    saver.Save();
    SVGMaskPainter::PaintSVGMaskLayer(
        context, *mask_source, observer, reference_box, zoom, composite_op,
        layer.MaskMode() == EFillMaskMode::kMatchSource);
    return;
  }

  BackgroundImageGeometry geometry;
  geometry.Calculate(layer, bg_paint_context);

  if (geometry.TileSize().IsEmpty()) {
    return;
  }

  const Document& document = object.GetDocument();
  scoped_refptr<Image> image = style_image->GetImage(
      observer, document, style, gfx::SizeF(geometry.TileSize()));
  if (!image) {
    return;
  }

  ScopedImageRenderingSettings image_rendering_settings_context(
      context, style.GetInterpolationQuality(), style.GetDynamicRangeLimit());

  // Adjust the coordinate space to consider zoom - which is applied to the
  // computed image geometry.
  if (!uses_zoomed_coordinates && style.EffectiveZoom() != 1) {
    const float inv_zoom = 1 / style.EffectiveZoom();
    saver.Save();
    context.Scale(inv_zoom, inv_zoom);
  }

  std::optional<GeometryBox> clip_box;
  switch (layer.Clip()) {
    case EFillBox::kText:
    case EFillBox::kNoClip:
      break;
    case EFillBox::kContent:
    case EFillBox::kFillBox:
    case EFillBox::kPadding:
      clip_box.emplace(GeometryBox::kFillBox);
      break;
    case EFillBox::kStrokeBox:
    case EFillBox::kBorder:
      clip_box.emplace(GeometryBox::kStrokeBox);
      break;
    case EFillBox::kViewBox:
      clip_box.emplace(GeometryBox::kViewBox);
      break;
  }
  if (clip_box) {
    gfx::RectF clip_rect = SVGResources::ReferenceBoxForEffects(
        object, *clip_box, SVGResources::ForeignObjectQuirk::kDisabled);
    clip_rect.Scale(style.EffectiveZoom());

    saver.SaveIfNeeded();
    context.Clip(clip_rect);
  }

  const RespectImageOrientationEnum respect_orientation =
      style_image->ForceOrientationIfNecessary(style.ImageOrientation());

  // Use the intrinsic size of the image if it has one, otherwise force the
  // generated image to be the tile size.
  // image-resolution information is baked into the given parameters, but we
  // need oriented size. That requires explicitly applying orientation here.
  Image::SizeConfig size_config;
  size_config.apply_orientation = respect_orientation;
  const gfx::SizeF intrinsic_tile_size =
      image->SizeWithConfigAsFloat(size_config);

  const gfx::RectF dest_rect(geometry.UnsnappedDestRect());

  // Note that this tile rect uses the image's pre-scaled size.
  ImageTilingInfo tiling_info;
  tiling_info.image_rect.set_size(intrinsic_tile_size);
  tiling_info.phase =
      dest_rect.origin() + gfx::Vector2dF(geometry.ComputePhase());
  tiling_info.spacing = gfx::SizeF(geometry.SpaceSize());
  tiling_info.scale = {
      geometry.TileSize().width / tiling_info.image_rect.width(),
      geometry.TileSize().height / tiling_info.image_rect.height()};

  auto image_auto_dark_mode = ImageClassifierHelper::GetImageAutoDarkMode(
      *document.GetFrame(), style, dest_rect, tiling_info.image_rect);
  // This call takes the unscaled image, applies the given scale, and paints it
  // into the dest rect using phase and the given repeat spacing. Note the
  // phase is already scaled.
  const ImagePaintTimingInfo paint_timing_info(false, false);
  context.DrawImageTiled(*image, dest_rect, tiling_info, image_auto_dark_mode,
                         paint_timing_info, composite_op, respect_orientation);
}

}  // namespace

void SVGMaskPainter::Paint(GraphicsContext& context,
                           const LayoutObject& layout_object,
                           const DisplayItemClient& display_item_client) {
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  DCHECK(properties);
  DCHECK(properties->Mask());
  DCHECK(properties->MaskClip());
  PropertyTreeStateOrAlias property_tree_state(
      properties->Mask()->LocalTransformSpace(), *properties->MaskClip(),
      *properties->Mask());
  ScopedPaintChunkProperties scoped_paint_chunk_properties(
      context.GetPaintController(), property_tree_state, display_item_client,
      DisplayItem::kSVGMask);

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGMask))
    return;

  // TODO(fs): Should clip this with the bounds of the mask's PaintRecord.
  gfx::RectF visual_rect = properties->MaskClip()->PaintClipRect().Rect();
  DrawingRecorder recorder(context, display_item_client, DisplayItem::kSVGMask,
                           gfx::ToEnclosingRect(visual_rect));

  Vector<const FillLayer*, 8> layer_list;
  for (const FillLayer* layer = &layout_object.StyleRef().MaskLayers(); layer;
       layer = layer->Next()) {
    layer_list.push_back(layer);
  }
  const SVGBackgroundPaintContext bg_paint_context(layout_object);
  for (const auto* layer : base::Reversed(layer_list)) {
    PaintMaskLayer(*layer, layout_object, bg_paint_context, context);
  }
}

void SVGMaskPainter::PaintSVGMaskLayer(GraphicsContext& context,
                                       const StyleMaskSourceImage& mask_source,
                                       const ImageResourceObserver& observer,
                                       const gfx::RectF& reference_box,
                                       const float zoom,
                                       const SkBlendMode composite_op,
                                       const bool apply_mask_type) {
  LayoutSVGResourceMasker* masker =
      ResolveElementReference(mask_source, observer);
  if (!masker) {
    return;
  }
  const AffineTransform content_transformation =
      MaskToContentTransform(*masker, reference_box, zoom);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();

  context.Clip(masker->ResourceBoundingBox(reference_box, zoom));

  bool has_layer = false;
  if (apply_mask_type &&
      masker->StyleRef().MaskType() == EMaskType::kLuminance) {
    context.BeginLayer(cc::ColorFilter::MakeLuma(), &composite_op);
    has_layer = true;
  } else if (composite_op != SkBlendMode::kSrcOver) {
    context.BeginLayer(composite_op);
    has_layer = true;
  }
  context.ConcatCTM(content_transformation);
  context.DrawRecord(std::move(record));
  if (has_layer) {
    context.EndLayer();
  }
}

bool SVGMaskPainter::MaskIsValid(const StyleMaskSourceImage& mask_source,
                                 const ImageResourceObserver& observer) {
  return ResolveElementReference(mask_source, observer);
}

gfx::RectF SVGMaskPainter::ResourceBoundsForSVGChild(
    const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();
  const gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
      object, GeometryBox::kFillBox,
      SVGResources::ForeignObjectQuirk::kDisabled);
  const float reference_box_zoom =
      object.IsSVGForeignObject() ? style.EffectiveZoom() : 1;
  gfx::RectF bounds;
  for (const FillLayer* layer = &style.MaskLayers(); layer;
       layer = layer->Next()) {
    const auto* mask_source =
        DynamicTo<StyleMaskSourceImage>(layer->GetImage());
    if (!mask_source) {
      continue;
    }
    LayoutSVGResourceMasker* masker =
        ResolveElementReference(*mask_source, object);
    if (!masker) {
      continue;
    }
    const gfx::RectF svg_mask_bounds =
        masker->ResourceBoundingBox(reference_box, reference_box_zoom);
    bounds.Union(svg_mask_bounds);
  }
  return gfx::UnionRects(bounds, object.VisualRectInLocalSVGCoordinates());
}

}  // namespace blink
