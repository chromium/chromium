// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc.h"

#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleRecalcTest : public PageTestBase {};

TEST_F(StyleRecalcTest, SuppressRecalc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .foo { color: green; }
    </style>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById("element");
  ASSERT_TRUE(element);
  element->classList().Add("foo");

  EXPECT_TRUE(StyleRecalcChange().ShouldRecalcStyleFor(*element));
  EXPECT_FALSE(
      StyleRecalcChange().SuppressRecalc().ShouldRecalcStyleFor(*element));
  // The flag should be lost when ForChildren is called.
  EXPECT_TRUE(StyleRecalcChange()
                  .SuppressRecalc()
                  .ForChildren(*element)
                  .ShouldRecalcStyleFor(*element));
}

TEST_F(StyleRecalcTest, SkipStyleRecalcForContainer) {
  ScopedCSSContainerQueriesForTest scoped_cq(true);
  ScopedCSSContainerSkipStyleRecalcForTest scoped_skip(true);

  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().body());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { width: 300px; }
      #outer.narrow { width: 200px; }
      #container { contain: inline-size layout style; }
      #container.narrow { width: 100px; }
      @container (max-width: 200px) {
        #affected { color: red; }
      }
      @container (max-width: 100px) {
        #affected { color: green; }
      }
      .flip { color: pink; }
    </style>
    <div id="outer">
      <div id="container">
        <span id="affected"></span>
        <span id="flip"></span>
      </div>
    </div>
  )HTML",
                                     ASSERT_NO_EXCEPTION);

  Element* outer = GetDocument().getElementById("outer");
  Element* container = GetDocument().getElementById("container");
  Element* affected = GetDocument().getElementById("affected");
  Element* flip = GetDocument().getElementById("flip");

  ASSERT_TRUE(outer);
  ASSERT_TRUE(container);
  ASSERT_TRUE(affected);
  ASSERT_TRUE(flip);

  // Initial style update should skip recalc for #container because it is a
  // container for size container queries, and it attaches a LayoutObject, which
  // means it will be visited for the following UpdateLayout().
  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  EXPECT_TRUE(container->GetComputedStyle());
  EXPECT_FALSE(affected->GetLayoutObject());
  EXPECT_FALSE(affected->GetComputedStyle());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_TRUE(container->GetContainerQueryData()->SkippedStyleRecalc());

  // UpdateStyleAndLayoutTree() will call UpdateLayout() when the style depends
  // on container queries.
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_FALSE(container->GetContainerQueryData()->SkippedStyleRecalc());
  EXPECT_FALSE(flip->NeedsStyleRecalc());
  EXPECT_TRUE(affected->GetLayoutObject());
  ASSERT_TRUE(affected->GetComputedStyle());
  EXPECT_EQ(Color::kBlack, affected->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyColor()));

  // Make everything clean and up-to-date.
  UpdateAllLifecyclePhasesForTest();

  // Change the #outer width to 200px which will affect the auto width of the
  // #container to make the 200px container query match. Since the style update
  // will not cause #container to be marked for layout, the style recalc can not
  // be blocked because we do not know for sure #container will be reached
  // during layout.
  outer->classList().Add("narrow");
  flip->classList().Add("flip");

  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_FALSE(container->GetContainerQueryData()->SkippedStyleRecalc());
  EXPECT_FALSE(flip->NeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
  ASSERT_TRUE(affected->GetComputedStyle());
  EXPECT_EQ(Color::kBlack, affected->ComputedStyleRef().VisitedDependentColor(
                               GetCSSPropertyColor()));

  // UpdateStyleAndLayoutTree() will perform the layout
  // on container queries.
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_FALSE(container->GetContainerQueryData()->SkippedStyleRecalc());
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  ASSERT_TRUE(affected->GetComputedStyle());
  EXPECT_EQ(Color(0xff, 0x00, 0x00),
            affected->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyColor()));

  // Make everything clean and up-to-date.
  UpdateAllLifecyclePhasesForTest();

  // Change the #container width directly to 100px which will means it will be
  // marked for layout and we can skip the style recalc.
  container->classList().Add("narrow");
  flip->classList().Remove("flip");

  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_TRUE(container->GetContainerQueryData()->SkippedStyleRecalc());
  EXPECT_TRUE(flip->NeedsStyleRecalc());
  EXPECT_TRUE(GetDocument().View()->NeedsLayout());
  ASSERT_TRUE(affected->GetComputedStyle());
  EXPECT_EQ(Color(0xff, 0x00, 0x00),
            affected->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyColor()));

  // UpdateStyleAndLayoutTree() will perform the layout
  // on container queries.
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(outer->GetLayoutObject());
  EXPECT_TRUE(container->GetLayoutObject());
  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_FALSE(container->GetContainerQueryData()->SkippedStyleRecalc());
  EXPECT_FALSE(GetDocument().View()->NeedsLayout());
  ASSERT_TRUE(affected->GetComputedStyle());
  EXPECT_EQ(Color(0x00, 0x80, 0x00),
            affected->ComputedStyleRef().VisitedDependentColor(
                GetCSSPropertyColor()));
}

}  // namespace blink
