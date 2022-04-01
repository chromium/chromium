// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_permission_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "url/origin.h"

WebEnginePermissionDelegate::WebEnginePermissionDelegate() = default;
WebEnginePermissionDelegate::~WebEnginePermissionDelegate() = default;

void WebEnginePermissionDelegate::RequestPermission(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  std::vector<content::PermissionType> permissions{permission};
  RequestPermissions(
      permissions, render_frame_host, origin, user_gesture,
      base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::PermissionStatus)> callback,
             const std::vector<blink::mojom::PermissionStatus>& state) {
            DCHECK_EQ(state.size(), 1U);
            std::move(callback).Run(state[0]);
          },
          std::move(callback)));
}

void WebEnginePermissionDelegate::RequestPermissions(
    const std::vector<content::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  frame->permission_controller()->RequestPermissions(
      permissions, url::Origin::Create(requesting_origin), user_gesture,
      std::move(callback));
}

void WebEnginePermissionDelegate::ResetPermission(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // TODO(crbug.com/1063094): Implement when the PermissionManager protocol is
  // defined and implemented.
  NOTIMPLEMENTED() << ": " << static_cast<int>(permission);
}

blink::mojom::PermissionStatus WebEnginePermissionDelegate::GetPermissionStatus(
    content::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Although GetPermissionStatusForFrame() should be used for most permissions,
  // some use cases (e.g., BACKGROUND_SYNC) do not have a frame.
  //
  // TODO(crbug.com/1063094): Handle frame-less permission status checks in the
  // PermissionManager API. Until then, reject such requests.
  return blink::mojom::PermissionStatus::DENIED;
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForFrame(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return frame->permission_controller()->GetPermissionState(
      permission, url::Origin::Create(requesting_origin));
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForCurrentDocument(
    content::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return frame->permission_controller()->GetPermissionState(
      permission, render_frame_host->GetLastCommittedOrigin());
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForWorker(
    content::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  // Use |worker_origin| for requesting_origin and embedding_origin because
  // workers don't have embedders.
  return GetPermissionStatus(permission, worker_origin, worker_origin);
}

WebEnginePermissionDelegate::SubscriptionId
WebEnginePermissionDelegate::SubscribePermissionStatusChange(
    content::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  // TODO(crbug.com/1063094): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE() << ": " << static_cast<int>(permission);
  return SubscriptionId();
}

void WebEnginePermissionDelegate::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {
  // TODO(crbug.com/1063094): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}
