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
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FocusControllerTest : public PageTestBase {
 public:
  Element* FindFocusableElementAfter(Element& element,
                                     mojom::blink::FocusType type) {
    if (type != mojom::blink::FocusType::kForward &&
        type != mojom::blink::FocusType::kBackward) {
      return nullptr;
    }
    element.GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kFocus);
    FocusController::OwnerMap owner_map;
    return GetFocusController().FindFocusableElementForImeAutofillAndTesting(
        type, element, owner_map);
  }

 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }
};

TEST_F(FocusControllerTest, SetInitialFocus) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<input><textarea>");
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<input id='first'><div id='second'></div><input id='third'><div "
      "id='fourth' tabindex='0'></div>");
  Element* first = GetElementById("first");
  Element* second = GetElementById("second");
  Element* third = GetElementById("third");
  Element* fourth = GetElementById("fourth");
  EXPECT_EQ(third, FindFocusableElementAfter(
                       *first, mojom::blink::FocusType::kForward));
  EXPECT_EQ(third, FindFocusableElementAfter(
                       *second, mojom::blink::FocusType::kForward));
  EXPECT_EQ(fourth, FindFocusableElementAfter(
                        *third, mojom::blink::FocusType::kForward));
  EXPECT_EQ(nullptr, FindFocusableElementAfter(
                         *fourth, mojom::blink::FocusType::kForward));

  EXPECT_EQ(nullptr, FindFocusableElementAfter(
                         *first, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(first, FindFocusableElementAfter(
                       *second, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(first, FindFocusableElementAfter(
                       *third, mojom::blink::FocusType::kBackward));
  EXPECT_EQ(third, FindFocusableElementAfter(
                       *fourth, mojom::blink::FocusType::kBackward));

  EXPECT_EQ(nullptr,
            FindFocusableElementAfter(*first, mojom::blink::FocusType::kNone));
}

TEST_F(FocusControllerTest, NextFocusableElementForImeAndAutofill) {
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
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

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      String::FromUTF8(main_html));
  auto* host = To<Element>(GetDocument().body()->firstChild());
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.SetInnerHTMLWithoutTrustedTypes(
      String::FromUTF8("<slot></slot>"));

  Element* inner1 = GetDocument().QuerySelector(AtomicString("#inner1"));
  Element* inner2 = GetDocument().QuerySelector(AtomicString("#inner2"));
  auto* slot =
      To<HTMLSlotElement>(shadow_root.QuerySelector(AtomicString("slot")));

  EXPECT_EQ(
      nullptr,
      FocusController::FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(
          *host));
  EXPECT_EQ(
      nullptr,
      FocusController::FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(
          *slot));
  EXPECT_EQ(
      slot,
      FocusController::FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(
          *inner1));
  EXPECT_EQ(
      slot,
      FocusController::FindScopeOwnerSlotOrScrollMarkerOrReadingFlowContainer(
          *inner2));
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
  shadow_root.SetInnerHTMLWithoutTrustedTypes("<div tabindex=0></div>");
  To<Element>(shadow_root.firstChild())->Focus();

  controller.SetActive(true);
  controller.SetFocused(true);
  GetDocument().UpdateStyleAndLayoutTree();
  const auto* style = host->GetComputedStyle();
  EXPECT_EQ(Color(0xA0, 0xA0, 0xA0),
            style->VisitedDependentColor(GetCSSPropertyColor()));
}

TEST_F(FocusControllerTest, FocusCanBeEmulated) {
  SetBodyInnerHTML("<div id=host></div>");
  auto& controller = GetFocusController();
  controller.SetFocused(false);
  EXPECT_FALSE(controller.IsDocumentFocused(GetDocument()));

  controller.SetFocusEmulationEnabled(true);
  EXPECT_TRUE(controller.IsDocumentFocused(GetDocument()));
}

TEST_F(FocusControllerTest, FocusIsRestoredAfterEmulation) {
  SetBodyInnerHTML("<div id=host></div>");
  auto& controller = GetFocusController();
  controller.SetFocused(false);
  controller.SetFocusEmulationEnabled(true);
  EXPECT_TRUE(controller.IsDocumentFocused(GetDocument()));

  controller.SetFocusEmulationEnabled(false);
  EXPECT_FALSE(controller.IsDocumentFocused(GetDocument()));
}

TEST_F(FocusControllerTest, FocusIsRestoredAfterNavigation) {
  SetBodyInnerHTML("<div id=host></div>");
  auto& controller = GetFocusController();
  controller.SetFocused(false);
  controller.SetFocusEmulationEnabled(true);
  EXPECT_TRUE(controller.IsDocumentFocused(GetDocument()));
  // This is similar to what happens during a navigation.
  controller.SetFocusedFrame(nullptr);
  controller.UpdateFocusOnNavigationCommit(GetDocument().GetFrame(), false);
  // The navigation logic ends.
  EXPECT_TRUE(controller.IsDocumentFocused(GetDocument()));
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

TEST_F(FocusControllerTest, FullCarouselFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 50px; height: 100px; }
      .before { scroll-marker-group: before; }
      .after { scroll-marker-group: after; }
      .scroller::scroll-marker-group { height: 100px; }
      .scroller::scroll-button(block-start) { content: "u"; }
      .scroller::scroll-button(inline-start) { content: "l"; }
      .scroller::scroll-button(inline-end) { content: "r"; }
      .scroller::scroll-button(block-end) { content: "d"; }
      .scroller::scroll-button(block-start):focus { outline: 1px solid blue; }
      .scroller::scroll-button(inline-start):focus { outline: 1px solid blue; }
      .scroller::scroll-button(inline-end):focus { outline: 1px solid blue; }
      .scroller::scroll-button(block-end):focus { outline: 1px solid blue; }
      .item { width: 100px; height: 100px; }
      .item::scroll-marker:focus { outline: 1px solid blue; opacity: 0.5; }
      .item::scroll-marker { content: "*" }
      .item::scroll-marker:target-current { color: red; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  before_scroller->setScrollTop(10);
  before_scroller->setScrollLeft(10);
  after_scroller->setScrollTop(10);
  after_scroller->setScrollLeft(10);
  UpdateAllLifecyclePhasesForTest();

  Element* before_block_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* before_inline_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* before_inline_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* before_block_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child = before_first_child->nextElementSibling();

  auto* before_scroll_marker_group = To<ScrollMarkerGroupPseudoElement>(
      before_scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore));
  Element* before_first_scroll_marker =
      before_first_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* before_second_scroll_marker =
      before_second_child->GetPseudoElement(kPseudoIdScrollMarker);

  Element* after_block_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* after_inline_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* after_inline_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* after_block_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_second_child =
      after_scroller->firstElementChild()->nextElementSibling();
  Element* after_last_child = after_scroller->lastElementChild();

  auto* after_scroll_marker_group = To<ScrollMarkerGroupPseudoElement>(
      after_scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter));
  Element* after_first_scroll_marker =
      after_first_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* after_second_scroll_marker =
      after_second_child->GetPseudoElement(kPseudoIdScrollMarker);

  std::array<Element*, 18> order = {pre_input,
                                    before_first_scroll_marker,
                                    before_block_start_button,
                                    before_inline_start_button,
                                    before_inline_end_button,
                                    before_block_end_button,
                                    before_scroller,
                                    before_first_child,
                                    before_second_child,
                                    after_first_scroll_marker,
                                    after_block_start_button,
                                    after_inline_start_button,
                                    after_inline_end_button,
                                    after_block_end_button,
                                    after_scroller,
                                    after_first_child,
                                    after_last_child,
                                    post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }

  before_second_scroll_marker->Focus();
  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  before_scroll_marker_group->ActivateScrollMarker(
      To<ScrollMarkerPseudoElement>(before_second_scroll_marker));
  const auto* style = before_second_scroll_marker->GetComputedStyle();
  EXPECT_TRUE(before_second_scroll_marker->IsFocused());
  EXPECT_EQ(0.5, style->Opacity());
  EXPECT_EQ(before_block_start_button,
            FindFocusableElementAfter(*before_second_scroll_marker,
                                      mojom::blink::FocusType::kForward));
  // https://drafts.csswg.org/css-overflow-5/#scroll-target-focus
  // When a scroll marker is activated, the next tabindex-ordered focus
  // navigation will focus the scroll target if it is focusable, otherwise, it
  // will find the next focusable element from the scroll target as though it
  // were focused.
  EXPECT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            before_second_child);
  GetFocusController().AdvanceFocus(mojom::blink::FocusType::kForward,
                                    /*source_capabilities=*/nullptr);
  EXPECT_TRUE(before_second_child->IsFocused());

  after_scroll_marker_group->ActivateScrollMarker(
      To<ScrollMarkerPseudoElement>(after_second_scroll_marker));
  // Should go to the last child of after scroller, as it is the first focusable
  // after second child of after scroller.
  EXPECT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            after_second_child);
  GetFocusController().AdvanceFocus(mojom::blink::FocusType::kForward,
                                    /*source_capabilities=*/nullptr);
  EXPECT_TRUE(after_last_child->IsFocused());
}

