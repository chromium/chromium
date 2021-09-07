// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_key_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "content/browser/aggregation_service/aggregatable_report_manager.h"
#include "content/browser/aggregation_service/aggregation_service_key_storage.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

AggregationServiceKeyFetcher::AggregationServiceKeyFetcher(
    AggregatableReportManager* manager)
    : manager_(manager) {}

AggregationServiceKeyFetcher::~AggregationServiceKeyFetcher() = default;

void AggregationServiceKeyFetcher::GetPublicKey(const url::Origin& origin,
                                                FetchCallback callback) {
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    std::move(callback).Run(absl::nullopt,
                            PublicKeyFetchStatus::kUntrustworthyOrigin);
    return;
  }

  manager_->GetKeyStorage()
      .AsyncCall(&AggregationServiceKeyStorage::GetPublicKeys)
      .WithArgs(origin)
      .Then(base::BindOnce(
          &AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AggregationServiceKeyFetcher::OnPublicKeysReceivedFromStorage(
    FetchCallback callback,
    PublicKeysForOrigin keys_for_origin) {
  if (keys_for_origin.keys.empty()) {
    // TODO(crbug.com/1217823): Fetch public keys from the network.

    std::move(callback).Run(absl::nullopt,
                            PublicKeyFetchStatus::kPublicKeyFetchFailed);
    return;
  }

  // Each report should randomly select a key. This ensures that the set of
  // reports a client sends are not a subset of the reports identified by any
  // one key.
  uint64_t key_index = base::RandGenerator(keys_for_origin.keys.size());
  std::move(callback).Run(std::move(keys_for_origin.keys[key_index]),
                          PublicKeyFetchStatus::kOk);
}

}  // namespace content
