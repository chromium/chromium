// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_

#include "build/build_config.h"
#include "third_party/blink/public/common/widget/visual_properties.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
class WebMouseEvent;
}

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace content {

//
// RenderWidgetHostOwnerDelegate
//
//  An interface implemented by an object owning a RenderWidgetHost. This is
//  intended to be temporary until the RenderViewHostImpl and
//  RenderWidgetHostImpl classes are disentangled; see http://crbug.com/542477
//  and http://crbug.com/478281.
class RenderWidgetHostOwnerDelegate {
 public:
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
      const input::NativeWebKeyboardEvent& key_event) = 0;

  // Allow OwnerDelegate to control whether its RenderWidgetHost contributes
  // priority to the RenderProcessHost.
  virtual bool ShouldContributePriorityToProcess() = 0;

  // When false, this allows the renderer's output to be transparent. By default
  // the renderer's background is forced to be opaque.
  virtual void SetBackgroundOpaque(bool opaque) = 0;

  // Returns true if the main frame is active, false if it is swapped out.
  virtual bool IsMainFrameActive() = 0;

  // Returns true if all widgets will never be user-visible, and thus do not
  // need to generate pixels for display.
  virtual bool IsNeverComposited() = 0;

  // Returns the WebkitPreferences for the page. The preferences are shared
  // between all widgets for the page.
  virtual blink::web_pref::WebPreferences GetWebkitPreferencesForWidget() = 0;

 protected:
  virtual ~RenderWidgetHostOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
