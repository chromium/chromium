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
#include "third_party/blink/renderer/core/page/grid_focusgroup_structure_info.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "ui/events/keycodes/dom/dom_key.h"

namespace blink {

using utils = FocusgroupControllerUtils;
using NoCellFoundAtIndexBehavior =
    GridFocusgroupStructureInfo::NoCellFoundAtIndexBehavior;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
          <table id=fg3 focusgroup=grid>
            <tr>
              <td id=item5 tabindex=-1>
                <!-- The following is an error. -->
                <div id=fg4 focusgroup=grid>
                  <span id=item6 tabindex=-1></span>
                  <div id=fg5 focusgroup>
                    <span id=item7 tabindex=-1></span>
                  </div>
                </div>
              </td>
            </tr>
          </table>
        </div>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* item1 = GetElementById("item1");
  auto* item2 = GetElementById("item2");
  auto* item3 = GetElementById("item3");
  auto* item4 = GetElementById("item4");
  auto* item5 = GetElementById("item5");
  auto* item6 = GetElementById("item6");
  auto* item7 = GetElementById("item7");
  auto* fg1 = GetElementById("fg1");
  auto* fg2 = GetElementById("fg2");
  auto* fg3 = GetElementById("fg3");
  auto* fg4 = GetElementById("fg4");
  auto* fg5 = GetElementById("fg5");
  ASSERT_TRUE(item1);
  ASSERT_TRUE(item2);
  ASSERT_TRUE(item3);
  ASSERT_TRUE(item4);
  ASSERT_TRUE(item5);
  ASSERT_TRUE(item6);
  ASSERT_TRUE(item7);
  ASSERT_TRUE(fg1);
  ASSERT_TRUE(fg2);
  ASSERT_TRUE(fg3);
  ASSERT_TRUE(fg4);
  ASSERT_TRUE(fg5);

  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item1, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item1, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item2, FocusgroupType::kLinear),
      fg1);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item2, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item3, FocusgroupType::kLinear),
      fg2);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item3, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item4, FocusgroupType::kLinear),
      fg2);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item4, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item5, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item5, FocusgroupType::kGrid),
            fg3);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item6, FocusgroupType::kLinear),
      nullptr);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item6, FocusgroupType::kGrid),
            nullptr);
  EXPECT_EQ(
      utils::FindNearestFocusgroupAncestor(item7, FocusgroupType::kLinear),
      fg5);
  EXPECT_EQ(utils::FindNearestFocusgroupAncestor(item7, FocusgroupType::kGrid),
            nullptr);
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

TEST_F(FocusgroupControllerTest, CellAtIndexInRowBehaviorOnNoCellFound) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(R"HTML(
    <table id=table focusgroup=grid>
      <tr>
        <td id=r1c1></td>
        <td id=r1c2></td>
        <td id=r1c3 rowspan=2></td>
      </tr>
      <tr id=row2>
        <td id=r2c1></td>
        <!-- r2c2 doesn't exist, but r2c3 exists because of the rowspan on the
             previous row. -->
      </tr>
      <tr>
        <td id=r3c1></td>
        <td id=r3c2></td>
        <td id=r3c3></td>
      </tr>
    </table>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* table = GetElementById("table");
  auto* row2 = GetElementById("row2");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c1 = GetElementById("r2c1");
  auto* r3c2 = GetElementById("r3c2");
  ASSERT_TRUE(table);
  ASSERT_TRUE(row2);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r3c2);

  ASSERT_TRUE(table->GetFocusgroupFlags() & FocusgroupFlags::kGrid);
  auto* helper = utils::CreateGridFocusgroupStructureInfoForGridRoot(table);

  // The first column starts at index 0.
  unsigned no_cell_index = 1;

  EXPECT_EQ(helper->CellAtIndexInRow(no_cell_index, row2,
                                     NoCellFoundAtIndexBehavior::kReturn),
            nullptr);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindPreviousCellInRow),
            r2c1);
  EXPECT_EQ(
      helper->CellAtIndexInRow(no_cell_index, row2,
                               NoCellFoundAtIndexBehavior::kFindNextCellInRow),
      r1c3);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindPreviousCellInColumn),
            r1c2);
  EXPECT_EQ(helper->CellAtIndexInRow(
                no_cell_index, row2,
                NoCellFoundAtIndexBehavior::kFindNextCellInColumn),
            r3c2);
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
  item1->Focus();

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
  item2->Focus();

  // 2. Create the "ArrowDown" event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, item2);

  // 3. Move the focus to a different element before we send the event.
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // 4. Pass the event we created earlier to our FocusgroupController. The
  // controller shouldn't even try to move the focus since the focus isn't on
  // the element that triggered the arrow key press event.
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// *****************************************************************************
// FORWARD NAVIGATION - DOWN ARROW & RIGHT ARROW
// *****************************************************************************

class FocusgroupControllerForwardNavigationTest
    : public FocusgroupControllerTest,
      public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(,
                         FocusgroupControllerForwardNavigationTest,
                         testing::Values(ui::DomKey::ARROW_DOWN,
                                         ui::DomKey::ARROW_RIGHT));

