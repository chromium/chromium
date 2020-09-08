// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_loader.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_prefetch_metric_recorder.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/web_package/signed_exchange_validity_pinger.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/data_pipe_to_source_stream.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/source_stream_to_data_pipe.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"

namespace content {

namespace {

constexpr char kLoadResultHistogram[] = "SignedExchange.LoadResult2";
constexpr char kPrefetchLoadResultHistogram[] =
    "SignedExchange.Prefetch.LoadResult2";

SignedExchangeHandlerFactory* g_signed_exchange_factory_for_testing_ = nullptr;

}  // namespace

SignedExchangeLoader::SignedExchangeLoader(
    const network::ResourceRequest& outer_request,
    network::mojom::URLResponseHeadPtr outer_response_head,
    mojo::ScopedDataPipeConsumerHandle outer_response_body,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    uint32_t url_loader_options,
    bool should_redirect_on_failure,
    std::unique_ptr<SignedExchangeDevToolsProxy> devtools_proxy,
    std::unique_ptr<SignedExchangeReporter> reporter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    int frame_tree_node_id,
    scoped_refptr<SignedExchangePrefetchMetricRecorder> metric_recorder,
    const std::string& accept_langs,
    bool keep_entry_for_prefetch_cache)
    : outer_request_(outer_request),
      outer_response_head_(std::move(outer_response_head)),
      forwarding_client_(std::move(forwarding_client)),
      url_loader_options_(url_loader_options),
      should_redirect_on_failure_(should_redirect_on_failure),
      devtools_proxy_(std::move(devtools_proxy)),
      reporter_(std::move(reporter)),
      url_loader_factory_(std::move(url_loader_factory)),
      url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
      frame_tree_node_id_(frame_tree_node_id),
      metric_recorder_(std::move(metric_recorder)),
      accept_langs_(accept_langs) {
  DCHECK(outer_request_.url.is_valid());

  if (keep_entry_for_prefetch_cache) {
    cache_entry_ = std::make_unique<PrefetchedSignedExchangeCacheEntry>();
    cache_entry_->SetOuterUrl(outer_request_.url);
    cache_entry_->SetOuterResponse(outer_response_head_->Clone());
  }

  // |metric_recorder_| could be null in some tests.
  if (!(outer_request_.load_flags & net::LOAD_PREFETCH) && metric_recorder_) {
    metric_recorder_->OnSignedExchangeNonPrefetch(
        outer_request_.url, outer_response_head_->response_time);
  }
  // Can't use HttpResponseHeaders::GetMimeType() because SignedExchangeHandler
  // checks "v=" parameter.
  outer_response_head_->headers->EnumerateHeader(nullptr, "content-type",
                                                 &content_type_);

  url_loader_.Bind(std::move(endpoints->url_loader));

  // |outer_response_body| is valid, when it's a navigation request.
  if (outer_response_body)
    OnStartLoadingResponseBody(std::move(outer_response_body));

  // Bind the endpoint with |this| to get the body DataPipe.
  url_loader_client_receiver_.Bind(std::move(endpoints->url_loader_client));

  // |client_| will be bound with a forwarding client by ConnectToClient().
  pending_client_receiver_ = client_.BindNewPipeAndPassReceiver();
}

SignedExchangeLoader::~SignedExchangeLoader() = default;

void SignedExchangeLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void SignedExchangeLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void SignedExchangeLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED();
}

void SignedExchangeLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  // CachedMetadata for Signed Exchange is not supported.
  NOTREACHED();
}

void SignedExchangeLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  // TODO(https://crbug.com/803774): Implement this to progressively update the
  // encoded data length in DevTools.
}

void SignedExchangeLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle response_body) {
  auto cert_fetcher_factory = SignedExchangeCertFetcherFactory::Create(
      url_loader_factory_, url_loader_throttles_getter_,
      outer_request_.throttling_profile_id,
      (outer_request_.trusted_params &&
       !outer_request_.trusted_params->isolation_info.IsEmpty())
          ? net::IsolationInfo::Create(
                net::IsolationInfo::RedirectMode::kUpdateNothing,
                *outer_request_.trusted_params->isolation_info
                     .top_frame_origin(),
                *outer_request_.trusted_params->isolation_info.frame_origin(),
                net::SiteForCookies())
          : net::IsolationInfo());

  if (g_signed_exchange_factory_for_testing_) {
    signed_exchange_handler_ = g_signed_exchange_factory_for_testing_->Create(
        outer_request_.url,
        std::make_unique<network::DataPipeToSourceStream>(
            std::move(response_body)),
        base::BindOnce(&SignedExchangeLoader::OnHTTPExchangeFound,
                       weak_factory_.GetWeakPtr()),
        std::move(cert_fetcher_factory));
    return;
  }

  signed_exchange_handler_ = std::make_unique<SignedExchangeHandler>(
      blink::network_utils::IsOriginSecure(outer_request_.url),
      signed_exchange_utils::HasNoSniffHeader(*outer_response_head_),
      content_type_,
      std::make_unique<network::DataPipeToSourceStream>(
          std::move(response_body)),
      base::BindOnce(&SignedExchangeLoader::OnHTTPExchangeFound,
                     weak_factory_.GetWeakPtr()),
      std::move(cert_fetcher_factory), outer_request_.load_flags,
      std::make_unique<blink::WebPackageRequestMatcher>(outer_request_.headers,
                                                        accept_langs_),
      std::move(devtools_proxy_), reporter_.get(), frame_tree_node_id_);
}

void SignedExchangeLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(!outer_response_length_info_);
  outer_response_length_info_ = OuterResponseLengthInfo();
  outer_response_length_info_->encoded_data_length = status.encoded_data_length;
  outer_response_length_info_->decoded_body_length = status.decoded_body_length;
  NotifyClientOnCompleteIfReady();
}

void SignedExchangeLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  NOTREACHED();
}

void SignedExchangeLoader::SetPriority(net::RequestPriority priority,
                                       int intra_priority_value) {
  url_loader_->SetPriority(priority, intra_priority_value);
}

void SignedExchangeLoader::PauseReadingBodyFromNet() {
  url_loader_->PauseReadingBodyFromNet();
}

void SignedExchangeLoader::ResumeReadingBodyFromNet() {
  url_loader_->ResumeReadingBodyFromNet();
}

void SignedExchangeLoader::ConnectToClient(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(pending_client_receiver_.is_valid());
  mojo::FusePipes(std::move(pending_client_receiver_), std::move(client));
}

std::unique_ptr<PrefetchedSignedExchangeCacheEntry>
SignedExchangeLoader::TakePrefetchedSignedExchangeCacheEntry() {
  DCHECK(cache_entry_);
  return std::move(cache_entry_);
}

void SignedExchangeLoader::OnHTTPExchangeFound(
    SignedExchangeLoadResult result,
    net::Error error,
    const GURL& request_url,
    network::mojom::URLResponseHeadPtr resource_response,
    std::unique_ptr<net::SourceStream> payload_stream) {
  if (error) {
    DCHECK_NE(result, SignedExchangeLoadResult::kSuccess);
    ReportLoadResult(result);

    if (error != net::ERR_INVALID_SIGNED_EXCHANGE ||
        !should_redirect_on_failure_ || !request_url.is_valid()) {
      // Let the request fail.
      // This will eventually delete |this|.
      forwarding_client_->OnComplete(network::URLLoaderCompletionStatus(error));
      return;
    }

    // Make a fallback redirect to |request_url|.
    DCHECK(!fallback_url_);
    fallback_url_ = request_url;
    forwarding_client_->OnReceiveRedirect(
        signed_exchange_utils::CreateRedirectInfo(
            request_url, outer_request_, *outer_response_head_,
            true /* is_fallback_redirect */),
        signed_exchange_utils::CreateRedirectResponseHead(
            *outer_response_head_, true /* is_fallback_redirect */));
    forwarding_client_.reset();
    return;
  }
  DCHECK_EQ(result, SignedExchangeLoadResult::kSuccess);
  inner_request_url_ = request_url;

  if (cache_entry_) {
    cache_entry_->SetInnerUrl(*inner_request_url_);
    auto inner_response_for_cache = resource_response->Clone();
    inner_response_for_cache->was_fetched_via_cache = true;
    inner_response_for_cache->was_in_prefetch_cache = true;
    cache_entry_->SetInnerResponse(std::move(inner_response_for_cache));
    const bool get_info_result =
        signed_exchange_handler_->GetSignedExchangeInfoForPrefetchCache(
            *cache_entry_);
    DCHECK(get_info_result);
  }

  forwarding_client_->OnReceiveRedirect(
      signed_exchange_utils::CreateRedirectInfo(
          request_url, outer_request_, *outer_response_head_,
          false /* is_fallback_redirect */),
      signed_exchange_utils::CreateRedirectResponseHead(
          *outer_response_head_, false /* is_fallback_redirect */));
  forwarding_client_.reset();

  const base::Optional<net::SSLInfo>& ssl_info = resource_response->ssl_info;
  if (ssl_info.has_value() &&
      (url_loader_options_ &
       network::mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
      net::IsCertStatusError(ssl_info->cert_status)) {
    ssl_info_ = ssl_info;
  }

  network::mojom::URLResponseHeadPtr inner_response_head_shown_to_client =
      std::move(resource_response);
  if (ssl_info.has_value() &&
      !(url_loader_options_ &
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse)) {
    inner_response_head_shown_to_client->ssl_info = base::nullopt;
  }
  inner_response_head_shown_to_client->was_fetched_via_cache =
      outer_response_head_->was_fetched_via_cache;
  client_->OnReceiveResponse(std::move(inner_response_head_shown_to_client));

  // Currently we always assume that we have body.
  // TODO(https://crbug.com/80374): Add error handling and bail out
  // earlier if there's an error.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = network::kDataPipeDefaultAllocationSize;
  if (mojo::CreateDataPipe(&options, &producer_handle, &consumer_handle) !=
      MOJO_RESULT_OK) {
    forwarding_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }
  pending_body_consumer_ = std::move(consumer_handle);
  body_data_pipe_adapter_ = std::make_unique<network::SourceStreamToDataPipe>(
      std::move(payload_stream), std::move(producer_handle));

  StartReadingBody();
}

void SignedExchangeLoader::StartReadingBody() {
  DCHECK(body_data_pipe_adapter_);
  DCHECK(pending_body_consumer_.is_valid());

  // If it's not for prefetch, kSignedHTTPExchangePingValidity is enabled
  // and validity_pinger_ is not initialized yet, create a validity pinger
  // and start it to ping the validity URL before start reading the inner
  // response body.
  if (!(outer_request_.load_flags & net::LOAD_PREFETCH) &&
      base::FeatureList::IsEnabled(features::kSignedHTTPExchangePingValidity) &&
      !validity_pinger_) {
    DCHECK(url_loader_factory_);
    DCHECK(url_loader_throttles_getter_);
    DCHECK(inner_request_url_);
    // For now we just use the fallback (request) URL to ping.
    // TODO(kinuko): Use the validity URL extracted from the exchange.
    validity_pinger_ = SignedExchangeValidityPinger::CreateAndStart(
        *inner_request_url_, url_loader_factory_,
        url_loader_throttles_getter_.Run(),
        outer_request_.throttling_profile_id,
        base::BindOnce(&SignedExchangeLoader::StartReadingBody,
                       weak_factory_.GetWeakPtr()));
    DCHECK(validity_pinger_);
    return;
  }

  validity_pinger_.reset();

  // Start reading.
  client_->OnStartLoadingResponseBody(std::move(pending_body_consumer_));
  body_data_pipe_adapter_->Start(base::BindOnce(
      &SignedExchangeLoader::FinishReadingBody, base::Unretained(this)));
}

void SignedExchangeLoader::FinishReadingBody(int result) {
  DCHECK(!decoded_body_read_result_);
  decoded_body_read_result_ = result;
  NotifyClientOnCompleteIfReady();
}

void SignedExchangeLoader::NotifyClientOnCompleteIfReady() {
  // If |outer_response_length_info_| or |decoded_body_read_result_| is
  // unavailable, do nothing and rely on the subsequent call to notify client.
  if (!outer_response_length_info_ || !decoded_body_read_result_)
    return;

  ReportLoadResult(*decoded_body_read_result_ == net::OK
                       ? SignedExchangeLoadResult::kSuccess
                       : SignedExchangeLoadResult::kMerkleIntegrityError);

  network::URLLoaderCompletionStatus status;
  status.error_code = *decoded_body_read_result_;
  status.completion_time = base::TimeTicks::Now();
  status.encoded_data_length = outer_response_length_info_->encoded_data_length;
  status.encoded_body_length =
      outer_response_length_info_->decoded_body_length -
      signed_exchange_handler_->GetExchangeHeaderLength();
  status.decoded_body_length = body_data_pipe_adapter_->TransferredBytes();

  if (ssl_info_) {
    DCHECK((url_loader_options_ &
            network::mojom::kURLLoadOptionSendSSLInfoForCertificateError) &&
           net::IsCertStatusError(ssl_info_->cert_status));
    status.ssl_info = *ssl_info_;
  }

  // This will eventually delete |this|.
  client_->OnComplete(status);
}

void SignedExchangeLoader::ReportLoadResult(SignedExchangeLoadResult result) {
  UMA_HISTOGRAM_ENUMERATION(kLoadResultHistogram, result);
  // |metric_recorder_| could be null in some tests.
  if ((outer_request_.load_flags & net::LOAD_PREFETCH) && metric_recorder_) {
    UMA_HISTOGRAM_ENUMERATION(kPrefetchLoadResultHistogram, result);
    metric_recorder_->OnSignedExchangePrefetchFinished(
        outer_request_.url, outer_response_head_->response_time);
  }

  if (reporter_)
    reporter_->ReportLoadResultAndFinish(result);
}

void SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(
    SignedExchangeHandlerFactory* factory) {
  g_signed_exchange_factory_for_testing_ = factory;
}

}  // namespace content