TEST_F(FocusControllerTest,
       CarouselWithOnlyButtonsFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 50px; height: 100px; }
      .scroller::scroll-button(block-start) { content: "u"; }
      .scroller::scroll-button(inline-start) { content: "l"; }
      .scroller::scroll-button(inline-end) { content: "r"; }
      .scroller::scroll-button(block-end) { content: "d"; }
      .scroller::scroll-button(block-start):focus { outline: 1px solid blue; opacity: 0.5; }
      .scroller::scroll-button(inline-start):focus { outline: 1px solid blue; }
      .scroller::scroll-button(inline-end):focus { outline: 1px solid blue; }
      .scroller::scroll-button(block-end):focus { outline: 1px solid blue; }
      .item { width: 100px; height: 100px; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  before_scroller->setScrollTop(10);
  before_scroller->setScrollLeft(10);
  after_scroller->setScrollTop(10);
  after_scroller->setScrollLeft(10);
  UpdateAllLifecyclePhasesForTest();

  Element* before_block_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* before_inline_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* before_inline_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* before_block_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child =
      before_scroller->firstElementChild()->nextElementSibling();

  Element* after_block_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* after_inline_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* after_inline_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* after_block_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_last_child = after_scroller->lastElementChild();

  std::array<Element*, 16> order = {pre_input,
                                    before_block_start_button,
                                    before_inline_start_button,
                                    before_inline_end_button,
                                    before_block_end_button,
                                    before_scroller,
                                    before_first_child,
                                    before_second_child,
                                    after_block_start_button,
                                    after_inline_start_button,
                                    after_inline_end_button,
                                    after_block_end_button,
                                    after_scroller,
                                    after_first_child,
                                    after_last_child,
                                    post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }

  before_block_start_button->Focus();
  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  const auto* style = before_block_start_button->GetComputedStyle();
  EXPECT_TRUE(before_block_start_button->IsFocused());
  EXPECT_EQ(0.5, style->Opacity());
}

