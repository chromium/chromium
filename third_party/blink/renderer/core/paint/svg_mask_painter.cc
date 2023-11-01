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
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/style/style_svg_mask_reference_image.h"
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

LayoutSVGResourceMasker* ResolveElementReference(SVGResource* mask_resource,
                                                 SVGResourceClient& client) {
  auto* masker =
      GetSVGResourceAsType<LayoutSVGResourceMasker>(client, mask_resource);
  if (!masker) {
    return nullptr;
  }
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*masker)) {
    return nullptr;
  }
  SECURITY_DCHECK(!masker->SelfNeedsFullLayout());
  masker->ClearInvalidationMask();
  return masker;
}

class SVGMaskGeometry {
  STACK_ALLOCATED();

 public:
  explicit SVGMaskGeometry(const LayoutObject& object) : object_(object) {}

  void Calculate(const FillLayer&);

  const gfx::RectF& DestRect() const { return dest_rect_; }
  const absl::optional<gfx::RectF>& ClipRect() const { return clip_rect_; }
  const gfx::SizeF& TileSize() const { return tile_size_; }
  const gfx::SizeF& Spacing() const { return spacing_; }

  gfx::Vector2dF ComputePhase() const;

 private:
  absl::optional<gfx::RectF> ComputePaintingArea(const FillLayer&) const;
  gfx::RectF ComputePositioningArea(const FillLayer&) const;
  gfx::SizeF ComputeTileSize(const FillLayer&,
                             const gfx::RectF& positioning_area) const;

  const LayoutObject& object_;

  gfx::RectF dest_rect_;
  absl::optional<gfx::RectF> clip_rect_;
  gfx::SizeF tile_size_;
  gfx::PointF phase_;
  gfx::SizeF spacing_;
};

absl::optional<gfx::RectF> SVGMaskGeometry::ComputePaintingArea(
    const FillLayer& layer) const {
  GeometryBox geometry_box = GeometryBox::kFillBox;
  switch (layer.Clip()) {
    case EFillBox::kText:
    case EFillBox::kNoClip:
      return absl::nullopt;
    case EFillBox::kContent:
    case EFillBox::kFillBox:
    case EFillBox::kPadding:
      break;
    case EFillBox::kStrokeBox:
    case EFillBox::kBorder:
      geometry_box = GeometryBox::kStrokeBox;
      break;
    case EFillBox::kViewBox:
      geometry_box = GeometryBox::kViewBox;
      break;
  }
  gfx::RectF painting_area = SVGResources::ReferenceBoxForEffects(
      object_, geometry_box, SVGResources::ForeignObjectQuirk::kDisabled);
  painting_area.Scale(object_.StyleRef().EffectiveZoom());
  return painting_area;
}

gfx::RectF SVGMaskGeometry::ComputePositioningArea(
    const FillLayer& layer) const {
  GeometryBox geometry_box = GeometryBox::kFillBox;
  switch (layer.Origin()) {
    case EFillBox::kBorder:
    case EFillBox::kContent:
    case EFillBox::kFillBox:
    case EFillBox::kPadding:
      break;
    case EFillBox::kStrokeBox:
      geometry_box = GeometryBox::kStrokeBox;
      break;
    case EFillBox::kViewBox:
      geometry_box = GeometryBox::kViewBox;
      break;
    case EFillBox::kNoClip:
    case EFillBox::kText:
      NOTREACHED();
      break;
  }
  gfx::RectF positioning_area = SVGResources::ReferenceBoxForEffects(
      object_, geometry_box, SVGResources::ForeignObjectQuirk::kDisabled);
  positioning_area.Scale(object_.StyleRef().EffectiveZoom());
  return positioning_area;
}

gfx::Vector2dF SVGMaskGeometry::ComputePhase() const {
  // TODO(fs): Support phase computations.
  return gfx::Vector2dF();
}

float ResolveWidthForRatio(float height, const gfx::SizeF& natural_ratio) {
  return height * natural_ratio.width() / natural_ratio.height();
}

float ResolveHeightForRatio(float width, const gfx::SizeF& natural_ratio) {
  return width * natural_ratio.height() / natural_ratio.width();
}

