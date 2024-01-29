// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "services/network/web_bundle/web_bundle_memory_quota_consumer.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"

namespace network {

class WebBundleManager::MemoryQuotaConsumer
    : public WebBundleMemoryQuotaConsumer {
 public:
  MemoryQuotaConsumer(base::WeakPtr<WebBundleManager> manager,
                      int32_t process_id)
      : manager_(std::move(manager)), process_id_(process_id) {}
  MemoryQuotaConsumer(const MemoryQuotaConsumer&) = delete;
  MemoryQuotaConsumer& operator=(const MemoryQuotaConsumer&) = delete;

  ~MemoryQuotaConsumer() override {
    if (!manager_)
      return;
    manager_->ReleaseMemoryForProcess(process_id_, allocated_bytes_);
  }

  bool AllocateMemory(uint64_t num_bytes) override {
    if (!manager_)
      return false;
    if (!manager_->AllocateMemoryForProcess(process_id_, num_bytes))
      return false;
    allocated_bytes_ += num_bytes;
    return true;
  }

 private:
  base::WeakPtr<WebBundleManager> manager_;
  const int32_t process_id_;
  uint64_t allocated_bytes_ = 0;
};

WebBundleManager::WebBundleManager()
    : max_memory_per_process_(web_package::kDefaultMaxMemoryPerProcess) {}

WebBundleManager::~WebBundleManager() = default;

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::CreateWebBundleURLLoaderFactory(
    const GURL& bundle_url,
    const ResourceRequest::WebBundleTokenParams& web_bundle_token_params,
    int32_t process_id,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    std::optional<std::string> devtools_request_id,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojom::CrossOriginEmbedderPolicyReporter* coep_reporter) {
  Key key = GetKey(web_bundle_token_params, process_id);
  DCHECK(factories_.find(key) == factories_.end());
  DCHECK(web_bundle_token_params.handle.is_valid());
  DCHECK_NE(process_id, mojom::kBrowserProcessId);

  mojo::Remote<mojom::WebBundleHandle> remote(
      web_bundle_token_params.CloneHandle());

  // Set a disconnect handler to remove a WebBundleURLLoaderFactory from this
  // WebBundleManager when the corresponding endpoint in the renderer is
  // removed.
  remote.set_disconnect_handler(
      base::BindOnce(&WebBundleManager::DisconnectHandler,
                     // |this| outlives |remote|.
                     base::Unretained(this), key));

  auto factory = std::make_unique<WebBundleURLLoaderFactory>(
      bundle_url, web_bundle_token_params, std::move(remote),
      std::make_unique<MemoryQuotaConsumer>(weak_ptr_factory_.GetWeakPtr(),
                                            process_id),
      std::move(devtools_observer), std::move(devtools_request_id),
      cross_origin_embedder_policy, coep_reporter);

  // Process pending subresource loaders if there are.
  // These subresource requests arrived earlier than the request for the bundle.
  auto it = pending_loaders_.find(key);
  if (it != pending_loaders_.end()) {
    for (auto& loader : it->second)
      factory->StartLoader(loader);
    pending_loaders_.erase(it);
  }

  auto weak_factory = factory->GetWeakPtr();
  factories_.insert({key, std::move(factory)});

  return weak_factory;
}

WebBundleManager::Key WebBundleManager::GetKey(
    const ResourceRequest::WebBundleTokenParams& token_params,
    int32_t process_id) {
  // If the request is from the browser process, use
  // WebBundleTokenParams::render_process_id for matching.
  if (process_id == mojom::kBrowserProcessId)
    process_id = token_params.render_process_id;

  return {process_id, token_params.token};
}

base::WeakPtr<WebBundleURLLoaderFactory>
WebBundleManager::GetWebBundleURLLoaderFactory(const Key& key) {
  auto it = factories_.find(key);
  if (it == factories_.end()) {
    return nullptr;
  }
  return it->second->GetWeakPtr();
}

void WebBundleManager::StartSubresourceRequest(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    const ResourceRequest& url_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    int32_t process_id,
    mojo::Remote<mojom::TrustedHeaderClient> trusted_header_client) {
  DCHECK(url_request.web_bundle_token_params.has_value());
  DCHECK(!url_request.web_bundle_token_params->handle.is_valid());

  Key key = GetKey(*url_request.web_bundle_token_params, process_id);
  base::WeakPtr<WebBundleURLLoaderFactory> web_bundle_url_loader_factory =
      GetWebBundleURLLoaderFactory(key);
  base::Time request_start_time = base::Time::Now();
  base::TimeTicks request_start_time_ticks = base::TimeTicks::Now();
  if (web_bundle_url_loader_factory) {
    auto loader = WebBundleURLLoaderFactory::CreateURLLoader(
        std::move(receiver), url_request, std::move(client),
        std::move(trusted_header_client), request_start_time,
        request_start_time_ticks, base::DoNothing());
    web_bundle_url_loader_factory->StartLoader(loader);
    return;
  }

  // A request for subresource arrives earlier than a request for a webbundle.
  pending_loaders_[key].push_back(WebBundleURLLoaderFactory::CreateURLLoader(
      std::move(receiver), url_request, std::move(client),
      std::move(trusted_header_client), request_start_time,
      request_start_time_ticks,
      base::BindOnce(&WebBundleManager::CleanUpWillBeDeletedURLLoader,
                     weak_ptr_factory_.GetWeakPtr(), key)));
}

void WebBundleManager::CleanUpWillBeDeletedURLLoader(
    Key key,
    WebBundleURLLoaderFactory::URLLoader* will_be_deleted_url_loader) {
  auto it = pending_loaders_.find(key);
  if (it == pending_loaders_.end())
    return;

  // Since we use std::vector for holding pending loaders, the clean up may take
  // O(N^2) if CleanUpWillBeDeletedURLLoader is called repeatedly. We might want
  // to use more appropriate data structure if this becomes a performance
  // issue. As of now, this happens only in non-regular cases; the bundle is
  // blocked by, for example, Chrome Extensions APIs.

  it->second.erase(base::ranges::remove_if(
                       it->second,
                       [will_be_deleted_url_loader](auto pending_loader) {
                         return !pending_loader ||
                                pending_loader.get() ==
                                    will_be_deleted_url_loader;
                       }),
                   it->second.end());

  if (it->second.empty()) {
    pending_loaders_.erase(key);
  }
}

void WebBundleManager::DisconnectHandler(Key key) {
  factories_.erase(key);
  DCHECK(!base::Contains(pending_loaders_, key));
}

bool WebBundleManager::AllocateMemoryForProcess(int32_t process_id,
                                                uint64_t num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (memory_usage_per_process_[process_id] + num_bytes >
      max_memory_per_process_) {
    return false;
  }
  memory_usage_per_process_[process_id] += num_bytes;

  if (max_memory_usage_per_process_[process_id] <
      memory_usage_per_process_[process_id]) {
    max_memory_usage_per_process_[process_id] =
        memory_usage_per_process_[process_id];
  }
  return true;
}

void WebBundleManager::ReleaseMemoryForProcess(int32_t process_id,
                                               uint64_t num_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(memory_usage_per_process_[process_id], num_bytes);
  memory_usage_per_process_[process_id] -= num_bytes;
  if (memory_usage_per_process_[process_id] == 0) {
    memory_usage_per_process_.erase(process_id);
    base::UmaHistogramCustomCounts(
        "SubresourceWebBundles.MaxMemoryUsagePerProcess",
        max_memory_usage_per_process_[process_id], 1, 50000000, 50);
    max_memory_usage_per_process_.erase(process_id);
  }
}

}  // namespace network