TEST_F(FocusControllerTest,
       CarouselWithOnlyScrollMarkersFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 100px; height: 100px; }
      .before { scroll-marker-group: before; }
      .after { scroll-marker-group: after; }
      .scroller::scroll-marker-group { height: 100px; }
      .item { width: 100px; height: 100px; }
      .item::scroll-marker:focus { outline: 1px solid blue; opacity: 0.5; }
      .item::scroll-marker { content: "*" }
      .item::scroll-marker:target-current { color: red; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child =
      before_scroller->firstElementChild()->nextElementSibling();

  Element* before_first_scroll_marker =
      before_first_child->GetPseudoElement(kPseudoIdScrollMarker);

  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_last_child = after_scroller->lastElementChild();

  Element* after_first_scroll_marker =
      after_first_child->GetPseudoElement(kPseudoIdScrollMarker);

  std::array<Element*, 10> order = {
      pre_input,          before_first_scroll_marker, before_scroller,
      before_first_child, before_second_child,        after_first_scroll_marker,
      after_scroller,     after_first_child,          after_last_child,
      post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }
}

TEST_F(
    FocusControllerTest,
    CarouselWithOnlyScrollMarkersAndChildrenFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 100px; height: 100px; }
      .before { scroll-marker-group: before; }
      .after { scroll-marker-group: after; }
      .scroller::scroll-marker-group { height: 100px; }
      .item { width: 100px; height: 100px; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child =
      before_scroller->firstElementChild()->nextElementSibling();


  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_last_child = after_scroller->lastElementChild();

  std::array<Element*, 8> order = {pre_input,          before_scroller,
                                   before_first_child, before_second_child,
                                   after_scroller,     after_first_child,
                                   after_last_child,   post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }
}

