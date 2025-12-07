// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_revert_rule_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSRevertRuleValue = cssvalue::CSSRevertRuleValue;

TEST(CSSRevertRuleValueTest, IsCSSWideKeyword) {
  EXPECT_TRUE(CSSRevertRuleValue::Create()->IsCSSWideKeyword());
}

TEST(CSSRevertRuleValueTest, CssText) {
  EXPECT_EQ("revert-rule", CSSRevertRuleValue::Create()->CssText());
}

TEST(CSSRevertRuleValueTest, Equals) {
  EXPECT_EQ(*CSSRevertRuleValue::Create(), *CSSRevertRuleValue::Create());
}

TEST(CSSRevertRuleValueTest, NotEquals) {
  EXPECT_FALSE(*CSSRevertRuleValue::Create() == *CSSInitialValue::Create());
}

}  // namespace blink
