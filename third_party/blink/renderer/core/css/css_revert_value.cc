// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_revert_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

CSSRevertValue* CSSRevertValue::Create() {
  return CssValuePool().RevertValue();
}

String CSSRevertValue::CustomCSSText() const {
  return "revert";
}

}  // namespace cssvalue
}  // namespace blink
