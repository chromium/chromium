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

#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_touch_action.h"
#include "third_party/blink/public/web/web_widget.h"

namespace blink {

class WebDragData;
class WebLocalFrame;
class WebInputMethodController;
class WebWidgetClient;
struct WebFloatPoint;

class WebFrameWidget : public WebWidget {
 public:
  // Makes a WebFrameWidget that wraps a pre-existing WebWidget from the
  // RenderView/WebView, for a new local main frame.
  BLINK_EXPORT static WebFrameWidget* CreateForMainFrame(
      WebWidgetClient*,
      WebLocalFrame* main_frame);
  // Makes a WebFrameWidget that wraps a WebLocalFrame that is not a main frame,
  // providing a WebWidget to interact with the child local root frame.
  BLINK_EXPORT static WebFrameWidget* CreateForChildLocalRoot(
      WebWidgetClient*,
      WebLocalFrame* local_root);

  // Sets the visibility of the WebFrameWidget.
  // We still track page-level visibility, but additionally we need to notify a
  // WebFrameWidget when its owning RenderWidget receives a Show or Hide
  // directive, so that it knows whether it needs to draw or not.
  virtual void SetVisibilityState(mojom::PageVisibilityState visibility_state) {
  }

  // Overrides the WebFrameWidget's background and base background color. You
  // can use this to enforce a transparent background, which is useful if you
  // want to have some custom background rendered behind the widget.
  virtual void SetBackgroundColorOverride(SkColor) = 0;
  virtual void ClearBackgroundColorOverride() = 0;
  virtual void SetBaseBackgroundColorOverride(SkColor) = 0;
  virtual void ClearBaseBackgroundColorOverride() = 0;

  // Sets the base color used for this WebFrameWidget's background. This is in
  // effect the default background color used for pages with no
  // background-color style in effect, or used as the alpha-blended basis for
  // any pages with translucent background-color style. (For pages with opaque
  // background-color style, this property is effectively ignored).
  // Setting this takes effect for the currently loaded page, if any, and
  // persists across subsequent navigations. Defaults to white prior to the
  // first call to this method.
  virtual void SetBaseBackgroundColor(SkColor) = 0;

  // Returns the local root of this WebFrameWidget.
  virtual WebLocalFrame* LocalRoot() const = 0;

  // WebWidget implementation.
  bool IsWebFrameWidget() const final { return true; }

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
      const WebFloatPoint& point_in_viewport,
      const WebFloatPoint& screen_point,
      WebDragOperationsMask operations_allowed,
      int modifiers) = 0;
  virtual WebDragOperation DragTargetDragOver(
      const WebFloatPoint& point_in_viewport,
      const WebFloatPoint& screen_point,
      WebDragOperationsMask operations_allowed,
      int modifiers) = 0;
  virtual void DragTargetDragLeave(const WebFloatPoint& point_in_viewport,
                                   const WebFloatPoint& screen_point) = 0;
  virtual void DragTargetDrop(const WebDragData&,
                              const WebFloatPoint& point_in_viewport,
                              const WebFloatPoint& screen_point,
                              int modifiers) = 0;

  // Notifies the WebFrameWidget that a drag has terminated.
  virtual void DragSourceEndedAt(const WebFloatPoint& point_in_viewport,
                                 const WebFloatPoint& screen_point,
                                 WebDragOperation) = 0;

  // Notifies the WebFrameWidget that the system drag and drop operation has
  // ended.
  virtual void DragSourceSystemDragEnded() = 0;

  // Constrains the viewport intersection for use by IntersectionObserver,
  // and indicates whether the frame may be painted over or obscured in the
  // parent. This is needed for out-of-process iframes to know if they are
  // clipped or obscured by ancestor frames in another process.
  virtual void SetRemoteViewportIntersection(const WebRect&, bool) {}

  // Sets the inert bit on an out-of-process iframe, causing it to ignore
  // input.
  virtual void SetIsInert(bool) {}

  // Sets the inherited effective touch action on an out-of-process iframe.
  virtual void SetInheritedEffectiveTouchAction(WebTouchAction) {}

  // Toggles render throttling for an out-of-process iframe. Local frames are
  // throttled based on their visibility in the viewport, but remote frames
  // have to have throttling information propagated from parent to child
  // across processes.
  virtual void UpdateRenderThrottlingStatus(bool is_throttled,
                                            bool subtree_throttled) {}

  // Returns the currently focused WebLocalFrame (if any) inside this
  // WebFrameWidget. That is a WebLocalFrame which is focused and shares the
  // same LocalRoot() as this WebFrameWidget's LocalRoot().
  virtual WebLocalFrame* FocusedWebLocalFrameInWidget() const = 0;

  // Scrolls the editable element which is currently focused in (a focused frame
  // inside) this widget into view. The scrolling might end with a final zooming
  // into the editable region which is performed in the main frame process.
  virtual bool ScrollFocusedEditableElementIntoView() = 0;
};

}  // namespace blink

#endif
