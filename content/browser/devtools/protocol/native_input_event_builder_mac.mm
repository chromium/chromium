// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include <algorithm>

#include "base/strings/sys_string_conversions.h"
#include "content/browser/devtools/protocol/native_input_event_builder.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {
namespace protocol {

// Mac requires a native event to emulate key events. This method gives
// only crude capabilities (see: crbug.com/667387).
// The returned object has a retain count of 1.
gfx::NativeEvent NativeInputEventBuilder::CreateEvent(
    const NativeWebKeyboardEvent& event) {
  NSEventType type = NSEventTypeKeyUp;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::Type::kKeyDown)
    type = NSEventTypeKeyDown;
  const char16_t* textStartAddr = &event.text[0];
  const int textLength =
      std::find(textStartAddr,
                textStartAddr + NativeWebKeyboardEvent::kTextLengthCap, '\0') -
      textStartAddr;
  NSString* character =
      base::SysUTF16ToNSString(std::u16string(textStartAddr, textLength));
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

  return [[NSEvent keyEventWithType:type
                           location:NSZeroPoint
                      modifierFlags:flags
                          timestamp:0
                       windowNumber:0
                            context:nil
                         characters:character
        charactersIgnoringModifiers:character
                          isARepeat:NO
                            keyCode:event.native_key_code] retain];
}

}  // namespace protocol
}  // namespace content
