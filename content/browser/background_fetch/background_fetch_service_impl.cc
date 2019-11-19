// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_registration_notifier.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/bad_message.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace content {

namespace {
// Maximum length of a developer-provided |developer_id| for a Background Fetch.
constexpr size_t kMaxDeveloperIdLength = 1024 * 1024;
}  // namespace

// static
void BackgroundFetchServiceImpl::CreateForWorker(
    const ServiceWorkerVersionInfo& info,
    mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(info.process_id);

  if (!render_process_host)
    return;

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          BackgroundFetchServiceImpl::CreateOnCoreThread,
          WrapRefCounted(static_cast<StoragePartitionImpl*>(
                             render_process_host->GetStoragePartition())
                             ->GetBackgroundFetchContext()),
          info.script_origin,
          /* render_frame_tree_node_id= */ 0,
          /* wc_getter= */ base::NullCallback(), std::move(receiver)));
}

// static
void BackgroundFetchServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(render_frame_host);
  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  DCHECK(render_process_host);

  WebContents::Getter wc_getter = base::NullCallback();

  // Permissions need to go through the DownloadRequestLimiter if the fetch
  // is started from a top-level frame.
  if (!render_frame_host->GetParent()) {
    wc_getter = base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                                    render_frame_host->GetFrameTreeNodeId());
  }

  RunOrPostTaskOnThread(
      FROM_HERE, ServiceWorkerContext::GetCoreThreadId(),
      base::BindOnce(
          BackgroundFetchServiceImpl::CreateOnCoreThread,
          WrapRefCounted(static_cast<StoragePartitionImpl*>(
                             render_process_host->GetStoragePartition())
                             ->GetBackgroundFetchContext()),
          render_frame_host->GetLastCommittedOrigin(),
          render_frame_host->GetFrameTreeNodeId(), std::move(wc_getter),
          std::move(receiver)));
}

// static
void BackgroundFetchServiceImpl::CreateOnCoreThread(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    url::Origin origin,
    int render_frame_tree_node_id,
    WebContents::Getter wc_getter,
    mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<BackgroundFetchServiceImpl>(
          std::move(background_fetch_context), std::move(origin),
          render_frame_tree_node_id, std::move(wc_getter)),
      std::move(receiver));
}

BackgroundFetchServiceImpl::BackgroundFetchServiceImpl(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    url::Origin origin,
    int render_frame_tree_node_id,
    WebContents::Getter wc_getter)
    : background_fetch_context_(std::move(background_fetch_context)),
      origin_(std::move(origin)),
      render_frame_tree_node_id_(render_frame_tree_node_id),
      wc_getter_(std::move(wc_getter)) {
  DCHECK(background_fetch_context_);
}

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
}

void BackgroundFetchServiceImpl::Fetch(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    std::vector<blink::mojom::FetchAPIRequestPtr> requests,
    blink::mojom::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  if (!ValidateDeveloperId(developer_id) || !ValidateRequests(requests)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        /* registration= */ nullptr);
    return;
  }

  // New |unique_id|, since this is a new Background Fetch registration. This is
  // the only place new |unique_id|s should be created outside of tests.
  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin_, developer_id,
                                                base::GenerateGUID());

  background_fetch_context_->StartFetch(
      registration_id, std::move(requests), std::move(options), icon,
      std::move(ukm_data), render_frame_tree_node_id_, wc_getter_,
      std::move(callback));
}

void BackgroundFetchServiceImpl::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  background_fetch_context_->GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  if (!ValidateDeveloperId(developer_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        /* registration= */ nullptr);
    return;
  }

  background_fetch_context_->GetRegistration(service_worker_registration_id,
                                             origin_, developer_id,
                                             std::move(callback));
}

void BackgroundFetchServiceImpl::GetDeveloperIds(
    int64_t service_worker_registration_id,
    GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  background_fetch_context_->GetDeveloperIdsForServiceWorker(
      service_worker_registration_id, origin_, std::move(callback));
}

bool BackgroundFetchServiceImpl::ValidateDeveloperId(
    const std::string& developer_id) {
  if (developer_id.empty() || developer_id.size() > kMaxDeveloperIdLength) {
    mojo::ReportBadMessage("Invalid developer_id");
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateUniqueId(
    const std::string& unique_id) {
  if (!base::IsValidGUIDOutputString(unique_id)) {
    mojo::ReportBadMessage("Invalid unique_id");
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateRequests(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& requests) {
  if (requests.empty()) {
    mojo::ReportBadMessage("Invalid requests");
    return false;
  }

  return true;
}

}  // namespace content
