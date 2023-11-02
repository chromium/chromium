// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSInvalidVariableValue* CSSInvalidVariableValue::Create() {
  return CssValuePool().InvalidVariableValue();
}

String CSSInvalidVariableValue::CustomCSSText() const {
  return "";
}

}  // namespace blink
