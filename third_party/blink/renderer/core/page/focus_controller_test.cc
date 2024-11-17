// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focus_controller.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class FocusControllerTest : public PageTestBase {
 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }
};

TEST_F(FocusControllerTest, SetInitialFocus) {
  GetDocument().body()->setInnerHTML("<input><textarea>");
  auto* input = To<Element>(GetDocument().body()->firstChild());
  // Set sequential focus navigation point before the initial focus.
  input->Focus();
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
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
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
  host->AttachShadowRootForTesting(ShadowRootMode::kOpen);

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

  Element* next = GetFocusController().NextFocusableElementForImeAndAutofill(
      first, mojom::blink::FocusType::kForward);
  EXPECT_EQ(next, last)
      << "SVG Element should be skipped even when focusable in form.";

  Element* prev = GetFocusController().NextFocusableElementForImeAndAutofill(
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

TEST_F(FocusControllerTest, NextFocusableElementForImeAndAutofill) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "  <input type='text' id='username'>"
      "  <input type='password' id='password'>"
      "  <input type='submit' value='Login'>"
      "</form>");
  Element* username = GetElementById("username");
  Element* password = GetElementById("password");
  ASSERT_TRUE(username);
  ASSERT_TRUE(password);

  EXPECT_EQ(password,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                username, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         username, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         password, mojom::blink::FocusType::kForward));
  EXPECT_EQ(username,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kBackward));
}

TEST_F(FocusControllerTest, NextFocusableElementForImeAndAutofill_NoFormTag) {
  GetDocument().body()->setInnerHTML(
      "  <input type='text' id='username'>"
      "  <input type='password' id='password'>"
      "  <input type='submit' value='Login'>");
  Element* username = GetElementById("username");
  Element* password = GetElementById("password");
  ASSERT_TRUE(username);
  ASSERT_TRUE(password);

  EXPECT_EQ(password,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                username, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         username, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         password, mojom::blink::FocusType::kForward));
  EXPECT_EQ(username,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kBackward));
}

// Ignore a checkbox to streamline form submission.
TEST_F(FocusControllerTest, NextFocusableElementForImeAndAutofill_Checkbox) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "  <input type='text' id='username'>"
      "  <input type='password' id='password'>"
      "  <input type='checkbox' id='remember-me'>"
      "  <input type='submit' value='Login'>"
      "</form>");
  Element* username = GetElementById("username");
  Element* password = GetElementById("password");
  ASSERT_TRUE(username);
  ASSERT_TRUE(password);

  EXPECT_EQ(password,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                username, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         username, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         password, mojom::blink::FocusType::kForward));
  EXPECT_EQ(username,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kBackward));
}

// A <select> element should block a form submission.
TEST_F(FocusControllerTest, NextFocusableElementForImeAndAutofill_Select) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "  <input type='text' id='username'>"
      "  <input type='password' id='password'>"
      "  <select id='login_type'>"
      "    <option value='regular'>Regular</option>"
      "    <option value='invisible'>Invisible</option>"
      "  </select>"
      "  <input type='submit' value='Login'>"
      "</form>");
  Element* username = GetElementById("username");
  Element* password = GetElementById("password");
  Element* login_type = GetElementById("login_type");
  ASSERT_TRUE(username);
  ASSERT_TRUE(password);
  ASSERT_TRUE(login_type);

  EXPECT_EQ(password,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                username, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         username, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(login_type,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kForward));
  EXPECT_EQ(username,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kBackward));
}

// A submit button is used to detect the end of a user form within a combined
// form. Combined form is a <form> element that encloses several user form (e.g.
// signin and signup). See the HTML in the test for clarity.
TEST_F(FocusControllerTest,
       NextFocusableElementForImeAndAutofill_SubmitButton) {
  GetDocument().body()->setInnerHTML(
      "<form>"
      "  <div>Login</div>"
      "    <input type='email' id='login_username'>"
      "    <input type='password' id='login_password'>"
      "    <input type='submit' id='login_submit'>"
      "  <div>Create an account</div>"
      "    <input type='email' id='signup_username'>"
      "    <input type='text' id='signup_full_name'>"
      "    <input type='password' id='signup_password'>"
      "    <button type='submit' id='signup_submit'>"
      "  <div>Forgot password?</div>"
      "    <input type='email' id='recover_username'>"
      "    <span>Request a recovery link</span>"
      "</form>");
  // "login_submit" closes the signin form.
  Element* login_password = GetElementById("login_password");
  ASSERT_TRUE(login_password);
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         login_password, mojom::blink::FocusType::kForward));
  Element* signup_username = GetElementById("signup_username");
  ASSERT_TRUE(signup_username);
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         signup_username, mojom::blink::FocusType::kBackward));

  // "signup_password" closes the signup form.
  Element* signup_password = GetElementById("signup_password");
  ASSERT_TRUE(signup_password);
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         signup_password, mojom::blink::FocusType::kForward));
  Element* recover_username = GetElementById("recover_username");
  ASSERT_TRUE(recover_username);
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         recover_username, mojom::blink::FocusType::kBackward));

  // The end of the recovery form is detected just because it the end of <form>.
  EXPECT_EQ(nullptr, GetFocusController().NextFocusableElementForImeAndAutofill(
                         recover_username, mojom::blink::FocusType::kForward));
}

