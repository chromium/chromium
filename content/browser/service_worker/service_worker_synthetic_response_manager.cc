// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"

#include <cstddef>
#include <numeric>
#include <string>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

namespace {

constexpr char kHistogramIsHeaderConsistent[] =
    "ServiceWorker.SyntheticResponse.IsHeaderConsistent";
constexpr char kHistogramIsHeaderStored[] =
    "ServiceWorker.SyntheticResponse.IsHeaderStored";
constexpr char kHistogramSyntheticResponseReloadReason[] =
    "ServiceWorker.SyntheticResponse.ReloadReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyntheticResponseReloadReason)
enum class SyntheticResponseReloadReason {
  kCachedResponseHeadCleared = 0,
  kHeaderInconsistent = 1,
  kRedirect = 2,
  kMaxValue = kRedirect,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/service/enums.xml:SyntheticResponseReloadReason)

// When this is enabled, the browser stores response headers for synthetic
// responses even if there is no opt-in header in its response. This is for
// local development and testing.
BASE_FEATURE(kServiceWorkerBypassSyntheticResponseHeaderCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string>
    kServiceWorkerBypassSyntheticResponseIgnoredHeaders{
        &kServiceWorkerBypassSyntheticResponseHeaderCheck,
        "ignored_headers_for_bypass", ""};

bool IsBypassSyntheticResponseHeaderCheckEnabled() {
  static const bool kIsEnabled = base::FeatureList::IsEnabled(
      kServiceWorkerBypassSyntheticResponseHeaderCheck);
  return kIsEnabled;
}

const std::string& GetIgnoredHeadersForBypass() {
  static const base::NoDestructor<std::string> ignored_headers(
      kServiceWorkerBypassSyntheticResponseIgnoredHeaders.Get());
  return *ignored_headers;
}

const base::flat_set<std::string>& GetIgnoredHeadersForSyntheticResponse() {
  static const base::NoDestructor<base::flat_set<std::string>>
      ignored_headers_set([]() {
        const std::string ignored_headers_str(
            blink::features::kServiceWorkerSyntheticResponseIgnoredHeaders
                .Get());
        const std::vector<std::string_view> ignored_headers_sv =
            base::SplitStringPiece(ignored_headers_str, ",",
                                   base::TRIM_WHITESPACE,
                                   base::SPLIT_WANT_NONEMPTY);
        return base::flat_set<std::string>(ignored_headers_sv.begin(),
                                           ignored_headers_sv.end());
      }());
  return *ignored_headers_set;
}

void RecordReloadReason(SyntheticResponseReloadReason reason) {
  base::UmaHistogramEnumeration(kHistogramSyntheticResponseReloadReason,
                                reason);
}

bool ShouldReportInconsistentHeader() {
  static const bool report_inconsistent_header(
      blink::features::kServiceWorkerSyntheticResponseReportInconsistentHeader
          .Get());
  return report_inconsistent_header;
}

void MaybeReportHeaderInconsistency(
    const base::flat_map<std::string, std::multiset<std::string>>&
        incoming_headers,
    const base::flat_map<std::string, std::multiset<std::string>>&
        stored_headers) {
  if (!ShouldReportInconsistentHeader()) {
    return;
  }
  auto to_string = [&](const std::multiset<std::string>& values) {
    return std::accumulate(std::begin(values), std::end(values), std::string{},
                           [](const std::string& a, const std::string& b) {
                             return a.empty() ? b : a + ',' + b;
                           });
  };
  for (const auto& item : stored_headers) {
    if (!incoming_headers.contains(item.first)) {
      // The header doesn't exist.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NoHeader", item.first);
      base::debug::DumpWithoutCrashing();
      continue;
    }
    if (incoming_headers.at(item.first) != item.second) {
      // The header value is wrong.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "WrongHeader",
                                 item.first);
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "IncomingValue",
                                 to_string(incoming_headers.at(item.first)));
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "StoredValue",
                                 to_string(item.second));
      base::debug::DumpWithoutCrashing();
      continue;
    }
  }
  for (const auto& item : incoming_headers) {
    if (!stored_headers.contains(item.first)) {
      // Unexpected header exists.
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NotExpectedHeader",
                                 item.first);
      SCOPED_CRASH_KEY_STRING256("SyntheticResponse", "NotExpectedValue",
                                 to_string(item.second));
      base::debug::DumpWithoutCrashing();
    }
  }
}
}  // namespace

