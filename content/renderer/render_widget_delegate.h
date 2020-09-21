// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
class WebWidget;
}  // namespace blink

namespace content {

//
// RenderWidgetDelegate
//
//  An interface to provide View-level (and/or Page-level) functionality to
//  the main frame's RenderWidget.
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

  // Returns the current state of auto resize.
  virtual bool AutoResizeMode() = 0;

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
      const gfx::Size& visible_viewport_size,
      cc::BrowserControlsParams browser_controls_params) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_DELEGATE_H_
