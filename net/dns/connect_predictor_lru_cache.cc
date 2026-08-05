// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/connect_predictor_lru_cache.h"

#include "net/base/features.h"

namespace net {

namespace {

// Masks the host-specific bits of the given IP address to group addresses
// from the same subnet or prefix for connect caching.
//
// Specifically, this zero-masks the last octet of IPv4 and IPv4-mapped IPv6
// addresses (masking to a /24 subnet), and zero-masks the lower 64 bits of
// IPv6 addresses (masking to a /64 prefix). This aligns with standard internet
// routing boundaries where route reachability and source address selection are
// identical.
IPAddress MaskIPAddress(const IPAddress& address) {
  if (address.IsIPv4()) {
    IPAddressBytes bytes = address.bytes();
    bytes[3] = 0;
    return IPAddress(bytes);
  } else if (address.IsIPv6()) {
    IPAddressBytes bytes = address.bytes();
    if (address.IsIPv4MappedIPv6()) {
      // Only mask the last byte, to match the IPv4 behavior.
      bytes[15] = 0;
      return IPAddress(bytes);
    }
    for (size_t i = 8; i < 16; ++i) {
      bytes[i] = 0;
    }
    return IPAddress(bytes);
  }
  return address;
}

}  // namespace

ConnectPredictorLRUCache::Partition::Partition()
    : cache_(features::kAddressSorterConnectCacheMaxPredictionsPerPartition
                 .Get()) {}

ConnectPredictorLRUCache::Partition::Partition(Partition&& other) noexcept
    : cache_(std::move(other.cache_)) {}

ConnectPredictorLRUCache::Partition&
ConnectPredictorLRUCache::Partition::operator=(Partition&& other) noexcept {
  cache_ = std::move(other.cache_);
  return *this;
}

ConnectPredictorLRUCache::Partition::~Partition() = default;

std::optional<ConnectPredictor::ConnectResult>
ConnectPredictorLRUCache::Partition::Predict(const IPAddress& destination) {
  const IPAddress masked_destination = MaskIPAddress(destination);
  auto it = cache_.Get(masked_destination);
  if (it != cache_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void ConnectPredictorLRUCache::Partition::RecordResult(
    const IPAddress& destination,
    const ConnectResult& result) {
  cache_.Put(MaskIPAddress(destination), result);
}

base::WeakPtr<ConnectPredictorLRUCache::Partition>
ConnectPredictorLRUCache::Partition::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ConnectPredictorLRUCache::ConnectPredictorLRUCache()
    : cache_(features::kAddressSorterConnectCacheMaxNetworks.Get()) {
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

ConnectPredictorLRUCache::~ConnectPredictorLRUCache() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void ConnectPredictorLRUCache::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  cache_.Clear();
}

base::WeakPtr<ConnectPredictor::Partition>
ConnectPredictorLRUCache::GetPartition(handles::NetworkHandle handle,
                                       NetworkAnonymizationKey nak) {
  auto handle_it = cache_.Get(handle);
  if (handle_it == cache_.end()) {
    handle_it = cache_.Put(
        handle,
        NetworkAnonymizationKeyToPartitionCache(
            features::kAddressSorterConnectCacheMaxNaksPerNetwork.Get()));
  }

  auto nak_it = handle_it->second.Get(nak);
  if (nak_it == handle_it->second.end()) {
    nak_it = handle_it->second.Put(nak, Partition());
  }

  return nak_it->second.GetWeakPtr();
}

}  // namespace net
