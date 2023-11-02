// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation.h"

#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_url_loader_factory.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace content {

WebBundleInterceptorForHistoryNavigation::
    WebBundleInterceptorForHistoryNavigation(
        const GURL& target_inner_url,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : target_inner_url_(target_inner_url),
      frame_tree_node_id_(frame_tree_node_id),
      done_callback_(std::move(done_callback)) {}

WebBundleInterceptorForHistoryNavigation::
    ~WebBundleInterceptorForHistoryNavigation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebBundleInterceptorForHistoryNavigation::CreateWebBundleURLLoaderFactory(
    scoped_refptr<WebBundleReader> reader) {
  DCHECK(reader->HasEntry(target_inner_url_));
  url_loader_factory_ = std::make_unique<WebBundleURLLoaderFactory>(
      std::move(reader), frame_tree_node_id_);
}

void WebBundleInterceptorForHistoryNavigation::CreateLoaderAndStartAndDone(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  network::ResourceRequest new_resource_request = resource_request;
  new_resource_request.url = target_inner_url_;
  url_loader_factory_->CreateLoaderAndStart(
      std::move(receiver), /*request_id=*/0,
      /*options=*/0, new_resource_request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(
          web_bundle_utils::kTrafficAnnotation));
  std::move(done_callback_)
      .Run(target_inner_url_, std::move(url_loader_factory_));
}
}  // namespace content
