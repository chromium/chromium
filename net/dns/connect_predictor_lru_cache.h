// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_CONNECT_PREDICTOR_LRU_CACHE_H_
#define NET_DNS_CONNECT_PREDICTOR_LRU_CACHE_H_

#include <optional>

#include "base/containers/hashing_lru_cache.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/dns/connect_predictor.h"

namespace net {

// An implementation of ConnectPredictor that uses a multi-level LRU cache.
class NET_EXPORT_PRIVATE ConnectPredictorLRUCache final
    : public ConnectPredictor,
      public NetworkChangeNotifier::NetworkChangeObserver {
  // Partition implementation that uses an LRU cache.
  class Partition final : public ConnectPredictor::Partition {
   public:
    Partition();

    Partition(const Partition&) = delete;
    Partition& operator=(const Partition&) = delete;

    // Allow Partition to be movable so that it can be moved into a
    // HashingLRUCache. Even though the base class is not movable, this is safe
    // because the base class is stateless.
    Partition(Partition&& other) noexcept;
    Partition& operator=(Partition&& other) noexcept;

    ~Partition();

    // Returns the predicted result of a connect() attempt to the given
    // destination.
    std::optional<ConnectResult> Predict(const IPAddress& destination) override;

    // Records the result of an actual connect() attempt to the given
    // destination so that it can be used for future predictions.
    void RecordResult(const IPAddress& destination,
                      const ConnectResult& result) override;

    base::WeakPtr<Partition> GetWeakPtr();

   private:
    base::HashingLRUCache<IPAddress, ConnectResult> cache_;

    base::WeakPtrFactory<Partition> weak_ptr_factory_{this};
  };

 public:
  ConnectPredictorLRUCache();

  ConnectPredictorLRUCache(const ConnectPredictorLRUCache&) = delete;
  ConnectPredictorLRUCache& operator=(const ConnectPredictorLRUCache&) = delete;

  ~ConnectPredictorLRUCache() override;

  // Returns the Partition for `handle` and `nak`, or creates it if it does not
  // exist.
  base::WeakPtr<ConnectPredictor::Partition> GetPartition(
      handles::NetworkHandle handle,
      NetworkAnonymizationKey nak) override;

  // NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(NetworkChangeNotifier::ConnectionType type) override;

 private:
  using NetworkAnonymizationKeyToPartitionCache =
      base::HashingLRUCache<NetworkAnonymizationKey, Partition>;

  base::HashingLRUCache<handles::NetworkHandle,
                        NetworkAnonymizationKeyToPartitionCache>
      cache_;
};

}  // namespace net

#endif  // NET_DNS_CONNECT_PREDICTOR_LRU_CACHE_H_
