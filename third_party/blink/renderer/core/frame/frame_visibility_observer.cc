// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_visibility_observer.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

FrameVisibilityObserver::FrameVisibilityObserver(LocalFrame* frame) {
  frame->GetFrameVisibilityObserverSet().insert(this);
}

}  // namespace blink
