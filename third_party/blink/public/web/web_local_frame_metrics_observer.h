// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_METRICS_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_METRICS_OBSERVER_H_

#include <stdint.h>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {
class WebLocalFrame;
class WebLocalFrameImpl;
class WebURLRequest;
class WebURLResponse;
struct JavaScriptFrameworkDetectionResult;

// Base class for objects that want to get notified of metrics events on a
// local frame.
class BLINK_EXPORT WebLocalFrameMetricsObserver : public base::CheckedObserver {
 public:
  // Called when the observed WebLocalFrame is detached.
  virtual void OnFrameDetached() {}

  // A user interaction is observed. A user interaction can be built up from
  // multiple input events (e.g. keydown then keyup). Each of these events has
  // an input to next frame latency. This reports the timings of the max
  // input-to-frame latency for each interaction. `max_event_start` is when
  // input was received, `max_event_end` is when the next frame was
  // presented, `max_event_queued_main_thread` is when the input was queued and
  // `max_event_commit_finish` is when the next commit finished after event has
  // been processed. See https://web.dev/inp/#whats-in-an-interaction for more
  // detailed motivation and explanation.
  virtual void DidObserveUserInteraction(
      base::TimeTicks max_event_start,
      base::TimeTicks max_event_queued_main_thread,
      base::TimeTicks max_event_processing_start,
      base::TimeTicks max_event_commit_finish,
      base::TimeTicks max_event_end,
      uint64_t interaction_offset,
      uint64_t performance_timeline_navigation_id) {}

  // A cpu task or tasks completed. Triggered when at least 100ms of wall time
  // was spent in tasks on the frame.
  virtual void DidChangeCpuTiming(base::TimeDelta time,
                                  base::TimeDelta ad_time) {}

  // Blink detected a JavaScript framework that the browser process will use for
  // UKM.
  virtual void DidObserveJavaScriptFrameworks(
      const JavaScriptFrameworkDetectionResult&) {}

  // A subresource load is observed.
  // It is called when there is a subresource load. The reported values via
  // arguments are cumulative. They are NOT a difference from the previous call.
  virtual void DidObserveSubresourceLoad(
      const SubresourceLoadMetrics& subresource_load_metrics) {}

  // Blink hits the code path for a certain UseCounterFeature for the first time
  // on this frame. As a performance optimization, features already hit on other
  // frames associated with the same page in the renderer are not currently
  // reported.
  virtual void DidObserveNewFeatureUsage(const UseCounterFeature&) {}

  // A new First Contentful Paint was observed for a soft navigation.
  virtual void DidObserveSoftNavigationFirstContentfulPaint(
      uint64_t performance_timeline_navigation_id,
      base::TimeDelta first_contentful_paint) {}

  // A new largest contentful paint candidate relating to the most recent
  // soft navigation was observed. Also see DidObserveSoftNavigation().
  virtual void DidObserveSoftLargestContentfulPaint(
      const LargestContentfulPaintDetailsForReporting& lcp) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  virtual void DidObserveLayoutShift(
      double score,
      bool after_input_or_scroll,
      uint64_t performance_timeline_navigation_id) {}

  // Called when the main frame's document rectangle changed, e.g. resizing a
  // tab causes the document width to change, or loading additional content
  // causes the document height to increase. Only invoked on the outermost main
  // frame.
  virtual void OnMainFrameRectangleChanged(const gfx::Rect& main_frame_rect) {}

  // Called when the main frame's viewport rectangle (the viewport dimensions
  // and the scroll position) changed, e.g. the user scrolled the main frame or
  // the viewport dimensions themselves changed. Only invoked on the outermost
  // main frame.
  virtual void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect) {}

  // Called when an ad element's geometry changed. An empty `ad_rect` is used
  // to signal the removal of the element. Only invoked on the outermost main
  // frame.
  virtual void OnMainFrameAdRectangleChanged(int element_id,
                                             const gfx::Rect& ad_rect) {}

  // The specified request was satisfied from WebCore's memory cache.
  virtual void DidLoadResourceFromMemoryCache(const WebURLRequest& request,
                                              const WebURLResponse& response) {}

  // Retrieves the WebLocalFrame that is being observed. Can be null.
  WebLocalFrame* GetWebLocalFrame() const;

 protected:
  friend class WebLocalFrameImpl;
  explicit WebLocalFrameMetricsObserver(WebLocalFrame* web_local_frame);
  ~WebLocalFrameMetricsObserver() override;

 private:
  // Called when `web_local_frame_` was detached.
  void WebLocalFrameDetached();

  // Sets `WebLocalFrame` to track.
  // Removes itself from previous (if any) `web_local_frame_` observer list and
  // adds to the new `web_local_frame`.
  void Observe(WebLocalFrameImpl* web_local_frame);

  WebPrivatePtrForGC<WebLocalFrameImpl,
                     WebPrivatePtrDestruction::kSameThread,
                     WebPrivatePtrStrength::kWeak>
      web_local_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_METRICS_OBSERVER_H_
