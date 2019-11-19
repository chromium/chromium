// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_
#define CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_

#include "third_party/blink/public/web/web_ime_text_span.h"
#include "ui/base/ime/ime_text_span.h"

namespace content {

blink::WebImeTextSpan::Type ConvertUiImeTextSpanTypeToWebType(
    ui::ImeTextSpan::Type type);
ui::ImeTextSpan::Type ConvertWebImeTextSpanTypeToUiType(
    blink::WebImeTextSpan::Type type);
ui::mojom::ImeTextSpanThickness ConvertUiThicknessToUiImeTextSpanThickness(
    ui::ImeTextSpan::Thickness thickness);
ui::ImeTextSpan::Thickness ConvertUiImeTextSpanThicknessToUiThickness(
    ui::mojom::ImeTextSpanThickness thickness);
blink::WebImeTextSpan ConvertUiImeTextSpanToBlinkImeTextSpan(
    const ui::ImeTextSpan&);
ui::ImeTextSpan ConvertBlinkImeTextSpanToUiImeTextSpan(
    const blink::WebImeTextSpan&);
std::vector<blink::WebImeTextSpan> ConvertUiImeTextSpansToBlinkImeTextSpans(
    const std::vector<ui::ImeTextSpan>&);
std::vector<ui::ImeTextSpan> ConvertBlinkImeTextSpansToUiImeTextSpans(
    const std::vector<blink::WebImeTextSpan>&);

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_
