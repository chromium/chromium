// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

class FocusgroupControllerTest : public PageTestBase {
 public:
  KeyboardEvent* KeyDownEvent(
      int dom_key,
      Element* target = nullptr,
      WebInputEvent::Modifiers modifiers = WebInputEvent::kNoModifiers) {
    WebKeyboardEvent web_event = {WebInputEvent::Type::kRawKeyDown, modifiers,
                                  WebInputEvent::GetStaticTimeStampForTests()};
    web_event.dom_key = dom_key;
    auto* event = KeyboardEvent::Create(web_event, nullptr);
    if (target)
      event->SetTarget(target);

    return event;
  }

  void SendEvent(KeyboardEvent* event) {
    GetDocument().GetFrame()->GetEventHandler().DefaultKeyboardEventHandler(
        event);
  }

  void AssertForwardDoesntMoveFocusWhenOutOfFocusgroup(int key);
  void AssertForwardDoesntMoveFocusWhenOnFocusgroupRoot(int key);
  void AssertForwardMovesToNextItem(int key);
  void AssertForwardDoesntMoveWhenOnlyOneItem(int key);
  void AssertForwardDoesntMoveWhenOnlyOneItemAndWraps(int key);
  void AssertForwardSkipsNonFocusableItems(int key);
  void AssertForwardMovesInExtendingFocusgroup(int key);
  void AssertForwardExitsExtendingFocusgroup(int key);
  void AssertForwardMovesToNextElementWithinDescendants(int key);
  void AssertForwardDoesntMoveFocusWhenAxisNotSupported(int key);
  void AssertForwardMovesFocusWhenInArrowAxisOnlyFocusgroup(int key);
  void AssertForwardSkipsExtendingFocusgroup(int key);
  void AssertForwardDoesntWrapWhenNotSupported(int key);
  void AssertForwardDoesntWrapEvenWhenOtherAxisSupported(int key);
  void AssertForwardWrapsSuccessfully(int key);
  void AssertForwardWrapsToParentFocusgroup(int key);
  void AssertForwardWrapsInInnerFocusgroupOnly(int key);
  void AssertForwardWrapsInExpectedScope(int key);
  void AssertForwardWrapsAndGoesInInnerFocusgroup(int key);
  void AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(int key);

 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  ScopedFocusgroupForTest focusgroup_enabled{true};
};

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventValid) {
  // Arrow right should be forward and horizontal.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardHorizontal);

  // Arrow down should be forward and vertical.
  event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardVertical);

  // Arrow left should be backward and horizontal.
  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardHorizontal);

  // Arrow up should be backward and vertical.
  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardVertical);

  // When the shift key is pressed, even when combined with a valid arrow key,
  // it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kShiftKey);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the ctrl key is pressed, even when combined with a valid arrow key, it
  // should return kNone.
  event =
      KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kControlKey);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the meta key (e.g.: CMD on mac) is pressed, even when combined with a
  // valid arrow key, it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kMetaKey);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // Any other key than an arrow key should return kNone.
  event = KeyDownEvent(ui::DomKey::TAB);
  EXPECT_EQ(FocusgroupControllerUtils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenNoFocusedElement) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  ASSERT_EQ(GetDocument().FocusedElement(), nullptr);

  // Since there are no focused element, the arrow down event shouldn't move the
  // focus.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), nullptr);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenModifierKeyIsSet) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  // 1. Set the focus on an item of the focusgroup.
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // 2. Send an "ArrowDown" event from that element.
  auto* event =
      KeyDownEvent(ui::DomKey::ARROW_DOWN, item1, WebInputEvent::kShiftKey);
  SendEvent(event);

  // 3. The focus shouldn't have moved because of the shift key.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, DontMoveFocusWhenItAlreadyMoved) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=0></span>
      <span tabindex=-1></span>
    </div>
  )HTML");
  // 1. Set the focus on an item of the focusgroup.
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item2);
  item2->focus();

  // 2. Create the "ArrowDown" event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, item2);

  // 3. Move the focus to a different element before we send the event.
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // 4. Pass the event we created earlier to our FocusgroupController. The
  // controller shouldn't even try to move the focus since the focus isn't on
  // the element that triggered the arrow key press event.
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// TODO(bebeaudr): All tests starting with "DISABLED_TDD_" are expected to
// purposefully fail but should eventually be enabled and pass. As part of our
// efforts on implementing the focusgroup feature, we adopt a test driven
// development approach to guide our implementation of the core parts of the
// algorithm.

