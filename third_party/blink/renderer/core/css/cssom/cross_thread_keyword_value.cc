// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"

#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"

namespace blink {

CSSStyleValue* CrossThreadKeywordValue::ToCSSStyleValue() {
  return CSSKeywordValue::Create(std::move(keyword_value_.IsolatedCopy()));
}

bool CrossThreadKeywordValue::operator==(
    const CrossThreadStyleValue& other) const {
  if (auto* o = DynamicTo<CrossThreadKeywordValue>(other))
    return keyword_value_ == o->keyword_value_;
  return false;
}

std::unique_ptr<CrossThreadStyleValue> CrossThreadKeywordValue::IsolatedCopy()
    const {
  return std::make_unique<CrossThreadKeywordValue>(
      keyword_value_.IsolatedCopy());
}

}  // namespace blink