namespace content {
namespace {
constexpr std::string_view kOptInHeaderName =
    "Service-Worker-Synthetic-Response";
constexpr std::string_view kOptInHeaderValue = "?1";

// Convert `network::mojom::URLResponseHead` to
// `blink::mojom::FetchAPIResponse`.
//
// TODO(crbug.com/352578800): Ensure converted fields are really sufficient.
blink::mojom::FetchAPIResponsePtr GetFetchAPIResponse(
    const network::mojom::URLResponseHead& head) {
  CHECK(IsBypassSyntheticResponseHeaderCheckEnabled() ||
        head.headers->HasHeaderValue(kOptInHeaderName, kOptInHeaderValue));
  auto out_response = blink::mojom::FetchAPIResponse::New();
  out_response->status_code = net::HTTP_OK;
  out_response->response_time = base::Time::Now();
  out_response->url_list = head.url_list_via_service_worker;
  out_response->response_type = head.response_type;
  out_response->padding = head.padding;
  out_response->mime_type.emplace(head.mime_type);
  out_response->response_source = head.service_worker_response_source;
  out_response->cache_storage_cache_name.emplace(head.cache_storage_cache_name);
  out_response->cors_exposed_header_names = head.cors_exposed_header_names;
  out_response->parsed_headers = head.parsed_headers.Clone();
  out_response->connection_info = head.connection_info;
  out_response->alpn_negotiated_protocol = head.alpn_negotiated_protocol;
  out_response->was_fetched_via_spdy = head.was_fetched_via_spdy;
  out_response->has_range_requested = head.has_range_requested;
  out_response->auth_challenge_info = head.auth_challenge_info;

  // Parse headers.
  size_t iter = 0;
  std::string header_name;
  std::string header_value;
  while (
      head.headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
    if (out_response->headers.contains(header_name)) {
      // TODO(crbug.com/352578800): Confirm if other headers work with comma
      // separated values. We need to handle multiple Accept-CH headers, but
      // `headers` in FetchAPIResponse is a string based key-value map. So we
      // make multiple header values into one comma separated string.
      header_value =
          base::StrCat({out_response->headers[header_name], ",", header_value});
    }
    out_response->headers[header_name] = header_value;
  }

  return out_response;
}
}  // namespace

class ServiceWorkerSyntheticResponseManager::SyntheticResponseURLLoaderClient
    : public network::mojom::URLLoaderClient {
 public:
  SyntheticResponseURLLoaderClient(
      OnReceiveResponseCallback receive_response_callback,
      OnReceiveRedirectCallback receive_redirect_callback,
      OnCompleteCallback complete_callback)
      : receive_response_callback_(std::move(receive_response_callback)),
        receive_redirect_callback_(std::move(receive_redirect_callback)),
        complete_callback_(std::move(complete_callback)) {}
  SyntheticResponseURLLoaderClient(const SyntheticResponseURLLoaderClient&) =
      delete;
  SyntheticResponseURLLoaderClient& operator=(
      const SyntheticResponseURLLoaderClient&) = delete;
  void Bind(mojo::PendingRemote<network::mojom::URLLoaderClient>* remote) {
    receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
  }

 private:
  // network::mojom::URLLoaderClient implementation:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    std::move(receive_response_callback_)
        .Run(std::move(response_head), std::move(body));
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    std::move(receive_redirect_callback_)
        .Run(redirect_info, std::move(response_head));
  }
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    std::move(complete_callback_).Run(status);
  }

  OnReceiveResponseCallback receive_response_callback_;
  OnReceiveRedirectCallback receive_redirect_callback_;
  OnCompleteCallback complete_callback_;

  mojo::Receiver<network::mojom::URLLoaderClient> receiver_{this};
};

ServiceWorkerSyntheticResponseManager::ServiceWorkerSyntheticResponseManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<ServiceWorkerVersion> version)
    : url_loader_factory_(url_loader_factory), version_(version) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::"
              "ServiceWorkerSyntheticResponseManager");
  write_buffer_manager_.emplace();
  status_ = write_buffer_manager_->is_data_pipe_created() &&
                    version_->GetResponseHeadForSyntheticResponse()
                ? SyntheticResponseStatus::kReady
                : SyntheticResponseStatus::kNotReady;
}

