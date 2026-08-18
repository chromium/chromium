// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_url_pattern_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSURLPatternValue::CSSURLPatternValue(const AtomicString& url_string)
    : CSSValue(kURLPatternClass), url_string_(url_string) {}

CSSURLPatternValue::~CSSURLPatternValue() = default;

String CSSURLPatternValue::CustomCSSText() const {
  StringBuilder builder;
  builder.Append("url-pattern(");
  SerializeString(url_string_, builder);
  builder.Append(')');
  return builder.ReleaseString();
}

bool CSSURLPatternValue::Equals(const CSSURLPatternValue& other) const {
  return url_string_ == other.url_string_;
}

}  // namespace blink
