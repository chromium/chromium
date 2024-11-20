// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
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
  void CheckElementsForFocus(const base::span<ElementResult> expected) const;

  enum AffectedByFlagName {
    kAffectedBySubjectHas,
    kAffectedByNonSubjectHas,
    kAncestorsOrAncestorSiblingsAffectedByHas,
    kSiblingsAffectedByHas,
    kSiblingsAffectedByHasForSiblingRelationship,
    kSiblingsAffectedByHasForSiblingDescendantRelationship,
    kAffectedByPseudoInHas,
    kAncestorsOrSiblingsAffectedByHoverInHas,
    kAffectedByLogicalCombinationsInHas
  };
  void CheckAffectedByFlagsForHas(
      const char* element_id,
      std::map<AffectedByFlagName, bool> expected) const;
  void CheckAffectedByFlagsForHasInShadowTree(
      const char* shadow_host_id,
      const char* element_id,
      std::map<AffectedByFlagName, bool> expected) const;
  Element* GetShadowTreeElementById(const char* shadow_host_id,
                                    const char* element_id) const;

 private:
  void CheckAffectedByFlagsForHasInternal(
      const char* shadow_host_id,
      const char* element_id,
      std::map<AffectedByFlagName, bool> expected) const;
};

void AffectedByPseudoTest::SetHtmlInnerHTML(const char* html_content) {
  GetDocument().documentElement()->setInnerHTML(String::FromUTF8(html_content));
  UpdateAllLifecyclePhasesForTest();
}

void AffectedByPseudoTest::CheckElementsForFocus(
    const base::span<ElementResult> expected) const {
  HTMLElement* element = GetDocument().body();

  for (const ElementResult& result : expected) {
    ASSERT_TRUE(element);
    EXPECT_TRUE(element->HasTagName(result.tag));
    EXPECT_TRUE(element->GetComputedStyle());
    EXPECT_EQ(result.children_or_siblings_affected_by,
              element->ChildrenOrSiblingsAffectedByFocus());
    element = Traversal<HTMLElement>::Next(*element);
  }
}

Element* AffectedByPseudoTest::GetShadowTreeElementById(
    const char* shadow_host_id,
    const char* element_id) const {
  ShadowRoot* shadow_root = GetElementById(shadow_host_id)->GetShadowRoot();
  DCHECK(shadow_root) << "#" << shadow_host_id << " must have shadow root";
  return shadow_root->getElementById(AtomicString(element_id));
}

void AffectedByPseudoTest::CheckAffectedByFlagsForHas(
    const char* element_id,
    std::map<AffectedByFlagName, bool> expected) const {
  CheckAffectedByFlagsForHasInternal(/* shadow_host_id */ nullptr, element_id,
                                     std::move(expected));
}

void AffectedByPseudoTest::CheckAffectedByFlagsForHasInShadowTree(
    const char* shadow_host_id,
    const char* element_id,
    std::map<AffectedByFlagName, bool> expected) const {
  CheckAffectedByFlagsForHasInternal(shadow_host_id, element_id,
                                     std::move(expected));
}

