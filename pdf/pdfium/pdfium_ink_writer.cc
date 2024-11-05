// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <optional>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_ink_transform.h"
#include "third_party/ink/src/ink/geometry/mesh.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

namespace {

// TODO(crbug.com/353904284):  Choose real marker name that doesn't conflict
// with other writers.
constexpr char kInkAnnotationIdentifierKey[] = "ink-annot-id";

// Wrapper around an `ink::ModeledShape` to iterate through all the outlines
// that make up the shape.
class ModeledShapeOutlinesIterator {
 public:
  struct OutlineData {
    uint32_t group_index;
    // Guaranteeded to be non-empty.
    // TODO(367764863) Rewrite to base::raw_span.
    RAW_PTR_EXCLUSION base::span<const ink::ModeledShape::VertexIndexPair>
        outline;
  };

  explicit ModeledShapeOutlinesIterator(const ink::ModeledShape& shape)
      : shape_(shape) {}

  std::optional<OutlineData> GetAndAdvance() {
    while (group_index_ < shape_->RenderGroupCount()) {
      if (outline_index_ < shape_->OutlineCount(group_index_)) {
        OutlineData outline_data{
            .group_index = group_index_,
            .outline = shape_->Outline(group_index_, outline_index_),
        };
        ++outline_index_;
        return outline_data;
      }

      ++group_index_;
      outline_index_ = 0;
    }
    return std::nullopt;
  }

 private:
  const raw_ref<const ink::ModeledShape> shape_;
  uint32_t group_index_ = 0;
  uint32_t outline_index_ = 0;
};

gfx::PointF GetVertexPosition(
    base::span<const ink::Mesh> meshes,
    const ink::ModeledShape::VertexIndexPair& vertex_index_pair) {
  ink::Point vertex_position =
      meshes[vertex_index_pair.mesh_index].VertexPosition(
          vertex_index_pair.vertex_index);
  return {vertex_position.x, vertex_position.y};
}

// Creates a path on `page` using `outline_data`.
// `shape` is the object that contains `outline_data`.
// `transform` converts the positions in `outline_data` to PDF coordinates.
//
// The returned page object is always a `FPDF_PAGEOBJ_PATH` and never null.
ScopedFPDFPageObject CreatePathFromOutlineData(
    FPDF_PAGE page,
    const ink::ModeledShape& shape,
    const ModeledShapeOutlinesIterator::OutlineData& outline_data,
    const gfx::AxisTransform2d& transform) {
  CHECK(page);

  base::span<const ink::Mesh> meshes =
      shape.RenderGroupMeshes(outline_data.group_index);
  const auto& first_outline_position = outline_data.outline.front();
  gfx::PointF transformed_vertex_position =
      transform.MapPoint(GetVertexPosition(meshes, first_outline_position));
  ScopedFPDFPageObject path(FPDFPageObj_CreateNewPath(
      transformed_vertex_position.x(), transformed_vertex_position.y()));
  CHECK(path);

  for (const auto& outline_position : outline_data.outline.subspan<1u>()) {
    transformed_vertex_position =
        transform.MapPoint(GetVertexPosition(meshes, outline_position));
    bool result = FPDFPath_LineTo(path.get(), transformed_vertex_position.x(),
                                  transformed_vertex_position.y());
    CHECK(result);
  }

  return path;
}

// Appends `outline_data` to `path`. `shape` and `transform` are the same as
// the CreatePathFromOutline() parameters with the same names.
void AppendOutlineToPath(
    FPDF_PAGEOBJECT path,
    const ink::ModeledShape& shape,
    const ModeledShapeOutlinesIterator::OutlineData& outline_data,
    const gfx::AxisTransform2d& transform) {
  CHECK(path);

  base::span<const ink::Mesh> meshes =
      shape.RenderGroupMeshes(outline_data.group_index);
  const auto& first_outline_position = outline_data.outline.front();
  gfx::PointF transformed_vertex_position =
      transform.MapPoint(GetVertexPosition(meshes, first_outline_position));
  bool result = FPDFPath_MoveTo(path, transformed_vertex_position.x(),
                                transformed_vertex_position.y());
  CHECK(result);

  for (const auto& outline_position : outline_data.outline.subspan<1u>()) {
    transformed_vertex_position =
        transform.MapPoint(GetVertexPosition(meshes, outline_position));
    result = FPDFPath_LineTo(path, transformed_vertex_position.x(),
                             transformed_vertex_position.y());
    CHECK(result);
  }
}

ScopedFPDFPageObject WriteShapeToNewPathOnPage(const ink::ModeledShape& shape,
                                               FPDF_PAGE page) {
  CHECK(page);

  ModeledShapeOutlinesIterator it(shape);
  std::optional<ModeledShapeOutlinesIterator::OutlineData> outline_data =
      it.GetAndAdvance();
  if (!outline_data.has_value()) {
    return nullptr;  // `shape` is empty.
  }

  const gfx::AxisTransform2d transform =
      GetCanonicalToPdfTransform(FPDF_GetPageHeightF(page));

  // Create a path using the first outline.
  ScopedFPDFPageObject path =
      CreatePathFromOutlineData(page, shape, outline_data.value(), transform);

  // Work through the remaining outlines, which are part of the same path.
  for (outline_data = it.GetAndAdvance(); outline_data.has_value();
       outline_data = it.GetAndAdvance()) {
    AppendOutlineToPath(path.get(), shape, outline_data.value(), transform);
  }

  bool result = FPDFPath_SetDrawMode(path.get(), FPDF_FILLMODE_WINDING,
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

FPDF_PAGEOBJECT WriteStrokeToPage(FPDF_DOCUMENT document,
                                  FPDF_PAGE page,
                                  const ink::Stroke& stroke) {
  if (!document || !page) {
    return nullptr;
  }

  ScopedFPDFPageObject path =
      WriteShapeToNewPathOnPage(stroke.GetShape(), page);
  if (!path) {
    return nullptr;
  }

  FPDF_PAGEOBJECT page_obj = path.get();
  FPDF_PAGEOBJECTMARK mark =
      FPDFPageObj_AddMark(page_obj, kInkAnnotationIdentifierKey);
  CHECK(mark);

  SetBrushPropertiesForPath(stroke.GetBrush(), page_obj);

  // Path is ready for the page.
  FPDFPage_InsertObject(page, path.release());

  return page_obj;
}

}  // namespace chrome_pdf
