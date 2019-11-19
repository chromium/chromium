// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_display.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"

namespace blink {

FontDisplay CSSValueToFontDisplay(const CSSValue* value) {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kAuto:
        return kFontDisplayAuto;
      case CSSValueID::kBlock:
        return kFontDisplayBlock;
      case CSSValueID::kSwap:
        return kFontDisplaySwap;
      case CSSValueID::kFallback:
        return kFontDisplayFallback;
      case CSSValueID::kOptional:
        return kFontDisplayOptional;
      default:
        break;
    }
  }
  return kFontDisplayAuto;
}

}  // namespace blink
