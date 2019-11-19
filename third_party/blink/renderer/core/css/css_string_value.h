// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STRING_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STRING_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSStringValue : public CSSValue {
 public:
  CSSStringValue(const String&);

  String Value() const { return string_; }

  String CustomCSSText() const;

  bool Equals(const CSSStringValue& other) const {
    return string_ == other.string_;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  String string_;
};

template <>
struct DowncastTraits<CSSStringValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsStringValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_STRING_VALUE_H_