// When the focus is set on an element outside of the focusgroup, an arrow key
// press shouldn't move the focus at all.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntMoveFocusWhenOutOfFocusgroup) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <span id=out tabindex=-1></span>
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* out = GetElementById("out");
  ASSERT_TRUE(out);
  out->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, out);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), out);
}

// When the focus is set on the root of a focusgroup element, an arrow key press
// shouldn't move the focus at all.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntMoveFocusWhenOnFocusgroupRoot) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root tabindex=-1 focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* root = GetElementById("root");
  ASSERT_TRUE(root);
  root->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, root);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), root);
}

// When the focus is set on a focusable element that isn't a focusgroup item, an
// arrow key press shouldn't move the focus at all.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntMoveWhenOnNonFocusgroupItem) {
  int key = GetParam();
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
  nonitem1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, nonitem1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), nonitem1);
}

// When the focus is set on a focusgroup item, an arrow key press should move
// the focus to the next item.
TEST_P(FocusgroupControllerForwardNavigationTest, MovesToNextItem) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the next sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on the only focusgroup item, the focus shouldn't move
// and we shouldn't get stuck in an infinite loop.
TEST_P(FocusgroupControllerForwardNavigationTest, DoesntMoveWhenOnlyOneItem) {
  int key = GetParam();
  ASSERT_TRUE(key == ui::DomKey::ARROW_DOWN || key == ui::DomKey::ARROW_RIGHT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the only focusgroup item and the focusgroup wraps in
// the axis of the arrow key pressed, the focus shouldn't move and we shouldn't
// get stuck in an infinite loop.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntMoveWhenOnlyOneItemAndWraps) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup=wrap>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on a focusgroup item, an arrow key press should move
// the focus to the next item and skip non-focusable items.
TEST_P(FocusgroupControllerForwardNavigationTest, SkipsNonFocusableItems) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the next focusable sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on a focusgroup item which happens to also be an
// extending focusgroup, an arrow key press should move the focus to the next
// item within the extending focusgroup and skip non-focusable items.
TEST_P(FocusgroupControllerForwardNavigationTest, MovesInExtendingFocusgroup) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the first *focusable* item withing the
  // extending focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on a focusgroup item which happens to also be an
// extending focusgroup, an arrow key press should move the focus to the next
// item within the extending focusgroup and skip non-focusable items. If no
// valid candidate is found within that extending focusgroup, the next element
// (in pre-order traversal) should be considered. In this case, |item4| is the
// valid next candidate.
TEST_P(FocusgroupControllerForwardNavigationTest, ExitsExtendingFocusgroup) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to |item1|'s next sibling, |item4|.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

// When the focus is set on a focusgroup item that is an ancestor to an
// extending focusgroup, the focus should move to the next element inside that
// extending focusgroup even if it's not a direct child.
TEST_P(FocusgroupControllerForwardNavigationTest,
       MovesToNextElementWithinDescendants) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on an item of a focusgroup that only supports the
// orthogonal axis to the arrow key pressed, the arrow pressed shouldn't move
// the focus.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntMoveFocusWhenAxisNotSupported) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on an item of a focusgroup that only supports the
// axis of the arrow key pressed the focus should move.
TEST_P(FocusgroupControllerForwardNavigationTest,
       MovesFocusWhenInArrowAxisOnlyFocusgroup) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus should have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on an extending focusgroup element but that focusgroup
// doesn't support the axis of the arrow key pressed, skip that subtree
// altogether.
TEST_P(FocusgroupControllerForwardNavigationTest, SkipsExtendingFocusgroup) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // Focus shouldn't go into |item1|'s subtree, but should go to its next
  // sibling.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the last item of a focusgroup that doesn't support
// wrapping in the axis of the arrow key pressed, the focus shouldn't move.
TEST_P(FocusgroupControllerForwardNavigationTest, DoesntWrapWhenNotSupported) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* item2 = GetElementById("item2");
  ASSERT_TRUE(item2);
  item2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on the last item of a focusgroup that doesn't support
// wrapping in the axis of the arrow key pressed but supports wrapping in the
// orthogonal axis, the focus shouldn't move.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntWrapEvenWhenOtherAxisSupported) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

// This test validates that we don't get stuck in an infinite loop searching for
// a focusable element in the extending focusgroup that wraps that doesn't
// contain one. Wrapping should only be allowed in the focusgroup that contains
// the focusable element we started on or in one of its ancestors.
TEST_P(FocusgroupControllerForwardNavigationTest,
       DoesntWrapInFocusgroupWithoutItems) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to |item1|'s next sibling, |item4|, without
  // getting stuck looping infinitely in the wrapping focusgroup deprived of
  // focusable elements.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);
}

// When the focus is set on the last item of a focusgroup that supports wrapping
// in the axis of the arrow key pressed, the focus should move back to the first
// item.
TEST_P(FocusgroupControllerForwardNavigationTest, WrapsSuccessfully) {
  int key = GetParam();
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
  item2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // Focus should have moved back to the first item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup also does, the focus should move to the
// first item of the parent focusgroup.
TEST_P(FocusgroupControllerForwardNavigationTest, WrapsToParentFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the parent focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup doesn't (in the axis of the arrow key
// pressed), the focus should move to the first item of the inner focusgroup.
TEST_P(FocusgroupControllerForwardNavigationTest, WrapsInInnerFocusgroupOnly) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the last item of an inner focusgroup that supports
// wrapping while its parent focusgroup doesn't (in the axis of the arrow key
// pressed), the focus should move to the first item of the inner focusgroup
// even if there's another focusgroup supporting wrapping in the same axis as
// the arrow key pressed in the hierarchy.
TEST_P(FocusgroupControllerForwardNavigationTest, WrapsInExpectedScope) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the last item of a focusgroup that supports
// wrapping in the axis of the arrow key pressed and the first item is in an
// inner focusgroup that supports it too, the focus moves to that item in the
// inner focusgroup.
TEST_P(FocusgroupControllerForwardNavigationTest,
       WrapsAndGoesInInnerFocusgroup) {
  int key = GetParam();
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
  item3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item3);
  SendEvent(event);

  // Focus should have moved to the first element of the inner focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup that supports
// wrapping in the axis of the arrow key pressed and the first item is in an
// inner focusgroup that doesn't support wrapping in the same axis, the focus
// moves to the next item out of that inner focusgroup.
TEST_P(FocusgroupControllerForwardNavigationTest,
       WrapsAndSkipsOrthogonalInnerFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // Focus should have moved to the first element after the inner focusgroup
  // that doesn't support wrapping in the arrow axis.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// *****************************************************************************
// BACKWARD NAVIGATION - UP ARROW & LEFT ARROW
// *****************************************************************************

class FocusgroupControllerBackwardNavigationTest
    : public FocusgroupControllerTest,
      public ::testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(,
                         FocusgroupControllerBackwardNavigationTest,
                         testing::Values(ui::DomKey::ARROW_UP,
                                         ui::DomKey::ARROW_LEFT));

// When the focus is set on an element outside of the focusgroup, an arrow key
// press shouldn't move the focus at all.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DoesntMoveFocusWhenOutOfFocusgroup) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <span id=out tabindex=-1></span>
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* out = GetElementById("out");
  ASSERT_TRUE(out);
  out->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, out);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), out);
}

