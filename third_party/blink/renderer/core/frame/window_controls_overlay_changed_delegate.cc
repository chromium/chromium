// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/window_controls_overlay_changed_delegate.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

WindowControlsOverlayChangedDelegate::WindowControlsOverlayChangedDelegate(
    LocalFrame* frame) {
  if (frame)
    frame->RegisterWindowControlsOverlayChangedDelegate(this);
}

}  // namespace blink
