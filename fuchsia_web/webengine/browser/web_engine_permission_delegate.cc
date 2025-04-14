// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_permission_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

WebEnginePermissionDelegate::WebEnginePermissionDelegate() = default;
WebEnginePermissionDelegate::~WebEnginePermissionDelegate() = default;

void WebEnginePermissionDelegate::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  frame->permission_controller()->RequestPermissions(
      blink::PermissionDescriptorToPermissionTypes(
          request_description.permissions),
      url::Origin::Create(request_description.requesting_origin),
      std::move(callback));
}

void WebEnginePermissionDelegate::ResetPermission(
    blink::PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // TODO(crbug.com/40680523): Implement when the PermissionManager protocol is
  // defined and implemented.
  NOTIMPLEMENTED() << ": " << static_cast<int>(permission);
}

void WebEnginePermissionDelegate::RequestPermissionsFromCurrentDocument(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  frame->permission_controller()->RequestPermissions(
      blink::PermissionDescriptorToPermissionTypes(
          request_description.permissions),
      render_frame_host->GetLastCommittedOrigin(), std::move(callback));
}

blink::mojom::PermissionStatus WebEnginePermissionDelegate::GetPermissionStatus(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Although GetPermissionStatusForCurrentDocument() should be used for most
  // permissions, some use cases (e.g., BACKGROUND_SYNC) do not have a frame.
  //
  // TODO(crbug.com/40680523): Handle frame-less permission status checks in the
  // PermissionManager API. Until then, reject such requests.
  return blink::mojom::PermissionStatus::DENIED;
}

content::PermissionResult
WebEnginePermissionDelegate::GetPermissionResultForOriginWithoutContext(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  blink::mojom::PermissionStatus status =
      GetPermissionStatus(permission_descriptor, requesting_origin.GetURL(),
                          embedding_origin.GetURL());

  return content::PermissionResult(
      status, content::PermissionStatusSource::UNSPECIFIED);
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return frame->permission_controller()->GetPermissionState(
      blink::PermissionDescriptorToPermissionType(permission_descriptor),
      render_frame_host->GetLastCommittedOrigin());
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  // Use |worker_origin| for requesting_origin and embedding_origin because
  // workers don't have embedders.
  return GetPermissionStatus(permission_descriptor, worker_origin,
                             worker_origin);
}

blink::mojom::PermissionStatus
WebEnginePermissionDelegate::GetPermissionStatusForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return frame->permission_controller()->GetPermissionState(
      blink::PermissionDescriptorToPermissionType(permission_descriptor),
      overridden_origin);
}

void WebEnginePermissionDelegate::OnPermissionStatusChangeSubscriptionAdded(
    content::PermissionController::SubscriptionId subscription_id) {
  // TODO(crbug.com/40680523): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebEnginePermissionDelegate::UnsubscribeFromPermissionStatusChange(
    content::PermissionController::SubscriptionId subscription_id) {
  // TODO(crbug.com/40680523): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}
