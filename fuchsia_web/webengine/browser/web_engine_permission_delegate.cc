// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_permission_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

WebEnginePermissionDelegate::WebEnginePermissionDelegate() = default;
WebEnginePermissionDelegate::~WebEnginePermissionDelegate() = default;

void WebEnginePermissionDelegate::RequestPermission(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& origin,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {
  std::vector<blink::PermissionType> permissions{permission};
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
    const std::vector<blink::PermissionType>& permissions,
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
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // TODO(crbug.com/1063094): Implement when the PermissionManager protocol is
  // defined and implemented.
  NOTIMPLEMENTED() << ": " << static_cast<int>(permission);
}

void WebEnginePermissionDelegate::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  frame->permission_controller()->RequestPermissions(
      permissions, render_frame_host->GetLastCommittedOrigin(), user_gesture,
      std::move(callback));
}

blink::mojom::PermissionStatus WebEnginePermissionDelegate::GetPermissionStatus(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Although GetPermissionStatusForCurrentDocument() should be used for most
  // permissions, some use cases (e.g., BACKGROUND_SYNC) do not have a frame.
  //
  // TODO(crbug.com/1063094): Handle frame-less permission status checks in the
  // PermissionManager API. Until then, reject such requests.
  return blink::mojom::PermissionStatus::DENIED;
}

content::PermissionResult
WebEnginePermissionDelegate::GetPermissionResultForOriginWithoutContext(
    blink::PermissionType permission,
    const url::Origin& origin) {
  blink::mojom::PermissionStatus status =
      GetPermissionStatus(permission, origin.GetURL(), origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForCurrentDocument(
    blink::PermissionType permission,
    content::RenderFrameHost* render_frame_host) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return frame->permission_controller()->GetPermissionState(
      permission, render_frame_host->GetLastCommittedOrigin());
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForWorker(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  // Use |worker_origin| for requesting_origin and embedding_origin because
  // workers don't have embedders.
  return GetPermissionStatus(permission, worker_origin, worker_origin);
}

WebEnginePermissionDelegate::SubscriptionId
WebEnginePermissionDelegate::SubscribePermissionStatusChange(
    blink::PermissionType permission,
    content::RenderProcessHost* render_process_host,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    base::RepeatingCallback<void(blink::mojom::PermissionStatus)> callback) {
  // TODO(crbug.com/1063094): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
  return SubscriptionId();
}

void WebEnginePermissionDelegate::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {
  // TODO(crbug.com/1063094): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}
