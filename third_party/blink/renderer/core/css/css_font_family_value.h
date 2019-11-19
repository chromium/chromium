// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FAMILY_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FAMILY_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CORE_EXPORT CSSFontFamilyValue : public CSSValue {
 public:
  static CSSFontFamilyValue* Create(const String& family_name);

  CSSFontFamilyValue(const String&);

  String Value() const { return string_; }

  String CustomCSSText() const;

  bool Equals(const CSSFontFamilyValue& other) const {
    return string_ == other.string_;
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  friend class CSSValuePool;

  // TODO(sashab): Change this to an AtomicString.
  String string_;
};

template <>
struct DowncastTraits<CSSFontFamilyValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsFontFamilyValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FONT_FAMILY_VALUE_H_
