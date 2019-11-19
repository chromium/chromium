// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/element_animations.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleResolverTest : public PageTestBase {
 protected:
};

TEST_F(StyleResolverTest, StyleForTextInDisplayNone) {
  GetDocument().documentElement()->SetInnerHTMLFromString(R"HTML(
    <body style="display:none">Text</body>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  GetDocument().body()->EnsureComputedStyle();

  ASSERT_TRUE(GetDocument().body()->GetComputedStyle());
  EXPECT_TRUE(
      GetDocument().body()->GetComputedStyle()->IsEnsuredInDisplayNone());
  EXPECT_FALSE(GetStyleEngine().Resolver()->StyleForText(
      To<Text>(GetDocument().body()->firstChild())));
}

TEST_F(StyleResolverTest, AnimationBaseComputedStyle) {
  GetDocument().documentElement()->SetInnerHTMLFromString(R"HTML(
    <style>
      html { font-size: 10px; }
      body { font-size: 20px; }
    </style>
    <div id="div">Test</div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* div = GetDocument().getElementById("div");
  StyleResolver* resolver = GetStyleEngine().Resolver();
  ASSERT_TRUE(resolver);
  ElementAnimations& animations = div->EnsureElementAnimations();
  animations.SetAnimationStyleChange(true);

  ASSERT_TRUE(resolver->StyleForElement(div));
  EXPECT_EQ(20, resolver->StyleForElement(div)->FontSize());
#if DCHECK_IS_ON()
  EXPECT_FALSE(animations.BaseComputedStyle());
#else
  ASSERT_TRUE(animations.BaseComputedStyle());
  EXPECT_EQ(20, animations.BaseComputedStyle()->FontSize());
#endif

  // Getting style with customized parent style should not affect cached
  // animation base computed style.
  const ComputedStyle* parent_style =
      GetDocument().documentElement()->GetComputedStyle();
  EXPECT_EQ(
      10,
      resolver->StyleForElement(div, parent_style, parent_style)->FontSize());
#if DCHECK_IS_ON()
  EXPECT_FALSE(animations.BaseComputedStyle());
#else
  ASSERT_TRUE(animations.BaseComputedStyle());
  EXPECT_EQ(20, animations.BaseComputedStyle()->FontSize());
#endif
  EXPECT_EQ(20, resolver->StyleForElement(div)->FontSize());
}

}  // namespace blink
