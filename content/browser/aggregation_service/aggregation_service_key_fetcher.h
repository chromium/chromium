// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

class AggregatableReportManager;

// This class is responsible for requesting keys from storage, owned by the
// assembler.
class CONTENT_EXPORT AggregationServiceKeyFetcher {
 public:
  enum class PublicKeyFetchStatus {
    // TODO(crbug.com/1217823): Propagate up more granular errors.
    kOk = 0,
    kPublicKeyFetchFailed = 1,
    kUntrustworthyOrigin = 2,
    kMaxValue = kUntrustworthyOrigin,
  };

  using FetchCallback =
      base::OnceCallback<void(absl::optional<PublicKey>, PublicKeyFetchStatus)>;

  explicit AggregationServiceKeyFetcher(AggregatableReportManager* manager);
  AggregationServiceKeyFetcher(const AggregationServiceKeyFetcher& other) =
      delete;
  AggregationServiceKeyFetcher& operator=(
      const AggregationServiceKeyFetcher& other) = delete;
  ~AggregationServiceKeyFetcher();

  // Gets the public key for `origin` and triggers the `callback` once
  // completed.
  //
  // Helper server's keys must be rotated weekly which is primarily to limit the
  // impact of a compromised key. Any public key must be valid when fetched and
  // this will be enforced by the key fetcher. This ensures that the key used to
  // encrypt is valid at encryption time.
  //
  // To further limit the impact of a compromised key, we will support "key
  // slicing". That is, each helper server may make multiple public keys
  // available. At encryption time, the fetcher will (uniformly at random) pick
  // one of the public keys to use. This selection should be made independently
  // between reports so that the key choice cannot be used to partition reports
  // into separate groups of users.
  void GetPublicKey(const url::Origin& origin, FetchCallback callback);

 private:
  // Called when public keys are received from the storage.
  void OnPublicKeysReceivedFromStorage(FetchCallback callback,
                                       PublicKeysForOrigin keys_for_origin);

  // Using a raw pointer is safe because `manager_` is guaranteed to outlive
  // `this`.
  AggregatableReportManager* manager_;
  base::WeakPtrFactory<AggregationServiceKeyFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
