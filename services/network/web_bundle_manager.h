// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_
#define SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

class WebBundleURLLoaderFactory;
struct WebBundlePendingSubresourceRequest;

// WebBundleManager manages the lifetime of a WebBundleURLLoaderFactory object,
// which is created for each WebBundle.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleManager {
 public:
  WebBundleManager();
  ~WebBundleManager();

  WebBundleManager(const WebBundleManager&) = delete;
  WebBundleManager& operator=(const WebBundleManager&) = delete;

  base::WeakPtr<WebBundleURLLoaderFactory> CreateWebBundleURLLoaderFactory(
      const GURL& bundle_url,
      const ResourceRequest::WebBundleTokenParams& params,
      const mojom::URLLoaderFactoryParamsPtr& factory_params);

  base::WeakPtr<WebBundleURLLoaderFactory> GetWebBundleURLLoaderFactory(
      const ResourceRequest::WebBundleTokenParams& params,
      int32_t process_id);

  void AddPendingSubresouceRequest(
      base::UnguessableToken token,
      int32_t process_id,
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

 private:
  void DisconnectHandler(base::UnguessableToken token, int32_t process_id);

  // Key is a tuple of (Process id, WebBundle token)
  using Key = std::pair<int32_t, base::UnguessableToken>;

  std::map<Key, std::unique_ptr<WebBundleURLLoaderFactory>> factories_;
  // Pending subresource requests for each key, which should be processed when
  // a request for the bundle arrives later.
  std::map<Key,
           std::vector<std::unique_ptr<WebBundlePendingSubresourceRequest>>>
      pending_requests_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_
