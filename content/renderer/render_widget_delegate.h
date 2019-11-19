// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
class WebWidget;
struct WebDeviceEmulationParams;
}  // namespace blink

namespace content {

//
// RenderWidgetDelegate
//
//  An interface implemented by an object owning a RenderWidget. This is
//  intended to be temporary until the RenderViewImpl and RenderWidget classes
//  are disentangled; see https://crbug.com/583347 and https://crbug.com/478281.
class CONTENT_EXPORT RenderWidgetDelegate {
 public:
  virtual ~RenderWidgetDelegate() = default;

  // See comment in RenderWidgetHost::SetActive().
  virtual void SetActiveForWidget(bool active) = 0;

  // Returns whether multiple windows are allowed for the widget. If true, then
  // Show() may be called more than once.
  virtual bool SupportsMultipleWindowsForWidget() = 0;

  // TODO(bokan): Temporary to unblock synthetic gesture events running under
  // VR. https://crbug.com/940063
  virtual bool ShouldAckSyntheticInputImmediately() = 0;

  // ==================================
  // These methods called during handling of a SynchronizeVisualProperties
  // message to handle updating state on the delegate.
  //
  // Called during handling a SynchronizeVisualProperties message, to close the
  // current PagePopup if there is one.
  virtual void CancelPagePopupForWidget() = 0;
  // Called during handling a SynchronizeVisualProperties message, with the new
  // display mode that will be applied to the RenderWidget. The display mode in
  // the RenderWidget is already changed when this method is called.
  virtual void ApplyNewDisplayModeForWidget(
      blink::mojom::DisplayMode new_display_mode) = 0;
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

  // Called when RenderWidget services RenderWidgetScreenMetricsEmulatorDelegate
  // SetScreenMetricsEmulationParameters().
  virtual void SetScreenMetricsEmulationParametersForWidget(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
