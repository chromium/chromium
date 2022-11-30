// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_invalid_variable_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

TEST(CSSInvalidVariableValueTest, Create) {
  EXPECT_TRUE(CSSInvalidVariableValue::Create());
}

TEST(CSSInvalidVariableValueTest, Pool) {
  const CSSInvalidVariableValue* value1 = CSSInvalidVariableValue::Create();
  const CSSInvalidVariableValue* value2 = CSSInvalidVariableValue::Create();
  EXPECT_EQ(value1, value2);
}

TEST(CSSInvalidVariableValueTest, Equals) {
  const CSSInvalidVariableValue* value1 = CSSInvalidVariableValue::Create();
  const CSSInvalidVariableValue* value2 = CSSInvalidVariableValue::Create();
  EXPECT_TRUE(value1->Equals(*value2));
}

TEST(CSSInvalidVariableValueTest, CustomCSSText) {
  EXPECT_EQ("", CSSInvalidVariableValue::Create()->CustomCSSText());
}

}  // namespace
}  // namespace blink
