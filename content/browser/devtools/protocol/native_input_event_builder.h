// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_

#include "build/build_config.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace content {
namespace protocol {

class NativeInputEventBuilder {
 public:
#if BUILDFLAG(IS_MAC)
  // This returned object has a retain count of 1.
  static gfx::NativeEvent CreateEvent(const NativeWebKeyboardEvent& event);
#else
  // We only need this for Macs because they require an OS event to process
  // some keyboard events in browser (see: crbug.com/667387).
  static gfx::NativeEvent CreateEvent(const NativeWebKeyboardEvent& event) {
    return nullptr;
  }
#endif
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_
