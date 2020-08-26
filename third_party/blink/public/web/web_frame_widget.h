/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_WIDGET_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_WIDGET_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "third_party/blink/public/common/page/web_drag_operation.h"
#include "third_party/blink/public/mojom/page/widget.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_swap_result.h"
#include "third_party/blink/public/web/web_widget.h"

namespace blink {

class WebDragData;
class WebLocalFrame;
class WebInputMethodController;
class WebWidgetClient;

class WebFrameWidget : public WebWidget {
 public:
  // Makes a WebFrameWidget that wraps a pre-existing WebWidget from the
  // RenderView/WebView, for a new local main frame.
  // Main frames can be nested in cases like Portals or GuestViews.
  BLINK_EXPORT static WebFrameWidget* CreateForMainFrame(
      WebWidgetClient*,
      WebLocalFrame* main_frame,
      CrossVariantMojoAssociatedRemote<mojom::FrameWidgetHostInterfaceBase>
          frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget,
      bool is_for_nested_main_frame = false);
  // Makes a WebFrameWidget that wraps a WebLocalFrame that is not a main frame,
  // providing a WebWidget to interact with the child local root frame.
  BLINK_EXPORT static WebFrameWidget* CreateForChildLocalRoot(
      WebWidgetClient*,
      WebLocalFrame* local_root,
      CrossVariantMojoAssociatedRemote<mojom::FrameWidgetHostInterfaceBase>
          frame_widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::FrameWidgetInterfaceBase>
          frame_widget,
      CrossVariantMojoAssociatedRemote<mojom::WidgetHostInterfaceBase>
          widget_host,
      CrossVariantMojoAssociatedReceiver<mojom::WidgetInterfaceBase> widget);

  // Returns the local root of this WebFrameWidget.
  virtual WebLocalFrame* LocalRoot() const = 0;

  // Called when the root LocalFrame of this WebFrameWidget (and implicitly its
  // subtree) are being detached. The frame pointer and the WebWidgetClient are
  // still valid while this is called, for them to be used for any cleanup.
  virtual void DidDetachLocalFrameTree() = 0;

  // Current instance of the active WebInputMethodController, that is, the
  // WebInputMethodController corresponding to (and owned by) the focused
  // WebLocalFrameImpl. It will return nullptr when there are no focused
  // frames inside this WebFrameWidget.
  virtual WebInputMethodController* GetActiveWebInputMethodController()
      const = 0;

  // Callback methods when a drag-and-drop operation is trying to drop something
  // on the WebFrameWidget.
  virtual WebDragOperation DragTargetDragEnter(
      const WebDragData&,
      const gfx::PointF& point_in_viewport,
      const gfx::PointF& screen_point,
      WebDragOperationsMask operations_allowed,
      uint32_t key_modifiers) = 0;
  virtual void DragTargetDragOver(
      const gfx::PointF& point_in_viewport,
      const gfx::PointF& screen_point,
      WebDragOperationsMask operations_allowed,
      uint32_t key_modifiers,
      base::OnceCallback<void(blink::WebDragOperation)> callback) = 0;
  virtual void DragTargetDragLeave(const gfx::PointF& point_in_viewport,
                                   const gfx::PointF& screen_point) = 0;
  virtual void DragTargetDrop(const WebDragData&,
                              const gfx::PointF& point_in_viewport,
                              const gfx::PointF& screen_point,
                              uint32_t key_modifiers) = 0;

  // Notifies the WebFrameWidget that a drag has terminated.
  virtual void DragSourceEndedAt(const gfx::PointF& point_in_viewport,
                                 const gfx::PointF& screen_point,
                                 WebDragOperation) = 0;

  // Notifies the WebFrameWidget that the system drag and drop operation has
  // ended.
  virtual void DragSourceSystemDragEnded() = 0;

