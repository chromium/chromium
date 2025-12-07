// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/pdf_ink_test_helpers.h"

#include <array>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/values.h"
#include "pdf/pdf_ink_conversions.h"

namespace chrome_pdf {

namespace {

// All possible variations of Ink feature params.
constexpr InkTestVariation kInkTestVariationNoTextSupport{
    /*use_text_annotations=*/false,
    /*use_text_highlighting=*/false};
constexpr InkTestVariation kInkTestVariationTextHighlighting{
    /*use_text_annotations=*/false,
    /*use_text_highlighting=*/true};
constexpr InkTestVariation kInkTestVariationTextHighlightingAndAnnotations{
    /*use_text_annotations=*/true, /*use_text_highlighting=*/true};

// Variations of Ink tests to cover all features in development.
constexpr auto kInkTestVariations = std::to_array<InkTestVariation>({
    kInkTestVariationNoTextSupport,
    kInkTestVariationTextHighlighting,
    kInkTestVariationTextHighlightingAndAnnotations,
});

// Variations of Ink tests with text highlighting enabled.
constexpr auto kInkTestVariationsWithTextHighlighting =
    std::to_array<InkTestVariation>({
        kInkTestVariationTextHighlighting,
        kInkTestVariationTextHighlightingAndAnnotations,
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

base::Value::Dict CreateSetAnnotationBrushMessageForTesting(
    std::string_view type,
    const TestAnnotationBrushMessageParams* params) {
  base::Value::Dict data;
  data.Set("type", type);
  if (params) {
    data.Set("color",
             base::Value::Dict()
                 .Set("r", static_cast<int>(SkColorGetR(params->color)))
                 .Set("g", static_cast<int>(SkColorGetG(params->color)))
                 .Set("b", static_cast<int>(SkColorGetB(params->color))));
    data.Set("size", params->size);
  }

  return base::Value::Dict()
      .Set("type", "setAnnotationBrush")
      .Set("data", std::move(data));
}

base::Value::Dict CreateSetAnnotationModeMessageForTesting(
    InkAnnotationMode mode) {
  return base::Value::Dict()
      .Set("type", "setAnnotationMode")
      .Set("mode", GetAnnotationModeMessageString(mode));
}

base::Value::Dict CreateSetAnnotationUndoRedoMessageForTesting(
    TestAnnotationUndoRedoMessageType type) {
  base::Value::Dict message;
  switch (type) {
    case TestAnnotationUndoRedoMessageType::kUndo:
      return base::Value::Dict().Set("type", "annotationUndo");
    case TestAnnotationUndoRedoMessageType::kRedo:
      return base::Value::Dict().Set("type", "annotationRedo");
  }
  NOTREACHED();
}

base::FilePath GetInkTestDataFilePath(base::FilePath::StringViewType filename) {
  return base::FilePath(FILE_PATH_LITERAL("ink")).Append(filename);
}

base::span<const InkTestVariation> GetAllInkTestVariations() {
  return kInkTestVariations;
}

base::span<const InkTestVariation> GetInkTestVariationsWithTextHighlighting() {
  return kInkTestVariationsWithTextHighlighting;
}

}  // namespace chrome_pdf
