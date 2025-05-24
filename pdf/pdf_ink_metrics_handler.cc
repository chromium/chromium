// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_metrics_handler.h"

#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_conversions.h"

namespace chrome_pdf {

namespace {

// LINT.IfChange(PenSizes)
constexpr auto kPenSizes =
    base::MakeFixedFlatMap<float, StrokeMetricBrushSize>({
        {1.0f, StrokeMetricBrushSize::kExtraThin},
        {2.0f, StrokeMetricBrushSize::kThin},
        {3.0f, StrokeMetricBrushSize::kMedium},
        {6.0f, StrokeMetricBrushSize::kThick},
        {8.0f, StrokeMetricBrushSize::kExtraThick},
    });
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

// LINT.IfChange(PenColors)
constexpr auto kPenColors =
    base::MakeFixedFlatMap<SkColor, StrokeMetricPenColor>({
        {SK_ColorBLACK, StrokeMetricPenColor::kBlack},
        {SkColorSetRGB(0x5F, 0x63, 0x68), StrokeMetricPenColor::kDarkGrey2},
        {SkColorSetRGB(0x9A, 0xA0, 0xA6), StrokeMetricPenColor::kDarkGrey1},
        {SkColorSetRGB(0xDA, 0xDC, 0xE0), StrokeMetricPenColor::kLightGrey},
        {SK_ColorWHITE, StrokeMetricPenColor::kWhite},
        {SkColorSetRGB(0xF2, 0x8B, 0x82), StrokeMetricPenColor::kRed1},
        {SkColorSetRGB(0xFD, 0xD6, 0x63), StrokeMetricPenColor::kYellow1},
        {SkColorSetRGB(0x81, 0xC9, 0x95), StrokeMetricPenColor::kGreen1},
        {SkColorSetRGB(0x8A, 0xB4, 0xF8), StrokeMetricPenColor::kBlue1},
        {SkColorSetRGB(0xEE, 0xC9, 0xAE), StrokeMetricPenColor::kTan1},
        {SkColorSetRGB(0xEA, 0x43, 0x35), StrokeMetricPenColor::kRed2},
        {SkColorSetRGB(0xFB, 0xBC, 0x04), StrokeMetricPenColor::kYellow2},
        {SkColorSetRGB(0x34, 0xA8, 0x53), StrokeMetricPenColor::kGreen2},
        {SkColorSetRGB(0x42, 0x85, 0xF4), StrokeMetricPenColor::kBlue2},
        {SkColorSetRGB(0xE2, 0xA1, 0x85), StrokeMetricPenColor::kTan2},
        {SkColorSetRGB(0xC5, 0x22, 0x1F), StrokeMetricPenColor::kRed3},
        {SkColorSetRGB(0xF2, 0x99, 0x00), StrokeMetricPenColor::kYellow3},
        {SkColorSetRGB(0x18, 0x80, 0x38), StrokeMetricPenColor::kGreen3},
        {SkColorSetRGB(0x19, 0x67, 0xD2), StrokeMetricPenColor::kBlue3},
        {SkColorSetRGB(0x88, 0x59, 0x45), StrokeMetricPenColor::kTan3},
    });
// LINT.ThenChange(//chrome/browser/resources/pdf/elements//ink_annotation_brush_mixin.ts:PenColors)

// LINT.IfChange(HighlighterColors)
constexpr auto kHighlighterColors =
    base::MakeFixedFlatMap<SkColor, StrokeMetricHighlighterColor>({
        {SkColorSetRGB(0xF2, 0x8B, 0x82),
         StrokeMetricHighlighterColor::kLightRed},
        {SkColorSetRGB(0xFD, 0xD6, 0x63),
         StrokeMetricHighlighterColor::kLightYellow},
        {SkColorSetRGB(0x34, 0xA8, 0x53),
         StrokeMetricHighlighterColor::kLightGreen},
        {SkColorSetRGB(0x42, 0x85, 0xF4),
         StrokeMetricHighlighterColor::kLightBlue},
        {SkColorSetRGB(0xFF, 0xAE, 0x80),
         StrokeMetricHighlighterColor::kLightOrange},
        {SkColorSetRGB(0xD9, 0x30, 0x25), StrokeMetricHighlighterColor::kRed},
        {SkColorSetRGB(0xDD, 0xF3, 0x00),
         StrokeMetricHighlighterColor::kYellow},
        {SkColorSetRGB(0x25, 0xE3, 0x87), StrokeMetricHighlighterColor::kGreen},
        {SkColorSetRGB(0x53, 0x79, 0xFF), StrokeMetricHighlighterColor::kBlue},
        {SkColorSetRGB(0xFF, 0x63, 0x0C),
         StrokeMetricHighlighterColor::kOrange},
    });
// LINT.ThenChange(//chrome/browser/resources/pdf/elements//ink_annotation_brush_mixin.ts:HighlighterColors)

constexpr char kStrokeInputDeviceMetricName[] = "PDF.Ink2StrokeInputDeviceType";

void ReportStrokeTypeAndMaybeSize(StrokeMetricBrushType type,
                                  std::optional<StrokeMetricBrushSize> size) {
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
      CHECK(!size.has_value());
      return;
  }
  CHECK(size_metric);
  base::UmaHistogramEnumeration(size_metric, size.value());
}

StrokeMetricInputDeviceType GetStrokeInputDeviceType(
    ink::StrokeInput::ToolType tool_type) {
  switch (tool_type) {
    case ink::StrokeInput::ToolType::kUnknown:
      NOTREACHED();
    case ink::StrokeInput::ToolType::kMouse:
      return StrokeMetricInputDeviceType::kMouse;
    case ink::StrokeInput::ToolType::kTouch:
      return StrokeMetricInputDeviceType::kTouch;
    case ink::StrokeInput::ToolType::kStylus:
      return StrokeMetricInputDeviceType::kPen;
  }
  NOTREACHED();
}

}  // namespace