// Test for FocusController::FindScopeOwnerSlotOrReadingFlowContainer().
TEST_F(FocusControllerTest, FindScopeOwnerSlotOrReadingFlowContainer) {
  const char* main_html =
      "<div id='host'>"
      "<div id='inner1'></div>"
      "<div id='inner2'></div>"
      "</div>";

  GetDocument().body()->setInnerHTML(String::FromUTF8(main_html));
  auto* host = To<Element>(GetDocument().body()->firstChild());
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8("<slot></slot>"));

  Element* inner1 = GetDocument().QuerySelector(AtomicString("#inner1"));
  Element* inner2 = GetDocument().QuerySelector(AtomicString("#inner2"));
  auto* slot =
      To<HTMLSlotElement>(shadow_root.QuerySelector(AtomicString("slot")));

  EXPECT_EQ(nullptr,
            FocusController::FindScopeOwnerSlotOrReadingFlowContainer(*host));
  EXPECT_EQ(nullptr,
            FocusController::FindScopeOwnerSlotOrReadingFlowContainer(*slot));
  EXPECT_EQ(slot,
            FocusController::FindScopeOwnerSlotOrReadingFlowContainer(*inner1));
  EXPECT_EQ(slot,
            FocusController::FindScopeOwnerSlotOrReadingFlowContainer(*inner2));
}

