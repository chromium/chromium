// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StylePropertyMapTest : public PageTestBase {};

TEST_F(StylePropertyMapTest, SetRevertWithFeatureEnabled) {
  ScopedCSSRevertForTest scoped_revert(true);

  DummyExceptionStateForTesting exception_state;

  HeapVector<CSSStyleValueOrString> revert_string;
  revert_string.push_back(CSSStyleValueOrString::FromString(" revert"));

  HeapVector<CSSStyleValueOrString> revert_style_value;
  revert_style_value.push_back(CSSStyleValueOrString::FromCSSStyleValue(
      CSSKeywordValue::Create("revert", exception_state)));

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "top", revert_string,
           exception_state);
  map->set(GetDocument().GetExecutionContext(), "left", revert_style_value,
           exception_state);

  CSSStyleValue* top =
      map->get(GetDocument().GetExecutionContext(), "top", exception_state);
  CSSStyleValue* left =
      map->get(GetDocument().GetExecutionContext(), "left", exception_state);

  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(top));
  EXPECT_EQ(CSSValueID::kRevert,
            DynamicTo<CSSKeywordValue>(top)->KeywordValueID());

  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(left));
  EXPECT_EQ(CSSValueID::kRevert,
            DynamicTo<CSSKeywordValue>(top)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(StylePropertyMapTest, SetRevertWithFeatureDisabled) {
  ScopedCSSRevertForTest scoped_revert(false);

  HeapVector<CSSStyleValueOrString> revert_string;
  revert_string.push_back(CSSStyleValueOrString::FromString(" revert"));

  HeapVector<CSSStyleValueOrString> revert_style_value;

  DummyExceptionStateForTesting exception_state;
  revert_style_value.push_back(CSSStyleValueOrString::FromCSSStyleValue(
      CSSKeywordValue::Create("revert", exception_state)));
  EXPECT_FALSE(exception_state.HadException());

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  {
    DummyExceptionStateForTesting exception_state;
    map->set(GetDocument().GetExecutionContext(), "top", revert_string,
             exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    map->set(GetDocument().GetExecutionContext(), "left", revert_style_value,
             exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }
  {
    DummyExceptionStateForTesting exception_state;
    map->set(GetDocument().GetExecutionContext(), "--y", revert_style_value,
             exception_state);
    EXPECT_TRUE(exception_state.HadException());
  }

  CSSStyleValue* top =
      map->get(GetDocument().GetExecutionContext(), "top", exception_state);
  CSSStyleValue* left =
      map->get(GetDocument().GetExecutionContext(), "left", exception_state);
  CSSStyleValue* y =
      map->get(GetDocument().GetExecutionContext(), "--y", exception_state);

  EXPECT_FALSE(top);
  EXPECT_FALSE(left);
  EXPECT_FALSE(y);
}

TEST_F(StylePropertyMapTest, SetOverflowClipString) {
  ScopedOverflowClipForTest overflow_clip_feature_enabler(true);

  DummyExceptionStateForTesting exception_state;

  HeapVector<CSSStyleValueOrString> clip_string;
  clip_string.push_back(CSSStyleValueOrString::FromString(" clip"));

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "overflow-x", clip_string,
           exception_state);

  CSSStyleValue* overflow = map->get(GetDocument().GetExecutionContext(),
                                     "overflow-x", exception_state);
  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip,
            DynamicTo<CSSKeywordValue>(overflow)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(StylePropertyMapTest, SetOverflowClipStyleValue) {
  ScopedOverflowClipForTest overflow_clip_feature_enabler(true);

  DummyExceptionStateForTesting exception_state;

  HeapVector<CSSStyleValueOrString> clip_style_value;
  clip_style_value.push_back(CSSStyleValueOrString::FromCSSStyleValue(
      CSSKeywordValue::Create("clip", exception_state)));

  auto* map =
      MakeGarbageCollected<InlineStylePropertyMap>(GetDocument().body());

  map->set(GetDocument().GetExecutionContext(), "overflow-x", clip_style_value,
           exception_state);

  CSSStyleValue* overflow = map->get(GetDocument().GetExecutionContext(),
                                     "overflow-x", exception_state);
  ASSERT_TRUE(DynamicTo<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip,
            DynamicTo<CSSKeywordValue>(overflow)->KeywordValueID());

  EXPECT_FALSE(exception_state.HadException());
}

}  // namespace blink
