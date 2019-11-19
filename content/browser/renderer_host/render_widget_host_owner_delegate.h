// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/visual_properties.h"

namespace blink {
class WebMouseEvent;
}

namespace gfx {
class Rect;
}

namespace content {
struct ContextMenuParams;
class FrameTreeNode;
struct NativeWebKeyboardEvent;
class RenderFrameHost;
struct WebPreferences;

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

  // The RenderWidget finished the first visually non-empty paint.
  virtual void RenderWidgetDidFirstVisuallyNonEmptyPaint() = 0;

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

  // When false, this allows the renderer's output to be transparent. By default
  // the renderer's background is forced to be opaque.
  virtual void SetBackgroundOpaque(bool opaque) = 0;

  // Returns true if the main frame is active, false if it is swapped out.
  virtual bool IsMainFrameActive() = 0;

  // Returns true if the page, including any widgets, will never be visible.
  virtual bool IsNeverVisible() = 0;

  // Returns the WebkitPreferences for the page. The preferences are shared
  // between all widgets for the page.
  virtual WebPreferences GetWebkitPreferencesForWidget() = 0;

  // Returns the focused frame.
  virtual FrameTreeNode* GetFocusedFrame() = 0;

  // Shows a context menu that is built using the context information
  // provided in |params|.
  virtual void ShowContextMenu(RenderFrameHost* render_frame_host,
                               const ContextMenuParams& params) = 0;

 protected:
  virtual ~RenderWidgetHostOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