// When the focus is set on the root of a focusgroup element, an arrow key press
// shouldn't move the focus at all.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DoesntMoveFocusWhenOnFocusgroupRoot) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div id=root tabindex=-1 focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* root = GetElementById("root");
  ASSERT_TRUE(root);
  root->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, root);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), root);
}

// When the focus is set on a focusable element that isn't a focusgroup item, an
// arrow key press shouldn't move the focus at all.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DoesntMoveWhenOnNonFocusgroupItem) {
  int key = GetParam();
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
  nonitem1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, nonitem1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), nonitem1);
}

// When the focus is set on the last element of a focusgroup, a backward key
// press should move the focus to the previous item.
TEST_P(FocusgroupControllerBackwardNavigationTest, MovesFocusToPreviousItem) {
  int key = GetParam();
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
  item2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus should have moved to the previous item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last element of a focusgroup, a backward key
// press should move the focus to the previous item, skipping any non-focusable
// element.
TEST_P(FocusgroupControllerBackwardNavigationTest, SkipsNonFocusableItems) {
  int key = GetParam();
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
  item3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item3);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the
  // non-focusable element.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the only element of a focusgroup that doesn't wrap,
// a backward key press shouldn't move the focus and we shouldn't get stuck in
// an infinite loop.
TEST_P(FocusgroupControllerBackwardNavigationTest, DoesntMoveWhenOnlyOneItem) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the only element of a focusgroup that wraps, a
// backward key press shouldn't move the focus and we shouldn't get stuck in an
// infinite loop.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DoesntMoveWhenOnlyOneItemAndWraps) {
  int key = GetParam();
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=wrap>
      <span id=item1 tabindex=0></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last element of a focusgroup that only supports
// the orthogonal axis of the arrow key pressed, the focus shouldn't move.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DoesntMoveFocusAxisNotSupported) {
  int key = GetParam();
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
  item2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus shouldn't move.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on the last element of a focusgroup that only supports
// the axis of the arrow key pressed, the focus should move to the previous
// item.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       MovesFocusWhenInArrowAxisOnlyFocusgroup) {
  int key = GetParam();
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
  item2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus should have moved to the previous item.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is a descendant of a subtree, a backward arrow key press should move the
// focus to that previous item within the subtree.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       DescendIntoExtendingFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, within the extending
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past a non-focusgroup subtree, a backward arrow key press should
// move the focus to that previous item without getting stuck in the subtree.
TEST_P(FocusgroupControllerBackwardNavigationTest, SkipsNonFocusgroupSubtree) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the subtree.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is a descendant of a subtree, a backward arrow key press should move the
// focus to that previous item within the subtree. However, if that subtree is
// an extending focusgroup that supports only the orthogonal axis, it should be
// skipped.
TEST_P(FocusgroupControllerBackwardNavigationTest, SkipsOrthogonalFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping in inner
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an other (non-extending) focusgroup subtree, a backward arrow
// key press should move the focus to that previous item without getting stuck
// in the other focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest, SkipsRootFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an extending focusgroup that wraps but has no item in it, a
// backward arrow key press should move the focus to that previous item without
// getting stuck in the inner focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       SkipsEmptyWrappingFocusgroup) {
  int key = GetParam();
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
  item4->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item4);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the inner
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an other (non-extending) focusgroup subtree, a backward arrow
// key press should move the focus to that previous item without getting stuck
// in the other focusgroup. The same should still be true when inside a
// focusgroup that extends a root focusgroup within the original focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       SkipsRootFocusgroupComplexCase) {
  int key = GetParam();
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
  item5->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item5);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the last item of a focusgroup and the previous item
