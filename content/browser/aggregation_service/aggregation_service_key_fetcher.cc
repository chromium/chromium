// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/rand_util.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

AggregationServiceKeyFetcher::AggregationServiceKeyFetcher(
    AggregationServiceStorageContext* storage_context,
    std::unique_ptr<NetworkFetcher> network_fetcher)
    : storage_context_(storage_context),
      network_fetcher_(std::move(network_fetcher)) {}

AggregationServiceKeyFetcher::~AggregationServiceKeyFetcher() = default;

void AggregationServiceKeyFetcher::GetPublicKey(const url::Origin& origin,
                                                FetchCallback callback) {
  DCHECK(network::IsOriginPotentiallyTrustworthy(origin));

  base::circular_deque<FetchCallback>& pending_callbacks =
      origin_callbacks_[origin];
  bool in_progress = !pending_callbacks.empty();
  pending_callbacks.push_back(std::move(callback));

  // If there is already a fetch request in progress, just enqueue the
  // callback and return.
  if (in_progress)
    return;

  // First we check if we already have keys stored.
  // TODO(crbug.com/1223488): Pass origin by value and move after C++17.
  storage_context_->GetKeyStorage()
      .AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
      .WithArgs(origin)
      .Then(base::BindOnce(
          &AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage,
          weak_factory_.GetWeakPtr(), origin));
}

void AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage(
    const url::Origin& origin,
    std::vector<PublicKey> keys) {
  if (keys.empty()) {
    // Fetch keys from the network if not found in the storage.
    FetchPublicKeysFromNetwork(origin);
    return;
  }

  RunCallbacksForOrigin(origin, keys);
}

void AggregationServiceKeyFetcher::FetchPublicKeysFromNetwork(
    const url::Origin& origin) {
  if (!network_fetcher_) {
    // Return error if fetching from network is not enabled.
    RunCallbacksForOrigin(origin, /*keys=*/{});
    return;
  }

  // Unretained is safe because the network fetcher is owned by `this` and will
  // be deleted before `this`.
  network_fetcher_->FetchPublicKeys(
      origin,
      base::BindOnce(
          &AggregationServiceKeyFetcher::OnPublicKeysReceivedFromNetwork,
          base::Unretained(this), origin));
}

void AggregationServiceKeyFetcher::OnPublicKeysReceivedFromNetwork(
    const url::Origin& origin,
    absl::optional<PublicKeyset> keyset) {
  if (!keyset.has_value() || keyset->expiry_time.is_null()) {
    // `keyset` will be absl::nullopt if an error occurred and `expiry_time`
    // will be null if the freshness lifetime was zero. In these cases, we will
    // still update the keys for `origin`, i,e. clear them.
    storage_context_->GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::ClearPublicKeys)
        .WithArgs(origin);
  } else {
    // Store public keys fetched from network to storage, the old keys will be
    // deleted from storage.
    storage_context_->GetKeyStorage()
        .AsyncCall(&AggregationServiceKeyStorage::SetPublicKeys)
        .WithArgs(origin, keyset.value());
  }

  RunCallbacksForOrigin(
      origin, keyset.has_value() ? keyset->keys : std::vector<PublicKey>());
}

void AggregationServiceKeyFetcher::RunCallbacksForOrigin(
    const url::Origin& origin,
    const std::vector<PublicKey>& keys) {
  auto iter = origin_callbacks_.find(origin);
  DCHECK(iter != origin_callbacks_.end());

  base::circular_deque<FetchCallback> pending_callbacks =
      std::move(iter->second);
  DCHECK(!pending_callbacks.empty());

  origin_callbacks_.erase(iter);

  if (keys.empty()) {
    // Return error, don't refetch to avoid infinite loop.
    for (auto& callback : pending_callbacks) {
      std::move(callback).Run(absl::nullopt,
                              PublicKeyFetchStatus::kPublicKeyFetchFailed);
    }
  } else {
    for (auto& callback : pending_callbacks) {
      // Each report should randomly select a key. This ensures that the set of
      // reports a client sends are not a subset of the reports identified by
      // any one key.
      uint64_t key_index = base::RandGenerator(keys.size());
      std::move(callback).Run(keys[key_index], PublicKeyFetchStatus::kOk);
    }
  }
}

}  // namespace content
