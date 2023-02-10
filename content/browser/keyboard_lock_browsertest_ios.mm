// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/keyboard_lock_browsertest.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/web_contents/web_contents_view_ios.h"

namespace content {

namespace {

bool g_window_has_focus = false;

class TestRenderWidgetHostView : public RenderWidgetHostViewIOS {
 public:
  TestRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewIOS(host) {}
  ~TestRenderWidgetHostView() override {}

  bool HasFocus() override { return g_window_has_focus; }
};
}  // namespace

void SetWindowFocusForKeyboardLockBrowserTests(bool is_focused) {
  g_window_has_focus = is_focused;
}

void InstallCreateHooksForKeyboardLockBrowserTests() {
  WebContentsViewIOS::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewIOS* {
        return new TestRenderWidgetHostView(host);
      });
}

}  // namespace content
