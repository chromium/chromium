// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_frame_host_test_support.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"

namespace content {

void LeaveInPendingDeletionState(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->DoNotDeleteForTesting();
}

void CreatePermissionService(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver) {
  static_cast<RenderFrameHostImpl*>(rfh)->CreatePermissionService(
      std::move(receiver));
}

void DisableUnloadTimerForTesting(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->DisableUnloadTimerForTesting();
}

void WebAuthnAssertionRequestSucceeded(RenderFrameHost* rfh) {
  static_cast<RenderFrameHostImpl*>(rfh)->WebAuthnAssertionRequestSucceeded();
}

}  // namespace content