TEST_F(FocusControllerTest,
       CarouselWithOnlyScrollMarkerGroupFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 100px; height: 100px; }
      .before { scroll-marker-group: before; }
      .after { scroll-marker-group: after; }
      .scroller::scroll-marker-group { height: 100px; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0"></div>

    <div id="after-scroller" class="after scroller" tabindex="0"></div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  std::array<Element*, 4> order = {pre_input, before_scroller, after_scroller,
                                   post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }
}

TEST_F(FocusControllerTest,
       FullCarouselWithExtraPseudoElementsFocusOrderPreScrollMarkerGroupMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(false);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 50px; height: 100px; }
      .before { scroll-marker-group: before; }
      .after { scroll-marker-group: after; }
      .scroller::after { content: "after"; }
      .scroller::before { content: "before"; }
      .scroller::scroll-marker-group { height: 100px; }
      .scroller::scroll-button(block-start) { content: "u"; }
      .scroller::scroll-button(inline-start) { content: "l"; }
      .scroller::scroll-button(inline-end) { content: "r"; }
      .scroller::scroll-button(block-end) { content: "d"; }
      .scroller::scroll-button(block-start):focus { outline: 1px solid blue; }
      .scroller::scroll-button(inline-start):focus { outline: 1px solid blue; }
      .scroller::scroll-button(inline-end):focus { outline: 1px solid blue; }
      .scroller::scroll-button(block-end):focus { outline: 1px solid blue; }
      .item { width: 100px; height: 100px; }
      .item::scroll-marker:focus { outline: 1px solid blue; opacity: 0.5; }
      .item::scroll-marker { content: "*" }
      .item::scroll-marker:target-current { color: red; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  before_scroller->setScrollTop(10);
  before_scroller->setScrollLeft(10);
  after_scroller->setScrollTop(10);
  after_scroller->setScrollLeft(10);
  UpdateAllLifecyclePhasesForTest();

  Element* before_block_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* before_inline_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* before_inline_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* before_block_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child =
      before_scroller->firstElementChild()->nextElementSibling();

  Element* before_first_scroll_marker =
      before_first_child->GetPseudoElement(kPseudoIdScrollMarker);

  Element* after_block_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* after_inline_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* after_inline_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* after_block_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_last_child = after_scroller->lastElementChild();

  Element* after_first_scroll_marker =
      after_first_child->GetPseudoElement(kPseudoIdScrollMarker);

  std::array<Element*, 18> order = {pre_input,
                                    before_first_scroll_marker,
                                    before_block_start_button,
                                    before_inline_start_button,
                                    before_inline_end_button,
                                    before_block_end_button,
                                    before_scroller,
                                    before_first_child,
                                    before_second_child,
                                    after_first_scroll_marker,
                                    after_block_start_button,
                                    after_inline_start_button,
                                    after_inline_end_button,
                                    after_block_end_button,
                                    after_scroller,
                                    after_first_child,
                                    after_last_child,
                                    post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }
}

