// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_DISPLAY_AD_ELEMENT_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AD_TRACKER_DISPLAY_AD_ELEMENT_MONITOR_H_

#include <optional>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

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
  void WillDisposeView() override;

  // Performs a hit-test at the center of the in-viewport part of
  // `rect_in_viewport` to determine if the front-most content there is rendered
  // by `element_` itself (including its user-agent shadow tree and, for a frame
  // owner, the content of its frame), as opposed to an overlay or one of its
  // descendant elements. Transparent boxes in front of it (e.g., an overlay
  // hosting video controls) are looked through.
  // This check can be skipped due to frequency-capping or if the element is
  // outside the viewport to reduce performance impact. Set `ignore_throttling`
  // to true to bypass the frequency capping.
  OverlayVisibility CheckOverlayVisibility(const LocalFrame& main_frame,
                                           const gfx::Rect& rect_in_viewport,
                                           bool ignore_throttling = false);

  bool ShouldHighlight() const { return should_highlight_; }

  bool IsVideoAd() const { return is_video_ad_; }
  void UpdateToVideoAd();

  bool IsOverlayPopupAd() const { return is_overlay_popup_ad_; }
  void SetIsOverlayPopupAd();

  const AdProvenance& GetAdProvenance() const { return ad_provenance_; }

  void Trace(Visitor*) const override;

 private:
  // The ad's placement, as observed within the viewport while the page scrolls.
  // Once determined, it may alternate between `kInline` and `kSticky` as the ad
  // moves (e.g., a video player that docks and undocks), but never reverts to
  // `kUnknown`.
  enum class Placement {
    // Not determined yet: the ad hasn't been seen within the viewport across
    // enough of a scroll.
    kUnknown,
    // Scrolls along with the document.
    kInline,
    // Stays in place while the page scrolls.
    kSticky,
  };

  // Stores the viewport scroll offset and the ad's Y-position relative to the
  // document when first observed, or when the latter changes. This establishes
  // the "anchor" used to detect if the ad scrolls along with the document.
  // Both are `LayoutUnit`s, the precision at which layout positions the ad and
  // applies the scroll offset, so the document position can be compared
  // exactly, free of rounding errors.
  struct InlineAdMeasurement {
    // The outermost main frame's scroll offset (Y-axis) when measured.
    LayoutUnit viewport_scroll_offset;

    // The ad's Y-coordinate relative to the document.
    LayoutUnit ad_y_position_in_document;
  };

  // Stores the initial viewport scroll position and the ad's Y-position when
  // first observed, or when its Y-position changes. This establishes the
  // "anchor" used to detect if the ad remains stationary during scrolling.
  struct StickyAdMeasurement {
    // The outermost main frame's scroll position (Y-axis) when measured.
    int viewport_scroll_position;

    // The ad's Y-coordinate relative to the viewport.
    int ad_y_position_in_viewport;

    // How far `position: sticky` shifted the ad vertically when measured,
    // counting only boxes that stick to the main frame's viewport.
    int viewport_sticky_offset_y;
  };

  // Evaluates whether the ad is inline, i.e., keeps its position relative to
  // the document while the page scrolls, and tracks where it was last seen
  // inline (see `Placement`).
  void CalculateInlineAdState(const LocalFrame& local_root_main_frame,
                              const gfx::Rect& rect_in_viewport,
                              LayoutUnit ad_y_position_in_document,
                              LayoutUnit viewport_scroll_offset);

  // Evaluates whether the ad is sticky based on its movement relative to the
  // viewport. Called on every lifecycle update, but remains inexpensive:
  // besides simple arithmetic, it only re-walks part of the containing block
  // chain that `AbsoluteBoundingBoxRectF()` already traverses.
  //
  // Returns the result of the unthrottled hit-test if one was performed as a
  // final sanity check; otherwise, returns `OverlayVisibility::kSkipped`.
  OverlayVisibility CalculateStickyAdState(
      const LocalFrame& local_root_main_frame,
      const gfx::Rect& rect_in_viewport);

  // Updates the internal state to reflect that the ad is sticky, having been
  // `kInline` or `kUnknown` until now. `main_frame_viewport` is the outermost
  // main frame's viewport anchored at (0,0). `rect_in_viewport` is the ad's
  // rectangle, not clipped to the viewport. Both use the main frame's viewport
  // coordinate space.
  void UpdateToStickyAd(const LocalFrame& local_root_main_frame,
                        const gfx::Rect& main_frame_viewport,
                        const gfx::Rect& rect_in_viewport);

  // Updates the internal state to reflect that the ad is a sticky video ad, and
  // records the kStickyVideoAdDetected use counter.
  void UpdateToStickyVideoAd();

  // Updates the internal state to reflect that the ad is a sticky pop-out ad,
  // i.e., it was displayed inline before it became sticky elsewhere within the
  // viewport.
  void UpdateToStickyPopOutAd();

  // Updates the internal state to reflect that the ad is a sticky pop-out video
  // ad, and records the kStickyPopOutVideoAdDetected use counter.
  void UpdateToStickyPopOutVideoAd();

  // Records the video ad UseCounter if the element is a video ad with
  // non-trivial geometry.
  void MaybeRecordVideoAdUseCounter();

  Member<Element> element_;

  AdProvenance ad_provenance_;

  bool started_ = false;
  bool is_video_ad_ = false;
  bool is_overlay_popup_ad_ = false;
  bool did_record_video_ad_use_counter_ = false;

  // The anchor for `CalculateInlineAdState`.
  std::optional<InlineAdMeasurement> inline_ad_measurement_;

  // The ad's rectangle relative to the outermost main frame's viewport when it
  // was last seen inline. Only meaningful once the placement has been
  // `kInline`.
  gfx::Rect last_inline_ad_rect_in_viewport_;

  // The anchor for `CalculateStickyAdState`.
  std::optional<StickyAdMeasurement> sticky_ad_measurement_;

  // The ad's current placement.
  Placement placement_ = Placement::kUnknown;

  // Whether the ad has ever been found to be a sticky ad, a sticky video ad, a
  // sticky pop-out ad, or a sticky pop-out video ad, respectively. Unlike
  // `placement_`, these are never reset: each sub-type is only recorded once,
  // and an ad that has popped back in still counts as a sticky (pop-out) ad if
  // it's tagged as a video ad later.
  bool is_sticky_ad_ = false;
  bool is_sticky_video_ad_ = false;
  bool is_sticky_pop_out_ad_ = false;
  bool is_sticky_pop_out_video_ad_ = false;

  // Caches the last known value of the DevTools "Highlight ads" setting. This
  // value remains `false` prior to First Contentful Paint or if the element is
  // not eligible for monitoring (e.g., a nested ad).
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
