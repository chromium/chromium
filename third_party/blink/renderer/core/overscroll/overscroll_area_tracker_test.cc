// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_event_init.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/indexed_pseudo_element.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/clip_paint_property_node.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink {

class OverscrollAreaTrackerTest : public testing::Test,
                                  ScopedOverscrollGesturesForTest {
 public:
  OverscrollAreaTrackerTest() : ScopedOverscrollGesturesForTest(true) {}

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
      return element->GetOverscrollAreaTracker();
    }
    return nullptr;
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

class OverscrollAreaTrackerPageTest : public PageTestBase,
                                      public testing::WithParamInterface<int>,
                                      ScopedOverscrollGesturesForTest {
 public:
  OverscrollAreaTrackerPageTest() : ScopedOverscrollGesturesForTest(true) {}
};

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

TEST_F(OverscrollAreaTrackerTest, OverscrollAreaAttribute) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div id="menu" overscrollarea></div>
    </div>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* area_tracker = OverscrollAreaTrackerById("container");
  ASSERT_TRUE(area_tracker);
  Element* menu = GetDocument().getElementById(AtomicString("menu"));
  EXPECT_EQ(area_tracker->DOMSortedElements().size(), 1u);
  EXPECT_EQ(area_tracker->DOMSortedElements()[0], menu);
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
                   ->GetOverscrollAreaTracker());
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
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container1->GetLayoutObject());
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container1->GetLayoutObject());

  mark_container(container2);
  // container1 and container2 are containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c1tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c2tracker.DOMSortedElements()[0], menu0);
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container2->GetLayoutObject());
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container1->GetLayoutObject());

  clear_container(container1);
  mark_container(container0);
  // container0 and container2 are containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c0tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 1);
  EXPECT_EQ(c2tracker.DOMSortedElements()[0], menu0);
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container2->GetLayoutObject());
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container0->GetLayoutObject());

  clear_container(container2);
  // container0 is a container.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 2);
  EXPECT_EQ(c0tracker.DOMSortedElements()[0], menu0);
  EXPECT_EQ(c0tracker.DOMSortedElements()[1], menu1);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container0->GetLayoutObject());
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container0->GetLayoutObject());

  clear_container(container0);
  // There are no containers.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(c0tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c1tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(c2tracker.DOMSortedElements().size(), 0);
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent), nullptr);
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent), nullptr);

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
  EXPECT_EQ(menu0->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container2->GetLayoutObject());
  EXPECT_EQ(menu1->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->Parent(),
            container1->GetLayoutObject());
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

