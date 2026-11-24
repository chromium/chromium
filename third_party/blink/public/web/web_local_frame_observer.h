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

namespace blink {
class WebFormElement;
class WebLocalFrame;
class WebLocalFrameImpl;
class WebString;
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

  // A performance timing event (e.g. first paint) occurred.
  virtual void DidChangePerformanceTiming() {}

  // Blink exhibited a certain loading behavior that the browser process will
  // use for segregated histograms.
  virtual void DidObserveLoadingBehavior(LoadingBehaviorFlag behavior) {}

  // A new soft navigation was observed.
  virtual void DidObserveSoftNavigation(
      SoftNavigationMetricsForReporting metrics) {}

  // A new message was added to the console. `stack_trace` is only populated
  // when detailed messages are requested for `source_name` (e.g. via
  // Platform::ShouldReportDetailedMessageForSource()).
  virtual void DidAddMessageToConsole(const WebConsoleMessage& message,
                                      const WebString& source_name,
                                      unsigned source_line,
                                      const WebString& stack_trace) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // Called when script in the page calls window.print().
  virtual void ScriptedPrint(bool user_initiated) {}

  // The frame's document and all of its subresources succeeded to load for
  // printing.
  virtual void DidFinishLoadForPrinting() {}

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
