// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_value_pool.h"

namespace blink::cssvalue {

CSSColor* CSSColor::Create(const Color& color) {
  return CssValuePool().GetOrCreateColor(color);
}

String CSSColor::SerializeAsCSSComponentValue(Color color) {
  return color.SerializeAsCSSColor();
}

}  // namespace blink::cssvalue
