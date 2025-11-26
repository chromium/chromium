// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_permission_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/web_engine_config.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/origin.h"

WebEnginePermissionDelegate::WebEnginePermissionDelegate() = default;
WebEnginePermissionDelegate::~WebEnginePermissionDelegate() = default;

void WebEnginePermissionDelegate::RequestPermissions(
    content::RenderFrameHost* render_frame_host,
    const content::PermissionRequestDescription& request_description,
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
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
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
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
  return content::PermissionResult(
      GetPermissionStatus(permission_descriptor, requesting_origin.GetURL(),
                          embedding_origin.GetURL()));
}

content::PermissionResult
WebEnginePermissionDelegate::GetPermissionResultForCurrentDocument(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    bool should_include_device_status) {
  // See the comment in GetPermissionResultForWorker.
  if (blink::PermissionDescriptorToPermissionType(permission_descriptor) ==
      blink::PermissionType::NOTIFICATIONS) {
    return content::PermissionResult(
        AllowNotifications(render_frame_host->GetLastCommittedURL())
            ? blink::mojom::PermissionStatus::GRANTED
            : blink::mojom::PermissionStatus::DENIED);
  }

  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return content::PermissionResult(
      frame->permission_controller()->GetPermissionState(
          blink::PermissionDescriptorToPermissionType(permission_descriptor),
          render_frame_host->GetLastCommittedOrigin()));
}

content::PermissionResult
WebEnginePermissionDelegate::GetPermissionResultForWorker(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderProcessHost* render_process_host,
    const GURL& worker_origin) {
  // Most of the push messaging API users would expect the permission of
  // notifications, or the service worker has no way to notify users. Though
  // WebEngine does not support platform notifications, to make the behavior as
  // close as a full Chrome browser, the notification permissions should be
  // granted to the same set of origins allowing using PushMessagingServiceImpl.
  if (blink::PermissionDescriptorToPermissionType(permission_descriptor) ==
      blink::PermissionType::NOTIFICATIONS) {
    return content::PermissionResult(
        AllowNotifications(worker_origin)
            ? blink::mojom::PermissionStatus::GRANTED
            : blink::mojom::PermissionStatus::DENIED);
  }
  // Use |worker_origin| for requesting_origin and embedding_origin because
  // workers don't have embedders.
  return content::PermissionResult(
      GetPermissionStatus(permission_descriptor, worker_origin, worker_origin));
}

content::PermissionResult
WebEnginePermissionDelegate::GetPermissionResultForEmbeddedRequester(
    const blink::mojom::PermissionDescriptorPtr& permission_descriptor,
    content::RenderFrameHost* render_frame_host,
    const url::Origin& overridden_origin) {
  FrameImpl* frame = FrameImpl::FromRenderFrameHost(render_frame_host);
  DCHECK(frame);
  return content::PermissionResult(
      frame->permission_controller()->GetPermissionState(
          blink::PermissionDescriptorToPermissionType(permission_descriptor),
          overridden_origin));
}

void WebEnginePermissionDelegate::OnPermissionStatusChangeSubscriptionAdded(
    content::PermissionController::SubscriptionId subscription_id) {
  // TODO(crbug.com/40680523): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WebEnginePermissionDelegate::UnsubscribeFromPermissionResultChange(
    content::PermissionController::SubscriptionId subscription_id) {
  // TODO(crbug.com/40680523): Implement permission status subscription. It's
  // used in blink to emit PermissionStatus.onchange notifications.
  NOTIMPLEMENTED_LOG_ONCE();
}
