// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock_browsertest.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_view_mac.h"

namespace content {

namespace {

bool g_window_has_focus = false;

class TestRenderWidgetHostView : public RenderWidgetHostViewMac {
 public:
  TestRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewMac(host) {}
  ~TestRenderWidgetHostView() override {}

  bool HasFocus() override { return g_window_has_focus; }
};
}

void SetWindowFocusForKeyboardLockBrowserTests(bool is_focused) {
  g_window_has_focus = is_focused;
}

void InstallCreateHooksForKeyboardLockBrowserTests() {
  WebContentsViewMac::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewMac* {
        return new TestRenderWidgetHostView(host);
      });
}

}  // namespace content
