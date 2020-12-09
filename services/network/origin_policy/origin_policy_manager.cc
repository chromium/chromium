// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/origin_policy/origin_policy_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_util.h"
#include "services/network/network_context.h"
#include "services/network/origin_policy/origin_policy_fetcher.h"
#include "services/network/origin_policy/origin_policy_parsed_header.h"

namespace network {

OriginPolicyManager::OriginPolicyManager(NetworkContext* owner_network_context)
    : owner_network_context_(owner_network_context) {
  CreateOrRecreateURLLoaderFactory();
}

OriginPolicyManager::~OriginPolicyManager() {}

void OriginPolicyManager::AddReceiver(
    mojo::PendingReceiver<mojom::OriginPolicyManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OriginPolicyManager::RetrieveOriginPolicy(
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    const base::Optional<std::string>& header,
    RetrieveOriginPolicyCallback callback) {
  DCHECK(origin.GetURL().is_valid());
  DCHECK(!origin.opaque());

  // TODO(crbug.com/1051603): reconsider this model for where and when
  // exemptions are processed.
  if (!header.has_value() || base::Contains(exempted_origins_, origin)) {
    InvokeCallbackWithPolicyState(origin, OriginPolicyState::kNoPolicyApplies,
                                  std::move(callback));
    return;
  }

  base::Optional<OriginPolicyParsedHeader> parsed_header =
      OriginPolicyParsedHeader::FromString(*header);
  if (!parsed_header.has_value()) {
    InvokeCallbackWithPolicyState(origin, OriginPolicyState::kCannotParseHeader,
                                  std::move(callback));
    return;
  }

  // TODO(https://crbug.com/1042049): actually used parsed_header.

  origin_policy_fetchers_.emplace(std::make_unique<OriginPolicyFetcher>(
      this, origin, isolation_info, url_loader_factory_.get(),
      std::move(callback)));
}

void OriginPolicyManager::AddExceptionFor(const url::Origin& origin) {
  exempted_origins_.insert(origin);
}

void OriginPolicyManager::FetcherDone(OriginPolicyFetcher* fetcher,
                                      const OriginPolicy& origin_policy,
                                      RetrieveOriginPolicyCallback callback) {
  std::move(callback).Run(origin_policy);

  auto it = origin_policy_fetchers_.find(fetcher);
  DCHECK(it != origin_policy_fetchers_.end());
  origin_policy_fetchers_.erase(it);
}

void OriginPolicyManager::CreateOrRecreateURLLoaderFactory() {
  url_loader_factory_.reset();
  owner_network_context_->CreateTrustedUrlLoaderFactoryForNetworkService(
      url_loader_factory_.BindNewPipeAndPassReceiver());

  // This disconnect handler is necessary to avoid crbug.com/1047275.
  url_loader_factory_.set_disconnect_handler(
      base::BindOnce(&OriginPolicyManager::CreateOrRecreateURLLoaderFactory,
                     base::Unretained(this)));
}

// static
void OriginPolicyManager::InvokeCallbackWithPolicyState(
    const url::Origin& origin,
    OriginPolicyState state,
    RetrieveOriginPolicyCallback callback) {
  OriginPolicy result;
  result.state = state;
  result.policy_url = OriginPolicyFetcher::GetPolicyURL(origin);
  std::move(callback).Run(result);
}

}  // namespace network
