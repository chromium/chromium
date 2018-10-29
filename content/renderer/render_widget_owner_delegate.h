// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
class WebMouseEvent;
}

namespace content {

//
// RenderWidgetOwnerDelegate
//
//  An interface implemented by an object owning a RenderWidget. This is
//  intended to be temporary until the RenderViewImpl and RenderWidget classes
//  are disentangled; see https://crbug.com/583347 and https://crbug.com/478281.
class CONTENT_EXPORT RenderWidgetOwnerDelegate {
 public:
  // Returns the WebWidget if the delegate has one. Otherwise it returns null,
  // and RenderWidget will fall back to its own WebWidget.
  virtual blink::WebWidget* GetWebWidgetForWidget() const = 0;

  // As in RenderWidgetInputHandlerDelegate. Return true if the event was
  // handled.
  virtual bool RenderWidgetWillHandleMouseEventForWidget(
      const blink::WebMouseEvent& event) = 0;

  // See comment in RenderWidgetHost::SetActive().
  virtual void SetActiveForWidget(bool active) = 0;

  // Returns whether multiple windows are allowed for the widget. If true, then
  // Show() may be called more than once.
  virtual bool SupportsMultipleWindowsForWidget() = 0;

  // Called after RenderWidget services WebWidgetClient::DidHandleGestureEvent()
  // if the event was not cancelled.
  virtual void DidHandleGestureEventForWidget(
      const blink::WebGestureEvent& event) = 0;

  // ==================================
  // These methods called during closing of a RenderWidget.
  //
  // Called when RenderWidget is closed that was "created for a frame".
  virtual void OverrideCloseForWidget() = 0;
  // Called after closing the RenderWidget and destroying the WebView.
  virtual void DidCloseWidget() = 0;
  // ==================================

  // ==================================
  // These methods called during handling of a SynchronizeVisualProperties
  // message to handle updating state on the delegate.
  //
  // Called during handling a SynchronizeVisualProperties message, with the new
  // size that will be applied to the RenderWidget. The size in the RenderWidget
  // has not yet changed when this method is called, as it is changed later.
  virtual void ApplyNewSizeForWidget(const gfx::Size& old_size,
                                     const gfx::Size& new_size) = 0;
  // Called during handling a SynchronizeVisualProperties message, with the new
  // display mode that will be applied to the RenderWidget. The display mode in
  // the RenderWidget is already changed when this method is called.
  virtual void ApplyNewDisplayModeForWidget(
      const blink::WebDisplayMode& new_display_mode) = 0;
  // Called during handling a SynchronizeVisualProperties message, if auto
  // resize is enabled, with the new auto size limits.
  virtual void ApplyAutoResizeLimitsForWidget(const gfx::Size& min_size,
                                              const gfx::Size& max_size) = 0;
  // Called during handling a SynchronizeVisualProperties message, if auto
  // resize was enabled but is being disabled.
  virtual void DisableAutoResizeForWidget() = 0;
  // Called during handling a SynchronizeVisualProperties message, if the
  // message informed that the focused node should be scrolled into view.
  virtual void ScrollFocusedNodeIntoViewForWidget() = 0;
  // ==================================

  // Called when RenderWidget receives a SetFocus event.
  virtual void DidReceiveSetFocusEventForWidget() = 0;

  // Called after RenderWidget changes focus.
  virtual void DidChangeFocusForWidget() = 0;

  // Called when the RenderWidget handles
  // LayerTreeViewDelegate::DidCommitCompositorFrame().
  virtual void DidCommitCompositorFrameForWidget() = 0;

  // Called when the RenderWidget handles
  // LayerTreeViewDelegate::DidCompletePageScaleAnimation().
  virtual void DidCompletePageScaleAnimationForWidget() = 0;

  // Called to resize the WebWidget, so the delegate may change how resize
  // happens.
  virtual void ResizeWebWidgetForWidget(
      const gfx::Size& size,
      float top_controls_height,
      float bottom_controls_height,
      bool browser_controls_shrink_blink_size) = 0;

  // Called to schedule an animation on the WebWidgetClient.
  virtual void RequestScheduleAnimationForWidget() = 0;

  // Called when RenderWidget services RenderWidgetScreenMetricsEmulatorDelegate
  // SetScreenMetricsEmulationParameters().
  virtual void SetScreenMetricsEmulationParametersForWidget(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) = 0;

 protected:
  virtual ~RenderWidgetOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_OWNER_DELEGATE_H_
