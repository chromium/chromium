// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache_adapter.h"
#include "content/browser/web_package/signed_exchange_prefetch_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/network_anonymization_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

constexpr char kSignedExchangeEnabledAcceptHeaderForCrossOriginPrefetch[] =
    "application/signed-exchange;v=b3;q=0.7,*/*;q=0.8";

}  // namespace

PrefetchURLLoader::PrefetchURLLoader(
    int32_t request_id,
    uint32_t options,
    FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& resource_request,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    BrowserContext* browser_context,
    scoped_refptr<PrefetchedSignedExchangeCache>
        prefetched_signed_exchange_cache,
    const std::string& accept_langs,
    RecursivePrefetchTokenGenerator recursive_prefetch_token_generator)
    : frame_tree_node_id_(frame_tree_node_id),
      resource_request_(resource_request),
      network_anonymization_key_(network_anonymization_key),
      network_loader_factory_(std::move(network_loader_factory)),
      forwarding_client_(std::move(client)),
      url_loader_throttles_getter_(url_loader_throttles_getter),
      accept_langs_(accept_langs),
      recursive_prefetch_token_generator_(
          std::move(recursive_prefetch_token_generator)),
      is_signed_exchange_handling_enabled_(
          signed_exchange_utils::IsSignedExchangeHandlingEnabled(
              browser_context)) {
  DCHECK(network_loader_factory_);
  CHECK(!resource_request.trusted_params ||
        resource_request.trusted_params->isolation_info.request_type() ==
            net::IsolationInfo::RequestType::kOther ||
        resource_request.trusted_params->isolation_info.request_type() ==
            net::IsolationInfo::RequestType::kMainFrame);

  if (is_signed_exchange_handling_enabled_) {
    // Set the SignedExchange accept header.
    // (https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#internet-media-type-applicationsigned-exchange).

    // TODO(crbug.com/40250488): find a solution for CORS requests,
    // perhaps exempt the Accept header from the 128-byte rule
    // (https://fetch.spec.whatwg.org/#cors-safelisted-request-header). For now,
    // we use the frame Accept header for prefetches only in requests with a
    // no-cors/same-origin mode to avoid an unintended preflight.
    std::string accept_header =
        resource_request_.mode == network::mojom::RequestMode::kCors
            ? kSignedExchangeEnabledAcceptHeaderForCrossOriginPrefetch
            : FrameAcceptHeaderValue(/*allow_sxg_responses=*/true,
                                     browser_context);
    resource_request_.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                        std::move(accept_header));
    if (prefetched_signed_exchange_cache) {
      prefetched_signed_exchange_cache_adapter_ =
          std::make_unique<PrefetchedSignedExchangeCacheAdapter>(
              std::move(prefetched_signed_exchange_cache),
              browser_context->GetBlobStorageContext(), resource_request.url,
              this);
    }
  }

  network_loader_factory_->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id, options,
      resource_request_, client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchURLLoader::OnNetworkConnectionError, base::Unretained(this)));
}

PrefetchURLLoader::~PrefetchURLLoader() = default;

void PrefetchURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  DCHECK(modified_headers.IsEmpty())
      << "Redirect with modified headers was not supported yet. "
         "crbug.com/845683";
  DCHECK(!new_url) << "Redirect with modified URL was not "
                      "supported yet. crbug.com/845683";
  if (signed_exchange_prefetch_handler_) {
    // Rebind |client_receiver_| and |loader_|.
    client_receiver_.Bind(signed_exchange_prefetch_handler_->FollowRedirect(
        loader_.BindNewPipeAndPassReceiver()));
    return;
  }

  DCHECK(loader_);
  loader_->FollowRedirect(
      removed_headers, net::HttpRequestHeaders() /* modified_headers */,
      net::HttpRequestHeaders() /* modified_cors_exempt_headers */,
      std::nullopt);
}

void PrefetchURLLoader::SetPriority(net::RequestPriority priority,
                                    int intra_priority_value) {
  if (loader_) {
    loader_->SetPriority(priority, intra_priority_value);
  }
}

void PrefetchURLLoader::PauseReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_) {
    loader_->PauseReadingBodyFromNet();
  }
}