TEST_F(OverscrollAreaTrackerTest, OverscrollAreaRebuildLayoutTree) {
  SetInnerHTML(R"HTML(
    <div id="container" overscrollcontainer>
      <div><div id="menu"></div></div>
    </div>
    <button id="button" command="toggle-overscroll" commandfor="menu"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  auto* container = GetDocument().getElementById(AtomicString("container"));
  auto* button = GetDocument().getElementById(AtomicString("button"));
  auto* menu = GetDocument().getElementById(AtomicString("menu"));

  button->SetAttributeWithoutValidation(html_names::kCommandAttr, "--foo");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_FALSE(menu->GetPseudoElement(kPseudoIdOverscrollAreaParent));
  button->SetAttributeWithoutValidation(html_names::kCommandAttr,
                                        "toggle-overscroll");
  UpdateAllLifecyclePhasesForTest();

  PseudoElement* overscroll_area_parent =
      menu->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  ASSERT_TRUE(overscroll_area_parent->GetLayoutObject());
  ASSERT_EQ(overscroll_area_parent->GetLayoutObject()->Parent(),
            container->GetLayoutObject());
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

TEST_F(OverscrollAreaTrackerTest, BackdropClickDismiss) {
  SetInnerHTML(R"HTML(
    <style>
      #container {
        width: 200px;
        height: 200px;
      }
      #menu {
        width: 200%;
        height: 200%;
        left: -50%;
        top: -50%;
      }
      #menu::backdrop {
        background-color: rgba(0,0,0,0.5);
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="menu" overscrollarea></div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* area_tracker = OverscrollAreaTrackerById("container");
  ASSERT_TRUE(area_tracker);
  Element* menu = GetDocument().getElementById(AtomicString("menu"));
  ASSERT_TRUE(menu);

  area_tracker->OpenArea(menu);
  UpdateAllLifecyclePhasesForTest();

  PseudoElement* overscroll_area_parent =
      menu->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  ASSERT_TRUE(overscroll_area_parent);
  auto* scrollable_area =
      DynamicTo<LayoutBox>(overscroll_area_parent->GetLayoutObject())
          ->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);

  EXPECT_NE(scrollable_area->GetScrollOffset(), ScrollOffset());

  PseudoElement* backdrop = menu->GetPseudoElement(kPseudoIdBackdrop);
  ASSERT_TRUE(backdrop);

  PointerEventInit* init = PointerEventInit::Create();
  init->setBubbles(true);
  init->setPointerId(1);
  PointerEvent* click_event =
      PointerEvent::Create(event_type_names::kClick, init);
  backdrop->DispatchEvent(*click_event);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(scrollable_area->GetScrollOffset(), ScrollOffset());
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollPseudoElementLayoutStructure) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      div, #scroller::before {
        /* Prevent wrapping by anonymous blocks. */
        display: block;
      }
      #scroller::before {
        content: "::before pseudo";
      }
    </style>
    <div id="previous-sibling"></div>
    <div id="scroller" overscrollcontainer>
      <div id="child"></div>
      <div id="foo"></div>
      <div id="bar"></div>
    </div>
    <div id="next-sibling"></div>
    <button command="toggle-overscroll" commandfor="foo"></button>
    <button command="toggle-overscroll" commandfor="bar"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* scroller = GetElementById("scroller");
  PseudoElement* overscroll_parent_foo =
      GetElementById("foo")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  PseudoElement* overscroll_parent_bar =
      GetElementById("bar")->GetPseudoElement(kPseudoIdOverscrollAreaParent);

  ASSERT_TRUE(overscroll_parent_foo);
  ASSERT_TRUE(overscroll_parent_bar);
  EXPECT_FALSE(scroller->GetPseudoElement(kPseudoIdOverscrollAreaParent,
                                          AtomicString("--baz")));

  // Order of children and pseudos within content:
  EXPECT_EQ(scroller->GetPseudoElement(kPseudoIdBefore)
                ->GetLayoutObject()
                ->PreviousSibling(),
            overscroll_parent_bar->GetLayoutObject());
  EXPECT_EQ(GetElementById("child")->GetLayoutObject()->PreviousSibling(),
            scroller->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());

  // Overscroll area parents:
  EXPECT_EQ(overscroll_parent_bar->GetLayoutObject()->Parent(),
            scroller->GetLayoutObject());
  EXPECT_EQ(overscroll_parent_foo->GetLayoutObject()->Parent(),
            scroller->GetLayoutObject());

  // Scroller siblings:
  EXPECT_EQ(scroller->GetLayoutObject()->PreviousSibling(),
            GetElementById("previous-sibling")->GetLayoutObject());
  EXPECT_EQ(scroller->GetLayoutObject()->NextSibling(),
            GetElementById("next-sibling")->GetLayoutObject());
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollPropertyTrees) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container {
        overflow: auto;
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="foo"></div>
      <div id="bar"></div>
    </div>
    <button command="toggle-overscroll" commandfor="foo"></button>
    <button command="toggle-overscroll" commandfor="bar"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* container = GetElementById("container");
  PseudoElement* foo =
      GetElementById("foo")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  PseudoElement* bar =
      GetElementById("bar")->GetPseudoElement(kPseudoIdOverscrollAreaParent);

  // Scroll chains from the element, to the overscroll-area-parents, to the
  // root.
  HeapVector<Member<const ScrollPaintPropertyNode>> scroll_chain(
      {container->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->Scroll(),
       bar->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       foo->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       GetDocument().View()->GetPage()->GetVisualViewport().GetScrollNode()});
  for (size_t i = 1; i < scroll_chain.size(); ++i) {
    const ScrollPaintPropertyNode* child = scroll_chain[i - 1];
    const ScrollPaintPropertyNode* parent = scroll_chain[i];
    EXPECT_EQ(child->Parent(), parent);
  }

  // In non-overlay, translation should be chained through the
  // overscroll-area-parents.
  const ObjectPaintProperties* container_props =
      container->GetLayoutObject()->FirstFragment().PaintProperties();
  const ObjectPaintProperties* bar_props =
      bar->GetLayoutObject()->FirstFragment().PaintProperties();
  const ObjectPaintProperties* foo_props =
      foo->GetLayoutObject()->FirstFragment().PaintProperties();

  HeapVector<Member<const TransformPaintPropertyNodeOrAlias>>
      overlay_transform_chain({
          container_props->ScrollTranslation(),
          container_props->ContentTranslation(),
          bar_props->ScrollTranslation(),
          bar_props->ContentTranslation(),
          bar_props->PaintOffsetTranslation(),
          foo_props->ScrollTranslation(),
          foo_props->PaintOffsetTranslation(),
          container_props->PaintOffsetTranslation(),
      });
  for (size_t i = 1; i < overlay_transform_chain.size(); ++i) {
    const TransformPaintPropertyNodeOrAlias* child =
        overlay_transform_chain[i - 1];
    const TransformPaintPropertyNodeOrAlias* parent =
        overlay_transform_chain[i];
    EXPECT_EQ(child->UnaliasedParent(), parent);
  }

  // Clip follows the scroll tree hierarchy
  HeapVector<Member<const ClipPaintPropertyNode>> overlay_clip_chain(
      {container->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->OverflowClip(),
       bar->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->OverflowClip(),
       foo->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->OverflowClip()});
  HeapVector<Member<const TransformPaintPropertyNode>> overlay_clip_transform(
      {container->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->ContentTranslation(),
       bar->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->ContentTranslation(),
       foo->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->PaintOffsetTranslation()});
  for (size_t i = 1; i < overlay_clip_chain.size(); ++i) {
    const ClipPaintPropertyNode* parent = overlay_clip_chain[i];
    EXPECT_EQ(&overlay_clip_chain[i]->LocalTransformSpace(),
              overlay_clip_transform[i]);
    if (i > 0) {
      const ClipPaintPropertyNode* child = overlay_clip_chain[i - 1];
      EXPECT_EQ(child->UnaliasedParent(), parent);
    }
  }
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollOverlayPropertyTrees) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container {
        overflow: auto;
      }
    </style>
    <div id="container" overscrollcontainer=overlay>
      <div id="foo"></div>
      <div id="bar"></div>
    </div>
    <button command="toggle-overscroll" commandfor="foo"></button>
    <button command="toggle-overscroll" commandfor="bar"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* container = GetElementById("container");
  PseudoElement* foo =
      GetElementById("foo")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  PseudoElement* bar =
      GetElementById("bar")->GetPseudoElement(kPseudoIdOverscrollAreaParent);

  // ::-internal-overscroll-area-parent skips the scrollers scroll translation.
  for (auto* pseudo_element : {foo, bar}) {
    EXPECT_EQ(pseudo_element->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation()
                  ->Parent(),
              container->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->PaintOffsetTranslation());
  }

  // Scroll chains from the element, to the overscroll-area-parents, to the
  // root.
  HeapVector<Member<const ScrollPaintPropertyNode>> scroll_chain(
      {container->GetLayoutObject()
           ->FirstFragment()
           .PaintProperties()
           ->Scroll(),
       bar->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       foo->GetLayoutObject()->FirstFragment().PaintProperties()->Scroll(),
       GetDocument().View()->GetPage()->GetVisualViewport().GetScrollNode()});
  for (size_t i = 1; i < scroll_chain.size(); ++i) {
    const ScrollPaintPropertyNode* child = scroll_chain[i - 1];
    const ScrollPaintPropertyNode* parent = scroll_chain[i];
    EXPECT_EQ(child->Parent(), parent);
  }
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollPropertyTreeInvalidation) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container {
        overflow: auto;
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="foo"></div>
    </div>
    <button command="toggle-overscroll" commandfor="foo"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* container = GetElementById("container");

  // A content translation is generated for the first fragment.
  EXPECT_TRUE(container->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->ContentTranslation());

  // Removing the overscroll container attribute removes this content
  // translation node.
  container->removeAttribute(html_names::kOverscrollcontainerAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(container->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties()
                   ->ContentTranslation());
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollPseudoElementStyles) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller {
        overflow: auto;
      }
      .smooth {
        scroll-behavior: smooth;
      }
    </style>
    <div id="scroller1" class="scroller" overscrollcontainer>
      <div id="foo"></div>
    </div>
    <div id="scroller2" class="scroller" overscrollcontainer>
      <div id="bar" class="smooth"></div>
    </div>
    <button command="toggle-overscroll" commandfor="foo"></button>
    <button command="toggle-overscroll" commandfor="bar"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  PseudoElement* overscroll_parent_foo =
      GetElementById("foo")->GetPseudoElement(kPseudoIdOverscrollAreaParent);

  ASSERT_TRUE(overscroll_parent_foo);

  // Computed style of the overscroll area parent pseudo-elements
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowX());
  EXPECT_EQ(EOverflow::kAuto,
            overscroll_parent_foo->GetComputedStyle()->OverflowY());
  EXPECT_EQ(EScrollbarWidth::kNone,
            overscroll_parent_foo->GetComputedStyle()->ScrollbarWidth());
  EXPECT_EQ(mojom::ScrollBehavior::kAuto,
            overscroll_parent_foo->GetComputedStyle()->GetScrollBehavior());

  // Computed scroll-behavior inherits scroll-behavior from the scroller.
  PseudoElement* overscroll_parent_bar =
      GetElementById("bar")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  EXPECT_EQ(mojom::ScrollBehavior::kSmooth,
            overscroll_parent_bar->GetComputedStyle()->GetScrollBehavior());
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollContainerWithElement) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container, #menu {
        width: 200px;
        height: 200px;
      }
      #menu {
        right: 200px; /* Positioned to the left */
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="menu"></div>
      <div id="content"></div>
    </div>
    <button id=button command="toggle-overscroll" commandfor="menu"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  ASSERT_TRUE(container);
  PseudoElement* overscroll_area_parent =
      GetElementById("menu")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  Element* menu = GetElementById("menu");
  Element* content = GetElementById("content");
  ASSERT_TRUE(overscroll_area_parent);
  ASSERT_TRUE(menu);
  ASSERT_TRUE(content);

  // We expect the following layout tree:
  // container
  //   overscroll-area-parent
  //     menu
  //   content
  EXPECT_EQ(menu->GetLayoutObject()->Parent(),
            overscroll_area_parent->GetLayoutObject());
  EXPECT_EQ(overscroll_area_parent->GetLayoutObject()->Parent(),
            container->GetLayoutObject());
  EXPECT_EQ(content->GetLayoutObject()->Parent(), container->GetLayoutObject());

  // We should have snap points for the menu and the initial area:
  const cc::SnapContainerData* snap_container_data =
      overscroll_area_parent->GetLayoutBox()
          ->GetScrollableArea()
          ->GetSnapContainerData();
  ASSERT_EQ(snap_container_data->size(), 2);
  const cc::SnapAreaData& parent_area_data = snap_container_data->at(0);
  EXPECT_EQ(
      parent_area_data.element_id,
      CompositorElementIdFromDOMNodeId(overscroll_area_parent->GetDomNodeId()));
  // The snap area coordinates are relative to the top-left of the scrollable
  // overflow, placing the origin scroll position at (200, 0).
  ASSERT_EQ(parent_area_data.rect, gfx::RectF(200, 0, 200, 200));
  const cc::SnapAreaData& child_data = snap_container_data->at(1);
  EXPECT_EQ(child_data.element_id,
            CompositorElementIdFromDOMNodeId(menu->GetDomNodeId()));
  ASSERT_EQ(child_data.rect, gfx::RectF(0, 0, 200, 200));

  cc::TargetSnapAreaElementIds target_ids =
      snap_container_data->GetTargetSnapAreaElementIds();
  EXPECT_EQ(target_ids.x, parent_area_data.element_id);
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollAreaChangingOrigin) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container, #menu, #menu2 {
        width: 200px;
        height: 200px;
        background: white;
      }
      #menu, #menu2 {
        right: 200px; /* Positioned to the left */
        background: gray;
      }
      .right {
        right: auto;
        left: 200px;
      }
    </style>
    <div id="container" overscrollcontainer>
      <div class="right" id="menu"></div>
      <div id="menu2"></div>
      <div id="content"></div>
    </div>
    <button id=button command="toggle-overscroll" commandfor="menu"></button>
    <button id=button command="toggle-overscroll" commandfor="menu2"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  Element* menu = GetElementById("menu");
  Element* menu2 = GetElementById("menu2");
  ASSERT_TRUE(container);
  ASSERT_TRUE(menu);
  ASSERT_TRUE(menu2);
  // Initially we require a 200px translation to offset the left-aligned menu
  // on the content.
  ASSERT_EQ(container->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            200);
  ASSERT_EQ(menu2->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            0);

  menu->classList().Remove(AtomicString("right"));
  UpdateAllLifecyclePhasesForTest();
  // Need 200px offset for both menus.
  ASSERT_EQ(container->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            200);
  ASSERT_EQ(menu2->GetPseudoElement(kPseudoIdOverscrollAreaParent)
                ->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            200);

  menu2->classList().Add(AtomicString("right"));
  UpdateAllLifecyclePhasesForTest();
  // When the menu is right aligned we should reset the translation to 0.
  ASSERT_EQ(container->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            0);

  // Remove the menus one at a time.
  menu2->remove();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(container->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation()
                ->Get2dTranslation()
                .x(),
            200);

  menu->remove();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(container->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ContentTranslation(),
            nullptr);
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollContainerStyleOnText) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="container" overscrollcontainer>
      aa
      <span></span>
      bb
    </div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  EXPECT_TRUE(container->GetLayoutObject()->IsOverscrollContainer());
  for (auto* child = container->GetLayoutObject()->SlowFirstChild(); child;
       child = child->NextSibling()) {
    EXPECT_FALSE(child->IsOverscrollContainer()) << child->DebugName();
  }
}

