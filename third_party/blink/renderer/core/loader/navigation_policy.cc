/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_window_features.h"
#include "third_party/blink/renderer/core/events/current_input_event.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/ui_event_with_key_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

NavigationPolicy NavigationPolicyFromEventModifiers(
    int16_t button,
    bool ctrl,
    bool shift,
    bool alt,
    bool meta,
    bool is_link_preview_enabled) {
#if BUILDFLAG(IS_MAC)
  const bool new_tab_modifier = (button == 1) || meta;
#else
  const bool new_tab_modifier = (button == 1) || ctrl;
#endif
  if (!new_tab_modifier && !shift && !alt) {
    return kNavigationPolicyCurrentTab;
  } else if (is_link_preview_enabled && !new_tab_modifier && !shift && alt) {
    return kNavigationPolicyLinkPreview;
  } else if (new_tab_modifier) {
    return shift ? kNavigationPolicyNewForegroundTab
                 : kNavigationPolicyNewBackgroundTab;
  }
  return shift ? kNavigationPolicyNewWindow : kNavigationPolicyDownload;
}

NavigationPolicy NavigationPolicyFromEventInternal(
    const Event* event,
    bool is_link_preview_enabled) {
  if (!event)
    return kNavigationPolicyCurrentTab;

  if (const auto* mouse_event = DynamicTo<MouseEvent>(event)) {
    return NavigationPolicyFromEventModifiers(
        mouse_event->button(), mouse_event->ctrlKey(), mouse_event->shiftKey(),
        mouse_event->altKey(), mouse_event->metaKey(), is_link_preview_enabled);
  } else if (const KeyboardEvent* key_event = DynamicTo<KeyboardEvent>(event)) {
    // The click is simulated when triggering the keypress event.
    return NavigationPolicyFromEventModifiers(
        0, key_event->ctrlKey(), key_event->shiftKey(), key_event->altKey(),
        key_event->metaKey(), is_link_preview_enabled);
  } else if (const auto* gesture_event = DynamicTo<GestureEvent>(event)) {
    // The click is simulated when triggering the gesture-tap event
    return NavigationPolicyFromEventModifiers(
        0, gesture_event->ctrlKey(), gesture_event->shiftKey(),
        gesture_event->altKey(), gesture_event->metaKey(),
        is_link_preview_enabled);
  }
  return kNavigationPolicyCurrentTab;
}

NavigationPolicy NavigationPolicyFromCurrentEvent(
    bool is_link_preview_enabled) {
  const WebInputEvent* event = CurrentInputEvent::Get();
  if (!event)
    return kNavigationPolicyCurrentTab;

  int16_t button = 0;
  if (event->GetType() == WebInputEvent::Type::kMouseUp) {
    const WebMouseEvent* mouse_event = static_cast<const WebMouseEvent*>(event);

    switch (mouse_event->button) {
      case WebMouseEvent::Button::kLeft:
        button = 0;
        break;
      case WebMouseEvent::Button::kMiddle:
        button = 1;
        break;
      case WebMouseEvent::Button::kRight:
        button = 2;
        break;
      default:
        return kNavigationPolicyCurrentTab;
    }
  } else if ((WebInputEvent::IsKeyboardEventType(event->GetType()) &&
              static_cast<const WebKeyboardEvent*>(event)->windows_key_code ==
                  VKEY_RETURN) ||
             WebInputEvent::IsGestureEventType(event->GetType())) {
    // Keyboard and gesture events can simulate mouse events.
    button = 0;
  } else {
    return kNavigationPolicyCurrentTab;
  }

  return NavigationPolicyFromEventModifiers(
      button, event->GetModifiers() & WebInputEvent::kControlKey,
      event->GetModifiers() & WebInputEvent::kShiftKey,
      event->GetModifiers() & WebInputEvent::kAltKey,
      event->GetModifiers() & WebInputEvent::kMetaKey, is_link_preview_enabled);
}

}  // namespace

NavigationPolicy NavigationPolicyFromEvent(const Event* event) {
  // TODO(b:298160400): Add a setting to disable Link Preview.
  bool is_link_preview_enabled = IsLinkPreviewTriggerTypeEnabled(
      features::LinkPreviewTriggerType::kAltClick);

  NavigationPolicy event_policy =
      NavigationPolicyFromEventInternal(event, is_link_preview_enabled);
  NavigationPolicy input_policy =
      NavigationPolicyFromCurrentEvent(is_link_preview_enabled);

  if (event_policy == kNavigationPolicyDownload &&
      input_policy != kNavigationPolicyDownload) {
    // No downloads from synthesized events without user intention.
    return kNavigationPolicyCurrentTab;
  }

  if (event_policy == kNavigationPolicyLinkPreview &&
      input_policy != kNavigationPolicyLinkPreview) {
    // No Link Preview from synthesized events without user intention.
    return kNavigationPolicyCurrentTab;
  }

  if (event_policy == kNavigationPolicyNewBackgroundTab &&
      input_policy != kNavigationPolicyNewBackgroundTab &&
      !UIEventWithKeyState::NewTabModifierSetFromIsolatedWorld()) {
    // No "tab-unders" from synthesized events without user intention.
    // Events originating from an isolated world are exempt.
    return kNavigationPolicyNewForegroundTab;
  }

  return event_policy;
}

NavigationPolicy NavigationPolicyForCreateWindow(
    const WebWindowFeatures& features) {
  // If our default configuration was modified by a script or wasn't
  // created by a user gesture, then show as a popup. Else, let this
  // new window be opened as a toplevel window.
  bool as_popup = features.is_popup || !features.resizable;
  NavigationPolicy app_policy =
      as_popup ? kNavigationPolicyNewPopup : kNavigationPolicyNewForegroundTab;
  NavigationPolicy user_policy =
      NavigationPolicyFromCurrentEvent(/*is_link_preview_enabled=*/false);

  if (user_policy == kNavigationPolicyNewWindow &&
      app_policy == kNavigationPolicyNewPopup) {
    // User and app agree that we want a new window; let the app override the
    // decorations.
    return app_policy;
  }

  if (user_policy == kNavigationPolicyCurrentTab) {
    // User doesn't want a specific policy, use app policy instead.
    return app_policy;
  }

  if (user_policy == kNavigationPolicyDownload) {
    // When the input event suggests a download, but the navigation was
    // initiated by script, we should not override it.
    return app_policy;
  }

  return user_policy;
}

STATIC_ASSERT_ENUM(kWebNavigationPolicyDownload, kNavigationPolicyDownload);
STATIC_ASSERT_ENUM(kWebNavigationPolicyCurrentTab, kNavigationPolicyCurrentTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewBackgroundTab,
                   kNavigationPolicyNewBackgroundTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewForegroundTab,
                   kNavigationPolicyNewForegroundTab);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewWindow, kNavigationPolicyNewWindow);
STATIC_ASSERT_ENUM(kWebNavigationPolicyNewPopup, kNavigationPolicyNewPopup);
STATIC_ASSERT_ENUM(kWebNavigationPolicyPictureInPicture,
                   kNavigationPolicyPictureInPicture);

}  // namespace blink
