// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include <memory>

#include "base/guid.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_metrics.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/bad_message.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

// Maximum length of a developer-provided |developer_id| for a Background Fetch.
constexpr size_t kMaxDeveloperIdLength = 1024 * 1024;

// Maximum length of a developer-provided title for a Background Fetch.
constexpr size_t kMaxTitleLength = 1024 * 1024;

}  // namespace

// static
void BackgroundFetchServiceImpl::CreateForWorker(
    blink::mojom::BackgroundFetchServiceRequest request,
    RenderProcessHost* render_process_host,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          BackgroundFetchServiceImpl::CreateOnIoThread,
          WrapRefCounted(static_cast<StoragePartitionImpl*>(
                             render_process_host->GetStoragePartition())
                             ->GetBackgroundFetchContext()),
          origin, nullptr /* render_frame_host */, std::move(request)));
}

// static
void BackgroundFetchServiceImpl::CreateForFrame(
    RenderProcessHost* render_process_host,
    int render_frame_id,
    blink::mojom::BackgroundFetchServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(render_process_host);
  auto* render_frame_host =
      RenderFrameHost::FromID(render_process_host->GetID(), render_frame_id);
  DCHECK(render_frame_host);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          BackgroundFetchServiceImpl::CreateOnIoThread,
          WrapRefCounted(static_cast<StoragePartitionImpl*>(
                             render_process_host->GetStoragePartition())
                             ->GetBackgroundFetchContext()),
          render_frame_host->GetLastCommittedOrigin(), render_frame_host,
          std::move(request)));
}

// static
void BackgroundFetchServiceImpl::CreateOnIoThread(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    url::Origin origin,
    RenderFrameHost* render_frame_host,
    blink::mojom::BackgroundFetchServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::MakeStrongBinding(std::make_unique<BackgroundFetchServiceImpl>(
                              std::move(background_fetch_context),
                              std::move(origin), render_frame_host),
                          std::move(request));
}

BackgroundFetchServiceImpl::BackgroundFetchServiceImpl(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    url::Origin origin,
    RenderFrameHost* render_frame_host)
    : background_fetch_context_(std::move(background_fetch_context)),
      origin_(std::move(origin)),
      render_frame_host_(render_frame_host) {
  DCHECK(background_fetch_context_);
}

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchServiceImpl::Fetch(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateDeveloperId(developer_id) || !ValidateRequests(requests)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    background_fetch::RecordRegistrationCreatedError(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  // New |unique_id|, since this is a new Background Fetch registration. This is
  // the only place new |unique_id|s should be created outside of tests.
  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin_, developer_id,
                                                base::GenerateGUID());

  background_fetch_context_->StartFetch(
      registration_id, requests, options, icon, std::move(ukm_data),
      render_frame_host_, std::move(callback));
}

void BackgroundFetchServiceImpl::GetIconDisplaySize(
    blink::mojom::BackgroundFetchService::GetIconDisplaySizeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  background_fetch_context_->GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchServiceImpl::MatchRequests(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    const std::string& unique_id,
    const base::Optional<ServiceWorkerFetchRequest>& request_to_match,
    blink::mojom::QueryParamsPtr cache_query_params,
    bool match_all,
    MatchRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, origin_, developer_id, unique_id);

  // Create BackgroundFetchMatchRequestParams.
  auto match_params = std::make_unique<BackgroundFetchRequestMatchParams>(
      request_to_match, std::move(cache_query_params), match_all);

  background_fetch_context_->MatchRequests(
      registration_id, std::move(match_params), std::move(callback));
}

void BackgroundFetchServiceImpl::UpdateUI(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    const std::string& unique_id,
    const base::Optional<std::string>& title,
    const SkBitmap& icon,
    UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!ValidateUniqueId(unique_id) || (title && !ValidateTitle(*title))) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchRegistrationId registration_id(
      service_worker_registration_id, origin_, developer_id, unique_id);

  // Wrap the icon in an optional for clarity.
  auto optional_icon =
      icon.isNull() ? base::nullopt : base::Optional<SkBitmap>(icon);

  background_fetch_context_->UpdateUI(registration_id, title, optional_icon,
                                      std::move(callback));
}

void BackgroundFetchServiceImpl::Abort(int64_t service_worker_registration_id,
                                       const std::string& developer_id,
                                       const std::string& unique_id,
                                       AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateDeveloperId(developer_id) || !ValidateUniqueId(unique_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  background_fetch_context_->Abort(
      BackgroundFetchRegistrationId(service_worker_registration_id, origin_,
                                    developer_id, unique_id),
      std::move(callback));
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateDeveloperId(developer_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    return;
  }

  background_fetch_context_->GetRegistration(service_worker_registration_id,
                                             origin_, developer_id,
                                             std::move(callback));
}

void BackgroundFetchServiceImpl::GetDeveloperIds(
    int64_t service_worker_registration_id,
    GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  background_fetch_context_->GetDeveloperIdsForServiceWorker(
      service_worker_registration_id, origin_, std::move(callback));
}

void BackgroundFetchServiceImpl::AddRegistrationObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateUniqueId(unique_id))
    return;

  background_fetch_context_->AddRegistrationObserver(unique_id,
                                                     std::move(observer));
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
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  if (requests.empty()) {
    mojo::ReportBadMessage("Invalid requests");
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateTitle(const std::string& title) {
  if (title.empty() || title.size() > kMaxTitleLength) {
    mojo::ReportBadMessage("Invalid title");
    return false;
  }

  return true;
}

}  // namespace content
