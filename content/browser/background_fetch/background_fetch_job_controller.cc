// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/public/common/origin_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

#include <utility>

#include "content/public/browser/browser_thread.h"

namespace content {

using blink::mojom::BackgroundFetchFailureReason;

BackgroundFetchJobController::BackgroundFetchJobController(
    BackgroundFetchDelegateProxy* delegate_proxy,
    BackgroundFetchScheduler* scheduler,
    const BackgroundFetchRegistrationId& registration_id,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    uint64_t bytes_downloaded,
    ProgressCallback progress_callback,
    BackgroundFetchScheduler::FinishedCallback finished_callback)
    : BackgroundFetchScheduler::Controller(scheduler,
                                           registration_id,
                                           std::move(finished_callback)),
      options_(options),
      icon_(icon),
      complete_requests_downloaded_bytes_cache_(bytes_downloaded),
      delegate_proxy_(delegate_proxy),
      progress_callback_(std::move(progress_callback)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchJobController::InitializeRequestStatus(
    int completed_downloads,
    int total_downloads,
    std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
        active_fetch_requests,
    const std::string& ui_title,
    bool start_paused) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't allow double initialization.
  DCHECK_GT(total_downloads, 0);
  DCHECK_EQ(total_downloads_, 0);

  outstanding_requests_ = active_fetch_requests;
  completed_downloads_ = completed_downloads;
  total_downloads_ = total_downloads;

  // TODO(nator): Update this when we support uploads.
  total_downloads_size_ = options_.download_total;

  std::vector<std::string> active_guids;
  active_guids.reserve(active_fetch_requests.size());
  for (const auto& request_info : active_fetch_requests)
    active_guids.push_back(request_info->download_guid());

  auto fetch_description = std::make_unique<BackgroundFetchDescription>(
      registration_id().unique_id(), ui_title, registration_id().origin(),
      icon_, completed_downloads, total_downloads,
      complete_requests_downloaded_bytes_cache_, total_downloads_size_,
      std::move(active_guids), start_paused);

  delegate_proxy_->CreateDownloadJob(GetWeakPtr(), std::move(fetch_description),
                                     std::move(active_fetch_requests));
}

BackgroundFetchJobController::~BackgroundFetchJobController() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool BackgroundFetchJobController::HasMoreRequests() {
  return completed_downloads_ < total_downloads_;
}

bool BackgroundFetchJobController::IsMixedContent(
    const BackgroundFetchRequestInfo& request) {
  // Empty request is valid, it shouldn't fail the mixed content check.
  if (request.fetch_request().url.is_empty())
    return false;

  return !IsOriginSecure(request.fetch_request().url);
}

bool BackgroundFetchJobController::RequiresCORSPreflight(
    const BackgroundFetchRequestInfo& request) {
  auto fetch_request = request.fetch_request();

  // Same origin requests don't require a CORS preflight.
  // https://fetch.spec.whatwg.org/#main-fetch
  // TODO(crbug.com/711354): Make sure that cross-origin redirects are disabled.
  if (url::IsSameOriginWith(registration_id().origin().GetURL(),
                            fetch_request.url)) {
    return false;
  }

  // Requests that are more involved than what is possible with HTML's form
  // element require a CORS-preflight request.
  // https://fetch.spec.whatwg.org/#main-fetch
  if (!fetch_request.method.empty() &&
      !network::cors::IsCORSSafelistedMethod(fetch_request.method)) {
    return true;
  }

  net::HttpRequestHeaders::HeaderVector headers;
  for (const auto& header : fetch_request.headers)
    headers.emplace_back(header.first, header.second);
  return !network::cors::CORSUnsafeRequestHeaderNames(headers).empty();
}

void BackgroundFetchJobController::StartRequest(
    scoped_refptr<BackgroundFetchRequestInfo> request,
    RequestFinishedCallback request_finished_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LT(completed_downloads_, total_downloads_);
  DCHECK(request_finished_callback);
  DCHECK(request);

  active_request_downloaded_bytes_ = 0;
  active_request_finished_callback_ = std::move(request_finished_callback);

  if (IsMixedContent(*request.get()) || RequiresCORSPreflight(*request.get())) {
    request->SetEmptyResultWithFailureReason(
        BackgroundFetchResult::FailureReason::FETCH_ERROR);

    ++completed_downloads_;
    std::move(active_request_finished_callback_).Run(request);
    return;
  }

  delegate_proxy_->StartRequest(registration_id().unique_id(),
                                registration_id().origin(), request);
}

std::vector<scoped_refptr<BackgroundFetchRequestInfo>>
BackgroundFetchJobController::TakeOutstandingRequests() {
  return std::move(outstanding_requests_);
}

void BackgroundFetchJobController::DidStartRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(crbug.com/884672): Either add CORS check here or remove this function
  // and do the CORS check in BackgroundFetchDelegateImpl (since
  // download::Client::OnDownloadStarted returns a value that can abort the
  // download).
}

void BackgroundFetchJobController::DidUpdateRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (active_request_downloaded_bytes_ == bytes_downloaded)
    return;

  active_request_downloaded_bytes_ = bytes_downloaded;

  auto registration =
      NewRegistration(blink::mojom::BackgroundFetchResult::UNSET);
  registration->downloaded += GetInProgressDownloadedBytes();
  progress_callback_.Run(*registration);
}

void BackgroundFetchJobController::DidCompleteRequest(
    const scoped_refptr<BackgroundFetchRequestInfo>& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // It's possible for the DidCompleteRequest() callback to have been in-flight
  // while this Job Controller was being aborted, in which case the
  // |active_request_finished_callback_| will have been reset.
  if (!active_request_finished_callback_)
    return;

  active_request_downloaded_bytes_ = 0;

  complete_requests_downloaded_bytes_cache_ += request->GetFileSize();
  ++completed_downloads_;

  std::move(active_request_finished_callback_).Run(request);
}

void BackgroundFetchJobController::UpdateUI(
    const base::Optional<std::string>& title,
    const base::Optional<SkBitmap>& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  delegate_proxy_->UpdateUI(registration_id().unique_id(), title, icon);
}

std::unique_ptr<BackgroundFetchRegistration>
BackgroundFetchJobController::NewRegistration(
    blink::mojom::BackgroundFetchResult result) const {
  return std::make_unique<BackgroundFetchRegistration>(
      registration_id().developer_id(), registration_id().unique_id(),
      0 /* upload_total */, 0 /* uploaded */, total_downloads_size_,
      complete_requests_downloaded_bytes_cache_, result, failure_reason_);
}

uint64_t BackgroundFetchJobController::GetInProgressDownloadedBytes() {
  return active_request_downloaded_bytes_;
}

void BackgroundFetchJobController::Abort(
    BackgroundFetchFailureReason failure_reason) {
  failure_reason_ = failure_reason;

  // Stop propagating any in-flight events to the scheduler.
  active_request_finished_callback_.Reset();

  // Cancel any in-flight downloads and UI through the BGFetchDelegate.
  delegate_proxy_->Abort(registration_id().unique_id());

  Finish(failure_reason_);
}

BackgroundFetchFailureReason BackgroundFetchJobController::MergeFailureReason(
    BackgroundFetchFailureReason failure_reason) {
  if (failure_reason_ == BackgroundFetchFailureReason::NONE)
    failure_reason_ = failure_reason;
  return failure_reason_;
}

}  // namespace content
