// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_delegate_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/download_manager.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// Internal functionality of the BackgroundFetchDelegateProxy that lives on the
// UI thread, where all interaction with the download manager must happen.
class BackgroundFetchDelegateProxy::Core
    : public BackgroundFetchDelegate::Client {
 public:
  Core(const base::WeakPtr<BackgroundFetchDelegateProxy>& parent,
       BrowserContext* browser_context)
      : parent_(parent), browser_context_(browser_context) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(browser_context_);
  }

  ~Core() override { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  base::WeakPtr<Core> GetWeakPtrOnUI() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    return weak_ptr_factory_.GetWeakPtr();
  }

  void ForwardGetPermissionForOriginCallbackToParentThread(
      BackgroundFetchDelegate::GetPermissionForOriginCallback callback,
      BackgroundFetchPermission permission) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                          base::BindOnce(std::move(callback), permission));
  }

  void GetPermissionForOrigin(
      const url::Origin& origin,
      const WebContents::Getter& wc_getter,
      BackgroundFetchDelegate::GetPermissionForOriginCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* delegate = browser_context_->GetBackgroundFetchDelegate()) {
      delegate->GetPermissionForOrigin(
          origin, wc_getter,
          base::BindOnce(
              &Core::ForwardGetPermissionForOriginCallbackToParentThread,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      std::move(callback).Run(BackgroundFetchPermission::BLOCKED);
    }
  }

  void ForwardGetIconDisplaySizeCallbackToParentThread(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback,
      const gfx::Size& display_size) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RunOrPostTaskOnThread(FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
                          base::BindOnce(std::move(callback), display_size));
  }

  void GetIconDisplaySize(
      BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* delegate = browser_context_->GetBackgroundFetchDelegate()) {
      delegate->GetIconDisplaySize(
          base::BindOnce(&Core::ForwardGetIconDisplaySizeCallbackToParentThread,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      RunOrPostTaskOnThread(
          FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
          base::BindOnce(std::move(callback), gfx::Size(0, 0)));
    }
  }

  void CreateDownloadJob(
      std::unique_ptr<BackgroundFetchDescription> fetch_description) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    auto* delegate = browser_context_->GetBackgroundFetchDelegate();
    if (delegate)
      delegate->CreateDownloadJob(GetWeakPtrOnUI(),
                                  std::move(fetch_description));
  }

  void StartRequest(const std::string& job_unique_id,
                    const url::Origin& origin,
                    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request);

    auto* delegate = browser_context_->GetBackgroundFetchDelegate();
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
        fetch_request->url, traffic_annotation, headers,
        /* has_request_body= */ request->request_body_size() > 0u);
  }

  void Abort(const std::string& job_unique_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* delegate = browser_context_->GetBackgroundFetchDelegate())
      delegate->Abort(job_unique_id);
  }

  void MarkJobComplete(const std::string& job_unique_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* delegate = browser_context_->GetBackgroundFetchDelegate())
      delegate->MarkJobComplete(job_unique_id);
  }

  void UpdateUI(const std::string& job_unique_id,
                const base::Optional<std::string>& title,
                const base::Optional<SkBitmap>& icon) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (auto* delegate = browser_context_->GetBackgroundFetchDelegate())
      delegate->UpdateUI(job_unique_id, title, icon);
  }

  // BackgroundFetchDelegate::Client implementation:
  void OnJobCancelled(
      const std::string& job_unique_id,
      blink::mojom::BackgroundFetchFailureReason reason_to_abort) override;
  void OnDownloadUpdated(const std::string& job_unique_id,
                         const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded) override;
  void OnDownloadComplete(
      const std::string& job_unique_id,
      const std::string& guid,
      std::unique_ptr<BackgroundFetchResult> result) override;
  void OnDownloadStarted(
      const std::string& job_unique_id,
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response) override;
  void OnUIActivated(const std::string& unique_id) override;
  void OnUIUpdated(const std::string& unique_id) override;
  void GetUploadData(
      const std::string& job_unique_id,
      const std::string& download_guid,
      BackgroundFetchDelegate::GetUploadDataCallback callback) override;

 private:
  // Weak reference to the service worker core thread outer class that owns us.
  base::WeakPtr<BackgroundFetchDelegateProxy> parent_;

  BrowserContext* browser_context_;

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Core);
};

void BackgroundFetchDelegateProxy::Core::OnJobCancelled(
    const std::string& job_unique_id,
    blink::mojom::BackgroundFetchFailureReason reason_to_abort) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::OnJobCancelled, parent_,
                     job_unique_id, reason_to_abort));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadUpdated(
    const std::string& job_unique_id,
    const std::string& guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::OnDownloadUpdated, parent_,
                     job_unique_id, guid, bytes_uploaded, bytes_downloaded));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadComplete(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::OnDownloadComplete, parent_,
                     job_unique_id, guid, std::move(result)));
}

void BackgroundFetchDelegateProxy::Core::OnDownloadStarted(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::DidStartRequest, parent_,
                     job_unique_id, guid, std::move(response)));
}

void BackgroundFetchDelegateProxy::Core::OnUIActivated(
    const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::DidActivateUI, parent_,
                     job_unique_id));
}

void BackgroundFetchDelegateProxy::Core::OnUIUpdated(
    const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::DidUpdateUI, parent_,
                     job_unique_id));
}

