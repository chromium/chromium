// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_

#include <queue>
#include <unordered_set>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_export.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace gfx {
class PointF;
}

namespace content {

class RenderWidgetHostViewBase;
class OneShotTimeoutMonitor;

// TODO(sunxd): Make |RenderWidgetTargetResult| a class. Merge the booleans into
// a mask to reduce the size. Make the constructor take in enums for better
// readability.
struct CONTENT_EXPORT RenderWidgetTargetResult {
  RenderWidgetTargetResult();
  RenderWidgetTargetResult(const RenderWidgetTargetResult&);
  RenderWidgetTargetResult(RenderWidgetHostViewBase* view,
                           bool should_query_view,
                           base::Optional<gfx::PointF> location,
                           bool latched_target,
                           bool should_verify_result);
  ~RenderWidgetTargetResult();

  RenderWidgetHostViewBase* view = nullptr;
  bool should_query_view = false;
  base::Optional<gfx::PointF> target_location = base::nullopt;
  // When |latched_target| is false, we explicitly hit-tested events instead of
  // using a known target.
  bool latched_target = false;
  // When |should_verify_result| is true, RenderWidgetTargeter will do async hit
  // testing and compare the target with the result of synchronous hit testing.
  // |should_verify_result| will always be false if we are doing draw quad based
  // hit testing.
  bool should_verify_result = false;
};

class TracingUmaTracker;

class RenderWidgetTargeter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual RenderWidgetTargetResult FindTargetSynchronously(
        RenderWidgetHostViewBase* root_view,
        const blink::WebInputEvent& event) = 0;

    // |event| is in |root_view|'s coordinate space.
    virtual void DispatchEventToTarget(
        RenderWidgetHostViewBase* root_view,
        RenderWidgetHostViewBase* target,
        const blink::WebInputEvent& event,
        const ui::LatencyInfo& latency,
        const base::Optional<gfx::PointF>& target_location) = 0;

    virtual void SetEventsBeingFlushed(bool events_being_flushed) = 0;

