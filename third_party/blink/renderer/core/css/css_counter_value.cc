// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_counter_value.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink::cssvalue {

String CSSCounterValue::CustomCSSText() const {
  StringBuilder result;
  if (is_reversed_) {
    result.Append("reversed(");
  }
  result.Append(identifier_->Value());
  if (is_reversed_) {
    result.Append(')');
  }
  if (value_) {
    result.Append(' ');
    result.Append(value_->CustomCSSText());
  }
  return result.ReleaseString();
}

}  // namespace blink::cssvalue
