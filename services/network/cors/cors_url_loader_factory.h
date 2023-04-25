// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class ResourceSchedulerClient;
class URLLoader;
class URLLoaderFactory;
struct ResourceRequest;
class SharedDictionaryStorage;

namespace cors {
class CorsURLLoader;

// A factory class to create a URLLoader that supports CORS.
// This class takes a network::mojom::URLLoaderFactory instance in the
// constructor and owns it to make network requests for CORS-preflight, and
// actual network request.
class COMPONENT_EXPORT(NETWORK_SERVICE) CorsURLLoaderFactory final
    : public mojom::URLLoaderFactory {
 public:
  // Set whether the factory allows CORS preflights. See IsValidRequest.
  static void SetAllowExternalPreflightsForTesting(bool allow) {
    allow_external_preflights_for_testing_ = allow;
  }

  // Check if members in `headers` are permitted by `allowed_exempt_headers`.
  static bool IsValidCorsExemptHeaders(
      const base::flat_set<std::string>& allowed_exempt_headers,
      const net::HttpRequestHeaders& headers);

  // `origin_access_list` should always outlive this factory instance.
  // Used by network::NetworkContext.
  CorsURLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      const OriginAccessList* origin_access_list);

  CorsURLLoaderFactory(const CorsURLLoaderFactory&) = delete;
  CorsURLLoaderFactory& operator=(const CorsURLLoaderFactory&) = delete;

  ~CorsURLLoaderFactory() override;

  void OnURLLoaderCreated(std::unique_ptr<URLLoader> loader);
  void OnCorsURLLoaderCreated(std::unique_ptr<CorsURLLoader> loader);
  void DestroyURLLoader(URLLoader* loader);
  void DestroyCorsURLLoader(CorsURLLoader* loader);

  // Clears the bindings for this factory, but does not touch any in-progress
  // URLLoaders. Calling this may delete this factory and remove it from the
  // network context.
  void ClearBindings();

  int32_t process_id() const { return process_id_; }
  mojom::CrossOriginEmbedderPolicyReporter* coep_reporter() {
    return coep_reporter_ ? coep_reporter_.get() : nullptr;
  }

  std::set<std::unique_ptr<URLLoader>, base::UniquePtrComparator>&
  url_loaders() {
    return url_loaders_;
  }

 private:
  class FactoryOverride;

  // Implements mojom::URLLoaderFactory.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  void DeleteIfNeeded();

  bool IsValidRequest(const ResourceRequest& request, uint32_t options);

  bool GetAllowAnyCorsExemptHeaderForBrowser() const;

  mojo::PendingRemote<mojom::DevToolsObserver> GetDevToolsObserver(
      const ResourceRequest& resource_request) const;

  template <class T>
  void OnLoaderCreated(
      std::unique_ptr<T> loader,
      std::set<std::unique_ptr<T>, base::UniquePtrComparator>& loaders) {
    context_->LoaderCreated(process_id_);
    loaders.insert(std::move(loader));
  }

  template <class T>
  void DestroyLoader(
      T* loader,
      std::set<std::unique_ptr<T>, base::UniquePtrComparator>& loaders) {
    context_->LoaderDestroyed(process_id_);
    auto it = loaders.find(loader);
    DCHECK(it != loaders.end());
    loaders.erase(it);

    DeleteIfNeeded();
  }

  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;

  // The NetworkContext owns `this`. Initialized in the construct and must be
  // non-null.
  const raw_ptr<NetworkContext> context_ = nullptr;

  // If false, ResourceRequests cannot have their `trusted_params` fields set.
  bool is_trusted_;

  // Retained from URLLoaderFactoryParams:
  const bool disable_web_security_;
  const int32_t process_id_ = mojom::kInvalidProcessId;
  const absl::optional<url::Origin> request_initiator_origin_lock_;
  const bool ignore_isolated_world_origin_;
  const mojom::TrustTokenOperationPolicyVerdict trust_token_issuance_policy_;
  const mojom::TrustTokenOperationPolicyVerdict trust_token_redemption_policy_;
  net::IsolationInfo isolation_info_;
  const bool automatically_assign_isolation_info_;
  const std::string debug_tag_;
  const CrossOriginEmbedderPolicy cross_origin_embedder_policy_;
  mojo::Remote<mojom::CrossOriginEmbedderPolicyReporter> coep_reporter_;
  const mojom::ClientSecurityStatePtr client_security_state_;

  // Relative order of `network_loader_factory_` and `loaders_` matters -
  // URLLoaderFactory needs to live longer than URLLoaders created using the
  // factory.  See also https://crbug.com/906305.
  std::unique_ptr<network::URLLoaderFactory> network_loader_factory_;

  // Used when the network loader factory is overridden.
  std::unique_ptr<FactoryOverride> factory_override_;

  std::set<std::unique_ptr<URLLoader>, base::UniquePtrComparator> url_loaders_;
  std::set<std::unique_ptr<CorsURLLoader>, base::UniquePtrComparator>
      cors_url_loaders_;

  // Accessed by instances in `loaders_` too. Since the factory outlives them,
  // it's safe.
  const raw_ptr<const OriginAccessList> origin_access_list_;

  scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage_;

  static bool allow_external_preflights_for_testing_;
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
