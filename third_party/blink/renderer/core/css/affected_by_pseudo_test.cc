// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class AffectedByPseudoTest : public PageTestBase {
 protected:
  struct ElementResult {
    const blink::HTMLQualifiedName tag;
    bool children_or_siblings_affected_by;
  };

  void SetHtmlInnerHTML(const char* html_content);
  void CheckElementsForFocus(ElementResult expected[],
                             unsigned expected_count) const;

  enum AffectedByFlagName {
    kAffectedBySubjectHas,
    kAffectedByNonSubjectHas,
    kAncestorsOrAncestorSiblingsAffectedByHas,
    kSiblingsAffectedByHas,
    kAffectedByPseudoInHas,
    kAncestorsOrSiblingsAffectedByHoverInHas
  };
  void CheckAffectedByFlagsForHas(
      const char* element_id,
      std::map<AffectedByFlagName, bool> expected) const;
};

void AffectedByPseudoTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

void AffectedByPseudoTest::CheckElementsForFocus(
    ElementResult expected[],
    unsigned expected_count) const {
  unsigned i = 0;
  HTMLElement* element = GetDocument().body();

  for (; element && i < expected_count;
       element = Traversal<HTMLElement>::Next(*element), ++i) {
    ASSERT_TRUE(element->HasTagName(expected[i].tag));
    DCHECK(element->GetComputedStyle());
    ASSERT_EQ(expected[i].children_or_siblings_affected_by,
              element->ChildrenOrSiblingsAffectedByFocus());
  }

  DCHECK(!element);
  DCHECK_EQ(i, expected_count);
}

void AffectedByPseudoTest::CheckAffectedByFlagsForHas(
    const char* element_id,
    std::map<AffectedByFlagName, bool> expected) const {
  bool actual;
  const char* flag_name = nullptr;
  for (auto iter : expected) {
    switch (iter.first) {
      case kAffectedBySubjectHas:
        actual = GetElementById(element_id)
                     ->GetComputedStyle()
                     ->AffectedBySubjectHas();
        flag_name = "AffectedBySubjectHas";
        break;
      case kAffectedByNonSubjectHas:
        actual = GetElementById(element_id)->AffectedByNonSubjectHas();
        flag_name = "AffectedByNonSubjectHas";
        break;
      case kAncestorsOrAncestorSiblingsAffectedByHas:
        actual = GetElementById(element_id)
                     ->AncestorsOrAncestorSiblingsAffectedByHas();
        flag_name = "AncestorsOrAncestorSiblingsAffectedByHas";
        break;
      case kSiblingsAffectedByHas:
        actual = GetElementById(element_id)->SiblingsAffectedByHas();
        flag_name = "SiblingsAffectedByHas";
        break;
      case kAffectedByPseudoInHas:
        actual = GetElementById(element_id)->AffectedByPseudoInHas();
        flag_name = "AffectedByPseudoInHas";
        break;
      case kAncestorsOrSiblingsAffectedByHoverInHas:
        actual = GetElementById(element_id)
                     ->AncestorsOrSiblingsAffectedByHoverInHas();
        flag_name = "AncestorsOrSiblingsAffectedByHoverInHas";
        break;
    }
    DCHECK(flag_name);
    if (iter.second == actual)
      continue;

    ADD_FAILURE() << "#" << element_id << " : " << flag_name << " should be "
                  << (iter.second ? "true" : "false") << " but "
                  << (actual ? "true" : "false");
  }
}

