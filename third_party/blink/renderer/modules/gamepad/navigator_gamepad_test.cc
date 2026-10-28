// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

#include <memory>

#include "device/gamepad/public/cpp/gamepads.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

Gamepad* CreateGamepadWithButtonDown() {
  base::TimeTicks dummy_time_origin =
      base::TimeTicks() + base::Microseconds(1000);
  base::TimeTicks dummy_time_floor =
      base::TimeTicks() + base::Microseconds(2000);
  auto* gamepad = MakeGarbageCollected<Gamepad>(nullptr, 0, dummy_time_origin,
                                                dummy_time_floor);
  double axes[1] = {0.0};
  device::GamepadButton buttons[1] = {{true, true, 1.0}};
  gamepad->SetId("gamepad");
  gamepad->SetAxes(axes);
  gamepad->SetButtons(buttons);
  gamepad->SetConnected(true);
  return gamepad;
}

HeapVector<Member<Gamepad>> CreateGamepadListWithButtonDown() {
  HeapVector<Member<Gamepad>> list(device::Gamepads::kItemsLengthCap);
  list[0] = CreateGamepadWithButtonDown();
  return list;
}

}  // namespace

class NavigatorGamepadTest : public testing::Test {
 public:
  NavigatorGamepadTest() = default;

 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(NavigatorGamepadTest, UnfocusedPageDoesNotGainUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  EXPECT_TRUE(page.IsPageVisible());
  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  page.GetFocusController().SetFocused(false);
  EXPECT_FALSE(page.GetFocusController().IsFocused());
  EXPECT_FALSE(frame.GetDocument()->hasFocus());

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);
  navigator_gamepad.SetGamepadsForTesting(CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());

  navigator_gamepad.Gamepads();

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());
}

TEST_F(NavigatorGamepadTest, FocusedPageGainsUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  EXPECT_TRUE(page.IsPageVisible());
  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  EXPECT_TRUE(page.GetFocusController().IsFocused());
  EXPECT_TRUE(frame.GetDocument()->hasFocus());

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);
  navigator_gamepad.SetGamepadsForTesting(CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());

  navigator_gamepad.Gamepads();

  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_TRUE(frame.HasStickyUserActivation());
}

TEST_F(NavigatorGamepadTest, SubframeInFocusedPageGainsUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), /*chrome_client=*/nullptr,
      MakeGarbageCollected<SingleChildLocalFrameClient>());
  LocalFrame& main_frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  EXPECT_TRUE(page.GetFocusController().IsFocused());

  page_holder->GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<iframe></iframe>");
  auto* iframe =
      To<HTMLIFrameElement>(page_holder->GetDocument().body()->firstChild());
  ASSERT_TRUE(iframe);
  LocalFrame* child_frame = To<LocalFrame>(iframe->ContentFrame());
  ASSERT_TRUE(child_frame);

  EXPECT_TRUE(main_frame.GetDocument()->hasFocus());
  EXPECT_FALSE(child_frame->GetDocument()->hasFocus());

  Navigator* child_navigator = child_frame->DomWindow()->navigator();
  NavigatorGamepad& child_navigator_gamepad =
      NavigatorGamepad::From(*child_navigator);
  child_navigator_gamepad.SetGamepadsForTesting(
      CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(child_frame));
  EXPECT_FALSE(child_frame->HasStickyUserActivation());

  child_navigator_gamepad.Gamepads();

  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(child_frame));
  EXPECT_TRUE(child_frame->HasStickyUserActivation());
}

}  // namespace blink
