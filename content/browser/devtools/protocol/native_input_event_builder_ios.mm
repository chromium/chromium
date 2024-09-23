// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/native_input_event_builder.h"

#include "base/apple/owned_objc.h"

namespace content::protocol {

gfx::NativeEvent NativeInputEventBuilder::CreateEvent(
    const input::NativeWebKeyboardEvent& event) {
  // We only need this for Macs because they require an OS event to process
  // some keyboard events in browser (see: https://crbug.com/667387). TODO: Does
  // this hold true for iOS Blink?
  return base::apple::OwnedUIEvent();
}

}  // namespace content::protocol
