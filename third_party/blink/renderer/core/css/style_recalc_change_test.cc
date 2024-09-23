// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_change.h"

#include "third_party/blink/renderer/core/css/container_query_data.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class StyleRecalcChangeTest : public PageTestBase {};

class StyleRecalcChangeTestCQ : public StyleRecalcChangeTest {};

TEST_F(StyleRecalcChangeTest, SuppressRecalc) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .foo { color: green; }
    </style>
    <div id=element></div>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("element"));
  ASSERT_TRUE(element);
  element->classList().Add(AtomicString("foo"));

  EXPECT_TRUE(StyleRecalcChange().ShouldRecalcStyleFor(*element));
  EXPECT_FALSE(
      StyleRecalcChange().SuppressRecalc().ShouldRecalcStyleFor(*element));
  // The flag should be lost when ForChildren is called.
  EXPECT_TRUE(StyleRecalcChange()
                  .SuppressRecalc()
                  .ForChildren(*element)
                  .ShouldRecalcStyleFor(*element));
}

TEST_F(StyleRecalcChangeTestCQ, SkipStyleRecalcForContainer) {
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().body());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer { width: 300px; }
      #outer.narrow { width: 200px; }
      #container { container-type: inline-size; }
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

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* affected = GetDocument().getElementById(AtomicString("affected"));
  Element* flip = GetDocument().getElementById(AtomicString("flip"));

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
  outer->classList().Add(AtomicString("narrow"));
  flip->classList().Add(AtomicString("flip"));

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
  container->classList().Add(AtomicString("narrow"));
  flip->classList().Remove(AtomicString("flip"));

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

TEST_F(StyleRecalcChangeTestCQ, SkipStyleRecalcForContainerCleanSubtree) {
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(GetDocument().body());

  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size; }
      #container.narrow { width: 100px; }
      @container (max-width: 100px) {
        #affected { color: green; }
      }
    </style>
    <div id="container">
      <span id="affected"></span>
    </div>
  )HTML",
                                     ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  ASSERT_TRUE(container);
  container->classList().Add(AtomicString("narrow"));
  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();

  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_FALSE(container->GetContainerQueryData()->SkippedStyleRecalc());
}

TEST_F(StyleRecalcChangeTestCQ, SkipAttachLayoutTreeForContainer) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #container { container-type: inline-size; }
      #container.narrow {
        width: 100px;
        display: inline-block;
        color: pink; /* Make sure there's a recalc to skip. */
      }
      @container (max-width: 100px) {
        #affected { color: green; }
      }
    </style>
    <div id="container">
      <span id="affected"></span>
    </div>
  )HTML",
                                     ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetDocument().getElementById(AtomicString("container"));
  Element* affected = GetDocument().getElementById(AtomicString("affected"));
  ASSERT_TRUE(container);
  ASSERT_TRUE(affected);
  EXPECT_TRUE(container->GetLayoutObject());
  EXPECT_TRUE(affected->GetLayoutObject());

  container->classList().Add(AtomicString("narrow"));
  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();

  ASSERT_TRUE(container->GetContainerQueryData());
  EXPECT_TRUE(container->GetContainerQueryData()->SkippedStyleRecalc());

  EXPECT_TRUE(container->GetLayoutObject());
  EXPECT_FALSE(affected->GetLayoutObject());
}

TEST_F(StyleRecalcChangeTestCQ, DontSkipLayoutRoot) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #outer, #inner { container-type: size; contain: layout; }
    </style>
    <div id="outer">
      <div id="inner">
        <span id="inner_child"></span>
      </div>
      <span id="outer_child"></span>
    </div>
  )HTML",
                                     ASSERT_NO_EXCEPTION);

  UpdateAllLifecyclePhasesForTest();

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  Element* outer_child =
      GetDocument().getElementById(AtomicString("outer_child"));
  Element* inner_child =
      GetDocument().getElementById(AtomicString("inner_child"));

  inner_child->GetLayoutObject()->SetNeedsLayout("test");
  outer_child->GetLayoutObject()->SetNeedsLayout("test");
  inner->SetInlineStyleProperty(CSSPropertyID::kColor, "green");
  outer->SetInlineStyleProperty(CSSPropertyID::kColor, "green");

  EXPECT_TRUE(outer->GetLayoutObject()->NeedsLayout());
  EXPECT_TRUE(inner->GetLayoutObject()->NeedsLayout());

  GetDocument().UpdateStyleAndLayoutTreeForThisDocument();

  ASSERT_TRUE(outer->GetContainerQueryData());
  EXPECT_FALSE(outer->GetContainerQueryData()->SkippedStyleRecalc());

  ASSERT_TRUE(inner->GetContainerQueryData());
  EXPECT_FALSE(inner->GetContainerQueryData()->SkippedStyleRecalc());

  // Should not fail DCHECKs.
  UpdateAllLifecyclePhasesForTest();
}

}  // namespace blink
