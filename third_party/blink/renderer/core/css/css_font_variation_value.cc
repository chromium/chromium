// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_font_variation_value.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSFontVariationValue::CSSFontVariationValue(const AtomicString& tag,
                                             float value)
    : CSSValue(kFontVariationClass), tag_(tag), value_(value) {}

String CSSFontVariationValue::CustomCSSText() const {
  StringBuilder builder;
  SerializeString(tag_, builder);
  builder.Append(' ');
  builder.AppendNumber(value_);
  return builder.ReleaseString();
}

bool CSSFontVariationValue::Equals(const CSSFontVariationValue& other) const {
  return tag_ == other.tag_ && value_ == other.value_;
}

}  // namespace cssvalue
}  // namespace blink
