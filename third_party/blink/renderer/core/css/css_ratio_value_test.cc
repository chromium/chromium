// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_ratio_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSRatioValue = cssvalue::CSSRatioValue;

namespace {

CSSPrimitiveValue* Number(double v) {
  return CSSNumericLiteralValue::Create(v,
                                        CSSPrimitiveValue::UnitType::kNumber);
}

}  // namespace

TEST(CSSRatioValueTest, First_Second) {
  CSSPrimitiveValue* first = Number(1.0);
  CSSPrimitiveValue* second = Number(2.0);

  EXPECT_EQ(&MakeGarbageCollected<CSSRatioValue>(*first, *second)->First(),
            first);
  EXPECT_EQ(&MakeGarbageCollected<CSSRatioValue>(*first, *second)->Second(),
            second);
}

TEST(CSSRatioValueTest, CssText) {
  EXPECT_EQ("1 / 2",
            MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(2.0))
                ->CssText());
}

TEST(CSSRatioValueTest, Equals) {
  EXPECT_EQ(*MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(2.0)),
            *MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(2.0)));
  EXPECT_NE(*MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(2.0)),
            *MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(3.0)));
  EXPECT_NE(*MakeGarbageCollected<CSSRatioValue>(*Number(1.0), *Number(2.0)),
            *MakeGarbageCollected<CSSRatioValue>(*Number(3.0), *Number(2.0)));
}

}  // namespace blink