gfx::SizeF FitToAspectRatio(const gfx::RectF& rect,
                            const gfx::SizeF& aspect_ratio,
                            bool grow) {
  const float constrained_height =
      ResolveHeightForRatio(rect.width(), aspect_ratio);
  if ((grow && constrained_height < rect.height()) ||
      (!grow && constrained_height > rect.height())) {
    const float constrained_width =
        ResolveWidthForRatio(rect.height(), aspect_ratio);
    return {constrained_width, rect.height()};
  }
  return {rect.width(), constrained_height};
}

gfx::SizeF SVGMaskGeometry::ComputeTileSize(
    const FillLayer& layer,
    const gfx::RectF& positioning_area) const {
  const StyleImage* image = layer.GetImage();
  const IntrinsicSizingInfo sizing_info = image->GetNaturalSizingInfo(
      object_.StyleRef().EffectiveZoom(),
      LayoutObject::ShouldRespectImageOrientation(&object_));

  switch (layer.SizeType()) {
    case EFillSizeType::kSizeLength: {
      const Length& layer_width = layer.SizeLength().Width();
      const Length& layer_height = layer.SizeLength().Height();
      gfx::SizeF tile_size(
          FloatValueForLength(layer_width, positioning_area.width()),
          FloatValueForLength(layer_height, positioning_area.height()));

      // An auto value for one dimension is resolved by using the image's
      // natural aspect ratio and the size of the other dimension, or failing
      // that, using the image's natural size, or failing that, treating it as
      // 100%.
      // If both values are auto then the natural width and/or height of the
      // image should be used, if any, the missing dimension (if any)
      // behaving as auto as described above. If the image has neither
      // natural size, its size is determined as for contain.
      if (layer_width.IsAuto() && !layer_height.IsAuto()) {
        if (!sizing_info.aspect_ratio.IsEmpty()) {
          tile_size.set_width(ResolveWidthForRatio(tile_size.height(),
                                                   sizing_info.aspect_ratio));
        } else if (sizing_info.has_width) {
          tile_size.set_width(sizing_info.size.width());
        }
      } else if (!layer_width.IsAuto() && layer_height.IsAuto()) {
        if (!sizing_info.aspect_ratio.IsEmpty()) {
          tile_size.set_height(ResolveHeightForRatio(tile_size.width(),
                                                     sizing_info.aspect_ratio));
        } else if (sizing_info.has_height) {
          tile_size.set_height(sizing_info.size.height());
        }
      } else if (layer_width.IsAuto() && layer_height.IsAuto()) {
        tile_size = image->ImageSize(
            object_.StyleRef().EffectiveZoom(), positioning_area.size(),
            LayoutObject::ShouldRespectImageOrientation(&object_));
      }
      return tile_size;
    }
    case EFillSizeType::kContain:
    case EFillSizeType::kCover: {
      if (sizing_info.aspect_ratio.IsEmpty()) {
        return positioning_area.size();
      }
      return FitToAspectRatio(positioning_area, sizing_info.aspect_ratio,
                              layer.SizeType() == EFillSizeType::kCover);
    }
    case EFillSizeType::kSizeNone:
      // This value should only be used while resolving style.
      NOTREACHED();
      return gfx::SizeF();
  }
}

void SVGMaskGeometry::Calculate(const FillLayer& layer) {
  clip_rect_ = ComputePaintingArea(layer);
  const gfx::RectF positioning_area = ComputePositioningArea(layer);
  dest_rect_ = positioning_area;
  tile_size_ = ComputeTileSize(layer, positioning_area);

  if (!object_.IsSVGForeignObject()) {
    if (clip_rect_) {
      clip_rect_->InvScale(object_.StyleRef().EffectiveZoom());
    }
    dest_rect_.InvScale(object_.StyleRef().EffectiveZoom());
    tile_size_.InvScale(object_.StyleRef().EffectiveZoom());
  }

  const FillRepeat& repeat = layer.Repeat();
  if (repeat.x == EFillRepeat::kNoRepeatFill) {
    dest_rect_.set_width(tile_size_.width());
  }
  if (repeat.y == EFillRepeat::kNoRepeatFill) {
    dest_rect_.set_height(tile_size_.height());
  }
}

