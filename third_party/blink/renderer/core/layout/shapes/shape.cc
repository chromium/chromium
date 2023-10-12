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

#include "cc/paint/paint_flags.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/shapes/box_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/ellipse_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/polygon_shape.h"
#include "third_party/blink/renderer/core/layout/shapes/raster_shape.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

static std::unique_ptr<Shape> CreateInsetShape(const FloatRoundedRect& bounds) {
  DCHECK_GE(bounds.Rect().width(), 0);
  DCHECK_GE(bounds.Rect().height(), 0);
  return std::make_unique<BoxShape>(bounds);
}

static inline gfx::RectF PhysicalRectToLogical(const gfx::RectF& rect,
                                               float logical_box_height,
                                               WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return rect;
  if (IsFlippedBlocksWritingMode(writing_mode)) {
    return gfx::RectF(rect.y(), logical_box_height - rect.right(),
                      rect.height(), rect.width());
  }
  return gfx::TransposeRect(rect);
}

static inline gfx::PointF PhysicalPointToLogical(const gfx::PointF& point,
                                                 float logical_box_height,
                                                 WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return point;
  if (IsFlippedBlocksWritingMode(writing_mode))
    return gfx::PointF(point.y(), logical_box_height - point.x());
  return gfx::TransposePoint(point);
}

static inline gfx::SizeF PhysicalSizeToLogical(const gfx::SizeF& size,
                                               WritingMode writing_mode) {
  if (IsHorizontalWritingMode(writing_mode))
    return size;
  return gfx::TransposeSize(size);
}

