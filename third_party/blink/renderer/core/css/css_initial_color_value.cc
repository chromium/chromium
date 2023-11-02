// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_initial_color_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSInitialColorValue* CSSInitialColorValue::Create() {
  return CssValuePool().InitialColorValue();
}

String CSSInitialColorValue::CustomCSSText() const {
  return "";
}

}  // namespace blink
