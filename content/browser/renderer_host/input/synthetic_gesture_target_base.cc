// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"

#include "components/input/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/events_helper.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event.h"
#include "ui/latency/latency_info.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebGestureEvent;

namespace content {
namespace {

// This value was determined experimentally. It was sufficient to not cause a
// fling on Android and Aura.
const int kPointerAssumedStoppedTimeMs = 100;

}  // namespace

SyntheticGestureTargetBase::SyntheticGestureTargetBase(
    RenderWidgetHostImpl* host)
    : host_(host) {
  DCHECK(host);
}

SyntheticGestureTargetBase::~SyntheticGestureTargetBase() {
}

void SyntheticGestureTargetBase::DispatchInputEventToPlatform(
    const WebInputEvent& event) {
  TRACE_EVENT1("input", "SyntheticGestureTarget::DispatchInputEventToPlatform",
               "type", WebInputEvent::GetName(event.GetType()));

  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);

  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& web_touch =
        static_cast<const WebTouchEvent&>(event);

    // Check that all touch pointers are within the content bounds.
    for (unsigned i = 0; i < web_touch.touches_length; i++) {
      if (web_touch.touches[i].state == WebTouchPoint::State::kStatePressed &&
          !PointIsWithinContents(web_touch.touches[i].PositionInWidget())) {
        LOG(WARNING)
            << "Touch coordinates are not within content bounds on TouchStart.";
        return;
      }
    }
    DispatchWebTouchEventToPlatform(web_touch, latency_info);
  } else if (event.GetType() == WebInputEvent::Type::kMouseWheel) {
    const WebMouseWheelEvent& web_wheel =
        static_cast<const WebMouseWheelEvent&>(event);
    if (!PointIsWithinContents(web_wheel.PositionInWidget())) {
      LOG(WARNING) << "Mouse wheel position is not within content bounds.";
      return;
    }
    if (web_wheel.delta_units != ui::ScrollGranularity::kScrollByPercentage)
      DispatchWebMouseWheelEventToPlatform(web_wheel, latency_info);
    else {
      // Percentage-based mouse wheel scrolls are implemented in the UI layer by
      // converting a native event's wheel tick amount to a percentage and
      // setting that directly on WebMouseWheelEvent (i.e. it does not read the
      // ui::MouseWheelEvent). However, when dispatching a synthetic
      // ui::MouseWheelEvent, the created WebMouseWheelEvent will copy values
      // from the ui::MouseWheelEvent. ui::MouseWheelEvent does
      // not have a float value for delta, so that codepath ends up truncating.
      // So instead, dispatch the WebMouseWheelEvent directly through the
      // RenderWidgetHostInputEventRouter attached to the RenderWidgetHostImpl.

      DCHECK(host_->delegate());
      DCHECK(host_->delegate()->IsWidgetForPrimaryMainFrame(host_));
      DCHECK(host_->delegate()->GetInputEventRouter());

      std::unique_ptr<WebInputEvent> wheel_evt_ptr = web_wheel.Clone();
      host_->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
          host_->GetView(),
          static_cast<WebMouseWheelEvent*>(wheel_evt_ptr.get()), latency_info);
    }
  } else if (WebInputEvent::IsMouseEventType(event.GetType())) {
    const WebMouseEvent& web_mouse =
        static_cast<const WebMouseEvent&>(event);
    if (event.GetType() == WebInputEvent::Type::kMouseDown &&
        !PointIsWithinContents(web_mouse.PositionInWidget())) {
      LOG(WARNING)
          << "Mouse pointer is not within content bounds on MouseDown.";
      return;
    }
    DispatchWebMouseEventToPlatform(web_mouse, latency_info);
  } else if (WebInputEvent::IsPinchGestureEventType(event.GetType())) {
    const WebGestureEvent& web_pinch =
        static_cast<const WebGestureEvent&>(event);
    // Touchscreen pinches should be injected as touch events.
    DCHECK_EQ(blink::WebGestureDevice::kTouchpad, web_pinch.SourceDevice());
    if (event.GetType() == WebInputEvent::Type::kGesturePinchBegin &&
        !PointIsWithinContents(web_pinch.PositionInWidget())) {
      LOG(WARNING)
          << "Pinch coordinates are not within content bounds on PinchBegin.";
      return;
    }
    DispatchWebGestureEventToPlatform(web_pinch, latency_info);
  } else if (WebInputEvent::IsFlingGestureEventType(event.GetType())) {
    const WebGestureEvent& web_fling =
        static_cast<const WebGestureEvent&>(event);
    // Touchscreen swipe should be injected as touch events.
    DCHECK_EQ(blink::WebGestureDevice::kTouchpad, web_fling.SourceDevice());
    if (event.GetType() == WebInputEvent::Type::kGestureFlingStart &&
        !PointIsWithinContents(web_fling.PositionInWidget())) {
      LOG(WARNING)
          << "Fling coordinates are not within content bounds on FlingStart.";
      return;
    }
    DispatchWebGestureEventToPlatform(web_fling, latency_info);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void SyntheticGestureTargetBase::GetVSyncParameters(
    base::TimeTicks& timebase,
    base::TimeDelta& interval) const {
  timebase = base::TimeTicks();
  interval = base::Microseconds(16667);
}

base::TimeDelta SyntheticGestureTargetBase::PointerAssumedStoppedTime()
    const {
  return base::Milliseconds(kPointerAssumedStoppedTimeMs);
}

float SyntheticGestureTargetBase::GetSpanSlopInDips() const {
  // * 2 because span is the distance between two touch points in a pinch-zoom
  // gesture so we're accounting for movement in two points.
  return 2.f * GetTouchSlopInDips();
}

int SyntheticGestureTargetBase::GetMouseWheelMinimumGranularity() const {
  return host_->GetView()->GetMouseWheelMinimumGranularity();
}

void SyntheticGestureTargetBase::WaitForTargetAck(
    SyntheticGestureParams::GestureType type,
    content::mojom::GestureSourceType source,
    base::OnceClosure callback) const {
  host_->WaitForInputProcessed(type, source, std::move(callback));
}

bool SyntheticGestureTargetBase::PointIsWithinContents(
    gfx::PointF point) const {
  gfx::Rect bounds = host_->GetView()->GetViewBounds();
  bounds -= bounds.OffsetFromOrigin();  // Translate the bounds to (0,0).
  return bounds.Contains(point.x(), point.y());
}

}  // namespace content
