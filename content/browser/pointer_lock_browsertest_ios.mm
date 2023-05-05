// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/pointer_lock_browsertest.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/web_contents/web_contents_view_ios.h"

namespace content {

class MockPointerLockRenderWidgetHostView : public RenderWidgetHostViewIOS {
 public:
  MockPointerLockRenderWidgetHostView(RenderWidgetHost* host)
      : RenderWidgetHostViewIOS(host) {}
  ~MockPointerLockRenderWidgetHostView() override {
    if (mouse_locked_) {
      UnlockMouse();
    }
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
  WebContentsViewIOS::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewIOS* {
        return new MockPointerLockRenderWidgetHostView(host);
      });
}

}  // namespace content
