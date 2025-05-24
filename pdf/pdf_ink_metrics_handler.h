// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_METRICS_HANDLER_H_
#define PDF_PDF_INK_METRICS_HANDLER_H_

#include "pdf/buildflags.h"
#include "pdf/pdf_ink_brush.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFInk2StrokeBrushSize)
enum class StrokeMetricBrushSize {
  kExtraThin = 0,
  kThin = 1,
  kMedium = 2,
  kThick = 3,
  kExtraThick = 4,
  kMaxValue = 4,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2StrokeBrushSize)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFInk2StrokeBrushType)
enum class StrokeMetricBrushType {
  kPen = 0,
  kHighlighter = 1,
  kEraser = 2,
  kMaxValue = 2,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2StrokeBrushType)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFInk2StrokeHighlighterColor)
enum class StrokeMetricHighlighterColor {
  kLightRed = 0,
  kLightYellow = 1,
  kLightGreen = 2,
  kLightBlue = 3,
  kLightOrange = 4,
  kRed = 5,
  kYellow = 6,
  kGreen = 7,
  kBlue = 8,
  kOrange = 9,
  kMaxValue = 9,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2StrokeHighlighterColor)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFInk2StrokeInputDeviceType)
enum class StrokeMetricInputDeviceType {
  kMouse = 0,
  kTouch = 1,
  kPen = 2,
  kMaxValue = 2,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2StrokeInputDeviceType)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFInk2StrokePenColor)
enum class StrokeMetricPenColor {
  kBlack = 0,
  kDarkGrey2 = 1,
  kDarkGrey1 = 2,
  kLightGrey = 3,
  kWhite = 4,
  kRed1 = 5,
  kYellow1 = 6,
  kGreen1 = 7,
  kBlue1 = 8,
  kTan1 = 9,
  kRed2 = 10,
  kYellow2 = 11,
  kGreen2 = 12,
  kBlue2 = 13,
  kTan2 = 14,
  kRed3 = 15,
  kYellow3 = 16,
  kGreen3 = 17,
  kBlue3 = 18,
  kTan3 = 19,
  kMaxValue = 19,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInk2StrokePenColor)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PDFLoadedWithV2InkAnnotations)
enum class PDFLoadedWithV2InkAnnotations {
  kUnknown = 0,
  kTrue = 1,
  kFalse = 2,
  kMaxValue = kFalse,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFLoadedWithV2InkAnnotations)

void ReportDrawStroke(PdfInkBrush::Type type,
                      const ink::Brush& brush,
                      ink::StrokeInput::ToolType tool_type);

void ReportEraseStroke(ink::StrokeInput::ToolType tool_type);

void ReportTextHighlight(const ink::Brush& brush,
                         ink::StrokeInput::ToolType tool_type);

void RecordPdfLoadedWithV2InkAnnotations(
    PDFLoadedWithV2InkAnnotations loaded_with_annotations);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_METRICS_HANDLER_H_
