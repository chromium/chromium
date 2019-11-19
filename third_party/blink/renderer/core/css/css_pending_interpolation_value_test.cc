// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_pending_interpolation_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using Type = cssvalue::CSSPendingInterpolationValue::Type;

cssvalue::CSSPendingInterpolationValue* Create(Type type) {
  return cssvalue::CSSPendingInterpolationValue::Create(type);
}

TEST(CSSPendingInterpolationValueTest, Create) {
  EXPECT_TRUE(Create(Type::kCSSProperty));
  EXPECT_TRUE(Create(Type::kPresentationAttribute));
}

TEST(CSSPendingInterpolationValueTest, Pool) {
  const auto* value1 = Create(Type::kCSSProperty);
  const auto* value2 = Create(Type::kCSSProperty);
  const auto* value3 = Create(Type::kPresentationAttribute);
  const auto* value4 = Create(Type::kPresentationAttribute);
  EXPECT_EQ(value1, value2);
  EXPECT_EQ(value3, value4);
  EXPECT_NE(value1, value4);
}

TEST(CSSPendingInterpolationValueTest, Equals) {
  const auto* value1 = Create(Type::kCSSProperty);
  const auto* value2 = Create(Type::kCSSProperty);
  const auto* value3 = Create(Type::kPresentationAttribute);
  const auto* value4 = Create(Type::kPresentationAttribute);
  EXPECT_TRUE(value1->Equals(*value2));
  EXPECT_TRUE(value2->Equals(*value1));
  EXPECT_TRUE(value3->Equals(*value4));
  EXPECT_TRUE(value4->Equals(*value3));
  EXPECT_FALSE(value1->Equals(*value4));
  EXPECT_FALSE(value4->Equals(*value1));
}

TEST(CSSPendingInterpolationValueTest, CustomCSSText) {
  EXPECT_EQ("", Create(Type::kCSSProperty)->CustomCSSText());
  EXPECT_EQ("", Create(Type::kPresentationAttribute)->CustomCSSText());
}

}  // namespace
}  // namespace blink
