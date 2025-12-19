// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class OverscrollAreaTrackerTest : public testing::Test,
                                  ScopedCSSOverscrollGesturesForTest {
 public:
  OverscrollAreaTrackerTest() : ScopedCSSOverscrollGesturesForTest(true) {}

  void SetUp() override {
    dummy_page_holder_ =
        std::make_unique<DummyPageHolder>(gfx::Size(0, 0), nullptr);
  }

  Document& GetDocument() { return dummy_page_holder_->GetDocument(); }

  void SetInnerHTML(const char* html) {
    GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(html);
  }

  void UpdateAllLifecyclePhasesForTest() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  OverscrollAreaTracker* OverscrollAreaTrackerById(const char* id) {
    if (auto* element = GetDocument().getElementById(AtomicString(id))) {
      return element->OverscrollAreaTracker();
    }
    return nullptr;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(OverscrollAreaTrackerTest, AddOverscrollAreaPopulatedManually) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div id="menu"></div>
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto& area_tracker = GetDocument()
                           .getElementById(AtomicString("container"))
                           ->EnsureOverscrollAreaTracker();
  Element* menu = GetDocument().getElementById(AtomicString("menu"));

  EXPECT_FALSE(area_tracker.NeedsLayoutTreeRebuild());
  area_tracker.AddOverscroll(menu);
  EXPECT_TRUE(area_tracker.NeedsLayoutTreeRebuild());
  EXPECT_EQ(area_tracker.DOMSortedElements().size(), 1u);
  EXPECT_EQ(area_tracker.DOMSortedElements()[0], menu);

  area_tracker.ClearNeedsLayoutTreeRebuild();
  EXPECT_FALSE(area_tracker.NeedsLayoutTreeRebuild());
}

TEST_F(OverscrollAreaTrackerTest, AddOverscrollAreaOneChild) {
  AtomicString tests[] = {
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div id="menu"></div>
  <button command="toggle-overscroll" commandfor="menu">
</div>
    )HTML")},
      {AtomicString(R"HTML(
<button command="toggle-overscroll" commandfor="menu">
<div id="container" overscrollcontainer>
  <div id="menu"></div>
</div>
    )HTML")},
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div id="menu"></div>
</div>
<button command="toggle-overscroll" commandfor="menu">
    )HTML")},
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div><div>
    <div id="menu"></div>
  </div></div>
  <button command="toggle-overscroll" commandfor="menu">
</div>
    )HTML")},
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
    <div id="menu"><button command="toggle-overscroll" commandfor="menu"></div>
</div>
    )HTML")},
      {AtomicString(R"HTML(
<div id="ancestor" overscrollcontainer>
  <div id="container" overscrollcontainer>
      <div id="menu"><button command="toggle-overscroll" commandfor="menu"></div>
  </div>
</div>
    )HTML")},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.Utf8().c_str());

    SetInnerHTML(test.Utf8().c_str());
    UpdateAllLifecyclePhasesForTest();

    auto* area_tracker = OverscrollAreaTrackerById("container");
    ASSERT_TRUE(area_tracker);

    Element* menu = GetDocument().getElementById(AtomicString("menu"));

    EXPECT_EQ(area_tracker->DOMSortedElements().size(), 1u);
    EXPECT_EQ(area_tracker->DOMSortedElements()[0], menu);
  }
}

TEST_F(OverscrollAreaTrackerTest, EmptyCommandForIsNotValid) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div id=""></div>
      <button command="toggle-overscroll" commandfor="">
    </div>)HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(GetDocument().OverscrollCommandTargets().empty());
  EXPECT_FALSE(GetDocument()
                   .getElementById(AtomicString("container"))
                   ->OverscrollAreaTracker());
}

