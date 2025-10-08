// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <optional>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "pdf/pdf_ink_constants.h"
#include "pdf/pdf_ink_conversions.h"
#include "pdf/pdf_transform.h"
#include "pdf/pdfium/pdfium_ink_transform.h"
#include "third_party/ink/src/ink/brush/brush_coat.h"
#include "third_party/ink/src/ink/brush/brush_tip.h"
#include "third_party/ink/src/ink/geometry/mesh.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

namespace chrome_pdf {

namespace {

// Wrapper around an `ink::PartitionedMesh` to iterate through all the outlines
// that make up the shape.
class ModeledShapeOutlinesIterator {
 public:
  struct OutlineData {
    uint32_t group_index;
    // Guaranteeded to be non-empty.
    // TODO(367764863) Rewrite to base::raw_span.
    RAW_PTR_EXCLUSION base::span<const ink::VertexIndexPair> outline;
  };

  explicit ModeledShapeOutlinesIterator(const ink::PartitionedMesh& shape)
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
  const raw_ref<const ink::PartitionedMesh> shape_;
  uint32_t group_index_ = 0;
  uint32_t outline_index_ = 0;
};

gfx::PointF GetVertexPosition(base::span<const ink::Mesh> meshes,
                              const ink::VertexIndexPair& vertex_index_pair) {
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
    const ink::PartitionedMesh& shape,
    const ModeledShapeOutlinesIterator::OutlineData& outline_data,
    const gfx::Transform& transform) {
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

  // Path completed. Close it, mark it, and return it.
  bool result = FPDFPath_SetDrawMode(path.get(), FPDF_FILLMODE_WINDING,
                                     /*stroke=*/false);
  CHECK(result);
  result = FPDFPath_Close(path.get());
  CHECK(result);
  FPDF_PAGEOBJECTMARK mark =
      FPDFPageObj_AddMark(path.get(), kInkAnnotationIdentifierKeyV2);
  CHECK(mark);

  return path;
}

std::vector<ScopedFPDFPageObject> WriteShapeToNewPathsOnPage(
    const ink::PartitionedMesh& shape,
    FPDF_PAGE page) {
  CHECK(page);

  const gfx::Transform transform = GetCanonicalToPdfTransformForPage(page);
  std::vector<ScopedFPDFPageObject> results;
  ModeledShapeOutlinesIterator it(shape);
  for (std::optional<ModeledShapeOutlinesIterator::OutlineData> outline_data =
           it.GetAndAdvance();
       outline_data.has_value(); outline_data = it.GetAndAdvance()) {
    results.push_back(CreatePathFromOutlineData(
        page, shape, outline_data.value(), transform));
  }

  return results;
}

void SetBrushPropertiesForPath(const ink::Brush& brush, FPDF_PAGEOBJECT path) {
  const SkColor color = GetSkColorFromInkBrush(brush);
  CHECK_EQ(SkColorGetA(color), SK_AlphaOPAQUE);

  // Ink says this can have a value up to 2.0f, but that should never be the
  // case, as //pdf code never sets it that high.
  const float opacity_multiplier = GetOpacityMultiplierFromBrush(brush);
  CHECK_LE(opacity_multiplier, 1.0f);

  bool result =
      FPDFPageObj_SetFillColor(path, SkColorGetR(color), SkColorGetG(color),
                               SkColorGetB(color), opacity_multiplier * 255);
  CHECK(result);
}

}  // namespace

std::vector<FPDF_PAGEOBJECT> WriteStrokeToPage(FPDF_PAGE page,
                                               const ink::Stroke& stroke) {
  std::vector<FPDF_PAGEOBJECT> results;
  if (!page) {
    return results;
  }

  std::vector<ScopedFPDFPageObject> paths =
      WriteShapeToNewPathsOnPage(stroke.GetShape(), page);
  results.reserve(paths.size());
  for (auto& path : paths) {
    FPDF_PAGEOBJECT page_obj = path.get();
    SetBrushPropertiesForPath(stroke.GetBrush(), page_obj);

    // Path is ready for the page.
    FPDFPage_InsertObject(page, path.release());

    results.push_back(page_obj);
  }

  return results;
}

}  // namespace chrome_pdf