// is located past an extending focusgroup that only supports the orthogonal
// axis, a backward arrow key press should move the focus to that previous item
// without getting stuck in the inner focusgroup that doesn't support the axis.
// The same should still be true when inside a focusgroup that extends another
// extending focusgroup that supports only the orthogonal axis within the
// original focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       SkipsOrthogonalFocusgroupComplexCase) {
  int key = GetParam();
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
  item5->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item5);
  SendEvent(event);

  // The focus should have moved to the previous item, skipping the other
  // focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the first item of an extending focusgroup that
// doesn't support the axis of the arrow key pressed but the parent focusgroup
// does, ascend to that focusgroup. This should work whether the extending
// focusgroup is the child of the other focusgroup or a distant descendant.
TEST_P(FocusgroupControllerBackwardNavigationTest, AscendsToParentFocusgroup) {
  int key = GetParam();
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
  item3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item3);
  SendEvent(event);

  // The focus should ascend to the parent element.
  ASSERT_EQ(GetDocument().FocusedElement(), item2);
}

// When the focus is set on the first item of a focusgroup, a backward arrow key
// press shouldn't move the focus since there aren't any previous item.
TEST_P(FocusgroupControllerBackwardNavigationTest, DoesntWrapWhenNotSupported) {
  int key = GetParam();
  ASSERT_TRUE(key == ui::DomKey::ARROW_UP || key == ui::DomKey::ARROW_LEFT);
  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup>
      <span id=item1 tabindex=0></span>
      <span id=item2 tabindex=-1></span>
    </div>
  )HTML");
  auto* item1 = GetElementById("item1");
  ASSERT_TRUE(item1);
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the first item of a focusgroup that wraps, a
// backward arrow key press should move the focus to the last item within the
// focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest, WrapsSuccessfully) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the last element.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the first item of a focusgroup that wraps and
// supports only the axis of the pressed arrow key, a backward arrow key press
// should move the focus to the last item within the focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest, WrapsSuccessfullyInAxis) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the last element.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// When the focus is set on the first item of a focusgroup that wraps and
// supports only the orthogonal axis of the pressed arrow key, a backward arrow
// key press shouldn't move the focus.
TEST_P(FocusgroupControllerBackwardNavigationTest, DoesntWrapInOrthogonalAxis) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the first item of an extending focusgroup that
// inherited its wrapping behavior, it should only wrap if the focused item is
// also the first item of that parent focusgroup. If it is, then it should wrap
// within the parent focusgroup, not within the extending focusgroup.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       WrapsSuccessfullyInExtendingFocusgroup) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the last element of the parent focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item4);

  // 2. Validate that we only wrap if we're on the first item of the parent
  // focusgroup.
  item2->Focus();

  // Send the key pressed event from that element.
  event = KeyDownEvent(key, item2);
  SendEvent(event);

  // The focus shouldn't have wrapped but simply move to the previous item,
  // outside of what was the current extending focusgroup.
  ASSERT_EQ(GetDocument().FocusedElement(), item1);
}

// When the focus is set on the first item of an extending focusgroup while
// there are other non-item elements before, we should still be able to wrap to
// the last item. Also, if the last item has other non-item elements after
// itself, skipping these non-item elements shouldn't be an issue.
TEST_P(FocusgroupControllerBackwardNavigationTest,
       WrapsSuccessfullyInComplexCase) {
  int key = GetParam();
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
  item1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(key, item1);
  SendEvent(event);

  // The focus should have moved to the last element without problem.
  ASSERT_EQ(GetDocument().FocusedElement(), item3);
}

// *****************************************************************************
// GRID NAVIGATION - ALL ARROWS
// *****************************************************************************

auto* kSimpleGrid = R"HTML(
    <table focusgroup=grid>
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
  )HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightGoesToNextCol) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c1);
  SendEvent(event);

  // The focus should have moved to the second column, same row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);
}