TEST_F(OverscrollAreaTrackerTest, MultipleElementsPerController) {
  AtomicString tests[] = {
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div id="menu1"></div>
  <div id="menu2"></div>
  <button command="toggle-overscroll" commandfor="menu1">
  <button command="toggle-overscroll" commandfor="menu2">
</div>
    )HTML")},
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div id="menu1"></div>
  <div id="menu2"></div>
  <button command="toggle-overscroll" commandfor="menu2">
  <button command="toggle-overscroll" commandfor="menu1">
</div>
    )HTML")},
      {AtomicString(R"HTML(
<div id="container" overscrollcontainer>
  <div id="menu1">
    <button command="toggle-overscroll" commandfor="menu2">
  </div>
  <div id="menu2">
    <button command="toggle-overscroll" commandfor="menu1">
  </div>
</div>
    )HTML")},
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.Utf8().c_str());

    SetInnerHTML(test.Utf8().c_str());
    UpdateAllLifecyclePhasesForTest();

    auto* area_tracker = OverscrollAreaTrackerById("container");
    ASSERT_TRUE(area_tracker);

    Element* menu1 = GetDocument().getElementById(AtomicString("menu1"));
    Element* menu2 = GetDocument().getElementById(AtomicString("menu2"));

    EXPECT_EQ(area_tracker->DOMSortedElements().size(), 2u);
    EXPECT_EQ(area_tracker->DOMSortedElements()[0], menu1);
    EXPECT_EQ(area_tracker->DOMSortedElements()[1], menu2);
  }
}

TEST_F(OverscrollAreaTrackerTest, ChangingContainer) {
  SetInnerHTML(R"HTML(
    <div id="container0">
      <div id="container1" overscrollcontainer>
        <div id="container2">
          <div id="menu0"></div>
        </div>
        <div id="menu1"></div>
      </div>
    </div>
    <button command="toggle-overscroll" commandfor="menu0"></button>
    <button command="toggle-overscroll" commandfor="menu1"></button>
  )HTML");

  auto* container0 = GetDocument().getElementById(AtomicString("container0"));
  auto& c0tracker = container0->EnsureOverscrollAreaTracker();
  auto* container1 = GetDocument().getElementById(AtomicString("container1"));
  auto& c1tracker = container1->EnsureOverscrollAreaTracker();
  auto* container2 = GetDocument().getElementById(AtomicString("container2"));
  auto& c2tracker = container2->EnsureOverscrollAreaTracker();

  auto* menu0 = GetDocument().getElementById(AtomicString("menu0"));
  auto* menu1 = GetDocument().getElementById(AtomicString("menu1"));

  auto mark_container = [&](Element* e) {
    e->SetAttributeWithoutValidation(html_names::kOverscrollcontainerAttr,
                                     AtomicString(""));
  };
  auto clear_container = [&](Element* e) {
    e->SetAttributeWithoutValidation(html_names::kOverscrollcontainerAttr,
                                     g_null_atom);
  };

  // container1 is a container.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 2);
  EXPECT_EQ(c1tracker.DOMSortedElements()[0], menu0);
  EXPECT_EQ(c1tracker.DOMSortedElements()[1], menu1);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 0);

  mark_container(container2);
  // container1 and container2 are containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c1tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c2tracker.DOMSortedElements()[0], menu0);

  clear_container(container1);
  mark_container(container0);
  // container0 and container2 are containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c0tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c2tracker.DOMSortedElements()[0], menu0);

  clear_container(container2);
  // container0 is a container.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 2);
  EXPECT_EQ(c0tracker.DOMSortedElements()[0], menu0);
  EXPECT_EQ(c0tracker.DOMSortedElements()[1], menu1);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 0);

  clear_container(container0);
  // There are no containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 0);

  mark_container(container0);
  mark_container(container1);
  mark_container(container2);
  // container0, container1, and container2 are containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c1tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c2tracker.DOMSortedElements()[0], menu0);
}

