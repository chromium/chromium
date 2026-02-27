// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"

#include <cstddef>
#include <numeric>
#include <string>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_synthetic_response_data_pipe_connector.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "content/public/browser/global_request_id.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/synthetic_response_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace {

constexpr char kHistogramIsHeaderConsistent[] =
    "ServiceWorker.SyntheticResponse.IsHeaderConsistent";
constexpr char kHistogramIsHeaderStored[] =
    "ServiceWorker.SyntheticResponse.IsHeaderStored";
constexpr char kHistogramStartRequestToReceiveResponse[] =
    "ServiceWorker.SyntheticResponse.StartRequestToReceiveResponse";
constexpr char kHistogramReceiveResponseToComplete[] =
    "ServiceWorker.SyntheticResponse.ReceiveResponseToComplete";
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
  kIntercepted = 3,
  kMaxValue = kIntercepted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/service/enums.xml:SyntheticResponseReloadReason)

// When this is enabled, the browser stores response headers for synthetic
// responses even if there is no opt-in header in its response. This is for
// local development and testing.
BASE_FEATURE(kServiceWorkerBypassSyntheticResponseHeaderCheck,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsBypassSyntheticResponseHeaderCheckEnabled() {
  return base::FeatureList::IsEnabled(
      kServiceWorkerBypassSyntheticResponseHeaderCheck);
}

void RecordReloadReason(SyntheticResponseReloadReason reason) {
  base::UmaHistogramEnumeration(kHistogramSyntheticResponseReloadReason,
                                reason);
}

bool IsServiceWorkerSyntheticResponseOffMainThread() {
  return blink::features::kServiceWorkerSyntheticResponseOffMainThread.Get() ==
         blink::features::ServiceWorkerSyntheticResponseProcessingMode::
             kBackgroundThread;
}

bool IsServiceWorkerSyntheticResponseNetworkService() {
  // The `kNetworkService` mode relies on the network service to accept the
  // response body stream provided by the browser process. If
  // `kURLLoaderUseProvidedResponseBodyStream` is disabled, the network service
  // will ignore the provided stream, and the browser process will crash in
  // `OnReceiveResponse()` because the body handle is still valid.
  return blink::features::kServiceWorkerSyntheticResponseOffMainThread.Get() ==
             blink::features::ServiceWorkerSyntheticResponseProcessingMode::
                 kNetworkService &&
         base::FeatureList::IsEnabled(
             network::features::kURLLoaderUseProvidedResponseBodyStream);
}

bool IsServiceWorkerSyntheticResponseSkipUnnecessaryBuffering() {
  static const bool skip_unnecessary_buffering(
      blink::features::kServiceWorkerSyntheticResponseSkipUnnecessaryBuffering
          .Get());
  return skip_unnecessary_buffering;
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
    scoped_refptr<ServiceWorkerVersion> version)
    : version_(version) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::"
              "ServiceWorkerSyntheticResponseManager",
              perfetto::Flow::FromPointer(this));
  write_buffer_manager_.emplace();
  const auto& response_head = version_->GetResponseHeadForSyntheticResponse();
  status_ = write_buffer_manager_->is_data_pipe_created() && response_head &&
                    response_head->headers
                ? SyntheticResponseStatus::kReady
                : SyntheticResponseStatus::kNotReady;
}

ServiceWorkerSyntheticResponseManager::
    ~ServiceWorkerSyntheticResponseManager() {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::"
              "~ServiceWorkerSyntheticResponseManager",
              perfetto::TerminatingFlow::FromPointer(this));
}

void ServiceWorkerSyntheticResponseManager::InitiateRequest(
    ServiceWorkerClient* service_worker_client,
    StoragePartitionImpl* storage_partition,
    network::ResourceRequest& request,
    OnReceiveResponseCallback receive_response_callback,
    OnReceiveRedirectCallback receive_redirect_callback,
    OnCompleteCallback complete_callback) {
  url_loader_factory_ = service_worker_client->CreateNetworkURLLoaderFactory(
      ServiceWorkerClient::CreateNetworkURLLoaderFactoryType::
          kSyntheticNetworkRequest,
      storage_partition, request);

  StartRequest(
      GlobalRequestID::MakeBrowserInitiated().request_id,
      NavigationURLLoader::GetURLLoaderOptions(request.is_outermost_main_frame),
      request, std::move(receive_response_callback),
      std::move(receive_redirect_callback), std::move(complete_callback));
}

void ServiceWorkerSyntheticResponseManager::StartRequest(
    int request_id,
    uint32_t options,
    network::ResourceRequest& request,
    OnReceiveResponseCallback receive_response_callback,
    OnReceiveRedirectCallback receive_redirect_callback,
    OnCompleteCallback complete_callback) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::StartRequest",
              perfetto::Flow::FromPointer(this), "request_id", request_id,
              "url", request.url.spec());
  CHECK(!request.client_side_content_decoding_enabled);
  request_start_time_ = base::TimeTicks::Now();
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

  if (status_ == SyntheticResponseStatus::kReady &&
      IsServiceWorkerSyntheticResponseNetworkService()) {
    if (!request.trusted_params) {
      request.trusted_params = network::ResourceRequest::TrustedParams();
    }
    const auto& response_head = version_->GetResponseHeadForSyntheticResponse();
    CHECK(response_head);
    if (response_head->headers) {
      // Set headers and body stream to let the network service use the
      // synthetic response.
      request.trusted_params->expected_response_headers_for_synthetic_response =
          response_head->headers;
      shared_producer_ =
          base::MakeRefCounted<network::SharedDataPipeProducerHandle>(
              write_buffer_manager_->ReleaseProducerHandle());
      request.trusted_params->response_body_stream = shared_producer_;
    }
  }

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

