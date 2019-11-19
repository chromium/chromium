// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
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
  // |origin_access_list| should always outlive this factory instance.
  // Used by network::NetworkContext. |network_loader_factory_for_testing|
  // should be nullptr unless you need to overwrite the default factory for
  // testing.
  CorsURLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      const OriginAccessList* origin_access_list,
      std::unique_ptr<mojom::URLLoaderFactory>
          network_loader_factory_for_testing);
  ~CorsURLLoaderFactory() override;

  void OnLoaderCreated(std::unique_ptr<mojom::URLLoader> loader);
  void DestroyURLLoader(mojom::URLLoader* loader);

  // Clears the bindings for this factory, but does not touch any in-progress
  // URLLoaders.
  void ClearBindings();

  uint32_t process_id() const { return process_id_; }

  // Set whether the factory allows CORS preflights. See IsSane.
  static void SetAllowExternalPreflightsForTesting(bool allow) {
    allow_external_preflights_for_testing_ = allow;
  }

 private:
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

  bool IsSane(const NetworkContext* context,
              const ResourceRequest& request,
              uint32_t options);

  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;

  // Used when constructed by NetworkContext.
  // The NetworkContext owns |this|.
  NetworkContext* const context_ = nullptr;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;

  // If false, ResourceRequests cannot have their |trusted_params| fields set.
  bool is_trusted_;

  // Retained from URLLoaderFactoryParams:
  const bool disable_web_security_;
  const uint32_t process_id_ = mojom::kInvalidProcessId;
  const base::Optional<url::Origin> request_initiator_site_lock_;

  // Relative order of |network_loader_factory_| and |loaders_| matters -
  // URLLoaderFactory needs to live longer than URLLoaders created using the
  // factory.  See also https://crbug.com/906305.
  std::unique_ptr<mojom::URLLoaderFactory> network_loader_factory_;
  std::set<std::unique_ptr<mojom::URLLoader>, base::UniquePtrComparator>
      loaders_;

  // Accessed by instances in |loaders_| too. Since the factory outlives them,
  // it's safe.
  const OriginAccessList* const origin_access_list_;

  // Owns factory bound OriginAccessList that to have factory specific
  // additional allowed access list.
  std::unique_ptr<OriginAccessList> factory_bound_origin_access_list_;

  static bool allow_external_preflights_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(CorsURLLoaderFactory);
};

}  // namespace cors

}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_FACTORY_H_