TEST_F(FocusgroupControllerTest, GridArrowDownGoesToNextRow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c1);
  SendEvent(event);

  // The focus should have moved to the second row, same column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftGoesToPreviousCol) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);

  r1c2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c2);
  SendEvent(event);

  // The focus should have moved to the first column, same row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpGoesToNextRow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c1);
  SendEvent(event);

  // The focus should have moved to the first row, same column.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

auto* kSimpleGridWithR2C2NonFocusable = R"HTML(
    <table focusgroup=grid>
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r3c1 tabindex=-1></td>
        <td id=r3c2 tabindex=-1></td>
        <td id=r3c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightSkipsNonFocusableItem) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGridWithR2C2NonFocusable);
  auto* r2c1 = GetElementById("r2c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r2c3);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r2c1);
  SendEvent(event);

  // The focus should have moved to the third column, same row.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

TEST_F(FocusgroupControllerTest, GridArrowDownSkipsNonFocusableItem) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGridWithR2C2NonFocusable);
  auto* r1c2 = GetElementById("r1c2");
  auto* r3c2 = GetElementById("r3c2");
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r3c2);

  r1c2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c2);
  SendEvent(event);

  // The focus should have moved to the third row, same column.
  ASSERT_EQ(GetDocument().FocusedElement(), r3c2);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftSkipsNonFocusableItem) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGridWithR2C2NonFocusable);
  auto* r2c3 = GetElementById("r2c3");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r2c1);

  r2c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r2c3);
  SendEvent(event);

  // The focus should have moved to the first column, same row.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpSkipsNonFocusableItem) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGridWithR2C2NonFocusable);
  auto* r3c2 = GetElementById("r3c2");
  auto* r1c2 = GetElementById("r1c2");
  ASSERT_TRUE(r3c2);
  ASSERT_TRUE(r1c2);

  r3c2->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r3c2);
  SendEvent(event);

  // The focus should have moved to the first row, same column.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);
}

TEST_F(FocusgroupControllerTest, GridArrowRightNoWrapNoFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c3);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowDownNoWrapNoFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus shouldn't have moved
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftNoWrapNoFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpNoWrapNoFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kSimpleGrid);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

auto* kGridWithSections = R"HTML(
    <table focusgroup="grid wrap">
      <thead>
        <tr>
          <td id=r1c1 tabindex=0></td>
        </tr>
        <tr>
          <td id=r2c1 tabindex=-1></td>
        </tr>
      </thead>
      <tbody>
        <tr>
          <td id=r3c1 tabindex=-1></td>
        </tr>
        <tr>
          <td id=r4c1 tabindex=-1></td>
        </tr>
      </tbody>
      <tbody></tbody>
      <tfoot>
        <tr>
          <td id=r5c1 tabindex=-1></td>
        </tr>
        <tr>
          <td id=r6c1 tabindex=-1></td>
        </tr>
      </foot>
    </table>
  )HTML";

TEST_F(FocusgroupControllerTest, GridArrowDownGoesToNextRowInAcrossSections) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithSections);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  auto* r3c1 = GetElementById("r3c1");
  auto* r4c1 = GetElementById("r4c1");
  auto* r5c1 = GetElementById("r5c1");
  auto* r6c1 = GetElementById("r6c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r3c1);
  ASSERT_TRUE(r4c1);
  ASSERT_TRUE(r5c1);
  ASSERT_TRUE(r6c1);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r3c1);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r3c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r4c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c1);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r5c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r6c1);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r6c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpGoesToPreviousRowAcrossSections) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithSections);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  auto* r3c1 = GetElementById("r3c1");
  auto* r4c1 = GetElementById("r4c1");
  auto* r5c1 = GetElementById("r5c1");
  auto* r6c1 = GetElementById("r6c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r3c1);
  ASSERT_TRUE(r4c1);
  ASSERT_TRUE(r5c1);
  ASSERT_TRUE(r6c1);

  r6c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r6c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r5c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r4c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r3c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r3c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r6c1);
}

auto* kGridWithRowWrapOnly = R"HTML(
    <table focusgroup="grid row-wrap">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithRowWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapOnly);
  auto* r1c3 = GetElementById("r1c3");
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r1c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithRowWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapOnly);
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithRowWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapOnly);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithRowWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapOnly);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

auto* kGridWithColWrapOnly = R"HTML(
    <table focusgroup="grid col-wrap">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithColWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColWrapOnly);
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c3);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithColWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColWrapOnly);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithColWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColWrapOnly);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithColWrapOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColWrapOnly);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

auto* kGridWithWrapInBothAxes = R"HTML(
    <table focusgroup="grid wrap">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithWrapInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithWrapInBothAxes);
  auto* r1c3 = GetElementById("r1c3");
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r1c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithWrapInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithWrapInBothAxes);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithWrapInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithWrapInBothAxes);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithWrapInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithWrapInBothAxes);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