ServiceWorkerSyntheticResponseManager::
    ~ServiceWorkerSyntheticResponseManager() = default;

void ServiceWorkerSyntheticResponseManager::StartRequest(
    int request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    OnReceiveResponseCallback receive_response_callback,
    OnReceiveRedirectCallback receive_redirect_callback,
    OnCompleteCallback complete_callback) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::StartRequest");
  CHECK(!request.client_side_content_decoding_enabled);
  response_callback_ = std::move(receive_response_callback);
  redirect_callback_ = std::move(receive_redirect_callback);
  complete_callback_ = std::move(complete_callback);
  client_ = std::make_unique<SyntheticResponseURLLoaderClient>(
      base::BindRepeating(
          &ServiceWorkerSyntheticResponseManager::OnReceiveResponse,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &ServiceWorkerSyntheticResponseManager::OnReceiveRedirect,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(&ServiceWorkerSyntheticResponseManager::OnComplete,
                     weak_factory_.GetWeakPtr()));
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_to_pass;
  client_->Bind(&client_to_pass);
  // TODO(crbug.com/352578800): Consider updating `request` with mode:
  // same-origin and redirect_mode: error to avoid concerns around origin
  // boundaries.
  //
  // TODO(crbug.com/352578800): Create and use own traffic_annotation tag.
  url_loader_factory_->CreateLoaderAndStart(
      url_loader_.InitWithNewPipeAndPassReceiver(), request_id, options,
      request, std::move(client_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          ServiceWorkerRaceNetworkRequestURLLoaderClient::
              NetworkTrafficAnnotationTag()));
}

void ServiceWorkerSyntheticResponseManager::StartSyntheticResponse(
    FetchCallback callback) {
  CHECK_EQ(status_, SyntheticResponseStatus::kReady);
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::StartSyntheticResponse");
  const auto& response_head = version_->GetResponseHeadForSyntheticResponse();
  CHECK(response_head);
  blink::mojom::FetchAPIResponsePtr response =
      GetFetchAPIResponse(*response_head);
  CHECK(response);
  auto stream_handle = blink::mojom::ServiceWorkerStreamHandle::New();
  stream_handle->stream = write_buffer_manager_->ReleaseConsumerHandle();
  stream_handle->callback_receiver =
      stream_callback_.BindNewPipeAndPassReceiver();
  auto timing = blink::mojom::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = base::TimeTicks::Now();
  std::move(callback).Run(
      blink::ServiceWorkerStatusCode::kOk,
      ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse,
      std::move(response), std::move(stream_handle), std::move(timing),
      version_);
  did_start_synthetic_response = true;
}

void ServiceWorkerSyntheticResponseManager::MaybeSetResponseHead(
    const network::mojom::URLResponseHead& response_head) {
  bool is_header_stored = false;
  // If the response is not successful or there is no opt-in header, do not
  // update the response head.
  if (network::IsSuccessfulStatus(response_head.headers->response_code()) &&
      (IsBypassSyntheticResponseHeaderCheckEnabled() ||
       response_head.headers->HasHeaderValue(kOptInHeaderName,
                                             kOptInHeaderValue))) {
    version_->SetMainScriptResponse(
        std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
            response_head));
    version_->SetResponseHeadForSyntheticResponse(response_head.Clone());
    is_header_stored = true;
  }
  base::UmaHistogramBoolean(kHistogramIsHeaderStored, is_header_stored);
}