// ":focus div" will mark ascendants of all divs with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByPseudoTest, FocusedAscendant) {
  ElementResult expected[] = {{html_names::kBodyTag, true},
                              {html_names::kDivTag, true},
                              {html_names::kDivTag, false},
                              {html_names::kDivTag, false},
                              {html_names::kSpanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// "body:focus div" will mark the body element with
// childrenOrSiblingsAffectedByFocus.
TEST_F(AffectedByPseudoTest, FocusedAscendantWithType) {
  ElementResult expected[] = {{html_names::kBodyTag, true},
                              {html_names::kDivTag, false},
                              {html_names::kDivTag, false},
                              {html_names::kDivTag, false},
                              {html_names::kSpanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>body:focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// ":not(body):focus div" should not mark the body element with
// childrenOrSiblingsAffectedByFocus.
// Note that currently ":focus:not(body)" does not do the same. Then the :focus
// is checked and the childrenOrSiblingsAffectedByFocus flag set before the
// negated type selector is found.
TEST_F(AffectedByPseudoTest, FocusedAscendantWithNegatedType) {
  ElementResult expected[] = {{html_names::kBodyTag, false},
                              {html_names::kDivTag, true},
                              {html_names::kDivTag, false},
                              {html_names::kDivTag, false},
                              {html_names::kSpanTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:not(body):focus div { background-color: pink }</style>
    </head>
    <body>
    <div><div></div></div>
    <div><span></span></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

// Checking current behavior for ":focus + div", but this is a BUG or at best
// sub-optimal. The focused element will also in this case get
// childrenOrSiblingsAffectedByFocus even if it's really a sibling. Effectively,
// the whole sub-tree of the focused element will have styles recalculated even
// though none of the children are affected. There are other mechanisms that
// makes sure the sibling also gets its styles recalculated.
TEST_F(AffectedByPseudoTest, FocusedSibling) {
  ElementResult expected[] = {{html_names::kBodyTag, false},
                              {html_names::kDivTag, true},
                              {html_names::kSpanTag, false},
                              {html_names::kDivTag, false}};

  SetHtmlInnerHTML(R"HTML(
    <head>
    <style>:focus + div { background-color: pink }</style>
    </head>
    <body>
    <div>
      <span></span>
    </div>
    <div></div>
    </body>
  )HTML");

  CheckElementsForFocus(expected, sizeof(expected) / sizeof(ElementResult));
}

TEST_F(AffectedByPseudoTest, AffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you only
  // get a single element style recalc.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, ChildrenOrSiblingsAffectedByFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the whole subtree.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus div { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(11U, element_count);
}

TEST_F(AffectedByPseudoTest, InvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div and the class=a div only.

  SetHtmlInnerHTML(R"HTML(
    <style>:focus .a { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div class='a'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(2U, element_count);
}

TEST_F(AffectedByPseudoTest, NoInvalidationSetFocusUpdate) {
  // Check that when focussing the outer div in the document below, you get a
  // style recalc for the outer div only. The invalidation set for :focus will
  // include 'a', but the id=d div should be affectedByFocus, not
  // childrenOrSiblingsAffectedByFocus.

  SetHtmlInnerHTML(R"HTML(
    <style>#nomatch:focus .a { border: 1px solid lime; }</style>
    <div id=d tabIndex=1>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div></div>
    <div class='a'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("d")->focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, FocusWithinCommonAncestor) {
  // Check that when changing the focus between 2 elements we don't need a style
  // recalc for all the ancestors affected by ":focus-within".

  SetHtmlInnerHTML(R"HTML(
    <style>div:focus-within { background-color: lime; }</style>
    <div>
      <div>
        <div id=focusme1 tabIndex=1></div>
        <div id=focusme2 tabIndex=2></div>
      <div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  unsigned start_count = GetStyleEngine().StyleForElementCount();

  GetElementById("focusme1")->focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  EXPECT_EQ(3U, element_count);

  start_count += element_count;

  GetElementById("focusme2")->focus();
  UpdateAllLifecyclePhasesForTest();

  element_count = GetStyleEngine().StyleForElementCount() - start_count;

  // Only "focusme1" & "focusme2" elements need a recalc thanks to the common
  // ancestor strategy.
  EXPECT_EQ(2U, element_count);
}

TEST_F(AffectedByPseudoTest, HoverScrollbar) {
  SetHtmlInnerHTML(
      "<style>div::-webkit-scrollbar:hover { color: pink; }</style>"
      "<div id=div1></div>");

  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetElementById("div1")->GetComputedStyle()->AffectedByHover());
}

TEST_F(AffectedByPseudoTest,
       AffectedBySubjectHasAndAncestorsOrAncestorSiblingsAffectedByHas) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(.b) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4></div>
      </div>
      <div id=div5 class='a'>
        <div id=div6></div>
        <div id=div7 class='b'></div>
      </div>
      <div id=div8>
        <div id=div9></div>
        <div id=div10></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div10")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div7")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
}

TEST_F(AffectedByPseudoTest,
       AffectedByPseudoInHasAndAncestorsOrSiblingsAffectedByHoverInHas1) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(.b:hover) { background-color: lime; }
      .c:has(:hover) { background-color: green; }
      .d:has(.e) { background-color: blue }
    </style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4></div>
      </div>
      <div id=div5 class='a'>
        <div id=div6></div>
        <div id=div7 class='b'></div>
      </div>
      <div id=div8 class='c'>
        <div id=div9></div>
        <div id=div10></div>
      </div>
      <div id=div11 class='d'>
        <div id=div12></div>
        <div id=div13></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div3")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div4")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div3")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div4")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div6")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div7")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div7")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div9")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div9")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div10")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div10")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div12")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div12")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div13")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div13")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(AffectedByPseudoTest,
       AffectedByPseudoInHasAndAncestorsOrSiblingsAffectedByHoverInHas2) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(.b:hover) .f { background-color: lime; }
      .c:has(:hover) .g { background-color: green; }
      .d:has(.e) .h { background-color: blue }
    </style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4></div>
        <div><div class='f'></div></div>
      </div>
      <div id=div5 class='a'>
        <div id=div6></div>
        <div id=div7 class='b'></div>
        <div><div class='f'></div></div>
      </div>
      <div id=div8 class='c'>
        <div id=div9></div>
        <div id=div10></div>
        <div><div class='g'></div></div>
      </div>
      <div id=div11 class='d'>
        <div id=div12></div>
        <div id=div13></div>
        <div><div class='h'></div></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedByPseudoInHas, false},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div3")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div4")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div3")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div4")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByPseudoInHas, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByPseudoInHas, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div6")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div7")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div7")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div9")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div9")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div10")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div10")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div12")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div12")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div13")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div13")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(AffectedByPseudoTest,
       AffectedByNonSubjectHasHasAndAncestorsOrAncestorSiblingsAffectedByHas) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(.b) .c { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3>
          <div id=div4>
            <div id=div5></div>
          </div>
          <div id=div6 class='b'></div>
        </div>
        <div id=div7></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div7")->setAttribute(html_names::kClassAttr, "c");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div5")->setAttribute(html_names::kClassAttr, "b");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
}

