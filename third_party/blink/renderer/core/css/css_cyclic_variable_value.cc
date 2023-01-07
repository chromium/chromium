// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_cyclic_variable_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSCyclicVariableValue* CSSCyclicVariableValue::Create() {
  return CssValuePool().CyclicVariableValue();
}

String CSSCyclicVariableValue::CustomCSSText() const {
  return "";
}

}  // namespace blink
