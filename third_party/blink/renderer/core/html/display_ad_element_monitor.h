// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DISPLAY_AD_ELEMENT_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DISPLAY_AD_ELEMENT_MONITOR_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

// Tracks a specific ad `Element` and reports its location and updates
// (including its removal and re-insertion) to the `PageTimingMetricsSender`.
class CORE_EXPORT DisplayAdElementMonitor
    : public GarbageCollected<DisplayAdElementMonitor>,
      public LocalFrameView::LifecycleNotificationObserver {
 public:
  explicit DisplayAdElementMonitor(Element* element);

  // Start receiving LifecycleNotificationObserver notifications if a
  // LocalFrameView exists. No-op if notifications are already active.
  void EnsureStarted();

  // Stop receiving LifecycleNotificationObserver notifications, and
  // notify `PageTimingMetricsSender` of the removal.
  void OnElementRemoved();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(
      const LocalFrameView& local_frame_view) override;

  void Trace(Visitor*) const override;

 private:
  Member<Element> element_;

  bool started_ = false;

  bool ad_use_counter_recorded_ = false;

  // The last rectangle reported to the `PageTimingMetricsSender`.
  // `last_reported_rect_` is empty if there's no report before, or if the last
  // report was used to signal the removal of this element (i.e. both cases
  // will be handled the same way).
  gfx::Rect last_reported_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DISPLAY_AD_ELEMENT_MONITOR_H_
