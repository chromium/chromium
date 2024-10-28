// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_matching_url_loader_factory.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/prefetch_cache.h"
#include "services/network/prefetch_url_loader_client.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
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
      context_(context),
      cache_(cache),
      use_matches_(base::FeatureList::IsEnabled(
          features::kNetworkContextPrefetchUseMatches)) {
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
  // If we don't think the request should be permitted from a render process, we
  // don't try to match it and instead let CorsURLLoaderFactory deal with the
  // issue.
  if (cache_ && IsRequestSafeForMatching(request)) {
    PrefetchURLLoaderClient* prefetch_client = cache_->Lookup(
        next_->isolation_info().network_isolation_key(), request.url);
    if (prefetch_client) {
      // A prefetch exists with the same NIK and URL.
      if (prefetch_client->Matches(request)) {
        if (use_matches_) {
          // The match has succeeded, and we are going to hand-off the
          // in-progress prefetch to the renderer.

          // TODO(crbug.com/342445996): Check that `options` is compatible with
          // the prefetch and whether anything needs to be done to the ongoing
          // request to match it.
          prefetch_client->Consume(std::move(loader), std::move(client));
          return;
        }
        // The match has succeeded, but the kNetworkContextPrefetchUseMatches
        // feature is not enabled. Try to make it possible for the render
        // process to reuse the cache entry.

        // Give the real URLLoader time to reach the cache, then cancel the
        // prefetch.
        cache_->DelayedErase(prefetch_client);
      } else {
        // There was an entry with the same NIK and URL, but it failed to match
        // on some other field. It is unlikely the renderer will subsequently
        // issue a matching request for the same URL, so erase the cache entry
        // to save resources.
        cache_->Erase(prefetch_client);
      }
    }
  }

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

bool PrefetchMatchingURLLoaderFactory::IsRequestSafeForMatching(
    const ResourceRequest& request) {
  // We never match requests from the browser process.
  if (next_->process_id() == mojom::kBrowserProcessId) {
    return false;
  }

  // `trusted_params` should never be set on a request from a render process.
  if (request.trusted_params) {
    return false;
  }

  // Ensure that the value of `request_initiator` is permitted for this render
  // process.
  InitiatorLockCompatibility compatibility =
      VerifyRequestInitiatorLock(next_->request_initiator_origin_lock(),
                                 request.mode == mojom::RequestMode::kNavigate
                                     ? url::Origin::Create(request.url)
                                     : request.request_initiator);

  // TODO(crbug.com/342445996): Share code with CorsURLLoaderFactory.
  switch (compatibility) {
    case InitiatorLockCompatibility::kCompatibleLock:
      return true;

    case InitiatorLockCompatibility::kBrowserProcess:
      NOTREACHED();

    case InitiatorLockCompatibility::kNoLock:
      // `request_initiator_origin_lock` should always be set in a
      // URLLoaderFactory vended to a renderer process.  See also
      // https://crbug.com/1114906.
      NOTREACHED_IN_MIGRATION();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator lock in a renderer request");
      return false;

    case InitiatorLockCompatibility::kNoInitiator:
      // Requests from the renderer need to always specify an initiator.
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator in a renderer request");
      return false;

    case InitiatorLockCompatibility::kIncorrectLock:
      // Requests from the renderer need to always specify a correct initiator.
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: lock VS initiator mismatch");
      return false;
  }
  NOTREACHED();
}

}  // namespace network
