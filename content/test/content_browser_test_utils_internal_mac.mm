// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test_utils_internal.h"

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

namespace content {

void RouteMouseEventToPopupViewMacForTesting(
    RenderWidgetHostView* rwhv,
    const blink::WebMouseEvent& event) {
  static_cast<RenderWidgetHostViewMac*>(rwhv)->RouteOrProcessMouseEvent(event);
}

}  // namespace content
