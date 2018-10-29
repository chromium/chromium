// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#import <AppKit/AppKit.h>

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace content {

namespace {

int modifiersForEvent(int modifiers) {
  int flags = 0;
  if (modifiers & blink::WebInputEvent::kControlKey)
    flags |= NSControlKeyMask;
  if (modifiers & blink::WebInputEvent::kShiftKey)
    flags |= NSShiftKeyMask;
  if (modifiers & blink::WebInputEvent::kAltKey)
    flags |= NSAlternateKeyMask;
  if (modifiers & blink::WebInputEvent::kMetaKey)
    flags |= NSCommandKeyMask;
  if (modifiers & blink::WebInputEvent::kCapsLockOn)
    flags |= NSAlphaShiftKeyMask;
  return flags;
}

size_t WebKeyboardEventTextLength(const blink::WebUChar* text) {
  size_t text_length = 0;
  while (text_length < blink::WebKeyboardEvent::kTextLengthCap &&
         text[text_length]) {
    ++text_length;
  }
  return text_length;
}

}  // namepsace

NativeWebKeyboardEvent::NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                                               int modifiers,
                                               base::TimeTicks timestamp)
    : WebKeyboardEvent(type, modifiers, timestamp),
      os_event(NULL),
      skip_in_browser(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const blink::WebKeyboardEvent& web_event,
    gfx::NativeView native_view)
    : WebKeyboardEvent(web_event), os_event(nullptr), skip_in_browser(false) {
  NSEventType type = NSKeyUp;
  int flags = modifiersForEvent(web_event.GetModifiers());
  if (web_event.GetType() == blink::WebInputEvent::kChar ||
      web_event.GetType() == blink::WebInputEvent::kRawKeyDown ||
      web_event.GetType() == blink::WebInputEvent::kKeyDown) {
    type = NSKeyDown;
  }
  size_t text_length = WebKeyboardEventTextLength(web_event.text);
  size_t unmod_text_length =
      WebKeyboardEventTextLength(web_event.unmodified_text);

  if (text_length == 0)
    type = NSFlagsChanged;

  NSString* text =
      [[[NSString alloc] initWithCharacters:web_event.text length:text_length]
          autorelease];
  NSString* unmodified_text =
      [[[NSString alloc] initWithCharacters:web_event.unmodified_text
                                     length:unmod_text_length] autorelease];

  os_event = [[NSEvent keyEventWithType:type
                               location:NSZeroPoint
                          modifierFlags:flags
                              timestamp:ui::EventTimeStampToSeconds(
                                            web_event.TimeStamp())
                           windowNumber:[[native_view.GetNativeNSView() window]
                                            windowNumber]
                                context:nil
                             characters:text
            charactersIgnoringModifiers:unmodified_text
                              isARepeat:NO
                                keyCode:web_event.native_key_code] retain];
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(gfx::NativeEvent native_event)
    : WebKeyboardEvent(WebKeyboardEventBuilder::Build(native_event)),
      os_event([native_event retain]),
      skip_in_browser(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(const ui::KeyEvent& key_event)
    : NativeWebKeyboardEvent(key_event.native_event()) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event([other.os_event retain]),
      skip_in_browser(other.skip_in_browser) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  NSObject* previous = os_event;
  os_event = [other.os_event retain];
  [previous release];

  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  [os_event release];
}

}  // namespace content
