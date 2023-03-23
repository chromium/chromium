// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_SHEET_IDS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_SHEET_IDS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CSSStyleSheet;

DECLARE_WEAK_IDENTIFIER_MAP(CSSStyleSheet);

// The CSSStyleSheetIds::IdForCSSStyleSheet() function generates unique
// IDs for CSS style sheets. These IDs are used to identify the style sheets
// within the InspectorStyleSheet, InspectorCSSAgent, and ElementRuleCollector
// classes. If the style sheet has a CSSStyleSheet object, its ID will have a
// "style-sheet-" prefix. If it lacks a CSSStyleSheet object (a UA stylesheet),
// its ID will be "ua-style-sheet".
class CORE_EXPORT CSSStyleSheetIds {
  STATIC_ONLY(CSSStyleSheetIds);

 public:
  // Return the existing ID if it has already been assigned, otherwise,
  // assign a new ID and return that.
  static String IdForCSSStyleSheet(const CSSStyleSheet*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STYLE_SHEET_IDS_H_