auto* kGridWithRowFlowOnly = R"HTML(
    <table focusgroup="grid row-flow">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithRowFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowOnly);
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next row, first
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithRowFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowOnly);
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithRowFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowOnly);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have flowed to the previous cell on the previous row, last
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithRowFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowOnly);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

auto* kGridWithColFlowOnly = R"HTML(
    <table focusgroup="grid col-flow">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithColFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColFlowOnly);
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c3);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithColFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColFlowOnly);
  auto* r1c2 = GetElementById("r1c2");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next column, first
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithColFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColFlowOnly);
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus shouldn't have moved.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithColFlowOnly) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColFlowOnly);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the previous column, last
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

auto* kGridWithFlowInBothAxes = R"HTML(
    <table focusgroup="grid flow">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithFlowInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithFlowInBothAxes);
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next row, first
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithFlowInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithFlowInBothAxes);
  auto* r1c2 = GetElementById("r1c2");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next column, first
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithFlowInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithFlowInBothAxes);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have flowed to the previous cell on the previous row, last
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithFlowInBothAxes) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithFlowInBothAxes);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the previous column, last
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

auto* kGridWithRowWrapAndColFlow = R"HTML(
    <table focusgroup="grid row-wrap col-flow">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithRowWrapColFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapAndColFlow);
  auto* r1c3 = GetElementById("r1c3");
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r1c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithRowWrapColFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapAndColFlow);
  auto* r1c2 = GetElementById("r1c2");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next column, first
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithRowWrapColFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapAndColFlow);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have wrapped in row.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithRowWrapColFlow) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowWrapAndColFlow);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have flowed to the next cell on the previous column, last
  // row.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

auto* kGridWithRowFlowAndColWrap = R"HTML(
    <table focusgroup="grid row-flow col-wrap">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithRowFlowColWrap) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowAndColWrap);
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c1);

  r1c3->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c3);
  SendEvent(event);

  // The focus should have flowed to the next cell on the next row, first
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithRowFlowColWrap) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowAndColWrap);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r2c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithRowFlowColWrap) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowAndColWrap);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  // The focus should have flowed to the previous cell on the previous row, last
  // column.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithRowFlowColWrap) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowFlowAndColWrap);
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c1);

  r1c1->Focus();

  // Send the key pressed event from that element.
  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  // The focus should have wrapped in col.
  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);
}

auto* kGridWithColSpan = R"HTML(
    <table focusgroup="grid">
      <tr>
        <td id=r1c1 tabindex=0></td>
        <td id=r1c2 tabindex=-1 colspan=2></td>
        <td id=r1c4 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=-1></td>
        <td id=r2c2 tabindex=-1></td>
        <td id=r2c3 tabindex=-1></td>
        <td id=r2c4 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithColSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColSpan);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c4 = GetElementById("r1c4");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c4);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  r1c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithColSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColSpan);

  // Focus will start on one of these:
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c4 = GetElementById("r1c4");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c4);

  // Focus is expected to end one of these:
  auto* r2c1 = GetElementById("r2c1");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c4 = GetElementById("r2c4");
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c4);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);

  r1c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r1c4->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c4);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithColSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColSpan);
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c4 = GetElementById("r1c4");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c4);

  r1c4->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  r1c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithColSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithColSpan);

  // Focus will start on one of these:
  auto* r2c1 = GetElementById("r2c1");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c3 = GetElementById("r2c3");
  auto* r2c4 = GetElementById("r2c4");
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r2c4);

  // Focus is expected to end one of these:
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c4 = GetElementById("r1c4");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c4);

  r2c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);

  r2c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  r2c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  r2c4->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);
}

