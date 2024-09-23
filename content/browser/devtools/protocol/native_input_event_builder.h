// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_

#include "build/build_config.h"
#include "components/input/native_web_keyboard_event.h"

namespace content::protocol {

class NativeInputEventBuilder {
 public:
#if BUILDFLAG(IS_APPLE)
  static gfx::NativeEvent CreateEvent(
      const input::NativeWebKeyboardEvent& event);
#else
  // We only need this for Macs because they require an OS event to process
  // some keyboard events in browser (see: crbug.com/667387).
  static gfx::NativeEvent CreateEvent(
      const input::NativeWebKeyboardEvent& event) {
    return nullptr;
  }
#endif
};

}  // namespace content::protocol

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_NATIVE_INPUT_EVENT_BUILDER_H_
