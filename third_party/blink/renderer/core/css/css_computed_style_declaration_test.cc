// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"

#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class CSSComputedStyleDeclarationTest : public PageTestBase {};

TEST_F(CSSComputedStyleDeclarationTest, CleanAncestorsNoRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
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
  GetDocument().body()->setInnerHTML(R"HTML(
    <div>
      <div id=dirty></div>
    </div>
    <div id=host></div>
  )HTML");

  Element* host = GetDocument().getElementById(AtomicString("host"));

  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(R"HTML(
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
  GetDocument().body()->setInnerHTML(R"HTML(
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
  GetDocument().body()->setInnerHTML(R"HTML(
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
  GetDocument().body()->setInnerHTML(R"HTML(
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
  ScopedScrollTimelineForTest scroll_timeline_feature(false);
  GetDocument().body()->setInnerHTML(R"HTML(
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

}  // namespace blink
