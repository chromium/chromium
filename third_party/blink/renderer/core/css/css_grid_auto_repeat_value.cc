// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_grid_auto_repeat_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

String CSSGridAutoRepeatValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("repeat(");
  result.Append(getValueName(AutoRepeatID()));
  result.Append(", ");
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ToString();
}

bool CSSGridAutoRepeatValue::Equals(const CSSGridAutoRepeatValue& other) const {
  return auto_repeat_id_ == other.auto_repeat_id_ &&
         CSSValueList::Equals(other);
}

}  // namespace cssvalue
}  // namespace blink
