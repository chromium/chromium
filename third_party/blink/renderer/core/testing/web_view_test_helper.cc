// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/web_view_test_helper.h"

#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"

namespace blink {
WebHitTestResult HitTestResultForTap(WebView* web_view,
                                     const gfx::Point& tap_point,
                                     const gfx::Size& tap_area) {
  if (!web_view) {
    return HitTestResult();
  }

  WebViewImpl* web_view_impl = DynamicTo<WebViewImpl>(web_view);

  auto* main_frame =
      DynamicTo<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  if (!main_frame) {
    return HitTestResult();
  }

  WebGestureEvent tap_event(WebInputEvent::Type::kGestureTap,
                            WebInputEvent::kNoModifiers, base::TimeTicks::Now(),
                            WebGestureDevice::kTouchscreen);
  // GestureTap is only ever from a touchscreen.
  tap_event.SetPositionInWidget(gfx::PointF(tap_point));
  tap_event.data.tap.tap_count = 1;
  tap_event.data.tap.width = tap_area.width();
  tap_event.data.tap.height = tap_area.height();

  WebLocalFrameImpl* local_frame_impl =
      WebLocalFrameImpl::FromFrame(main_frame);
  WebGestureEvent scaled_event =
      TransformWebGestureEvent(local_frame_impl->GetFrameView(), tap_event);

  HitTestResult result =
      main_frame->GetEventHandler()
          .HitTestResultForGestureEvent(
              scaled_event, HitTestRequest::kReadOnly | HitTestRequest::kActive)
          .GetHitTestResult();

  result.SetToShadowHostIfInUAShadowRoot();
  return result;
}
}  // namespace blink
