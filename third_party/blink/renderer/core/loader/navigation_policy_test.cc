/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "base/auto_reset.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/page/create_window.h"

namespace blink {

class NavigationPolicyTest : public testing::Test {
 protected:
  NavigationPolicy GetPolicyForCreateWindow(int modifiers,
                                            WebMouseEvent::Button button,
                                            bool as_popup) {
    WebMouseEvent event(WebInputEvent::kMouseUp, modifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.button = button;
    if (as_popup)
      features.tool_bar_visible = false;
    base::AutoReset<const WebInputEvent*> current_event_change(
        &CurrentInputEvent::current_input_event_, &event);
    return NavigationPolicyForCreateWindow(features);
  }

  Event* GetEvent(int modifiers, WebMouseEvent::Button button) {
    MouseEventInit* mouse_initializer = MouseEventInit::Create();
    if (button == WebMouseEvent::Button::kLeft)
      mouse_initializer->setButton(0);
    if (button == WebMouseEvent::Button::kMiddle)
      mouse_initializer->setButton(1);
    if (button == WebMouseEvent::Button::kRight)
      mouse_initializer->setButton(2);
    if (modifiers & WebInputEvent::kShiftKey)
      mouse_initializer->setShiftKey(true);
    if (modifiers & WebInputEvent::kControlKey)
      mouse_initializer->setCtrlKey(true);
    if (modifiers & WebInputEvent::kAltKey)
      mouse_initializer->setAltKey(true);
    if (modifiers & WebInputEvent::kMetaKey)
      mouse_initializer->setMetaKey(true);
    return MouseEvent::Create(nullptr, event_type_names::kClick,
                              mouse_initializer);
  }

  NavigationPolicy GetPolicyFromEvent(int modifiers,
                                      WebMouseEvent::Button button,
                                      int user_modifiers,
                                      WebMouseEvent::Button user_button) {
    WebMouseEvent event(WebInputEvent::kMouseUp, user_modifiers,
                        WebInputEvent::GetStaticTimeStampForTests());
    event.button = user_button;
    base::AutoReset<const WebInputEvent*> current_event_change(
        &CurrentInputEvent::current_input_event_, &event);
    return NavigationPolicyFromEvent(GetEvent(modifiers, button));
  }

  WebWindowFeatures features;
};

TEST_F(NavigationPolicyTest, LeftClick) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, LeftClickPopup) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ShiftLeftClick) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewWindow,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ShiftLeftClickPopup) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaLeftClickPopup) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaAndShiftLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = false;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, ControlOrMetaAndShiftLeftClickPopup) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  modifiers |= WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  bool as_popup = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, MiddleClick) {
  int modifiers = 0;
  bool as_popup = false;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, MiddleClickPopup) {
  int modifiers = 0;
  bool as_popup = true;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyForCreateWindow(modifiers, button, as_popup));
}

