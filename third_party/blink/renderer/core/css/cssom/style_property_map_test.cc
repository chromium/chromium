// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_string.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_keywords.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
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

TEST_F(StylePropertyMapTest, CSSKeywordValuesTest) {
  Element* body = GetDocument().body();
  StylePropertyMap* inline_style = body->attributeStyleMap();
  ExecutionContext* execution_context = GetDocument().GetExecutionContext();
  DummyExceptionStateForTesting exception_state;
  HeapVector<Member<V8UnionCSSStyleValueOrString>, 1> set_input({nullptr});

  for (int i = 1; i < kNumCSSValueKeywords; i++) {
    CSSValueID keyword = static_cast<CSSValueID>(i);
    if (css_parsing_utils::IsCSSWideKeyword(keyword)) {
      continue;
    }
    CSSKeywordValue* keyword_value =
        MakeGarbageCollected<CSSKeywordValue>(keyword);
    set_input[0] =
        MakeGarbageCollected<V8UnionCSSStyleValueOrString>(keyword_value);
    for (CSSPropertyID property_id : CSSPropertyIDList()) {
      switch (property_id) {
        // TODO(crbug.com/460361858): These properties need to support the
        // listed keywords in css_properties.json5 as CSSIdentifierValue
        // internally, or remove such keywords from the list of keywords.
        //
        // The presence of the properties below means there are existing CSS
        // Typed OM crash bugs for these properties.
        //
        // *** DO NOT ADD ADDITIONAL PROPERTIES BELOW ***
        case CSSPropertyID::kAnimationDirection:
        case CSSPropertyID::kAnimationFillMode:
        case CSSPropertyID::kAnimationIterationCount:
        case CSSPropertyID::kAnimationName:
        case CSSPropertyID::kAnimationPlayState:
        case CSSPropertyID::kAnimationTimeline:
        case CSSPropertyID::kAnimationTimingFunction:
        case CSSPropertyID::kBackgroundImage:
        case CSSPropertyID::kClipPath:
        case CSSPropertyID::kContain:
        case CSSPropertyID::kContainerType:
        case CSSPropertyID::kFontSizeAdjust:
        case CSSPropertyID::kFontVariantEastAsian:
        case CSSPropertyID::kFontVariantLigatures:
        case CSSPropertyID::kFontVariantNumeric:
        case CSSPropertyID::kGridAutoColumns:
        case CSSPropertyID::kGridAutoRows:
        case CSSPropertyID::kOffsetRotate:
        case CSSPropertyID::kPositionArea:
        case CSSPropertyID::kPositionTryFallbacks:
        case CSSPropertyID::kScrollSnapType:
        case CSSPropertyID::kScrollbarGutter:
          // scrollbar-gutter:both-edges DCHECK fails for other reasons. Needs
          // investigation.
        case CSSPropertyID::kTextDecorationLine:
        case CSSPropertyID::kTimelineTriggerSource:
        case CSSPropertyID::kTouchAction:
        case CSSPropertyID::kTransitionBehavior:
        case CSSPropertyID::kTransitionProperty:
        case CSSPropertyID::kTransitionTimingFunction:
          continue;
        default:
          break;
      }
      if (!CSSOMKeywords::ValidKeywordForProperty(property_id,
                                                  *keyword_value)) {
        continue;
      }
      auto* longhand_property =
          DynamicTo<Longhand>(CSSProperty::Get(property_id));
      if (!longhand_property || !longhand_property->IsProperty() ||
          longhand_property->IsInternal() || longhand_property->IsSurrogate() ||
          !longhand_property->IsWebExposed(execution_context)) {
        continue;
      }
      inline_style->clear();
      inline_style->set(
          execution_context,
          longhand_property->GetCSSPropertyName().ToAtomicString(), set_input,
          exception_state);
      EXPECT_FALSE(exception_state.HadException())
          << longhand_property->GetCSSPropertyName().ToAtomicString() << " : "
          << keyword_value->value();
      UpdateAllLifecyclePhasesForTest();
    }
  }
}

}  // namespace blink
