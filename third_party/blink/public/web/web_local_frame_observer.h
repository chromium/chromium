// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_OBSERVER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_OBSERVER_H_

#include <stdint.h>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace blink {
class WebFormElement;
class WebLocalFrame;
class WebLocalFrameImpl;
class WebString;
struct JavaScriptFrameworkDetectionResult;
struct WebConsoleMessage;

enum class BFCacheStateChange {
  kStoredToBFCache,
  kRestoredFromBFCache,
  kNoChange,
};

// Base class for objects that want to get notified of changes to the local
// frame.
class BLINK_EXPORT WebLocalFrameObserver : public base::CheckedObserver {
 public:
  // A subclass can use this to delete itself.
  virtual void OnFrameDetached() = 0;

  // A form submission has been requested, but the page's submit event handler
  // hasn't yet had a chance to run (and possibly alter/interrupt the submit.)
  virtual void WillSendSubmitEvent(const WebFormElement&) {}

  // Notification that the frame was hidden/shown.
  virtual void WasHidden() {}
  virtual void WasShown() {}

  // Called when a frame's page lifecycle state gets updated.
  virtual void DidSetPageLifecycleState(BFCacheStateChange bfcache_change) {}

  // Called when an ad element's geometry changed. An empty `ad_rect` is used to
  // signal the removal of the element. Only invoked on the outermost main
  // frame.
  virtual void OnMainFrameAdRectangleChanged(int element_id,
                                             const gfx::Rect& ad_rect) {}

  // A performance timing event (e.g. first paint) occurred.
  virtual void DidChangePerformanceTiming() {}

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

  // Blink exhibited a certain loading behavior that the browser process will
  // use for segregated histograms.
  virtual void DidObserveLoadingBehavior(LoadingBehaviorFlag behavior) {}

  // Blink detected a JavaScript framework that the browser process will use for
  // UKM.
  virtual void DidObserveJavaScriptFrameworks(
      const JavaScriptFrameworkDetectionResult&) {}

  // A new soft navigation was observed.
  virtual void DidObserveSoftNavigation(
      SoftNavigationMetricsForReporting metrics) {}

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

  // A new message was added to the console. `stack_trace` is only populated
  // when detailed messages are requested for `source_name` (e.g. via
  // Platform::ShouldReportDetailedMessageForSource()).
  virtual void DidAddMessageToConsole(const WebConsoleMessage& message,
                                      const WebString& source_name,
                                      unsigned source_line,
                                      const WebString& stack_trace) {}

  // Retrieves the WebLocalFrame that is being observed. Can be null.
  WebLocalFrame* GetWebLocalFrame() const;

 protected:
  friend class WebLocalFrameImpl;
  explicit WebLocalFrameObserver(WebLocalFrame* web_local_frame);
  ~WebLocalFrameObserver() override;

 private:
  // Called when `web_local_frame_` was detached.
  void WebLocalFrameDetached();

  // Sets `WebLocalFrame` to track.
  // Removes itself of previous (if any) `web_local_frame_` observer list and
  // adds to the new `web_local_frame`.
  void Observe(WebLocalFrameImpl* web_local_frame);

  WebPrivatePtrForGC<WebLocalFrameImpl,
                     WebPrivatePtrDestruction::kSameThread,
                     WebPrivatePtrStrength::kWeak>
      web_local_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_LOCAL_FRAME_OBSERVER_H_
