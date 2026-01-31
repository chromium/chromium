// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_url_pattern_value.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

namespace blink {

CSSURLPatternValue::CSSURLPatternValue(const AtomicString& url_string)
    : CSSValue(kURLPatternClass), url_string_(url_string) {}

CSSURLPatternValue::~CSSURLPatternValue() = default;

String CSSURLPatternValue::CustomCSSText() const {
  // TODO(crbug.com/436805487): Implement.
  return "FIXME";
}

bool CSSURLPatternValue::Equals(const CSSURLPatternValue& other) const {
  return url_string_ == other.url_string_;
}

void CSSURLPatternValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  // FIXME: If this is all, we can omit it.
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