void BackgroundFetchDelegateProxy::Core::GetUploadData(
    const std::string& job_unique_id,
    const std::string& download_guid,
    BackgroundFetchDelegate::GetUploadDataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Pass this to the core thread for processing, but wrap |callback|
  // to be posted back to the UI thread when executed.
  BackgroundFetchDelegate::GetUploadDataCallback wrapped_callback =
      base::BindOnce(
          [](BackgroundFetchDelegate::GetUploadDataCallback callback,
             blink::mojom::SerializedBlobPtr blob) {
            base::PostTask(
                FROM_HERE, {BrowserThread::UI},
                base::BindOnce(std::move(callback), std::move(blob)));
          },
          std::move(callback));

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(&BackgroundFetchDelegateProxy::GetUploadData, parent_,
                     job_unique_id, download_guid,
                     std::move(wrapped_callback)));
}

BackgroundFetchDelegateProxy::BackgroundFetchDelegateProxy(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Normally it would be unsafe to obtain a weak pointer on the UI thread from
  // a factory that lives on the service worker core thread, but it's ok in the
  // constructor as |this| can't be destroyed before the constructor finishes.
  ui_core_.reset(new Core(weak_ptr_factory_.GetWeakPtr(), browser_context));

  // Since this constructor runs on the UI thread, a WeakPtr can be safely
  // obtained from the Core.
  ui_core_ptr_ = ui_core_->GetWeakPtrOnUI();
}

BackgroundFetchDelegateProxy::~BackgroundFetchDelegateProxy() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void BackgroundFetchDelegateProxy::SetClickEventDispatcher(
    DispatchClickEventCallback callback) {
  click_event_dispatcher_callback_ = std::move(callback);
}

void BackgroundFetchDelegateProxy::GetIconDisplaySize(
    BackgroundFetchDelegate::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&Core::GetIconDisplaySize, ui_core_ptr_,
                                       std::move(callback)));
}

void BackgroundFetchDelegateProxy::GetPermissionForOrigin(
    const url::Origin& origin,
    const WebContents::Getter& wc_getter,
    BackgroundFetchDelegate::GetPermissionForOriginCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&Core::GetPermissionForOrigin, ui_core_ptr_, origin,
                     wc_getter, std::move(callback)));
}

void BackgroundFetchDelegateProxy::CreateDownloadJob(
    base::WeakPtr<Controller> controller,
    std::unique_ptr<BackgroundFetchDescription> fetch_description) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  DCHECK(!controller_map_.count(fetch_description->job_unique_id));
  controller_map_[fetch_description->job_unique_id] = std::move(controller);

  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&Core::CreateDownloadJob, ui_core_ptr_,
                                       std::move(fetch_description)));
}

void BackgroundFetchDelegateProxy::StartRequest(
    const std::string& job_unique_id,
    const url::Origin& origin,
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  DCHECK(controller_map_.count(job_unique_id));
  DCHECK(request);
  DCHECK(!request->download_guid().empty());

  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&Core::StartRequest, ui_core_ptr_,
                                       job_unique_id, origin, request));
}

void BackgroundFetchDelegateProxy::UpdateUI(
    const std::string& job_unique_id,
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon,
    blink::mojom::BackgroundFetchRegistrationService::UpdateUICallback
        update_ui_callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  DCHECK(!update_ui_callback_map_.count(job_unique_id));
  update_ui_callback_map_.emplace(job_unique_id, std::move(update_ui_callback));

  RunOrPostTaskOnThread(FROM_HERE, BrowserThread::UI,
                        base::BindOnce(&Core::UpdateUI, ui_core_ptr_,
                                       job_unique_id, title, icon));
}

void BackgroundFetchDelegateProxy::Abort(const std::string& job_unique_id) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&Core::Abort, ui_core_ptr_, job_unique_id));
}

void BackgroundFetchDelegateProxy::MarkJobComplete(
    const std::string& job_unique_id) {
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&Core::MarkJobComplete, ui_core_ptr_, job_unique_id));
  controller_map_.erase(job_unique_id);
}

void BackgroundFetchDelegateProxy::OnJobCancelled(
    const std::string& job_unique_id,
    blink::mojom::BackgroundFetchFailureReason reason_to_abort) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(
      reason_to_abort ==
          blink::mojom::BackgroundFetchFailureReason::CANCELLED_FROM_UI ||
      reason_to_abort ==
          blink::mojom::BackgroundFetchFailureReason::DOWNLOAD_TOTAL_EXCEEDED);

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second)
    controller->AbortFromDelegate(reason_to_abort);
}

void BackgroundFetchDelegateProxy::DidStartRequest(
    const std::string& job_unique_id,
    const std::string& guid,
    std::unique_ptr<BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  auto it = controller_map_.find(job_unique_id);
  if (it == controller_map_.end())
    return;

  if (const auto& controller = it->second)
    controller->DidStartRequest(guid, std::move(response));
}

void BackgroundFetchDelegateProxy::DidActivateUI(
    const std::string& job_unique_id) {
  DCHECK(click_event_dispatcher_callback_);
  click_event_dispatcher_callback_.Run(job_unique_id);
}

void BackgroundFetchDelegateProxy::DidUpdateUI(
    const std::string& job_unique_id) {
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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

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

}  // namespace content
