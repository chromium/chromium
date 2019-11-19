// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSUnsupportedColorValueTest, CreateColorStyleValue) {
  CSSStyleValue* style_value =
      CSSUnsupportedColorValue::Create(Color(0, 255, 0));

  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnsupportedColorType);

  EXPECT_TRUE(DynamicTo<CSSUnsupportedStyleValue>(style_value));

  CSSUnsupportedColorValue* color_value =
      DynamicTo<CSSUnsupportedColorValue>(style_value);

  EXPECT_TRUE(color_value);
  EXPECT_EQ(color_value->Value(), Color(0, 255, 0));
}

TEST(CSSUnsupportedColorValueTest, ColorStyleValueToString) {
  CSSUnsupportedColorValue* style_value =
      CSSUnsupportedColorValue::Create(Color(0, 255, 0));

  EXPECT_TRUE(style_value);
  EXPECT_EQ(
      style_value->toString(),
      cssvalue::CSSColorValue::SerializeAsCSSComponentValue(Color(0, 255, 0)));
}

}  // namespace blink
