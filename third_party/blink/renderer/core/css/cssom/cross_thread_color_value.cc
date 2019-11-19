// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_color_value.h"

#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color_value.h"

namespace blink {

CSSStyleValue* CrossThreadColorValue::ToCSSStyleValue() {
  return CSSUnsupportedColorValue::Create(value_);
}

bool CrossThreadColorValue::operator==(
    const CrossThreadStyleValue& other) const {
  if (auto* o = DynamicTo<CrossThreadColorValue>(other))
    return value_ == o->value_;
  return false;
}

std::unique_ptr<CrossThreadStyleValue> CrossThreadColorValue::IsolatedCopy()
    const {
  return std::make_unique<CrossThreadColorValue>(value_);
}

}  // namespace blink