  // Constrains the viewport intersection for use by IntersectionObserver,
  // and indicates whether the frame may be painted over or obscured in the
  // parent. This is needed for out-of-process iframes to know if they are
  // clipped or obscured by ancestor frames in another process.
  virtual void SetRemoteViewportIntersection(const ViewportIntersectionState&) {
  }

  // Sets the inherited effective touch action on an out-of-process iframe.
  virtual void SetInheritedEffectiveTouchAction(WebTouchAction) {}

  // Returns the currently focused WebLocalFrame (if any) inside this
  // WebFrameWidget. That is a WebLocalFrame which is focused and shares the
  // same LocalRoot() as this WebFrameWidget's LocalRoot().
  virtual WebLocalFrame* FocusedWebLocalFrameInWidget() const = 0;

  // Scrolls the editable element which is currently focused in (a focused frame
  // inside) this widget into view. The scrolling might end with a final zooming
  // into the editable region which is performed in the main frame process.
  virtual bool ScrollFocusedEditableElementIntoView() = 0;

  // This function provides zooming for find in page results when browsing with
  // page autosize.
  virtual void ZoomToFindInPageRect(const WebRect& rect_in_root_frame) = 0;

  // Applies viewport related properties that are normally provided by the
  // compositor. Useful for tests that don't use a compositor.
  virtual void ApplyViewportChangesForTesting(
      const cc::ApplyViewportChangesArgs& args) = 0;

  // The |callback| will be fired when the corresponding renderer frame is
  // submitted (still called "swapped") to the display compositor (either with
  // DidSwap or DidNotSwap).
  virtual void NotifySwapAndPresentationTime(
      WebReportTimeCallback swap_callback,
      WebReportTimeCallback presentation_callback) = 0;

  // Instructs devtools to pause loading of the frame as soon as it's shown
  // until explicit command from the devtools client.
  virtual void WaitForDebuggerWhenShown() = 0;

  // Scales the text in the frame by a factor of text_zoom_factor.
  virtual void SetTextZoomFactor(float text_zoom_factor) = 0;
  // Returns the current text zoom factor, where 1.0 is the normal size, > 1.0
  // is scaled up and < 1.0 is scaled down.
  virtual float TextZoomFactor() = 0;

  // Overlay this frame with a solid color. Only valid for the main frame's
  // widget.
  virtual void SetMainFrameOverlayColor(SkColor) = 0;

  // Add an edit command to be processed as the default action if the next
  // keyboard event is unhandled.
  virtual void AddEditCommandForNextKeyEvent(const WebString& name,
                                             const WebString& value) = 0;

  // Clear any active edit commands that are pending.
  virtual void ClearEditCommands() = 0;

  // If the widget is currently handling a paste.
  virtual bool IsPasting() = 0;

  // If the widget is currently selecting a range.
  virtual bool HandlingSelectRange() = 0;

  // If fullscreen has been granted.
  virtual bool IsFullscreenGranted() = 0;

  // Returns true if a pinch gesture is currently active in main frame.
  virtual bool PinchGestureActiveInMainFrame() = 0;

  // Returns page scale in main frame..
  virtual float PageScaleInMainFrame() = 0;

  // Override the zoom level for testing.
  virtual void SetZoomLevelForTesting(double zoom_level) = 0;

  // Remove the override for zoom level.
  virtual void ResetZoomLevelForTesting() = 0;

  // Override the device scale factor for testing.
  virtual void SetDeviceScaleFactorForTesting(float factor) = 0;

  // Get the window segments for this widget.
  // See https://github.com/webscreens/window-segments/blob/master/EXPLAINER.md
  virtual const WebVector<gfx::Rect>& WindowSegments() const = 0;

 private:
  // This private constructor and the class/friend declaration ensures that
  // WebFrameWidgetBase is the only concrete subclass that implements
  // WebFrameWidget, so that it is safe to downcast to WebFrameWidgetBase.
  friend class WebFrameWidgetBase;
  WebFrameWidget() = default;
};

}  // namespace blink

#endif
