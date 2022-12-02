// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_file.h"

#include "base/ranges/algorithm.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_redirect_url_loader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

namespace content {
WebBundleInterceptorForFile::WebBundleInterceptorForFile(
    WebBundleDoneCallback done_callback,
    int frame_tree_node_id)
    : done_callback_(std::move(done_callback)),
      frame_tree_node_id_(frame_tree_node_id) {}

WebBundleInterceptorForFile::~WebBundleInterceptorForFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebBundleInterceptorForFile::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // WebBundleInterceptorForFile::MaybeCreateLoader() creates a loader only
  // after recognising that the response is a Web Bundle file at
  // MaybeCreateLoaderForResponse() and successfully created
  // |url_loader_factory_|.
  if (!url_loader_factory_) {
    std::move(callback).Run({});
    return;
  }
  std::move(callback).Run(
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&WebBundleInterceptorForFile::StartResponse,
                         weak_factory_.GetWeakPtr())));
}

bool WebBundleInterceptorForFile::MaybeCreateLoaderForResponse(
    const network::ResourceRequest& request,
    network::mojom::URLResponseHeadPtr* response_head,
    mojo::ScopedDataPipeConsumerHandle* response_body,
    mojo::PendingRemote<network::mojom::URLLoader>* loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
    blink::ThrottlingURLLoader* url_loader,
    bool* skip_other_interceptors,
    bool* will_return_unsafe_redirect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(web_bundle_utils::IsSupportedFileScheme(request.url));
  if ((*response_head)->mime_type !=
      web_bundle_utils::kWebBundleFileMimeTypeWithoutParameters) {
    return false;
  }
  std::unique_ptr<WebBundleSource> source =
      WebBundleSource::MaybeCreateFromFileUrl(request.url);
  if (!source)
    return false;
  reader_ = base::MakeRefCounted<WebBundleReader>(std::move(source));
  reader_->ReadMetadata(
      base::BindOnce(&WebBundleInterceptorForFile::OnMetadataReady,
                     weak_factory_.GetWeakPtr(), request));
  *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
  *will_return_unsafe_redirect = true;
  return true;
}

void WebBundleInterceptorForFile::OnMetadataReady(
    const network::ResourceRequest& request,
    web_package::mojom::BundleMetadataParseErrorPtr metadata_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_error) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::GetMetadataParseErrorMessage(metadata_error));
    return;
  }
  DCHECK(reader_);
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

  url_loader_factory_ = std::make_unique<WebBundleURLLoaderFactory>(
      std::move(reader_), frame_tree_node_id_);

  const GURL new_url = web_bundle_utils::GetSynthesizedUrlForWebBundle(
      request.url, primary_url_);
  auto redirect_loader =
      std::make_unique<WebBundleRedirectURLLoader>(forwarding_client_.Unbind());
  redirect_loader->OnReadyToRedirect(request, new_url);
}

void WebBundleInterceptorForFile::StartResponse(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network::ResourceRequest new_resource_request = resource_request;
  new_resource_request.url = primary_url_;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), /*request_id=*/0, /*options=*/0,
      new_resource_request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(
          web_bundle_utils::kTrafficAnnotation));
  std::move(done_callback_).Run(primary_url_, std::move(url_loader_factory_));
}
}  // namespace content
