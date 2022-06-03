// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focus_controller.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FocusControllerTest : public PageTestBase {
 private:
  void SetUp() override { PageTestBase::SetUp(IntSize()); }
};

TEST_F(FocusControllerTest, SetInitialFocus) {
  GetDocument().body()->setInnerHTML("<input><textarea>");
  auto* input = To<Element>(GetDocument().body()->firstChild());
  // Set sequential focus navigation point before the initial focus.
  input->focus();
  input->blur();
  GetFocusController().SetInitialFocus(mojom::blink::FocusType::kForward);
  EXPECT_EQ(input, GetDocument().FocusedElement())
      << "We should ignore sequential focus navigation starting point in "
         "setInitialFocus().";
}

TEST_F(FocusControllerTest, DoNotCrash1) {
  GetDocument().body()->setInnerHTML(
      "<div id='host'></div>This test is for crbug.com/609012<p id='target' "
      "tabindex='0'></p>");
  // <div> with shadow root
  auto* host = To<Element>(GetDocument().body()->firstChild());
  host->AttachShadowRootInternal(ShadowRootType::kOpen);
  // "This test is for crbug.com/609012"
  Node* text = host->nextSibling();
  // <p>
  auto* target = To<Element>(text->nextSibling());

  // Set sequential focus navigation point at text node.
  GetDocument().SetSequentialFocusNavigationStartingPoint(text);

  GetFocusController().AdvanceFocus(mojom::blink::FocusType::kForward);
  EXPECT_EQ(target, GetDocument().FocusedElement())
      << "This should not hit assertion and finish properly.";
}

TEST_F(FocusControllerTest, DoNotCrash2) {
  GetDocument().body()->setInnerHTML(
      "<p id='target' tabindex='0'></p>This test is for crbug.com/609012<div "
      "id='host'></div>");
  // <p>
  auto* target = To<Element>(GetDocument().body()->firstChild());
  // "This test is for crbug.com/609012"
  Node* text = target->nextSibling();
  // <div> with shadow root
  auto* host = To<Element>(text->nextSibling());
  host->AttachShadowRootInternal(ShadowRootType::kOpen);

  // Set sequential focus navigation point at text node.
  GetDocument().SetSequentialFocusNavigationStartingPoint(text);

  GetFocusController().AdvanceFocus(mojom::blink::FocusType::kBackward);
  EXPECT_EQ(target, GetDocument().FocusedElement())
      << "This should not hit assertion and finish properly.";
}

TEST_F(FocusControllerTest, SetActiveOnInactiveDocument) {
  // Test for crbug.com/700334
  GetDocument().Shutdown();
  // Document::shutdown() detaches document from its frame, and thus
  // document().page() becomes nullptr.
  // Use DummyPageHolder's page to retrieve FocusController.
  GetPage().GetFocusController().SetActive(true);
}

// This test is for crbug.com/733218
TEST_F(FocusControllerTest, SVGFocusableElementInForm) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "<input id='first'>"
      "<svg width='100px' height='100px' tabindex='0'>"
      "<circle cx='50' cy='50' r='30' />"
      "</svg>"
      "<input id='last'>"
      "</form>");

  auto* form = To<Element>(GetDocument().body()->firstChild());
  auto* first = To<Element>(form->firstChild());
  auto* last = To<Element>(form->lastChild());

  Element* next = GetFocusController().NextFocusableElementInForm(
      first, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next, last)
      << "SVG Element should be skipped even when focusable in form.";

  Element* prev = GetFocusController().NextFocusableElementInForm(
      next, mojom::blink::FocusType::kBackward);
  EXPECT_EQ(prev, first)
      << "SVG Element should be skipped even when focusable in form.";
}

TEST_F(FocusControllerTest, FindFocusableAfterElement) {
  GetDocument().body()->setInnerHTML(
      "<input id='first'><div id='second'></div><input id='third'><div "
      "id='fourth' tabindex='0'></div>");
  Element* first = GetElementById("first");
  Element* second = GetElementById("second");
  Element* third = GetElementById("third");
  Element* fourth = GetElementById("fourth");
  EXPECT_EQ(third, GetFocusController().FindFocusableElementAfter(
                       *first, mojom::blink::FocusType::kForward));
  EXPECT_EQ(third, GetFocusController().FindFocusableElementAfter(
                       *second, mojom::blink::FocusType::kForward));
  EXPECT_EQ(fourth, GetFocusController().FindFocusableElementAfter(
                        *third, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, GetFocusController().FindFocusableElementAfter(
                         *fourth, mojom::blink::FocusType::kForward));

  EXPECT_EQ(nullptr, GetFocusController().FindFocusableElementAfter(
                         *first, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(first, GetFocusController().FindFocusableElementAfter(
                       *second, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(first, GetFocusController().FindFocusableElementAfter(
                       *third, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(third, GetFocusController().FindFocusableElementAfter(
                       *fourth, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(nullptr, GetFocusController().FindFocusableElementAfter(
                         *first, mojom::blink::FocusType::kNone));
}

}  // namespace blink
