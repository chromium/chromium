// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_with_existing_reader.h"

#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {

WebBundleInterceptorForHistoryNavigationWithExistingReader::
    WebBundleInterceptorForHistoryNavigationWithExistingReader(
        scoped_refptr<WebBundleReader> reader,
        const GURL& target_inner_url,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : WebBundleInterceptorForHistoryNavigation(target_inner_url,
                                               std::move(done_callback),
                                               frame_tree_node_id) {
  CreateWebBundleURLLoaderFactory(std::move(reader));
}

WebBundleInterceptorForHistoryNavigationWithExistingReader::
    ~WebBundleInterceptorForHistoryNavigationWithExistingReader() = default;

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForHistoryNavigationWithExistingReader::
    MaybeCreateLoader(const network::ResourceRequest& resource_request,
                      BrowserContext* browser_context,
                      LoaderCallback callback,
                      FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(net::SimplifyUrlForRequest(resource_request.url),
            url_loader_factory_->reader()->source().is_file()
                ? net::SimplifyUrlForRequest(
                      web_bundle_utils::GetSynthesizedUrlForWebBundle(
                          url_loader_factory_->reader()->source().url(),
                          target_inner_url_))
                : net::SimplifyUrlForRequest(target_inner_url_));
  std::move(callback).Run(
      base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
          &WebBundleInterceptorForHistoryNavigationWithExistingReader::
              CreateURLLoader,
          weak_factory_.GetWeakPtr())));
}

void WebBundleInterceptorForHistoryNavigationWithExistingReader::
    CreateURLLoader(
        const network::ResourceRequest& resource_request,
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(net::SimplifyUrlForRequest(resource_request.url),
            url_loader_factory_->reader()->source().is_file()
                ? net::SimplifyUrlForRequest(
                      web_bundle_utils::GetSynthesizedUrlForWebBundle(
                          url_loader_factory_->reader()->source().url(),
                          target_inner_url_))
                : net::SimplifyUrlForRequest(target_inner_url_));
  CreateLoaderAndStartAndDone(resource_request, std::move(receiver),
                              std::move(client));
}
}  // namespace content
