// Copyright 2021 The Chromium Authors
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
#include "content/browser/aggregation_service/aggregation_service_storage.h"
#include "content/browser/aggregation_service/aggregation_service_storage_context.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

AggregationServiceKeyFetcher::AggregationServiceKeyFetcher(
    AggregationServiceStorageContext* storage_context,
    std::unique_ptr<NetworkFetcher> network_fetcher)
    : storage_context_(storage_context),
      network_fetcher_(std::move(network_fetcher)) {}

AggregationServiceKeyFetcher::~AggregationServiceKeyFetcher() = default;

void AggregationServiceKeyFetcher::GetPublicKey(const GURL& url,
                                                FetchCallback callback) {
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));

  base::circular_deque<FetchCallback>& pending_callbacks = url_callbacks_[url];
  bool in_progress = !pending_callbacks.empty();
  pending_callbacks.push_back(std::move(callback));

  // If there is already a fetch request in progress, just enqueue the
  // callback and return.
  if (in_progress)
    return;

  // First we check if we already have keys stored.
  // TODO(crbug.com/1223488): Pass url by value and move after C++17.
  storage_context_->GetStorage()
      .AsyncCall(&AggregationServiceStorage::GetPublicKeys)
      .WithArgs(url)
      .Then(base::BindOnce(
          &AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage,
          weak_factory_.GetWeakPtr(), url));
}

void AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage(
    const GURL& url,
    std::vector<PublicKey> keys) {
  if (keys.empty()) {
    // Fetch keys from the network if not found in the storage.
    FetchPublicKeysFromNetwork(url);
    return;
  }

  RunCallbacksForUrl(url, keys);
}

void AggregationServiceKeyFetcher::FetchPublicKeysFromNetwork(const GURL& url) {
  if (!network_fetcher_) {
    // Return error if fetching from network is not enabled.
    RunCallbacksForUrl(url, /*keys=*/{});
    return;
  }

  // Unretained is safe because the network fetcher is owned by `this` and will
  // be deleted before `this`.
  network_fetcher_->FetchPublicKeys(
      url, base::BindOnce(
               &AggregationServiceKeyFetcher::OnPublicKeysReceivedFromNetwork,
               base::Unretained(this), url));
}

void AggregationServiceKeyFetcher::OnPublicKeysReceivedFromNetwork(
    const GURL& url,
    absl::optional<PublicKeyset> keyset) {
  if (!keyset.has_value() || keyset->expiry_time.is_null()) {
    // `keyset` will be absl::nullopt if an error occurred and `expiry_time`
    // will be null if the freshness lifetime was zero. In these cases, we will
    // still update the keys for `url`, i,e. clear them.
    storage_context_->GetStorage()
        .AsyncCall(&AggregationServiceStorage::ClearPublicKeys)
        .WithArgs(url);
  } else {
    // Store public keys fetched from network to storage, the old keys will be
    // deleted from storage.
    storage_context_->GetStorage()
        .AsyncCall(&AggregationServiceStorage::SetPublicKeys)
        .WithArgs(url, keyset.value());
  }

  RunCallbacksForUrl(
      url, keyset.has_value() ? keyset->keys : std::vector<PublicKey>());
}

void AggregationServiceKeyFetcher::RunCallbacksForUrl(
    const GURL& url,
    const std::vector<PublicKey>& keys) {
  auto iter = url_callbacks_.find(url);
  DCHECK(iter != url_callbacks_.end());

  base::circular_deque<FetchCallback> pending_callbacks =
      std::move(iter->second);
  DCHECK(!pending_callbacks.empty());

  url_callbacks_.erase(iter);

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