    virtual RenderWidgetHostViewBase* FindViewFromFrameSinkId(
        const viz::FrameSinkId& frame_sink_id) const = 0;
  };

  // The delegate must outlive this targeter.
  explicit RenderWidgetTargeter(Delegate* delegate);
  ~RenderWidgetTargeter();

  // Finds the appropriate target inside |root_view| for |event|, and dispatches
  // it through the delegate. |event| is in the coord-space of |root_view|.
  void FindTargetAndDispatch(RenderWidgetHostViewBase* root_view,
                             const blink::WebInputEvent& event,
                             const ui::LatencyInfo& latency);

  void ViewWillBeDestroyed(RenderWidgetHostViewBase* view);

  void set_async_hit_test_timeout_delay_for_testing(
      const base::TimeDelta& delay) {
    async_hit_test_timeout_delay_ = delay;
  }

  unsigned num_requests_in_queue_for_testing() { return requests_.size(); }
  bool is_request_in_flight_for_testing() { return request_in_flight_; }

 private:
  // Attempts to target and dispatch all events in the queue. It stops if it has
  // to query a client, or if the queue becomes empty.
  void FlushEventQueue(bool is_verifying);

  // Queries |target| to find the correct target for |event|.
  // |event| is in the coordinate space of |root_view|.
  // |target_location|, if set, is the location in |target|'s coordinate space.
  // |last_request_target| and |last_target_location| provide a fallback target
  // the case that the query times out. These should be null values when
  // querying the root view, and the target's immediate parent view otherwise.
  // |expected_frame_sink_id| is temporarily added for v2 viz hit testing.
  // V2 uses cc generated hit test data and we need to verify its correctness.
  // The variable is the target frame sink id v2 finds in synchronous hit
  // testing. It should be the same as the async hit testing target if v2 works
  // correctly.
  // TODO(sunxd): Remove |expected_frame_sink_id| after verifying synchronous
  // hit testing correctness. See https://crbug.com/871996.
  void QueryClientInternal(RenderWidgetHostViewBase* root_view,
                           RenderWidgetHostViewBase* target,
                           const blink::WebInputEvent& event,
                           const ui::LatencyInfo& latency,
                           const gfx::PointF& target_location,
                           RenderWidgetHostViewBase* last_request_target,
                           const gfx::PointF& last_target_location,
                           const viz::FrameSinkId& expected_frame_sink_id);

  void QueryClient(RenderWidgetHostViewBase* root_view,
                   RenderWidgetHostViewBase* target,
                   const blink::WebInputEvent& event,
                   const ui::LatencyInfo& latency,
                   const gfx::PointF& target_location,
                   RenderWidgetHostViewBase* last_request_target,
                   const gfx::PointF& last_target_location);

  void QueryAndVerifyClient(RenderWidgetHostViewBase* root_view,
                            RenderWidgetHostViewBase* target,
                            const blink::WebInputEvent& event,
                            const ui::LatencyInfo& latency,
                            const gfx::PointF& target_location,
                            RenderWidgetHostViewBase* last_request_target,
                            const gfx::PointF& last_target_location,
                            const viz::FrameSinkId& expected_frame_sink_id);

  // |event| is in the coordinate space of |root_view|. |target_location|, if
  // set, is the location in |target|'s coordinate space.
  // |target| is the current target that will be queried using its
  // InputTargetClient interface.
  // |frame_sink_id| is returned from the InputTargetClient to indicate where
  // the event should be routed, and |transformed_location| is the point in
  // that new target's coordinate space.
  // |expected_frame_sink_id| is the expected hit test result based on
  // synchronous event targeting with cc generated data.
  void FoundFrameSinkId(base::WeakPtr<RenderWidgetHostViewBase> root_view,
                        base::WeakPtr<RenderWidgetHostViewBase> target,
                        ui::WebScopedInputEvent event,
                        const ui::LatencyInfo& latency,
                        uint32_t request_id,
                        const gfx::PointF& target_location,
                        TracingUmaTracker tracker,
                        const viz::FrameSinkId& expected_frame_sink_id,
                        const viz::FrameSinkId& frame_sink_id,
                        const gfx::PointF& transformed_location);

  // |event| is in the coordinate space of |root_view|. |target_location|, if
  // set, is the location in |target|'s coordinate space. If |latched_target| is
  // false, we explicitly did hit-testing for this event, instead of using a
  // known target.
  void FoundTarget(RenderWidgetHostViewBase* root_view,
                   RenderWidgetHostViewBase* target,
                   const blink::WebInputEvent& event,
                   const ui::LatencyInfo& latency,
                   const base::Optional<gfx::PointF>& target_location,
                   bool latched_target,
                   const viz::FrameSinkId& expected_frame_sink_id);

  // Callback when the hit testing timer fires, to resume event processing
  // without further waiting for a response to the last targeting request.
  void AsyncHitTestTimedOut(
      base::WeakPtr<RenderWidgetHostViewBase> current_request_root_view,
      base::WeakPtr<RenderWidgetHostViewBase> current_request_target,
      const gfx::PointF& current_target_location,
      base::WeakPtr<RenderWidgetHostViewBase> last_request_target,
      const gfx::PointF& last_target_location,
      ui::WebScopedInputEvent event,
      const ui::LatencyInfo& latency,
      const viz::FrameSinkId& expected_frame_sink_id);

  base::TimeDelta async_hit_test_timeout_delay() {
    return async_hit_test_timeout_delay_;
  }

  struct TargetingRequest {
    TargetingRequest();
    TargetingRequest(TargetingRequest&& request);
    TargetingRequest& operator=(TargetingRequest&& other);
    ~TargetingRequest();

    base::WeakPtr<RenderWidgetHostViewBase> root_view;
    ui::WebScopedInputEvent event;
    ui::LatencyInfo latency;
    viz::FrameSinkId expected_frame_sink_id;
    std::unique_ptr<TracingUmaTracker> tracker;
  };

  bool request_in_flight_ = false;
  uint32_t last_request_id_ = 0;
  std::queue<TargetingRequest> requests_;

  // With viz-hit-testing-surface-layer being enabled, we do async hit testing
  // for already dispatched events for verification. These verification requests
  // should not block normal hit testing requests.
  bool verify_request_in_flight_ = false;
  uint32_t last_verify_request_id_ = 0;
  std::queue<TargetingRequest> verify_requests_;

  std::unordered_set<RenderWidgetHostViewBase*> unresponsive_views_;

  // This value keeps track of the number of clients we have asked in order to
  // do async hit-testing.
  uint32_t async_depth_ = 0;

  // This value limits how long to wait for a response from the renderer
  // process before giving up and resuming event processing.
  base::TimeDelta async_hit_test_timeout_delay_ =
      base::TimeDelta::FromMilliseconds(kAsyncHitTestTimeoutMs);

  std::unique_ptr<OneShotTimeoutMonitor> async_hit_test_timeout_;
  std::unique_ptr<OneShotTimeoutMonitor> async_verify_hit_test_timeout_;

  uint64_t trace_id_;

  Delegate* const delegate_;
  base::WeakPtrFactory<RenderWidgetTargeter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetTargeter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
