// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_unparsed_value.h"

#include "third_party/blink/renderer/core/css/cssom/css_unparsed_value.h"

namespace blink {

CSSStyleValue* CrossThreadUnparsedValue::ToCSSStyleValue() {
  return CSSUnparsedValue::FromString(std::move(value_.IsolatedCopy()));
}

bool CrossThreadUnparsedValue::operator==(
    const CrossThreadStyleValue& other) const {
  if (auto* o = DynamicTo<CrossThreadUnparsedValue>(other))
    return value_ == o->value_;
  return false;
}

std::unique_ptr<CrossThreadStyleValue> CrossThreadUnparsedValue::IsolatedCopy()
    const {
  return std::make_unique<CrossThreadUnparsedValue>(value_.IsolatedCopy());
}

}  // namespace blink