void PrefetchURLLoader::ResumeReadingBodyFromNet() {
  // TODO(kinuko): Propagate or handle the case where |loader_| is
  // detached (for SignedExchanges), see OnReceiveResponse.
  if (loader_) {
    loader_->ResumeReadingBodyFromNet();
  }
}

void PrefetchURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void PrefetchURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  if (is_signed_exchange_handling_enabled_ &&
      signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(
          resource_request_.url, *response)) {
    DCHECK(!signed_exchange_prefetch_handler_);
    const bool keep_entry_for_prefetch_cache =
        !!prefetched_signed_exchange_cache_adapter_;

    // Note that after this point this doesn't directly get upcalls from the
    // network. (Until |this| calls the handler's FollowRedirect.)
    signed_exchange_prefetch_handler_ =
        std::make_unique<SignedExchangePrefetchHandler>(
            frame_tree_node_id_, resource_request_, std::move(response),
            std::move(body), loader_.Unbind(), client_receiver_.Unbind(),
            network_loader_factory_, url_loader_throttles_getter_, this,
            network_anonymization_key_, accept_langs_,
            keep_entry_for_prefetch_cache);
    return;
  }

  // If the response is marked as a restricted cross-origin prefetch, we
  // populate the response's |recursive_prefetch_token| member with a unique
  // token. The renderer will propagate this token to recursive prefetches
  // coming from this response, in the form of preload headers. This token is
  // later used by the PrefetchURLLoaderService to recover the correct
  // NetworkAnonymizationKey to use when fetching the request. In the Signed
  // Exchange case, we do this after redirects from the outer response, because
  // we redirect back here for the inner response.
  if (resource_request_.load_flags &
      net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME) {
    DCHECK(!recursive_prefetch_token_generator_.is_null());
    base::UnguessableToken recursive_prefetch_token =
        std::move(recursive_prefetch_token_generator_).Run(resource_request_);
    response->recursive_prefetch_token = recursive_prefetch_token;
  }

  // Just drop any cached metadata; we don't need to forward it to the renderer
  // for prefetch.
  cached_metadata.reset();

  if (!body) {
    forwarding_client_->OnReceiveResponse(std::move(response),
                                          mojo::ScopedDataPipeConsumerHandle(),
                                          std::nullopt);
    return;
  }

  response_ = std::move(response);
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnStartLoadingResponseBody(
        std::move(body));
    return;
  }

  // Just drain the original response's body here.
  DCHECK(!pipe_drainer_);
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));

  SendEmptyBody();
}

void PrefetchURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnReceiveSignedExchange(
        signed_exchange_prefetch_handler_
            ->TakePrefetchedSignedExchangeCacheEntry());
  }

  resource_request_.url = redirect_info.new_url;
  resource_request_.site_for_cookies = redirect_info.new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info.new_referrer);
  resource_request_.referrer_policy = redirect_info.new_referrer_policy;
  forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchURLLoader::OnUploadProgress(int64_t current_position,
                                         int64_t total_size,
                                         base::OnceCallback<void()> callback) {
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
}

void PrefetchURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kPrefetchURLLoader);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (prefetched_signed_exchange_cache_adapter_ &&
      signed_exchange_prefetch_handler_) {
    prefetched_signed_exchange_cache_adapter_->OnComplete(status);
    return;
  }

  SendOnComplete(status);
}

bool PrefetchURLLoader::SendEmptyBody() {
  // Send an empty response's body.
  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (CreateDataPipe(nullptr, producer, consumer) != MOJO_RESULT_OK) {
    // No more resources available for creating a data pipe. Close the
    // connection, which will in turn make this loader destroyed.
    forwarding_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
    forwarding_client_.reset();
    client_receiver_.reset();
    return false;
  }
  DCHECK(response_);
  forwarding_client_->OnReceiveResponse(std::move(response_),
                                        std::move(consumer), std::nullopt);
  return true;
}

void PrefetchURLLoader::SendOnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  forwarding_client_->OnComplete(completion_status);
}

void PrefetchURLLoader::OnNetworkConnectionError() {
  // The network loader has an error; we should let the client know it's closed
  // by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

}  // namespace content
