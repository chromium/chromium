// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include <algorithm>
#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "pdf/page_rotation.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/geometry/envelope.h"
#include "third_party/ink/src/ink/geometry/rect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

using printing::kUnitConversionFactorPixelsToPoints;

namespace chrome_pdf {

namespace {

gfx::Size GetOriginalUnrotatedSize(PageOrientation orientation,
                                   const gfx::Size& size) {
  if (!IsTransposedPageOrientation(orientation)) {
    return size;
  }

  gfx::Size transposed_size(size);
  transposed_size.Transpose();
  return transposed_size;
}

}  // namespace

gfx::Transform GetEventToCanonicalTransform(PageOrientation orientation,
                                            const gfx::Rect& page_content_rect,
                                            float scale_factor) {
  CHECK_GT(scale_factor, 0.0f);
  CHECK(!page_content_rect.IsEmpty());

  gfx::Transform transform;
  transform.PostTranslate(-page_content_rect.OffsetFromOrigin());
  switch (orientation) {
    case PageOrientation::kOriginal:
      // No further modification needed.
      break;
    case PageOrientation::kClockwise90:
      transform.PostConcat(gfx::Transform::Make270degRotation());
      transform.PostTranslate(0, page_content_rect.width() - 1);
      break;
    case PageOrientation::kClockwise180:
      transform.PostConcat(gfx::Transform::Make180degRotation());
      transform.PostTranslate(page_content_rect.width() - 1,
                              page_content_rect.height() - 1);
      break;
    case PageOrientation::kClockwise270:
      transform.PostConcat(gfx::Transform::Make90degRotation());
      transform.PostTranslate(page_content_rect.height() - 1, 0);
      break;
  }
  transform.PostScale(1 / scale_factor);
  return transform;
}

ink::AffineTransform GetInkRenderTransform(
    const gfx::Vector2dF& viewport_origin_offset,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    const gfx::SizeF& page_size_in_points) {
  CHECK_GE(viewport_origin_offset.x(), 0.0f);
  CHECK_GE(viewport_origin_offset.y(), 0.0f);
  CHECK(!page_content_rect.IsEmpty());
  CHECK(!page_size_in_points.IsEmpty());

  // To avoid a noticeable shift in position of an in-progress vs. applied
  // Ink stroke, the rendering transform generated here needs to match the
  // matrix setup done in PDFium's `CPDF_Page::GetDisplayMatrix()`.
  const float dx = viewport_origin_offset.x() + page_content_rect.x();
  const float dy = viewport_origin_offset.y() + page_content_rect.y();
  const gfx::Size original_unrotated_page_size =
      GetOriginalUnrotatedSize(orientation, page_content_rect.size());
  const float scale_factor_x = original_unrotated_page_size.width() *
                               kUnitConversionFactorPixelsToPoints /
                               page_size_in_points.width();
  const float scale_factor_y = original_unrotated_page_size.height() *
                               kUnitConversionFactorPixelsToPoints /
                               page_size_in_points.height();

  switch (orientation) {
    case PageOrientation::kOriginal:
      return ink::AffineTransform(scale_factor_x, 0, dx, 0, scale_factor_y, dy);
    case PageOrientation::kClockwise90:
      return ink::AffineTransform(0, -scale_factor_x,
                                  dx + page_content_rect.width(),
                                  scale_factor_y, 0, dy);
    case PageOrientation::kClockwise180:
      return ink::AffineTransform(
          -scale_factor_x, 0, dx + page_content_rect.width(), 0,
          -scale_factor_y, dy + page_content_rect.height());
    case PageOrientation::kClockwise270:
      return ink::AffineTransform(0, scale_factor_x, dx, -scale_factor_y, 0,
                                  dy + page_content_rect.height());
  }
  NOTREACHED();
}

ink::AffineTransform GetInkThumbnailTransform(
    const gfx::Size& canvas_size,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor) {
  // Since thumbnails are always drawn without any rotation, the transform only
  // needs to perform scaling.
  //
  // However, `page_content_rect` may be rotated, so normalize it as needed.
  gfx::Size content_size = page_content_rect.size();
  if (orientation == PageOrientation::kClockwise90 ||
      orientation == PageOrientation::kClockwise270) {
    content_size.Transpose();
  }

  const float ratio =
      scale_factor *
      std::min(
          static_cast<float>(canvas_size.width()) / content_size.width(),
          static_cast<float>(canvas_size.height()) / content_size.height());
  return {ratio, 0, 0, 0, ratio, 0};
}

gfx::Rect CanonicalInkEnvelopeToInvalidationScreenRect(
    const ink::Envelope& envelope,
    const gfx::Transform& transform) {
  const std::optional<ink::Rect>& ink_rect = envelope.AsRect();
  CHECK(ink_rect.has_value());

  gfx::PointF p1 =
      transform.MapPoint(gfx::PointF(ink_rect->XMin(), ink_rect->YMin()));
  gfx::PointF p2 =
      transform.MapPoint(gfx::PointF(ink_rect->XMax(), ink_rect->YMax()));

  // Width and height get +1 since both of the points are to be included in the
  // area; otherwise it would be an open rectangle on two edges.
  const std::pair<float, float> minmax_x = std::minmax(p1.x(), p2.x());
  const std::pair<float, float> minmax_y = std::minmax(p1.y(), p2.y());
  float w = 1 + minmax_x.second - minmax_x.first;
  float h = 1 + minmax_y.second - minmax_y.first;
  gfx::Rect result =
      gfx::ToEnclosingRect(gfx::RectF(minmax_x.first, minmax_y.first, w, h));
  // Expand the invalidation rect a bit more to account for any other rounding
  // errors that may have occurred.
  result.Outset(1);
  return result;
}

gfx::Transform GetCanonicalToPdfTransform(const gfx::SizeF& page_size,
                                          PageRotation page_rotation,
                                          const gfx::Vector2dF& translate) {
  CHECK_GE(page_size.width(), 0);
  CHECK_GE(page_size.height(), 0);

  auto transform =
      gfx::Transform::MakeScale(kUnitConversionFactorPixelsToPoints,
                                -kUnitConversionFactorPixelsToPoints);

  switch (page_rotation) {
    case PageRotation::kRotate0:
      transform.PostTranslate(
          {translate.x(), page_size.height() + translate.y()});
      return transform;
    case PageRotation::kRotate90:
      transform.PostConcat(gfx::Transform::Make90degRotation());
      transform.PostTranslate({translate.x(), translate.y()});
      return transform;
    case PageRotation::kRotate180:
      transform.PostConcat(gfx::Transform::Make180degRotation());
      transform.PostTranslate(
          {page_size.width() + translate.x(), translate.y()});
      return transform;
    case PageRotation::kRotate270:
      transform.PostConcat(gfx::Transform::Make270degRotation());
      transform.PostTranslate({page_size.height() + translate.x(),
                               page_size.width() + translate.y()});
      return transform;
  }
  NOTREACHED();
}

}  // namespace chrome_pdf
