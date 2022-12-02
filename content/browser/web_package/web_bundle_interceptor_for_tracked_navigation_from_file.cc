// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_tracked_navigation_from_file.h"

#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_redirect_url_loader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"

namespace content {

// A class to inherit NavigationLoaderInterceptor for a navigation within a
// Web Bundle file.
// For example:
//   A user opened "file:///tmp/a.wbn", and InterceptorForFile redirected to
//   "file:///tmp/a.wbn?https://example.com/a.html" and "a.html" in "a.wbn" was
//   loaded. And the user clicked a link to "https://example.com/b.html" from
//   "a.html".
// In this case, this interceptor intecepts the navigation request to "b.html",
// and redirect the navigation request to
// "file:///tmp/a.wbn?https://example.com/b.html" and creates a URLLoader using
// the WebBundleURLLoaderFactory to load the response of "b.html" in
// "a.wbn".
WebBundleInterceptorForTrackedNavigationFromFile::
    WebBundleInterceptorForTrackedNavigationFromFile(
        scoped_refptr<WebBundleReader> reader,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : url_loader_factory_(
          std::make_unique<WebBundleURLLoaderFactory>(std::move(reader),
                                                      frame_tree_node_id)),
      done_callback_(std::move(done_callback)) {}
WebBundleInterceptorForTrackedNavigationFromFile::
    ~WebBundleInterceptorForTrackedNavigationFromFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForTrackedNavigationFromFile::MaybeCreateLoader(
    const network::ResourceRequest& resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(
      base::MakeRefCounted<
          network::SingleRequestURLLoaderFactory>(base::BindOnce(
          &WebBundleInterceptorForTrackedNavigationFromFile::CreateURLLoader,
          weak_factory_.GetWeakPtr())));
}

bool WebBundleInterceptorForTrackedNavigationFromFile::
    ShouldBypassRedirectChecks() {
  return true;
}

void WebBundleInterceptorForTrackedNavigationFromFile::CreateURLLoader(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_redirected_) {
    DCHECK(url_loader_factory_->reader()->HasEntry(resource_request.url));
    is_redirected_ = true;
    original_request_url_ = resource_request.url;

    GURL web_bundle_url = url_loader_factory_->reader()->source().url();
    const GURL new_url = web_bundle_utils::GetSynthesizedUrlForWebBundle(
        web_bundle_url, original_request_url_);
    auto redirect_loader =
        std::make_unique<WebBundleRedirectURLLoader>(std::move(client));
    redirect_loader->OnReadyToRedirect(resource_request, new_url);
    mojo::MakeSelfOwnedReceiver(
        std::move(redirect_loader),
        mojo::PendingReceiver<network::mojom::URLLoader>(std::move(receiver)));
    return;
  }
  network::ResourceRequest new_resource_request = resource_request;
  new_resource_request.url = original_request_url_;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), /*request_id=*/0, /*options=*/0,
      new_resource_request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(
          web_bundle_utils::kTrafficAnnotation));
  std::move(done_callback_)
      .Run(original_request_url_, std::move(url_loader_factory_));
}
}  // namespace content
