// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture_target_mac.h"

#include "components/input/render_widget_host_input_event_router.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

// Unlike some event APIs, Apple does not provide a way to programmatically
// build a zoom event. To work around this, we leverage ObjectiveC's flexible
// typing and build up an object with the right interface to provide a zoom
// event.
@interface SyntheticPinchEvent : NSObject

// Populated based on desired zoom level.
@property CGFloat magnification;
@property NSPoint locationInWindow;
@property NSEventType type;
@property NSTimeInterval timestamp;
@property NSEventPhase phase;

// Filled with default values.
@property(readonly) CGFloat deltaX;
@property(readonly) CGFloat deltaY;
@property(readonly) NSEventModifierFlags modifierFlags;

@end

@implementation SyntheticPinchEvent

@synthesize magnification = _magnification;
@synthesize locationInWindow = _locationInWindow;
@synthesize type = _type;
@synthesize timestamp = _timestamp;
@synthesize phase = _phase;
@synthesize deltaX = _deltaX;
@synthesize deltaY = _deltaY;
@synthesize modifierFlags = _modifierFlags;

- (instancetype)initWithMagnification:(float)magnification
                     locationInWindow:(NSPoint)location
                            timestamp:(NSTimeInterval)timestamp {
  if (self = [super init]) {
    _type = NSEventTypeMagnify;
    _phase = NSEventPhaseChanged;
    _magnification = magnification;
    _locationInWindow = location;
    _timestamp = timestamp;

    _deltaX = 0;
    _deltaY = 0;
    _modifierFlags = 0;
  }

  return self;
}

+ (instancetype)eventWithMagnification:(float)magnification
                      locationInWindow:(NSPoint)location
                             timestamp:(NSTimeInterval)timestamp
                                 phase:(NSEventPhase)phase {
  SyntheticPinchEvent* event =
      [[SyntheticPinchEvent alloc] initWithMagnification:magnification
                                        locationInWindow:location
                                               timestamp:timestamp];
  event.phase = phase;
  return event;
}

@end

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace content {

SyntheticGestureTargetMac::SyntheticGestureTargetMac(
    RenderWidgetHostImpl* host,
    RenderWidgetHostViewCocoa* cocoa_view)
    : SyntheticGestureTargetBase(host), cocoa_view_(cocoa_view) {}

void SyntheticGestureTargetMac::DispatchWebGestureEventToPlatform(
    const WebGestureEvent& web_gesture,
    const ui::LatencyInfo& latency_info) {
  // Create an autorelease pool so that we clean up any synthetic events we
  // generate.
  @autoreleasepool {
    NSPoint content_local = NSMakePoint(
        web_gesture.PositionInWidget().x(),
        cocoa_view_.frame.size.height - web_gesture.PositionInWidget().y());
    NSPoint location_in_window = [cocoa_view_ convertPoint:content_local
                                                    toView:nil];
    NSTimeInterval timestamp =
        ui::EventTimeStampToSeconds(web_gesture.TimeStamp());

    switch (web_gesture.GetType()) {
      case WebInputEvent::Type::kGesturePinchBegin: {
        id cocoa_event =
            [SyntheticPinchEvent eventWithMagnification:0.0f
                                       locationInWindow:location_in_window
                                              timestamp:timestamp
                                                  phase:NSEventPhaseBegan];
        [cocoa_view_ handleBeginGestureWithEvent:cocoa_event
                         isSyntheticallyInjected:YES];
        return;
      }
      case WebInputEvent::Type::kGesturePinchEnd: {
        id cocoa_event =
            [SyntheticPinchEvent eventWithMagnification:0.0f
                                       locationInWindow:location_in_window
                                              timestamp:timestamp
                                                  phase:NSEventPhaseEnded];
        [cocoa_view_ handleEndGestureWithEvent:cocoa_event];
        return;
      }
      case WebInputEvent::Type::kGesturePinchUpdate: {
        id cocoa_event = [SyntheticPinchEvent
            eventWithMagnification:web_gesture.data.pinch_update.scale - 1.0f
                  locationInWindow:location_in_window
                         timestamp:timestamp
                             phase:NSEventPhaseChanged];
        [cocoa_view_ magnifyWithEvent:cocoa_event];
        return;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

void SyntheticGestureTargetMac::DispatchWebTouchEventToPlatform(
    const blink::WebTouchEvent& web_touch,
    const ui::LatencyInfo& latency_info) {
  GetView()->InjectTouchEvent(web_touch, latency_info);
}

void SyntheticGestureTargetMac::DispatchWebMouseWheelEventToPlatform(
    const blink::WebMouseWheelEvent& web_wheel,
    const ui::LatencyInfo& latency_info) {
  blink::WebMouseWheelEvent wheel_event = web_wheel;
  wheel_event.wheel_ticks_x =
      web_wheel.delta_x / ui::kScrollbarPixelsPerCocoaTick;
  wheel_event.wheel_ticks_y =
      web_wheel.delta_y / ui::kScrollbarPixelsPerCocoaTick;

  // Manually route the WebMouseWheelEvent to any open popup window if the
  // mouse is currently over the pop window, because window-level event routing
  // on Mac happens at the OS API level which we cannot easily inject the
  // events into.
  if (GetView()->PopupChildHostView() &&
      PointIsWithinContents(GetView()->PopupChildHostView(),
                            web_wheel.PositionInWidget())) {
    GetView()->PopupChildHostView()->RouteOrProcessWheelEvent(wheel_event);
  } else {
    GetView()->RouteOrProcessWheelEvent(wheel_event);
  }
  if (web_wheel.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
    // Send the pending wheel end event immediately. Otherwise, the
    // MouseWheelPhaseHandler will defer the end event in case of momentum
    // scrolling. We want the end event sent before resolving the completion
    // callback.
    GetView()->GetMouseWheelPhaseHandler()->DispatchPendingWheelEndEvent();
  }
}

void SyntheticGestureTargetMac::DispatchWebMouseEventToPlatform(
    const blink::WebMouseEvent& web_mouse,
    const ui::LatencyInfo& latency_info) {
  GetView()->RouteOrProcessMouseEvent(web_mouse);
}

content::mojom::GestureSourceType
SyntheticGestureTargetMac::GetDefaultSyntheticGestureSourceType() const {
  return content::mojom::GestureSourceType::kMouseInput;
}

float SyntheticGestureTargetMac::GetTouchSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()
      ->max_touch_move_in_pixels_for_click();
}

float SyntheticGestureTargetMac::GetSpanSlopInDips() const {
  return ui::GestureConfiguration::GetInstance()->span_slop();
}

float SyntheticGestureTargetMac::GetMinScalingSpanInDips() const {
  return ui::GestureConfiguration::GetInstance()->min_scaling_span_in_pixels();
}

RenderWidgetHostViewMac* SyntheticGestureTargetMac::GetView() const {
  auto* view =
      static_cast<RenderWidgetHostViewMac*>(render_widget_host()->GetView());
  DCHECK(view);
  return view;
}

bool SyntheticGestureTargetMac::PointIsWithinContents(
    RenderWidgetHostView* view,
    const gfx::PointF& point) {
  gfx::Rect bounds = view->GetViewBounds();
  gfx::Rect bounds_in_window =
      bounds - bounds.OffsetFromOrigin();  // Translate the bounds to (0,0).
  return bounds_in_window.Contains(point.x(), point.y());
}

}  // namespace content
