// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_element_offset_value.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_function_value.h"
#include "third_party/blink/renderer/core/css/css_id_selector_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using CSSElementOffsetValue = cssvalue::CSSElementOffsetValue;
using CSSIdSelectorValue = cssvalue::CSSIdSelectorValue;

namespace {

CSSValue* MakeSelectorFunction(String id) {
  auto* function =
      MakeGarbageCollected<CSSFunctionValue>(CSSValueID::kSelector);
  function->Append(*MakeGarbageCollected<CSSIdSelectorValue>(id));
  return function;
}

CSSValue* MakeEdge(CSSValueID edge) {
  return CSSIdentifierValue::Create(edge);
}

CSSValue* MakeThreshold(double threshold) {
  return CSSNumericLiteralValue::Create(threshold,
                                        CSSPrimitiveValue::UnitType::kNumber);
}

CSSElementOffsetValue* MakeOffset(String id,
                                  base::Optional<CSSValueID> edge,
                                  base::Optional<double> threshold) {
  return MakeGarbageCollected<CSSElementOffsetValue>(
      MakeSelectorFunction(id), edge ? MakeEdge(*edge) : nullptr,
      threshold ? MakeThreshold(*threshold) : nullptr);
}

}  // namespace

TEST(CSSElementOffsetValueTest, Accessors) {
  auto* offset = MakeOffset("foo", CSSValueID::kEnd, 2.0);
  ASSERT_TRUE(offset);
  ASSERT_TRUE(offset->Target());
  ASSERT_TRUE(offset->Edge());
  ASSERT_TRUE(offset->Threshold());

  EXPECT_EQ("selector(#foo)", offset->Target()->CssText());
  EXPECT_EQ("end", offset->Edge()->CssText());
  EXPECT_EQ("2", offset->Threshold()->CssText());
}

TEST(CSSElementOffsetValueTest, Equals) {
  EXPECT_EQ(*MakeOffset("foo", CSSValueID::kEnd, 2.0),
            *MakeOffset("foo", CSSValueID::kEnd, 2.0));
  EXPECT_EQ(*MakeOffset("foo", base::nullopt, base::nullopt),
            *MakeOffset("foo", base::nullopt, base::nullopt));
  EXPECT_NE(*MakeOffset("foo", CSSValueID::kEnd, 2.0),
            *MakeOffset("bar", CSSValueID::kEnd, 2.0));
  EXPECT_NE(*MakeOffset("foo", CSSValueID::kEnd, 2.0),
            *MakeOffset("foo", CSSValueID::kStart, 2.0));
  EXPECT_NE(*MakeOffset("foo", CSSValueID::kEnd, 2.0),
            *MakeOffset("foo", CSSValueID::kEnd, 1.0));
}

TEST(CSSElementOffsetValueTest, CustomCSSText) {
  EXPECT_EQ("selector(#foo) end 2",
            MakeOffset("foo", CSSValueID::kEnd, 2.0)->CustomCSSText());
  EXPECT_EQ(
      "selector(#foo) end",
      MakeOffset("foo", CSSValueID::kEnd, base::nullopt)->CustomCSSText());
  EXPECT_EQ("selector(#foo) 2",
            MakeOffset("foo", base::nullopt, 2.0)->CustomCSSText());
  EXPECT_EQ("selector(#foo)",
            MakeOffset("foo", base::nullopt, base::nullopt)->CustomCSSText());
}

}  // namespace blink
