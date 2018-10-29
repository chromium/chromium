// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
class WebMouseEvent;
}

namespace content {

struct NativeWebKeyboardEvent;

//
// RenderWidgetHostOwnerDelegate
//
//  An interface implemented by an object owning a RenderWidgetHost. This is
//  intended to be temporary until the RenderViewHostImpl and
//  RenderWidgetHostImpl classes are disentangled; see http://crbug.com/542477
//  and http://crbug.com/478281.
class CONTENT_EXPORT RenderWidgetHostOwnerDelegate {
 public:
  // The RenderWidgetHost has been initialized.
  virtual void RenderWidgetDidInit() = 0;

  // The RenderWidget was closed. Only swapped-in RenderWidgets receive this.
  virtual void RenderWidgetDidClose() = 0;

  // The RenderWidget was closed while in a swapped out state. Used to
  // notify the swapped in render widget to close, which will result in a
  // RenderWidgetDidClose() on the swapped in widget eventually.
  virtual void RenderWidgetNeedsToRouteCloseEvent() = 0;

  // The RenderWidgetHost will be setting its loading state.
  virtual void RenderWidgetWillSetIsLoading(bool is_loading) = 0;

  // The RenderWidget finished the first visually non-empty paint.
  virtual void RenderWidgetDidFirstVisuallyNonEmptyPaint() = 0;

  // The RenderWidget has issued a draw command, signaling the widget
  // has been visually updated.
  virtual void RenderWidgetDidCommitAndDrawCompositorFrame() = 0;

  // The RenderWidgetHost got the focus.
  virtual void RenderWidgetGotFocus() = 0;

  // The RenderWidgetHost lost the focus.
  virtual void RenderWidgetLostFocus() = 0;

  // The RenderWidgetHost forwarded a mouse event.
  virtual void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) = 0;

  // The RenderWidgetHost wants to forward a keyboard event; returns whether
  // it's allowed to do so.
  virtual bool MayRenderWidgetForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) = 0;

  // Allow OwnerDelegate to control whether its RenderWidgetHost contributes
  // priority to the RenderProcessHost.
  virtual bool ShouldContributePriorityToProcess() = 0;

  // Notify the OwnerDelegate that the renderer has requested a change in
  // the bounds of the content area.
  virtual void RequestSetBounds(const gfx::Rect& bounds) = 0;

 protected:
  virtual ~RenderWidgetHostOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