TEST_F(AffectedByPseudoTest,
       AffectedByNonSubjectHasHasAndSiblingsAffectedByHas) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b) .c { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4></div>
      <div id=div5 class='b'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "c");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div5")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase1) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(.b ~ .c) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4>
          <div id=div5></div>
          <div id=div6 class='b'></div>
          <div id=div7></div>
          <div id=div8 class='c'></div>
          <div id=div9></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div4",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div8")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div3",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div4",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase2) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4>
        <div id=div5></div>
      </div>
      <div id=div6 class='b'>
        <div id=div7></div>
        <div id=div8>
          <div id=div9></div>
          <div id=div10 class='c'></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase3) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(.b ~ .c .d) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
        <div id=div4>
          <div id=div5></div>
          <div id=div6 class='b'></div>
          <div id=div7></div>
          <div id=div8 class='c'>
            <div id=div9></div>
            <div id=div10>
              <div id=div11></div>
              <div id=div12 class='d'></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div3",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div4",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div8")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div3",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div4",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase4) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c ~ .d .e) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4>
        <div id=div5></div>
      </div>
      <div id=div6 class='b'>
        <div id=div7></div>
        <div id=div8>
          <div id=div9></div>
          <div id=div10 class='c'></div>
          <div id=div11></div>
          <div id=div12 class='d'>
            <div id=div13></div>
            <div id=div14 class='e'></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div14",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div13",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div14",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase5) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'></div>
      <div id=div3></div>
      <div id=div4 class='b'>
        <div id=div5 class='a'></div>
        <div id=div6></div>
        <div id=div7 class='b'>
          <div id=div8 class='a'></div>
          <div id=div9></div>
          <div id=div10 class='b'>
            <div id=div11 class='c'></div>
          </div>
          <div id=div12></div>
        </div>
        <div id=div13></div>
      </div>
      <div id=div14></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div13",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, true}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase6) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'></div>
      <div id=div3></div>
      <div id=div4 class='b'>
        <div id=div5 class='a'></div>
        <div id=div6></div>
        <div id=div7 class='b'>
          <div id=div8 class='a'></div>
          <div id=div9></div>
          <div id=div10 class='b'>
            <div id=div11></div>
          </div>
          <div id=div12></div>
        </div>
        <div id=div13></div>
      </div>
      <div id=div14></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div13",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, true}});
}

TEST_F(AffectedByPseudoTest, AffectedBySubjectHasComplexCase7) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(+ .b .c) { background-color: lime; }</style>
    <div id=div1>
      <div id=div2></div>
      <div id=div3 class='a'></div>
      <div id=div4 class='b'>
        <div id=div5></div>
        <div id=div6 class='a'></div>
        <div id=div7 class='b'>
          <div id=div8></div>
          <div id=div9 class='a'></div>
          <div id=div10 class='b'>
            <div id=div11></div>
          </div>
          <div id=div12></div>
        </div>
        <div id=div13></div>
      </div>
      <div id=div14></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, true},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div7",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div10",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div13",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByNonSubjectHasComplexCase1) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c) .d { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4>
        <div id=div5>
          <div id=div6></div>
        </div>
      </div>
      <div id=div7 class='b'>
        <div id=div8>
          <div id=div9 class='c'></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr, "d");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div9")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByNonSubjectHasComplexCase2) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b .c) ~ .d { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4>
        <div id=div5>
          <div id=div6></div>
        </div>
      </div>
      <div id=div7 class='b'>
        <div id=div8>
          <div id=div9 class='c'></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "d");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div9")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByNonSubjectHasComplexCase3) {
  SetHtmlInnerHTML(R"HTML(
    <style>.a:has(~ .b > .c > .d) ~ .e { background-color: lime; }</style>
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4>
        <div id=div5>
          <div id=div6></div>
        </div>
      </div>
      <div id=div7 class='b'>
        <div id=div8 class='c'>
          <div id=div9 class='d'>
            <div id=div10></div>
          </div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div8", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div9", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr, "e");
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div6", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div8")->setAttribute(html_names::kClassAttr, "");
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div6",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div7", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas("div8",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div9",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div10", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
}

}  // namespace blink
