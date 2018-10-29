// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_HELPER_H_

#include "base/macros.h"

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

namespace content {

namespace mojom {
class RenderWidgetHostNSViewClient;
}  // namespace mojom

class BrowserAccessibilityManager;
struct EditCommand;
struct NativeWebKeyboardEvent;

// An interface through which the NSView for a RenderWidgetHostViewMac is to
// communicate with the RenderWidgetHostViewMac (potentially in another
// process). Unlike mojom::RenderWidgetHostNSViewClient, this object is always
// instantiated in the local process. This is to implement functions that
// cannot be sent across mojo (e.g, GetRootBrowserAccessibilityManager), or
// to avoid unnecessary translation of event types.
class RenderWidgetHostNSViewClientHelper {
 public:
  // Create a RenderWidgetHostNSViewClientHelper that will only implement
  // functionality through mojo (this is in contrast with an in-process
  // RenderWidgetHostNSViewClientHelper that would use raw pointer access).
  static std::unique_ptr<RenderWidgetHostNSViewClientHelper>
  CreateForMojoClient(content::mojom::RenderWidgetHostNSViewClient* client);

  RenderWidgetHostNSViewClientHelper() {}
  virtual ~RenderWidgetHostNSViewClientHelper() {}

  // Return the RenderWidget's BrowserAccessibilityManager.
  // TODO(ccameron): This returns nullptr for non-local NSViews. A scheme for
  // non-local accessibility needs to be developed.
  virtual BrowserAccessibilityManager* GetRootBrowserAccessibilityManager() = 0;

  // Forward a keyboard event to the RenderWidgetHost that is currently handling
  // the key-down event.
  virtual void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                                    const ui::LatencyInfo& latency_info) = 0;
  virtual void ForwardKeyboardEventWithCommands(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      const std::vector<EditCommand>& commands) = 0;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewClientHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_NS_VIEW_CLIENT_HELPER_H_
