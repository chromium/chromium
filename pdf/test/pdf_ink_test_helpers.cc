// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/pdf_ink_test_helpers.h"

#include <array>
#include <ostream>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "pdf/pdf_ink_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

// All possible variations of Ink feature params.
constexpr InkTestVariation kInkTestVariationNoTextSupport{
    /*use_text_annotations=*/false};
constexpr InkTestVariation kInkTestVariationTextSupport{
    /*use_text_annotations=*/true};

// Variations of Ink tests to cover all features in development.
constexpr auto kInkTestVariations = std::to_array<InkTestVariation>({
    kInkTestVariationNoTextSupport,
    kInkTestVariationTextSupport,
});

std::string GetAnnotationModeMessageString(InkAnnotationMode mode) {
  switch (mode) {
    case InkAnnotationMode::kOff:
      return "off";
    case InkAnnotationMode::kDraw:
      return "draw";
    case InkAnnotationMode::kText:
      return "text";
  }
  NOTREACHED();
}

}  // namespace

std::optional<ink::StrokeInputBatch> CreateInkInputBatch(
    base::span<const PdfInkInputData> inputs) {
  ink::StrokeInputBatch input_batch;
  for (const auto& input : inputs) {
    auto result = input_batch.Append(CreateInkStrokeInput(
        ink::StrokeInput::ToolType::kMouse, input.position, input.time));
    if (!result.ok()) {
      return std::nullopt;
    }
  }
  return input_batch;
}

base::DictValue CreateSetAnnotationBrushMessageForTesting(
    std::string_view type,
    const TestAnnotationBrushMessageParams* params) {
  base::DictValue data;
  data.Set("type", type);
  if (params) {
    data.Set("color",
             base::DictValue()
                 .Set("r", static_cast<int>(SkColorGetR(params->color)))
                 .Set("g", static_cast<int>(SkColorGetG(params->color)))
                 .Set("b", static_cast<int>(SkColorGetB(params->color))));
    data.Set("size", params->size);
  }

  return base::DictValue()
      .Set("type", "setAnnotationBrush")
      .Set("data", std::move(data));
}

base::DictValue CreateSetAnnotationModeMessageForTesting(
    InkAnnotationMode mode) {
  return base::DictValue()
      .Set("type", "setAnnotationMode")
      .Set("mode", GetAnnotationModeMessageString(mode));
}

base::DictValue CreateSetAnnotationUndoRedoMessageForTesting(
    TestAnnotationUndoRedoMessageType type) {
  base::DictValue message;
  switch (type) {
    case TestAnnotationUndoRedoMessageType::kUndo:
      return base::DictValue().Set("type", "annotationUndo");
    case TestAnnotationUndoRedoMessageType::kRedo:
      return base::DictValue().Set("type", "annotationRedo");
  }
  NOTREACHED();
}

base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename) {
  return base::FilePath(FILE_PATH_LITERAL("ink")).Append(filename);
}

base::span<const InkTestVariation> GetAllInkTestVariations() {
  return kInkTestVariations;
}

void PrintTo(const InkTextInfo& info, std::ostream* os) {
  *os << "{\n  font_id=" << info.font_id
      << ", is_horizontal=" << base::ToString(info.is_horizontal)
      << ",\n  location=" << info.location.ToString()
      << ",\n  glyphs=" << testing::PrintToString(info.glyphs)
      << ",\n  glyph_positions=" << testing::PrintToString(info.glyph_positions)
      << "\n}";
}

void PrintTo(const InkTextBoxAttributes& info, std::ostream* os) {
  std::string_view typeface;
  switch (info.typeface) {
    case TextTypeface::kSansSerif:
      typeface = "Sans Serif (0)";
      break;
    case TextTypeface::kSerif:
      typeface = "Serif (1)";
      break;
    case TextTypeface::kMonospace:
      typeface = "Monospace (2)";
      break;
    default:
      NOTREACHED();
  }

  std::string_view alignment;
  switch (info.alignment) {
    case TextAlignment::kLeft:
      alignment = "Left (0)";
      break;
    case TextAlignment::kCenter:
      alignment = "Center (1)";
      break;
    case TextAlignment::kRight:
      alignment = "Right (2)";
      break;
    default:
      NOTREACHED();
  }

  const SkColor color = info.color;
  *os << "{" << "\n"
      << "\t" << "rect=" << info.rect.ToString() << ",\n"
      << "\t" << "color (RGBA)=(" << SkColorGetR(color) << ", "
      << SkColorGetG(color) << ", " << SkColorGetB(color) << ", "
      << SkColorGetA(color) << "),\n"
      << "\t" << "css_font_size=" << info.css_font_size << ",\n"
      << "\t" << "typeface=" << typeface << ",\n"
      << "\t" << "alignment=" << alignment << ",\n"
      << "\t" << "orientation=" << info.orientation << ",\n"
      << "\t" << "is_bold=" << base::ToString(info.is_bold) << ", "
      << "\t" << "is_italic=" << base::ToString(info.is_italic) << ",\n"
      << "\t" << "text=" << info.text << "\n}";
}

}  // namespace chrome_pdf
