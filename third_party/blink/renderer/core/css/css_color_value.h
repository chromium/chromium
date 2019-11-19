// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSValuePool;

namespace cssvalue {

// Represents the non-keyword subset of <color>.
class CORE_EXPORT CSSColorValue : public CSSValue {
 public:
  // TODO(sashab): Make this create() method take a Color instead.
  static CSSColorValue* Create(RGBA32 color);

  CSSColorValue(Color color) : CSSValue(kColorClass), color_(color) {}

  String CustomCSSText() const { return SerializeAsCSSComponentValue(color_); }

  Color Value() const { return color_; }

  bool Equals(const CSSColorValue& other) const {
    return color_ == other.color_;
  }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

  // Returns the color serialized according to CSSOM:
  // https://drafts.csswg.org/cssom/#serialize-a-css-component-value
  static String SerializeAsCSSComponentValue(Color color);

 private:
  friend class ::blink::CSSValuePool;

  Color color_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSColorValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsColorValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COLOR_VALUE_H_
