// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_
#define SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {

class WebBundleURLLoaderFactory;

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

 private:
  void DisconnectHandler(base::UnguessableToken token, int32_t process_id);

  // Maps a tuple (PID, WebBundle token) to a WebBundleURLLoaderFactory.
  std::map<std::pair<int32_t, base::UnguessableToken>,
           std::unique_ptr<WebBundleURLLoaderFactory>>
      factories_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_
