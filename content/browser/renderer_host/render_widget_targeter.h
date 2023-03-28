// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_

#include <queue>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

// TODO(sunxd): Make |RenderWidgetTargetResult| a class. Merge the booleans into
// a mask to reduce the size. Make the constructor take in enums for better
// readability.
struct CONTENT_EXPORT RenderWidgetTargetResult {
  RenderWidgetTargetResult();
  RenderWidgetTargetResult(const RenderWidgetTargetResult&);
  RenderWidgetTargetResult(RenderWidgetHostViewBase* view,
                           bool should_query_view,
                           absl::optional<gfx::PointF> location,
                           bool latched_target);
  ~RenderWidgetTargetResult();

  raw_ptr<RenderWidgetHostViewBase, DanglingUntriaged> view = nullptr;
  bool should_query_view = false;
  absl::optional<gfx::PointF> target_location = absl::nullopt;
  // When |latched_target| is false, we explicitly hit-tested events instead of
  // using a known target.
  bool latched_target = false;
};

class RenderWidgetTargeter {
 public:
  using RenderWidgetHostAtPointCallback =
      base::OnceCallback<void(base::WeakPtr<RenderWidgetHostViewBase>,
                              absl::optional<gfx::PointF>)>;

  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual RenderWidgetTargetResult FindTargetSynchronouslyAtPoint(
        RenderWidgetHostViewBase* root_view,
        const gfx::PointF& location) = 0;

    virtual RenderWidgetTargetResult FindTargetSynchronously(
        RenderWidgetHostViewBase* root_view,
        const blink::WebInputEvent& event) = 0;

    // |event| must be non-null, and is in |root_view|'s coordinate space.
    virtual void DispatchEventToTarget(
        RenderWidgetHostViewBase* root_view,
        RenderWidgetHostViewBase* target,
        blink::WebInputEvent* event,
        const ui::LatencyInfo& latency,
        const absl::optional<gfx::PointF>& target_location) = 0;

    virtual void SetEventsBeingFlushed(bool events_being_flushed) = 0;

    virtual RenderWidgetHostViewBase* FindViewFromFrameSinkId(
        const viz::FrameSinkId& frame_sink_id) const = 0;

