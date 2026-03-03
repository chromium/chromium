// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSComputedStyleDeclarationTest : public PageTestBase {};

TEST_F(CSSComputedStyleDeclarationTest, CleanAncestorsNoRecalc) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div>
      <div id=dirty></div>
    </div>
    <div>
      <div id=target style='color:green'></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetElementById("dirty")->setAttribute(html_names::kStyleAttr,
                                        AtomicString("color:pink"));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, CleanShadowAncestorsNoRecalc) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div>
      <div id=dirty></div>
    </div>
    <div id=host></div>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id=target style='color:green'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetElementById("dirty")->setAttribute(html_names::kStyleAttr,
                                        AtomicString("color:pink"));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = shadow_root.getElementById(AtomicString("target"));
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, AdjacentInvalidation) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #b { color: red; }
      .test + #b { color: green; }
    </style>
    <div>
      <span id="a"></span>
      <span id="b"></span>
    </div>
    <div id="c"></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  Element* a = GetDocument().getElementById(AtomicString("a"));
  Element* b = GetDocument().getElementById(AtomicString("b"));
  Element* c = GetDocument().getElementById(AtomicString("c"));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*b));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c));

  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(b);

  EXPECT_EQ("rgb(255, 0, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));

  a->classList().Add(AtomicString("test"));

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*a));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*b));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c));

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
}

TEST_F(CSSComputedStyleDeclarationTest,
       NoCrashWhenCallingGetPropertyCSSValueWithVariable) {
  UpdateAllLifecyclePhasesForTest();
  Element* target = GetDocument().body();
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);
  ASSERT_TRUE(computed);
  const CSSValue* result =
      computed->GetPropertyCSSValue(CSSPropertyID::kVariable);
  EXPECT_FALSE(result);
  // Don't crash.
}

// https://crbug.com/1115877
TEST_F(CSSComputedStyleDeclarationTest, SVGBlockSizeLayoutDependent) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <svg viewBox="0 0 400 400">
      <rect width="400" height="400"></rect>
    </svg>
  )HTML");

  Element* rect = GetDocument().QuerySelector(AtomicString("rect"));
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(rect);

  EXPECT_EQ("400px", computed->GetPropertyValue(CSSPropertyID::kBlockSize));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*rect));
  EXPECT_FALSE(rect->NeedsStyleRecalc());
  EXPECT_FALSE(rect->GetLayoutObject()->NeedsLayout());
}

// https://crbug.com/1115877
TEST_F(CSSComputedStyleDeclarationTest, SVGInlineSizeLayoutDependent) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <svg viewBox="0 0 400 400">
      <rect width="400" height="400"></rect>
    </svg>
  )HTML");

  Element* rect = GetDocument().QuerySelector(AtomicString("rect"));
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(rect);

  EXPECT_EQ("400px", computed->GetPropertyValue(CSSPropertyID::kInlineSize));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*rect));
  EXPECT_FALSE(rect->NeedsStyleRecalc());
  EXPECT_FALSE(rect->GetLayoutObject()->NeedsLayout());
}

TEST_F(CSSComputedStyleDeclarationTest, UseCountDurationZero) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      div {
        color: green;
        /* No animation here. */
      }
    </style>
    <div id=div></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById(AtomicString("div"));
  ASSERT_TRUE(div);
  auto* style = MakeGarbageCollected<CSSComputedStyleDeclaration>(div);

  // There is no animation property specified at all, so getting the computed
  // value should not trigger the counter.
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimationDuration));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDurationZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kWebkitFontSmoothing));
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedWebkitFontSmoothingAnimationDurationZero));

  // Set some animation with zero duration.
  div->SetInlineStyleProperty(CSSPropertyID::kAnimation, "anim 0s linear");
  UpdateAllLifecyclePhasesForTest();

  // Duration should remain uncounted until we retrieve the computed value.
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDurationZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kAnimationDuration));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedAnimationDurationZero));

  // Font smoothing count should remain uncounted until we retrieve the computed
  // value.
  EXPECT_FALSE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedWebkitFontSmoothingAnimationDurationZero));
  EXPECT_TRUE(style->GetPropertyCSSValue(CSSPropertyID::kWebkitFontSmoothing));
  EXPECT_TRUE(GetDocument().IsUseCounted(
      WebFeature::kCSSGetComputedWebkitFontSmoothingAnimationDurationZero));
}

