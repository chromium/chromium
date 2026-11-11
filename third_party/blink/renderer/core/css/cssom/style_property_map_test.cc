// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssstylevalue_undefined.h"
#include "third_party/blink/renderer/core/css/css_property_keywords.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cssom_keywords.h"
#include "third_party/blink/renderer/core/css/cssom/inline_style_property_map.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class StylePropertyMapTest : public PageTestBase {};

namespace {

// Sets |property| to a CSSKeywordValue holding |keyword|, and returns the
// resulting specified value, or nullptr if set() threw.
CSSStyleValue* SetKeyword(Document& document,
                          StylePropertyMap* style_map,
                          const char* property,
                          const char* keyword) {
  DummyExceptionStateForTesting exception_state;
  HeapVector<Member<V8UnionCSSStyleValueOrString>> values;
  values.push_back(MakeGarbageCollected<V8UnionCSSStyleValueOrString>(
      CSSKeywordValue::Create(keyword, exception_state)));

  style_map->clear();
  style_map->set(document.GetExecutionContext(), property, values,
                 exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return style_map
      ->get(document.GetExecutionContext(), property, exception_state)
      ->GetAsCSSStyleValue();
}

}  // namespace

// An identifier that happens to name a keyword of some other property is still
// a perfectly good <custom-ident>, so setting one must not be rejected. 'auto'
// is page's own keyword rather than a name, and must round-trip as well.
TEST_F(StylePropertyMapTest, SetCustomIdentNamingAKeyword) {
  StylePropertyMap* style_map = GetDocument().body()->attributeStyleMap();

  for (const char* property : {"animation-name", "container-name", "page"}) {
    for (const char* keyword : {"auto", "ease", "row", "red", "span"}) {
      CSSStyleValue* value =
          SetKeyword(GetDocument(), style_map, property, keyword);
      ASSERT_TRUE(value) << property << " : " << keyword;
      ASSERT_TRUE(IsA<CSSKeywordValue>(value)) << property << " : " << keyword;
      EXPECT_EQ(keyword, To<CSSKeywordValue>(*value).value())
          << property << " : " << keyword;
    }
  }
}

// Identifiers that the property's own grammar excludes must still be rejected,
// even though CSSOMKeywords cannot enumerate them.
TEST_F(StylePropertyMapTest, SetCustomIdentExcludedByTheProperty) {
  StylePropertyMap* style_map = GetDocument().body()->attributeStyleMap();

  // 'not', 'and' and 'or' are not valid container names, but they are ordinary
  // custom idents for animation-name.
  for (const char* keyword : {"not", "and", "or"}) {
    EXPECT_FALSE(
        SetKeyword(GetDocument(), style_map, "container-name", keyword))
        << keyword;
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "animation-name", keyword))
        << keyword;
  }

  // 'default' is excluded from <custom-ident> for every property.
  EXPECT_FALSE(
      SetKeyword(GetDocument(), style_map, "container-name", "default"));
  EXPECT_FALSE(
      SetKeyword(GetDocument(), style_map, "animation-name", "default"));
}

TEST_F(StylePropertyMapTest, SetRuntimeEnabledKeyword) {
  StylePropertyMap* style_map = GetDocument().body()->attributeStyleMap();

  {
    ScopedCSSTextAlignMatchParentForTest feature(false);
    EXPECT_FALSE(
        SetKeyword(GetDocument(), style_map, "text-align", "match-parent"));
  }
  {
    ScopedCSSTextAlignMatchParentForTest feature(true);
    EXPECT_TRUE(
        SetKeyword(GetDocument(), style_map, "text-align", "match-parent"));
  }

  {
    ScopedCSSGridLanesLayoutForTest feature(false);
    EXPECT_FALSE(SetKeyword(GetDocument(), style_map, "display", "grid-lanes"));
    EXPECT_FALSE(
        SetKeyword(GetDocument(), style_map, "display", "inline-grid-lanes"));
    EXPECT_FALSE(SetKeyword(GetDocument(), style_map, "reading-flow",
                            "grid-lanes-track"));
  }
  {
    ScopedCSSGridLanesLayoutForTest feature(true);
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "display", "grid-lanes"));
    EXPECT_TRUE(
        SetKeyword(GetDocument(), style_map, "display", "inline-grid-lanes"));
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "reading-flow",
                           "grid-lanes-track"));
  }

  {
    ScopedCSSTextDecorationSkipInkAllForTest feature(false);
    EXPECT_FALSE(SetKeyword(GetDocument(), style_map,
                            "text-decoration-skip-ink", "all"));
  }
  {
    ScopedCSSTextDecorationSkipInkAllForTest feature(true);
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "text-decoration-skip-ink",
                           "all"));
  }

  {
    ScopedCSSUserSelectContainForTest feature(false);
    EXPECT_FALSE(
        SetKeyword(GetDocument(), style_map, "user-select", "contain"));
  }
  {
    ScopedCSSUserSelectContainForTest feature(true);
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "user-select", "contain"));
  }

  {
    ScopedCSSScrollSnapStopBeforeForTest feature(false);
    EXPECT_FALSE(
        SetKeyword(GetDocument(), style_map, "scroll-snap-stop", "before"));
  }
  {
    ScopedCSSScrollSnapStopBeforeForTest feature(true);
    EXPECT_TRUE(
        SetKeyword(GetDocument(), style_map, "scroll-snap-stop", "before"));
  }

  {
    ScopedCSSResizeAutoForTest feature(false);
    EXPECT_FALSE(SetKeyword(GetDocument(), style_map, "resize", "auto"));
  }
  {
    ScopedCSSResizeAutoForTest feature(true);
    EXPECT_TRUE(SetKeyword(GetDocument(), style_map, "resize", "auto"));
  }
}