void ServiceWorkerSyntheticResponseManager::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnReceiveResponse");
  switch (status_) {
    case SyntheticResponseStatus::kReady: {
      CHECK(write_buffer_manager_.has_value());
      bool is_header_consistent = false;
      if (version_->GetResponseHeadForSyntheticResponse()) {
        is_header_consistent = CheckHeaderConsistency(response_head->headers);
        if (is_header_consistent) {
          simple_buffer_manager_.emplace(std::move(body));
          simple_buffer_manager_->Clone(
              write_buffer_manager_->ReleaseProducerHandle(),
              base::BindOnce(
                  &ServiceWorkerSyntheticResponseManager::OnCloneCompleted,
                  weak_factory_.GetWeakPtr()));
        } else {
          // Clear the stored header when it's inconsistent with the header from
          // the network so that the next navigation won't get the header
          // mismatch and reloading consistently.
          //
          // TODO(crbug.com/352578800): Consider setting the response header
          // here rather than resetting it in order to improve the synthetic
          // response coverage. Revisit this after collecting coverage data.
          version_->ResetResponseHeadForSyntheticResponse();
          NotifyReloading();
          RecordReloadReason(
              SyntheticResponseReloadReason::kHeaderInconsistent);
        }
      } else {
        // The cached response head may have been cleared by another request
        // that detected a header inconsistency. Tell the client to reload to
        // get the latest version.
        NotifyReloading();
        RecordReloadReason(
            SyntheticResponseReloadReason::kCachedResponseHeadCleared);
      }
      base::UmaHistogramBoolean(kHistogramIsHeaderConsistent,
                                is_header_consistent);
      break;
    }
    case SyntheticResponseStatus::kNotReady:
      MaybeSetResponseHead(*response_head);
      std::move(response_callback_)
          .Run(std::move(response_head), std::move(body));
      break;
  }
}

void ServiceWorkerSyntheticResponseManager::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  if (did_start_synthetic_response) {
    // If the response is already returned from the stored data, that means the
    // renderer may already have received `OnReceiveResponse`. Sending
    // `OnReceiveRedirect` after `OnReceiveResponse` brings errors in that case.
    // Instead, we reload the navigation as a fallback. In the next navigation,
    // the synthetic response is not enabled because it's a reload navigation.
    version_->ResetResponseHeadForSyntheticResponse();
    NotifyReloading();
    RecordReloadReason(SyntheticResponseReloadReason::kRedirect);
    return;
  }
  std::move(redirect_callback_).Run(redirect_info, std::move(response_head));
}

void ServiceWorkerSyntheticResponseManager::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnComplete");
  std::move(complete_callback_).Run(status);
}

void ServiceWorkerSyntheticResponseManager::OnCloneCompleted() {
  write_buffer_manager_->ResetProducer();
  CHECK(stream_callback_);
  // Perhaps this assumption is wrong because the write operation may not be
  // completed at the timing of when the read buffer is empty.
  stream_callback_->OnCompleted();
}

bool ServiceWorkerSyntheticResponseManager::CheckHeaderConsistency(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  const auto& response_head = version_->GetResponseHeadForSyntheticResponse();
  CHECK(response_head);
  base::flat_set<std::string> ignored_headers =
      GetIgnoredHeadersForSyntheticResponse();
  if (IsBypassSyntheticResponseHeaderCheckEnabled()) {
    const std::string& ignored_headers_str = GetIgnoredHeadersForBypass();
    std::vector<std::string_view> testing_ignored_headers =
        base::SplitStringPiece(ignored_headers_str, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    for (const auto& header : testing_ignored_headers) {
      ignored_headers.insert(base::ToLowerASCII(header));
    }
  }
  auto collect_significant_headers =
      [&](const net::HttpResponseHeaders& headers) {
        base::flat_map<std::string, std::multiset<std::string>> collected;
        size_t iter = 0;
        std::string name;
        std::string value;
        while (headers.EnumerateHeaderLines(&iter, &name, &value)) {
          if (!ignored_headers.contains(base::ToLowerASCII(name))) {
            collected[name].insert(value);
          }
        }
        return collected;
      };
  auto incoming_headers = collect_significant_headers(*headers);
  auto stored_headers = collect_significant_headers(*response_head->headers);

  bool result = incoming_headers == stored_headers;
  if (!result) {
    MaybeReportHeaderInconsistency(incoming_headers, stored_headers);
  }

  return result;
}

void ServiceWorkerSyntheticResponseManager::NotifyReloading() {
  auto body_for_reload = base::span_from_cstring<const char>(
      "<meta http-equiv=\"refresh\" content=\"0;\" />");
  auto [result, size] = write_buffer_manager_->WriteData(body_for_reload);
  if (result != MOJO_RESULT_OK) {
    return;
  }
  OnCloneCompleted();
}

bool ServiceWorkerSyntheticResponseManager::dry_run_mode_for_testing_ = false;
void ServiceWorkerSyntheticResponseManager::SetDryRunMode(bool enabled) {
  dry_run_mode_for_testing_ = enabled;
}
bool ServiceWorkerSyntheticResponseManager::IsDryRunModeEnabledForTesting() {
  return dry_run_mode_for_testing_;
}

}  // namespace content
