/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/shapes/shape.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/css/basic_shape_functions.h"
#include "third_party/blink/renderer/core/layout/shapes/box_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/polygon_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/raster_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/rectangle_shape.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

static std::unique_ptr<Shape> CreateInsetShape(const FloatRoundedRect& bounds) {
  DCHECK_GE(bounds.Rect().Width(), 0);
  DCHECK_GE(bounds.Rect().Height(), 0);
  return std::make_unique<BoxShape>(bounds);
}

static std::unique_ptr<Shape> CreateCircleShape(const FloatPoint& center,
                                                float radius) {
  DCHECK_GE(radius, 0);
  return std::make_unique<RectangleShape>(
      FloatRect(center.X() - radius, center.Y() - radius, radius * 2,
                radius * 2),
      FloatSize(radius, radius));
}

static std::unique_ptr<Shape> CreateEllipseShape(const FloatPoint& center,
                                                 const FloatSize& radii) {
  DCHECK_GE(radii.Width(), 0);
  DCHECK_GE(radii.Height(), 0);
  return std::make_unique<RectangleShape>(
      FloatRect(center.X() - radii.Width(), center.Y() - radii.Height(),
                radii.Width() * 2, radii.Height() * 2),
      radii);
}

static std::unique_ptr<Shape> CreatePolygonShape(Vector<FloatPoint> vertices,
                                                 WindRule fill_rule) {
  return std::make_unique<PolygonShape>(std::move(vertices), fill_rule);
}

static inline FloatRect PhysicalRectToLogical(const FloatRect& rect,
                                              float logical_box_height,
                                              WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return rect;
  if (IsFlippedBlocksWritingMode(writing_mode))
    return FloatRect(rect.Y(), logical_box_height - rect.MaxX(), rect.Height(),
                     rect.Width());
  return rect.TransposedRect();
}

static inline FloatPoint PhysicalPointToLogical(const FloatPoint& point,
                                                float logical_box_height,
                                                WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return point;
  if (IsFlippedBlocksWritingMode(writing_mode))
    return FloatPoint(point.Y(), logical_box_height - point.X());
  return point.TransposedPoint();
}

static inline FloatSize PhysicalSizeToLogical(const FloatSize& size,
                                              WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return size;
  return size.TransposedSize();
}

