// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include <memory>

#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace content {
namespace {

void RecordCookieCopyTimes(
    const base::TimeTicks& cookie_copy_start_time,
    const base::TimeTicks& cookie_read_end_and_write_start_time,
    const base::TimeTicks& cookie_copy_end_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      cookie_read_end_and_write_start_time - cookie_copy_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      cookie_copy_end_time - cookie_read_end_and_write_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      cookie_copy_end_time - cookie_copy_start_time, base::TimeDelta(),
      base::Seconds(5), 50);
}

}  // namespace

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      url_(url),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      prefetch_document_manager_(prefetch_document_manager),
      ukm_source_id_(prefetch_document_manager_
                         ? prefetch_document_manager_->render_frame_host()
                               .GetPageUkmSourceId()
                         : ukm::kInvalidSourceId),
      request_id_(base::UnguessableToken::Create().ToString()) {}

PrefetchContainer::~PrefetchContainer() {
  ukm::builders::PrefetchProxy_PrefetchedResource builder(ukm_source_id_);
  builder.SetResourceType(/*mainframe*/ 1);
  builder.SetStatus(static_cast<int>(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted)));
  builder.SetLinkClicked(navigated_to_);

  if (data_length_) {
    builder.SetDataLength(
        ukm::GetExponentialBucketMinForBytes(data_length_.value()));
  }

  if (fetch_duration_) {
    builder.SetFetchDurationMS(fetch_duration_->InMilliseconds());
  }

  if (probe_result_) {
    builder.SetISPFilteringStatus(static_cast<int>(probe_result_.value()));
  }

  // TODO(https://crbug.com/1299059): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());
}

PrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

void PrefetchContainer::TakeProxyLookupClient(
    std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client) {
  DCHECK(!proxy_lookup_client_);
  proxy_lookup_client_ = std::move(proxy_lookup_client);
}

std::unique_ptr<ProxyLookupClientImpl>
PrefetchContainer::ReleaseProxyLookupClient() {
  DCHECK(proxy_lookup_client_);
  return std::move(proxy_lookup_client_);
}

PrefetchNetworkContext* PrefetchContainer::GetOrCreateNetworkContext(
    PrefetchService* prefetch_service) {
  if (!network_context_) {
    network_context_ = std::make_unique<PrefetchNetworkContext>(
        prefetch_service, prefetch_type_, referrer_,
        referring_render_frame_host_id_);
  }
  return network_context_.get();
}

PrefetchDocumentManager* PrefetchContainer::GetPrefetchDocumentManager() const {
  return prefetch_document_manager_.get();
}

void PrefetchContainer::RegisterCookieListener(
    network::mojom::CookieManager* cookie_manager) {
  cookie_listener_ =
      PrefetchCookieListener::MakeAndRegister(url_, cookie_manager);
}

void PrefetchContainer::StopCookieListener() {
  if (cookie_listener_)
    cookie_listener_->StopListening();
}

bool PrefetchContainer::HaveDefaultContextCookiesChanged() const {
  if (cookie_listener_)
    return cookie_listener_->HaveCookiesChanged();
  return false;
}

bool PrefetchContainer::IsIsolatedCookieCopyInProgress() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
    case CookieCopyStatus::kCompleted:
      return false;
    case CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchContainer::OnIsolatedCookieCopyStart() {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We don't want the cookie listener for this URL to get the changes from the
  // copy.
  StopCookieListener();

  cookie_copy_status_ = CookieCopyStatus::kInProgress;

  cookie_copy_start_time_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookiesReadCompleteAndWriteStart() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  cookie_read_end_and_write_start_time_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookieCopyComplete() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  cookie_copy_status_ = CookieCopyStatus::kCompleted;

  if (cookie_copy_start_time_.has_value() &&
      cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(*cookie_copy_start_time_,
                          *cookie_read_end_and_write_start_time_,
                          base::TimeTicks::Now());
  }

  if (on_cookie_copy_complete_callback_)
    std::move(on_cookie_copy_complete_callback_).Run();
}

void PrefetchContainer::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());
  on_cookie_copy_complete_callback_ = std::move(callback);
}

void PrefetchContainer::TakeURLLoader(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  DCHECK(!loader_);
  loader_ = std::move(loader);
}

void PrefetchContainer::ResetURLLoader() {
  DCHECK(loader_);
  loader_.reset();
}

void PrefetchContainer::OnPrefetchProbeResult(
    PrefetchProbeResult probe_result) {
  probe_result_ = probe_result;

  switch (probe_result) {
    case PrefetchProbeResult::kNoProbing:
      prefetch_status_ = PrefetchStatus::kPrefetchUsedNoProbe;
      break;
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      prefetch_status_ = PrefetchStatus::kPrefetchUsedProbeSuccess;
      break;
    case PrefetchProbeResult::kDNSProbeFailure:
    case PrefetchProbeResult::kTLSProbeFailure:
      prefetch_status_ = PrefetchStatus::kPrefetchNotUsedProbeFailed;
      break;
  }
}

void PrefetchContainer::OnPrefetchComplete() {
  if (!loader_)
    return;
  UpdatePrefetchRequestMetrics(loader_->CompletionStatus(),
                               loader_->ResponseInfo());
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const absl::optional<network::URLLoaderCompletionStatus>& completion_status,
    const network::mojom::URLResponseHead* head) {
  if (completion_status)
    data_length_ = completion_status->encoded_data_length;

  if (head)
    header_latency_ =
        head->load_timing.receive_headers_end - head->load_timing.request_start;

  if (completion_status && head)
    fetch_duration_ =
        completion_status->completion_time - head->load_timing.request_start;
}

bool PrefetchContainer::HasValidPrefetchedResponse(
    base::TimeDelta cacheable_duration) const {
  return prefetched_response_ != nullptr &&
         prefetch_received_time_.has_value() &&
         base::TimeTicks::Now() <
             prefetch_received_time_.value() + cacheable_duration;
}

void PrefetchContainer::TakePrefetchedResponse(
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response) {
  DCHECK(!prefetched_response_);
  DCHECK(!is_decoy_);

  if (prefetch_document_manager_)
    prefetch_document_manager_->OnPrefetchSuccessful();

  prefetch_received_time_ = base::TimeTicks::Now();
  prefetched_response_ = std::move(prefetched_response);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ReleasePrefetchedResponse() {
  prefetch_received_time_.reset();
  return std::move(prefetched_response_);
}

}  // namespace content
