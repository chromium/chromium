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
      public LocalFrameView::LifecycleNotificationObserver,
      public ElementRareDataField {
 public:
  enum class OverlayVisibility {
    kSkipped,
    kVisible,
    kInvisible,
  };

  explicit DisplayAdElementMonitor(Element* element);

  // Start receiving LifecycleNotificationObserver notifications if the element
  // is eligible for ad monitoring. No-op if notifications are already active.
  void EnsureStarted();

  // Stop receiving LifecycleNotificationObserver notifications, and
  // notify `PageTimingMetricsSender` of the removal.
  void OnElementRemovedOrUntagged();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(
      const LocalFrameView& local_frame_view) override;

  // Performs a hit-test on `element_` to determine if it's the topmost element
  // at its center. This check can be skipped due to frequency-capping or if the
  // element is outside the viewport to reduce performance impact.
  OverlayVisibility CheckOverlayVisibility(const LocalFrame& main_frame,
                                           const gfx::Rect& rect_in_viewport);

  bool ShouldHighlight() const { return should_highlight_; }

  void Trace(Visitor*) const override;

 private:
  Member<Element> element_;

  bool started_ = false;

  // Caches the last known value of the DevTools "Highlight ads" setting. This
  // value remains `false` if the element is not eligible for monitoring (e.g.,
  // a nested ad).
  bool should_highlight_ = false;

  base::TimeTicks last_overlay_check_time_;

  // The last rectangle reported to the `PageTimingMetricsSender`.
  // `last_reported_rect_` is empty if there's no report before, or if the last
  // report was used to signal the removal of this element (i.e. both cases
  // will be handled the same way).
  gfx::Rect last_reported_rect_;

  // Elements are treated as visible by default as a best-effort approach (i.e.,
  // for performance reasons, we only check for overlay visibility for elements
  // within the viewport). In practice, elements that are overlaid at the bottom
  // of a page are often fixed-position within the viewport, so this heuristic
  // works well.
  OverlayVisibility overlay_visibility_ = OverlayVisibility::kVisible;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_DISPLAY_AD_ELEMENT_MONITOR_H_