bool ServiceWorkerSyntheticResponseManager::MaybeStartSyntheticResponse(
    FetchCallback callback) {
  if (status_ != SyntheticResponseStatus::kReady) {
    return false;
  }

  TRACE_EVENT(
      "ServiceWorker",
      "ServiceWorkerSyntheticResponseManager::MaybeStartSyntheticResponse");
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
  did_start_synthetic_response_ = true;

  return true;
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

// static
void ServiceWorkerSyntheticResponseManager::CloneBufferInBackground(
    mojo::ScopedDataPipeConsumerHandle consumer,
    mojo::ScopedDataPipeProducerHandle producer,
    base::OnceCallback<void()> callback) {
  if (IsServiceWorkerSyntheticResponseSkipUnnecessaryBuffering()) {
    auto data_pipe_connector =
        std::make_unique<ServiceWorkerSyntheticResponseDataPipeConnector>(
            std::move(consumer));
    // To keep `data_pipe_connector` alive for the duration of the async
    // `Transfer` operation, we move its `std::unique_ptr` into a callback that
    // is chained to run after the main `callback`. The `data_pipe_connector`
    // is destroyed when this chained callback runs and the `unique_ptr` goes
    // out of scope.
    //
    // TODO(crbug.com/447039330): Consider using `RefCountedThreadSafe`, we
    // should guarantee `data_pipe_connector` is successfully destroyed even
    // if the callback chain is never run.
    data_pipe_connector->Transfer(
        std::move(producer),
        std::move(callback).Then(base::BindOnce(
            [](std::unique_ptr<ServiceWorkerSyntheticResponseDataPipeConnector>
                   keep_alive_connector) {
              // This lambda is executed as the `on_complete_` from
              // `ServiceWorkerSyntheticResponseDataPipeConnector::Finish`. If
              // we allow `keep_alive_connector` to be destructed
              // synchronously here, `Finish()` will still be on the call stack,
              // leading to a use-after-free. `DeleteSoon()` defers the
              // deletion, avoiding this issue.
              base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
                  FROM_HERE, keep_alive_connector.release());
            },
            std::move(data_pipe_connector))));
    return;
  }
  auto simple_buffer_manager =
      std::make_unique<RaceNetworkRequestSimpleBufferManager>(
          std::move(consumer));
  simple_buffer_manager->Clone(
      std::move(producer),
      std::move(callback).Then(base::BindOnce(
          [](std::unique_ptr<RaceNetworkRequestSimpleBufferManager>
                 simple_buffer_manager) {
            base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
                FROM_HERE, simple_buffer_manager.release());
          },
          std::move(simple_buffer_manager))));
}