void PaintSVGMask(LayoutSVGResourceMasker* masker,
                  const LayoutObject& layout_object,
                  GraphicsContext& context) {
  const ComputedStyle& style = layout_object.StyleRef();
  const gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
      layout_object, GeometryBox::kFillBox,
      SVGResources::ForeignObjectQuirk::kDisabled);
  const AffineTransform content_transformation = MaskToContentTransform(
      *masker, reference_box,
      layout_object.IsSVGForeignObject() ? style.EffectiveZoom() : 1);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();

  context.Save();
  context.ConcatCTM(content_transformation);
  bool needs_luminance_layer =
      masker->StyleRef().MaskType() == EMaskType::kLuminance;
  if (needs_luminance_layer) {
    context.BeginLayer(cc::ColorFilter::MakeLuma());
  }
  context.DrawRecord(std::move(record));
  if (needs_luminance_layer) {
    context.EndLayer();
  }
  context.Restore();
}

struct FillInfo {
  STACK_ALLOCATED();

 public:
  const InterpolationQuality interpolation_quality;
  const cc::PaintFlags::DynamicRangeLimit dynamic_range_limit;
  const RespectImageOrientationEnum respect_orientation;
  const LayoutObject& object;
};

void PaintMaskLayer(const FillLayer& layer,
                    const FillInfo& info,
                    SVGMaskGeometry& geometry,
                    GraphicsContext& context) {
  const StyleImage* style_image = layer.GetImage();
  if (!style_image) {
    return;
  }
  // If the "image" referenced by the FillLayer is an SVG <mask> reference (and
  // this is a layer for a mask), then repeat, position, clip, origin and size
  // should have no effect.
  if (const auto* svg_reference =
          DynamicTo<StyleSVGMaskReferenceImage>(*style_image)) {
    LayoutSVGResourceMasker* masker = ResolveElementReference(
        svg_reference->GetSVGResource(), svg_reference->GetSVGResourceClient());
    if (!masker) {
      return;
    }
    const gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
        info.object, GeometryBox::kFillBox,
        SVGResources::ForeignObjectQuirk::kDisabled);
    context.Save();
    context.Clip(masker->ResourceBoundingBox(
        reference_box, info.object.IsSVGForeignObject()
                           ? info.object.StyleRef().EffectiveZoom()
                           : 1));
    PaintSVGMask(masker, info.object, context);
    context.Restore();
    return;
  }
  geometry.Calculate(layer);

  scoped_refptr<Image> image =
      style_image->GetImage(info.object, info.object.GetDocument(),
                            info.object.StyleRef(), geometry.TileSize());
  if (!image) {
    return;
  }

  ScopedImageRenderingSettings image_rendering_settings_context(
      context, info.interpolation_quality, info.dynamic_range_limit);

  GraphicsContextStateSaver saver(context, false);
  if (auto clip_rect = geometry.ClipRect()) {
    saver.Save();
    context.Clip(*clip_rect);
  }

  const RespectImageOrientationEnum respect_orientation =
      style_image->ForceOrientationIfNecessary(info.respect_orientation);

  // Use the intrinsic size of the image if it has one, otherwise force the
  // generated image to be the tile size.
  // image-resolution information is baked into the given parameters, but we
  // need oriented size. That requires explicitly applying orientation here.
  Image::SizeConfig size_config;
  size_config.apply_orientation = respect_orientation;
  const gfx::SizeF intrinsic_tile_size =
      image->SizeWithConfigAsFloat(size_config);

  // Note that this tile rect uses the image's pre-scaled size.
  ImageTilingInfo tiling_info;
  tiling_info.image_rect.set_size(intrinsic_tile_size);
  tiling_info.phase = geometry.DestRect().origin() + geometry.ComputePhase();
  tiling_info.spacing = geometry.Spacing();
  tiling_info.scale = {
      geometry.TileSize().width() / tiling_info.image_rect.width(),
      geometry.TileSize().height() / tiling_info.image_rect.height()};

  auto image_auto_dark_mode = ImageClassifierHelper::GetImageAutoDarkMode(
      *info.object.GetFrame(), info.object.StyleRef(), geometry.DestRect(),
      tiling_info.image_rect);
  // This call takes the unscaled image, applies the given scale, and paints it
  // into the dest rect using phase and the given repeat spacing. Note the
  // phase is already scaled.
  const SkBlendMode composite_op =
      WebCoreCompositeToSkiaComposite(layer.Composite(), layer.GetBlendMode());
  const ImagePaintTimingInfo paint_timing_info(false, false);
  context.DrawImageTiled(*image, geometry.DestRect(), tiling_info,
                         image_auto_dark_mode, paint_timing_info, composite_op,
                         respect_orientation);
}

