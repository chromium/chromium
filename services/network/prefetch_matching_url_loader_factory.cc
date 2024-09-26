// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_matching_url_loader_factory.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"

namespace network {

PrefetchMatchingURLLoaderFactory::PrefetchMatchingURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
    const cors::OriginAccessList* origin_access_list,
    PrefetchCache* cache)
    : next_(std::make_unique<cors::CorsURLLoaderFactory>(
          context,
          std::move(params),
          std::move(resource_scheduler_client),
          mojo::PendingReceiver<mojom::URLLoaderFactory>(),
          origin_access_list,
          this)),
      origin_access_list_(origin_access_list),
      context_(context),
      cache_(cache) {
  receivers_.Add(this, std::move(receiver));
  // This use of base::Unretained() is safe because `receivers_` won't call the
  // disconnect handler after it has been destroyed.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &PrefetchMatchingURLLoaderFactory::OnDisconnect, base::Unretained(this)));
}

PrefetchMatchingURLLoaderFactory::~PrefetchMatchingURLLoaderFactory() = default;

void PrefetchMatchingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // TODO: crbug.com/332706093 - This path was getting hit by the
  // iphone-simulator bot in unittests. See if this can be turned into a
  // NOTREACHED().
  ResourceRequest copy(request);
  CreateLoaderAndStart(std::move(loader), request_id, options, copy,
                       std::move(client), traffic_annotation);
}

void PrefetchMatchingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    ResourceRequest& request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // TODO(ricea): Actually match prefetches here.

  next_->CreateLoaderAndStart(std::move(loader), request_id, options, request,
                              std::move(client), traffic_annotation);
}

void PrefetchMatchingURLLoaderFactory::Clone(
    mojo::PendingReceiver<URLLoaderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrefetchMatchingURLLoaderFactory::ClearBindings() {
  receivers_.Clear();
  next_->ClearBindings();
}

net::handles::NetworkHandle
PrefetchMatchingURLLoaderFactory::GetBoundNetworkForTesting() const {
  return next_->GetBoundNetworkForTesting();  // IN-TEST
}

void PrefetchMatchingURLLoaderFactory::
    CancelRequestsIfNonceMatchesAndUrlNotExempted(
        const base::UnguessableToken& nonce,
        const std::set<GURL>& exemptions) {
  next_->CancelRequestsIfNonceMatchesAndUrlNotExempted(nonce, exemptions);
}

void PrefetchMatchingURLLoaderFactory::DestroyURLLoaderFactory(
    cors::CorsURLLoaderFactory* factory) {
  CHECK_EQ(factory, next_.get());
  context_->DestroyURLLoaderFactory(this);
}

bool PrefetchMatchingURLLoaderFactory::HasAdditionalReferences() const {
  return !receivers_.empty();
}

cors::CorsURLLoaderFactory*
PrefetchMatchingURLLoaderFactory::GetCorsURLLoaderFactoryForTesting() {
  return next_.get();
}

void PrefetchMatchingURLLoaderFactory::OnDisconnect() {
  if (receivers_.empty()) {
    next_->ClearBindings();
    // `this` may be deleted here.
  }
}

}  // namespace network