TEST_F(OverscrollAreaTrackerPageTest, OverscrollContainerNegativeScroll) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container {
        width: 200px;
        height: 200px;
      }
      #largeoverscrollarea {
        width: 200%;
        height: 200%;
        /* Overscrolls by 50% / 100px on all sides. */
        left: -50%;
        top: -50%;
      }
      #content {
        height: 100%;
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="largeoverscrollarea"></div>
      <div id="content"></div>
    </div>
    <button id=button command="toggle-overscroll"
        commandfor="largeoverscrollarea"></button>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* container = GetElementById("container");
  ASSERT_TRUE(container);
  PseudoElement* overscroll_area_parent =
      GetElementById("largeoverscrollarea")
          ->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  Element* content = GetElementById("content");
  ASSERT_TRUE(overscroll_area_parent);
  ASSERT_TRUE(content);

  PaintLayerScrollableArea* overscrollable_area =
      overscroll_area_parent->GetLayoutBox()->GetScrollableArea();

  // We should be able to overscroll in any direction.
  ASSERT_TRUE(overscrollable_area);
  ASSERT_EQ(overscrollable_area->MinimumScrollOffset().x(), -100);
  ASSERT_EQ(overscrollable_area->MinimumScrollOffset().y(), -100);
  ASSERT_EQ(overscrollable_area->MaximumScrollOffset().x(), 100);
  ASSERT_EQ(overscrollable_area->MaximumScrollOffset().y(), 100);

  // Scrolling to this area pushes the main content.
  overscroll_area_parent->scrollToForTesting(-100, -100);
  UpdateAllLifecyclePhasesForTest();
  DOMRect* content_rect = content->GetBoundingClientRect();
  DOMRect* container_rect = container->GetBoundingClientRect();
  ASSERT_TRUE(content_rect);
  ASSERT_EQ(content_rect->x(), container_rect->x() + 100);
  ASSERT_EQ(content_rect->y(), container_rect->y() + 100);

  // With the content scrolled to (100, 100), a click at (50, 50)
  // should only hit the outer area.
  //    -----------------------
  //   | #largeoverscrollarea  |
  //   |   X <- (50, 50)       |
  //   |          -------------|
  //   |         |  #content   |
  //   |         |             |
  //    -----------------------
  HeapVector<Member<Element>> elements_from_point =
      GetDocument().ElementsFromPoint(container_rect->x() + 50,
                                      container_rect->y() + 50);
  ASSERT_EQ(elements_from_point[0], GetElementById("largeoverscrollarea"));
  ASSERT_EQ(elements_from_point[1], container);
}

