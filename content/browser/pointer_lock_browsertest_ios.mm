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
    if (pointer_locked_) {
      UnlockPointer();
    }
  }

  blink::mojom::PointerLockResult LockPointer(
      bool request_unadjusted_movement) override {
    pointer_locked_ = true;
    pointer_lock_unadjusted_movement_ = request_unadjusted_movement;

    return blink::mojom::PointerLockResult::kSuccess;
  }

  blink::mojom::PointerLockResult ChangePointerLock(
      bool request_unadjusted_movement) override {
    pointer_lock_unadjusted_movement_ = request_unadjusted_movement;

    return blink::mojom::PointerLockResult::kSuccess;
  }

  void UnlockPointer() override {
    if (RenderWidgetHostImpl* host =
            RenderWidgetHostImpl::From(GetRenderWidgetHost())) {
      host->LostPointerLock();
    }
    pointer_locked_ = false;
    pointer_lock_unadjusted_movement_ = false;
  }

  bool IsPointerLocked() override { return pointer_locked_; }

  bool GetIsPointerLockedUnadjustedMovementForTesting() override {
    return pointer_lock_unadjusted_movement_;
  }
  bool HasFocus() override { return true; }
  bool CanBePointerLocked() override { return true; }
};

void InstallCreateHooksForPointerLockBrowserTests() {
  WebContentsViewIOS::InstallCreateHookForTests(
      [](RenderWidgetHost* host) -> RenderWidgetHostViewIOS* {
        return new MockPointerLockRenderWidgetHostView(host);
      });
}

}  // namespace content
