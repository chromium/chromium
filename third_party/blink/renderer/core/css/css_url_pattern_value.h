// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_PATTERN_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_PATTERN_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class CSSURLPatternValue : public CSSValue {
 public:
  explicit CSSURLPatternValue(const AtomicString&);
  ~CSSURLPatternValue();

  // The base class requires these to be implemented:
  String CustomCSSText() const;
  bool Equals(const CSSURLPatternValue&) const;
  void TraceAfterDispatch(Visitor*) const;

  const AtomicString& UrlString() const { return url_string_; }

 private:
  AtomicString url_string_;
};

template <>
struct DowncastTraits<CSSURLPatternValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsURLPatternValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_URL_PATTERN_VALUE_H_
