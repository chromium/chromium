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
  DCHECK(factories_.find(web_bundle_token_params.token) == factories_.end());

  mojo::Remote<mojom::WebBundleHandle> remote(
      web_bundle_token_params.CloneHandle());

  // Set a disconnect handler to remove a WebBundleURLLoaderFactory from this
  // WebBundleManager when the corresponding endpoint in the renderer is
  // removed.
  remote.set_disconnect_handler(
      base::BindOnce(&WebBundleManager::DisconnectHandler,
                     // |this| outlives |remote|.
                     base::Unretained(this), web_bundle_token_params.token));

  auto factory = std::make_unique<WebBundleURLLoaderFactory>(
      bundle_url, std::move(remote),
      factory_params->request_initiator_origin_lock);
  auto weak_factory = factory->GetWeakPtr();
  factories_.insert({web_bundle_token_params.token, std::move(factory)});

  return weak_factory;
}

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::GetWebBundleURLLoaderFactory(
    const base::UnguessableToken& web_bundle_token) {
  auto it = factories_.find(web_bundle_token);
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void WebBundleManager::DisconnectHandler(
    base::UnguessableToken web_bundle_token) {
  factories_.erase(web_bundle_token);
}

}  // namespace network
