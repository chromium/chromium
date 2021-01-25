// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

class NetworkContext;
class ResourceSchedulerClient;
class URLLoader;
struct ResourceRequest;

namespace cors {

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

  // Check if members in |headers| are permitted by |allowed_exempt_headers|.
  static bool IsValidCorsExemptHeaders(
      const base::flat_set<std::string>& allowed_exempt_headers,
      const net::HttpRequestHeaders& headers);

  // |origin_access_list| should always outlive this factory instance.
  // Used by network::NetworkContext.
  CorsURLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      const OriginAccessList* origin_access_list);
  ~CorsURLLoaderFactory() override;

  void OnLoaderCreated(std::unique_ptr<mojom::URLLoader> loader);
  void DestroyURLLoader(mojom::URLLoader* loader);

  // Clears the bindings for this factory, but does not touch any in-progress
  // URLLoaders. Calling this may delete this factory and remove it from the
  // network context.
  void ClearBindings();

  int32_t process_id() const { return process_id_; }

 private:
  class FactoryOverride;

  // Implements mojom::URLLoaderFactory.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  void DeleteIfNeeded();

  bool IsValidRequest(const ResourceRequest& request, uint32_t options);

  InitiatorLockCompatibility VerifyRequestInitiatorLockWithPluginCheck(
      uint32_t process_id,
      const base::Optional<url::Origin>& request_initiator_origin_lock,
      const base::Optional<url::Origin>& request_initiator);

  bool GetAllowAnyCorsExemptHeaderForBrowser() const;

  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;

  // Used when constructed by NetworkContext.
  // The NetworkContext owns |this|.
  NetworkContext* const context_ = nullptr;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  // If false, ResourceRequests cannot have their |trusted_params| fields set.
  bool is_trusted_;

  // Retained from URLLoaderFactoryParams:
  const bool disable_web_security_;
  const int32_t process_id_ = mojom::kInvalidProcessId;
  const base::Optional<url::Origin> request_initiator_origin_lock_;
  const bool ignore_isolated_world_origin_;
  const mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy_;
  net::IsolationInfo isolation_info_;
  const std::string debug_tag_;

  // Relative order of |network_loader_factory_| and |loaders_| matters -
  // URLLoaderFactory needs to live longer than URLLoaders created using the
  // factory.  See also https://crbug.com/906305.
  std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory_;

  // Used when the network loader factory is overridden.
  std::unique_ptr<FactoryOverride> factory_override_;

  std::set<std::unique_ptr<mojom::URLLoader>, base::UniquePtrComparator>
      loaders_;

  // Accessed by instances in |loaders_| too. Since the factory outlives them,
  // it's safe.
  const OriginAccessList* const origin_access_list_;

  static bool allow_external_preflights_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoaderFactory);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
