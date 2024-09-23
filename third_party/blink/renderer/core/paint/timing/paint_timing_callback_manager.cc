// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/paint_timing_callback_manager.h"

#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

void PaintTimingCallbackManagerImpl::ReportPaintTime(
    std::unique_ptr<PaintTimingCallbackManager::CallbackQueue> frame_callbacks,
    const viz::FrameTimingDetails& presentation_details) {
  // Do not report any paint timings for detached frames.
  if (frame_view_->GetFrame().IsDetached()) {
    return;
  }

  while (!frame_callbacks->empty()) {
    std::move(frame_callbacks->front())
        .Run(presentation_details.presentation_feedback.timestamp);
    frame_callbacks->pop();
  }
  frame_view_->GetPaintTimingDetector().UpdateLcpCandidate();
}

void PaintTimingCallbackManagerImpl::
    RegisterPaintTimeCallbackForCombinedCallbacks() {
  DCHECK(!frame_callbacks_->empty());
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage()) {
    return;
  }

  auto combined_callback = CrossThreadBindOnce(
      &PaintTimingCallbackManagerImpl::ReportPaintTime,
      WrapCrossThreadWeakPersistent(this), std::move(frame_callbacks_));
  frame_callbacks_ =
      std::make_unique<PaintTimingCallbackManager::CallbackQueue>();

  // |ReportPaintTime| on |layerTreeView| will queue a presentation-promise, the
  // callback is called when the presentation for current render frame completes
  // or fails to happen.
  frame.GetPage()->GetChromeClient().NotifyPresentationTime(
      frame, std::move(combined_callback));
}

void PaintTimingCallbackManagerImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  PaintTimingCallbackManager::Trace(visitor);
}
}  // namespace blink
