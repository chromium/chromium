// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/pdf_ink_test_helpers.h"

#include <array>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "pdf/mojom/pdf.mojom.h"
#include "pdf/pdf_ink_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

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

base::DictValue CreateEditTextAnnotationMessage(int frontend_id) {
  return base::DictValue()
      .Set("type", "editTextAnnotation")
      .Set("data", frontend_id);
}

base::DictValue CreateFinishTextAnnotationMessage(base::DictValue data) {
  return base::DictValue()
      .Set("type", "finishTextAnnotation")
      .Set("data", std::move(data));
}

base::DictValue SampleTextAttributesDict() {
  base::DictValue text_attributes;
  // Color components values for `kYellow`.
  text_attributes.Set(
      "color", base::DictValue().Set("r", 253).Set("g", 214).Set("b", 99));
  text_attributes.Set("size", 12.0f);
  text_attributes.Set("typeface", "serif");
  text_attributes.Set("alignment", "center");
  text_attributes.Set("styles", base::DictValue()
                                    .Set("bold", true)
                                    .Set("italic", true)
                                    .Set("strikethrough", true));
  return text_attributes;
}

base::DictValue SampleTextBoxRectDict() {
  base::DictValue textbox_rect;
  textbox_rect.Set("locationX", 10.0f);
  textbox_rect.Set("locationY", 20.0f);
  textbox_rect.Set("width", 100.0f);
  textbox_rect.Set("height", 15.0f);
  return textbox_rect;
}

InkTextBoxAttributes SampleInkTextBoxAttributes() {
  return SampleInkTextBoxAttributesWithText("Box 0");
}

InkTextBoxAttributes SampleInkTextBoxAttributesWithText(std::string text) {
  return InkTextBoxAttributes{
      .rect = gfx::RectF(10.0f, 20.0f, 100.0f, 50.0f),
      .color = SkColorSetRGB(0, 0, 255),
      .css_font_size = 12.0f,
      .typeface = TextTypeface::kMonospace,
      .alignment = TextAlignment::kCenter,
      .orientation = 1,
      .viewport_orientation = PageOrientation::kOriginal,
      .is_bold = false,
      .is_italic = true,
      .is_strikethrough = false,
      .text = std::move(text),
  };
}

testing::Matcher<const InkTextBoxAttributes&>
SampleInkTextBoxAttributesMatcher() {
  return SampleInkTextBoxAttributesMatcherWith("hi",
                                               PageOrientation::kOriginal);
}

testing::Matcher<const InkTextBoxAttributes&>
SampleInkTextBoxAttributesMatcherWith(const std::string& text,
                                      PageOrientation viewport_orientation) {
  return testing::Eq(InkTextBoxAttributes{
      .rect = gfx::RectF(10.0f, 20.0f, 100.0f, 15.0f),
      .color = SkColorSetRGB(253, 214, 99),
      .css_font_size = 12.0f,
      .typeface = TextTypeface::kSerif,
      .alignment = TextAlignment::kCenter,
      .orientation = 1,
      .viewport_orientation = viewport_orientation,
      .is_bold = true,
      .is_italic = true,
      .is_strikethrough = true,
      .text = text,
  });
}

base::BlobStorage SampleInkTextInfoBlob(FontId typeface_id) {
  auto mojo_text_info = pdf::mojom::InkTextInfo::New();
  mojo_text_info->effective_zoom = 10.0f;
  mojo_text_info->primary_ascent = 5.0f;
  auto mojo_text_run = pdf::mojom::InkTextRun::New();
  mojo_text_run->location = gfx::RectF(100.0f, 200.0f, 300.0f, 400.0f);
  auto mojo_typeface_run = pdf::mojom::InkTypefaceRun::New();
  mojo_typeface_run->is_horizontal = true;
  mojo_typeface_run->typeface_id = typeface_id.value();
  auto mojo_glyph1 = pdf::mojom::InkGlyphInfo::New();
  mojo_glyph1->glyph = 4;
  auto mojo_glyph2 = pdf::mojom::InkGlyphInfo::New();
  mojo_glyph2->glyph = 5;
  mojo_typeface_run->glyphs.push_back(std::move(mojo_glyph1));
  mojo_typeface_run->glyphs.push_back(std::move(mojo_glyph2));
  mojo_text_run->typeface_runs.push_back(std::move(mojo_typeface_run));
  mojo_text_info->text_runs.push_back(std::move(mojo_text_run));
  return pdf::mojom::InkTextInfo::Serialize(&mojo_text_info);
}

