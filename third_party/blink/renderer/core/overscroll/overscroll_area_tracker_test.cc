// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
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

  area_tracker.AddOverscroll(menu);
  EXPECT_EQ(area_tracker.DOMSortedElements().size(), 1u);
  EXPECT_EQ(area_tracker.DOMSortedElements()[0], menu);
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
  // TODO(crbug.com/463970475): These should be DOM sorted.
  EXPECT_EQ(c0tracker.DOMSortedElements()[0], menu1);
  EXPECT_EQ(c0tracker.DOMSortedElements()[1], menu0);
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

}  // namespace blink
