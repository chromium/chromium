// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PREFETCH_MATCHING_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PREFETCH_MATCHING_URL_LOADER_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/network_handle.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace base {
class UnguessableToken;
}

namespace net {
struct MutableNetworkTrafficAnnotationTag;
}

namespace network {

namespace cors {
class CorsURLLoaderFactory;
class OriginAccessList;
}  // namespace cors

class NetworkContext;
class PrefetchCache;
class ResourceSchedulerClient;
struct ResourceRequest;

// An implementation of URLLoaderFactory that attempts to match against a cache
// of available prefetches. It no matching prefetches are available, it falls
// back to the CORSUrlLoaderFactory.
class COMPONENT_EXPORT(NETWORK_SERVICE) PrefetchMatchingURLLoaderFactory final
    : public mojom::URLLoaderFactory {
 public:
  // If `cache` is nullptr this class will operate in pass-through mode. Every
  // request will be sent to CORSURLLoaderFactory and no matching will be
  // attempted.
  PrefetchMatchingURLLoaderFactory(
      NetworkContext* context,
      mojom::URLLoaderFactoryParamsPtr params,
      scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
      mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
      const cors::OriginAccessList* origin_access_list,
      PrefetchCache* cache);

  PrefetchMatchingURLLoaderFactory(const PrefetchMatchingURLLoaderFactory&) =
      delete;
  PrefetchMatchingURLLoaderFactory& operator=(
      const PrefetchMatchingURLLoaderFactory&) = delete;

  ~PrefetchMatchingURLLoaderFactory() final;

  // Implementation of mojom::URLLoaderFactory.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) final;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      ResourceRequest& request,
      mojo::PendingRemote<mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) final;

  void Clone(mojo::PendingReceiver<URLLoaderFactory> receiver) final;

  // Methods called by NetworkContext.
  void ClearBindings();

  net::handles::NetworkHandle GetBoundNetworkForTesting() const;

  void CancelRequestsIfNonceMatchesAndUrlNotExempted(
      const base::UnguessableToken& nonce,
      const std::set<GURL>& exemptions);

  // Methods called from CorsURLLoaderFactory.
  void DestroyURLLoaderFactory(cors::CorsURLLoaderFactory* factory);

  // Returns true if there are live mojo connections requiring this class and
  // the CorsURLLoaderFactory it owns be kept alive.
  bool HasAdditionalReferences() const;

  // Returns the owned CorsURLLoaderFactory for unit tests. It's not a good idea
  // to call this and also call other methods on this object.
  cors::CorsURLLoaderFactory* GetCorsURLLoaderFactoryForTesting();

 private:
  void OnDisconnect();

  const std::unique_ptr<cors::CorsURLLoaderFactory> next_;
  const raw_ptr<const cors::OriginAccessList> origin_access_list_;
  const raw_ptr<NetworkContext> context_;
  const raw_ptr<PrefetchCache> cache_;
  mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PREFETCH_MATCHING_URL_LOADER_FACTORY_H_
