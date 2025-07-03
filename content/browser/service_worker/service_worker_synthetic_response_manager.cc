// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"

#include <cstddef>

#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom.h"

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
    network::mojom::URLResponseHeadPtr head) {
  CHECK(head->headers->HasHeaderValue(kOptInHeaderName, kOptInHeaderValue));
  auto out_response = blink::mojom::FetchAPIResponse::New();
  out_response->status_code = net::HTTP_OK;
  out_response->response_time = base::Time::Now();
  out_response->url_list = head->url_list_via_service_worker;
  out_response->response_type = head->response_type;
  out_response->padding = head->padding;
  out_response->mime_type.emplace(head->mime_type);
  out_response->response_source = head->service_worker_response_source;
  out_response->cache_storage_cache_name.emplace(
      head->cache_storage_cache_name);
  out_response->cors_exposed_header_names = head->cors_exposed_header_names;
  out_response->parsed_headers = head->parsed_headers.Clone();
  out_response->connection_info = head->connection_info;
  out_response->alpn_negotiated_protocol = head->alpn_negotiated_protocol;
  out_response->was_fetched_via_spdy = head->was_fetched_via_spdy;
  out_response->has_range_requested = head->has_range_requested;
  out_response->auth_challenge_info = head->auth_challenge_info;

  // Parse headers.
  size_t iter = 0;
  std::string header_name;
  std::string header_value;
  while (
      head->headers->EnumerateHeaderLines(&iter, &header_name, &header_value)) {
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
      OnCompleteCallback complete_callback)
      : receive_response_callback_(std::move(receive_response_callback)),
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
    receive_response_callback_.Run(response_head.Clone(), std::move(body));
  }
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    std::move(complete_callback_).Run(status);
  }

  OnReceiveResponseCallback receive_response_callback_;
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
    OnCompleteCallback complete_callback) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::StartRequest");
  response_callback_ = std::move(receive_response_callback);
  complete_callback_ = std::move(complete_callback);
  client_ = std::make_unique<SyntheticResponseURLLoaderClient>(
      base::BindRepeating(
          &ServiceWorkerSyntheticResponseManager::OnReceiveResponse,
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
  auto response_head = version_->GetResponseHeadForSyntheticResponse();
  CHECK(response_head);
  blink::mojom::FetchAPIResponsePtr response =
      GetFetchAPIResponse(response_head.Clone());
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
}

void ServiceWorkerSyntheticResponseManager::MaybeSetResponseHead(
    network::mojom::URLResponseHeadPtr response_head) {
  if (!network::IsSuccessfulStatus(response_head->headers->response_code())) {
    // If the response is not successful, do not update the response head.
    return;
  }
  if (!response_head->headers->HasHeaderValue(kOptInHeaderName,
                                              kOptInHeaderValue)) {
    // If there is no opt-in header, do not update the response head.
    return;
  }
  version_->SetMainScriptResponse(
      std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
          *response_head.Clone()));
  version_->SetResponseHeadForSyntheticResponse(*response_head.Clone());
}

void ServiceWorkerSyntheticResponseManager::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body) {
  TRACE_EVENT("ServiceWorker",
              "ServiceWorkerSyntheticResponseManager::OnReceiveResponse");
  switch (status_) {
    case SyntheticResponseStatus::kReady:
      CHECK(write_buffer_manager_.has_value());
      if (CheckHeaderConsistency(response_head->headers)) {
        simple_buffer_manager_.emplace(std::move(body));
        simple_buffer_manager_->Clone(
            write_buffer_manager_->ReleaseProducerHandle(),
            base::BindOnce(
                &ServiceWorkerSyntheticResponseManager::OnCloneCompleted,
                weak_factory_.GetWeakPtr()));
      } else {
        // Clear the stored header when it's inconsistent with the header from
        // the network so that the next navigation won't get the header mismatch
        // and reloading consistently.
        //
        // TODO(crbug.com/352578800): Consider setting the response header here
        // rather than resetting it in order to improve the synthetic response
        // coverage. Revisit this after collecting coverage data.
        version_->ResetResponseHeadForSyntheticResponse();
        NotifyReloading();
      }
      break;
    case SyntheticResponseStatus::kNotReady:
      MaybeSetResponseHead(response_head.Clone());
      std::move(response_callback_)
          .Run(std::move(response_head), std::move(body));
      break;
  }
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
  CHECK(version_->GetResponseHeadForSyntheticResponse());
  // TODO(crbug.com/352578800): Handle other necessary headers e.g. encoding.
  const base::flat_set<std::string_view> ignored_headers = {"date"};
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
  auto stored_headers = collect_significant_headers(
      *version_->GetResponseHeadForSyntheticResponse()->headers);

  return incoming_headers == stored_headers;
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
}  // namespace content