// *****************************************************************************
// FORWARD NAVIGATION - VERTICAL AXIS (DOWN ARROW & RIGHT ARROW)
// *****************************************************************************

// When the focus is set on an element outside of the focusgroup, an arrow key
// press shouldn't move the focus at all.
void FocusgroupControllerTest::AssertForwardDoesntMoveFocusWhenOutOfFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <span id=out tabindex=-1></span>
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* out = GetElementById("out");
  ASSERT_TRUE(out);
  out->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, out);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), out);
}

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveFocusWhenOutOfFocusgroup) {
  AssertForwardDoesntMoveFocusWhenOutOfFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntMoveFocusWhenOutOfFocusgroup) {
  AssertForwardDoesntMoveFocusWhenOutOfFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the root of a focusgroup element, an arrow key press
// shouldn't move the focus at all.
void FocusgroupControllerTest::AssertForwardDoesntMoveFocusWhenOnFocusgroupRoot(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root tabindex=-1 focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* root = GetElementById("root");
  ASSERT_TRUE(root);
  root->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, root);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), root);
}

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveFocusWhenOnFocusgroupRoot) {
  AssertForwardDoesntMoveFocusWhenOnFocusgroupRoot(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightDoesntMoveFocusWhenOnFocusgroupRoot) {
  AssertForwardDoesntMoveFocusWhenOnFocusgroupRoot(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on a focusgroup item, an arrow key press should move
// the focus to the next item.
void FocusgroupControllerTest::AssertForwardMovesToNextItem(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the next sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowDownMovesToNextItem) {
  AssertForwardMovesToNextItem(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowRightMovesToNextItem) {
  AssertForwardMovesToNextItem(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the only focusgroup item, the focus shouldn't move
// and we shouldn't get stuck in an infinite loop.
void FocusgroupControllerTest::AssertForwardDoesntMoveWhenOnlyOneItem(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownDoesntMoveWhenOnlyOneItem) {
  AssertForwardDoesntMoveWhenOnlyOneItem(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightDoesntMoveWhenOnlyOneItem) {
  AssertForwardDoesntMoveWhenOnlyOneItem(ui::DomKey::ARROW_RIGHT);
}
// When the focus is set on the only focusgroup item and the focusgroup wraps in
// the axis of the arrow key pressed, the focus shouldn't move and we shouldn't
// get stuck in an infinite loop.
void FocusgroupControllerTest::AssertForwardDoesntMoveWhenOnlyOneItemAndWraps(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownDoesntMoveWhenOnlyOneItemAndWraps) {
  AssertForwardDoesntMoveWhenOnlyOneItemAndWraps(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightDoesntMoveWhenOnlyOneItemAndWraps) {
  AssertForwardDoesntMoveWhenOnlyOneItemAndWraps(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on a focusgroup item, an arrow key press should move
// the focus to the next item and skip non-focusable items.
void FocusgroupControllerTest::AssertForwardSkipsNonFocusableItems(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2></span> <!--NOT FOCUSABLE-->
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item3 = GetElementById("item3");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item3);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the next focusable sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowDownSkipsNonFocusableItems) {
  AssertForwardSkipsNonFocusableItems(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightSkipsNonFocusableItems) {
  AssertForwardSkipsNonFocusableItems(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on a focusgroup item which happens to also be an
// extending focusgroup, an arrow key press should move the focus to the next
// item within the extending focusgroup and skip non-focusable items.
void FocusgroupControllerTest::AssertForwardMovesInExtendingFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <div id=item1 tabindex=0 focusgroup=extend>
        <span id=item2></span> <!--NOT FOCUSABLE-->
        <span id=item3 tabindex=-1></span>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item3 = GetElementById("item3");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item3);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the first *focusable* item withing the
  // extending focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownMovesInExtendingFocusgroup) {
  AssertForwardMovesInExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightMovesInExtendingFocusgroup) {
  AssertForwardMovesInExtendingFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on a focusgroup item which happens to also be an
// extending focusgroup, an arrow key press should move the focus to the next
// item within the extending focusgroup and skip non-focusable items. If no
// valid candidate is found within that extending focusgroup, the next element
// (in pre-order traversal) should be considered. In this case, |item4| is the
// valid next candidate.
void FocusgroupControllerTest::AssertForwardExitsExtendingFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <div id=item1 tabindex=0 focusgroup=extend>
        <span id=item2></span> <!--NOT FOCUSABLE-->
        <span id=item3></span> <!--NOT FOCUSABLE-->
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to |item1|'s next sibling, |item4|.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownExitsExtendingFocusgroup) {
  AssertForwardExitsExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightExitsExtendingFocusgroup) {
  AssertForwardExitsExtendingFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on a focusgroup item that is an ancestor to an
// extending focusgroup, the focus should move to the next element inside that
// extending focusgroup even if it's not a direct child.
void FocusgroupControllerTest::AssertForwardMovesToNextElementWithinDescendants(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <div id=item1 tabindex=0>
        <div>
          <div focusgroup=extend>
            <span id=item2 tabindex=-1><span>
          </div>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownMovesToNextElementWithinDescendants) {
  AssertForwardMovesToNextElementWithinDescendants(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightMovesToNextElementWithinDescendants) {
  AssertForwardMovesToNextElementWithinDescendants(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on an item of a focusgroup that only supports the
// orthogonal axis to the arrow key pressed, the arrow pressed shouldn't move
// the focus.
void FocusgroupControllerTest::AssertForwardDoesntMoveFocusWhenAxisNotSupported(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // Arrow in the vertical axis, set the test to support only horizontal.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=horizontal>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
    )HTML");
  } else {
    // Arrow in the horizontal axis, set the test to support only vertical.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=vertical>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
    )HTML");
  }
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownDoesntMoveFocusWhenAxisNotSupported) {
  AssertForwardDoesntMoveFocusWhenAxisNotSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightDoesntMoveFocusWhenAxisNotSupported) {
  AssertForwardDoesntMoveFocusWhenAxisNotSupported(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on an item of a focusgroup that only supports the
// axis of the arrow key pressed the focus should move.
void FocusgroupControllerTest::
    AssertForwardMovesFocusWhenInArrowAxisOnlyFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow is in the vertical axis, so the focusgroup should support only
    // the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=vertical>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the focusgroup should support
    // only the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=horizontal>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus should have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownMovesFocusWhenInArrowAxisOnlyFocusgroup) {
  AssertForwardMovesFocusWhenInArrowAxisOnlyFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightMovesFocusWhenInArrowAxisOnlyFocusgroup) {
  AssertForwardMovesFocusWhenInArrowAxisOnlyFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on an extending focusgroup element but that focusgroup
// doesn't support the axis of the arrow key pressed, skip that subtree
// altogether.
void FocusgroupControllerTest::AssertForwardSkipsExtendingFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow is in the vertical axis, so the extending focusgroup should
    // support only the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <div id=item1 tabindex=0 focusgroup="extend horizontal">
        <span id=item2 tabindex=-1></span>
      </div>
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the extending focusgroup should
    // support only the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <div id=item1 tabindex=0 focusgroup="extend vertical">
        <span id=item2 tabindex=-1></span>
      </div>
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  auto* item3 = GetElementById("item3");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item3);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus shouldn't go into |item1|'s subtree, but should go to its next
  // sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownSkipsExtendingFocusgroup) {
  AssertForwardSkipsExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightSkipsExtendingFocusgroup) {
  AssertForwardSkipsExtendingFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of a focusgroup that doesn't support
// wrapping in the axis of the arrow key pressed, the focus shouldn't move.
void FocusgroupControllerTest::AssertForwardDoesntWrapWhenNotSupported(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item2);
  item2->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownDoesntWrapWhenNotSupported) {
  AssertForwardDoesntWrapWhenNotSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightDoesntWrapWhenNotSupported) {
  AssertForwardDoesntWrapWhenNotSupported(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of a focusgroup that doesn't support
// wrapping in the axis of the arrow key pressed but supports wrapping in the
// orthogonal axis, the focus shouldn't move.
void FocusgroupControllerTest::
    AssertForwardDoesntWrapEvenWhenOtherAxisSupported(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow is in the vertical axis, so the focusgroup that wraps should be
    // in the horizontal axis only.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup="horizontal wrap">
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1 focusgroup=extend>
        <!--This fg supports both axes, but only wraps in the horizontal one.-->
        <span id=item3 tabindex=-1></span>
        <span id=item4 tabindex=-1></span>
      </div>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the focusgroup that wraps should
    // be
    // in the vertical axis only.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup="vertical wrap">
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1 focusgroup=extend>
        <!--This fg supports both axes, but only wraps in the vertical one.-->
        <span id=item3 tabindex=-1></span>
        <span id=item4 tabindex=-1></span>
      </div>
    </div>
  )HTML");
  }
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownDoesntWrapEvenWhenOtherAxisSupported) {
  AssertForwardDoesntWrapEvenWhenOtherAxisSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightDoesntWrapEvenWhenOtherAxisSupported) {
  AssertForwardDoesntWrapEvenWhenOtherAxisSupported(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of a focusgroup that supports wrapping
// in the axis of the arrow key pressed, the focus should move back to the first
// item.
void FocusgroupControllerTest::AssertForwardWrapsSuccessfully(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item2->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // Focus should have moved back to the first item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowDownWrapsSuccessfully) {
  AssertForwardWrapsSuccessfully(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowRightWrapsSuccessfully) {
  AssertForwardWrapsSuccessfully(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup also does, the focus should move to the
// first item of the parent focusgroup.
void FocusgroupControllerTest::AssertForwardWrapsToParentFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1 focusgroup=extend>
        <span id=item3 tabindex=-1></span>
        <span id=item4 tabindex=-1></span>
      </div>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the parent focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownWrapsToParentFocusgroup) {
  AssertForwardWrapsToParentFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightWrapsToParentFocusgroup) {
  AssertForwardWrapsToParentFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup doesn't (in the axis of the arrow key
// pressed), the focus should move to the first item of the inner focusgroup.
void FocusgroupControllerTest::AssertForwardWrapsInInnerFocusgroupOnly(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow key is in the vertical axis, so the outer focusgroup should
    // only support the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup="horizontal wrap">
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1 focusgroup="extend wrap">
        <!--This fg supports wrapping in both axis, but only extend the wrapping
            behavior of its parent in the horizontal axis. -->
        <span id=item3 tabindex=-1></span>
        <span id=item4 tabindex=-1></span>
      </div>
    </div>
  )HTML");
  } else {
    // The arrow key is in the horizontal axis, so the outer focusgroup should
    // only support the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup="vertical wrap">
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1 focusgroup="extend wrap">
        <!--This fg supports wrapping in both axis, but only extend the wrapping
            behavior of its parent in the vertical axis. -->
        <span id=item3 tabindex=-1></span>
        <span id=item4 tabindex=-1></span>
      </div>
    </div>
  )HTML");
  }
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownWrapsInInnerFocusgroupOnly) {
  AssertForwardWrapsInInnerFocusgroupOnly(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightWrapsInInnerFocusgroupOnly) {
  AssertForwardWrapsInInnerFocusgroupOnly(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup doesn't (in the axis of the arrow key
// pressed), the focus should move to the first item of the inner focusgroup
// even if there's another focusgroup supporting wrapping in the same axis as
// the arrow key pressed in the hierarchy.
void FocusgroupControllerTest::AssertForwardWrapsInExpectedScope(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow key supports the vertical axis, so the outer focusgroup should
    // only support horizontal wrapping.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap> <!--Supports vertical wrapping-->
      <div focusgroup="extend horizontal"> <!--Doesn't support vertical wrap-->
        <span id=item1 tabindex=0></span>
        <div id=item2 tabindex=-1 focusgroup="extend wrap">
          <!--This fg supports wrapping in both axis, but only extend the
              wrapping behavior of its ancestors in the horizontal axis. -->
          <span id=item3 tabindex=-1></span>
          <span id=item4 tabindex=-1></span>
        </div>
      </div>
    </div>
  )HTML");
  } else {
    // The arrow key supports the horizontal axis, so the outer focusgroup
    // should only support vertical wrapping.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap> <!--Supports horizontal wrapping-->
      <div focusgroup="extend vertical"> <!--Doesn't support horizontal wrap-->
        <span id=item1 tabindex=0></span>
        <div id=item2 tabindex=-1 focusgroup="extend wrap">
          <!--This fg supports wrapping in both axis, but only extend the
              wrapping behavior of its ancestors in the vertical axis. -->
          <span id=item3 tabindex=-1></span>
          <span id=item4 tabindex=-1></span>
        </div>
      </div>
    </div>
  )HTML");
  }
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowDownWrapsInExpectedScope) {
  AssertForwardWrapsInExpectedScope(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, DISABLED_TDD_ArrowRightWrapsInExpectedScope) {
  AssertForwardWrapsInExpectedScope(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of a focusgroup that supports
// wrapping in the axis of the arrow key pressed and the first item is in an
// inner focusgroup that supports it too, the focus moves to that item in the
// inner focusgroup.
void FocusgroupControllerTest::AssertForwardWrapsAndGoesInInnerFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <div focusgroup=extend>
        <span id=item1 tabindex=-1></span>
        <span id=item2 tabindex=-1></span>
      </div>
      <span id=item3 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item3 = GetElementById("item3");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item3);
  item3->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item3);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownWrapsAndGoesInInnerFocusgroup) {
  AssertForwardWrapsAndGoesInInnerFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightWrapsAndGoesInInnerFocusgroup) {
  AssertForwardWrapsAndGoesInInnerFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// When the focus is set on the last item of a focusgroup that supports
// wrapping in the axis of the arrow key pressed and the first item is in an
// inner focusgroup that doesn't support wrapping in the same axis, the focus
// moves to the next item out of that inner focusgroup.
void FocusgroupControllerTest::
    AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  if (key == ui::DomKey::ARROW_DOWN) {
    // The arrow key is in the vertical axis, so the inner focusgroup should
    // only support the horizontal one.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <div focusgroup="extend horizontal">
        <span id=item1 tabindex=-1></span>
        <span id=item2 tabindex=-1></span>
      </div>
      <span id=item3 tabindex=-1></span>
      <span id=item4 tabindex=0></span>
    </div>
  )HTML");
  } else {
    // The arrow key is in the horizontal axis, so the inner focusgroup should
    // only support the vertical one.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <div focusgroup="extend vertical">
        <span id=item1 tabindex=-1></span>
        <span id=item2 tabindex=-1></span>
      </div>
      <span id=item3 tabindex=-1></span>
      <span id=item4 tabindex=0></span>
    </div>
  )HTML");
  }
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element after the inner focusgroup
  // that doesn't support wrapping in the arrow axis.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowDownWrapsAndSkipsOrthogonalInnerFocusgroup) {
  AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       DISABLED_TDD_ArrowRightWrapsAndSkipsOrthogonalInnerFocusgroup) {
  AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(ui::DomKey::ARROW_RIGHT);
}

}  // namespace blink
