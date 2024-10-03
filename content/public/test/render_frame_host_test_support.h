// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_
#define CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace content {
class RenderFrameHost;

// Test-only helpers.

// Forces RenderFrameHost to be left in pending deletion, so it would not be
// deleted. This is done to ensure that the tests have a way to reliably get a
// RenderFrameHost which is inactive (see RenderFrameHost::IsActive) and
// test that they handle it correctly.
void LeaveInPendingDeletionState(RenderFrameHost* rfh);

// Runs a check to determine whether the runtime-enabled feature, third-party
// storage partitioning, is disabled in the current frame.
bool IsDisableThirdPartyStoragePartitioning2Enabled(RenderFrameHost* rfh);

// Create a permission service bound to the specified receiver.
void CreatePermissionService(
    RenderFrameHost* rfh,
    mojo::PendingReceiver<blink::mojom::PermissionService> receiver);

// Calls RenderFrameHostImpl::DisableUnloadTimerForTesting for the given
// RenderFrameHost.
void DisableUnloadTimerForTesting(RenderFrameHost* rfh);

// Calls RenderFrameHostImpl::WebAuthnAssertionRequestSucceeded for the given
// RenderFrameHost.
void WebAuthnAssertionRequestSucceeded(RenderFrameHost* rfh);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_FRAME_HOST_TEST_SUPPORT_H_
