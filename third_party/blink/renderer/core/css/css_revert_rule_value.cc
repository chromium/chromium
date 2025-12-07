// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_revert_rule_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"

namespace blink::cssvalue {

CSSRevertRuleValue* CSSRevertRuleValue::Create() {
  return CssValuePool().RevertRuleValue();
}

String CSSRevertRuleValue::CustomCSSText() const {
  return "revert-rule";
}

}  // namespace blink::cssvalue
