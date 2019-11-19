// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"

#include "third_party/blink/renderer/core/css/cssom/css_unsupported_style_value.h"

namespace blink {

CSSStyleValue* CrossThreadUnsupportedValue::ToCSSStyleValue() {
  return CSSUnsupportedStyleValue::Create(std::move(value_.IsolatedCopy()));
}

bool CrossThreadUnsupportedValue::operator==(
    const CrossThreadStyleValue& other) const {
  if (auto* o = DynamicTo<CrossThreadUnsupportedValue>(other))
    return value_ == o->value_;
  return false;
}

std::unique_ptr<CrossThreadStyleValue>
CrossThreadUnsupportedValue::IsolatedCopy() const {
  return std::make_unique<CrossThreadUnsupportedValue>(value_.IsolatedCopy());
}

}  // namespace blink
