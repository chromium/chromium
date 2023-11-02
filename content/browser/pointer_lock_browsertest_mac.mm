// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/pointer_lock_browsertest.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/web_contents/web_contents_view_mac.h"

namespace content {

class MockPointerLockRenderWidgetHostView : public RenderWidgetHostViewMac {
 public:
  MockPointerLockRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewMac(host) {}
  ~MockPointerLockRenderWidgetHostView() override {
    if (mouse_locked_)
      UnlockMouse();
  }

  blink::mojom::PointerLockResult LockMouse(
      bool request_unadjusted_movement) override {
    mouse_locked_ = true;
    mouse_lock_unadjusted_movement_ = request_unadjusted_movement;

    return blink::mojom::PointerLockResult::kSuccess;
  }

  blink::mojom::PointerLockResult ChangeMouseLock(
      bool request_unadjusted_movement) override {
    mouse_lock_unadjusted_movement_ = request_unadjusted_movement;

    return blink::mojom::PointerLockResult::kSuccess;
  }

  void UnlockMouse() override {
    if (RenderWidgetHostImpl* host =
            RenderWidgetHostImpl::From(GetRenderWidgetHost())) {
      host->LostMouseLock();
    }
    mouse_locked_ = false;
    mouse_lock_unadjusted_movement_ = false;
  }

  bool IsMouseLocked() override { return mouse_locked_; }

  bool GetIsMouseLockedUnadjustedMovementForTesting() override {
    return mouse_lock_unadjusted_movement_;
  }
  bool HasFocus() override { return true; }
  bool CanBeMouseLocked() override { return true; }
};

void InstallCreateHooksForPointerLockBrowserTests() {
  WebContentsViewMac::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewMac* {
        return new MockPointerLockRenderWidgetHostView(host);
      });
}

}  // namespace content