TEST_P(OverscrollAreaTrackerPageTest,
       OverscrollContainerWithElementInvalidationChecks) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      #container, #menu {
        width: 200px;
        height: 200px;
      }
      #menu {
        right: 200px;
      }
    </style>
    <div id="container" overscrollcontainer>
      <div id="menu"></div>
      <div id="content"></div>
    </div>
    <button id=button command="toggle-overscroll" commandfor="menu"></button>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  {
    Element* container = GetElementById("container");
    ASSERT_TRUE(container);
    PseudoElement* overscroll_area_parent =
        GetElementById("menu")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
    Element* menu = GetElementById("menu");
    Element* content = GetElementById("content");
    ASSERT_TRUE(overscroll_area_parent);
    ASSERT_TRUE(menu);
    ASSERT_TRUE(content);
  }

  ASSERT_LE(GetParam(), 2);
  switch (GetParam()) {
    case 0:
      GetElementById("button")->remove();
      break;
    case 1:
      GetElementById("button")->SetAttributeWithoutValidation(
          html_names::kCommandAttr, AtomicString("toggle-foo"));
      break;
    case 2:
      GetElementById("button")->removeAttribute(html_names::kCommandAttr);
      break;
  }

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(GetDocument().OverscrollCommandTargets().size(), 0u);

  Element* container = GetElementById("container");
  ASSERT_TRUE(container);
  PseudoElement* overscroll_area_parent =
      GetElementById("menu")->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  Element* menu = GetElementById("menu");
  Element* content = GetElementById("content");

  EXPECT_FALSE(overscroll_area_parent);
  EXPECT_TRUE(menu);
  EXPECT_TRUE(content);

  EXPECT_EQ(menu->GetLayoutObject()->Parent(), container->GetLayoutObject());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OverscrollAreaTrackerPageTest,
                         ::testing::Values(0, 1, 2));

}  // namespace blink