// Verifies that every non-internal longhand CSS property is included in the
// kCSSComputableProperties array (and thus enumerable via getComputedStyle),
// unless it is on a small, intentional exceptions list. This prevents
// regressions where new properties are accidentally made non-enumerable by
// setting computable:false in css_properties.json5.
//
// If this test fails because you added a new property:
//   - If the property should be enumerable in getComputedStyle (the common
//     case), remove any `computable: false` from css_properties.json5.
//   - If the property is intentionally non-enumerable, add it to the
//     kIntentionallyNotComputable set below with a comment explaining why.
TEST_F(CSSComputedStyleDeclarationTest, AllLonghandsAreComputable) {
  // Properties that are intentionally excluded from getComputedStyle iteration.
  // Each entry needs a clear reason for not being enumerable.
  static constexpr CSSPropertyID kIntentionallyNotComputable[] = {
      // 'all' is a shorthand-like longhand; it resets all properties but
      // has no meaningful computed value of its own.
      CSSPropertyID::kAll,
      // background-position-x/y are sub-longhands exposed only through
      // the background-position shorthand in computed style.
      CSSPropertyID::kBackgroundPositionX,
      CSSPropertyID::kBackgroundPositionY,
      // @page descriptor properties, not meaningful outside @page context.
      CSSPropertyID::kPage,
      CSSPropertyID::kPageMarginSafety,
      CSSPropertyID::kPageOrientation,
      CSSPropertyID::kSize,
      // Legacy prefixed sub-longhands superseded by the unprefixed shorthand.
      CSSPropertyID::kWebkitPerspectiveOriginX,
      CSSPropertyID::kWebkitPerspectiveOriginY,
      CSSPropertyID::kWebkitTransformOriginX,
      CSSPropertyID::kWebkitTransformOriginY,
      CSSPropertyID::kWebkitTransformOriginZ,
      // Origin trial test property, never web-exposed.
      CSSPropertyID::kOriginTrialTestProperty,
  };

  HashSet<CSSPropertyID> computable_set;
  for (CSSPropertyID id : kCSSComputableProperties) {
    computable_set.insert(id);
  }

  HashSet<CSSPropertyID> exceptions_set;
  for (CSSPropertyID id : kIntentionallyNotComputable) {
    exceptions_set.insert(id);
  }

  for (CSSPropertyID id = kFirstCSSProperty; id <= kLastCSSProperty;
       id = static_cast<CSSPropertyID>(static_cast<int>(id) + 1)) {
    const CSSProperty& property = CSSProperty::Get(id);

    // Only check non-internal longhands that are actual properties.
    if (!property.IsLonghand() || property.IsInternal() ||
        !property.IsProperty()) {
      continue;
    }

    bool is_computable = computable_set.Contains(id);
    bool is_exception = exceptions_set.Contains(id);

    EXPECT_TRUE(is_computable || is_exception)
        << "Property '" << property.GetPropertyName()
        << "' is a non-internal longhand but is not in "
           "kCSSComputableProperties and not on the exceptions list. "
           "If this property should be enumerable in getComputedStyle "
           "(the common case), remove `computable: false` from "
           "css_properties.json5. Otherwise, add it to "
           "kIntentionallyNotComputable with a comment explaining why.";

    EXPECT_FALSE(is_computable && is_exception)
        << "Property '" << property.GetPropertyName()
        << "' is in kCSSComputableProperties but also on the exceptions "
           "list. Remove it from kIntentionallyNotComputable.";
  }
}

}  // namespace blink
