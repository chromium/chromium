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
// which is created for each WebBundle. And also manages the quota of memory
// usage.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebBundleManager {
 public:
  WebBundleManager();
  ~WebBundleManager();

  WebBundleManager(const WebBundleManager&) = delete;
  WebBundleManager& operator=(const WebBundleManager&) = delete;

  base::WeakPtr<WebBundleURLLoaderFactory> CreateWebBundleURLLoaderFactory(
      const GURL& bundle_url,
      const ResourceRequest::WebBundleTokenParams& params,
      int32_t process_id,
      const base::Optional<url::Origin>& request_initiator_origin_lock);

  void StartSubresourceRequest(
      mojo::PendingReceiver<mojom::URLLoader> receiver,
      const ResourceRequest& url_request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      int32_t process_id,
      mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client);

 private:
  friend class WebBundleManagerTest;

  class MemoryQuotaConsumer;

  // Key is a tuple of (Process id, WebBundle token)
  using Key = std::pair<int32_t, base::UnguessableToken>;

  base::WeakPtr<WebBundleURLLoaderFactory> GetWebBundleURLLoaderFactory(
      const ResourceRequest::WebBundleTokenParams& params,
      int32_t process_id);

  void DisconnectHandler(base::UnguessableToken token, int32_t process_id);

  bool AllocateMemoryForProcess(int32_t process_id, uint64_t num_bytes);
  void ReleaseMemoryForProcess(int32_t process_id, uint64_t num_bytes);
  void set_max_memory_per_process_for_testing(uint64_t max_memory_per_process) {
    max_memory_per_process_ = max_memory_per_process;
  }

  std::map<Key, std::unique_ptr<WebBundleURLLoaderFactory>> factories_;
  // Pending subresource requests for each key, which should be processed when
  // a request for the bundle arrives later.
  std::map<Key,
           std::vector<std::unique_ptr<WebBundlePendingSubresourceRequest>>>
      pending_requests_;

  uint64_t max_memory_per_process_;
  std::map<int32_t, uint64_t> memory_usage_per_process_;
  std::map<int32_t, uint64_t> max_memory_usage_per_process_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebBundleManager> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_MANAGER_H_
