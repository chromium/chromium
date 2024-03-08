// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_revert_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSRevertValue = cssvalue::CSSRevertValue;

TEST(CSSRevertValueTest, IsCSSWideKeyword) {
  EXPECT_TRUE(CSSRevertValue::Create()->IsCSSWideKeyword());
}

TEST(CSSRevertValueTest, CssText) {
  EXPECT_EQ("revert", CSSRevertValue::Create()->CssText());
}

TEST(CSSRevertValueTest, Equals) {
  EXPECT_EQ(*CSSRevertValue::Create(), *CSSRevertValue::Create());
}

TEST(CSSRevertValueTest, NotEquals) {
  EXPECT_NE(*CSSRevertValue::Create(), *CSSInitialValue::Create());
}

}  // namespace blink
