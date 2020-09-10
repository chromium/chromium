// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_URL_LOADER_FACTORY_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

class NetworkContext;
class ResourceSchedulerClient;
class URLLoader;

namespace cors {
class CorsURLLoaderFactory;
}  // namespace cors

// This class is an implementation of mojom::URLLoaderFactory that
// creates a mojom::URLLoader.
// A URLLoaderFactory has a pointer to ResourceSchedulerClient. A
// ResourceSchedulerClient is associated with cloned
// NetworkServiceURLLoaderFactories. Roughly one URLLoaderFactory
// is created for one frame in render process, so it means ResourceScheduler
// works on each frame.
// A URLLoaderFactory can be created with null ResourceSchedulerClient, in which
// case requests constructed by the factory will not be throttled.
// The CORS related part is implemented in CorsURLLoader[Factory] until
// kOutOfBlinkCors and kNetworkService is fully enabled. Note that
// NetworkContext::CreateURLLoaderFactory returns a CorsURLLoaderFactory,
// instead of a URLLoaderFactory.
class URLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  // NOTE: |context| must outlive this instance.
  URLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      cors::CorsURLLoaderFactory* cors_url_loader_factory);

  ~URLLoaderFactory() override;

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& url_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  static constexpr int kMaxKeepaliveConnections = 2048;
  static constexpr int kMaxKeepaliveConnectionsPerTopLevelFrame = 256;
  static constexpr int kMaxTotalKeepaliveRequestSize = 512 * 1024;

 private:
  // The NetworkContext that indirectly owns |this|.
  NetworkContext* const context_;
  mojom::URLLoaderFactoryParamsPtr params_;
  scoped_refptr<ResourceSchedulerClient> resource_scheduler_client_;
  mojo::Remote<mojom::TrustedURLLoaderHeaderClient> header_client_;
  mojo::Remote<mojom::CrossOriginEmbedderPolicyReporter> coep_reporter_;

  // |cors_url_loader_factory_| owns this.
  cors::CorsURLLoaderFactory* cors_url_loader_factory_;

  mojo::Remote<mojom::CookieAccessObserver> cookie_observer_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_FACTORY_H_
