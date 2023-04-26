// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focus_controller.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/element.h"
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

// Test for FocusController::FindScopeOwnerSlot().
TEST_F(FocusControllerTest, FindScopeOwnerSlot) {
  const char* main_html =
      "<div id='host'>"
      "<div id='inner1'></div>"
      "<div id='inner2'></div>"
      "</div>";

  GetDocument().body()->setInnerHTML(String::FromUTF8(main_html));
  auto* host = To<Element>(GetDocument().body()->firstChild());
  ShadowRoot& shadow_root =
      host->AttachShadowRootInternal(ShadowRootType::kOpen);
  shadow_root.setInnerHTML(String::FromUTF8("<slot></slot>"));

  Element* inner1 = GetDocument().QuerySelector("#inner1");
  Element* inner2 = GetDocument().QuerySelector("#inner2");
  auto* slot = To<HTMLSlotElement>(shadow_root.QuerySelector("slot"));

  EXPECT_EQ(nullptr, FocusController::FindScopeOwnerSlot(*host));
  EXPECT_EQ(nullptr, FocusController::FindScopeOwnerSlot(*slot));
  EXPECT_EQ(slot, FocusController::FindScopeOwnerSlot(*inner1));
  EXPECT_EQ(slot, FocusController::FindScopeOwnerSlot(*inner2));
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
  Element* checkbox = child_document->getElementById("checkbox");
  ASSERT_TRUE(checkbox);

  // |NextFocusableElementForImeAndAutofill| finds another element that needs
  // user input - don't auto-submit after filling in the username and password
  // fields.
  EXPECT_EQ(checkbox,
            GetFocusController().NextFocusableElementForImeAndAutofill(
                password, mojom::blink::FocusType::kForward));
}

}  // namespace blink