void ReportDrawStroke(PdfInkBrush::Type type,
                      const ink::Brush& brush,
                      ink::StrokeInput::ToolType tool_type) {
  bool is_pen = type == PdfInkBrush::Type::kPen;
  const base::fixed_flat_map<float, StrokeMetricBrushSize, 5>& sizes =
      is_pen ? kPenSizes : kHighlighterSizes;
  auto size_iter = sizes.find(brush.GetSize());
  CHECK(size_iter != sizes.end());
  ReportStrokeTypeAndMaybeSize(is_pen ? StrokeMetricBrushType::kPen
                                      : StrokeMetricBrushType::kHighlighter,
                               size_iter->second);
  base::UmaHistogramEnumeration(kStrokeInputDeviceMetricName,
                                GetStrokeInputDeviceType(tool_type));

  SkColor sk_color = GetSkColorFromInkBrush(brush);
  if (is_pen) {
    auto color_iter = kPenColors.find(sk_color);
    CHECK(color_iter != kPenColors.end());
    base::UmaHistogramEnumeration("PDF.Ink2StrokePenColor", color_iter->second);
  } else {
    auto color_iter = kHighlighterColors.find(sk_color);
    CHECK(color_iter != kHighlighterColors.end());
    base::UmaHistogramEnumeration("PDF.Ink2StrokeHighlighterColor",
                                  color_iter->second);
  }
}

void ReportEraseStroke(ink::StrokeInput::ToolType tool_type) {
  ReportStrokeTypeAndMaybeSize(StrokeMetricBrushType::kEraser, std::nullopt);
  base::UmaHistogramEnumeration(kStrokeInputDeviceMetricName,
                                GetStrokeInputDeviceType(tool_type));
}

void ReportTextHighlight(const ink::Brush& brush,
                         ink::StrokeInput::ToolType tool_type) {
  SkColor sk_color = GetSkColorFromInkBrush(brush);
  auto color_iter = kHighlighterColors.find(sk_color);
  CHECK(color_iter != kHighlighterColors.end());
  base::UmaHistogramEnumeration("PDF.Ink2TextHighlighterColor",
                                color_iter->second);
  base::UmaHistogramEnumeration("PDF.Ink2TextHighlightInputDeviceType",
                                GetStrokeInputDeviceType(tool_type));
}

void RecordPdfLoadedWithV2InkAnnotations(
    PDFLoadedWithV2InkAnnotations loaded_with_annotations) {
  base::UmaHistogramEnumeration("PDF.LoadedWithV2InkAnnotations2",
                                loaded_with_annotations);
}

}  // namespace chrome_pdf