testing::Matcher<const InkTextInfo&> SampleInkTextInfoMatcher(
    FontId typeface_id) {
  return InkTextInfoEq(typeface_id, /*glyphs=*/std::vector<uint32_t>{4, 5},
                       /*glyph_positions=*/std::vector<float>(2),
                       /*location=*/gfx::RectF(10.0f, 20.0f, 30.0f, 40.0f),
                       /*is_horizontal=*/true);
}

testing::Matcher<const InkTextLine&> SampleInkTextLineMatcher(
    FontId typeface_id) {
  return testing::AllOf(
      testing::Field(&InkTextLine::location,
                     gfx::RectF(10.0f, 20.0f, 30.0f, 40.0f)),
      testing::Field(
          &InkTextLine::text_info,
          testing::ElementsAre(SampleInkTextInfoMatcher(typeface_id))));
}

base::DictValue SampleSerializedTypeface(FontId font_id,
                                         base::span<const uint8_t> font_data) {
  return base::DictValue()
      .Set("uniqueId", font_id.value())
      .Set("serializedTypeface", base::Value(font_data))
      .Set("name", base::StringPrintf("sample font_id: %d", font_id.value()));
}

base::DictValue SampleFinishTextAnnotationData(int frontend_id,
                                               FontId font_id,
                                               int page_index,
                                               double pdf_zoom) {
  return SampleFinishTextAnnotationDataWithSource(frontend_id, font_id,
                                                  page_index, pdf_zoom, "user");
}

base::DictValue SampleFinishTextAnnotationDataWithSource(
    int frontend_id,
    FontId font_id,
    int page_index,
    double pdf_zoom,
    std::string_view source) {
  return base::DictValue()
      .Set("id", frontend_id)
      .Set("isEdited", true)
      .Set("mojoTextInfo", SampleInkTextInfoBlob(font_id))
      .Set("newTypefaces", base::ListValue())
      .Set("pageIndex", page_index)
      .Set("pdfZoom", pdf_zoom)
      .Set("source", source)
      .Set("text", "hi")
      .Set("textAttributes", SampleTextAttributesDict())
      .Set("textBoxRect", SampleTextBoxRectDict())
      .Set("textOrientation", 1)
      .Set("viewportOrientation", 0);
}

base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename) {
  return base::FilePath(FILE_PATH_LITERAL("ink")).Append(filename);
}

base::span<const InkTestVariation> GetAllInkTestVariations() {
  return kInkTestVariations;
}

void PrintTo(const InkTextInfo& info, std::ostream* os) {
  *os << "{\n  font_id=" << info.font_id
      << ",\n  is_horizontal=" << base::ToString(info.is_horizontal)
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
  *os << "{\n  rect=" << info.rect.ToString() << ",\n  color (RGBA)=("
      << SkColorGetR(color) << ", " << SkColorGetG(color) << ", "
      << SkColorGetB(color) << ", " << SkColorGetA(color) << ")"
      << ",\n  css_font_size=" << info.css_font_size
      << ",\n  typeface=" << typeface << ",\n  alignment=" << alignment
      << ",\n  orientation=" << info.orientation << ",\n  viewport_orientation="
      << static_cast<int>(info.viewport_orientation)
      << ",\n  is_bold=" << base::ToString(info.is_bold)
      << ",\n  is_italic=" << base::ToString(info.is_italic)
      << ",\n  is_strikethrough=" << base::ToString(info.is_strikethrough)
      << ",\n  text=" << info.text << "\n}";
}

bool InkTextInfoEquals(const InkTextInfo& lhs, const InkTextInfo& rhs) {
  const bool glyph_positions_eq = std::ranges::equal(
      lhs.glyph_positions, rhs.glyph_positions, [](float lhs, float rhs) {
        return base::IsApproximatelyEqual(lhs, rhs, 0.01f);
      });
  return glyph_positions_eq && lhs.font_id == rhs.font_id &&
         lhs.glyphs == rhs.glyphs && lhs.location == rhs.location &&
         lhs.is_horizontal == rhs.is_horizontal && lhs.text == rhs.text;
}

}  // namespace chrome_pdf
