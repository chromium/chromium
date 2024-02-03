// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MANAGER_H_
#define SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MANAGER_H_

#include <map>
#include <optional>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

namespace network {

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
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
      std::optional<std::string> devtools_request_id,
      const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      mojom::CrossOriginEmbedderPolicyReporter* coep_reporter);

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

  static Key GetKey(const ResourceRequest::WebBundleTokenParams& token_params,
                    int32_t process_id);
  base::WeakPtr<WebBundleURLLoaderFactory> GetWebBundleURLLoaderFactory(
      const Key& key);

  void DisconnectHandler(Key key);

  bool AllocateMemoryForProcess(int32_t process_id, uint64_t num_bytes);
  void ReleaseMemoryForProcess(int32_t process_id, uint64_t num_bytes);
  void set_max_memory_per_process_for_testing(uint64_t max_memory_per_process) {
    max_memory_per_process_ = max_memory_per_process;
  }

  void CleanUpWillBeDeletedURLLoader(
      Key key,
      WebBundleURLLoaderFactory::URLLoader* will_be_deleted_url_loader);

  bool IsPendingLoadersEmptyForTesting(Key key) const {
    return pending_loaders_.find(key) == pending_loaders_.end();
  }

  std::map<Key, std::unique_ptr<WebBundleURLLoaderFactory>> factories_;
  // Pending subresource loaders for each key, which should be processed when
  // a request for the bundle arrives later.
  std::map<Key,
           std::vector<base::WeakPtr<WebBundleURLLoaderFactory::URLLoader>>>
      pending_loaders_;

  uint64_t max_memory_per_process_;
  std::map<int32_t, uint64_t> memory_usage_per_process_;
  std::map<int32_t, uint64_t> max_memory_usage_per_process_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebBundleManager> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_BUNDLE_WEB_BUNDLE_MANAGER_H_
