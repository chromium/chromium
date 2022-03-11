// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/focusgroup_controller.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/page/focusgroup_controller_utils.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

using utils = FocusgroupControllerUtils;

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
  void AssertForwardDoesntMoveWhenOnNonFocusgroupItem(int key);
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
  void AssertForwardDoesntWrapInFocusgroupWithoutItems(int key);
  void AssertForwardWrapsSuccessfully(int key);
  void AssertForwardWrapsToParentFocusgroup(int key);
  void AssertForwardWrapsInInnerFocusgroupOnly(int key);
  void AssertForwardWrapsInExpectedScope(int key);
  void AssertForwardWrapsAndGoesInInnerFocusgroup(int key);
  void AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(int key);

  void AssertBackwardDoesntMoveFocusWhenOutOfFocusgroup(int key);
  void AssertBackwardDoesntMoveFocusWhenOnFocusgroupRoot(int key);
  void AssertBackwardDoesntMoveWhenOnNonFocusgroupItem(int key);
  void AssertBackwardMovesFocusToPreviousItem(int key);
  void AssertBackwardSkipsNonFocusableItems(int key);
  void AssertBackwardDoesntMoveWhenOnlyOneItem(int key);
  void AssertBackwardDoesntMoveWhenOnlyOneItemAndWraps(int key);
  void AssertBackwardDoesntMoveFocusAxisNotSupported(int key);
  void AssertBackwardMovesFocusWhenInArrowAxisOnlyFocusgroup(int key);
  void AssertBackwardDescendIntoExtendingFocusgroup(int key);
  void AssertBackwardSkipsNonFocusgroupSubtree(int key);
  void AssertBackwardSkipsOrthogonalFocusgroup(int key);
  void AssertBackwardSkipsRootFocusgroup(int key);
  void AssertBackwardSkipsEmptyWrappingFocusgroup(int key);
  void AssertBackwardSkipsRootFocusgroupComplexCase(int key);
  void AssertBackwardSkipsOrthogonalFocusgroupComplexCase(int key);
  void AssertBackwardAscendsToParentFocusgroup(int key);
  void AssertBackwardDoesntWrapWhenNotSupported(int key);
  void AssertBackwardWrapsSuccessfully(int key);
  void AssertBackwardWrapsSuccessfullyInAxis(int key);
  void AssertBackwardDoesntWrapInOrthogonalAxis(int key);
  void AssertBackwardWrapsSuccessfullyInExtendingFocusgroup(int key);
  void AssertBackwardWrapsSuccessfullyInComplexCase(int key);

 private:
  void SetUp() override { PageTestBase::SetUp(gfx::Size()); }

  ScopedFocusgroupForTest focusgroup_enabled{true};
};

TEST_F(FocusgroupControllerTest, FocusgroupDirectionForEventValid) {
  // Arrow right should be forward and horizontal.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardHorizontal);

  // Arrow down should be forward and vertical.
  event = KeyDownEvent(ui::DomKey::ARROW_DOWN);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kForwardVertical);

  // Arrow left should be backward and horizontal.
  event = KeyDownEvent(ui::DomKey::ARROW_LEFT);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardHorizontal);

  // Arrow up should be backward and vertical.
  event = KeyDownEvent(ui::DomKey::ARROW_UP);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kBackwardVertical);

  // When the shift key is pressed, even when combined with a valid arrow key,
  // it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kShiftKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the ctrl key is pressed, even when combined with a valid arrow key, it
  // should return kNone.
  event =
      KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kControlKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // When the meta key (e.g.: CMD on mac) is pressed, even when combined with a
  // valid arrow key, it should return kNone.
  event = KeyDownEvent(ui::DomKey::ARROW_UP, nullptr, WebInputEvent::kMetaKey);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);

  // Any other key than an arrow key should return kNone.
  event = KeyDownEvent(ui::DomKey::TAB);
  EXPECT_EQ(utils::FocusgroupDirectionForEvent(event),
            FocusgroupDirection::kNone);
}

