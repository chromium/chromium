// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include "base/check_op.h"
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

InkAffineTransform GetInkRenderTransform(
    const gfx::Vector2dF& viewport_origin_offset,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor) {
  CHECK_GE(viewport_origin_offset.x(), 0.0f);
  CHECK_GE(viewport_origin_offset.y(), 0.0f);
  CHECK_GT(scale_factor, 0.0f);
  CHECK(!page_content_rect.IsEmpty());
  float dx = viewport_origin_offset.x() + page_content_rect.x();
  float dy = viewport_origin_offset.y() + page_content_rect.y();
  InkAffineTransform transform;

  switch (orientation) {
    case PageOrientation::kOriginal:
      transform.a = scale_factor;
      transform.b = 0;
      transform.c = dx;
      transform.d = 0;
      transform.e = scale_factor;
      transform.f = dy;
      break;
    case PageOrientation::kClockwise90:
      transform.a = 0;
      transform.b = -scale_factor;
      transform.c = dx + page_content_rect.width() - 1;
      transform.d = scale_factor;
      transform.e = 0;
      transform.f = dy;
      break;
    case PageOrientation::kClockwise180:
      transform.a = -scale_factor;
      transform.b = 0;
      transform.c = dx + page_content_rect.width() - 1;
      transform.d = 0;
      transform.e = -scale_factor;
      transform.f = dy + page_content_rect.height() - 1;
      break;
    case PageOrientation::kClockwise270:
      transform.a = 0;
      transform.b = scale_factor;
      transform.c = dx;
      transform.d = -scale_factor;
      transform.e = 0;
      transform.f = dy + page_content_rect.height() - 1;
      break;
  }
  return transform;
}

}  // namespace chrome_pdf