auto* kGridWithRowSpan = R"HTML(
    <table focusgroup="grid">
          <tr>
        <td id=r1c1 tabindex=-1></td>
        <td id=r1c2 tabindex=-1></td>
        <td id=r1c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r2c1 tabindex=0></td>
        <td id=r2c2 tabindex=-1 rowspan=3></td>
        <td id=r2c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r3c1 tabindex=-1></td>
        <td id=r3c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r4c1 tabindex=-1></td>
        <td id=r4c3 tabindex=-1></td>
      </tr>
      <tr>
        <td id=r5c1 tabindex=-1></td>
        <td id=r5c2 tabindex=-1></td>
        <td id=r5c3 tabindex=-1></td>
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithRowSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowSpan);
  auto* r2c1 = GetElementById("r2c1");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c3 = GetElementById("r2c3");
  auto* r3c1 = GetElementById("r3c1");
  auto* r4c1 = GetElementById("r4c1");
  auto* r5c1 = GetElementById("r5c1");
  auto* r5c2 = GetElementById("r5c2");
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r3c1);
  ASSERT_TRUE(r4c1);
  ASSERT_TRUE(r5c1);
  ASSERT_TRUE(r5c2);

  r2c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r2c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r2c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);

  r3c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r3c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r4c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r4c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r5c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r5c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c2);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithRowSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowSpan);
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c3 = GetElementById("r1c3");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c3 = GetElementById("r2c3");
  auto* r3c3 = GetElementById("r3c3");
  auto* r4c3 = GetElementById("r4c3");
  auto* r5c2 = GetElementById("r5c2");
  auto* r5c3 = GetElementById("r5c3");
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r3c3);
  ASSERT_TRUE(r4c3);
  ASSERT_TRUE(r5c2);
  ASSERT_TRUE(r5c3);

  r1c2->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r1c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);

  r2c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c2);

  r2c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r3c3);

  r3c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r3c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c3);

  r4c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r4c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c3);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithRowSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowSpan);
  auto* r2c3 = GetElementById("r2c3");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c1 = GetElementById("r2c1");
  auto* r3c3 = GetElementById("r3c3");
  auto* r4c3 = GetElementById("r4c3");
  auto* r5c3 = GetElementById("r5c3");
  auto* r5c2 = GetElementById("r5c2");
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c1);
  ASSERT_TRUE(r3c3);
  ASSERT_TRUE(r4c3);
  ASSERT_TRUE(r5c3);
  ASSERT_TRUE(r5c2);

  r2c3->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r2c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);

  r3c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r3c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r4c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r4c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r5c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r5c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r5c2);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithRowSpan) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithRowSpan);
  auto* r5c2 = GetElementById("r5c2");
  auto* r5c3 = GetElementById("r5c3");
  auto* r2c2 = GetElementById("r2c2");
  auto* r4c3 = GetElementById("r4c3");
  auto* r3c3 = GetElementById("r3c3");
  auto* r2c3 = GetElementById("r2c3");
  auto* r1c2 = GetElementById("r1c2");
  auto* r1c3 = GetElementById("r1c3");
  ASSERT_TRUE(r5c2);
  ASSERT_TRUE(r5c3);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r4c3);
  ASSERT_TRUE(r3c3);
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r1c3);

  r5c2->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r5c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  r5c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r5c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c3);

  r2c2->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  r4c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r4c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r3c3);

  r3c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r3c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c3);

  r2c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);
}

// The 4 following tests are very corner-case.
//
// We are creating empty spaces that don't have cells through a weird table
// structure. The spaces at the following locations don't have cells (assuming
// that the first row/column starts at index 1): r2c5, r2c6, r2c7, r3c3,
// r3c5, r3c6, r3c7 and r4c7.
auto* kGridWithEmptyCells = R"HTML(
    <table focusgroup="grid flow">
      <tr>
        <td id=r1c1 tabindex=-1 rowspan=2 colspan=2></td>
        <td id=r1c3 tabindex=-1></td>
        <td id=r1c4 tabindex=-1 rowspan=3></td>
        <td id=r1c5 tabindex=-1></td>
        <td id=r1c6 tabindex=-1></td>
        <td id=r1c7 tabindex=-1></td>
      </tr>
      <tr>
        <!-- r2c1 and r2c2 starts in the previous row and spans to here. -->
        <td id=r2c3 tabindex=-1></td>
        <!-- Leave the rest of the row empty -->
      </tr>
      <tr>
        <td id=r3c1 tabindex=-1></td>
        <td id=r3c2 tabindex=-1></td>
        <!-- There will be a cell at r3c4, but it starts in row 1. -->
      </tr>
      <tr>
        <td id=r4c1 tabindex=-1 colspan=5></td>
        <td id=r4c6 tabindex=-1></td>
        <!-- No last cell - leave it empty for the test -->
      </tr>
    </table>
)HTML";

TEST_F(FocusgroupControllerTest, GridArrowRightWithEmptyCells) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithEmptyCells);
  auto* r3c2 = GetElementById("r3c2");
  auto* r1c4 = GetElementById("r1c4");
  auto* r1c5 = GetElementById("r1c5");
  auto* r4c6 = GetElementById("r4c6");
  auto* r1c1 = GetElementById("r1c1");
  auto* r2c3 = GetElementById("r2c3");
  ASSERT_TRUE(r3c2);
  ASSERT_TRUE(r1c4);
  ASSERT_TRUE(r1c5);
  ASSERT_TRUE(r4c6);
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r2c3);

  r3c2->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r3c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);

  r1c4->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c5);

  r4c6->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r4c6);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);

  r2c3->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);
}

TEST_F(FocusgroupControllerTest, GridArrowDownWithEmptyCells) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithEmptyCells);
  auto* r2c3 = GetElementById("r2c3");
  auto* r4c1 = GetElementById("r4c1");
  auto* r1c5 = GetElementById("r1c5");
  auto* r1c6 = GetElementById("r1c6");
  auto* r4c6 = GetElementById("r4c6");
  auto* r1c7 = GetElementById("r1c7");
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c4 = GetElementById("r1c4");
  ASSERT_TRUE(r2c3);
  ASSERT_TRUE(r4c1);
  ASSERT_TRUE(r1c5);
  ASSERT_TRUE(r1c6);
  ASSERT_TRUE(r4c6);
  ASSERT_TRUE(r1c7);
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c4);

  r2c3->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r2c3);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);

  r1c5->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c5);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);

  r1c6->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c6);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c6);

  r1c7->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c7);
  SendEvent(event);

  // Goes to r1c1 because it flows to the first cell of the first column when
  // on the last cell of the last column.
  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);

  r4c6->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r4c6);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c7);

  r1c4->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);
}

