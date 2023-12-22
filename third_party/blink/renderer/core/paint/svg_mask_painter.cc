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

LayoutSVGResourceMasker* ResolveElementReference(SVGResource* mask_resource,
                                                 SVGResourceClient* client) {
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

LayoutSVGResourceMasker* ResolveElementReference(
    const StyleMaskSourceImage& mask_source,
    const ImageResourceObserver& observer) {
  return ResolveElementReference(mask_source.GetSVGResource(),
                                 mask_source.GetSVGResourceClient(observer));
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
  // Given the size that the whole image should draw at, and the input phase
  // requested by the content, and the space between repeated tiles, compute a
  // phase that is no more than one size + space in magnitude.
  const gfx::SizeF step_per_tile = tile_size_ + spacing_;
  return {std::fmod(-phase_.x(), step_per_tile.width()),
          std::fmod(-phase_.y(), step_per_tile.height())};
}

float GetSpaceBetweenImageTiles(float area_size, float tile_size) {
  const float number_of_tiles = std::floor(area_size / tile_size);
  if (number_of_tiles < 1) {
    return -1;
  }
  return (area_size - number_of_tiles * tile_size) / (number_of_tiles - 1);
}

float ComputeRoundedTileSize(float area_size, float tile_size) {
  const float nr_tiles = std::max(1.0f, std::round(area_size / tile_size));
  return area_size / nr_tiles;
}

float ComputeTilePhase(float position, float tile_extent) {
  return tile_extent ? tile_extent - std::fmod(position, tile_extent) : 0;
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
  const IntrinsicSizingInfo sizing_info =
      image->GetNaturalSizingInfo(object_.StyleRef().EffectiveZoom(),
                                  object_.StyleRef().ImageOrientation());

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
        tile_size = image->ImageSize(object_.StyleRef().EffectiveZoom(),
                                     positioning_area.size(),
                                     object_.StyleRef().ImageOrientation());
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

  const gfx::SizeF available_size = positioning_area.size() - tile_size_;
  const gfx::PointF computed_position(
      FloatValueForLength(layer.PositionX(), available_size.width()),
      FloatValueForLength(layer.PositionY(), available_size.height()));
  // Adjust position based on the specified edge origin.
  const gfx::PointF offset(
      layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight
          ? available_size.width() - computed_position.x()
          : computed_position.x(),
      layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom
          ? available_size.height() - computed_position.y()
          : computed_position.y());

  const FillRepeat& repeat = layer.Repeat();
  switch (repeat.x) {
    case EFillRepeat::kRoundFill:
      if (tile_size_.width() <= 0) {
        break;
      }
      if (positioning_area.width() > 0) {
        const float rounded_width = ComputeRoundedTileSize(
            positioning_area.width(), tile_size_.width());
        // Maintain aspect ratio if mask-size: auto is set
        if (layer.SizeLength().Height().IsAuto() &&
            repeat.y != EFillRepeat::kRoundFill) {
          tile_size_.set_height(
              ResolveHeightForRatio(rounded_width, tile_size_));
        }
        tile_size_.set_width(rounded_width);

        // Force the first tile to line up with the edge of the positioning
        // area.
        phase_.set_x(ComputeTilePhase(offset.x(), tile_size_.width()));
      }
      break;
    case EFillRepeat::kRepeatFill:
      if (tile_size_.width() <= 0) {
        break;
      }
      phase_.set_x(ComputeTilePhase(offset.x(), tile_size_.width()));
      break;
    case EFillRepeat::kSpaceFill: {
      if (tile_size_.width() <= 0) {
        break;
      }
      const float space = GetSpaceBetweenImageTiles(positioning_area.width(),
                                                    tile_size_.width());
      if (space >= 0) {
        spacing_.set_width(space);
        phase_.set_x(ComputeTilePhase(0, tile_size_.width() + space));
        break;
      }
      // Handle as no-repeat.
      [[fallthrough]];
    }
    case EFillRepeat::kNoRepeatFill:
      dest_rect_.set_x(dest_rect_.x() + offset.x());
      dest_rect_.set_width(tile_size_.width());
      break;
  }

  switch (repeat.y) {
    case EFillRepeat::kRoundFill:
      if (tile_size_.height() <= 0) {
        break;
      }
      if (positioning_area.height() > 0) {
        const float rounded_height = ComputeRoundedTileSize(
            positioning_area.height(), tile_size_.height());
        // Maintain aspect ratio if mask-size: auto is set
        if (layer.SizeLength().Width().IsAuto() &&
            repeat.x != EFillRepeat::kRoundFill) {
          tile_size_.set_width(
              ResolveWidthForRatio(rounded_height, tile_size_));
        }
        tile_size_.set_height(rounded_height);

        phase_.set_y(ComputeTilePhase(offset.y(), tile_size_.height()));
      }
      break;
    case EFillRepeat::kRepeatFill:
      if (tile_size_.height() <= 0) {
        break;
      }
      phase_.set_y(ComputeTilePhase(offset.y(), tile_size_.height()));
      break;
    case EFillRepeat::kSpaceFill: {
      if (tile_size_.height() <= 0) {
        break;
      }
      const float space = GetSpaceBetweenImageTiles(positioning_area.height(),
                                                    tile_size_.height());
      if (space >= 0) {
        spacing_.set_height(space);
        phase_.set_y(ComputeTilePhase(0, tile_size_.height() + space));
        break;
      }
      // Handle as no-repeat.
      [[fallthrough]];
    }
    case EFillRepeat::kNoRepeatFill:
      dest_rect_.set_y(dest_rect_.y() + offset.y());
      dest_rect_.set_height(tile_size_.height());
      break;
  }

  if (!object_.IsSVGForeignObject()) {
    const float zoom = object_.StyleRef().EffectiveZoom();
    if (clip_rect_) {
      clip_rect_->InvScale(zoom);
    }
    dest_rect_.InvScale(zoom);
    tile_size_.InvScale(zoom);
    spacing_.InvScale(zoom);
    phase_.InvScale(zoom);
  }
}

void PaintSVGMask(LayoutSVGResourceMasker* masker,
                  const gfx::RectF& reference_box,
                  float zoom,
                  GraphicsContext& context,
                  SkBlendMode composite_op,
                  bool apply_mask_type) {
  const AffineTransform content_transformation =
      MaskToContentTransform(*masker, reference_box, zoom);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();

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

struct FillInfo {
  STACK_ALLOCATED();

 public:
  const InterpolationQuality interpolation_quality;
  const cc::PaintFlags::DynamicRangeLimit dynamic_range_limit;
  const RespectImageOrientationEnum respect_orientation;
  const LayoutObject& object;
};

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
                    const FillInfo& info,
                    SVGMaskGeometry& geometry,
                    GraphicsContext& context) {
  const StyleImage* style_image = layer.GetImage();
  if (!style_image) {
    return;
  }

  absl::optional<ScopedMaskLuminanceLayer> mask_luminance_scope;
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

  GraphicsContextStateSaver saver(context, false);

  // If the "image" referenced by the FillLayer is an SVG <mask> reference (and
  // this is a layer for a mask), then repeat, position, clip, origin and size
  // should have no effect.
  if (const auto* mask_source = ToMaskSourceIfSVGMask(*style_image)) {
    const ComputedStyle& style = info.object.StyleRef();
    const float zoom =
        info.object.IsSVGForeignObject() ? style.EffectiveZoom() : 1;
    gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
        info.object, GeometryBox::kFillBox,
        SVGResources::ForeignObjectQuirk::kDisabled);
    reference_box.Scale(zoom);

    saver.Save();
    SVGMaskPainter::PaintSVGMaskLayer(
        context, *mask_source, info.object, reference_box, zoom, composite_op,
        layer.MaskMode() == EFillMaskMode::kMatchSource);
    return;
  }
  geometry.Calculate(layer);

  if (geometry.TileSize().IsEmpty()) {
    return;
  }

  scoped_refptr<Image> image =
      style_image->GetImage(info.object, info.object.GetDocument(),
                            info.object.StyleRef(), geometry.TileSize());
  if (!image) {
    return;
  }

  ScopedImageRenderingSettings image_rendering_settings_context(
      context, info.interpolation_quality, info.dynamic_range_limit);

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
      style.ImageOrientation(),
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

  const gfx::RectF reference_box = SVGResources::ReferenceBoxForEffects(
      layout_object, GeometryBox::kFillBox,
      SVGResources::ForeignObjectQuirk::kDisabled);
  const float zoom =
      layout_object.IsSVGForeignObject() ? style.EffectiveZoom() : 1;

  context.Save();
  PaintSVGMask(masker, reference_box, zoom, context, SkBlendMode::kSrcOver,
               /*apply_mask_type=*/true);
  context.Restore();
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
  context.Clip(masker->ResourceBoundingBox(reference_box, zoom));
  PaintSVGMask(masker, reference_box, zoom, context, composite_op,
               apply_mask_type);
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
