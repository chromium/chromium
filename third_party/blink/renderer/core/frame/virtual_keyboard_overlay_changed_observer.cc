// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/virtual_keyboard_overlay_changed_observer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

VirtualKeyboardOverlayChangedObserver::VirtualKeyboardOverlayChangedObserver(
    LocalFrame* frame) {
  if (frame)
    frame->RegisterVirtualKeyboardOverlayChangedObserver(this);
}

}  // namespace blink
