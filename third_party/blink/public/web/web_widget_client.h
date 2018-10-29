/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_WIDGET_CLIENT_H_

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_text_direction.h"

class SkBitmap;

namespace blink {

class WebDragData;
class WebGestureEvent;
class WebString;
class WebWidget;
struct WebCursorInfo;
struct WebFloatPoint;
struct WebFloatRect;
struct WebFloatSize;

class WebWidgetClient {
 public:
  virtual ~WebWidgetClient() = default;

  // Called when a region of the WebWidget needs to be re-painted.
  virtual void DidInvalidateRect(const WebRect&) {}

  // FIXME: Remove all overrides of this.
  virtual bool AllowsBrokenNullLayerTreeView() const { return false; }

  // Called when a call to WebWidget::animate is required
  virtual void ScheduleAnimation() {}

  // A notification callback for when the intrinsic sizing of the
  // widget changed. This is only called for SVG within a remote frame.
  virtual void IntrinsicSizingInfoChanged(const WebIntrinsicSizingInfo&) {}

  // Called immediately following the first compositor-driven (frame-generating)
  // layout that happened after an interesting document lifecyle change (see
  // WebMeaningfulLayout for details.)
  virtual void DidMeaningfulLayout(WebMeaningfulLayout) {}

  virtual void DidFirstLayoutAfterFinishedParsing() {}

  // Called when the cursor for the widget changes.
  virtual void DidChangeCursor(const WebCursorInfo&) {}

  virtual void AutoscrollStart(const WebFloatPoint&) {}
  virtual void AutoscrollFling(const WebFloatSize& velocity) {}
  virtual void AutoscrollEnd() {}

  // Called when the widget should be closed.  WebWidget::close() should
  // be called asynchronously as a result of this notification.
  virtual void CloseWidgetSoon() {}

  // Called to show the widget according to the given policy.
  virtual void Show(WebNavigationPolicy) {}

  // Called to get/set the position of the widget's window in screen
  // coordinates. Note, the window includes any decorations such as borders,
  // scrollbars, URL bar, tab strip, etc. if they exist.
  virtual WebRect WindowRect() { return WebRect(); }
  virtual void SetWindowRect(const WebRect&) {}

  // Called to get the view rect in screen coordinates. This is the actual
  // content view area, i.e. doesn't include any window decorations.
  virtual WebRect ViewRect() { return WebRect(); }

  // Called when a tooltip should be shown at the current cursor position.
  virtual void SetToolTipText(const WebString&, WebTextDirection hint) {}

  // Requests to lock the mouse cursor. If true is returned, the success
  // result will be asynchronously returned via a single call to
  // WebWidget::didAcquirePointerLock() or
  // WebWidget::didNotAcquirePointerLock().
  // If false, the request has been denied synchronously.
  virtual bool RequestPointerLock() { return false; }

  // Cause the pointer lock to be released. This may be called at any time,
  // including when a lock is pending but not yet acquired.
  // WebWidget::didLosePointerLock() is called when unlock is complete.
  virtual void RequestPointerUnlock() {}

  // Returns true iff the pointer is locked to this widget.
  virtual bool IsPointerLocked() { return false; }

  // Called when a gesture event is handled.
  virtual void DidHandleGestureEvent(const WebGestureEvent& event,
                                     bool event_cancelled) {}

  // Called when overscrolled on main thread. All parameters are in
  // viewport-space.
  virtual void DidOverscroll(const WebFloatSize& overscroll_delta,
                             const WebFloatSize& accumulated_overscroll,
                             const WebFloatPoint& position_in_viewport,
                             const WebFloatSize& velocity_in_viewport,
                             const cc::OverscrollBehavior& behavior) {}

  // Called to update if pointerrawmove events should be sent.
  virtual void HasPointerRawMoveEventHandlers(bool) {}

  // Called to update if touch events should be sent.
  virtual void HasTouchEventHandlers(bool) {}

  // Called to update whether low latency input mode is enabled or not.
  virtual void SetNeedsLowLatencyInput(bool) {}

  // Requests unbuffered (ie. low latency) input until a pointerup
  // event occurs.
  virtual void RequestUnbufferedInputEvents() {}

  // Called during WebWidget::HandleInputEvent for a TouchStart event to inform
  // the embedder of the touch actions that are permitted for this touch.
  virtual void SetTouchAction(WebTouchAction touch_action) {}

  // Request the browser to show virtual keyboard for current input type.
  virtual void ShowVirtualKeyboardOnElementFocus() {}

  // Converts the |rect| from Blink's Viewport coordinates to the
  // coordinates in the native window used to display the content, in
  // DIP.  They're identical in tradional world, but will differ when
  // use-zoom-for-dsf feature is eanbled, and Viewport coordinates
  // becomes DSF times larger than window coordinates.
  // TODO(oshima): Update the comment when the migration is completed.
  virtual void ConvertViewportToWindow(WebRect* rect) {}

  // Converts the |rect| from the coordinates in native window in
  // DIP to Blink's Viewport coordinates. They're identical in
  // tradional world, but will differ when use-zoom-for-dsf feature
  // is eanbled.  TODO(oshima): Update the comment when the
  // migration is completed.
  virtual void ConvertWindowToViewport(WebFloatRect* rect) {}

  // Called when a drag-and-drop operation should begin.
  virtual void StartDragging(network::mojom::ReferrerPolicy,
                             const WebDragData&,
                             WebDragOperationsMask,
                             const SkBitmap& drag_image,
                             const WebPoint& drag_image_offset) {}
};

}  // namespace blink

#endif