TEST_F(StylePropertyMapTest, KeywordValidationExceptions) {
  const struct {
    CSSPropertyID property;
    CSSValueID keyword;
    bool shared;
    bool typed_om;
  } cases[] = {
      {CSSPropertyID::kGridLanesDirection, CSSValueID::kNormal, true, true},
      {CSSPropertyID::kGridLanesDirection, CSSValueID::kRow, true, false},
      {CSSPropertyID::kGridLanesDirection, CSSValueID::kFillReverse, true,
       false},
      {CSSPropertyID::kGridTemplateColumns, CSSValueID::kNone, true, true},
      {CSSPropertyID::kGridTemplateColumns, CSSValueID::kAuto, true, false},
      {CSSPropertyID::kGridTemplateRows, CSSValueID::kNone, true, true},
      {CSSPropertyID::kGridTemplateRows, CSSValueID::kMaxContent, true, false},
      {CSSPropertyID::kPositionArea, CSSValueID::kNone, true, true},
      {CSSPropertyID::kPositionArea, CSSValueID::kTop, true, false},
      {CSSPropertyID::kPositionTryFallbacks, CSSValueID::kNone, true, true},
      {CSSPropertyID::kPositionTryFallbacks, CSSValueID::kFlipBlock, true,
       false},
      {CSSPropertyID::kOutlineStyle, CSSValueID::kAuto, false, true},
      {CSSPropertyID::kOutlineStyle, CSSValueID::kHidden, true, true},
      {CSSPropertyID::kLineBreak, CSSValueID::kAfterWhiteSpace, true, true},
      {CSSPropertyID::kAppRegion, CSSValueID::kDrag, true, false},
      {CSSPropertyID::kMinWidth, CSSValueID::kAuto, true, false},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(static_cast<int>(test.property));
    SCOPED_TRACE(static_cast<int>(test.keyword));
    EXPECT_EQ(test.shared,
              IsValidCSSPropertyKeyword(test.property, test.keyword));
    auto* keyword = MakeGarbageCollected<CSSKeywordValue>(test.keyword);
    EXPECT_EQ(test.typed_om,
              CSSOMKeywords::ValidKeywordForProperty(test.property, *keyword));
  }
}

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
      map->get(GetDocument().GetExecutionContext(), "top", exception_state)
          ->GetAsCSSStyleValue();
  CSSStyleValue* left =
      map->get(GetDocument().GetExecutionContext(), "left", exception_state)
          ->GetAsCSSStyleValue();

  ASSERT_TRUE(IsA<CSSKeywordValue>(top));
  EXPECT_EQ(CSSValueID::kRevert, To<CSSKeywordValue>(*top).KeywordValueID());

  ASSERT_TRUE(IsA<CSSKeywordValue>(left));
  EXPECT_EQ(CSSValueID::kRevert, To<CSSKeywordValue>(*left).KeywordValueID());

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
                                     "overflow-x", exception_state)
                                ->GetAsCSSStyleValue();
  ASSERT_TRUE(IsA<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip, To<CSSKeywordValue>(*overflow).KeywordValueID());

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
                                     "overflow-x", exception_state)
                                ->GetAsCSSStyleValue();
  ASSERT_TRUE(IsA<CSSKeywordValue>(overflow));
  EXPECT_EQ(CSSValueID::kClip, To<CSSKeywordValue>(*overflow).KeywordValueID());

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
        // listed 'keywords' in css_properties.json5 as CSSIdentifierValue
        // internally, or restrict which keywords can be reified as and set via
        // CSSKeywordValue in CSSOMKeywords.
        //
        // If the property definition in css_properties.json5 contains the
        // 'separator' field (CSSProperty::IsRepeated()), a CSSKeywordValue will
        // be converted into a CSSValueList wrapping a CSSIdentifierValue
        // instead.
        //
        // To have a more property-specific handling of CSSKeywordValue requires
        // special handling in StyleValueToCSSValue() (style_property_map.cc).
        //
        // The properties skipped below have existing CSS Typed OM crash bugs.
        //
        // *** DO NOT ADD ADDITIONAL PROPERTIES BELOW ***
        case CSSPropertyID::kAnimationTimingFunction:
        case CSSPropertyID::kBackgroundImage:
        case CSSPropertyID::kContain:
        case CSSPropertyID::kFontSizeAdjust:
        case CSSPropertyID::kScrollSnapType:
        case CSSPropertyID::kScrollbarGutter:
          // scrollbar-gutter:both-edges DCHECK fails for other reasons. Needs
          // investigation.
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
