// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_unset_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

CSSUnsetValue* CSSUnsetValue::Create() {
  return CssValuePool().UnsetValue();
}

String CSSUnsetValue::CustomCSSText() const {
  return "unset";
}

}  // namespace cssvalue
}  // namespace blink
