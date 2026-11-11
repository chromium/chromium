// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_KEYWORDS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_KEYWORDS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"

namespace blink {

enum class CSSValueID;

// The corresponding css_property_keywords.cc is generated from
// css_properties.json5 by build/scripts/core/css/make_css_property_keywords.py
// using build/scripts/core/css/templates/css_property_keywords.cc.tmpl.

// Properties whose keyword field is parsed by the keyword fast path.
extern CORE_EXPORT const CSSBitset kKeywordFastPathProperties;

// Checks keyword membership and runtime availability in css_properties.json5.
// Context-specific restrictions and additional keywords are handled separately.
CORE_EXPORT bool IsValidCSSPropertyKeyword(CSSPropertyID, CSSValueID);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_KEYWORDS_H_
