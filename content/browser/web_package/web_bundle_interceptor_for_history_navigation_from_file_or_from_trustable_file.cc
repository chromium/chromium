// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_from_file_or_from_trustable_file.h"

#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "url/gurl.h"

namespace content {

WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
    WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile(
        std::unique_ptr<WebBundleSource> source,
        const GURL& target_inner_url,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : WebBundleInterceptorForHistoryNavigation(target_inner_url,
                                               std::move(done_callback),
                                               frame_tree_node_id),
      reader_(base::MakeRefCounted<WebBundleReader>(std::move(source))) {
  reader_->ReadMetadata(base::BindOnce(
      &WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
          OnMetadataReady,
      weak_factory_.GetWeakPtr()));
}

WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
    ~WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile() =
        default;

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
    MaybeCreateLoader(const network::ResourceRequest& resource_request,
                      BrowserContext* browser_context,
                      LoaderCallback callback,
                      FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(
      base::MakeRefCounted<
          network::SingleRequestURLLoaderFactory>(base::BindOnce(
          &WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
              CreateURLLoader,
          weak_factory_.GetWeakPtr())));
}

void WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
    CreateURLLoader(
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
    pending_resource_request_ = resource_request;
    pending_receiver_ = std::move(receiver);
    pending_client_ = std::move(client);
    return;
  }
  CreateLoaderAndStartAndDone(resource_request, std::move(receiver),
                              std::move(client));
}

void WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile::
    OnMetadataReady(web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!url_loader_factory_);

  if (error) {
    metadata_error_ =
        web_bundle_utils::GetMetadataParseErrorMessage(std::move(error));
  } else {
    const absl::optional<GURL>& primary_url = reader_->GetPrimaryURL();
    if (!primary_url.has_value()) {
      metadata_error_ = web_bundle_utils::kNoPrimaryUrlErrorMessage;
    } else if (!web_bundle_utils::IsAllowedExchangeUrl(*primary_url)) {
      metadata_error_ = web_bundle_utils::kInvalidPrimaryUrlErrorMessage;
    } else if (!base::ranges::all_of(reader_->GetEntries(),
                                     &web_bundle_utils::IsAllowedExchangeUrl)) {
      metadata_error_ = web_bundle_utils::kInvalidExchangeUrlErrorMessage;
    } else {
      CreateWebBundleURLLoaderFactory(std::move(reader_));
    }
  }

  if (pending_receiver_) {
    DCHECK(pending_client_);
    CreateURLLoader(pending_resource_request_, std::move(pending_receiver_),
                    std::move(pending_client_));
  }
}
}  // namespace content
