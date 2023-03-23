// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_style_sheet_ids.h"

#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

DEFINE_WEAK_IDENTIFIER_MAP(CSSStyleSheet)

// static
String CSSStyleSheetIds::IdForCSSStyleSheet(const CSSStyleSheet* style_sheet) {
  if (style_sheet == nullptr) {
    return "ua-style-sheet";
  }
  const int id = WeakIdentifierMap<CSSStyleSheet>::Identifier(
      const_cast<CSSStyleSheet*>(style_sheet));
  return "style-sheet-" + String::Number(id);
}

}  // namespace blink
