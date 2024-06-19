// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StylePropertyMapTest : public PageTestBase {};

TEST_F(StylePropertyMapTest, SetRevertWithFeatureEnabled) {
  DummyExceptionStateForTesting exception_state;

  HeapVector<Member<V8UnionCSSStyleValueOrString>> revert_string;
  revert_string.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(" revert"));

  HeapVector<Member<V8UnionCSSStyleValueOrString>> revert_style_value;
  revert_style_value.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(
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

TEST_F(StylePropertyMapTest, SetOverflowClipString) {
  DummyExceptionStateForTesting exception_state;

  HeapVector<Member<V8UnionCSSStyleValueOrString>> clip_string;
  clip_string.push_back(
      MakeGarbageCollected<V8UnionCSSStyleValueOrString>(" clip"));

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
  DummyExceptionStateForTesting exception_state;

  HeapVector<Member<V8UnionCSSStyleValueOrString>> clip_style_value;
  clip_style_value.push_back(MakeGarbageCollected<V8UnionCSSStyleValueOrString>(
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
