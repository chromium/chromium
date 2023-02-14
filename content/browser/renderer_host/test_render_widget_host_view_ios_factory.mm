// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_render_widget_host_view_ios_factory.h"

#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

RenderWidgetHostViewBase* CreateRenderWidgetHostViewIOSForTesting(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewIOS(widget);
}

BrowserCompositorIOS* GetBrowserCompositorIOSForTesting(
    const RenderWidgetHostView* rwhv) {
  return static_cast<const RenderWidgetHostViewIOS*>(rwhv)->BrowserCompositor();
}

}  // namespace content
