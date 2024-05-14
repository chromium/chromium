// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "services/network/public/cpp/not_implemented_url_loader_factory.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/gurl.h"

namespace blink {

namespace {

template <typename TKey>
void BindPendingRemoteMapToRemoteMap(
    std::map<TKey, mojo::Remote<network::mojom::URLLoaderFactory>>* target,
    std::map<TKey, mojo::PendingRemote<network::mojom::URLLoaderFactory>>
        input) {
  for (auto& it : input) {
    const TKey& key = it.first;
    mojo::PendingRemote<network::mojom::URLLoaderFactory>& pending_factory =
        it.second;
    if ((*target)[key].is_bound())
      (*target)[key].reset();
    if (pending_factory)  // pending_factory.is_valid().
      (*target)[key].Bind(std::move(pending_factory));
  }
}

}  // namespace

PendingURLLoaderFactoryBundle::PendingURLLoaderFactoryBundle() = default;

PendingURLLoaderFactoryBundle::PendingURLLoaderFactoryBundle(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory,
    SchemeMap pending_scheme_specific_factories,
    OriginMap pending_isolated_world_factories,
    bool bypass_redirect_checks)
    : pending_default_factory_(std::move(pending_default_factory)),
      pending_scheme_specific_factories_(
          std::move(pending_scheme_specific_factories)),
      pending_isolated_world_factories_(
          std::move(pending_isolated_world_factories)),
      bypass_redirect_checks_(bypass_redirect_checks) {}

PendingURLLoaderFactoryBundle::~PendingURLLoaderFactoryBundle() = default;

bool PendingURLLoaderFactoryBundle::
    IsTrackedChildPendingURLLoaderFactoryBundle() const {
  return false;
}

scoped_refptr<network::SharedURLLoaderFactory>
PendingURLLoaderFactoryBundle::CreateFactory() {
  auto other = std::make_unique<PendingURLLoaderFactoryBundle>();
  other->pending_default_factory_ = std::move(pending_default_factory_);
  other->pending_scheme_specific_factories_ =
      std::move(pending_scheme_specific_factories_);
  other->pending_isolated_world_factories_ =
      std::move(pending_isolated_world_factories_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<URLLoaderFactoryBundle>(std::move(other));
}

// -----------------------------------------------------------------------------

URLLoaderFactoryBundle::URLLoaderFactoryBundle() = default;

URLLoaderFactoryBundle::URLLoaderFactoryBundle(
    std::unique_ptr<PendingURLLoaderFactoryBundle> pending_factory) {
  Update(std::move(pending_factory));
}

URLLoaderFactoryBundle::~URLLoaderFactoryBundle() = default;

network::mojom::URLLoaderFactory* URLLoaderFactoryBundle::GetFactory(
    const network::ResourceRequest& request) {
  auto it = scheme_specific_factories_.find(request.url.scheme());
  if (it != scheme_specific_factories_.end())
    return it->second.get();

  if (request.isolated_world_origin.has_value()) {
    auto it2 =
        isolated_world_factories_.find(request.isolated_world_origin.value());
    if (it2 != isolated_world_factories_.end())
      return it2->second.get();
  }

  if (!default_factory_.is_bound()) {
    // Hitting the NOTREACHED below means that a subresource load has
    // unexpectedly happened in a speculative frame (or in a test frame created
    // via RenderViewTest).  This most likely indicates a bug somewhere else.
    NOTREACHED_IN_MIGRATION();

    // TODO(https://crbug.com/1300973): Once known issues are fixed, remove the
    // NotImplementedURLLoaderFactory (i.e. trust the NOTREACHED above, replace
    // it with an equivalent DCHECK, and accept crashing if a nullptr is
    // returned from this method).
    static const base::NoDestructor<
        mojo::Remote<network::mojom::URLLoaderFactory>>
        s_fallback_factory(network::NotImplementedURLLoaderFactory::Create());
    return s_fallback_factory->get();
  }

  return default_factory_.get();
}

void URLLoaderFactoryBundle::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::mojom::URLLoaderFactory* factory_ptr = GetFactory(request);
  factory_ptr->CreateLoaderAndStart(std::move(loader), request_id, options,
                                    request, std::move(client),
                                    traffic_annotation);
}

void URLLoaderFactoryBundle::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED_IN_MIGRATION();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
URLLoaderFactoryBundle::Clone() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  if (default_factory_) {
    default_factory_->Clone(
        pending_default_factory.InitWithNewPipeAndPassReceiver());
  }

  auto pending_factories =
      std::make_unique<blink::PendingURLLoaderFactoryBundle>(
          std::move(pending_default_factory),
          CloneRemoteMapToPendingRemoteMap(scheme_specific_factories_),
          CloneRemoteMapToPendingRemoteMap(isolated_world_factories_),
          bypass_redirect_checks_);

  return pending_factories;
}

bool URLLoaderFactoryBundle::BypassRedirectChecks() const {
  return bypass_redirect_checks_;
}

void URLLoaderFactoryBundle::Update(
    std::unique_ptr<PendingURLLoaderFactoryBundle> pending_factories) {
  if (pending_factories->pending_default_factory()) {
    default_factory_.reset();
    default_factory_.Bind(
        std::move(pending_factories->pending_default_factory()));
  }
  BindPendingRemoteMapToRemoteMap(
      &scheme_specific_factories_,
      std::move(pending_factories->pending_scheme_specific_factories()));
  BindPendingRemoteMapToRemoteMap(
      &isolated_world_factories_,
      std::move(pending_factories->pending_isolated_world_factories()));
  bypass_redirect_checks_ = pending_factories->bypass_redirect_checks();
}

}  // namespace blink