TEST_F(FocusgroupControllerTest, GridArrowLeftWithEmptyCells) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithEmptyCells);
  auto* r1c1 = GetElementById("r1c1");
  auto* r4c6 = GetElementById("r4c6");
  auto* r3c1 = GetElementById("r3c1");
  auto* r1c4 = GetElementById("r1c4");
  auto* r1c3 = GetElementById("r1c3");
  auto* r4c1 = GetElementById("r4c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r4c6);
  ASSERT_TRUE(r3c1);
  ASSERT_TRUE(r1c4);
  ASSERT_TRUE(r1c3);
  ASSERT_TRUE(r4c1);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c6);

  r3c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r3c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);

  r1c4->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r1c4);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c3);

  r4c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r4c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c4);
}

TEST_F(FocusgroupControllerTest, GridArrowUpWithEmptyCells) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(kGridWithEmptyCells);
  auto* r1c7 = GetElementById("r1c7");
  auto* r4c6 = GetElementById("r4c6");
  auto* r1c6 = GetElementById("r1c6");
  auto* r4c1 = GetElementById("r4c1");
  auto* r3c1 = GetElementById("r3c1");
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c7);
  ASSERT_TRUE(r4c6);
  ASSERT_TRUE(r1c6);
  ASSERT_TRUE(r4c1);
  ASSERT_TRUE(r3c1);
  ASSERT_TRUE(r1c1);

  r1c7->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c7);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c6);

  r4c6->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r4c6);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c6);

  r1c6->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c6);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r4c1);

  r4c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r4c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r3c1);

  r1c1->Focus();

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c7);
}

// Tests that grid focusgroups also work on CSS tables (i.e.: 'display: table').
// The implementation relies on the layout objects, so the test suite above
// that covers HTML tables doesn't need to be duplicated to test the same cases
// with CSS tables.
TEST_F(FocusgroupControllerTest, GridFocusgroupWithCssTable) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="display:table" focusgroup=grid>
      <div style="display:table-row">
        <div id=r1c1 style="display:table-cell" tabindex=0></div>
        <div id=r1c2 style="display:table-cell" tabindex=-1></div>
      </div>
      <div style="display:table-row">
        <div id=r2c1 style="display:table-cell" tabindex=-1></div>
        <div id=r2c2 style="display:table-cell" tabindex=-1></div>
      </div>
    </div>
  )HTML");
  auto* r1c1 = GetElementById("r1c1");
  auto* r1c2 = GetElementById("r1c2");
  auto* r2c2 = GetElementById("r2c2");
  auto* r2c1 = GetElementById("r2c1");
  ASSERT_TRUE(r1c1);
  ASSERT_TRUE(r1c2);
  ASSERT_TRUE(r2c2);
  ASSERT_TRUE(r2c1);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c2);

  event = KeyDownEvent(ui::DomKey::ARROW_DOWN, r1c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c2);

  event = KeyDownEvent(ui::DomKey::ARROW_LEFT, r2c2);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r2c1);

  event = KeyDownEvent(ui::DomKey::ARROW_UP, r2c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

// Validates that a CSS table that doesn't have the focusgroup=grid attribute
// set doesn't allow arrow-keys navigation.
TEST_F(FocusgroupControllerTest, CssTableWithoutGridFocusgroupNotAFocusgroup) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(R"HTML(
    <div style="display:table">
      <div style="display:table-row">
        <div id=r1c1 style="display:table-cell" tabindex=0></div>
        <div id=r1c2 style="display:table-cell" tabindex=-1></div>
      </div>
      <div style="display:table-row">
        <div id=r2c1 style="display:table-cell" tabindex=-1></div>
        <div id=r2c2 style="display:table-cell" tabindex=-1></div>
      </div>
    </div>
  )HTML");
  auto* r1c1 = GetElementById("r1c1");
  ASSERT_TRUE(r1c1);

  r1c1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_RIGHT, r1c1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), r1c1);
}

// Validates that focusgroup=grid set on a non table layout doesn't become a
// grid focusgroup nor a linear one.
TEST_F(FocusgroupControllerTest, GridFocusgroupOnNonTableElementIgnored) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  GetDocument().body()->setInnerHTML(R"HTML(
    <div focusgroup=grid>
      <span id=nonitem1 tabindex=0></span>
      <span id=nonitem2 tabindex=-1></span>
    </div>
  )HTML");
  auto* non_item_1 = GetElementById("nonitem1");
  ASSERT_TRUE(non_item_1);

  non_item_1->Focus();

  auto* event = KeyDownEvent(ui::DomKey::ARROW_DOWN, non_item_1);
  SendEvent(event);

  ASSERT_EQ(GetDocument().FocusedElement(), non_item_1);
}

}  // namespace blink