void PaintMaskLayers(GraphicsContext& context, const LayoutObject& object) {
  const ComputedStyle& style = object.StyleRef();
  Vector<const FillLayer*, 8> layer_list;
  for (const FillLayer* layer = &style.MaskLayers(); layer;
       layer = layer->Next()) {
    layer_list.push_back(layer);
  }
  const FillInfo fill_info = {
      style.GetInterpolationQuality(),
      static_cast<cc::PaintFlags::DynamicRangeLimit>(style.DynamicRangeLimit()),
      LayoutObject::ShouldRespectImageOrientation(&object),
      object,
  };
  SVGMaskGeometry geometry(object);
  for (const auto* layer : base::Reversed(layer_list)) {
    PaintMaskLayer(*layer, fill_info, geometry, context);
  }
}

}  // namespace

void SVGMaskPainter::Paint(GraphicsContext& context,
                           const LayoutObject& layout_object,
                           const DisplayItemClient& display_item_client) {
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  // TODO(crbug.com/814815): This condition should be a DCHECK, but for now
  // we may paint the object for filters during PrePaint before the
  // properties are ready.
  if (!properties || !properties->Mask())
    return;

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

  if (RuntimeEnabledFeatures::CSSMaskingInteropEnabled()) {
    PaintMaskLayers(context, layout_object);
    return;
  }

  SVGResourceClient* client = SVGResources::GetClient(layout_object);
  const ComputedStyle& style = layout_object.StyleRef();
  auto* masker = GetSVGResourceAsType<LayoutSVGResourceMasker>(
      *client, style.MaskerResource());
  DCHECK(masker);
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*masker))
    return;
  SECURITY_DCHECK(!masker->SelfNeedsFullLayout());
  masker->ClearInvalidationMask();

  PaintSVGMask(masker, layout_object, context);
}

PaintRecord SVGMaskPainter::PaintResource(SVGResource* mask_resource,
                                          SVGResourceClient& client,
                                          const gfx::RectF& reference_box,
                                          float zoom) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return PaintRecord();
  }

  const AffineTransform content_transformation =
      MaskToContentTransform(*masker, reference_box, zoom);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();
  if (record.empty()) {
    return record;
  }
  gfx::RectF bounds = masker->ResourceBoundingBox(reference_box, zoom);
  AffineTransform origin =
      AffineTransform::Translation(-bounds.x(), -bounds.y());

  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  canvas->concat(AffineTransformToSkM44(origin * content_transformation));
  canvas->drawPicture(std::move(record));
  return recorder.finishRecordingAsPicture();
}

bool SVGMaskPainter::MaskIsValid(SVGResource* mask_resource,
                                 SVGResourceClient& client) {
  return ResolveElementReference(mask_resource, client);
}

gfx::RectF SVGMaskPainter::ResourceBounds(SVGResource* mask_resource,
                                          SVGResourceClient& client,
                                          const gfx::RectF& reference_box,
                                          float zoom) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return gfx::RectF();
  }
  return masker->ResourceBoundingBox(reference_box, zoom);
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
    const auto* svg_mask_reference =
        DynamicTo<StyleSVGMaskReferenceImage>(layer->GetImage());
    if (!svg_mask_reference) {
      continue;
    }
    const gfx::RectF svg_mask_bounds =
        ResourceBounds(svg_mask_reference->GetSVGResource(),
                       svg_mask_reference->GetSVGResourceClient(),
                       reference_box, reference_box_zoom);
    bounds.Union(svg_mask_bounds);
  }
  return gfx::UnionRects(bounds, object.VisualRectInLocalSVGCoordinates());
}

EMaskType SVGMaskPainter::MaskType(SVGResource* mask_resource,
                                   SVGResourceClient& client) {
  auto* masker = ResolveElementReference(mask_resource, client);
  if (!masker) {
    return EMaskType::kAlpha;
  }
  return masker->StyleRef().MaskType();
}

}  // namespace blink
