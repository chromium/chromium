// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_transform.h"

#include <algorithm>
#include <optional>

#include "base/check_op.h"
#include "base/notreached.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/geometry/envelope.h"
#include "third_party/ink/src/ink/geometry/rect.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

namespace {

// Performs an inverse operation of `EventPositionToCanonicalPosition()`, to
// convert from canonical coordinates to screen coordinates.
// TODO(crbug.com/379003898): Change EventPositionToCanonicalPosition() to
// return gfx::AxisTransform2d, so that callers can just use the inverse of
// the transform instead of this helper.
gfx::PointF CanonicalPositionToScreenPosition(
    const gfx::PointF& canonical_position,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor) {
  CHECK_GT(scale_factor, 0.0f);
  CHECK(!page_content_rect.IsEmpty());
  gfx::PointF screen_position = canonical_position;
  screen_position.Scale(scale_factor);
  switch (orientation) {
    case PageOrientation::kOriginal:
      // No further modification needed.
      break;
    case PageOrientation::kClockwise90:
      screen_position.SetPoint(
          page_content_rect.width() - screen_position.y() - 1,
          screen_position.x());
      break;
    case PageOrientation::kClockwise180:
      screen_position.SetPoint(
          page_content_rect.width() - screen_position.x() - 1,
          page_content_rect.height() - screen_position.y() - 1);
      break;
    case PageOrientation::kClockwise270:
      screen_position.SetPoint(
          screen_position.y(),
          page_content_rect.height() - screen_position.x() - 1);
      break;
  }
  // Account for scrolling, which is in the page content's origin.
  screen_position += page_content_rect.origin().OffsetFromOrigin();
  return screen_position;
}

}  // namespace

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

gfx::Rect CanonicalInkEnvelopeToInvalidationScreenRect(
    const ink::Envelope& envelope,
    PageOrientation orientation,
    const gfx::Rect& page_content_rect,
    float scale_factor) {
  const std::optional<ink::Rect>& ink_rect = envelope.AsRect();
  CHECK(ink_rect.has_value());

  gfx::PointF p1 = CanonicalPositionToScreenPosition(
      gfx::PointF(ink_rect->XMin(), ink_rect->YMin()), orientation,
      page_content_rect, scale_factor);
  gfx::PointF p2 = CanonicalPositionToScreenPosition(
      gfx::PointF(ink_rect->XMax(), ink_rect->YMax()), orientation,
      page_content_rect, scale_factor);

  // Width and height get +1 since both of the points are to be included in the
  // area; otherwise it would be an open rectangle on two edges.
  float x = std::min(p1.x(), p2.x());
  float y = std::min(p1.y(), p2.y());
  float w = std::max(p1.x(), p2.x()) - x + 1;
  float h = std::max(p1.y(), p2.y()) - y + 1;
  return gfx::ToEnclosingRect(gfx::RectF(x, y, w, h));
}

gfx::AxisTransform2d GetCanonicalToPdfTransform(float page_height) {
  CHECK_GE(page_height, 0);
  constexpr float kScreenToPageScale =
      static_cast<float>(printing::kPointsPerInch) / printing::kPixelsPerInch;
  return gfx::AxisTransform2d::FromScaleAndTranslation(
      {kScreenToPageScale, -kScreenToPageScale}, {0, page_height});
}

}  // namespace chrome_pdf