std::unique_ptr<Shape> Shape::CreateShape(const BasicShape* basic_shape,
                                          const LayoutSize& logical_box_size,
                                          WritingMode writing_mode,
                                          float margin) {
  DCHECK(basic_shape);

  bool horizontal_writing_mode = IsHorizontalWritingMode(writing_mode);
  float box_width = horizontal_writing_mode
                        ? logical_box_size.Width().ToFloat()
                        : logical_box_size.Height().ToFloat();
  float box_height = horizontal_writing_mode
                         ? logical_box_size.Height().ToFloat()
                         : logical_box_size.Width().ToFloat();
  std::unique_ptr<Shape> shape;

  switch (basic_shape->GetType()) {
    case BasicShape::kBasicShapeCircleType: {
      const BasicShapeCircle* circle = To<BasicShapeCircle>(basic_shape);
      FloatPoint center =
          FloatPointForCenterCoordinate(circle->CenterX(), circle->CenterY(),
                                        FloatSize(box_width, box_height));
      float radius =
          circle->FloatValueForRadiusInBox(FloatSize(box_width, box_height));
      FloatPoint logical_center = PhysicalPointToLogical(
          center, logical_box_size.Height().ToFloat(), writing_mode);

      shape = CreateCircleShape(logical_center, radius);
      break;
    }

    case BasicShape::kBasicShapeEllipseType: {
      const BasicShapeEllipse* ellipse = To<BasicShapeEllipse>(basic_shape);
      FloatPoint center =
          FloatPointForCenterCoordinate(ellipse->CenterX(), ellipse->CenterY(),
                                        FloatSize(box_width, box_height));
      float radius_x = ellipse->FloatValueForRadiusInBox(ellipse->RadiusX(),
                                                         center.X(), box_width);
      float radius_y = ellipse->FloatValueForRadiusInBox(
          ellipse->RadiusY(), center.Y(), box_height);
      FloatPoint logical_center = PhysicalPointToLogical(
          center, logical_box_size.Height().ToFloat(), writing_mode);

      shape = CreateEllipseShape(logical_center, FloatSize(radius_x, radius_y));
      break;
    }

    case BasicShape::kBasicShapePolygonType: {
      const BasicShapePolygon* polygon = To<BasicShapePolygon>(basic_shape);
      const Vector<Length>& values = polygon->Values();
      wtf_size_t values_size = values.size();
      DCHECK(!(values_size % 2));
      Vector<FloatPoint> vertices(values_size / 2);
      for (wtf_size_t i = 0; i < values_size; i += 2) {
        FloatPoint vertex(FloatValueForLength(values.at(i), box_width),
                          FloatValueForLength(values.at(i + 1), box_height));
        vertices[i / 2] = PhysicalPointToLogical(
            vertex, logical_box_size.Height().ToFloat(), writing_mode);
      }
      shape = CreatePolygonShape(std::move(vertices), polygon->GetWindRule());
      break;
    }

    case BasicShape::kBasicShapeInsetType: {
      const BasicShapeInset& inset = *To<BasicShapeInset>(basic_shape);
      float left = FloatValueForLength(inset.Left(), box_width);
      float top = FloatValueForLength(inset.Top(), box_height);
      float right = FloatValueForLength(inset.Right(), box_width);
      float bottom = FloatValueForLength(inset.Bottom(), box_height);
      FloatRect rect(left, top, std::max<float>(box_width - left - right, 0),
                     std::max<float>(box_height - top - bottom, 0));
      FloatRect logical_rect = PhysicalRectToLogical(
          rect, logical_box_size.Height().ToFloat(), writing_mode);

      FloatSize box_size(box_width, box_height);
      FloatSize top_left_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.TopLeftRadius(), box_size),
          writing_mode);
      FloatSize top_right_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.TopRightRadius(), box_size),
          writing_mode);
      FloatSize bottom_left_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.BottomLeftRadius(), box_size),
          writing_mode);
      FloatSize bottom_right_radius = PhysicalSizeToLogical(
          FloatSizeForLengthSize(inset.BottomRightRadius(), box_size),
          writing_mode);
      FloatRoundedRect::Radii corner_radii(top_left_radius, top_right_radius,
                                           bottom_left_radius,
                                           bottom_right_radius);

      FloatRoundedRect final_rect(logical_rect, corner_radii);
      final_rect.ConstrainRadii();

      shape = CreateInsetShape(final_rect);
      break;
    }

    default:
      NOTREACHED();
  }

  shape->writing_mode_ = writing_mode;
  shape->margin_ = margin;

  return shape;
}

std::unique_ptr<Shape> Shape::CreateEmptyRasterShape(WritingMode writing_mode,
                                                     float margin) {
  std::unique_ptr<RasterShapeIntervals> intervals =
      std::make_unique<RasterShapeIntervals>(0, 0);
  std::unique_ptr<RasterShape> raster_shape =
      std::make_unique<RasterShape>(std::move(intervals), IntSize());
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

static bool ExtractImageData(Image* image,
                             const IntSize& image_size,
                             ArrayBufferContents& contents,
                             RespectImageOrientationEnum respect_orientation) {
  if (!image)
    return false;

  CanvasColorParams color_params;
  SkImageInfo info = SkImageInfo::Make(
      image_size.Width(), image_size.Height(), color_params.GetSkColorType(),
      color_params.GetSkAlphaType(),
      color_params.GetSkColorSpaceForSkSurfaces());
  sk_sp<SkSurface> surface =
      SkSurface::MakeRaster(info, color_params.GetSkSurfaceProps());

  if (!surface)
    return false;

  // FIXME: This is not totally correct but it is needed to prevent shapes
  // that loads SVG Images during paint invalidations to mark layoutObjects
  // for layout, which is not allowed. See https://crbug.com/429346
  ImageObserverDisabler disabler(image);
  PaintFlags flags;
  FloatRect image_source_rect(FloatPoint(), FloatSize(image->Size()));
  IntRect image_dest_rect(IntPoint(), image_size);
  SkiaPaintCanvas canvas(surface->getCanvas());
  canvas.clear(SK_ColorTRANSPARENT);

  image->Draw(&canvas, flags, FloatRect(image_dest_rect), image_source_rect,
              respect_orientation, Image::kDoNotClampImageToSourceRect,
              Image::kSyncDecode);

  size_t size_in_bytes;
  if (!StaticBitmapImage::GetSizeInBytes(image_dest_rect, color_params)
           .AssignIfValid(&size_in_bytes) ||
      size_in_bytes > v8::TypedArray::kMaxLength) {
    return false;
  }
  ArrayBufferContents result(size_in_bytes, 1, ArrayBufferContents::kNotShared,
                             ArrayBufferContents::kZeroInitialize);
  if (result.DataLength() != size_in_bytes)
    return false;
  result.Transfer(contents);

  return StaticBitmapImage::CopyToByteArray(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot()),
      base::span<uint8_t>(reinterpret_cast<uint8_t*>(contents.Data()),
                          contents.DataLength()),
      image_dest_rect, color_params);
}

