// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/test_render_widget_host_view_mac_factory.h"

#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_widget_host.h"

namespace content {

RenderWidgetHostViewBase* CreateRenderWidgetHostViewMacForTesting(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewMac(widget);
}

}  // namespace content
