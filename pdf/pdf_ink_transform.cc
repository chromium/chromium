// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome_pdf {

gfx::PointF EventPositionToCanonicalPosition(const gfx::PointF& event_position,
                                             PageOrientation orientation,
                                             const gfx::Rect& page_content_rect,
                                             float scale_factor) {
  CHECK_GT(scale_factor, 0.0f);
  CHECK(!page_content_rect.IsEmpty());
  gfx::PointF page_position =
      event_position - page_content_rect.OffsetFromOrigin();
  switch (orientation) {
    case PageOrientation::kOriginal:
      // No further modification needed.
      break;
    case PageOrientation::kClockwise90:
      page_position.SetPoint(page_position.y(),
                             page_content_rect.width() - page_position.x() - 1);
      break;
    case PageOrientation::kClockwise180:
      page_position.SetPoint(
          page_content_rect.width() - page_position.x() - 1,
          page_content_rect.height() - page_position.y() - 1);
      break;
    case PageOrientation::kClockwise270:
      page_position.SetPoint(page_content_rect.height() - page_position.y() - 1,
                             page_position.x());
      break;
  }
  page_position.InvScale(scale_factor);
  return page_position;
}

ink::AffineTransform GetInkRenderTransform(
    const gfx::Vector2dF& viewport_origin_offset,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor) {
  CHECK_GE(viewport_origin_offset.x(), 0.0f);
  CHECK_GE(viewport_origin_offset.y(), 0.0f);
  CHECK_GT(scale_factor, 0.0f);
  CHECK(!page_content_rect.IsEmpty());

  const float dx = viewport_origin_offset.x() + page_content_rect.x();
  const float dy = viewport_origin_offset.y() + page_content_rect.y();

  switch (orientation) {
    case PageOrientation::kOriginal:
      return ink::AffineTransform(scale_factor, 0, dx, 0, scale_factor, dy);
    case PageOrientation::kClockwise90:
      return ink::AffineTransform(0, -scale_factor,
                                  dx + page_content_rect.width() - 1,
                                  scale_factor, 0, dy);
    case PageOrientation::kClockwise180:
      return ink::AffineTransform(
          -scale_factor, 0, dx + page_content_rect.width() - 1, 0,
          -scale_factor, dy + page_content_rect.height() - 1);
    case PageOrientation::kClockwise270:
      return ink::AffineTransform(0, scale_factor, dx, -scale_factor, 0,
                                  dy + page_content_rect.height() - 1);
  }
  NOTREACHED();
}

}  // namespace chrome_pdf