void ServiceWorkerSyntheticResponseManager::TransferResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  if (IsServiceWorkerSyntheticResponseOffMainThread()) {
    // Offload the buffer cloning to a background thread.
    base::OnceCallback<void()> callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&ServiceWorkerSyntheticResponseManager::OnCloneCompleted,
                       weak_factory_.GetWeakPtr()));
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ServiceWorkerSyntheticResponseManager::CloneBufferInBackground,
                std::move(body), write_buffer_manager_->ReleaseProducerHandle(),
                std::move(callback)));
    return;
  }
  if (IsServiceWorkerSyntheticResponseSkipUnnecessaryBuffering()) {
    data_pipe_connector_.emplace(std::move(body));
    data_pipe_connector_->Transfer(
        write_buffer_manager_->ReleaseProducerHandle(),
        base::BindOnce(&ServiceWorkerSyntheticResponseManager::OnCloneCompleted,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  simple_buffer_manager_.emplace(std::move(body));
  simple_buffer_manager_->Clone(
      write_buffer_manager_->ReleaseProducerHandle(),
      base::BindOnce(&ServiceWorkerSyntheticResponseManager::OnCloneCompleted,
                     weak_factory_.GetWeakPtr()));
}

void ServiceWorkerSyntheticResponseManager::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnReceiveResponse",
              perfetto::Flow::FromPointer(this));
  response_received_time_ = base::TimeTicks::Now();
  base::UmaHistogramTimes(kHistogramStartRequestToReceiveResponse,
                          response_received_time_ - request_start_time_);
  switch (status_) {
    case SyntheticResponseStatus::kReady: {
      CHECK(write_buffer_manager_.has_value());
      if (IsServiceWorkerSyntheticResponseNetworkService()) {
        if (body.is_valid()) {
          // If the network request was intercepted by an embedder (e.g. Search
          // Prefetch), it might provide its own response body through the
          // `body` handle. However, `ServiceWorkerSyntheticResponseManager`
          // expects to use the data pipe provided to the network service via
          // `trusted_params->response_body_stream`. To handle this
          // inconsistency, we reclaim the original producer handle from
          // `shared_producer_` and write a fallback body (meta refresh) to
          // trigger a reload.
          CHECK(shared_producer_);
          version_->ResetResponseHeadForSyntheticResponse();
          NotifyReloading(std::move(shared_producer_->pipe));
          RecordReloadReason(SyntheticResponseReloadReason::kIntercepted);
        } else if (!response_head->headers->HasHeader(kOptInHeaderName)) {
          // In the NetworkService mode, the fallback logic is executed in the
          // network service. If the fallback is triggered, the network service
          // provides a fake 200 OK response with the fallback body. This fake
          // response does not have the opt-in header. We detect this to clear
          // the stored response head so that the next navigation (reloading)
          // won't trigger the synthetic response again.
          version_->ResetResponseHeadForSyntheticResponse();
          // We don't need to call NotifyReloading() here because the response
          // body (meta refresh) is already populated by the network service.
          RecordReloadReason(
              SyntheticResponseReloadReason::kHeaderInconsistent);
        }
        return;
      }
      bool is_header_consistent = false;
      if (version_->GetResponseHeadForSyntheticResponse()) {
        is_header_consistent = CheckHeaderConsistency(response_head->headers);
        if (is_header_consistent) {
          TransferResponseBody(std::move(body));
        } else {
          // Clear the stored header when it's inconsistent with the header from
          // the network so that the next navigation won't get the header
          // mismatch and reloading consistently.
          //
          // TODO(crbug.com/352578800): Consider setting the response header
          // here rather than resetting it in order to improve the synthetic
          // response coverage. Revisit this after collecting coverage data.
          version_->ResetResponseHeadForSyntheticResponse();
          NotifyReloading(write_buffer_manager_->ReleaseProducerHandle());
          RecordReloadReason(
              SyntheticResponseReloadReason::kHeaderInconsistent);
        }
      } else {
        // The cached response head may have been cleared by another request
        // that detected a header inconsistency. Tell the client to reload to
        // get the latest version.
        NotifyReloading(write_buffer_manager_->ReleaseProducerHandle());
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnReceiveRedirect",
              perfetto::Flow::FromPointer(this));
  if (did_start_synthetic_response_) {
    if (IsServiceWorkerSyntheticResponseNetworkService()) {
      // In the NetworkService mode, the redirect response is managed in the
      // network service. Instead of calling `OnReceiveRedirect()`, the network
      // service sends a fallback trigger (e.g. <meta refresh>) with the
      // non-redirect response.
      NOTREACHED();
    }
    // If the response is already returned from the stored data, that means the
    // renderer may already have received `OnReceiveResponse`. Sending
    // `OnReceiveRedirect` after `OnReceiveResponse` brings errors in that case.
    // Instead, we reload the navigation as a fallback. In the next navigation,
    // the synthetic response is not enabled because it's a reload navigation.
    version_->ResetResponseHeadForSyntheticResponse();
    NotifyReloading(write_buffer_manager_->ReleaseProducerHandle());
    RecordReloadReason(SyntheticResponseReloadReason::kRedirect);
    return;
  }
  std::move(redirect_callback_).Run(redirect_info, std::move(response_head));
}

void ServiceWorkerSyntheticResponseManager::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnComplete",
              perfetto::Flow::FromPointer(this));
  base::UmaHistogramTimes(kHistogramReceiveResponseToComplete,
                          base::TimeTicks::Now() - response_received_time_);
  std::move(complete_callback_).Run(status);
}

void ServiceWorkerSyntheticResponseManager::OnCloneCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnCloneCompleted",
              perfetto::Flow::FromPointer(this));
  write_buffer_manager_->ResetProducer();
  if (auto callback = std::exchange(stream_callback_, {})) {
    // Perhaps this assumption is wrong because the write operation may not be
    // completed at the timing of when the read buffer is empty.
    callback->OnCompleted();
  }
}

bool ServiceWorkerSyntheticResponseManager::CheckHeaderConsistency(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  const auto& response_head = version_->GetResponseHeadForSyntheticResponse();
  CHECK(response_head);
  bool result = network::CheckHeaderConsistencyForSyntheticResponse(
      *headers, *response_head->headers);
  TRACE_EVENT1("ServiceWorker",
               "ServiceWorkerSyntheticResponseManager::CheckHeaderConsistency",
               "result", result);

  return result;
}

void ServiceWorkerSyntheticResponseManager::NotifyReloading(
    mojo::ScopedDataPipeProducerHandle producer) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::NotifyReloading");
  CHECK(producer.is_valid());
  auto [result, written_bytes] =
      network::WriteSyntheticResponseFallbackBody(producer);
  if (result != MOJO_RESULT_OK) {
    write_buffer_manager_->ResetProducer();
    if (auto callback = std::exchange(stream_callback_, {})) {
      callback->OnAborted();
    }
    return;
  }
  CHECK_GE(written_bytes, 0u);
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
