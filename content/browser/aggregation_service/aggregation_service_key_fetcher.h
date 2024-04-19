// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class AggregationServiceStorageContext;

// This class is responsible for requesting keys from storage, owned by the
// assembler.
class CONTENT_EXPORT AggregationServiceKeyFetcher {
 public:
  // This class is responsible for fetching public keys from helper servers over
  // the network.
  class NetworkFetcher {
   public:
    virtual ~NetworkFetcher() = default;

    using NetworkFetchCallback =
        base::OnceCallback<void(std::optional<PublicKeyset>)>;

    // Fetch public keys from the helper server endpoint `url`. Returns
    // std::nullopt in case of network or parsing error.
    virtual void FetchPublicKeys(const GURL& url,
                                 NetworkFetchCallback callback) = 0;
  };

  enum class PublicKeyFetchStatus {
    // TODO(crbug.com/40185368): Propagate up more granular errors.
    kOk,
    kPublicKeyFetchFailed,
    kMaxValue = kPublicKeyFetchFailed,
  };

  using FetchCallback =
      base::OnceCallback<void(std::optional<PublicKey>, PublicKeyFetchStatus)>;

  AggregationServiceKeyFetcher(
      AggregationServiceStorageContext* storage_context,
      std::unique_ptr<NetworkFetcher> network_fetcher);
  AggregationServiceKeyFetcher(const AggregationServiceKeyFetcher& other) =
      delete;
  AggregationServiceKeyFetcher& operator=(
      const AggregationServiceKeyFetcher& other) = delete;
  virtual ~AggregationServiceKeyFetcher();

  // Gets a currently valid public key for `url` and triggers the `callback`
  // once completed.
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
  // into separate groups of users. Virtual for mocking in tests.
  virtual void GetPublicKey(const GURL& url, FetchCallback callback);

 private:
  // Called when public keys are received from the storage.
  void OnPublicKeysReceivedFromStorage(const GURL& url,
                                       std::vector<PublicKey> keys);

  // Keys are fetched from the network if they are not found in storage.
  void FetchPublicKeysFromNetwork(const GURL& url);

  // Called when public keys are received from the network fetcher.
  void OnPublicKeysReceivedFromNetwork(const GURL& url,
                                       std::optional<PublicKeyset> keyset);

  // Runs callbacks for pending requests for `url` with the public keys
  // received from the network or storage. Any keys specified must be currently
  // valid.
  void RunCallbacksForUrl(const GURL& url, const std::vector<PublicKey>& keys);

  // Using a raw pointer is safe because `storage_context_` is guaranteed to
  // outlive `this`.
  raw_ptr<AggregationServiceStorageContext> storage_context_;

  // Map of all URLs that are currently waiting for the public keys, and
  // their associated fetch callbacks. Used to cache ongoing requests to the
  // storage or network to prevent looking up the same key multiple times at
  // once.
  base::flat_map<GURL, base::circular_deque<FetchCallback>> url_callbacks_;

  // Responsible for issuing requests to network for fetching public keys.
  std::unique_ptr<NetworkFetcher> network_fetcher_;

  base::WeakPtrFactory<AggregationServiceKeyFetcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_KEY_FETCHER_H_