    // Returns true if a further asynchronous query should be sent to the
    // candidate RenderWidgetHostView.
    virtual bool ShouldContinueHitTesting(
        RenderWidgetHostViewBase* target_view) const = 0;
  };

  enum class HitTestResultsMatch {
    kDoNotMatch = 0,
    kMatch = 1,
    kHitTestResultChanged = 2,
    kHitTestDataOutdated = 3,
    kMaxValue = kHitTestDataOutdated,
  };

  // The delegate must outlive this targeter.
  explicit RenderWidgetTargeter(Delegate* delegate);

  RenderWidgetTargeter(const RenderWidgetTargeter&) = delete;
  RenderWidgetTargeter& operator=(const RenderWidgetTargeter&) = delete;

  ~RenderWidgetTargeter();

  // Finds the appropriate target inside |root_view| for |event|, and dispatches
  // it through the delegate. |event| is in the coord-space of |root_view|.
  void FindTargetAndDispatch(RenderWidgetHostViewBase* root_view,
                             const blink::WebInputEvent& event,
                             const ui::LatencyInfo& latency);

  // Finds the appropriate target inside |root_view| for |point|, and passes the
  // target along with the transformed coordinates of the point with respect to
  // the target's coordinates.
  void FindTargetAndCallback(RenderWidgetHostViewBase* root_view,
                             const gfx::PointF& point,
                             RenderWidgetHostAtPointCallback callback);

  void ViewWillBeDestroyed(RenderWidgetHostViewBase* view);

  bool HasEventsPendingDispatch() const;

  void set_async_hit_test_timeout_delay_for_testing(
      const base::TimeDelta& delay) {
    async_hit_test_timeout_delay_ = delay;
  }

  size_t num_requests_in_queue_for_testing() { return requests_.size(); }
  bool is_request_in_flight_for_testing() {
    return request_in_flight_.has_value();
  }

  void SetIsAutoScrollInProgress(bool autoscroll_in_progress);

  bool is_auto_scroll_in_progress() const { return is_autoscroll_in_progress_; }

 private:
  class TargetingRequest {
   public:
    TargetingRequest(base::WeakPtr<RenderWidgetHostViewBase>,
                     const blink::WebInputEvent&,
                     const ui::LatencyInfo&);
    TargetingRequest(base::WeakPtr<RenderWidgetHostViewBase>,
                     const gfx::PointF&,
                     RenderWidgetHostAtPointCallback);
    TargetingRequest(TargetingRequest&& request);
    TargetingRequest& operator=(TargetingRequest&& other);

    TargetingRequest(const TargetingRequest&) = delete;
    TargetingRequest& operator=(const TargetingRequest&) = delete;

    ~TargetingRequest();

    void RunCallback(RenderWidgetHostViewBase* target,
                     absl::optional<gfx::PointF> point);

    bool MergeEventIfPossible(const blink::WebInputEvent& event);
    bool IsWebInputEventRequest() const;
    blink::WebInputEvent* GetEvent();
    RenderWidgetHostViewBase* GetRootView() const;
    gfx::PointF GetLocation() const;
    const ui::LatencyInfo& GetLatency() const;

   private:
    base::WeakPtr<RenderWidgetHostViewBase> root_view;

    RenderWidgetHostAtPointCallback callback;

    // |location| is in the coordinate space of |root_view| which is
    // either set directly when event is null or calculated from the event.
    gfx::PointF location;
    // |event| if set is in the coordinate space of |root_view|.
    ui::WebScopedInputEvent event;
    ui::LatencyInfo latency;
  };

  void ResolveTargetingRequest(TargetingRequest);

  // Attempts to target and dispatch all events in the queue. It stops if it has
  // to query a client, or if the queue becomes empty.
  void FlushEventQueue();

  // Queries |target| to find the correct target for |request|.
  // |target_location| is the location in |target|'s coordinate space.
  // |last_request_target| and |last_target_location| provide a fallback target
  // in the case that the query times out. These should be null values when
  // querying the root view, and the target's immediate parent view otherwise.
  void QueryClient(RenderWidgetHostViewBase* target,
                   const gfx::PointF& target_location,
                   RenderWidgetHostViewBase* last_request_target,
                   const gfx::PointF& last_target_location,
                   TargetingRequest request);

  // |target_location|, if
  // set, is the location in |target|'s coordinate space.
  // |target| is the current target that will be queried using its
  // InputTargetClient interface.
  // |frame_sink_id| is returned from the InputTargetClient to indicate where
  // the event should be routed, and |transformed_location| is the point in
  // that new target's coordinate space.
  void FoundFrameSinkId(base::WeakPtr<RenderWidgetHostViewBase> target,
                        uint32_t request_id,
                        const gfx::PointF& target_location,
                        const viz::FrameSinkId& frame_sink_id,
                        const gfx::PointF& transformed_location);

  // |target_location|, if
  // set, is the location in |target|'s coordinate space.
  void FoundTarget(RenderWidgetHostViewBase* target,
                   const absl::optional<gfx::PointF>& target_location,
                   TargetingRequest* request);

  // Callback when the hit testing timer fires, to resume event processing
  // without further waiting for a response to the last targeting request.
  void AsyncHitTestTimedOut(
      base::WeakPtr<RenderWidgetHostViewBase> current_request_target,
      const gfx::PointF& current_target_location,
      base::WeakPtr<RenderWidgetHostViewBase> last_request_target,
      const gfx::PointF& last_target_location);

  void OnInputTargetDisconnect(base::WeakPtr<RenderWidgetHostViewBase> target,
                               const gfx::PointF& location);

  HitTestResultsMatch GetHitTestResultsMatchBucket(
      RenderWidgetHostViewBase* target,
      TargetingRequest* request) const;

  base::TimeDelta async_hit_test_timeout_delay() {
    return async_hit_test_timeout_delay_;
  }

  absl::optional<TargetingRequest> request_in_flight_;
  uint32_t last_request_id_ = 0;
  std::queue<TargetingRequest> requests_;

  std::unordered_set<RenderWidgetHostViewBase*> unresponsive_views_;

  // Target to send events to if autoscroll is in progress
  RenderWidgetTargetResult middle_click_result_;

  // True when the user middle click's mouse for autoscroll
  bool is_autoscroll_in_progress_ = false;

  // This value limits how long to wait for a response from the renderer
  // process before giving up and resuming event processing.
  base::TimeDelta async_hit_test_timeout_delay_;

  base::OneShotTimer async_hit_test_timeout_;

  uint64_t trace_id_;

  const raw_ptr<Delegate, DanglingUntriaged> delegate_;
  base::WeakPtrFactory<RenderWidgetTargeter> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
