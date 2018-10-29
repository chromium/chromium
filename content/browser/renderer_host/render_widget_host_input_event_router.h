// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_

#include <stdint.h>

#include <map>
#include <unordered_map>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "components/viz/service/surfaces/surface_hittest_delegate.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/browser/renderer_host/render_widget_host_view_base_observer.h"
#include "content/browser/renderer_host/render_widget_targeter.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_state.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/transform.h"

struct FrameHostMsg_HittestData_Params;

namespace blink {
class WebGestureEvent;
class WebInputEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;
}

namespace gfx {
class Point;
class PointF;
}

namespace ui {
class LatencyInfo;
}

namespace viz {
class HostFrameSinkManager;
}

namespace content {

class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class RenderWidgetTargeter;
class TouchEmulator;
class TouchEventAckQueue;

// Helper method also used from hit_test_debug_key_event_observer.cc
viz::HitTestQuery* GetHitTestQuery(
    viz::HostFrameSinkManager* host_frame_sink_manager,
    const viz::FrameSinkId& frame_sink_id);

// Class owned by WebContentsImpl for the purpose of directing input events
// to the correct RenderWidgetHost on pages with multiple RenderWidgetHosts.
// It maintains a mapping of RenderWidgetHostViews to Surface IDs that they
// own. When an input event requires routing based on window coordinates,
// this class requests a Surface hit test from the provided |root_view| and
// forwards the event to the owning RWHV of the returned Surface ID.
class CONTENT_EXPORT RenderWidgetHostInputEventRouter
    : public RenderWidgetHostViewBaseObserver,
      public RenderWidgetTargeter::Delegate,
      public TouchEmulatorClient {
 public:
  RenderWidgetHostInputEventRouter();
  ~RenderWidgetHostInputEventRouter() final;

  void OnRenderWidgetHostViewBaseDestroyed(
      RenderWidgetHostViewBase* view) override;

  void RouteMouseEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebMouseEvent* event,
                       const ui::LatencyInfo& latency);
  void RouteMouseWheelEvent(RenderWidgetHostViewBase* root_view,
                            blink::WebMouseWheelEvent* event,
                            const ui::LatencyInfo& latency);
  void RouteGestureEvent(RenderWidgetHostViewBase* root_view,
                         const blink::WebGestureEvent* event,
                         const ui::LatencyInfo& latency);
  void OnHandledTouchStartOrFirstTouchMove(uint32_t unique_touch_event_id);
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& event,
                              InputEventAckState ack_result,
                              RenderWidgetHostViewBase* view);
  void RouteTouchEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebTouchEvent *event,
                       const ui::LatencyInfo& latency);

  // |event| is in root coordinates.
  void BubbleScrollEvent(RenderWidgetHostViewBase* target_view,
                         const blink::WebGestureEvent& event,
                         const RenderWidgetHostViewBase* resending_view);
  void CancelScrollBubbling(RenderWidgetHostViewBase* target_view);

  void AddFrameSinkIdOwner(const viz::FrameSinkId& id,
                           RenderWidgetHostViewBase* owner);
  void RemoveFrameSinkIdOwner(const viz::FrameSinkId& id);

  bool is_registered(const viz::FrameSinkId& id) {
    return owner_map_.find(id) != owner_map_.end();
  }

  void OnHittestData(const FrameHostMsg_HittestData_Params& params);

  TouchEmulator* GetTouchEmulator();
  // Since GetTouchEmulator will lazily create a touch emulator, the following
  // accessor allows testing for its existence without causing it to be created.
  bool has_touch_emulator() const { return touch_emulator_.get(); }

  // Returns the RenderWidgetHostImpl inside the |root_view| at |point| where
  // |point| is with respect to |root_view|'s coordinates. If a RWHI is found,
  // the value of |transformed_point| is the coordinate of the point with
  // respect to the RWHI's coordinates. If |root_view| is nullptr, this method
  // will return nullptr and will not modify |transformed_point|.
  RenderWidgetHostImpl* GetRenderWidgetHostAtPoint(
      RenderWidgetHostViewBase* root_view,
      const gfx::PointF& point,
      gfx::PointF* transformed_point);

  // RenderWidgetTargeter::Delegate:
  RenderWidgetHostViewBase* FindViewFromFrameSinkId(
      const viz::FrameSinkId& frame_sink_id) const override;

  // Allows a target to claim or release capture of mouse events.
  void SetMouseCaptureTarget(RenderWidgetHostViewBase* target,
                             bool captures_dragging);
  RenderWidgetHostImpl* GetMouseCaptureWidgetForTests() const;

  std::vector<RenderWidgetHostView*> GetRenderWidgetHostViewsForTests() const;
  RenderWidgetTargeter* GetRenderWidgetTargeterForTests();

  // TouchEmulatorClient:
  void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& event) override;
  void ForwardEmulatedTouchEvent(const blink::WebTouchEvent& event,
                                 RenderWidgetHostViewBase* target) override;
  void SetCursor(const WebCursor& cursor) override;
  void ShowContextMenuAtPoint(const gfx::Point& point,
                              const ui::MenuSourceType source_type) override;

 private:
  struct HittestData {
    bool ignored_for_hittest;
  };

  class HittestDelegate : public viz::SurfaceHittestDelegate {
   public:
    HittestDelegate(const std::unordered_map<viz::SurfaceId,
                                             HittestData,
                                             viz::SurfaceIdHash>& hittest_data);
    bool RejectHitTarget(const viz::SurfaceDrawQuad* surface_quad,
                         const gfx::Point& point_in_quad_space) override;
    bool AcceptHitTarget(const viz::SurfaceDrawQuad* surface_quad,
                         const gfx::Point& point_in_quad_space) override;

    const std::unordered_map<viz::SurfaceId, HittestData, viz::SurfaceIdHash>&
        hittest_data_;
  };

  using FrameSinkIdOwnerMap = std::unordered_map<viz::FrameSinkId,
                                                 RenderWidgetHostViewBase*,
                                                 viz::FrameSinkIdHash>;
  struct TargetData {
    RenderWidgetHostViewBase* target;
    gfx::Vector2dF delta;
    gfx::Transform transform;

    TargetData() : target(nullptr) {}
  };
  using TargetMap = std::map<uint32_t, TargetData>;

  void ClearAllObserverRegistrations();
  RenderWidgetTargetResult FindViewAtLocation(
      RenderWidgetHostViewBase* root_view,
      const gfx::PointF& point,
      const gfx::PointF& point_in_screen,
      viz::EventSource source,
      gfx::PointF* transformed_point) const;

  bool IsViewInMap(const RenderWidgetHostViewBase* view) const;
  void RouteTouchscreenGestureEvent(RenderWidgetHostViewBase* root_view,
                                    const blink::WebGestureEvent* event,
                                    const ui::LatencyInfo& latency);

  RenderWidgetTargetResult FindTouchpadGestureEventTarget(
      RenderWidgetHostViewBase* root_view,
      const blink::WebGestureEvent& event) const;
  void RouteTouchpadGestureEvent(RenderWidgetHostViewBase* root_view,
                                 const blink::WebGestureEvent* event,
                                 const ui::LatencyInfo& latency);
  void DispatchTouchpadGestureEvent(
      RenderWidgetHostViewBase* root_view,
      RenderWidgetHostViewBase* target,
      const blink::WebGestureEvent& touchpad_gesture_event,
      const ui::LatencyInfo& latency,
      const base::Optional<gfx::PointF>& target_location);

  // MouseMove/Enter/Leave events might need to be processed by multiple frames
  // in different processes for MouseEnter and MouseLeave event handlers to
  // properly fire. This method determines which RenderWidgetHostViews other
  // than the actual target require notification, and sends the appropriate
  // events to them. |event| should be in |root_view|'s coordinate space.
  void SendMouseEnterOrLeaveEvents(const blink::WebMouseEvent& event,
                                   RenderWidgetHostViewBase* target,
                                   RenderWidgetHostViewBase* root_view);

  // The following methods take a GestureScrollUpdate event and send a
  // GestureScrollBegin or GestureScrollEnd for wrapping it. This is needed
  // when GestureScrollUpdates are being forwarded for scroll bubbling.
  void SendGestureScrollBegin(RenderWidgetHostViewBase* view,
                              const blink::WebGestureEvent& event);
  void SendGestureScrollEnd(RenderWidgetHostViewBase* view,
                            const blink::WebGestureEvent& event);

  // Helper functions to implement RenderWidgetTargeter::Delegate functions.
  RenderWidgetTargetResult FindMouseEventTarget(
      RenderWidgetHostViewBase* root_view,
      const blink::WebMouseEvent& event) const;
  RenderWidgetTargetResult FindMouseWheelEventTarget(
      RenderWidgetHostViewBase* root_view,
      const blink::WebMouseWheelEvent& event) const;
  // Returns target for first TouchStart in a sequence, or a null target
  // otherwise.
  RenderWidgetTargetResult FindTouchEventTarget(
      RenderWidgetHostViewBase* root_view,
      const blink::WebTouchEvent& event);
  RenderWidgetTargetResult FindTouchscreenGestureEventTarget(
      RenderWidgetHostViewBase* root_view,
      const blink::WebGestureEvent& gesture_event);

  // |mouse_event| is in the coord-space of |root_view|.
  void DispatchMouseEvent(RenderWidgetHostViewBase* root_view,
                          RenderWidgetHostViewBase* target,
                          const blink::WebMouseEvent& mouse_event,
                          const ui::LatencyInfo& latency,
                          const base::Optional<gfx::PointF>& target_location);
  // |mouse_wheel_event| is in the coord-space of |root_view|.
  void DispatchMouseWheelEvent(
      RenderWidgetHostViewBase* root_view,
      RenderWidgetHostViewBase* target,
      const blink::WebMouseWheelEvent& mouse_wheel_event,
      const ui::LatencyInfo& latency,
      const base::Optional<gfx::PointF>& target_location);
  // Assumes |touch_event| has coordinates in the root view's coordinate space.
  void DispatchTouchEvent(RenderWidgetHostViewBase* root_view,
                          RenderWidgetHostViewBase* target,
                          const blink::WebTouchEvent& touch_event,
                          const ui::LatencyInfo& latency,
                          const base::Optional<gfx::PointF>& target_location,
                          bool is_emulated);
  // Assumes |gesture_event| has coordinates in root view's coordinate space.
  void DispatchTouchscreenGestureEvent(
      RenderWidgetHostViewBase* root_view,
      RenderWidgetHostViewBase* target,
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency,
      const base::Optional<gfx::PointF>& target_location);

  // Transforms |point| from |root_view| coord space to |target| coord space.
  // Result is stored in |transformed_point|. Returns true if the transform
  // is successful, false otherwise.
  bool TransformPointToTargetCoordSpace(RenderWidgetHostViewBase* root_view,
                                        RenderWidgetHostViewBase* target,
                                        const gfx::PointF& point,
                                        gfx::PointF* transformed_point,
                                        viz::EventSource source) const;

  // TODO(828422): Remove once this issue no longer occurs.
  void ReportBubblingScrollToSameView(const blink::WebGestureEvent& event,
                                      const RenderWidgetHostViewBase* view);

  // RenderWidgetTargeter::Delegate:
  RenderWidgetTargetResult FindTargetSynchronously(
      RenderWidgetHostViewBase* root_view,
      const blink::WebInputEvent& event) override;
  void DispatchEventToTarget(
      RenderWidgetHostViewBase* root_view,
      RenderWidgetHostViewBase* target,
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency,
      const base::Optional<gfx::PointF>& target_location) override;
  // Notify whether the events in the queue are being flushed due to touch ack
  // timeout, or the flushing has completed.
  void SetEventsBeingFlushed(bool events_being_flushed) override;

  FrameSinkIdOwnerMap owner_map_;
  TargetMap touchscreen_gesture_target_map_;
  TargetData touch_target_;
  TargetData touchscreen_gesture_target_;
  // The following variable is temporary, for diagnosis of
  // https://crbug.com/824774.
  bool touchscreen_gesture_target_in_map_;
  TargetData touchpad_gesture_target_;
  TargetData bubbling_gesture_scroll_target_;
  TargetData first_bubbling_scroll_target_;
  // Used to target wheel events for the duration of a scroll.
  TargetData wheel_target_;
  // Maintains the same target between mouse down and mouse up.
  TargetData mouse_capture_target_;

  // Tracked for the purpose of generating MouseEnter and MouseLeave events.
  RenderWidgetHostViewBase* last_mouse_move_target_;
  RenderWidgetHostViewBase* last_mouse_move_root_view_;

  // Tracked for the purpose of targeting subsequent fling cancel events.
  RenderWidgetHostViewBase* last_fling_start_target_ = nullptr;

  // During scroll bubbling we bubble the GFS to the target view so that its
  // fling controller takes care of flinging. In this case we should also send
  // the GFC to the bubbling target so that the fling controller currently in
  // charge of the fling progress could handle the fling cancellelation as well.
  RenderWidgetHostViewBase* last_fling_start_bubbled_target_ = nullptr;

  // Tracked for the purpose of providing a root_view when dispatching emulated
  // touch/gesture events.
  RenderWidgetHostViewBase* last_emulated_event_root_view_;

  // Used to send a GSE with proper source device to terminate scroll bubbling
  // whenever needed.
  blink::WebGestureDevice bubbling_gesture_scroll_source_device_;

  float last_device_scale_factor_;

  int active_touches_;
  // Keep track of when we are between GesturePinchBegin and GesturePinchEnd
  // inclusive, as we need to route these events (and anything in between) to
  // the main frame.
  bool in_touchscreen_gesture_pinch_;
  bool gesture_pinch_did_send_scroll_begin_;
  std::unordered_map<viz::SurfaceId, HittestData, viz::SurfaceIdHash>
      hittest_data_;

  std::unique_ptr<RenderWidgetTargeter> event_targeter_;
  bool use_viz_hit_test_ = false;
  bool events_being_flushed_ = false;

  std::unique_ptr<TouchEmulator> touch_emulator_;
  std::unique_ptr<TouchEventAckQueue> touch_event_ack_queue_;

  base::WeakPtrFactory<RenderWidgetHostInputEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostInputEventRouter);
  friend class RenderWidgetHostInputEventRouterTest;
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessHitTestBrowserTest,
                           HitTestStaleDataDeletedView);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessHitTestBrowserTest,
                           InputEventRouterGestureTargetMapTest);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessHitTestBrowserTest,
                           InputEventRouterGesturePreventDefaultTargetMapTest);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessHitTestBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessHitTestBrowserTest,
                           TouchpadPinchOverOOPIF);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessMouseWheelHitTestBrowserTest,
                           InputEventRouterWheelTargetTest);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessMacBrowserTest,
                           InputEventRouterTouchpadGestureTargetTest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