TEST_F(FocusControllerTest, FullCarouselFocusOrderInLinksMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 50px; height: 100px; }
      .before { scroll-marker-group: before links; }
      .after { scroll-marker-group: after links; }
      .scroller::scroll-marker-group { height: 100px; }
      .scroller::scroll-button(block-start) { content: "u"; }
      .scroller::scroll-button(inline-start) { content: "l"; }
      .scroller::scroll-button(inline-end) { content: "r"; }
      .scroller::scroll-button(block-end) { content: "d"; }
      .item { width: 100px; height: 100px; }
      .item::scroll-marker { content: "*" }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item" tabindex="0">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  before_scroller->setScrollTop(10);
  before_scroller->setScrollLeft(10);
  after_scroller->setScrollTop(10);
  after_scroller->setScrollLeft(10);
  UpdateAllLifecyclePhasesForTest();

  Element* before_block_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* before_inline_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* before_inline_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* before_block_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child = before_first_child->nextElementSibling();
  Element* before_third_child = before_second_child->nextElementSibling();

  auto* before_scroll_marker_group = To<ScrollMarkerGroupPseudoElement>(
      before_scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore));
  Element* before_first_scroll_marker =
      before_first_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* before_second_scroll_marker =
      before_second_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* before_third_scroll_marker =
      before_third_child->GetPseudoElement(kPseudoIdScrollMarker);

  Element* after_block_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* after_inline_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* after_inline_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* after_block_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* after_first_child = after_scroller->firstElementChild();
  Element* after_second_child =
      after_scroller->firstElementChild()->nextElementSibling();
  Element* after_third_child = after_scroller->lastElementChild();

  Element* after_first_scroll_marker =
      after_first_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* after_second_scroll_marker =
      after_second_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* after_third_scroll_marker =
      after_third_child->GetPseudoElement(kPseudoIdScrollMarker);

  std::array<Element*, 22> order = {pre_input,
                                    before_first_scroll_marker,
                                    before_second_scroll_marker,
                                    before_third_scroll_marker,
                                    before_block_start_button,
                                    before_inline_start_button,
                                    before_inline_end_button,
                                    before_block_end_button,
                                    before_scroller,
                                    before_first_child,
                                    before_second_child,
                                    after_first_scroll_marker,
                                    after_second_scroll_marker,
                                    after_third_scroll_marker,
                                    after_block_start_button,
                                    after_inline_start_button,
                                    after_inline_end_button,
                                    after_block_end_button,
                                    after_scroller,
                                    after_first_child,
                                    after_third_child,
                                    post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }

  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  before_scroll_marker_group->ActivateScrollMarker(
      To<ScrollMarkerPseudoElement>(before_second_scroll_marker));
  // When in `links` mode, we should loose focus from ::scroll-marker.
  EXPECT_FALSE(before_second_scroll_marker->IsFocused());
  // When ::scroll-marker in `links` mode is activated, the next
  // tabindex-ordered focus navigation will focus the scroll target if it is
  // focusable, otherwise, it will find the next focusable element from the
  // scroll target as though it were focused, which is the first ::scroll-marker
  // of the second scroller.
  EXPECT_EQ(GetDocument().SequentialFocusNavigationStartingPoint(
                mojom::blink::FocusType::kForward),
            before_second_child);
  GetFocusController().AdvanceFocus(mojom::blink::FocusType::kForward,
                                    /*source_capabilities=*/nullptr);
  EXPECT_TRUE(after_first_scroll_marker->IsFocused());
}

