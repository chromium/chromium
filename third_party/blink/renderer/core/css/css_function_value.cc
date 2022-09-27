// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_function_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String CSSFunctionValue::CustomCSSText() const {
  StringBuilder result;
  result.Append(getValueName(value_id_));
  result.Append('(');
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

}  // namespace blink
