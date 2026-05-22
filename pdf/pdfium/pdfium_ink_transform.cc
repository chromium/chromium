// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_transform.h"

#include "base/notreached.h"
#include "pdf/pdf_ink_transform.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_rotation.h"
#include "printing/units.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

using printing::kUnitConversionFactorPixelsToPoints;

namespace chrome_pdf {

gfx::Transform GetCanonicalToPdfTransformForPage(FPDF_PAGE page) {
  CHECK(page);

  // Get the intersection between the page's MediaBox and CropBox, to find
  // the translation offset for the shapes' transform.
  const PdfRect bounding_box = GetPageBoundingBox(page).value();
  const gfx::Vector2dF offset(bounding_box.left(), bounding_box.bottom());

  return GetCanonicalToPdfTransform(
      {FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page)},
      GetPageRotation(page).value_or(PageRotation::kRotate0), offset);
}

FS_MATRIX CalculateTextBoxTransform(
    const gfx::RectF& textbox_page_rect,
    int orientation,
    const gfx::Transform& canonical_to_pdf_transform) {
  gfx::PointF textbox_page_origin;
  gfx::Vector2dF text_direction_vector;
  gfx::Vector2dF text_ascent_vector;

  switch (orientation) {
    case 0:
      // Origin is at top-left. Text reads left-to-right, ascent points up.
      textbox_page_origin = textbox_page_rect.origin();
      text_direction_vector = {1, 0};
      text_ascent_vector = {0, -1};
      break;
    case 1:
      // Origin is at top-right. Text reads top-to-bottom, ascent points right.
      textbox_page_origin = textbox_page_rect.top_right();
      text_direction_vector = {0, 1};
      text_ascent_vector = {1, 0};
      break;
    case 2:
      // Origin is at bottom-right. Text reads right-to-left, ascent points
      // down.
      textbox_page_origin = textbox_page_rect.bottom_right();
      text_direction_vector = {-1, 0};
      text_ascent_vector = {0, 1};
      break;
    case 3:
      // Origin is at bottom-left. Text reads bottom-to-top, ascent points left.
      textbox_page_origin = textbox_page_rect.bottom_left();
      text_direction_vector = {0, -1};
      text_ascent_vector = {-1, 0};
      break;
    default:
      NOTREACHED();
  }

  gfx::PointF textbox_pdf_origin =
      canonical_to_pdf_transform.MapPoint(textbox_page_origin);

  gfx::Transform unscaled_transform = canonical_to_pdf_transform;
  unscaled_transform.Scale(1.0f / kUnitConversionFactorPixelsToPoints);
  gfx::Vector3dF text_pdf_direction =
      unscaled_transform.MapVector(gfx::Vector3dF(text_direction_vector));
  gfx::Vector3dF text_pdf_ascent =
      unscaled_transform.MapVector(gfx::Vector3dF(text_ascent_vector));

  return FS_MATRIX{text_pdf_direction.x(), text_pdf_direction.y(),
                   text_pdf_ascent.x(),    text_pdf_ascent.y(),
                   textbox_pdf_origin.x(), textbox_pdf_origin.y()};
}

}  // namespace chrome_pdf
