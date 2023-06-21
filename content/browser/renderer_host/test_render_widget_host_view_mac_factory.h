// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_WIDGET_HOST_VIEW_MAC_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_WIDGET_HOST_VIEW_MAC_FACTORY_H_

#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {

class BrowserCompositorMac;
class RenderWidgetHost;

// Returns a new RenderWidgetHostViewMac that can be used in a C++ unit test or
// browser test. These tests can't include render_widget_host_view_mac.h
// directly because it contains Objective-C code. The returned object must be
// deleted by calling its `Destroy` method (see the comments in
// render_widget_host_view_mac.h.)
RenderWidgetHostViewBase* CreateRenderWidgetHostViewMacForTesting(
    RenderWidgetHost* widget);

// Returns the BrowserCompositorMac for `rwhv`, which must be a
// RenderWidgetHostViewMac.
BrowserCompositorMac* GetBrowserCompositorMacForTesting(
    const RenderWidgetHostView* rwhv);

// Force setting the cached screen info in RenderWidgetHostViewMac. `rwhv` must
// be an instance of RenderWidgetHostViewMac.
void SetScreenInfosForTesting(RenderWidgetHostView* rwhv,
                              const display::ScreenInfos& screen_infos);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEST_RENDER_WIDGET_HOST_VIEW_MAC_FACTORY_H_