// crbug.com/1508258
TEST_F(FocusControllerTest, FocusHasChangedShouldInvalidateFocusStyle) {
  SetBodyInnerHTML(
      "<style>#host:focus { color:#A0A0A0; }</style>"
      "<div id=host></div>");
  auto& controller = GetFocusController();
  controller.SetFocused(false);

  auto* host = GetElementById("host");
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML("<div tabindex=0></div>");
  To<Element>(shadow_root.firstChild())->Focus();

  controller.SetActive(true);
  controller.SetFocused(true);
  GetDocument().UpdateStyleAndLayoutTree();
  const auto* style = host->GetComputedStyle();
  EXPECT_EQ(Color(0xA0, 0xA0, 0xA0),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}

class FocusControllerTestWithIframes : public RenderingTest {
 public:
  FocusControllerTestWithIframes()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

// A captcha should block a form submission.
TEST_F(FocusControllerTestWithIframes,
       NextFocusableElementForImeAndAutofill_Captcha) {
  SetBodyInnerHTML(
      "<!DOCTYPE html>"
      "<form>"
      "  <input type='text' id='username'>"
      "  <input type='password' id='password'>"
      "  <iframe id='captcha' src='https://captcha.com'></iframe>"
      "  <button type='submit' value='Login'>"
      "</form>");
  SetChildFrameHTML(
      "<!DOCTYPE html>"
      "<div id='checkbox' tabindex='0'>");
  UpdateAllLifecyclePhasesForTest();

  Element* password = GetElementById("password");
  ASSERT_TRUE(password);

  LocalFrame* child_frame = To<LocalFrame>(GetFrame().Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);
  Element* checkbox = child_document->getElementById(AtomicString("checkbox"));
  ASSERT_TRUE(checkbox);

  // |NextFocusableElementForImeAndAutofill| finds another element that needs
  // user input - don't auto-submit after filling in the username and password
  // fields.
  EXPECT_EQ(checkbox,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kForward));
}

TEST_F(FocusControllerTest, ScrollMarkersAreFocusable) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow-y: scroll;
        width: 200px;
        height: 200px;
        scroll-marker-group: after;
        &::scroll-marker-group {
          display: block;
          height: 100px;
        }
        div { height: 200px; }
        div::scroll-marker { content: '-'; }
        div::scroll-marker:focus { opacity: 0.5; }
      }
    </style>
    <input id="pre-input">
    <div id="scroller">
      <div>X</div>
      <div>Y</div>
      <div>Z</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* scroller = GetElementById("scroller");
  Element* pre_input = GetElementById("pre-input");
  Element* post_input = GetElementById("post-input");

  auto* scroll_marker_group = To<ScrollMarkerGroupPseudoElement>(
      scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter));
  ASSERT_TRUE(scroll_marker_group);

  Element* first_scroll_marker =
      scroller->firstElementChild()->GetPseudoElement(kPseudoIdScrollMarker);
  ASSERT_TRUE(first_scroll_marker);

  Element* second_scroll_marker =
      scroller->firstElementChild()->nextElementSibling()->GetPseudoElement(
          kPseudoIdScrollMarker);
  ASSERT_TRUE(second_scroll_marker);

  Element* last_scroll_marker =
      scroller->lastElementChild()->GetPseudoElement(kPseudoIdScrollMarker);
  ASSERT_TRUE(last_scroll_marker);

  EXPECT_EQ(scroller, GetFocusController().FindFocusableElementAfter(
                          *pre_input, mojom::blink::FocusType::kForward));
  EXPECT_EQ(first_scroll_marker,
            GetFocusController().FindFocusableElementAfter(
                *scroller, mojom::blink::FocusType::kForward));
  EXPECT_EQ(post_input,
            GetFocusController().FindFocusableElementAfter(
                *first_scroll_marker, mojom::blink::FocusType::kForward));

  EXPECT_EQ(pre_input, GetFocusController().FindFocusableElementAfter(
                           *scroller, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(scroller,
            GetFocusController().FindFocusableElementAfter(
                *first_scroll_marker, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(first_scroll_marker,
            GetFocusController().FindFocusableElementAfter(
                *post_input, mojom::blink::FocusType::kBackward));

  second_scroll_marker->Focus();
  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  scroll_marker_group->SetSelected(
      *To<ScrollMarkerPseudoElement>(second_scroll_marker));
  const auto* style = second_scroll_marker->GetComputedStyle();
  EXPECT_TRUE(second_scroll_marker->IsFocused());
  EXPECT_EQ(0.5, style->Opacity());

  // Focusgroup restores last focused element.
  EXPECT_EQ(scroller, GetFocusController().FindFocusableElementAfter(
                          *pre_input, mojom::blink::FocusType::kForward));
  EXPECT_EQ(second_scroll_marker,
            GetFocusController().FindFocusableElementAfter(
                *scroller, mojom::blink::FocusType::kForward));
  EXPECT_EQ(second_scroll_marker,
            GetFocusController().FindFocusableElementAfter(
                *post_input, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(post_input,
            GetFocusController().FindFocusableElementAfter(
                *second_scroll_marker, mojom::blink::FocusType::kForward));
  EXPECT_EQ(scroller,
            GetFocusController().FindFocusableElementAfter(
                *second_scroll_marker, mojom::blink::FocusType::kBackward));
}

TEST_F(FocusControllerTest, ScrollButtonsAreFocusable) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <style>
      #scroller {
        overflow: scroll;
        width: 200px;
        height: 200px;
        &::scroll-next-button, &::scroll-prev-button { content: "-" }
        &::scroll-next-button:focus,&::scroll-prev-button:focus { opacity: 0.5 }
      }
      #spacer { width: 400px; height: 400px; }
    </style>
    <input id="pre-input">
    <div id="scroller">
      <div id="spacer"></div>
    </div>
    <input id='post-input'>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  Element* scroller = GetElementById("scroller");
  Element* pre_input = GetElementById("pre-input");
  Element* post_input = GetElementById("post-input");

  PseudoElement* scroll_button_next =
      scroller->GetPseudoElement(kPseudoIdScrollNextButton);
  ASSERT_TRUE(scroll_button_next);
  PseudoElement* scroll_button_prev =
      scroller->GetPseudoElement(kPseudoIdScrollPrevButton);
  ASSERT_TRUE(scroll_button_prev);

  EXPECT_EQ(scroller, GetFocusController().FindFocusableElementAfter(
                          *pre_input, mojom::blink::FocusType::kForward));
  EXPECT_EQ(scroll_button_prev,
            GetFocusController().FindFocusableElementAfter(
                *scroller, mojom::blink::FocusType::kForward));
  EXPECT_EQ(scroll_button_next,
            GetFocusController().FindFocusableElementAfter(
                *scroll_button_prev, mojom::blink::FocusType::kForward));
  EXPECT_EQ(post_input,
            GetFocusController().FindFocusableElementAfter(
                *scroll_button_next, mojom::blink::FocusType::kForward));

  EXPECT_EQ(pre_input, GetFocusController().FindFocusableElementAfter(
                           *scroller, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(scroller,
            GetFocusController().FindFocusableElementAfter(
                *scroll_button_prev, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(scroll_button_prev,
            GetFocusController().FindFocusableElementAfter(
                *scroll_button_next, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(scroll_button_next,
            GetFocusController().FindFocusableElementAfter(
                *post_input, mojom::blink::FocusType::kBackward));

  scroll_button_prev->Focus();
  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);

  const ComputedStyle* style = scroll_button_prev->GetComputedStyle();
  EXPECT_TRUE(scroll_button_prev->IsFocused());
  EXPECT_EQ(0.5, style->Opacity());
}

}  // namespace blink