TEST_F(FocusControllerTest, FullCarouselFocusOrderInTabsMode) {
  ScopedCSSScrollMarkerGroupModesForTest feature(true);

  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <style>
      .scroller { overflow: hidden; width: 50px; height: 100px; }
      .before { scroll-marker-group: before tabs; }
      .after { scroll-marker-group: after tabs; }
      .scroller::scroll-marker-group { height: 100px; }
      .scroller::scroll-button(block-start) { content: "u"; }
      .scroller::scroll-button(inline-start) { content: "l"; }
      .scroller::scroll-button(inline-end) { content: "r"; }
      .scroller::scroll-button(block-end) { content: "d"; }
      .item { width: 100px; height: 100px; }
      .item::scroll-marker:focus { outline: 1px solid blue; opacity: 0.5; }
      .item::scroll-marker { content: "*" }
      .item::scroll-marker:target-current { color: red; }
    </style>
    <input id="pre-input">
    <div id="before-scroller" class="before scroller" tabindex="0">
      <div id="01" class="item" tabindex="0">1</div>
      <div id="02" class="item" tabindex="0">2</div>
      <div id="03" class="item">3</div>
    </div>

    <div id="after-scroller" class="after scroller" tabindex="0">
      <div id="11" class="item">1</div>
      <div id="12" class="item">2</div>
      <div id="13" class="item" tabindex="0">3</div>
    </div>
    <input id="post-input">
  )HTML");

  UpdateAllLifecyclePhasesForTest();
  Element* pre_input = GetElementById("pre-input");
  Element* before_scroller = GetElementById("before-scroller");
  Element* after_scroller = GetElementById("after-scroller");
  Element* post_input = GetElementById("post-input");

  before_scroller->setScrollTop(10);
  before_scroller->setScrollLeft(10);
  after_scroller->setScrollTop(10);
  after_scroller->setScrollLeft(10);
  UpdateAllLifecyclePhasesForTest();

  Element* before_block_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* before_inline_start_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* before_inline_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* before_block_end_button =
      before_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* before_first_child = before_scroller->firstElementChild();
  Element* before_second_child = before_first_child->nextElementSibling();

  auto* before_scroll_marker_group = To<ScrollMarkerGroupPseudoElement>(
      before_scroller->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore));
  Element* before_first_scroll_marker =
      before_first_child->GetPseudoElement(kPseudoIdScrollMarker);
  Element* before_second_scroll_marker =
      before_second_child->GetPseudoElement(kPseudoIdScrollMarker);

  Element* after_block_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockStart);
  Element* after_inline_start_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineStart);
  Element* after_inline_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonInlineEnd);
  Element* after_block_end_button =
      after_scroller->GetPseudoElement(kPseudoIdScrollButtonBlockEnd);

  Element* after_first_child = after_scroller->firstElementChild();

  Element* after_first_scroll_marker =
      after_first_child->GetPseudoElement(kPseudoIdScrollMarker);

  std::array<Element*, 15> order = {pre_input,
                                    before_first_scroll_marker,
                                    before_first_child,
                                    before_block_start_button,
                                    before_inline_start_button,
                                    before_inline_end_button,
                                    before_block_end_button,
                                    before_scroller,
                                    after_first_scroll_marker,
                                    after_block_start_button,
                                    after_inline_start_button,
                                    after_inline_end_button,
                                    after_block_end_button,
                                    after_scroller,
                                    post_input};

  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i + 1], FindFocusableElementAfter(
                                *order[i], mojom::blink::FocusType::kForward));
  }
  for (std::size_t i = 0u; i < order.size() - 1; ++i) {
    EXPECT_EQ(order[i], FindFocusableElementAfter(
                            *order[i + 1], mojom::blink::FocusType::kBackward));
  }

  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);
  // When in `tabs` mode, we should keep the focus on ::scroll-marker.
  before_scroll_marker_group->ActivateScrollMarker(
      To<ScrollMarkerPseudoElement>(before_second_scroll_marker));
  const auto* style = before_second_scroll_marker->GetComputedStyle();
  EXPECT_TRUE(before_second_scroll_marker->IsFocused());
  EXPECT_EQ(0.5, style->Opacity());
  // And the next in focus order we go to the ultimate originating element of
  // selected scroll marker.
  EXPECT_EQ(before_second_child,
            FindFocusableElementAfter(*before_second_scroll_marker,
                                      mojom::blink::FocusType::kForward));
}

}  // namespace blink
