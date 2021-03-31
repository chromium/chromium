// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNSUPPORTED_COLOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_CSS_UNSUPPORTED_COLOR_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_color.h"
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
  explicit CSSUnsupportedColorValue(Color color)
      : CSSUnsupportedStyleValue(
            cssvalue::CSSColor::SerializeAsCSSComponentValue(color)),
        color_value_(color) {}
  explicit CSSUnsupportedColorValue(const CSSPropertyName& name, Color color)
      : CSSUnsupportedStyleValue(
            name,
            cssvalue::CSSColor::SerializeAsCSSComponentValue(color)),
        color_value_(color) {}
  explicit CSSUnsupportedColorValue(const cssvalue::CSSColor& color_value)
      : CSSUnsupportedColorValue(color_value.Value()) {}
  CSSUnsupportedColorValue(const CSSUnsupportedColorValue&) = delete;
  CSSUnsupportedColorValue& operator=(const CSSUnsupportedColorValue&) = delete;

  StyleValueType GetType() const override { return kUnsupportedColorType; }

  Color Value() const;

  const CSSValue* ToCSSValue() const override;

 private:
  Color color_value_;
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
