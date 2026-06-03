// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <map>
#include <optional>
#include <set>
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
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_ink_transform.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/geometry/mesh.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/geometry/tessellator.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace chrome_pdf {

namespace {

// LINT.IfChange(TextSizes)
constexpr float kMinFontSize = 6.0f;
constexpr float kMaxFontSize = 100.0f;
// LINT.ThenChange(//chrome/browser/resources/pdf/elements/ink_annotation_text_mixin.ts:TextSizes)

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

std::vector<FPDF_PAGEOBJECTMARK> FindInkTextAnnotationMarks(
    FPDF_PAGEOBJECT page_object) {
  std::vector<FPDF_PAGEOBJECTMARK> marks;
  if (FPDFPageObj_GetType(page_object) != FPDF_PAGEOBJ_TEXT) {
    return marks;
  }

  const int mark_count = FPDFPageObj_CountMarks(page_object);
  for (int i = 0; i < mark_count; ++i) {
    FPDF_PAGEOBJECTMARK mark = FPDFPageObj_GetMark(page_object, i);
    if (base::UTF16ToUTF8(GetPageObjectMarkName(mark)) ==
        kInkTextAnnotationIdentifierKey) {
      marks.push_back(mark);
    }
  }
  return marks;
}

bool IsValidTypeface(std::optional<int> typeface) {
  return typeface.has_value() &&
         typeface.value() >= static_cast<int>(TextTypeface::kFirst) &&
         typeface.value() <= static_cast<int>(TextTypeface::kLast);
}

bool IsValidAlignment(std::optional<int> alignment) {
  return alignment.has_value() &&
         alignment.value() >= static_cast<int>(TextAlignment::kFirst) &&
         alignment.value() <= static_cast<int>(TextAlignment::kLast);
}

bool IsValidOrientation(std::optional<int> orientation) {
  return orientation.has_value() && orientation.value() >= 0 &&
         orientation.value() <= 3;
}

// Extracts textbox attributes from `mark`. Returns `std::nullopt` if any
// required parameters are invalid.
std::optional<InkTextBoxAttributes> ExtractAttributesFromMark(
    FPDF_PAGEOBJECT page_object,
    FPDF_PAGEOBJECTMARK mark) {
  // Read the metadata in `mark`.
  std::optional<int> version = GetPageObjectMarkIntParam(mark, "Version");
  if (!version.has_value() || version.value() != kInkTextAnnotationVersion) {
    return std::nullopt;
  }

  std::optional<float> x = GetPageObjectMarkFloatParam(mark, "BoundsX");
  std::optional<float> y = GetPageObjectMarkFloatParam(mark, "BoundsY");
  std::optional<float> w = GetPageObjectMarkFloatParam(mark, "BoundsWidth");
  std::optional<float> h = GetPageObjectMarkFloatParam(mark, "BoundsHeight");
  if (!x.has_value() || !y.has_value() || !w.has_value() || !h.has_value()) {
    return std::nullopt;
  }

  std::optional<int> typeface = GetPageObjectMarkIntParam(mark, "Typeface");
  std::optional<int> alignment = GetPageObjectMarkIntParam(mark, "Alignment");
  std::optional<int> orientation =
      GetPageObjectMarkIntParam(mark, "Orientation");
  if (!IsValidTypeface(typeface) || !IsValidAlignment(alignment) ||
      !IsValidOrientation(orientation)) {
    return std::nullopt;
  }

  std::optional<int> bold = GetPageObjectMarkIntParam(mark, "IsBold");
  std::optional<int> italic = GetPageObjectMarkIntParam(mark, "IsItalic");
  if (!bold.has_value() || !italic.has_value()) {
    return std::nullopt;
  }

  // Read the font color and size directly from `page_object`.
  //
  // `a` must be 255.
  unsigned int r;
  unsigned int g;
  unsigned int b;
  unsigned int a;
  if (!FPDFPageObj_GetFillColor(page_object, &r, &g, &b, &a) || a != 255) {
    return std::nullopt;
  }

  float pdf_font_size;
  if (!FPDFTextObj_GetFontSize(page_object, &pdf_font_size)) {
    return std::nullopt;
  }

  const float css_font_size = PdfFontSizeToCssFontSize(pdf_font_size);
  if (css_font_size < kMinFontSize || css_font_size > kMaxFontSize) {
    return std::nullopt;
  }

  std::optional<std::u16string> text =
      GetPageObjectMarkStringParam(mark, "Text");
  if (!text.has_value() || text.value().empty()) {
    return std::nullopt;
  }

  // Convert the textbox bounds from PDF points to CSS page pixels.
  gfx::RectF bounds{x.value(), y.value(), w.value(), h.value()};
  bounds.Scale(1.0f / printing::kUnitConversionFactorPixelsToPoints);

  const bool is_bold = bold.value() != 0;
  const bool is_italic = italic.value() != 0;
  return InkTextBoxAttributes(bounds, SkColorSetRGB(r, g, b), css_font_size,
                              static_cast<TextTypeface>(typeface.value()),
                              static_cast<TextAlignment>(alignment.value()),
                              orientation.value(), is_bold, is_italic,
                              base::UTF16ToUTF8(text.value()));
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

  const gfx::Transform transform =
      GetCanonicalToPdfTransformForPage(page).GetCheckedInverse();
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

ReadInkTextResult::ReadInkTextResult(InkTextBox textbox,
                                     std::vector<FPDF_PAGEOBJECT> text_objects)
    : textbox(std::move(textbox)), text_objects(std::move(text_objects)) {}

ReadInkTextResult::ReadInkTextResult(ReadInkTextResult&&) noexcept = default;

ReadInkTextResult& ReadInkTextResult::operator=(ReadInkTextResult&&) noexcept =
    default;

ReadInkTextResult::~ReadInkTextResult() = default;

bool PageContainsInkTextAnnotation(FPDF_PAGE page) {
  if (!page) {
    return false;
  }

  const int page_object_count = FPDFPage_CountObjects(page);
  for (int i = 0; i < page_object_count; ++i) {
    if (!FindInkTextAnnotationMarks(FPDFPage_GetObject(page, i)).empty()) {
      return true;
    }
  }
  return false;
}

std::vector<ReadInkTextResult> ReadInkTextAnnotationsFromPage(FPDF_PAGE page) {
  struct TextboxData {
    std::optional<InkTextBoxAttributes> attributes;
    std::vector<FPDF_PAGEOBJECT> page_objects;
    FPDF_PAGEOBJECTMARK mark = nullptr;
  };

  std::vector<ReadInkTextResult> results;
  if (!page) {
    return results;
  }

  // Collect invalid textbox IDs to ignore them.
  std::set<int> invalid_ids;

  // Maps textbox IDs to their loading data.
  std::map<int, TextboxData> ids_to_data;

  const int page_object_count = FPDFPage_CountObjects(page);
  for (int i = 0; i < page_object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    std::vector<FPDF_PAGEOBJECTMARK> marks =
        FindInkTextAnnotationMarks(page_object);
    if (marks.empty()) {
      continue;
    }

    if (marks.size() > 1) {
      // If a text object has multiple marks with TextboxId, it is invalid.
      // Collect all associated textbox IDs to ensure they are completely
      // invalidated and skipped, rather than partially loaded from other
      // objects.
      for (FPDF_PAGEOBJECTMARK mark : marks) {
        std::optional<int> id = GetPageObjectMarkIntParam(mark, "TextboxId");
        if (id.has_value()) {
          invalid_ids.insert(id.value());
        }
      }
      continue;
    }

    FPDF_PAGEOBJECTMARK mark = marks[0];
    std::optional<int> id = GetPageObjectMarkIntParam(mark, "TextboxId");
    if (!id.has_value()) {
      continue;
    }

    int textbox_id = id.value();
    if (invalid_ids.contains(textbox_id)) {
      continue;
    }

    auto& data = ids_to_data[textbox_id];
    data.page_objects.push_back(page_object);

    // Extract attributes from the mark.
    std::optional<InkTextBoxAttributes> attributes =
        ExtractAttributesFromMark(page_object, mark);
    if (attributes.has_value()) {
      if (data.attributes.has_value()) {
        // Note: when a single mark spans multiple page objects, that same
        // `mark` pointer appears multiple times in this loop. So if this is the
        // second time the mark was processed just skip it.
        if (data.mark != mark) {
          // The same textbox_id was used in multiple separate marks
          invalid_ids.insert(textbox_id);
        }
      } else {
        data.attributes = std::move(attributes.value());
        data.mark = mark;
      }
    }
  }

  for (auto& [textbox_id, data] : ids_to_data) {
    if (data.attributes.has_value() && !invalid_ids.contains(textbox_id)) {
      results.emplace_back(
          InkTextBox(textbox_id, std::move(data.attributes.value())),
          std::move(data.page_objects));
    }
  }

  return results;
}

}  // namespace chrome_pdf