static std::unique_ptr<RasterShapeIntervals> ExtractIntervalsFromImageData(
    ArrayBufferContents& contents,
    float threshold,
    const IntRect& image_rect,
    const IntRect& margin_rect) {
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);
  DOMUint8ClampedArray* pixel_array =
      DOMUint8ClampedArray::Create(array_buffer, 0, array_buffer->ByteLength());

  unsigned pixel_array_offset = 3;  // Each pixel is four bytes: RGBA.
  uint8_t alpha_pixel_threshold = threshold * 255;

  DCHECK_EQ(image_rect.Size().Area() * 4, pixel_array->length());

  int min_buffer_y = std::max(0, margin_rect.Y() - image_rect.Y());
  int max_buffer_y =
      std::min(image_rect.Height(), margin_rect.MaxY() - image_rect.Y());

  std::unique_ptr<RasterShapeIntervals> intervals =
      std::make_unique<RasterShapeIntervals>(margin_rect.Height(),
                                             -margin_rect.Y());

  for (int y = min_buffer_y; y < max_buffer_y; ++y) {
    int start_x = -1;
    for (int x = 0; x < image_rect.Width(); ++x, pixel_array_offset += 4) {
      uint8_t alpha = pixel_array->Item(pixel_array_offset);
      bool alpha_above_threshold = alpha > alpha_pixel_threshold;
      if (start_x == -1 && alpha_above_threshold) {
        start_x = x;
      } else if (start_x != -1 &&
                 (!alpha_above_threshold || x == image_rect.Width() - 1)) {
        int end_x = alpha_above_threshold ? x + 1 : x;
        intervals->IntervalAt(y + image_rect.Y())
            .Unite(IntShapeInterval(start_x + image_rect.X(),
                                    end_x + image_rect.X()));
        start_x = -1;
      }
    }
  }
  return intervals;
}

static bool IsValidRasterShapeSize(const IntSize& size) {
  // Some platforms don't limit MaxDecodedImageBytes.
  constexpr size_t size32_max_bytes = 0xFFFFFFFF / 4;
  static const size_t max_image_size_bytes =
      std::min(size32_max_bytes, Platform::Current()->MaxDecodedImageBytes());
  return size.Area() * 4 < max_image_size_bytes;
}

std::unique_ptr<Shape> Shape::CreateRasterShape(
    Image* image,
    float threshold,
    const LayoutRect& image_r,
    const LayoutRect& margin_r,
    WritingMode writing_mode,
    float margin,
    RespectImageOrientationEnum respect_orientation) {
  IntRect image_rect = PixelSnappedIntRect(image_r);
  IntRect margin_rect = PixelSnappedIntRect(margin_r);

  if (!IsValidRasterShapeSize(margin_rect.Size()) ||
      !IsValidRasterShapeSize(image_rect.Size())) {
    return CreateEmptyRasterShape(writing_mode, margin);
  }

  ArrayBufferContents contents;
  if (!ExtractImageData(image, image_rect.Size(), contents,
                        respect_orientation)) {
    return CreateEmptyRasterShape(writing_mode, margin);
  }

  std::unique_ptr<RasterShapeIntervals> intervals =
      ExtractIntervalsFromImageData(contents, threshold, image_rect,
                                    margin_rect);
  std::unique_ptr<RasterShape> raster_shape =
      std::make_unique<RasterShape>(std::move(intervals), margin_rect.Size());
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

std::unique_ptr<Shape> Shape::CreateLayoutBoxShape(
    const FloatRoundedRect& rounded_rect,
    WritingMode writing_mode,
    float margin) {
  FloatRect rect(0, 0, rounded_rect.Rect().Width(),
                 rounded_rect.Rect().Height());
  FloatRoundedRect bounds(rect, rounded_rect.GetRadii());
  std::unique_ptr<Shape> shape = CreateInsetShape(bounds);
  shape->writing_mode_ = writing_mode;
  shape->margin_ = margin;

  return shape;
}

}  // namespace blink
