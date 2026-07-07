// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// This is an observer to observe changes to the visibility of a given frame.
// For example, a frame is hidden when:
// - The "visibility" property is set to "hidden",
// - The "display" property is set to "none",
// - The frame has zero area (width or height is 0).
class CORE_EXPORT FrameVisibilityObserver : public GarbageCollectedMixin {
 public:
  virtual void OnFrameHidden() = 0;
  virtual void OnFrameShown() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_
