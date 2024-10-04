// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_loader.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_entry.h"
#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/data_pipe_to_source_stream.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/source_stream_to_data_pipe.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/web_package/web_package_request_matcher.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kPrefetchLoadResultHistogram[] =
    "SignedExchange.Prefetch.LoadResult2";

SignedExchangeHandlerFactory* g_signed_exchange_factory_for_testing_ = nullptr;

net::IsolationInfo CreateIsolationInfoForCertFetch(
    const network::ResourceRequest& outer_request) {
  if (!outer_request.trusted_params ||
      outer_request.trusted_params->isolation_info.IsEmpty()) {
    return net::IsolationInfo();
  }
  return net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      *outer_request.trusted_params->isolation_info.top_frame_origin(),
      *outer_request.trusted_params->isolation_info.frame_origin(),
      net::SiteForCookies());
}

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
    FrameTreeNodeId frame_tree_node_id,
    const std::string& accept_langs,
    bool keep_entry_for_prefetch_cache)
    : outer_request_(outer_request),
      outer_response_head_(std::move(outer_response_head)),
      forwarding_client_(std::move(forwarding_client)),
      reporter_(std::move(reporter)),
      url_loader_options_(url_loader_options),
      should_redirect_on_failure_(should_redirect_on_failure) {
  DCHECK(outer_request_.url.is_valid());
  DCHECK(outer_response_body);

  if (keep_entry_for_prefetch_cache) {
    cache_entry_ = std::make_unique<PrefetchedSignedExchangeCacheEntry>();
    cache_entry_->SetOuterUrl(outer_request_.url);
    cache_entry_->SetOuterResponse(outer_response_head_->Clone());
  } else {
    // `outer_request` corresponds to a navigation, so we expect the
    // TrustedParams and IsolationInfo to be set.
    CHECK(outer_request.trusted_params.has_value());
  }

  url_loader_.Bind(std::move(endpoints->url_loader));

  auto cert_fetcher_factory = SignedExchangeCertFetcherFactory::Create(
      std::move(url_loader_factory), std::move(url_loader_throttles_getter),
      outer_request_.throttling_profile_id,
      CreateIsolationInfoForCertFetch(outer_request_),
      outer_request_.request_initiator);

  if (g_signed_exchange_factory_for_testing_) {
    signed_exchange_handler_ = g_signed_exchange_factory_for_testing_->Create(
        outer_request_.url,
        std::make_unique<network::DataPipeToSourceStream>(
            std::move(outer_response_body)),
        base::BindOnce(&SignedExchangeLoader::OnHTTPExchangeFound,
                       weak_factory_.GetWeakPtr()),
        std::move(cert_fetcher_factory));
  } else {
    // Can't use HttpResponseHeaders::GetMimeType() because
    // SignedExchangeHandler checks "v=" parameter.
    std::optional<std::string_view> content_type =
        outer_response_head_->headers->EnumerateHeader(nullptr, "content-type");

    signed_exchange_handler_ = std::make_unique<SignedExchangeHandler>(
        network::IsUrlPotentiallyTrustworthy(outer_request_.url),
        web_package::HasNoSniffHeader(*outer_response_head_),
        content_type.value_or(std::string_view()),
        std::make_unique<network::DataPipeToSourceStream>(
            std::move(outer_response_body)),
        base::BindOnce(&SignedExchangeLoader::OnHTTPExchangeFound,
                       weak_factory_.GetWeakPtr()),
        std::move(cert_fetcher_factory),
        keep_entry_for_prefetch_cache
            ? std::nullopt
            : std::make_optional(outer_request_.trusted_params->isolation_info),
        outer_request_.load_flags, outer_response_head_->remote_endpoint,
        std::make_unique<blink::WebPackageRequestMatcher>(
            outer_request_.headers, accept_langs),
        std::move(devtools_proxy), reporter_.get(), frame_tree_node_id);
  }

  // Bind the endpoint with |this| to get the body DataPipe.
  url_loader_client_receiver_.Bind(std::move(endpoints->url_loader_client));

  // |client_| will be bound with a forwarding client by ConnectToClient().
  pending_client_receiver_ = client_.BindNewPipeAndPassReceiver();
}

SignedExchangeLoader::~SignedExchangeLoader() = default;

void SignedExchangeLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  // Must not be called because this SignedExchangeLoader and the client
  // endpoints were bound after OnReceiveResponse() is called.
  NOTREACHED_IN_MIGRATION();
}

void SignedExchangeLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  // TODO(crbug.com/40558902): Implement this to progressively update the
  // encoded data length in DevTools.
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kSignedExchangeLoader);
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
    const std::optional<GURL>& new_url) {
  NOTREACHED_IN_MIGRATION();
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

  const std::optional<net::SSLInfo>& ssl_info = resource_response->ssl_info;
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
    inner_response_head_shown_to_client->ssl_info = std::nullopt;
  }
  inner_response_head_shown_to_client->was_fetched_via_cache =
      outer_response_head_->was_fetched_via_cache;

  // Currently we always assume that we have body.
  // TODO(crbug.com/40558879): Add error handling and bail out
  // earlier if there's an error.
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes =
      network::features::GetDataPipeDefaultAllocationSize();
  if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
      MOJO_RESULT_OK) {
    forwarding_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    return;
  }

  client_->OnReceiveResponse(std::move(inner_response_head_shown_to_client),
                             std::move(consumer_handle), std::nullopt);

  body_data_pipe_adapter_ = std::make_unique<network::SourceStreamToDataPipe>(
      std::move(payload_stream), std::move(producer_handle));

  // Start reading.
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
  signed_exchange_utils::RecordLoadResultHistogram(result);
  if (outer_request_.load_flags & net::LOAD_PREFETCH) {
    UMA_HISTOGRAM_ENUMERATION(kPrefetchLoadResultHistogram, result);
  }

  if (reporter_)
    reporter_->ReportLoadResultAndFinish(result);
}

void SignedExchangeLoader::SetSignedExchangeHandlerFactoryForTest(
    SignedExchangeHandlerFactory* factory) {
  g_signed_exchange_factory_for_testing_ = factory;
}

}  // namespace content
