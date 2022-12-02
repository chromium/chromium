// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_trustable_file.h"

#include "base/bind.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_redirect_url_loader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"

namespace content {

WebBundleInterceptorForTrustableFile::WebBundleInterceptorForTrustableFile(
    std::unique_ptr<WebBundleSource> source,
    WebBundleDoneCallback done_callback,
    int frame_tree_node_id)
    : source_(std::move(source)),
      reader_(base::MakeRefCounted<WebBundleReader>(source_->Clone())),
      done_callback_(std::move(done_callback)),
      frame_tree_node_id_(frame_tree_node_id) {
  reader_->ReadMetadata(
      base::BindOnce(&WebBundleInterceptorForTrustableFile::OnMetadataReady,
                     weak_factory_.GetWeakPtr()));
}
WebBundleInterceptorForTrustableFile::~WebBundleInterceptorForTrustableFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForTrustableFile::MaybeCreateLoader(
    const network::ResourceRequest& resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(
      base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
          base::BindOnce(&WebBundleInterceptorForTrustableFile::CreateURLLoader,
                         weak_factory_.GetWeakPtr())));
}

void WebBundleInterceptorForTrustableFile::CreateURLLoader(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (metadata_error_) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        mojo::Remote<network::mojom::URLLoaderClient>(std::move(client)),
        frame_tree_node_id_, *metadata_error_);
    return;
  }

  if (!url_loader_factory_) {
    // This must be the first request to the Web Bundle file.
    DCHECK_EQ(source_->url(), resource_request.url);
    pending_resource_request_ = resource_request;
    pending_receiver_ = std::move(receiver);
    pending_client_ = std::move(client);
    return;
  }

  // Currently |source_| must be a local file. And the bundle's primary URL
  // can't be a local file URL. So while handling redirected request to the
  // primary URL, |resource_request.url| must not be same as the |source_|'s
  // URL.
  if (source_->url() != resource_request.url) {
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), /*request_id=*/0,
        /*options=*/0, resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(
            web_bundle_utils::kTrafficAnnotation));
    std::move(done_callback_)
        .Run(resource_request.url, std::move(url_loader_factory_));
    return;
  }

  auto redirect_loader =
      std::make_unique<WebBundleRedirectURLLoader>(std::move(client));
  redirect_loader->OnReadyToRedirect(resource_request, primary_url_);
  mojo::MakeSelfOwnedReceiver(
      std::move(redirect_loader),
      mojo::PendingReceiver<network::mojom::URLLoader>(std::move(receiver)));
}

void WebBundleInterceptorForTrustableFile::OnMetadataReady(
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url_loader_factory_);

  if (error) {
    metadata_error_ =
        web_bundle_utils::GetMetadataParseErrorMessage(std::move(error));
  } else {
    primary_url_ = reader_->GetPrimaryURL().value_or(GURL());

    if (primary_url_.is_empty()) {
      metadata_error_ = web_bundle_utils::kNoPrimaryUrlErrorMessage;
    } else if (!web_bundle_utils::IsAllowedExchangeUrl(primary_url_)) {
      metadata_error_ = web_bundle_utils::kInvalidPrimaryUrlErrorMessage;
    } else if (!base::ranges::all_of(reader_->GetEntries(),
                                     &web_bundle_utils::IsAllowedExchangeUrl)) {
      metadata_error_ = web_bundle_utils::kInvalidExchangeUrlErrorMessage;
    } else {
      url_loader_factory_ = std::make_unique<WebBundleURLLoaderFactory>(
          std::move(reader_), frame_tree_node_id_);
    }
  }

  if (pending_receiver_) {
    DCHECK(pending_client_);
    CreateURLLoader(pending_resource_request_, std::move(pending_receiver_),
                    std::move(pending_client_));
  }
}
}  // namespace content
