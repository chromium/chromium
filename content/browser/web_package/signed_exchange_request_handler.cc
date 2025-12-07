// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_request_handler.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/web_package/signed_exchange_devtools_proxy.h"
#include "content/browser/web_package/signed_exchange_loader.h"
#include "content/browser/web_package/signed_exchange_reporter.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

// static
bool SignedExchangeRequestHandler::IsSupportedMimeType(
    const std::string& mime_type) {
  return mime_type == "application/signed-exchange";
}

SignedExchangeRequestHandler::SignedExchangeRequestHandler(
    uint32_t url_loader_options,
    FrameTreeNodeId frame_tree_node_id,
    const base::UnguessableToken& devtools_navigation_token,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    std::string accept_langs)
    : url_loader_options_(url_loader_options),
      frame_tree_node_id_(frame_tree_node_id),
      devtools_navigation_token_(devtools_navigation_token),
      url_loader_factory_(url_loader_factory),
      url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
      accept_langs_(std::move(accept_langs)) {}

SignedExchangeRequestHandler::~SignedExchangeRequestHandler() = default;

void SignedExchangeRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  if (!signed_exchange_loader_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (signed_exchange_loader_->fallback_url()) {
    DCHECK(tentative_resource_request.url.EqualsIgnoringRef(
        *signed_exchange_loader_->fallback_url()));
    signed_exchange_loader_ = nullptr;
    // Skip subsequent interceptors and fallback to the network.
    std::move(callback).Run(NavigationLoaderInterceptor::Result(
        /*factory=*/nullptr, /*subresource_loader_params=*/{}));
    return;
  }

  DCHECK(tentative_resource_request.url.EqualsIgnoringRef(
      *signed_exchange_loader_->inner_request_url()));
  std::move(callback).Run(NavigationLoaderInterceptor::Result(
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&SignedExchangeRequestHandler::StartResponse,
                         weak_factory_.GetWeakPtr())),
      /*subresource_loader_params=*/{}));
}

bool SignedExchangeRequestHandler::MaybeCreateLoaderForResponse(
    const network::URLLoaderCompletionStatus& status,
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response_head,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors) {
  DCHECK(!signed_exchange_loader_);

  // Navigation ResourceRequests always have non-empty trusted_params.
  CHECK(request.trusted_params);

  if (!signed_exchange_utils::ShouldHandleAsSignedHTTPExchange(
          request.url, **response_head)) {
    return false;
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient> client;
  *client_receiver = client.InitWithNewPipeAndPassReceiver();

  const net::NetworkAnonymizationKey& network_anonymization_key =
      request.trusted_params->isolation_info.network_anonymization_key();
  // This lets the SignedExchangeLoader directly returns an artificial redirect
  // to the downstream client without going through blink::ThrottlingURLLoader,
  // which means some checks like SafeBrowsing may not see the redirect. Given
  // that the redirected request will be checked when it's restarted we suppose
  // this is fine.
  auto reporter = SignedExchangeReporter::MaybeCreate(
      request.url, request.referrer.spec(), **response_head,
      network_anonymization_key, frame_tree_node_id_);
  auto devtools_proxy = std::make_unique<SignedExchangeDevToolsProxy>(
      request.url, response_head->Clone(), frame_tree_node_id_,
      devtools_navigation_token_, request.devtools_request_id.has_value());
  signed_exchange_loader_ = std::make_unique<SignedExchangeLoader>(
      request, std::move(*response_head), std::move(*response_body),
      std::move(client), url_loader->Unbind(), url_loader_options_,
      true /* should_redirect_to_fallback */, std::move(devtools_proxy),
      std::move(reporter), url_loader_factory_, url_loader_throttles_getter_,
      frame_tree_node_id_, accept_langs_,
      false /* keep_entry_for_prefetch_cache */);

  *skip_other_interceptors = true;
  return true;
}

void SignedExchangeRequestHandler::StartResponse(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  signed_exchange_loader_->ConnectToClient(std::move(client));
  mojo::MakeSelfOwnedReceiver(std::move(signed_exchange_loader_),
                              std::move(receiver));
}

}  // namespace content
