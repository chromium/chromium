// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <optional>

#include "base/check.h"
#include "base/containers/span.h"
#include "pdf/pdf_ink_conversions.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"

using printing::kPixelsPerInch;
using printing::kPointsPerInch;

namespace chrome_pdf {

namespace {

// TODO(crbug.com/353904284):  Choose real marker name that doesn't conflict
// with other writers.
constexpr char kInkAnnotationIdentifierKey[] = "ink-annot-id";

// Wrapper around an ink::ModeledShape to allow for iterating through all the
// triangles that make up its many meshes.
class TriangleIterator {
 public:
  explicit TriangleIterator(const ink::ModeledShape& shape)
      : meshes_(shape.Meshes()) {}

  std::optional<ink::Triangle> GetAndAdvance() {
    if (mesh_index_ == meshes_.size()) {
      return std::nullopt;
    }

    // Get the triangle to be returned.
    ink::Triangle triangle = meshes_[mesh_index_].GetTriangle(triangle_index_);

    // Advance to next triangle in preparation for the next call.  When all
    // triangles of a mesh have been consumed, advance to the next mesh.  Meshes
    // are guaranteed by ink::ModeledShape to never be empty.
    ++triangle_index_;
    if (triangle_index_ == meshes_[mesh_index_].TriangleCount()) {
      ++mesh_index_;
      triangle_index_ = 0;
    }
    return triangle;
  }

 private:
  const base::span<const ink::Mesh> meshes_;
  size_t mesh_index_ = 0;
  uint32_t triangle_index_ = 0;
};

ScopedFPDFPageObject WriteShapeToNewPathOnPage(const ink::ModeledShape& shape,
                                               FPDF_PAGE page) {
  CHECK(page);

  // A shape is made up of meshes, which in turn are made up of triangles.
  // All of these get combined into a single PDF path.  The first triangle is
  // special because its first point is used to create the path.
  TriangleIterator triangle_iter(shape);
  std::optional<ink::Triangle> triangle = triangle_iter.GetAndAdvance();
  if (!triangle.has_value()) {
    return nullptr;  // No meshes with actual shape data.
  }

  ScopedFPDFPageObject path(
      FPDFPageObj_CreateNewPath(triangle.value().p0.x, triangle.value().p0.y));
  CHECK(path);

  // Outline the edges of the first triangle.
  bool result =
      FPDFPath_LineTo(path.get(), triangle.value().p1.x, triangle.value().p1.y);
  CHECK(result);
  result =
      FPDFPath_LineTo(path.get(), triangle.value().p2.x, triangle.value().p2.y);
  CHECK(result);

  // Work through the remaining triangles, which are part of the same path.
  for (triangle = triangle_iter.GetAndAdvance(); triangle.has_value();
       triangle = triangle_iter.GetAndAdvance()) {
    result = FPDFPath_MoveTo(path.get(), triangle.value().p0.x,
                             triangle.value().p0.y);
    CHECK(result);
    result = FPDFPath_LineTo(path.get(), triangle.value().p1.x,
                             triangle.value().p1.y);
    CHECK(result);
    result = FPDFPath_LineTo(path.get(), triangle.value().p2.x,
                             triangle.value().p2.y);
    CHECK(result);
  }

  // All triangles of the shape completed.  Initialize the path's transform,
  // draw mode, and color.
  // The transform converts from canonical coordinates (which has a top-left
  // origin and a different DPI), to PDF coordinates (which has a bottom-left
  // origin).
  constexpr float kScreenToPageScale =
      static_cast<float>(kPointsPerInch) / kPixelsPerInch;
  FS_MATRIX transform{kScreenToPageScale,  0, 0,
                      -kScreenToPageScale, 0, FPDF_GetPageHeightF(page)};
  FPDFPageObj_TransformF(path.get(), &transform);

  result = FPDFPath_SetDrawMode(path.get(), FPDF_FILLMODE_WINDING,
                                /*stroke=*/false);
  CHECK(result);

  // Path completed, close and mark it with an ID.
  result = FPDFPath_Close(path.get());
  CHECK(result);

  return path;
}

void SetBrushPropertiesForPath(const ink::Brush& brush, FPDF_PAGEOBJECT path) {
  // TODO(crbug.com/353942910) Write out the brush type and size.
  SkColor color = GetSkColorFromInkBrush(brush);
  bool result =
      FPDFPageObj_SetFillColor(path, SkColorGetR(color), SkColorGetG(color),
                               SkColorGetB(color), SkColorGetA(color));
  CHECK(result);
}

}  // namespace

bool WriteStrokeToPage(FPDF_DOCUMENT document,
                       FPDF_PAGE page,
                       const ink::Stroke& stroke) {
  if (!document || !page) {
    return false;
  }

  ScopedFPDFPageObject path =
      WriteShapeToNewPathOnPage(stroke.GetShape(), page);
  if (!path) {
    return false;
  }

  FPDF_PAGEOBJECTMARK mark =
      FPDFPageObj_AddMark(path.get(), kInkAnnotationIdentifierKey);
  CHECK(mark);

  SetBrushPropertiesForPath(stroke.GetBrush(), path.get());

  // Path is ready for the page.
  FPDFPage_InsertObject(page, path.release());

  return true;
}

}  // namespace chrome_pdf