TEST_F(OverscrollAreaTrackerTest, OverscrollElementsAreDOMSorted) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div><div id="menu1"></div></div>
      <div><div id="menu2"></div></div>
      <div><div id="menu3"></div></div>
    </div>
    <button id="button1" command="toggle-overscroll" commandfor="menu1"></button>
    <button id="button2" command="toggle-overscroll" commandfor="menu2"></button>
    <button id="button3" command="toggle-overscroll" commandfor="menu3"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto& tracker = container->EnsureOverscrollAreaTracker();

  auto* menu1 = GetDocument().getElementById(AtomicString("menu1"));
  auto* menu2 = GetDocument().getElementById(AtomicString("menu2"));
  auto* menu3 = GetDocument().getElementById(AtomicString("menu3"));

  auto* button1 = GetDocument().getElementById(AtomicString("button1"));
  auto* button2 = GetDocument().getElementById(AtomicString("button2"));
  auto* button3 = GetDocument().getElementById(AtomicString("button3"));

  auto verify_order = [&](const char* scope) {
    SCOPED_TRACE(scope);
    EXPECT_EQ(tracker.DOMSortedElements().size(), 3);
    EXPECT_EQ(tracker.DOMSortedElements()[0], menu1);
    EXPECT_EQ(tracker.DOMSortedElements()[1], menu2);
    EXPECT_EQ(tracker.DOMSortedElements()[2], menu3);
  };

  verify_order("initial");

  button2->SetAttributeWithoutValidation(html_names::kCommandAttr, "--foo");
  UpdateAllLifecyclePhasesForTest();
  button2->SetAttributeWithoutValidation(html_names::kCommandAttr,
                                         "toggle-overscroll");
  UpdateAllLifecyclePhasesForTest();

  verify_order("menu2 command changed");

  menu1->SetAttributeWithoutValidation(html_names::kIdAttr, "foo");
  UpdateAllLifecyclePhasesForTest();
  menu1->SetAttributeWithoutValidation(html_names::kIdAttr, "menu1");
  UpdateAllLifecyclePhasesForTest();

  verify_order("menu1 id changed");

  button3->SetAttributeWithoutValidation(html_names::kCommandforAttr, "foo");
  UpdateAllLifecyclePhasesForTest();
  button3->SetAttributeWithoutValidation(html_names::kCommandforAttr, "menu3");
  UpdateAllLifecyclePhasesForTest();

  verify_order("button3 commandfor changed");

  button1->SetAttributeWithoutValidation(html_names::kCommandforAttr, "menu3");
  UpdateAllLifecyclePhasesForTest();
  button3->SetAttributeWithoutValidation(html_names::kCommandforAttr, "menu1");
  UpdateAllLifecyclePhasesForTest();

  verify_order("button1 and button3 commandfor swapped");

  // Our dom order after this should be "menu2 menu3 menu1"
  container->AppendChild(menu1);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker.DOMSortedElements().size(), 3);
  EXPECT_EQ(tracker.DOMSortedElements()[0], menu2);
  EXPECT_EQ(tracker.DOMSortedElements()[1], menu3);
  EXPECT_EQ(tracker.DOMSortedElements()[2], menu1);

  // Our dom order after this should be "menu2 menu1"
  menu3->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker.DOMSortedElements().size(), 2);
  EXPECT_EQ(tracker.DOMSortedElements()[0], menu2);
  EXPECT_EQ(tracker.DOMSortedElements()[1], menu1);

  // Our dom order after this should be "menu1"
  menu2->SetAttributeWithoutValidation(html_names::kIdAttr, "");
  menu2->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(tracker.DOMSortedElements()[0], menu1);
}

TEST_F(OverscrollAreaTrackerTest, MultipleIdsReferToFirstElement) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div id="first"></div>
      <div id="second"></div>
      <div id="third"></div>
      <button command="toggle-overscroll" commandfor="menu">
    </div>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* first = GetDocument().getElementById(AtomicString("first"));
  auto* second = GetDocument().getElementById(AtomicString("second"));
  auto* third = GetDocument().getElementById(AtomicString("third"));

  first->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  third->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), first);
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_TRUE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  second->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), first);
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_TRUE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  first->SetAttributeWithoutValidation(html_names::kIdAttr, "foo");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), second);
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_FALSE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_TRUE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  second->SetAttributeWithoutValidation(html_names::kIdAttr, "foo");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), third);
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_FALSE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_TRUE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  first->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), first);
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_TRUE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  first->SetAttributeWithoutValidation(html_names::kIdAttr, "foo");
  third->SetAttributeWithoutValidation(html_names::kIdAttr, "foo");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(GetDocument().getElementById(AtomicString("menu")), nullptr);
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_FALSE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  first->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  second->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  third->SetAttributeWithoutValidation(html_names::kIdAttr, "menu");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_TRUE(first->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_FALSE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());

  first->remove();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*first));
  EXPECT_FALSE(first->GetComputedStyle());
  EXPECT_TRUE(SelectorChecker::MatchesOverscrollTarget(*second));
  EXPECT_TRUE(second->ComputedStyleRef().IsInternalOverscrollPositionAuto());
  EXPECT_FALSE(SelectorChecker::MatchesOverscrollTarget(*third));
  EXPECT_FALSE(third->ComputedStyleRef().IsInternalOverscrollPositionAuto());
}

}  // namespace blink
