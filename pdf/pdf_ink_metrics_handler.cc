// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_metrics_handler.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "pdf/pdf_ink_brush.h"

namespace chrome_pdf {

namespace {

// LINT.IfChange(PenSizes)
// LINT.IfChange(EraserSizes)
// Pens and erasers share the same sizes.
constexpr auto kPenAndEraserSizes =
    base::MakeFixedFlatMap<float, StrokeMetricBrushSize>({
        {1.0f, StrokeMetricBrushSize::kExtraThin},
        {2.0f, StrokeMetricBrushSize::kThin},
        {3.0f, StrokeMetricBrushSize::kMedium},
        {6.0f, StrokeMetricBrushSize::kThick},
        {8.0f, StrokeMetricBrushSize::kExtraThick},
    });
// LINT.ThenChange(//chrome/browser/resources/pdf/elements/ink_size_selector.ts:EraserSizes)
// LINT.ThenChange(//chrome/browser/resources/pdf/elements/ink_size_selector.ts:PenSizes)

// LINT.IfChange(HighlighterSizes)
constexpr auto kHighlighterSizes =
    base::MakeFixedFlatMap<float, StrokeMetricBrushSize>({
        {4.0f, StrokeMetricBrushSize::kExtraThin},
        {6.0f, StrokeMetricBrushSize::kThin},
        {8.0f, StrokeMetricBrushSize::kMedium},
        {12.0f, StrokeMetricBrushSize::kThick},
        {16.0f, StrokeMetricBrushSize::kExtraThick},
    });
// LINT.ThenChange(//chrome/browser/resources/pdf/elements/ink_size_selector.ts:HighlighterSizes)

void ReportStrokeTypeAndSize(StrokeMetricBrushType type,
                             StrokeMetricBrushSize size) {
  base::UmaHistogramEnumeration("PDF.Ink2StrokeBrushType", type);
  const char* size_metric = nullptr;
  switch (type) {
    case StrokeMetricBrushType::kPen:
      size_metric = "PDF.Ink2StrokePenSize";
      break;
    case StrokeMetricBrushType::kHighlighter:
      size_metric = "PDF.Ink2StrokeHighlighterSize";
      break;
    case StrokeMetricBrushType::kEraser:
      size_metric = "PDF.Ink2StrokeEraserSize";
      break;
  };
  CHECK(size_metric);
  base::UmaHistogramEnumeration(size_metric, size);
}

}  // namespace

void ReportDrawStroke(PdfInkBrush::Type type, const ink::Brush& brush) {
  bool is_pen = type == PdfInkBrush::Type::kPen;
  const base::fixed_flat_map<float, StrokeMetricBrushSize, 5>& sizes =
      is_pen ? kPenAndEraserSizes : kHighlighterSizes;
  auto size_iter = sizes.find(brush.GetSize());
  CHECK(size_iter != sizes.end());
  ReportStrokeTypeAndSize(is_pen ? StrokeMetricBrushType::kPen
                                 : StrokeMetricBrushType::kHighlighter,
                          size_iter->second);
}

void ReportEraseStroke(float size) {
  auto iter = kPenAndEraserSizes.find(size);
  CHECK(iter != kPenAndEraserSizes.end());
  ReportStrokeTypeAndSize(StrokeMetricBrushType::kEraser, iter->second);
}

}  // namespace chrome_pdf
