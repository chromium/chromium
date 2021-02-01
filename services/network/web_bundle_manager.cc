// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle_manager.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "services/network/web_bundle_url_loader_factory.h"

namespace network {

// Represents a pending subresource request.
struct WebBundlePendingSubresourceRequest {
  WebBundlePendingSubresourceRequest(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      : receiver(std::move(receiver)),
        routing_id(routing_id),
        request_id(request_id),
        options(options),
        url_request(url_request),
        client(std::move(client)),
        traffic_annotation(traffic_annotation) {}
  ~WebBundlePendingSubresourceRequest() = default;

  WebBundlePendingSubresourceRequest(
      const WebBundlePendingSubresourceRequest&) = delete;
  WebBundlePendingSubresourceRequest& operator=(
      const WebBundlePendingSubresourceRequest&) = delete;

  mojo::PendingReceiver<mojom::URLLoader> receiver;
  int32_t routing_id;
  int32_t request_id;
  uint32_t options;
  const ResourceRequest url_request;
  mojo::PendingRemote<mojom::URLLoaderClient> client;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation;
};

WebBundleManager::WebBundleManager() = default;

WebBundleManager::~WebBundleManager() = default;

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::CreateWebBundleURLLoaderFactory(
    const GURL& bundle_url,
    const ResourceRequest::WebBundleTokenParams& web_bundle_token_params,
    const mojom::URLLoaderFactoryParamsPtr& factory_params) {
  DCHECK(factories_.find({factory_params->process_id,
                          web_bundle_token_params.token}) == factories_.end());

  mojo::Remote<mojom::WebBundleHandle> remote(
      web_bundle_token_params.CloneHandle());

  // Set a disconnect handler to remove a WebBundleURLLoaderFactory from this
  // WebBundleManager when the corresponding endpoint in the renderer is
  // removed.
  remote.set_disconnect_handler(
      base::BindOnce(&WebBundleManager::DisconnectHandler,
                     // |this| outlives |remote|.
                     base::Unretained(this), web_bundle_token_params.token,
                     factory_params->process_id));

  auto factory = std::make_unique<WebBundleURLLoaderFactory>(
      bundle_url, std::move(remote),
      factory_params->request_initiator_origin_lock);

  // Process pending subresource requests if there are.
  // These subresource requests arrived earlier than the request for the bundle.
  auto it = pending_requests_.find(
      {factory_params->process_id, web_bundle_token_params.token});
  if (it != pending_requests_.end()) {
    for (auto& pending_request : it->second) {
      factory->CreateLoaderAndStart(
          std::move(pending_request->receiver), pending_request->routing_id,
          pending_request->request_id, pending_request->options,
          pending_request->url_request, std::move(pending_request->client),
          pending_request->traffic_annotation);
    }
    pending_requests_.erase(it);
  }

  auto weak_factory = factory->GetWeakPtr();
  factories_.insert({std::make_pair(factory_params->process_id,
                                    web_bundle_token_params.token),
                     std::move(factory)});

  return weak_factory;
}

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::GetWebBundleURLLoaderFactory(
    const ResourceRequest::WebBundleTokenParams& token_params,
    int32_t process_id) {
  // If the request is from the browser process, use
  // WebBundleTokenParams::render_process_id for matching.
  if (process_id == mojom::kBrowserProcessId)
    process_id = token_params.render_process_id;

  auto it = factories_.find({process_id, token_params.token});
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void WebBundleManager::AddPendingSubresouceRequest(
    base::UnguessableToken token,
    int32_t process_id,
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  pending_requests_[{process_id, token}].push_back(
      std::make_unique<WebBundlePendingSubresourceRequest>(
          std::move(receiver), routing_id, request_id, options, url_request,
          std::move(client), traffic_annotation));
}

void WebBundleManager::DisconnectHandler(
    base::UnguessableToken web_bundle_token,
    int32_t process_id) {
  factories_.erase({process_id, web_bundle_token});
  pending_requests_.erase({process_id, web_bundle_token});
}

}  // namespace network
