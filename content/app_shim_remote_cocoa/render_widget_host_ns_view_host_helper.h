// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_HOST_HELPER_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_HOST_HELPER_H_

#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"

#include <vector>

namespace blink {
class WebGestureEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebTouchEvent;
}  // namespace blink

namespace ui {
class LatencyInfo;
}  // namespace ui

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace remote_cocoa {

namespace mojom {
class RenderWidgetHostNSViewHost;
}  // namespace mojom

// An interface through which the NSView for a RenderWidgetHostViewMac is to
// communicate with the RenderWidgetHostViewMac (potentially in another
// process). Unlike mojom::RenderWidgetHostNSViewHost, this object is always
// instantiated in the local process. This is to implement functions that
// cannot be sent across mojo or to avoid unnecessary translation of event
// types.
class RenderWidgetHostNSViewHostHelper {
 public:
  RenderWidgetHostNSViewHostHelper() = default;

  RenderWidgetHostNSViewHostHelper(const RenderWidgetHostNSViewHostHelper&) =
      delete;
  RenderWidgetHostNSViewHostHelper& operator=(
      const RenderWidgetHostNSViewHostHelper&) = delete;

  virtual ~RenderWidgetHostNSViewHostHelper() = default;

  // Return the RenderWidget's accessibility node.
  virtual id GetAccessibilityElement() = 0;

  // Return the RenderWidget's BrowserAccessibilityManager's root accessibility
  // node.
  virtual id GetRootBrowserAccessibilityElement() = 0;

  // Return the currently focused accessibility element.
  virtual id GetFocusedBrowserAccessibilityElement() = 0;

  // Set the NSWindow that will be the accessibility parent of the NSView.
  virtual void SetAccessibilityWindow(NSWindow* window) = 0;

  // Forward a keyboard event to the RenderWidgetHost that is currently handling
  // the key-down event.
  virtual void ForwardKeyboardEvent(
      const input::NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info) = 0;
  virtual void ForwardKeyboardEventWithCommands(
      const input::NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      std::vector<blink::mojom::EditCommandPtr> commands) = 0;

  // Forward events to the renderer or the input router, as appropriate.
  virtual void RouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) = 0;
  virtual void RouteOrProcessTouchEvent(
      const blink::WebTouchEvent& web_event) = 0;
  virtual void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) = 0;

  // Special case forwarding of synthetic events to the renderer.
  virtual void ForwardMouseEvent(const blink::WebMouseEvent& web_event) = 0;
  virtual void ForwardWheelEvent(
      const blink::WebMouseWheelEvent& web_event) = 0;

  // Handling pinch gesture events.
  virtual void GestureBegin(blink::WebGestureEvent begin_event,
                            bool is_synthetically_injected) = 0;
  virtual void GestureUpdate(blink::WebGestureEvent update_event) = 0;
  virtual void GestureEnd(blink::WebGestureEvent end_event) = 0;
  virtual void SmartMagnify(
      const blink::WebGestureEvent& smart_magnify_event) = 0;
};

}  // namespace remote_cocoa

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_HOST_HELPER_H_