std::unique_ptr<Shape> Shape::CreateShape(const BasicShape* basic_shape,
                                          const LogicalSize& logical_box_size,
                                          WritingMode writing_mode,
                                          float margin) {
  DCHECK(basic_shape);

  bool horizontal_writing_mode = IsHorizontalWritingMode(writing_mode);
  float box_width = horizontal_writing_mode
                        ? logical_box_size.inline_size.ToFloat()
                        : logical_box_size.block_size.ToFloat();
  float box_height = horizontal_writing_mode
                         ? logical_box_size.block_size.ToFloat()
                         : logical_box_size.inline_size.ToFloat();
  std::unique_ptr<Shape> shape;

  switch (basic_shape->GetType()) {
    case BasicShape::kBasicShapeCircleType: {
      const BasicShapeCircle* circle = To<BasicShapeCircle>(basic_shape);
      gfx::PointF center =
          PointForCenterCoordinate(circle->CenterX(), circle->CenterY(),
                                   gfx::SizeF(box_width, box_height));
      float radius = circle->FloatValueForRadiusInBox(
          center, gfx::SizeF(box_width, box_height));
      gfx::PointF logical_center = PhysicalPointToLogical(
          center, logical_box_size.block_size.ToFloat(), writing_mode);

      shape = std::make_unique<EllipseShape>(logical_center, radius, radius);
      break;
    }

    case BasicShape::kBasicShapeEllipseType: {
      const BasicShapeEllipse* ellipse = To<BasicShapeEllipse>(basic_shape);
      gfx::PointF center =
          PointForCenterCoordinate(ellipse->CenterX(), ellipse->CenterY(),
                                   gfx::SizeF(box_width, box_height));
      float radius_x = ellipse->FloatValueForRadiusInBox(ellipse->RadiusX(),
                                                         center.x(), box_width);
      float radius_y = ellipse->FloatValueForRadiusInBox(
          ellipse->RadiusY(), center.y(), box_height);
      gfx::PointF logical_center = PhysicalPointToLogical(
          center, logical_box_size.block_size.ToFloat(), writing_mode);

      shape =
          std::make_unique<EllipseShape>(logical_center, radius_x, radius_y);
      break;
    }

    case BasicShape::kBasicShapePolygonType: {
      const BasicShapePolygon* polygon = To<BasicShapePolygon>(basic_shape);
      const Vector<Length>& values = polygon->Values();
      wtf_size_t values_size = values.size();
      DCHECK(!(values_size % 2));
      Vector<gfx::PointF> vertices(values_size / 2);
      for (wtf_size_t i = 0; i < values_size; i += 2) {
        gfx::PointF vertex(FloatValueForLength(values.at(i), box_width),
                           FloatValueForLength(values.at(i + 1), box_height));
        vertices[i / 2] = PhysicalPointToLogical(
            vertex, logical_box_size.block_size.ToFloat(), writing_mode);
      }
      shape = std::make_unique<PolygonShape>(std::move(vertices),
                                             polygon->GetWindRule());
      break;
    }

    case BasicShape::kBasicShapeInsetType: {
      const BasicShapeInset& inset = *To<BasicShapeInset>(basic_shape);
      float left = FloatValueForLength(inset.Left(), box_width);
      float top = FloatValueForLength(inset.Top(), box_height);
      float right = FloatValueForLength(inset.Right(), box_width);
      float bottom = FloatValueForLength(inset.Bottom(), box_height);
      gfx::RectF rect(left, top, std::max<float>(box_width - left - right, 0),
                      std::max<float>(box_height - top - bottom, 0));
      gfx::RectF logical_rect = PhysicalRectToLogical(
          rect, logical_box_size.block_size.ToFloat(), writing_mode);

      gfx::SizeF box_size(box_width, box_height);
      gfx::SizeF top_left_radius = PhysicalSizeToLogical(
          SizeForLengthSize(inset.TopLeftRadius(), box_size), writing_mode);
      gfx::SizeF top_right_radius = PhysicalSizeToLogical(
          SizeForLengthSize(inset.TopRightRadius(), box_size), writing_mode);
      gfx::SizeF bottom_left_radius = PhysicalSizeToLogical(
          SizeForLengthSize(inset.BottomLeftRadius(), box_size), writing_mode);
      gfx::SizeF bottom_right_radius = PhysicalSizeToLogical(
          SizeForLengthSize(inset.BottomRightRadius(), box_size), writing_mode);
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
      std::make_unique<RasterShape>(std::move(intervals), gfx::Size());
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

static bool ExtractImageData(Image* image,
                             const gfx::Size& image_size,
                             ArrayBufferContents& contents,
                             RespectImageOrientationEnum respect_orientation) {
  if (!image)
    return false;

  // Compute the SkImageInfo for the output.
  SkImageInfo dst_info = SkImageInfo::Make(
      image_size.width(), image_size.height(), kN32_SkColorType,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB());

  // Populate |contents| with newly allocated and zero-initialized data, big
  // enough for |dst_info|.
  size_t dst_size_bytes = dst_info.computeMinByteSize();
  {
    if (SkImageInfo::ByteSizeOverflowed(dst_size_bytes) ||
        dst_size_bytes > v8::TypedArray::kMaxByteLength) {
      return false;
    }
    ArrayBufferContents result(dst_size_bytes, 1,
                               ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kZeroInitialize);
    if (result.DataLength() != dst_size_bytes)
      return false;
    result.Transfer(contents);
  }

  // Set |surface| to draw directly to |contents|.
  const SkSurfaceProps disable_lcd_props(0, kUnknown_SkPixelGeometry);
  sk_sp<SkSurface> surface = SkSurfaces::WrapPixels(
      dst_info, contents.Data(), dst_info.minRowBytes(), &disable_lcd_props);
  if (!surface)
    return false;

  // FIXME: This is not totally correct but it is needed to prevent shapes
  // that loads SVG Images during paint invalidations to mark layoutObjects
  // for layout, which is not allowed. See https://crbug.com/429346
  ImageObserverDisabler disabler(image);
  cc::PaintFlags flags;
  gfx::RectF image_source_rect(gfx::SizeF(image->Size()));
  gfx::Rect image_dest_rect(image_size);
  SkiaPaintCanvas canvas(surface->getCanvas());
  canvas.clear(SkColors::kTransparent);
  ImageDrawOptions draw_options;
  draw_options.respect_orientation = respect_orientation;
  draw_options.clamping_mode = Image::kDoNotClampImageToSourceRect;
  image->Draw(&canvas, flags, gfx::RectF(image_dest_rect), image_source_rect,
              draw_options);
  return true;
}

static std::unique_ptr<RasterShapeIntervals> ExtractIntervalsFromImageData(
    ArrayBufferContents& contents,
    float threshold,
    const gfx::Rect& image_rect,
    const gfx::Rect& margin_rect) {
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);
  DOMUint8ClampedArray* pixel_array =
      DOMUint8ClampedArray::Create(array_buffer, 0, array_buffer->ByteLength());

  unsigned pixel_array_offset = 3;  // Each pixel is four bytes: RGBA.
  uint8_t alpha_pixel_threshold = threshold * 255;

  DCHECK_EQ(image_rect.size().Area64() * 4, pixel_array->length());

  int min_buffer_y = std::max(0, margin_rect.y() - image_rect.y());
  int max_buffer_y =
      std::min(image_rect.height(), margin_rect.bottom() - image_rect.y());

  std::unique_ptr<RasterShapeIntervals> intervals =
      std::make_unique<RasterShapeIntervals>(margin_rect.height(),
                                             -margin_rect.y());

  for (int y = min_buffer_y; y < max_buffer_y; ++y) {
    int start_x = -1;
    for (int x = 0; x < image_rect.width(); ++x, pixel_array_offset += 4) {
      uint8_t alpha = pixel_array->Item(pixel_array_offset);
      bool alpha_above_threshold = alpha > alpha_pixel_threshold;
      if (start_x == -1 && alpha_above_threshold) {
        start_x = x;
      } else if (start_x != -1 &&
                 (!alpha_above_threshold || x == image_rect.width() - 1)) {
        int end_x = alpha_above_threshold ? x + 1 : x;
        intervals->IntervalAt(y + image_rect.y())
            .Unite(IntShapeInterval(start_x + image_rect.x(),
                                    end_x + image_rect.x()));
        start_x = -1;
      }
    }
  }
  return intervals;
}

static bool IsValidRasterShapeSize(const gfx::Size& size) {
  // Some platforms don't limit MaxDecodedImageBytes.
  constexpr size_t size32_max_bytes = 0xFFFFFFFF / 4;
  static const size_t max_image_size_bytes =
      std::min(size32_max_bytes, Platform::Current()->MaxDecodedImageBytes());
  return size.Area64() * 4 < max_image_size_bytes;
}

std::unique_ptr<Shape> Shape::CreateRasterShape(
    Image* image,
    float threshold,
    const DeprecatedLayoutRect& image_r,
    const DeprecatedLayoutRect& margin_r,
    WritingMode writing_mode,
    float margin,
    RespectImageOrientationEnum respect_orientation) {
  gfx::Rect image_rect = ToPixelSnappedRect(image_r);
  gfx::Rect margin_rect = ToPixelSnappedRect(margin_r);

  if (!IsValidRasterShapeSize(margin_rect.size()) ||
      !IsValidRasterShapeSize(image_rect.size())) {
    return CreateEmptyRasterShape(writing_mode, margin);
  }

  ArrayBufferContents contents;
  if (!ExtractImageData(image, image_rect.size(), contents,
                        respect_orientation)) {
    return CreateEmptyRasterShape(writing_mode, margin);
  }

  std::unique_ptr<RasterShapeIntervals> intervals =
      ExtractIntervalsFromImageData(contents, threshold, image_rect,
                                    margin_rect);
  std::unique_ptr<RasterShape> raster_shape =
      std::make_unique<RasterShape>(std::move(intervals), margin_rect.size());
  raster_shape->writing_mode_ = writing_mode;
  raster_shape->margin_ = margin;
  return std::move(raster_shape);
}

std::unique_ptr<Shape> Shape::CreateLayoutBoxShape(
    const FloatRoundedRect& rounded_rect,
    WritingMode writing_mode,
    float margin) {
  gfx::RectF rect(rounded_rect.Rect().size());
  FloatRoundedRect bounds(rect, rounded_rect.GetRadii());
  std::unique_ptr<Shape> shape = CreateInsetShape(bounds);
  shape->writing_mode_ = writing_mode;
  shape->margin_ = margin;

  return shape;
}

}  // namespace blink
