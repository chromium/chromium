// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/network_usage_accumulator.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

// An enum class representing whether / how keepalive requests are blocked. This
// is used for UMA so do NOT re-assign values.
enum class KeepaliveBlockStatus {
  // The request is not blocked.
  kNotBlocked = 0,
  // The request is blocked due to NetworkContext::CanCreateLoader.
  kBlockedDueToCanCreateLoader = 1,
  // The request is blocked due to the number of requests per process.
  kBlockedDueToNumberOfRequestsPerProcess = 2,
  // The request is blocked due to the number of requests per top-level frame.
  kBlockedDueToNumberOfRequestsPerTopLevelFrame = 3,
  // The request is blocked due to the number of requests in the system.
  kBlockedDueToNumberOfRequests = 4,
  // The request is blocked due to the total size of URL and request headers.
  kBlockedDueToTotalSizeOfUrlAndHeaders = 5,
  // The request is NOT blocked but the total size of URL and request headers
  // exceeds 384kb.
  kNotBlockedButUrlAndHeadersExceeds384kb = 6,
  // The request is NOT blocked but the total size of URL and request headers
  // exceeds 256kb.
  kNotBlockedButUrlAndHeadersExceeds256kb = 7,
  kMaxValue = kNotBlockedButUrlAndHeadersExceeds256kb,
};

}  // namespace

constexpr int URLLoaderFactory::kMaxKeepaliveConnections;
constexpr int URLLoaderFactory::kMaxKeepaliveConnectionsPerProcess;
constexpr int URLLoaderFactory::kMaxTotalKeepaliveRequestSize;

URLLoaderFactory::URLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    cors::CorsURLLoaderFactory* cors_url_loader_factory)
    : context_(context),
      params_(std::move(params)),
      resource_scheduler_client_(std::move(resource_scheduler_client)),
      header_client_(std::move(params_->header_client)),
      cors_url_loader_factory_(cors_url_loader_factory) {
  DCHECK(context);
  DCHECK_NE(mojom::kInvalidProcessId, params_->process_id);

  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Register(
        params_->process_id);
  }
}

URLLoaderFactory::~URLLoaderFactory() {
  if (context_->network_service()) {
    context_->network_service()->keepalive_statistics_recorder()->Unregister(
        params_->process_id);
  }
}

void URLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Requests with |trusted_params| when params_->is_trusted is not set should
  // have been rejected at the CorsURLLoader layer.
  DCHECK(!url_request.trusted_params || params_->is_trusted);

  std::string origin_string;
  bool has_origin = url_request.headers.GetHeader("Origin", &origin_string) &&
                    origin_string != "null";
  base::Optional<url::Origin> request_initiator = url_request.request_initiator;
  if (has_origin && request_initiator.has_value()) {
    url::Origin origin = url::Origin::Create(GURL(origin_string));
    bool origin_head_same_as_request_origin =
        request_initiator.value().IsSameOriginWith(origin);
    UMA_HISTOGRAM_BOOLEAN(
        "NetworkService.URLLoaderFactory.OriginHeaderSameAsRequestOrigin",
        origin_head_same_as_request_origin);
  }

  mojom::NetworkServiceClient* network_service_client = nullptr;
  base::WeakPtr<KeepaliveStatisticsRecorder> keepalive_statistics_recorder;
  base::WeakPtr<NetworkUsageAccumulator> network_usage_accumulator;
  if (context_->network_service()) {
    network_service_client = context_->network_service()->client();
    keepalive_statistics_recorder = context_->network_service()
                                        ->keepalive_statistics_recorder()
                                        ->AsWeakPtr();
    network_usage_accumulator =
        context_->network_service()->network_usage_accumulator()->AsWeakPtr();
  }

  bool exhausted = false;
  if (!context_->CanCreateLoader(params_->process_id)) {
    exhausted = true;
  }

  int keepalive_request_size = 0;
  if (url_request.keepalive && keepalive_statistics_recorder) {
    const size_t url_size = url_request.url.spec().size();
    size_t headers_size = 0;

    net::HttpRequestHeaders merged_headers = url_request.headers;
    merged_headers.MergeFrom(url_request.cors_exempt_headers);

    for (const auto& pair : merged_headers.GetHeaderVector()) {
      headers_size += (pair.key.size() + pair.value.size());
    }

    UMA_HISTOGRAM_COUNTS_10000("Net.KeepaliveRequest.UrlSize", url_size);
    UMA_HISTOGRAM_COUNTS_10000("Net.KeepaliveRequest.HeadersSize",
                               headers_size);
    UMA_HISTOGRAM_COUNTS_10000("Net.KeepaliveRequest.UrlPlusHeadersSize",
                               url_size + headers_size);

    keepalive_request_size = url_size + headers_size;

    KeepaliveBlockStatus block_status = KeepaliveBlockStatus::kNotBlocked;
    const auto& recorder = *keepalive_statistics_recorder;
    if (!context_->CanCreateLoader(params_->process_id)) {
      // We already checked this, but we have this here for histogram.
      DCHECK(exhausted);
      block_status = KeepaliveBlockStatus::kBlockedDueToCanCreateLoader;
    } else if (recorder.num_inflight_requests() >= kMaxKeepaliveConnections) {
      exhausted = true;
      block_status = KeepaliveBlockStatus::kBlockedDueToNumberOfRequests;
    } else if (recorder.NumInflightRequestsPerProcess(params_->process_id) >=
               kMaxKeepaliveConnectionsPerProcess) {
      exhausted = true;
      block_status =
          KeepaliveBlockStatus::kBlockedDueToNumberOfRequestsPerProcess;
    } else if (recorder.GetTotalRequestSizePerProcess(params_->process_id) +
                   keepalive_request_size >
               kMaxTotalKeepaliveRequestSize) {
      exhausted = true;
      block_status =
          KeepaliveBlockStatus::kBlockedDueToTotalSizeOfUrlAndHeaders;
    } else if (recorder.GetTotalRequestSizePerProcess(params_->process_id) +
                   keepalive_request_size >
               384 * 1024) {
      block_status =
          KeepaliveBlockStatus::kNotBlockedButUrlAndHeadersExceeds384kb;
    } else if (recorder.GetTotalRequestSizePerProcess(params_->process_id) +
                   keepalive_request_size >
               256 * 1024) {
      block_status =
          KeepaliveBlockStatus::kNotBlockedButUrlAndHeadersExceeds256kb;
    } else {
      block_status = KeepaliveBlockStatus::kNotBlocked;
    }
    UMA_HISTOGRAM_ENUMERATION("Net.KeepaliveRequest.BlockStatus", block_status);
  }

  if (exhausted) {
    URLLoaderCompletionStatus status;
    status.error_code = net::ERR_INSUFFICIENT_RESOURCES;
    status.exists_in_cache = false;
    status.completion_time = base::TimeTicks::Now();
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))->OnComplete(status);
    return;
  }

  auto loader = std::make_unique<URLLoader>(
      context_->url_request_context(), network_service_client,
      context_->client(),
      base::BindOnce(&cors::CorsURLLoaderFactory::DestroyURLLoader,
                     base::Unretained(cors_url_loader_factory_)),
      std::move(receiver), options, url_request, std::move(client),
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      params_.get(), request_id, keepalive_request_size,
      resource_scheduler_client_, std::move(keepalive_statistics_recorder),
      std::move(network_usage_accumulator),
      header_client_.is_bound() ? header_client_.get() : nullptr,
      context_->origin_policy_manager());
  cors_url_loader_factory_->OnLoaderCreated(std::move(loader));
}

void URLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

}  // namespace network
