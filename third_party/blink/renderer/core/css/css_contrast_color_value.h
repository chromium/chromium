// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTRAST_COLOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTRAST_COLOR_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// This is a class for storing the result of parsing the contrast-color function
// before resolving it into a blink::Color. See
// https://drafts.csswg.org/css-color-5/#contrast-color
class CORE_EXPORT CSSContrastColorValue : public CSSValue {
 public:
  explicit CSSContrastColorValue(const CSSValue* color)
      : CSSValue(kContrastColorClass), color_(color) {}

  String CustomCSSText() const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

  bool Equals(const CSSContrastColorValue& other) const;

  const CSSValue& Color() const { return *color_; }

 private:
  // The color parameter value
  Member<const CSSValue> color_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSContrastColorValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsContrastColorValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTRAST_COLOR_VALUE_H_
