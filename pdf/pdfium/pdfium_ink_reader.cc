// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdf_ink_constants.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_transform.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_rotation.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/geometry/mesh.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/geometry/tessellator.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace chrome_pdf {

namespace {

bool IsV2InkPath(FPDF_PAGEOBJECT page_object) {
  if (FPDFPageObj_GetType(page_object) != FPDF_PAGEOBJ_PATH) {
    return false;
  }

  const int mark_count = FPDFPageObj_CountMarks(page_object);
  for (int i = 0; i < mark_count; ++i) {
    FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, i);
    const std::string name = base::UTF16ToUTF8(GetPageObjectMarkName(mark));
    if (name == kInkAnnotationIdentifierKeyV2) {
      return true;
    }
  }

  return false;
}

// Returns the location of `segment` in PDF coordinates.
gfx::PointF GetSegmentPoint(FPDF_PATHSEGMENT segment) {
  float x;
  float y;
  bool result = FPDFPathSegment_GetPoint(segment, &x, &y);
  CHECK(result);
  return {x, y};
}

// Applies `transform` to `point`. Returns the result as an ink::Point, for use
// with Ink code. Although the actual transform depends on the values in
// `transform` and `point`, this should always be used to convert PDF
// coordinates to canonical coordinates.
ink::Point GetTransformedInkPoint(const gfx::Transform& transform,
                                  const gfx::PointF& point) {
  return InkPointFromGfxPoint(transform.MapPoint(point));
}

// Wrapper around ink::CreateMeshFromPolyline() to convert `polyline` into an
// ink::Mesh. It applies an additional check to make sure the points in
// `polyline` have sane values.
std::optional<ink::Mesh> CreateInkMeshFromPolyline(
    base::span<const ink::Point> polyline) {
  // Limit for ink::Point values in pixels. It is divided by 2 because the limit
  // extends half way in the negative range.
  constexpr int kInkPointDimensionLimit =
      kMaxPdfDimensionInches * printing::kPixelsPerInch / 2;
  for (const auto& pt : polyline) {
    if (pt.x < -kInkPointDimensionLimit || pt.x > kInkPointDimensionLimit ||
        pt.y < -kInkPointDimensionLimit || pt.y > kInkPointDimensionLimit) {
      return std::nullopt;
    }
  }

  auto mesh = ink::CreateMeshFromPolyline(polyline);
  if (!mesh.ok()) {
    return std::nullopt;
  }

  return *mesh;
}

std::optional<ink::PartitionedMesh> ReadV2InkModeledShapeFromPath(
    FPDF_PAGEOBJECT path,
    const gfx::Transform& transform) {
  CHECK_EQ(FPDFPageObj_GetType(path), FPDF_PAGEOBJ_PATH);

  const int segment_count = FPDFPath_CountSegments(path);
  if (segment_count <= 0) {
    return std::nullopt;
  }

  std::vector<ink::Point> polyline;
  polyline.reserve(segment_count);

  {
    // The first segment must be a move. Check it outside of the for-loop to
    // avoid an extra conditional for all other segments. This segment must
    // exist because `segment_count` has already been checked.
    FPDF_PATHSEGMENT segment = FPDFPath_GetPathSegment(path, 0);
    CHECK(segment);
    const int type = FPDFPathSegment_GetType(segment);
    if (type != FPDF_SEGMENT_MOVETO) {
      return std::nullopt;
    }

    gfx::PointF point = GetSegmentPoint(segment);
    polyline.push_back(GetTransformedInkPoint(transform, point));
  }

  for (int i = 1; i < segment_count; ++i) {
    FPDF_PATHSEGMENT segment = FPDFPath_GetPathSegment(path, i);
    CHECK(segment);

    // Remaining entries must all be line-to segments, as "V2" shapes only have
    // a single mesh inside.
    if (FPDFPathSegment_GetType(segment) != FPDF_SEGMENT_LINETO) {
      return std::nullopt;
    }

    gfx::PointF point = GetSegmentPoint(segment);
    polyline.push_back(GetTransformedInkPoint(transform, point));
  }

  std::optional<ink::Mesh> mesh = CreateInkMeshFromPolyline(polyline);
  if (!mesh.has_value()) {
    return std::nullopt;
  }

  // Note that `shape` only has enough data for use with ink::Intersects(). It
  // has no outline.
  auto shape =
      ink::PartitionedMesh::FromMeshes(base::span_from_ref(mesh.value()),
                                       /*outlines=*/{});
  if (!shape.ok()) {
    return std::nullopt;
  }

  return *shape;
}

}  // namespace

bool PageContainsV2InkPath(FPDF_PAGE page) {
  if (!page) {
    return false;
  }

  const int page_object_count = FPDFPage_CountObjects(page);
  for (int i = 0; i < page_object_count; ++i) {
    if (IsV2InkPath(FPDFPage_GetObject(page, i))) {
      return true;
    }
  }
  return false;
}

std::vector<ReadV2InkPathResult> ReadV2InkPathsFromPageAsModeledShapes(
    FPDF_PAGE page) {
  std::vector<ReadV2InkPathResult> results;
  if (!page) {
    return results;
  }

  // Get the intersection between the page's MediaBox and CropBox, to find
  // the translation offset for the shapes' transform.
  FS_RECTF bounding_box;
  auto result = FPDF_GetPageBoundingBox(page, &bounding_box);
  CHECK(result);
  const gfx::Vector2dF offset(bounding_box.left, bounding_box.bottom);

  const gfx::Transform transform =
      GetCanonicalToPdfTransform(
          {FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page)},
          GetPageRotation(page).value_or(PageRotation::kRotate0), offset)
          .GetCheckedInverse();

  const int page_object_count = FPDFPage_CountObjects(page);
  for (int i = 0; i < page_object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    if (!IsV2InkPath(page_object)) {
      continue;
    }

    std::optional<ink::PartitionedMesh> shape =
        ReadV2InkModeledShapeFromPath(page_object, transform);
    if (!shape.has_value()) {
      continue;
    }
    results.emplace_back(page_object, std::move(shape.value()));
  }
  return results;
}

std::optional<ink::Mesh> CreateInkMeshFromPolylineForTesting(  // IN-TEST
    base::span<const ink::Point> polyline) {
  return CreateInkMeshFromPolyline(polyline);
}

}  // namespace chrome_pdf