TEST_F(FocusgroupControllerTest, IsDirectionBackward) {
  ASSERT_FALSE(utils::IsDirectionBackward(FocusgroupDirection::kNone));
  ASSERT_TRUE(
      utils::IsDirectionBackward(FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(
      utils::IsDirectionBackward(FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::IsDirectionBackward(FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(
      utils::IsDirectionBackward(FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, IsDirectionForward) {
  ASSERT_FALSE(utils::IsDirectionForward(FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::IsDirectionForward(FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::IsDirectionForward(FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(
      utils::IsDirectionForward(FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::IsDirectionForward(FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, IsDirectionHorizontal) {
  ASSERT_FALSE(utils::IsDirectionHorizontal(FocusgroupDirection::kNone));
  ASSERT_TRUE(
      utils::IsDirectionHorizontal(FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::IsDirectionHorizontal(FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(
      utils::IsDirectionHorizontal(FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(
      utils::IsDirectionHorizontal(FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, IsDirectionVertical) {
  ASSERT_FALSE(utils::IsDirectionVertical(FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::IsDirectionVertical(FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(
      utils::IsDirectionVertical(FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::IsDirectionVertical(FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(
      utils::IsDirectionVertical(FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, IsAxisSupported) {
  FocusgroupFlags flags_horizontal_only = FocusgroupFlags::kHorizontal;
  ASSERT_FALSE(utils::IsAxisSupported(flags_horizontal_only,
                                      FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsAxisSupported(flags_horizontal_only,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(utils::IsAxisSupported(flags_horizontal_only,
                                      FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(utils::IsAxisSupported(flags_horizontal_only,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::IsAxisSupported(flags_horizontal_only,
                                      FocusgroupDirection::kForwardVertical));

  FocusgroupFlags flags_vertical_only = FocusgroupFlags::kVertical;
  ASSERT_FALSE(
      utils::IsAxisSupported(flags_vertical_only, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::IsAxisSupported(
      flags_vertical_only, FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(utils::IsAxisSupported(flags_vertical_only,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(utils::IsAxisSupported(flags_vertical_only,
                                      FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::IsAxisSupported(flags_vertical_only,
                                     FocusgroupDirection::kForwardVertical));

  FocusgroupFlags flags_both_directions =
      FocusgroupFlags::kHorizontal | FocusgroupFlags::kVertical;
  ASSERT_FALSE(utils::IsAxisSupported(flags_both_directions,
                                      FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::IsAxisSupported(flags_both_directions,
                                     FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, WrapsInDirection) {
  FocusgroupFlags flags_no_wrap = FocusgroupFlags::kNone;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_no_wrap, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::WrapsInDirection(
      flags_no_wrap, FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(utils::WrapsInDirection(
      flags_no_wrap, FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::WrapsInDirection(flags_no_wrap,
                                       FocusgroupDirection::kForwardVertical));

  FocusgroupFlags flags_wrap_horizontal = FocusgroupFlags::kWrapHorizontally;
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_horizontal,
                                       FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::WrapsInDirection(
      flags_wrap_horizontal, FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_horizontal,
                                       FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_horizontal,
                                      FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::WrapsInDirection(flags_wrap_horizontal,
                                       FocusgroupDirection::kForwardVertical));

  FocusgroupFlags flags_wrap_vertical = FocusgroupFlags::kWrapVertically;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_wrap_vertical, FocusgroupDirection::kNone));
  ASSERT_FALSE(utils::WrapsInDirection(
      flags_wrap_vertical, FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_vertical,
                                      FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(utils::WrapsInDirection(
      flags_wrap_vertical, FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_vertical,
                                      FocusgroupDirection::kForwardVertical));

  FocusgroupFlags flags_wrap_both =
      FocusgroupFlags::kWrapHorizontally | FocusgroupFlags::kWrapVertically;
  ASSERT_FALSE(
      utils::WrapsInDirection(flags_wrap_both, FocusgroupDirection::kNone));
  ASSERT_TRUE(utils::WrapsInDirection(
      flags_wrap_both, FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::WrapsInDirection(flags_wrap_both,
                                      FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, FocusgroupExtendsInAxis) {
  FocusgroupFlags focusgroup = FocusgroupFlags::kNone;
  FocusgroupFlags extending_focusgroup = FocusgroupFlags::kNone;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  focusgroup |= FocusgroupFlags::kHorizontal | FocusgroupFlags::kVertical;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  extending_focusgroup |=
      FocusgroupFlags::kHorizontal | FocusgroupFlags::kVertical;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  extending_focusgroup = FocusgroupFlags::kExtend;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  extending_focusgroup |= FocusgroupFlags::kHorizontal;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  extending_focusgroup |= FocusgroupFlags::kVertical;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  focusgroup = FocusgroupFlags::kNone;
  extending_focusgroup = FocusgroupFlags::kExtend |
                         FocusgroupFlags::kHorizontal |
                         FocusgroupFlags::kVertical;

  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                              FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_FALSE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  focusgroup |= FocusgroupFlags::kVertical;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_FALSE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));

  focusgroup |= FocusgroupFlags::kHorizontal;

  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                             FocusgroupDirection::kNone));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardHorizontal));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kBackwardVertical));
  ASSERT_TRUE(
      utils::FocusgroupExtendsInAxis(extending_focusgroup, focusgroup,
                                     FocusgroupDirection::kForwardHorizontal));
  ASSERT_TRUE(utils::FocusgroupExtendsInAxis(
      extending_focusgroup, focusgroup, FocusgroupDirection::kForwardVertical));
}

TEST_F(FocusgroupControllerTest, FindNearestFocusgroupAncestor) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div>
      <span id=item1 tabindex=0></span>
    </div>
    <div id=fg1 focusgroup>
      <span id=item2 tabindex=-1></span>
      <div>
        <div id=fg2 focusgroup=extend>
          <span id=item3 tabindex=-1></span>
          <div>
            <span id=item4></span>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);

  ASSERT_EQ(utils::FindNearestFocusgroupAncestor(item1), nullptr);
  ASSERT_EQ(utils::FindNearestFocusgroupAncestor(item2), fg1);
  ASSERT_EQ(utils::FindNearestFocusgroupAncestor(item3), fg2);
  ASSERT_EQ(utils::FindNearestFocusgroupAncestor(item4), fg2);
}

TEST_F(FocusgroupControllerTest, NextElement) {
  GetDocument().body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
      <span id=item3 tabindex=-1></span>
    </div>
    <div id=fg3 focusgroup>
        <template shadowroot=open>
          <span id=item4 tabindex=-1></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* fg3 = GetElementById("fg3");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(fg3);

  auto* item1 = GetElementById("item1");
  auto* item4 = fg3->GetShadowRoot()->getElementById("item4");
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);

  ASSERT_EQ(utils::NextElement(fg1, /* skip_subtree */ false), item1);
  ASSERT_EQ(utils::NextElement(fg1, /* skip_subtree */ true), fg2);
  ASSERT_EQ(utils::NextElement(fg3, /* skip_subtree */ false), item4);
  ASSERT_EQ(utils::NextElement(item4, /* skip_subtree */ false), item5);
}

TEST_F(FocusgroupControllerTest, PreviousElement) {
  GetDocument().body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
      <span id=item3 tabindex=-1></span>
    </div>
    <div id=fg3 focusgroup>
        <template shadowroot=open>
          <span id=item4 tabindex=-1></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg3 = GetElementById("fg3");
  ASSERT_TRUE(fg3);

  auto* item3 = GetElementById("item3");
  auto* item4 = fg3->GetShadowRoot()->getElementById("item4");
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);

  ASSERT_EQ(utils::PreviousElement(item5), item4);
  ASSERT_EQ(utils::PreviousElement(item4), fg3);
  ASSERT_EQ(utils::PreviousElement(fg3), item3);
}

TEST_F(FocusgroupControllerTest, LastElementWithin) {
  GetDocument().body()->setInnerHTMLWithDeclarativeShadowDOMForTesting(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1></span>
      <span id=item2 tabindex=-1></span>
    </div>
    <div id=fg2 focusgroup>
        <template shadowroot=open>
          <span id=item3 tabindex=-1></span>
          <span id=item4></span>
        </template>
    </div>
    <span id=item5 tabindex=-1></span>
  )HTML");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);

  auto* item2 = GetElementById("item2");
  auto* item4 = fg2->GetShadowRoot()->getElementById("item4");
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item4);

  ASSERT_EQ(utils::LastElementWithin(fg1), item2);
  ASSERT_EQ(utils::LastElementWithin(fg2), item4);
  ASSERT_EQ(utils::LastElementWithin(item4), nullptr);
}

TEST_F(FocusgroupControllerTest, IsFocusgroupItem) {
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=fg1 focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2></span>
      <div id=fg2 focusgroup=extend>
        <span tabindex=-1></span>
        <div id=non-fg1 tabindex=-1>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <button id=button1></button>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* non_fg1 = GetElementById("non-fg1");
  auto* button1 = GetElementById("button1");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(non_fg1);
  ASSERT_TRUE(button1);

  ASSERT_TRUE(utils::IsFocusgroupItem(item1));
  ASSERT_FALSE(utils::IsFocusgroupItem(item2));
  ASSERT_FALSE(utils::IsFocusgroupItem(item3));
  ASSERT_FALSE(utils::IsFocusgroupItem(fg1));
  ASSERT_FALSE(utils::IsFocusgroupItem(fg2));
  ASSERT_TRUE(utils::IsFocusgroupItem(non_fg1));
  ASSERT_TRUE(utils::IsFocusgroupItem(button1));
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

// *****************************************************************************
// FORWARD NAVIGATION - DOWN ARROW & RIGHT ARROW
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

// When the focus is set on a focusable element that isn't a focusgroup item, an
// arrow key press shouldn't move the focus at all.
void FocusgroupControllerTest::AssertForwardDoesntMoveWhenOnNonFocusgroupItem(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div tabindex=-1 focusgroup>
      <div>
        <span id=nonitem1 tabindex=0></span>
      </div>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* nonitem1 = GetElementById("nonitem1");
  ASSERT_TRUE(nonitem1);
  nonitem1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, nonitem1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), nonitem1);
}

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveWhenOnNonFocusgroupItem) {
  AssertForwardDoesntMoveWhenOnNonFocusgroupItem(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntMoveWhenOnNonFocusgroupItem) {
  AssertForwardDoesntMoveWhenOnNonFocusgroupItem(ui::DomKey::ARROW_RIGHT);
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

TEST_F(FocusgroupControllerTest, ArrowDownMovesToNextItem) {
  AssertForwardMovesToNextItem(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightMovesToNextItem) {
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

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveWhenOnlyOneItem) {
  AssertForwardDoesntMoveWhenOnlyOneItem(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntMoveWhenOnlyOneItem) {
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

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveWhenOnlyOneItemAndWraps) {
  AssertForwardDoesntMoveWhenOnlyOneItemAndWraps(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntMoveWhenOnlyOneItemAndWraps) {
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

TEST_F(FocusgroupControllerTest, ArrowDownSkipsNonFocusableItems) {
  AssertForwardSkipsNonFocusableItems(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightSkipsNonFocusableItems) {
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

TEST_F(FocusgroupControllerTest, ArrowDownMovesInExtendingFocusgroup) {
  AssertForwardMovesInExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightMovesInExtendingFocusgroup) {
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

TEST_F(FocusgroupControllerTest, ArrowDownExitsExtendingFocusgroup) {
  AssertForwardExitsExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightExitsExtendingFocusgroup) {
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

TEST_F(FocusgroupControllerTest, ArrowDownMovesToNextElementWithinDescendants) {
  AssertForwardMovesToNextElementWithinDescendants(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightMovesToNextElementWithinDescendants) {
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

TEST_F(FocusgroupControllerTest, ArrowDownDoesntMoveFocusWhenAxisNotSupported) {
  AssertForwardDoesntMoveFocusWhenAxisNotSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightDoesntMoveFocusWhenAxisNotSupported) {
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
       ArrowDownMovesFocusWhenInArrowAxisOnlyFocusgroup) {
  AssertForwardMovesFocusWhenInArrowAxisOnlyFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightMovesFocusWhenInArrowAxisOnlyFocusgroup) {
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

TEST_F(FocusgroupControllerTest, ArrowDownSkipsExtendingFocusgroup) {
  AssertForwardSkipsExtendingFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightSkipsExtendingFocusgroup) {
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

TEST_F(FocusgroupControllerTest, ArrowDownDoesntWrapWhenNotSupported) {
  AssertForwardDoesntWrapWhenNotSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntWrapWhenNotSupported) {
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
       ArrowDownDoesntWrapEvenWhenOtherAxisSupported) {
  AssertForwardDoesntWrapEvenWhenOtherAxisSupported(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightDoesntWrapEvenWhenOtherAxisSupported) {
  AssertForwardDoesntWrapEvenWhenOtherAxisSupported(ui::DomKey::ARROW_RIGHT);
}

// This test validates that we don't get stuck in an infinite loop searching for
// a focusable element in the extending focusgroup that wraps that doesn't
// contain one. Wrapping should only be allowed in the focusgroup that contains
// the focusable element we started on or in one of its ancestors.
void FocusgroupControllerTest::AssertForwardDoesntWrapInFocusgroupWithoutItems(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
      <div focusgroup="extend wrap">
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

  // The focus should have moved to |item1|'s next sibling, |item4|, without
  // getting stuck looping infinitely in the wrapping focusgroup deprived of
  // focusable elements.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

TEST_F(FocusgroupControllerTest, ArrowDownDoesntWrapInFocusgroupWithoutItems) {
  AssertForwardDoesntWrapInFocusgroupWithoutItems(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightDoesntWrapInFocusgroupWithoutItems) {
  AssertForwardDoesntWrapInFocusgroupWithoutItems(ui::DomKey::ARROW_RIGHT);
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

TEST_F(FocusgroupControllerTest, ArrowDownWrapsSuccessfully) {
  AssertForwardWrapsSuccessfully(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightWrapsSuccessfully) {
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

TEST_F(FocusgroupControllerTest, ArrowDownWrapsToParentFocusgroup) {
  AssertForwardWrapsToParentFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightWrapsToParentFocusgroup) {
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

TEST_F(FocusgroupControllerTest, ArrowDownWrapsInInnerFocusgroupOnly) {
  AssertForwardWrapsInInnerFocusgroupOnly(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightWrapsInInnerFocusgroupOnly) {
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

TEST_F(FocusgroupControllerTest, ArrowDownWrapsInExpectedScope) {
  AssertForwardWrapsInExpectedScope(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightWrapsInExpectedScope) {
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

TEST_F(FocusgroupControllerTest, ArrowDownWrapsAndGoesInInnerFocusgroup) {
  AssertForwardWrapsAndGoesInInnerFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest, ArrowRightWrapsAndGoesInInnerFocusgroup) {
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
       ArrowDownWrapsAndSkipsOrthogonalInnerFocusgroup) {
  AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(ui::DomKey::ARROW_DOWN);
}

TEST_F(FocusgroupControllerTest,
       ArrowRightWrapsAndSkipsOrthogonalInnerFocusgroup) {
  AssertForwardWrapsAndSkipsOrthogonalInnerFocusgroup(ui::DomKey::ARROW_RIGHT);
}

// *****************************************************************************
// FORWARD NAVIGATION - UP ARROW & LEFT ARROW
// *****************************************************************************

// When the focus is set on an element outside of the focusgroup, an arrow key
// press shouldn't move the focus at all.
void FocusgroupControllerTest::AssertBackwardDoesntMoveFocusWhenOutOfFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
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

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveFocusWhenOutOfFocusgroup) {
  AssertBackwardDoesntMoveFocusWhenOutOfFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveFocusWhenOutOfFocusgroup) {
  AssertBackwardDoesntMoveFocusWhenOutOfFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the root of a focusgroup element, an arrow key press
// shouldn't move the focus at all.
void FocusgroupControllerTest::
    AssertBackwardDoesntMoveFocusWhenOnFocusgroupRoot(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
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

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveFocusWhenOnFocusgroupRoot) {
  AssertBackwardDoesntMoveFocusWhenOnFocusgroupRoot(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveFocusWhenOnFocusgroupRoot) {
  AssertBackwardDoesntMoveFocusWhenOnFocusgroupRoot(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on a focusable element that isn't a focusgroup item, an
// arrow key press shouldn't move the focus at all.
void FocusgroupControllerTest::AssertBackwardDoesntMoveWhenOnNonFocusgroupItem(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div tabindex=-1 focusgroup>
      <div>
        <span id=nonitem1 tabindex=0></span>
      </div>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* nonitem1 = GetElementById("nonitem1");
  ASSERT_TRUE(nonitem1);
  nonitem1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, nonitem1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), nonitem1);
}

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveWhenOnNonFocusgroupItem) {
  AssertBackwardDoesntMoveWhenOnNonFocusgroupItem(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveWhenOnNonFocusgroupItem) {
  AssertBackwardDoesntMoveWhenOnNonFocusgroupItem(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last element of a focusgroup, a backward key
// press should move the focus to the previous item.
void FocusgroupControllerTest::AssertBackwardMovesFocusToPreviousItem(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
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

  // The focus should have moved to the previous item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpMovesFocusToPreviousItem) {
  AssertBackwardMovesFocusToPreviousItem(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftMovesFocusToPreviousItem) {
  AssertBackwardMovesFocusToPreviousItem(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last element of a focusgroup, a backward key
// press should move the focus to the previous item, skipping any non-focusable
// element.
void FocusgroupControllerTest::AssertBackwardSkipsNonFocusableItems(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2></span>
      <span id=item3 tabindex=-1></span>
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

  // The focus should have moved to the previous item, skipping the
  // non-focusable element.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsNonFocusableItems) {
  AssertBackwardSkipsNonFocusableItems(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsNonFocusableItems) {
  AssertBackwardSkipsNonFocusableItems(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the only element of a focusgroup that doesn't wrap,
// a backward key press shouldn't move the focus and we shouldn't get stuck in
// an infinite loop.
void FocusgroupControllerTest::AssertBackwardDoesntMoveWhenOnlyOneItem(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
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

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveWhenOnlyOneItem) {
  AssertBackwardDoesntMoveWhenOnlyOneItem(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveWhenOnlyOneItem) {
  AssertBackwardDoesntMoveWhenOnlyOneItem(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the only element of a focusgroup that wraps, a
// backward key press shouldn't move the focus and we shouldn't get stuck in an
// infinite loop.
void FocusgroupControllerTest::AssertBackwardDoesntMoveWhenOnlyOneItemAndWraps(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
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

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveWhenOnlyOneItemAndWraps) {
  AssertBackwardDoesntMoveWhenOnlyOneItemAndWraps(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveWhenOnlyOneItemAndWraps) {
  AssertBackwardDoesntMoveWhenOnlyOneItemAndWraps(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last element of a focusgroup that only supports
// the orthogonal axis of the arrow key pressed, the focus shouldn't move.
void FocusgroupControllerTest::AssertBackwardDoesntMoveFocusAxisNotSupported(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow is in the vertical axis, so the focusgroup should only
    // support the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=horizontal>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the focusgroup should only
    // support the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=vertical>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item2);
  item2->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus shouldn't move.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest, ArrowUpDoesntMoveFocusAxisNotSupported) {
  AssertBackwardDoesntMoveFocusAxisNotSupported(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntMoveFocusAxisNotSupported) {
  AssertBackwardDoesntMoveFocusAxisNotSupported(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last element of a focusgroup that only supports
// the axis of the arrow key pressed, the focus should move to the previous
// item.
void FocusgroupControllerTest::
    AssertBackwardMovesFocusWhenInArrowAxisOnlyFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow is in the vertical axis, so the focusgroup should only
    // support the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=vertical>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the focusgroup should only
    // support the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=horizontal>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  item2->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus should have moved to the previous item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       ArrowUpMovesFocusWhenInArrowAxisOnlyFocusgroup) {
  AssertBackwardMovesFocusWhenInArrowAxisOnlyFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest,
       ArrowLeftMovesFocusWhenInArrowAxisOnlyFocusgroup) {
  AssertBackwardMovesFocusWhenInArrowAxisOnlyFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is a descendant of a subtree, a backward arrow key press should move the
// focus to that previous item within the subtree.
void FocusgroupControllerTest::AssertBackwardDescendIntoExtendingFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup=extend>
          <span id=item2 tabindex=-1></span>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, within the extending
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, ArrowUpDescendIntoExtendingFocusgroup) {
  AssertBackwardDescendIntoExtendingFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDescendIntoExtendingFocusgroup) {
  AssertBackwardDescendIntoExtendingFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past a non-focusgroup subtree, a backward arrow key press should
// move the focus to that previous item without getting stuck in the subtree.
void FocusgroupControllerTest::AssertBackwardSkipsNonFocusgroupSubtree(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <span id=item2 tabindex=-1></span>
        <span id=item3 tabindex=-1></span>
      </div>
      <span id=item4 tabindex=-1></span>
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

  // The focus should have moved to the previous item, skipping the subtree.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsNonFocusgroupSubtree) {
  AssertBackwardSkipsNonFocusgroupSubtree(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsNonFocusgroupSubtree) {
  AssertBackwardSkipsNonFocusgroupSubtree(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is a descendant of a subtree, a backward arrow key press should move the
// focus to that previous item within the subtree. However, if that subtree is
// an extending focusgroup that supports only the orthogonal axis, it should be
// skipped.
void FocusgroupControllerTest::AssertBackwardSkipsOrthogonalFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow is in the vertical axis, so the inner focusgroup should support
    // only the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup="extending horizontal">
          <span id=item2 tabindex=-1></span>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow is in the horizontal axis, so the inner focusgroup should
    // support only the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup="extending vertical">
          <span id=item2 tabindex=-1></span>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  item4->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping in inner
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsOrthogonalFocusgroup) {
  AssertBackwardSkipsOrthogonalFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsOrthogonalFocusgroup) {
  AssertBackwardSkipsOrthogonalFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an other (non-extending) focusgroup subtree, a backward arrow
// key press should move the focus to that previous item without getting stuck
// in the other focusgroup.
void FocusgroupControllerTest::AssertBackwardSkipsRootFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup>
          <span id=item2 tabindex=-1></span>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
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

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsRootFocusgroup) {
  AssertBackwardSkipsRootFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsRootFocusgroup) {
  AssertBackwardSkipsRootFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an extending focusgroup that wraps but has no item in it, a
// backward arrow key press should move the focus to that previous item without
// getting stuck in the inner focusgroup.
void FocusgroupControllerTest::AssertBackwardSkipsEmptyWrappingFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup="extend wrap">
          <span id=item2></span> <!-- Not focusable -->
          <span id=item3></span> <!-- Not focusable -->
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
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

  // The focus should have moved to the previous item, skipping the inner
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsEmptyWrappingFocusgroup) {
  AssertBackwardSkipsEmptyWrappingFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsEmptyWrappingFocusgroup) {
  AssertBackwardSkipsEmptyWrappingFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an other (non-extending) focusgroup subtree, a backward arrow
// key press should move the focus to that previous item without getting stuck
// in the other focusgroup. The same should still be true when inside a
// focusgroup that extends a root focusgroup within the original focusgroup.
void FocusgroupControllerTest::AssertBackwardSkipsRootFocusgroupComplexCase(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup>
          <div id=item2 tabindex=-1>
            <div focusgroup=extend>
              <span id=item3 tabindex=-1></span>
              <span id=item4 tabindex=-1></span>
            </div>
          </div>
        </div>
      </div>
      <span id=item5 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item5);
  item5->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item5);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsRootFocusgroupComplexCase) {
  AssertBackwardSkipsRootFocusgroupComplexCase(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftSkipsRootFocusgroupComplexCase) {
  AssertBackwardSkipsRootFocusgroupComplexCase(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an extending focusgroup that only supports the orthogonal
// axis, a backward arrow key press should move the focus to that previous item
// without getting stuck in the inner focusgroup that doesn't support the axis.
// The same should still be true when inside a focusgroup that extends another
// extending focusgroup that supports only the orthogonal axis within the
// original focusgroup.
void FocusgroupControllerTest::
    AssertBackwardSkipsOrthogonalFocusgroupComplexCase(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow key is vertical, so the middle focusgroup should only support
    // the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup="extend horizontal">
          <div id=item2 tabindex=-1>
            <div focusgroup=extend>
              <span id=item3 tabindex=-1></span>
              <span id=item4 tabindex=-1></span>
            </div>
          </div>
        </div>
      </div>
      <span id=item5 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow key is horizontal, so the middle focusgroup should only support
    // the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <div>
        <div focusgroup="extend vertical">
          <div id=item2 tabindex=-1>
            <div focusgroup=extend>
              <span id=item3 tabindex=-1></span>
              <span id=item4 tabindex=-1></span>
            </div>
          </div>
        </div>
      </div>
      <span id=item5 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  auto* item5 = GetElementById("item5");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item5);
  item5->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item5);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpSkipsOrthogonalFocusgroupComplexCase) {
  AssertBackwardSkipsOrthogonalFocusgroupComplexCase(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest,
       ArrowLeftSkipsOrthogonalFocusgroupComplexCase) {
  AssertBackwardSkipsOrthogonalFocusgroupComplexCase(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of an extending focusgroup that
// doesn't support the axis of the arrow key pressed but the parent focusgroup
// does, ascend to that focusgroup. This should work whether the extending
// focusgroup is the child of the other focusgroup or a distant descendant.
void FocusgroupControllerTest::AssertBackwardAscendsToParentFocusgroup(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow key is vertical, so the inner focusgroup should only support
    // the horizontal axis and the outer one should only support the vertical
    // one.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=vertical>
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1>
        <div>
          <div focusgroup="extend horizontal">
            <span id=item3 tabindex=-1></span>
            <span id=item4 tabindex=-1></span>
          </div>
        </div>
      </div>
      <span id=item5 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow key is horizontal, so the inner focusgroup should only support
    // the vertical axis and the outer one should only support the horizontal
    // one.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=horizontal>
      <span id=item1 tabindex=0></span>
      <div id=item2 tabindex=-1>
        <div>
          <div focusgroup="extend vertical">
            <span id=item3 tabindex=-1></span>
            <span id=item4 tabindex=-1></span>
          </div>
        </div>
      </div>
      <span id=item5 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  item3->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item3);
  SendEvent(event);

  // The focus should ascend to the parent element.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

TEST_F(FocusgroupControllerTest, ArrowUpAscendsToParentFocusgroup) {
  AssertBackwardAscendsToParentFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftAscendsToParentFocusgroup) {
  AssertBackwardAscendsToParentFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of a focusgroup, a backward arrow key
// press shouldn't move the focus since there aren't any previous item.
void FocusgroupControllerTest::AssertBackwardDoesntWrapWhenNotSupported(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
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

TEST_F(FocusgroupControllerTest, ArrowUpDoesntWrapWhenNotSupported) {
  AssertBackwardDoesntWrapWhenNotSupported(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntWrapWhenNotSupported) {
  AssertBackwardDoesntWrapWhenNotSupported(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of a focusgroup that wraps, a
// backward arrow key press should move the focus to the last item within the
// focusgroup.
void FocusgroupControllerTest::AssertBackwardWrapsSuccessfully(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
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

  // The focus should have moved to the last element.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, ArrowUpWrapsSuccessfully) {
  AssertBackwardWrapsSuccessfully(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftWrapsSuccessfully) {
  AssertBackwardWrapsSuccessfully(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of a focusgroup that wraps and
// supports only the axis of the pressed arrow key, a backward arrow key press
// should move the focus to the last item within the focusgroup.
void FocusgroupControllerTest::AssertBackwardWrapsSuccessfullyInAxis(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow key is in the vertical axis, so the focusgroup should only
    // support the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup="vertical wrap">
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow key is in the horizontal axis, so the focusgroup should only
    // support the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup="horizontal wrap">
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
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

  // The focus should have moved to the last element.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, ArrowUpWrapsSuccessfullyInAxis) {
  AssertBackwardWrapsSuccessfullyInAxis(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftWrapsSuccessfullyInAxis) {
  AssertBackwardWrapsSuccessfullyInAxis(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of a focusgroup that wraps and
// supports only the orthogonal axis of the pressed arrow key, a backward arrow
// key press shouldn't move the focus.
void FocusgroupControllerTest::AssertBackwardDoesntWrapInOrthogonalAxis(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  if (key == ui::DomKey::ARROW_UP) {
    // The arrow key is in the vertical axis, so the focusgroup should only
    // support the horizontal axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup="horizontal wrap">
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  } else {
    // The arrow key is in the horizontal axis, so the focusgroup should only
    // support the vertical axis.
    GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup="vertical wrap">
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
      <span id=item3 tabindex=-1></span>
    </div>
  )HTML");
  }
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest, ArrowUpDoesntWrapInOrthogonalAxis) {
  AssertBackwardDoesntWrapInOrthogonalAxis(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftDoesntWrapInOrthogonalAxis) {
  AssertBackwardDoesntWrapInOrthogonalAxis(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of an extending focusgroup that
// inherited its wrapping behavior, it should only wrap if the focused item is
// also the first item of that parent focusgroup. If it is, then it should wrap
// within the parent focusgroup, not within the extending focusgroup.
void FocusgroupControllerTest::
    AssertBackwardWrapsSuccessfullyInExtendingFocusgroup(int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <div focusgroup=extend>
        <span id=item1 tabindex=0></span>
        <div focusgroup=extend>
          <span id=item2 tabindex=-1></span>
          <span id=item3 tabindex=-1></span>
        </div>
      </div>
      <span id=item4 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item4 = GetElementById("item4");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item4);
  // 1. Validate that we wrap in the right focusgroup.
  item1->focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the last element of the parent focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);

  // 2. Validate that we only wrap if we're on the first item of the parent
  // focusgroup.
  item2->focus();

  // Send the key pressed event from that element.
  event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus shouldn't have wrapped but simply move to the previous item,
  // outside of what was the current extending focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

TEST_F(FocusgroupControllerTest,
       ArrowUpWrapsSuccessfullyInExtendingFocusgroup) {
  AssertBackwardWrapsSuccessfullyInExtendingFocusgroup(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest,
       ArrowLeftWrapsSuccessfullyInExtendingFocusgroup) {
  AssertBackwardWrapsSuccessfullyInExtendingFocusgroup(ui::DomKey::ARROW_LEFT);
}

// When the focus is set on the first item of an extending focusgroup while
// there are other non-item elements before, we should still be able to wrap to
// the last item. Also, if the last item has other non-item elements after
// itself, skipping these non-item elements shouldn't be an issue.
void FocusgroupControllerTest::AssertBackwardWrapsSuccessfullyInComplexCase(
    int key) {
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <div>
        <span id=nonitem1></span>
        <span id=nonitem2></span>
      </div>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
      <span id=item3 tabindex=-1></span>
      <div>
        <span id=nonitem3></span>
        <span id=nonitem4></span>
      </div>
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

  // The focus should have moved to the last element without problem.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

TEST_F(FocusgroupControllerTest, ArrowUpWrapsSuccessfullyInComplexCase) {
  AssertBackwardWrapsSuccessfullyInComplexCase(ui::DomKey::ARROW_UP);
}

TEST_F(FocusgroupControllerTest, ArrowLeftWrapsSuccessfullyInComplexCase) {
  AssertBackwardWrapsSuccessfullyInComplexCase(ui::DomKey::ARROW_LEFT);
}
}  // namespace blink
