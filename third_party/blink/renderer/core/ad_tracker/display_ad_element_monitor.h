// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_DISPLAY_AD_ELEMENT_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_DISPLAY_AD_ELEMENT_MONITOR_H_

#include <optional>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"

namespace blink {

// Tracks a specific ad `Element` and reports its location and updates
// (including its removal and re-insertion) to the `PageTimingMetricsSender`.
class CORE_EXPORT DisplayAdElementMonitor final
    : public GarbageCollected<DisplayAdElementMonitor>,
      public LocalFrameView::LifecycleNotificationObserver,
      public NodeRareDataField {
 public:
  enum class OverlayVisibility {
    kSkipped,
    kVisible,
    kInvisible,
  };

  explicit DisplayAdElementMonitor(Element* element,
                                   AdProvenance ad_provenance);

  // Start receiving LifecycleNotificationObserver notifications if the element
  // is eligible for ad monitoring. No-op if notifications are already active.
  void EnsureStarted();

  // Stop receiving LifecycleNotificationObserver notifications, and
  // notify `PageTimingMetricsSender` of the removal.
  void OnElementRemoved();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(
      const LocalFrameView& local_frame_view) override;

  // Performs a hit-test on `element_` to determine if it's the topmost element
  // at its center. This check can be skipped due to frequency-capping or if the
  // element is outside the viewport to reduce performance impact.
  // Set `ignore_throttling` to true to bypass the frequency capping.
  OverlayVisibility CheckOverlayVisibility(const LocalFrame& main_frame,
                                           const gfx::Rect& rect_in_viewport,
                                           bool ignore_throttling = false);

  bool ShouldHighlight() const { return should_highlight_; }

  bool IsVideoAd() const { return is_video_ad_; }
  void UpdateToVideoAd();

  const AdProvenance& GetAdProvenance() const { return ad_provenance_; }

  void Trace(Visitor*) const override;

 private:
  // Stores the initial viewport scroll position and the ad's Y-position when
  // first observed, or when its Y-position changes. This establishes the
  // "anchor" used to detect if the ad remains stationary during scrolling.
  struct StickyAdMeasurement {
    // The outermost main frame's scroll position (Y-axis) when measured.
    int viewport_scroll_position;

    // The ad's Y-coordinate relative to the viewport.
    int ad_y_position_in_viewport;

    // The ad's height when measured.
    int ad_height;
  };

  // Evaluates whether the ad is sticky based on its movement relative to the
  // viewport. Called on every lifecycle update, but remains inexpensive as it
  // only performs simple arithmetic on cached geometry.
  //
  // Returns the result of the unthrottled hit-test if one was performed as a
  // final sanity check; otherwise, returns `OverlayVisibility::kSkipped`.
  OverlayVisibility CalculateStickyAdState(
      const LocalFrame& local_root_main_frame,
      const gfx::Rect& rect_in_viewport);

  // Updates the internal state to reflect that the ad is sticky.
  // `main_frame_viewport` is the outermost main frame's viewport anchored at
  // (0,0). `ad_visible_rect` is the portion of the ad visible within the
  // `main_frame_viewport`. Both use the main frame's viewport coordinate space.
  void UpdateToStickyAd(const LocalFrame& local_root_main_frame,
                        const gfx::Rect& main_frame_viewport,
                        const gfx::Rect& ad_visible_rect);

  // Updates the internal state to reflect that the ad is a sticky video ad, and
  // records the kStickyVideoAdDetected use counter.
  void UpdateToStickyVideoAd();

  // Records the video ad UseCounter if the element is a video ad with
  // non-trivial geometry.
  void MaybeRecordVideoAdUseCounter();

  Member<Element> element_;

  AdProvenance ad_provenance_;

  bool started_ = false;
  bool is_video_ad_ = false;
  bool did_record_video_ad_use_counter_ = false;

  bool is_sticky_ad_ = false;
  bool is_sticky_video_ad_ = false;
  std::optional<StickyAdMeasurement> sticky_ad_measurement_;

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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_DISPLAY_AD_ELEMENT_MONITOR_H_
