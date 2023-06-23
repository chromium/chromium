// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"

#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_widget_host.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace content {

RenderWidgetHostViewBase* CreateRenderWidgetHostViewMacForTesting(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

BrowserCompositorMac* GetBrowserCompositorMacForTesting(
    const RenderWidgetHostView* rwhv) {
  return static_cast<const RenderWidgetHostViewMac*>(rwhv)->BrowserCompositor();
}

void SetScreenInfosForTesting(RenderWidgetHostView* rwhv,
                              const display::ScreenInfos& screen_infos) {
  return static_cast<RenderWidgetHostViewMac*>(rwhv)->OnScreenInfosChanged(
      screen_infos);
}

}  // namespace content
