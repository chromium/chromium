// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace content {

BackgroundFetchDelegateProxy::BackgroundFetchDelegateProxy(
    base::WeakPtr<StoragePartitionImpl> storage_partition)
    : storage_partition_(std::move(storage_partition)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

BackgroundFetchDelegateProxy::~BackgroundFetchDelegateProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFetchDelegateProxy::SetClickEventDispatcher(
    DispatchClickEventCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  click_event_dispatcher_callback_ = std::move(callback);
}

void BackgroundFetchDelegateProxy::GetIconDisplaySize(
    BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto* delegate = GetDelegate()) {
    delegate->GetIconDisplaySize(std::move(callback));
  } else {
    std::move(callback).Run(gfx::Size(0, 0));
  }
}

void BackgroundFetchDelegateProxy::GetPermissionForOrigin(
    const url::Origin& origin,
    RenderProcessHost* rph,
    RenderFrameHostImpl* rfh,
    GetPermissionForOriginCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Permissions need to go through the DownloadRequestLimiter for top level
  // frames. (This may be missing in unit tests.)
  if (rfh && !rfh->GetParent() &&
      rfh->GetBrowserContext()->GetDownloadManager()->GetDelegate()) {
    WebContents::Getter web_contents_getter(base::BindRepeating(
        [](GlobalRenderFrameHostId rfh_id) {
          return WebContents::FromRenderFrameHost(
              RenderFrameHost::FromID(rfh_id));
        },
        rfh->GetGlobalId()));
    rfh->GetBrowserContext()
        ->GetDownloadManager()
        ->GetDelegate()
        ->CheckDownloadAllowed(
            std::move(web_contents_getter), origin.GetURL(), "GET",
            std::nullopt, /* from_download_cross_origin_redirect= */ false,
            /* content_initiated= */ true, /* mime_type= */ std::string(),
            /* page_transition= */ std::nullopt,
            base::BindOnce(&BackgroundFetchDelegateProxy::
                               DidGetPermissionFromDownloadRequestLimiter,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
    return;
  }

  BackgroundFetchPermission result = BackgroundFetchPermission::BLOCKED;

  if (auto* controller = GetPermissionController()) {
    blink::mojom::PermissionStatus permission_status =
        blink::mojom::PermissionStatus::DENIED;
    if (rfh) {
      DCHECK(origin == rfh->GetLastCommittedOrigin());
      permission_status = controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::BACKGROUND_FETCH, rfh);
    } else if (rph) {
      permission_status = controller->GetPermissionStatusForWorker(
          blink::PermissionType::BACKGROUND_FETCH, rph, origin);
    }
    switch (permission_status) {
      case blink::mojom::PermissionStatus::GRANTED:
        result = BackgroundFetchPermission::ALLOWED;
        break;
      case blink::mojom::PermissionStatus::DENIED:
        result = BackgroundFetchPermission::BLOCKED;
        break;
      case blink::mojom::PermissionStatus::ASK:
        result = BackgroundFetchPermission::ASK;
        break;
    }
  }

  std::move(callback).Run(result);
}

void BackgroundFetchDelegateProxy::CreateDownloadJob(
    base::WeakPtr<Controller> controller,
    std::unique_ptr<BackgroundFetchDescription> fetch_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!controller_map_.count(fetch_description->job_unique_id));
  controller_map_[fetch_description->job_unique_id] = std::move(controller);

  auto* delegate = GetDelegate();
  if (delegate) {
    delegate->CreateDownloadJob(weak_ptr_factory_.GetWeakPtr(),
                                std::move(fetch_description));
  }
}