void AffectedByPseudoTest::CheckAffectedByFlagsForHasInternal(
    const char* shadow_host_id,
    const char* element_id,
    std::map<AffectedByFlagName, bool> expected) const {
  Element* element;
  if (shadow_host_id) {
    element = GetShadowTreeElementById(shadow_host_id, element_id);
  } else {
    element = GetElementById(element_id);
  }
  bool actual;
  const char* flag_name = nullptr;
  for (auto iter : expected) {
    switch (iter.first) {
      case kAffectedBySubjectHas:
        actual = element->AffectedBySubjectHas();
        flag_name = "AffectedBySubjectHas";
        break;
      case kAffectedByNonSubjectHas:
        actual = element->AffectedByNonSubjectHas();
        flag_name = "AffectedByNonSubjectHas";
        break;
      case kAncestorsOrAncestorSiblingsAffectedByHas:
        actual = element->AncestorsOrAncestorSiblingsAffectedByHas();
        flag_name = "AncestorsOrAncestorSiblingsAffectedByHas";
        break;
      case kSiblingsAffectedByHas:
        actual = element->GetSiblingsAffectedByHasFlags();
        flag_name = "SiblingsAffectedByHas";
        break;
      case kSiblingsAffectedByHasForSiblingRelationship:
        actual = element->HasSiblingsAffectedByHasFlags(
            SiblingsAffectedByHasFlags::kFlagForSiblingRelationship);
        flag_name = "SiblingsAffectedByHasForSiblingRelationship";
        break;
      case kSiblingsAffectedByHasForSiblingDescendantRelationship:
        actual = element->HasSiblingsAffectedByHasFlags(
            SiblingsAffectedByHasFlags::kFlagForSiblingDescendantRelationship);
        flag_name = "SiblingsAffectedByHasForSiblingDescendantRelationship";
        break;
      case kAffectedByPseudoInHas:
        actual = element->AffectedByPseudoInHas();
        flag_name = "AffectedByPseudoInHas";
        break;
      case kAncestorsOrSiblingsAffectedByHoverInHas:
        actual = element->AncestorsOrSiblingsAffectedByHoverInHas();
        flag_name = "AncestorsOrSiblingsAffectedByHoverInHas";
        break;
      case kAffectedByLogicalCombinationsInHas:
        actual = element->AffectedByLogicalCombinationsInHas();
        flag_name = "AffectedByLogicalCombinationsInHas";
        break;
    }
    DCHECK(flag_name);
    if (iter.second == actual) {
      continue;
    }

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

  CheckElementsForFocus(expected);
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

  CheckElementsForFocus(expected);
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

  CheckElementsForFocus(expected);
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

  CheckElementsForFocus(expected);
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

  GetElementById("d")->Focus();
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

  GetElementById("d")->Focus();
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

  GetElementById("d")->Focus();
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

  GetElementById("d")->Focus();
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

  GetElementById("focusme1")->Focus();
  UpdateAllLifecyclePhasesForTest();

  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;

  EXPECT_EQ(3U, element_count);

  start_count += element_count;

  GetElementById("focusme2")->Focus();
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
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
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
  GetElementById("div10")->setAttribute(html_names::kClassAttr,
                                        AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div4")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div6")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div7")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
  GetElementById("div7")->setAttribute(html_names::kClassAttr,
                                       AtomicString("c"));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
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
  GetElementById("div6")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
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
  GetElementById("div5")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedByNonSubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr,
                                       AtomicString("c"));
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div5")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
  CheckAffectedByFlagsForHas("div2",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
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
  GetElementById("div8")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div6")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
               {kSiblingsAffectedByHas, true}});
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
  CheckAffectedByFlagsForHas("div2",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
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
  GetElementById("div8")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div6")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
               {kSiblingsAffectedByHas, true}});
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
               {kSiblingsAffectedByHas, true}});
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
               {kSiblingsAffectedByHas, true}});
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
               {kSiblingsAffectedByHas, true}});
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
               {kSiblingsAffectedByHas, true}});
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
                              {kSiblingsAffectedByHas, true}});
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
                              {kSiblingsAffectedByHas, true}});
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
               {kSiblingsAffectedByHas, true}});
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
                              {kSiblingsAffectedByHas, true}});
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
  GetElementById("div3")->setAttribute(html_names::kClassAttr,
                                       AtomicString("d"));
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div9")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr,
                                       AtomicString("d"));
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div9")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div4")->setAttribute(html_names::kClassAttr,
                                       AtomicString("e"));
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
               {kSiblingsAffectedByHas, true}});
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
  GetElementById("div8")->setAttribute(html_names::kClassAttr, g_empty_atom);
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
               {kSiblingsAffectedByHas, true}});
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

