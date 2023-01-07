// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_revert_layer_value.h"
#include "third_party/blink/renderer/core/css/css_initial_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSRevertLayerValue = cssvalue::CSSRevertLayerValue;

TEST(CSSRevertLayerValueTest, IsCSSWideKeyword) {
  EXPECT_TRUE(CSSRevertLayerValue::Create()->IsCSSWideKeyword());
}

TEST(CSSRevertLayerValueTest, CssText) {
  EXPECT_EQ("revert-layer", CSSRevertLayerValue::Create()->CssText());
}

TEST(CSSRevertLayerValueTest, Equals) {
  EXPECT_EQ(*CSSRevertLayerValue::Create(), *CSSRevertLayerValue::Create());
}

TEST(CSSRevertLayerValueTest, NotEquals) {
  EXPECT_FALSE(*CSSRevertLayerValue::Create() == *CSSInitialValue::Create());
}

}  // namespace blink
