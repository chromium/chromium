// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_id_selector_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSIdSelectorValue = cssvalue::CSSIdSelectorValue;

TEST(CSSIdSelectorValueTest, Id) {
  EXPECT_EQ("test", MakeGarbageCollected<CSSIdSelectorValue>("test")->Id());
}

TEST(CSSIdSelectorValueTest, Equals) {
  EXPECT_EQ(*MakeGarbageCollected<CSSIdSelectorValue>("foo"),
            *MakeGarbageCollected<CSSIdSelectorValue>("foo"));
  EXPECT_NE(*MakeGarbageCollected<CSSIdSelectorValue>("foo"),
            *MakeGarbageCollected<CSSIdSelectorValue>("bar"));
  EXPECT_NE(*MakeGarbageCollected<CSSIdSelectorValue>("bar"),
            *MakeGarbageCollected<CSSIdSelectorValue>("foo"));
}

TEST(CSSIdSelectorValueTest, CustomCSSText) {
  EXPECT_EQ("#foo",
            MakeGarbageCollected<CSSIdSelectorValue>("foo")->CustomCSSText());
  // The identifier part must follow the serialization rules of:
  // https://drafts.csswg.org/cssom/#serialize-an-identifier
  EXPECT_EQ("#\\31 23",
            MakeGarbageCollected<CSSIdSelectorValue>("123")->CustomCSSText());
}

}  // namespace blink
