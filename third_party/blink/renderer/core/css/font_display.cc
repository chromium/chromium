// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_display.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"

namespace blink {

FontDisplay CSSValueToFontDisplay(const CSSValue* value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kAuto:
        return FontDisplay::kAuto;
      case CSSValueID::kBlock:
        return FontDisplay::kBlock;
      case CSSValueID::kSwap:
        return FontDisplay::kSwap;
      case CSSValueID::kFallback:
        return FontDisplay::kFallback;
      case CSSValueID::kOptional:
        return FontDisplay::kOptional;
      default:
        break;
    }
  }
  return FontDisplay::kAuto;
}

}  // namespace blink
