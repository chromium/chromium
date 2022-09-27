// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_unsupported_color.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSUnsupportedColorValueTest, CreateColorStyleValue) {
  CSSStyleValue* style_value =
      MakeGarbageCollected<CSSUnsupportedColor>(Color(0, 255, 0));

  EXPECT_EQ(style_value->GetType(),
            CSSStyleValue::StyleValueType::kUnsupportedColorType);

  EXPECT_TRUE(DynamicTo<CSSUnsupportedStyleValue>(style_value));

  CSSUnsupportedColor* color_value =
      DynamicTo<CSSUnsupportedColor>(style_value);

  EXPECT_TRUE(color_value);
  EXPECT_EQ(color_value->Value(), Color(0, 255, 0));
}

TEST(CSSUnsupportedColorValueTest, ColorStyleValueToString) {
  CSSUnsupportedColor* style_value =
      MakeGarbageCollected<CSSUnsupportedColor>(Color(0, 255, 0));

  EXPECT_TRUE(style_value);
  EXPECT_EQ(style_value->toString(),
            cssvalue::CSSColor::SerializeAsCSSComponentValue(Color(0, 255, 0)));
}

}  // namespace blink
