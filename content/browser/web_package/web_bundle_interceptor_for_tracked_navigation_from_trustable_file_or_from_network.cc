// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_tracked_navigation_from_trustable_file_or_from_network.h"

#include "base/command_line.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork::
    WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork(
        scoped_refptr<WebBundleReader> reader,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : url_loader_factory_(
          std::make_unique<WebBundleURLLoaderFactory>(std::move(reader),
                                                      frame_tree_node_id)),
      done_callback_(std::move(done_callback)) {
  DCHECK((base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kTrustableWebBundleFileUrl) &&
          url_loader_factory_->reader()->source().is_trusted_file()) ||
         (base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork) &&
          url_loader_factory_->reader()->source().is_network()));
}

WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork::
    ~WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork::
    MaybeCreateLoader(const network::ResourceRequest& resource_request,
                      BrowserContext* browser_context,
                      LoaderCallback callback,
                      FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_factory_->reader()->HasEntry(resource_request.url));
  DCHECK(url_loader_factory_->reader()->source().is_trusted_file() ||
         (url_loader_factory_->reader()->source().is_network() &&
          url_loader_factory_->reader()->source().IsPathRestrictionSatisfied(
              resource_request.url)));
  std::move(callback).Run(base::MakeRefCounted<
                          network::
                              SingleRequestURLLoaderFactory>(base::BindOnce(
      &WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork::
          CreateURLLoader,
      weak_factory_.GetWeakPtr())));
}

void WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork::
    CreateURLLoader(
        const network::ResourceRequest& resource_request,
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), /*request_id=*/0,
      /*options=*/0, resource_request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(
          web_bundle_utils::kTrafficAnnotation));
  std::move(done_callback_)
      .Run(resource_request.url, std::move(url_loader_factory_));
}
}  // namespace content
