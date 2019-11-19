// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNSUPPORTED_COLOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNSUPPORTED_COLOR_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// CSSUnsupportedColorValue represents all color values that are normally
// treated as CSSUnsupportedValue.  When compositing color values cross thread,
// this class can be used to differentiate between color values and other types
// that use CSSUnsupportedStyleValue.
class CORE_EXPORT CSSUnsupportedColorValue final
    : public CSSUnsupportedStyleValue {
 public:
  static CSSUnsupportedColorValue* Create(Color color);
  static CSSUnsupportedColorValue* Create(const CSSPropertyName& name,
                                          Color color);
  static CSSUnsupportedColorValue* FromCSSValue(const cssvalue::CSSColorValue&);

  explicit CSSUnsupportedColorValue(Color color)
      : CSSUnsupportedStyleValue(
            cssvalue::CSSColorValue::SerializeAsCSSComponentValue(color)),
        color_value_(color) {}
  explicit CSSUnsupportedColorValue(const CSSPropertyName& name, Color color)
      : CSSUnsupportedStyleValue(
            name,
            cssvalue::CSSColorValue::SerializeAsCSSComponentValue(color)),
        color_value_(color) {}

  StyleValueType GetType() const override { return kUnsupportedColorType; }

  Color Value() const;

  const CSSValue* ToCSSValue() const override;

 private:
  Color color_value_;
  DISALLOW_COPY_AND_ASSIGN(CSSUnsupportedColorValue);
};

template <>
struct DowncastTraits<CSSUnsupportedColorValue> {
  static bool AllowFrom(const CSSStyleValue& value) {
    return value.GetType() ==
           CSSStyleValue::StyleValueType::kUnsupportedColorType;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNSUPPORTED_COLOR_VALUE_H_
