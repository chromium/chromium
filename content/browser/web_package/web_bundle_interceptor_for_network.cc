// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_network.h"

#include "base/strings/stringprintf.h"
#include "components/web_package/web_bundle_utils.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_redirect_url_loader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/download_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"

namespace content {

WebBundleInterceptorForNetwork::WebBundleInterceptorForNetwork(
    WebBundleDoneCallback done_callback,
    BrowserContext* browser_context,
    int frame_tree_node_id)
    : done_callback_(std::move(done_callback)),
      browser_context_(browser_context),
      frame_tree_node_id_(frame_tree_node_id) {
  DCHECK(browser_context_);
}

WebBundleInterceptorForNetwork::~WebBundleInterceptorForNetwork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForNetwork::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!reader_) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&WebBundleInterceptorForNetwork::StartResponse,
                         weak_factory_.GetWeakPtr())));
}

bool WebBundleInterceptorForNetwork::MaybeCreateLoaderForResponse(
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response_head,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors,
    bool* will_return_and_handle_unsafe_redirect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if ((*response_head)->mime_type !=
      web_bundle_utils::kWebBundleFileMimeTypeWithoutParameters) {
    return false;
  }
  if (download_utils::MustDownload(request.url, (*response_head)->headers.get(),
                                   (*response_head)->mime_type)) {
    return false;
  }
  *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();

  if (!web_package::HasNoSniffHeader(**response_head)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kNoSniffErrorMessage);
    return true;
  }
  auto source = WebBundleSource::MaybeCreateFromNetworkUrl(request.url);
  if (!source) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        "Web Bundle response must be served from HTTPS or localhost HTTP.");
    return true;
  }

  uint64_t length_hint =
      (*response_head)->content_length > 0
          ? static_cast<uint64_t>((*response_head)->content_length)
          : 0;

  // TODO(crbug.com/1018640): Check the special HTTP response header if we
  // decided to require one for WBN navigation.

  reader_ = base::MakeRefCounted<WebBundleReader>(
      std::move(source), length_hint, std::move(*response_body),
      url_loader->Unbind(), browser_context_->GetBlobStorageContext());
  reader_->ReadMetadata(
      base::BindOnce(&WebBundleInterceptorForNetwork::OnMetadataReady,
                     weak_factory_.GetWeakPtr(), request));
  return true;
}

void WebBundleInterceptorForNetwork::OnMetadataReady(
    network::ResourceRequest request,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  if (error) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::GetMetadataParseErrorMessage(error));
    return;
  }
  primary_url_ = reader_->GetPrimaryURL().value_or(GURL());
  if (primary_url_.is_empty()) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kNoPrimaryUrlErrorMessage);
    return;
  }
  if (!web_bundle_utils::IsAllowedExchangeUrl(primary_url_)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kInvalidPrimaryUrlErrorMessage);
    return;
  }
  if (!base::ranges::all_of(reader_->GetEntries(),
                            &web_bundle_utils::IsAllowedExchangeUrl)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kInvalidExchangeUrlErrorMessage);
    return;
  }
  if (!reader_->HasEntry(primary_url_)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        "The primary URL resource is not found in the web bundle.");
    return;
  }
  if (primary_url_.DeprecatedGetOriginAsURL() !=
      reader_->source().url().DeprecatedGetOriginAsURL()) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        "The origin of primary URL doesn't match with the origin of the web "
        "bundle.");
    return;
  }
  if (!reader_->source().IsPathRestrictionSatisfied(primary_url_)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        base::StringPrintf("Path restriction mismatch: Can't navigate to %s "
                           "in the web bundle served from %s.",
                           primary_url_.spec().c_str(),
                           reader_->source().url().spec().c_str()));
    return;
  }
  url_loader_factory_ =
      std::make_unique<WebBundleURLLoaderFactory>(reader_, frame_tree_node_id_);
  auto redirect_loader =
      std::make_unique<WebBundleRedirectURLLoader>(forwarding_client_.Unbind());
  redirect_loader->OnReadyToRedirect(request, primary_url_);
}

void WebBundleInterceptorForNetwork::StartResponse(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  network::ResourceRequest new_resource_request = resource_request;
  new_resource_request.url = primary_url_;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), 0, 0, new_resource_request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(
          web_bundle_utils::kTrafficAnnotation));
  std::move(done_callback_).Run(primary_url_, std::move(url_loader_factory_));
}
}  // namespace content
