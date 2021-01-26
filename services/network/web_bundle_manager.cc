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

void WebBundleManager::DisconnectHandler(
    base::UnguessableToken web_bundle_token,
    int32_t process_id) {
  factories_.erase({process_id, web_bundle_token});
}

}  // namespace network
