// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class LocalFrame;

// This is an observer to observe changes to the in-viewport visibility of a
// given frame.
class CORE_EXPORT FrameVisibilityObserver : public GarbageCollectedMixin {
 public:
  virtual ~FrameVisibilityObserver();

  // TODO(crbug.com/351354996): Remove this after the refactor is completed.
  virtual void FrameVisibilityChanged(mojom::blink::FrameVisibility) {}

  // TODO(crbug.com/351354996): Make these methods pure virtual in the follow-up
  // CL.
  // Called when the observed frame becomes not rendered - i.e., when the
  // frame visibility status is `blink::mojom::FrameVisibility::kNotRendered`.
  virtual void OnFrameHidden() {}
  virtual void OnFrameShown() {}

 protected:
  explicit FrameVisibilityObserver(LocalFrame*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_VISIBILITY_OBSERVER_H_
