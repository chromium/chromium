// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LIGHT_DARK_COLOR_PAIR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LIGHT_DARK_COLOR_PAIR_H_

#include "third_party/blink/renderer/core/css/css_value_pair.h"

namespace blink {

class CORE_EXPORT CSSLightDarkColorPair : public CSSValuePair {
 public:
  CSSLightDarkColorPair(const CSSValue* first, const CSSValue* second)
      : CSSValuePair(kLightDarkColorPairClass, first, second) {}
  String CustomCSSText() const;
  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValuePair::TraceAfterDispatch(visitor);
  }
};

template <>
struct DowncastTraits<CSSLightDarkColorPair> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsLightDarkColorPair();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_LIGHT_DARK_COLOR_PAIR_H_
