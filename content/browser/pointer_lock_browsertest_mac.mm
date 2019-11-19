// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/pointer_lock_browsertest.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_view_mac.h"

namespace content {

class MockPointerLockRenderWidgetHostView : public RenderWidgetHostViewMac {
 public:
  MockPointerLockRenderWidgetHostView(RenderWidgetHost* host,
                                      bool is_guest_view_hack)
      : RenderWidgetHostViewMac(host, is_guest_view_hack) {}
  ~MockPointerLockRenderWidgetHostView() override {
    if (mouse_locked_)
      UnlockMouse();
  }

  bool LockMouse(bool request_unadjusted_movement) override {
    if (request_unadjusted_movement)
      return false;

    mouse_locked_ = true;

    return true;
  }

  void UnlockMouse() override {
    if (RenderWidgetHostImpl* host =
            RenderWidgetHostImpl::From(GetRenderWidgetHost())) {
      host->LostMouseLock();
    }
    mouse_locked_ = false;
  }

  bool IsMouseLocked() override { return mouse_locked_; }

  bool GetIsMouseLockedUnadjustedMovementForTesting() override { return false; }
  bool HasFocus() override { return true; }
};

void InstallCreateHooksForPointerLockBrowserTests() {
  WebContentsViewMac::InstallCreateHookForTests(
      [](RenderWidgetHost* host,
         bool is_guest_view_hack) -> RenderWidgetHostViewMac* {
        return new MockPointerLockRenderWidgetHostView(host,
                                                       is_guest_view_hack);
      });
}

}  // namespace content
