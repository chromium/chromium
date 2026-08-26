// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/wake_lock/wake_lock_service_impl.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/wake_lock_context.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

namespace content {

// static
void WakeLockServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WakeLockService> receiver) {
  CHECK(render_frame_host);
  new WakeLockServiceImpl(*render_frame_host, std::move(receiver));
}

void WakeLockServiceImpl::GetWakeLock(
    device::mojom::WakeLockType type,
    device::mojom::WakeLockReason reason,
    const std::string& description,
    mojo::PendingReceiver<device::mojom::WakeLock> receiver) {
  // Web content is only permitted to request screen wake locks.
  if (type != device::mojom::WakeLockType::kPreventDisplaySleep) {
    return;
  }

  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kScreenWakeLock)) {
    return;
  }

  device::mojom::WakeLockContext* wake_lock_context =
      WebContents::FromRenderFrameHost(&render_frame_host())
          ->GetWakeLockContext();

  if (!wake_lock_context)
    return;

  wake_lock_context->GetWakeLock(type, reason, description,
                                 std::move(receiver));
}

WakeLockServiceImpl::WakeLockServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::WakeLockService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

}  // namespace content
