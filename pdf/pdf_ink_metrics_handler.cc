// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_metrics_handler.h"

#include "base/metrics/histogram_functions.h"
#include "pdf/pdf_ink_brush.h"

namespace chrome_pdf {

namespace {

void ReportStrokeType(StrokeMetricBrushType type) {
  base::UmaHistogramEnumeration("PDF.Ink2StrokeBrushType", type);
}

}  // namespace

void ReportDrawStroke(PdfInkBrush::Type type) {
  bool is_pen = type == PdfInkBrush::Type::kPen;
  ReportStrokeType(is_pen ? StrokeMetricBrushType::kPen
                          : StrokeMetricBrushType::kHighlighter);
}

void ReportEraseStroke() {
  ReportStrokeType(StrokeMetricBrushType::kEraser);
}

}  // namespace chrome_pdf
