// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"

#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class CSSComputedStyleDeclarationTest : public PageTestBase {};

TEST_F(CSSComputedStyleDeclarationTest, CleanAncestorsNoRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=dirty></div>
    <div>
      <div id=target style='color:green'></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetDocument().getElementById("dirty")->setAttribute("style", "color:pink");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = GetDocument().getElementById("target");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, CleanShadowAncestorsNoRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=dirty></div>
    <div id=host></div>
  )HTML");

  Element* host = GetDocument().getElementById("host");

  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(R"HTML(
    <div id=target style='color:green'></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  GetDocument().getElementById("dirty")->setAttribute("style", "color:pink");
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());

  Element* target = shadow_root.getElementById("target");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(target);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
}

TEST_F(CSSComputedStyleDeclarationTest, NeedsAdjacentStyleRecalc) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #a + #b { color: green }
    </style>
    <div id="container" style="display:none">
      <span id="a"></span>
      <span id="b">
        <span id="c"></span>
        <span id="d"></span>
      </span>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());

  Element* container = GetDocument().getElementById("container");
  Element* c_span = GetDocument().getElementById("c");
  Element* d_span = GetDocument().getElementById("d");

  d_span->setAttribute("style", "color:pink");

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*d_span));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c_span));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c_span, true));
  EXPECT_FALSE(container->NeedsAdjacentStyleRecalc());

  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(c_span);

  EXPECT_EQ("rgb(0, 128, 0)",
            computed->GetPropertyValue(CSSPropertyID::kColor));

  d_span->setAttribute("style", "color:green");

  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*d_span));
  EXPECT_TRUE(GetDocument().NeedsLayoutTreeUpdateForNode(*c_span));
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*c_span, true));
  EXPECT_TRUE(container->NeedsAdjacentStyleRecalc());
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

  Element* rect = GetDocument().QuerySelector("rect");
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

  Element* rect = GetDocument().QuerySelector("rect");
  auto* computed = MakeGarbageCollected<CSSComputedStyleDeclaration>(rect);

  EXPECT_EQ("400px", computed->GetPropertyValue(CSSPropertyID::kInlineSize));

  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdate());
  EXPECT_FALSE(GetDocument().NeedsLayoutTreeUpdateForNode(*rect));
  EXPECT_FALSE(rect->NeedsStyleRecalc());
  EXPECT_FALSE(rect->GetLayoutObject()->NeedsLayout());
}

}  // namespace blink
