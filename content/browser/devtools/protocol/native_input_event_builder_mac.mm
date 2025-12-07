// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/native_input_event_builder.h"

#include <Cocoa/Cocoa.h>

#include <algorithm>
#include <ranges>
#include <string_view>

#include "base/apple/owned_objc.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content::protocol {

// Mac requires a native event to emulate key events. This method gives
// only crude capabilities (see: crbug.com/667387).
gfx::NativeEvent NativeInputEventBuilder::CreateEvent(
    const input::NativeWebKeyboardEvent& event) {
  NSEventType type = NSEventTypeKeyUp;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::Type::kKeyDown)
    type = NSEventTypeKeyDown;
  size_t length = std::ranges::find(event.text, u'\0') - event.text.begin();
  std::u16string_view text_view =
      base::as_string_view(base::span(event.text).first(length));
  NSString* character = base::SysUTF16ToNSString(std::u16string(text_view));
  int modifiers = event.GetModifiers();
  NSUInteger flags =
      (modifiers & blink::WebInputEvent::kShiftKey ? NSEventModifierFlagShift
                                                   : 0) |
      (modifiers & blink::WebInputEvent::kControlKey
           ? NSEventModifierFlagControl
           : 0) |
      (modifiers & blink::WebInputEvent::kAltKey ? NSEventModifierFlagOption
                                                 : 0) |
      (modifiers & blink::WebInputEvent::kMetaKey ? NSEventModifierFlagCommand
                                                  : 0);

  return base::apple::OwnedNSEvent([NSEvent
                 keyEventWithType:type
                         location:NSZeroPoint
                    modifierFlags:flags
                        timestamp:0
                     windowNumber:0
                          context:nil
                       characters:character
      charactersIgnoringModifiers:character
                        isARepeat:NO
                          keyCode:event.native_key_code]);
}

}  // namespace content::protocol
