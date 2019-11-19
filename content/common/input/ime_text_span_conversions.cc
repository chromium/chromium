// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/ime_text_span_conversions.h"

#include "base/logging.h"

namespace content {

blink::WebImeTextSpan::Type ConvertUiImeTextSpanTypeToWebType(
    ui::ImeTextSpan::Type type) {
  switch (type) {
    case ui::ImeTextSpan::Type::kComposition:
      return blink::WebImeTextSpan::Type::kComposition;
    case ui::ImeTextSpan::Type::kSuggestion:
      return blink::WebImeTextSpan::Type::kSuggestion;
    case ui::ImeTextSpan::Type::kMisspellingSuggestion:
      return blink::WebImeTextSpan::Type::kMisspellingSuggestion;
  }

  NOTREACHED();
  return blink::WebImeTextSpan::Type::kComposition;
}

ui::ImeTextSpan::Type ConvertWebImeTextSpanTypeToUiType(
    blink::WebImeTextSpan::Type type) {
  switch (type) {
    case blink::WebImeTextSpan::Type::kComposition:
      return ui::ImeTextSpan::Type::kComposition;
    case blink::WebImeTextSpan::Type::kSuggestion:
      return ui::ImeTextSpan::Type::kSuggestion;
    case blink::WebImeTextSpan::Type::kMisspellingSuggestion:
      return ui::ImeTextSpan::Type::kMisspellingSuggestion;
  }

  NOTREACHED();
  return ui::ImeTextSpan::Type::kComposition;
}

ui::mojom::ImeTextSpanThickness ConvertUiThicknessToUiImeTextSpanThickness(
    ui::ImeTextSpan::Thickness thickness) {
  switch (thickness) {
    case ui::ImeTextSpan::Thickness::kNone:
      return ui::mojom::ImeTextSpanThickness::kNone;
    case ui::ImeTextSpan::Thickness::kThin:
      return ui::mojom::ImeTextSpanThickness::kThin;
    case ui::ImeTextSpan::Thickness::kThick:
      return ui::mojom::ImeTextSpanThickness::kThick;
  }

  NOTREACHED();
  return ui::mojom::ImeTextSpanThickness::kThin;
}

ui::ImeTextSpan::Thickness ConvertUiImeTextSpanThicknessToUiThickness(
    ui::mojom::ImeTextSpanThickness thickness) {
  switch (thickness) {
    case ui::mojom::ImeTextSpanThickness::kNone:
      return ui::ImeTextSpan::Thickness::kNone;
    case ui::mojom::ImeTextSpanThickness::kThin:
      return ui::ImeTextSpan::Thickness::kThin;
    case ui::mojom::ImeTextSpanThickness::kThick:
      return ui::ImeTextSpan::Thickness::kThick;
  }

  NOTREACHED();
  return ui::ImeTextSpan::Thickness::kThin;
}

blink::WebImeTextSpan ConvertUiImeTextSpanToBlinkImeTextSpan(
    const ui::ImeTextSpan& ui_ime_text_span) {
  blink::WebImeTextSpan blink_ime_text_span = blink::WebImeTextSpan(
      ConvertUiImeTextSpanTypeToWebType(ui_ime_text_span.type),
      ui_ime_text_span.start_offset, ui_ime_text_span.end_offset,
      ConvertUiThicknessToUiImeTextSpanThickness(ui_ime_text_span.thickness),
      ui_ime_text_span.background_color,
      ui_ime_text_span.suggestion_highlight_color,
      ui_ime_text_span.suggestions);
  blink_ime_text_span.underline_color = ui_ime_text_span.underline_color;
  blink_ime_text_span.remove_on_finish_composing =
      ui_ime_text_span.remove_on_finish_composing;
  return blink_ime_text_span;
}

std::vector<blink::WebImeTextSpan> ConvertUiImeTextSpansToBlinkImeTextSpans(
    const std::vector<ui::ImeTextSpan>& ui_ime_text_spans) {
  std::vector<blink::WebImeTextSpan> blink_ime_text_spans;
  for (const auto& ui_ime_text_span : ui_ime_text_spans) {
    blink_ime_text_spans.push_back(
        ConvertUiImeTextSpanToBlinkImeTextSpan(ui_ime_text_span));
  }
  return blink_ime_text_spans;
}

ui::ImeTextSpan ConvertBlinkImeTextSpanToUiImeTextSpan(
    const blink::WebImeTextSpan& blink_ime_text_span) {
  ui::ImeTextSpan ui_ime_text_span = ui::ImeTextSpan(
      ConvertWebImeTextSpanTypeToUiType(blink_ime_text_span.type),
      blink_ime_text_span.start_offset, blink_ime_text_span.end_offset,
      ConvertUiImeTextSpanThicknessToUiThickness(blink_ime_text_span.thickness),
      blink_ime_text_span.background_color,
      blink_ime_text_span.suggestion_highlight_color,
      blink_ime_text_span.suggestions);
  ui_ime_text_span.underline_color = blink_ime_text_span.underline_color;
  ui_ime_text_span.remove_on_finish_composing =
      blink_ime_text_span.remove_on_finish_composing;
  return ui_ime_text_span;
}

std::vector<ui::ImeTextSpan> ConvertBlinkImeTextSpansToUiImeTextSpans(
    const std::vector<blink::WebImeTextSpan>& blink_ime_text_spans) {
  std::vector<ui::ImeTextSpan> ui_ime_text_spans;
  for (const auto& blink_ime_text_span : blink_ime_text_spans) {
    ui_ime_text_spans.push_back(
        ConvertBlinkImeTextSpanToUiImeTextSpan(blink_ime_text_span));
  }
  return ui_ime_text_spans;
}

}  // namespace content
