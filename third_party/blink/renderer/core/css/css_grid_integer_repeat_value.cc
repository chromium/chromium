// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

String CSSGridIntegerRepeatValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("repeat(");
  result.Append(String::Number(Repetitions()));
  result.Append(", ");
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

bool CSSGridIntegerRepeatValue::Equals(
    const CSSGridIntegerRepeatValue& other) const {
  return repetitions_ == other.repetitions_ && CSSValueList::Equals(other);
}

}  // namespace cssvalue
}  // namespace blink