TEST_F(AffectedByPseudoTest, AffectedBySelectorQuery) {
  SetHtmlInnerHTML(R"HTML(
    <div id=div1>
      <div id=div2 class='a'>
        <div id=div3></div>
      </div>
      <div id=div4 class='e'>
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

  StaticElementList* result = GetDocument().QuerySelectorAll(
      AtomicString(".a:has(~ .b > .c > .d) ~ .e"));
  ASSERT_EQ(1U, result->length());
  EXPECT_EQ(result->item(0)->GetIdAttribute(), "div4");

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
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion1) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(.b) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'></div>
    </div>
    <div id=div2>
      <div id=div21>
        <div id=div211>
          <div id=div2111></div>
        </div>
        <div id=div212 class='b'>
          <div id=div2121></div>
        </div>
      </div>
      <div id=div22></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div211", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2111", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div212", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2121", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div21")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div21",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div211", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2111", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div212",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2121",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  auto* subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div12"));
  subtree_root->setInnerHTML(
      String::FromUTF8(R"HTML(<div id=div121></div>)HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div111>
          <div id=div1111></div>
          <div id=div1112></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas("div111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1112",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div1112")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div11121>
          <div id=div111211></div>
          <div id=div111212 class='b'>
            <div id=div1112121></div>
          </div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(5U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas("div11121",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div111211",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div111212",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1112121",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div2111")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div21111>
          <div id=div211111></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div21111", {{kAffectedBySubjectHas, false},
                   {kAffectedByNonSubjectHas, false},
                   {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                   {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div211111", {{kAffectedBySubjectHas, false},
                    {kAffectedByNonSubjectHas, false},
                    {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                    {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div2121")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div21211>
          <div id=div212111></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas("div21211",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div212111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion2) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(> .b > .c) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'></div>
    </div>
    <div id=div2>
      <div id=div21>
        <div id=div211 class='b'>
          <div id=div2111 class='c'>
            <div id=div21111></div>
          </div>
        </div>
        <div id=div212></div>
      </div>
      <div id=div22></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div211", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2111", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21111", {{kAffectedBySubjectHas, false},
                   {kAffectedByNonSubjectHas, false},
                   {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                   {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div212", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div21")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div21",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div211",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21111", {{kAffectedBySubjectHas, false},
                   {kAffectedByNonSubjectHas, false},
                   {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                   {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div212",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div111 class='b'>
          <div id=div1111>
            <div id=div11111></div>
          </div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(4U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas("div111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // There can be some inefficiency for fixed depth :has() argument
  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11111")
      ->setAttribute(html_names::kClassAttr, AtomicString("c"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div21111")
      ->setInnerHTML(String::FromUTF8(
          R"HTML(
        <div id=div211111>
          <div id=div2111111></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div211111", {{kAffectedBySubjectHas, false},
                    {kAffectedByNonSubjectHas, false},
                    {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                    {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2111111", {{kAffectedBySubjectHas, false},
                     {kAffectedByNonSubjectHas, false},
                     {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                     {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div212")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div2121>
          <div id=div21211>
            <div id=div212111></div>
          </div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas("div2121",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div21211",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div212111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion3) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(~ .b) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'>
        <div id=div111></div>
      </div>
    </div>
    <div id=div2>
      <div id=div21></div>
      <div id=div22></div>
      <div id=div23></div>
      <div id=div24 class='b'></div>
      <div id=div25></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div111", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div23", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div24", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div25", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div22")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div111", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div23",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div24",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div25",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div111")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div1111>
          <div id=div11112 class='b'></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1111", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11112", {{kAffectedBySubjectHas, false},
                   {kAffectedByNonSubjectHas, false},
                   {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                   {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  auto* subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div12"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div121>
        <div id=div1211></div>
        <div id=div1212 class='a'>
          <div id=div12121></div>
        </div>
        <div id=div1213></div>
        <div id=div1214 class='b'></div>
        <div id=div1215></div>
      </div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(8U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1211", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1212",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div12121", {{kAffectedBySubjectHas, false},
                   {kAffectedByNonSubjectHas, false},
                   {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                   {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1213",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1214",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1215",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion4) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(+ .b + .c) { color: green; }
    </style>
    <div id=div1>
      <div id=div11></div>
      <div id=div13 class='b'></div>
      <div id=div14></div>
      <div id=div17></div>
      <div id=div18></div>
      <div id=div19></div>
    </div>
    <div id=div2>
      <div id=div21></div>
      <div id=div22></div>
      <div id=div23 class='b'>
        <div id=div231></div>
      </div>
      <div id=div24 class='c'></div>
      <div id=div25></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div17", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div18", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div19", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div23", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div24", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div25", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(html_names::kIdAttr, AtomicString("div12"));
  element->setAttribute(html_names::kClassAttr, AtomicString("a"));
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->InsertBefore(element,
                     GetDocument().getElementById(AtomicString("div13")));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div14",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div17",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div12")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // There can be some inefficiency for fixed adjacent distance :has() argument
  start_count = GetStyleEngine().StyleForElementCount();
  element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(html_names::kIdAttr, AtomicString("div16"));
  element->setAttribute(html_names::kClassAttr, AtomicString("b c"));
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->InsertBefore(element,
                     GetDocument().getElementById(AtomicString("div17")));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div12")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  CheckAffectedByFlagsForHas(
      "div16",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div17",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div14")->setAttribute(html_names::kClassAttr,
                                        AtomicString("c"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetElementById("div12")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  CheckAffectedByFlagsForHas(
      "div16",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div17",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(html_names::kIdAttr, AtomicString("div15"));
  element->setAttribute(html_names::kClassAttr, AtomicString("a"));
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->InsertBefore(element,
                     GetDocument().getElementById(AtomicString("div16")));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div16",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div17",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div18",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  element = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  element->setAttribute(html_names::kIdAttr, AtomicString("div15.5"));
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->InsertBefore(element,
                     GetDocument().getElementById(AtomicString("div16")));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15.5",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div22")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div23",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div231", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div24",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div25", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion5) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(~ .b .c) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'></div>
    </div>
    <div id=div2>
      <div id=div21 class='a'></div>
      <div id=div22 class='b'>
        <div id=div221></div>
        <div id=div222>
          <div id=div2221></div>
          <div id=div2223></div>
          <div id=div2224 class='b'>
            <div id=div22241 class='c'></div>
          </div>
          <div id=div2225></div>
        </div>
      </div>
      <div id=div25></div>
    </div>
    <div id=div3>
      <div id=div31></div>
      <div id=div32></div>
      <div id=div33>
        <div id=div331></div>
      </div>
      <div id=div34></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div22",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div221", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div222",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2221", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2223", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2224",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div22241",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div2225",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div25",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div31", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div32", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div33", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div331", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div34", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div12"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div121>
        <div id=div1211></div>
        <div id=div1212></div>
      </div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(4U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1211",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1212",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  start_count = GetStyleEngine().StyleForElementCount();
  subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div13"));
  subtree_root->setAttribute(html_names::kClassAttr, AtomicString("b"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div131>
        <div id=div1311 class='c'></div>
        <div id=div1312></div>
      </div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(5U, GetStyleEngine().StyleForElementCount() - start_count);

  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1211",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1212",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div131",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1311",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1312",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div2222"));
  subtree_root->setAttribute(html_names::kClassAttr, AtomicString("a"));
  subtree_root->setInnerHTML(
      String::FromUTF8(R"HTML(<div id=div22221></div>)HTML"));
  GetDocument()
      .getElementById(AtomicString("div222"))
      ->InsertBefore(subtree_root,
                     GetDocument().getElementById(AtomicString("div2223")));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div2221", {{kAffectedBySubjectHas, false},
                  {kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2222",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div22221",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div2223",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div2224",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas("div22241",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div2225",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterInsertion6) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(+ .b + .c .d) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'></div>
    </div>
    <div id=div2>
      <div id=div21></div>
      <div id=div22></div>
      <div id=div23 class='b'></div>
      <div id=div24 class='c'></div>
      <div id=div25>
        <div id=div251></div>
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
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div23", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div24", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div25", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div251", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  auto* subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div12"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div121></div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div13"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div131></div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div131",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // There can be some inefficiency for fixed adjacent distance :has() argument
  start_count = GetStyleEngine().StyleForElementCount();
  subtree_root = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  subtree_root->setAttribute(html_names::kIdAttr, AtomicString("div14"));
  subtree_root->setInnerHTML(String::FromUTF8(R"HTML(
      <div id=div141 class='d'></div>
  )HTML"));
  GetDocument().getElementById(AtomicString("div1"))->AppendChild(subtree_root);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div11")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  CheckAffectedByFlagsForHas(
      "div14",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div141",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div22")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div22",
      {{kAffectedBySubjectHas, true},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div23",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div24",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div25", {{kAffectedBySubjectHas, false},
                {kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div251", {{kAffectedBySubjectHas, false},
                 {kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div22")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  // There can be some inefficiency for fixed adjacent distance :has() argument
  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div23")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div231 class='d'></div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(2U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div23",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div231",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  EXPECT_EQ(Color::FromRGB(0, 0, 0),
            GetElementById("div22")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div24")->setInnerHTML(String::FromUTF8(
      R"HTML(
        <div id=div241>
          <div id=div2411 class='d'></div>
        </div>
      )HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(3U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div24",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div241",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div2411",
      {{kAffectedBySubjectHas, false},
       {kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingRelationship, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  EXPECT_EQ(Color::FromRGB(0, 128, 0),
            GetElementById("div22")->GetComputedStyle()->VisitedDependentColor(
                GetCSSPropertyColor()));
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterWiping) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(.b) { color: green; }
    </style>
    <div id='div1' class='a'>
      <div id='div11'>
        div11 <div id='div111' class='b'></div>
      </div>
      <div id='div12'>
        div12 <div id='div121' class='b'></div>
      </div>
    </div>
    <div id='div2'>
      div2 <div id='div21' class='b'></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div111", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->setInnerHTML(String::FromUTF8(R"HTML(div11)HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div12"))
      ->setInnerHTML(String::FromUTF8(R"HTML(div12)HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div21", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div2"))
      ->setInnerHTML(String::FromUTF8(R"HTML(div2)HTML"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0U, GetStyleEngine().StyleForElementCount() - start_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByLogicalCombinationsInHas) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(:is(.b .c)) { color: green; }
      .d:has(:is(.e)) { color: green; }
    </style>
    <div id=div1>
      <div id=div11 class='a'>
        <div id=div111>
          <div id=div1111 class='c'></div>
        </div>
      </div>
      <div id=div12 class='d'>
        <div id=div121>
          <div id=div1211></div>
        </div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByLogicalCombinationsInHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByLogicalCombinationsInHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1111",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div12",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByLogicalCombinationsInHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div121",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div1211",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByNonSubjectHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a b"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1U, GetStyleEngine().StyleForElementCount() - start_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div11")->setAttribute(html_names::kClassAttr,
                                        AtomicString("a invalid"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0U, GetStyleEngine().StyleForElementCount() - start_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div12")->setAttribute(html_names::kClassAttr,
                                        AtomicString("d e"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0U, GetStyleEngine().StyleForElementCount() - start_count);
}

TEST_F(AffectedByPseudoTest,
       AncestorsOrSiblingsAffectedByHoverInHasWithFastRejection) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(nonexistent), .a:has(.b:hover) { color: green }
    </style>
    <div id=div1 class='a'>
      <div id=div11></div>
      <div id=div12 class='b'></div>
      <div id=div13></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAffectedByPseudoInHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                {kAncestorsOrSiblingsAffectedByHoverInHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div13")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
  GetElementById("div13")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div12")->SetHovered(true);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
  GetElementById("div12")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval1) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(.b) { color: green }
    </style>
    <div id=div1 class='a'>
      <div id=div11></div>
      <div id=div12 class='b'>
        <div id=div121 class='b'></div>
      </div>
      <div id=div13 class='b'></div>
      <div id=div14 class='b'></div>
      <div id=div15></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div15", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div12"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div121")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div12")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div14")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div13")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval2) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(> .b > .c) { color: green }
    </style>
    <div id=div1 class='a'>
      <div id=div11></div>
      <div id=div12 class='b'>
        <div id=div121 class='c'></div>
        <div id=div122 class='c'></div>
      </div>
      <div id=div13 class='b'>
        <div id=div131 class='c'></div>
        <div id=div132 class='c'></div>
        <div id=div133 class='c'></div>
        <div id=div134></div>
      </div>
      <div id=div14></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div122", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div131", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div132", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div133", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div134", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div14", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div14")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div13"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div134")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div13"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div131")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div13"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div133")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div122", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div132", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div13")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, true},
               {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, true}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div122", {{kAffectedBySubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div12"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div121")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval3) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(~ .b) { color: green }
    </style>
    <div id=div1>
      <div id=div11 class='a'>
        <div id=div111 class='a'></div>
        <div id=div112 class='b'></div>
        <div id=div113 class='b'></div>
        <div id=div114></div>
      </div>
      <div id=div12>
        <div id=div121 class='b'></div>
        <div id=div122 class='b'></div>
      </div>
      <div id=div13>
        <div id=div131></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, true},
                {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div111", {{kAffectedBySubjectHas, true},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div112", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div114", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedBySubjectHas, false},
                {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div122", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div13", {{kAffectedBySubjectHas, false},
                {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div131", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div114")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div112")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div113")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div12"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div122")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div13")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div12")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval4) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(+ .b + .c) { color: green }
    </style>
    <div id=div1>
      <div id=div11 class='a'>
        <div id=div111 class='a'></div>
        <div id=div112 class='b'></div>
        <div id=div113 class='c'></div>
        <div id=div114 class='c'></div>
        <div id=div115 class='c'></div>
        <div id=div116></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedBySubjectHas, true},
                {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div111", {{kAffectedBySubjectHas, true},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div112", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div114", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div115", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div116", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div115")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div113")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div114", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div116", {{kAffectedBySubjectHas, false},
                 {kSiblingsAffectedByHasForSiblingRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div116")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div114")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval5) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(~ .b .c) { color: green }
    </style>
    <div id=div1>
      <div id=div11 class='a'>
        <div id=div111 class='c'></div>
      </div>
      <div id=div12>
        <div id=div121></div>
        <div id=div122 class='c'></div>
      </div>
      <div id=div13>
        <div id=div131 class='c'></div>
      </div>
      <div id=div14 class='b'>
        <div id=div141></div>
        <div id=div142 class='c'></div>
        <div id=div143 class='c'></div>
      </div>
      <div id=div15 class='b'>
        <div id=div151 class='c'></div>
        <div id=div152></div>
      </div>
      <div id=div16 class='b'>
        <div id=div161 class='c'></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div111",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div122",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div131",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div14",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div141",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div142",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div143",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div151",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div152",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div16",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div161",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div11"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div111")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div12"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div122")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div12")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div13")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div14",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div141",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div142",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div143",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div151",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div152",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div16",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div161",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div16")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div143",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div151",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div152",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div15"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div152")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div15"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div151")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div14",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div141",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div142",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div143",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div15",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div14"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div142")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
}

TEST_F(AffectedByPseudoTest, AffectedByHasAfterRemoval6) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .a:has(+ .b > .c) { color: green }
    </style>
    <div id=div1>
      <div id=div11 class='a'></div>
      <div id=div12 class='b'>
        <div id=div121></div>
        <div id=div122 class='c'>
          <div id=div1221 class='c'></div>
        </div>
      </div>
      <div id=div13 class='b'>
        <div id=div131></div>
        <div id=div132 class='c'></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div12",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div121",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div122",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div1221",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div131",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div132",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div122"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div1221")));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetDocument()
      .getElementById(AtomicString("div1"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("div12")));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div11",
      {{kAffectedBySubjectHas, true},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div13",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, true}});
  CheckAffectedByFlagsForHas(
      "div131",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
  CheckAffectedByFlagsForHas(
      "div132",
      {{kAffectedBySubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});
}

TEST_F(AffectedByPseudoTest, AffectedByHasWithoutNth) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #root:has(.foo) { background-color: green }
      :nth-child(1000) * { background-color: red }
    </style>
    <div id="root">
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
      <div></div>
      <div></div>
      <div id="foo"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  CheckAffectedByFlagsForHas(
      "root",
      {{kAffectedBySubjectHas, true},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHasForSiblingDescendantRelationship, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  Element* foo = GetElementById("foo");
  foo->setAttribute(html_names::kClassAttr, AtomicString("foo"));

  UpdateAllLifecyclePhasesForTest();

  ASSERT_EQ(GetStyleEngine().StyleForElementCount() - start_count, 1U);
}

TEST_F(AffectedByPseudoTest, AffectedByPseudoInHasWithNestingParent) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .b:hover {
        .a:has(~ &) { background-color: green; }
      }
    </style>
    <div id=div1></div>
    <div id=div2 class='a'></div>
    <div id=div3></div>
    <div id=div4 class='b'></div>
    <div id=div5></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, false},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, true},
               {kAffectedByPseudoInHas, true},
               {kSiblingsAffectedByHasForSiblingRelationship, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, false}});
  CheckAffectedByFlagsForHas(
      "div4", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, true},
               {kAncestorsOrSiblingsAffectedByHoverInHas, true}});
  CheckAffectedByFlagsForHas(
      "div5", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kSiblingsAffectedByHasForSiblingRelationship, true},
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
  ASSERT_EQ(2U, element_count);
  GetElementById("div4")->SetHovered(false);
  UpdateAllLifecyclePhasesForTest();
}

TEST_F(AffectedByPseudoTest, AffectedByPseudoInHasWithNestingComplexParent) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      .b .c {
        .a:has(> &) { background-color: green; }
      }
    </style>
    <div id=div1></div>
    <div id=div2>
      <div id=div3></div>
      <div id=div4 class='a'>
        <div id=div5 class='c'></div>
      </div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kAffectedByLogicalCombinationsInHas, false}});
  CheckAffectedByFlagsForHas(
      "div2", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kAffectedByLogicalCombinationsInHas, false}});
  CheckAffectedByFlagsForHas(
      "div3", {{kAffectedBySubjectHas, false},
               {kAffectedByPseudoInHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kAffectedByLogicalCombinationsInHas, false}});
  CheckAffectedByFlagsForHas("div4",
                             {{kAffectedBySubjectHas, true},
                              {kAffectedByPseudoInHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kAffectedByLogicalCombinationsInHas, true}});
  CheckAffectedByFlagsForHas("div5",
                             {{kAffectedBySubjectHas, false},
                              {kAffectedByPseudoInHas, false},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kAffectedByLogicalCombinationsInHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div1")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div3")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div2")->setAttribute(html_names::kClassAttr,
                                       AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(2U, element_count);
}

TEST_F(AffectedByPseudoTest,
       ShadowHostAffectedByNonSubjectHasInShadowTreeStyle) {
  GetDocument().body()->setHTMLUnsafe(R"HTML(
    <div id="div1">
      <div id="div11">
        <template shadowrootmode="open">
          <style>
            :host:has(.a) .b { background-color: lime; }
            :host:has(~ .a) .c { background-color: lime; }
          </style>
          <div id="div111">
            <div id="div1111">
              <div id="div11111"></div>
            </div>
            <div id="div1112" class="a"></div>
          </div>
          <div id="div112"></div>
        </template>
        <div id="div113">
          <div id="div1131"></div>
          <div id="div1132" class="a"></div>
        </div>
      </div>
      <div id="div12" class="a"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div11", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div11111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1131", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1132", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  unsigned start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div112")
      ->setAttribute(html_names::kClassAttr, AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  unsigned element_count =
      GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedByNonSubjectHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div11111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1131", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1132", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div1111")
      ->setAttribute(html_names::kClassAttr, AtomicString("c"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedByNonSubjectHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div11111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1131", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1132", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div111")
      ->setAttribute(html_names::kClassAttr, AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedByNonSubjectHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div11111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, false},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1131", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1132", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div1112")
      ->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(3U, element_count);

  CheckAffectedByFlagsForHas(
      "div1", {{kAffectedByNonSubjectHas, false},
               {kAncestorsOrAncestorSiblingsAffectedByHas, false},
               {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas("div11",
                             {{kAffectedByNonSubjectHas, true},
                              {kAncestorsOrAncestorSiblingsAffectedByHas, true},
                              {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div11111",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div1112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHasInShadowTree(
      "div11", "div112",
      {{kAffectedByNonSubjectHas, false},
       {kAncestorsOrAncestorSiblingsAffectedByHas, true},
       {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div113", {{kAffectedByNonSubjectHas, false},
                 {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                 {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1131", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div1132", {{kAffectedByNonSubjectHas, false},
                  {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                  {kSiblingsAffectedByHas, false}});
  CheckAffectedByFlagsForHas(
      "div12", {{kAffectedByNonSubjectHas, false},
                {kAncestorsOrAncestorSiblingsAffectedByHas, false},
                {kSiblingsAffectedByHas, false}});

  start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div1111")
      ->setAttribute(html_names::kClassAttr, AtomicString("b"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(1U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetShadowTreeElementById("div11", "div111")
      ->setAttribute(html_names::kClassAttr, AtomicString("a"));
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(3U, element_count);

  start_count = GetStyleEngine().StyleForElementCount();
  GetElementById("div12")->setAttribute(html_names::kClassAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  element_count = GetStyleEngine().StyleForElementCount() - start_count;
  ASSERT_EQ(0U, element_count);
}

}  // namespace blink