TEST_F(NavigationPolicyTest, NoToolbarsForcesPopup) {
  features.tool_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.tool_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NoStatusBarForcesPopup) {
  features.status_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.status_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NoMenuBarForcesPopup) {
  features.menu_bar_visible = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.menu_bar_visible = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, NoOpener) {
  static const struct {
    const char* feature_string;
    NavigationPolicy policy;
  } kCases[] = {
      {"", kNavigationPolicyNewForegroundTab},
      {"something", kNavigationPolicyNewPopup},
      {"something, something", kNavigationPolicyNewPopup},
      {"notnoopener", kNavigationPolicyNewPopup},
      {"noopener", kNavigationPolicyNewForegroundTab},
      {"something, noopener", kNavigationPolicyNewPopup},
      {"noopener, something", kNavigationPolicyNewPopup},
      {"NoOpEnEr", kNavigationPolicyNewForegroundTab},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.policy,
              NavigationPolicyForCreateWindow(
                  GetWindowFeaturesFromString(test.feature_string)))
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(NavigationPolicyTest, NoOpenerAndNoReferrer) {
  static const struct {
    const char* feature_string;
    NavigationPolicy policy;
  } kCases[] = {
      {"", kNavigationPolicyNewForegroundTab},
      {"noopener, noreferrer", kNavigationPolicyNewForegroundTab},
      {"noopener, notreferrer", kNavigationPolicyNewPopup},
      {"notopener, noreferrer", kNavigationPolicyNewPopup},
      {"something, noopener, noreferrer", kNavigationPolicyNewPopup},
      {"noopener, noreferrer, something", kNavigationPolicyNewPopup},
      {"noopener, something, noreferrer", kNavigationPolicyNewPopup},
      {"NoOpEnEr, NoReFeRrEr", kNavigationPolicyNewForegroundTab},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.policy,
              NavigationPolicyForCreateWindow(
                  GetWindowFeaturesFromString(test.feature_string)))
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(NavigationPolicyTest, NoReferrer) {
  static const struct {
    const char* feature_string;
    NavigationPolicy policy;
  } kCases[] = {
      {"", kNavigationPolicyNewForegroundTab},
      {"something", kNavigationPolicyNewPopup},
      {"something, something", kNavigationPolicyNewPopup},
      {"notreferrer", kNavigationPolicyNewPopup},
      {"noreferrer", kNavigationPolicyNewForegroundTab},
      {"something, noreferrer", kNavigationPolicyNewPopup},
      {"noreferrer, something", kNavigationPolicyNewPopup},
      {"NoReFeRrEr", kNavigationPolicyNewForegroundTab},
  };

  for (const auto& test : kCases) {
    EXPECT_EQ(test.policy,
              NavigationPolicyForCreateWindow(
                  GetWindowFeaturesFromString(test.feature_string)))
        << "Testing '" << test.feature_string << "'";
  }
}

TEST_F(NavigationPolicyTest, NotResizableForcesPopup) {
  features.resizable = false;
  EXPECT_EQ(kNavigationPolicyNewPopup,
            NavigationPolicyForCreateWindow(features));
  features.resizable = true;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyForCreateWindow(features));
}

TEST_F(NavigationPolicyTest, EventLeftClick) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyCurrentTab,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventShiftLeftClick) {
  int modifiers = WebInputEvent::kShiftKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewWindow,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventControlOrMetaLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventControlOrMetaLeftClickWithUserEvent) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyFromEvent(modifiers, button, modifiers, button));
}

TEST_F(NavigationPolicyTest,
       EventControlOrMetaLeftClickWithDifferentUserEvent) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey;
#else
  int modifiers = WebInputEvent::kControlKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyFromEvent(modifiers, button, 0, button));
}

TEST_F(NavigationPolicyTest, EventShiftControlOrMetaLeftClick) {
#if defined(OS_MACOSX)
  int modifiers = WebInputEvent::kMetaKey | WebInputEvent::kShiftKey;
#else
  int modifiers = WebInputEvent::kControlKey | WebInputEvent::kShiftKey;
#endif
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventMiddleClick) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventMiddleClickWithUserEvent) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  EXPECT_EQ(kNavigationPolicyNewBackgroundTab,
            GetPolicyFromEvent(modifiers, button, modifiers, button));
}

TEST_F(NavigationPolicyTest, EventMiddleClickWithDifferentUserEvent) {
  int modifiers = 0;
  WebMouseEvent::Button button = WebMouseEvent::Button::kMiddle;
  WebMouseEvent::Button user_button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyNewForegroundTab,
            GetPolicyFromEvent(modifiers, button, modifiers, user_button));
}

TEST_F(NavigationPolicyTest, EventAltClick) {
  int modifiers = WebInputEvent::kAltKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyCurrentTab,
            NavigationPolicyFromEvent(GetEvent(modifiers, button)));
}

TEST_F(NavigationPolicyTest, EventAltClickWithUserEvent) {
  int modifiers = WebInputEvent::kAltKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyDownload,
            GetPolicyFromEvent(modifiers, button, modifiers, button));
}

TEST_F(NavigationPolicyTest, EventAltClickWithDifferentUserEvent) {
  int modifiers = WebInputEvent::kAltKey;
  WebMouseEvent::Button button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(kNavigationPolicyCurrentTab,
            GetPolicyFromEvent(modifiers, button, 0, button));
}

}  // namespace blink