void BackgroundFetchDelegateProxy::StartRequest(
    const std::string& job_unique_id,
    const url::Origin& origin,
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(controller_map_.count(job_unique_id));
  DCHECK(request);
  DCHECK(!request->download_guid().empty());

  auto* delegate = GetDelegate();
  if (!delegate)
    return;

  const blink::mojom::FetchAPIRequestPtr& fetch_request =
      request->fetch_request();

  const net::NetworkTrafficAnnotationTag traffic_annotation(
      net::DefineNetworkTrafficAnnotation("background_fetch_context",
                                          R"(
            semantics {
              sender: "Background Fetch API"
              description:
                "The Background Fetch API enables developers to upload or "
                "download files on behalf of the user. Such fetches will yield "
                "a user visible notification to inform the user of the "
                "operation, through which it can be suspended, resumed and/or "
                "cancelled. The developer retains control of the file once the "
                "fetch is completed,  similar to XMLHttpRequest and other "
                "mechanisms for fetching resources using JavaScript."
              trigger:
                "When the website uses the Background Fetch API to request "
                "fetching a file and/or a list of files. This is a Web "
                "Platform API for which no express user permission is required."
              data:
                "The request headers and data as set by the website's "
                "developer."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting: "This feature cannot be disabled in settings."
              policy_exception_justification: "Not implemented."
            })"));

  // TODO(peter): The |headers| should be populated with all the properties
  // set in the |fetch_request| structure.
  net::HttpRequestHeaders headers;
  for (const auto& pair : fetch_request->headers)
    headers.SetHeader(pair.first, pair.second);

  // Append the Origin header for requests whose CORS flag is set, or whose
  // request method is not GET or HEAD. See section 3.1 of the standard:
  // https://fetch.spec.whatwg.org/#origin-header
  if (fetch_request->mode == network::mojom::RequestMode::kCors ||
      fetch_request->mode ==
          network::mojom::RequestMode::kCorsWithForcedPreflight ||
      (fetch_request->method != "GET" && fetch_request->method != "HEAD")) {
    headers.SetHeader("Origin", origin.Serialize());
  }

  delegate->DownloadUrl(
      job_unique_id, request->download_guid(), fetch_request->method,
      fetch_request->url, fetch_request->credentials_mode, traffic_annotation,
      headers,
      /* has_request_body= */ request->request_body_size() > 0u);
}

void BackgroundFetchDelegateProxy::UpdateUI(
    const std::string& job_unique_id,
    const std::optional<std::string>& title,
    const std::optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchRegistrationService::UpdateUICallback
        update_ui_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!update_ui_callback_map_.count(job_unique_id));
  update_ui_callback_map_.emplace(job_unique_id, std::move(update_ui_callback));

  if (auto* delegate = GetDelegate())
    delegate->UpdateUI(job_unique_id, title, icon);
}

void BackgroundFetchDelegateProxy::Abort(const std::string& job_unique_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto* delegate = GetDelegate())
    delegate->Abort(job_unique_id);
}

void BackgroundFetchDelegateProxy::MarkJobComplete(
    const std::string& job_unique_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto* delegate = GetDelegate())
    delegate->MarkJobComplete(job_unique_id);

  controller_map_.erase(job_unique_id);
}

void BackgroundFetchDelegateProxy::OnJobCancelled(
    const std::string& job_unique_id,
    const std::string& download_guid,
    blink::mojom::BackgroundFetchFailureReason reason_to_abort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(
      reason_to_abort ==
          blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
      reason_to_abort ==
          blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second) {
    if (reason_to_abort ==
        blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED) {
      // Mark the request as complete and failed to avoid leaking information
      // about the size of the resource.
      controller->DidCompleteRequest(
          download_guid,
          std::make_unique<BackgroundFetchResult>(
              nullptr /* response */, base::Time::Now(),
              BackgroundFetchResult::FailureReason::FETCH_ERROR));
    }

    controller->AbortFromDelegate(reason_to_abort);
  }
}

void BackgroundFetchDelegateProxy::OnDownloadStarted(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second)
    controller->DidStartRequest(guid, std::move(response));
}

void BackgroundFetchDelegateProxy::OnUIActivated(
    const std::string& job_unique_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(click_event_dispatcher_callback_);
  click_event_dispatcher_callback_.Run(job_unique_id);
}

void BackgroundFetchDelegateProxy::OnUIUpdated(
    const std::string& job_unique_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = update_ui_callback_map_.find(job_unique_id);
  if (it == update_ui_callback_map_.end())
    return;

  DCHECK(it->second);
  std::move(it->second).Run(blink::mojom::BackgroundFetchError::NONE);
  update_ui_callback_map_.erase(it);
}

void BackgroundFetchDelegateProxy::OnDownloadUpdated(
    const std::string& job_unique_id,
    const std::string& guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second)
    controller->DidUpdateRequest(guid, bytes_uploaded, bytes_downloaded);
}

void BackgroundFetchDelegateProxy::OnDownloadComplete(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second)
    controller->DidCompleteRequest(guid, std::move(result));
}

void BackgroundFetchDelegateProxy::GetUploadData(
    const std::string& job_unique_id,
    const std::string& download_guid,
    BackgroundFetchDelegate::GetUploadDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (const auto& controller = it->second)
    controller->GetUploadData(download_guid, std::move(callback));
  else
    std::move(callback).Run(nullptr);
}

BrowserContext* BackgroundFetchDelegateProxy::GetBrowserContext() {
  if (!storage_partition_)
    return nullptr;
  return storage_partition_->browser_context();
}

BackgroundFetchDelegate* BackgroundFetchDelegateProxy::GetDelegate() {
  auto* browser_context = GetBrowserContext();
  if (!browser_context)
    return nullptr;
  return browser_context->GetBackgroundFetchDelegate();
}

PermissionController* BackgroundFetchDelegateProxy::GetPermissionController() {
  auto* browser_context = GetBrowserContext();
  if (!browser_context)
    return nullptr;
  return browser_context->GetPermissionController();
}

void BackgroundFetchDelegateProxy::DidGetPermissionFromDownloadRequestLimiter(
    GetPermissionForOriginCallback callback,
    bool has_permission) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(has_permission
                              ? content::BackgroundFetchPermission::ALLOWED
                              : content::BackgroundFetchPermission::BLOCKED);
}

}  // namespace content
