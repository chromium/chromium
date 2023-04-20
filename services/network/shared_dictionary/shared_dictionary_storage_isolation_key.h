// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_

#include "base/component_export.h"
#include "net/base/network_isolation_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace network {

// Key used to isolate shared dictionary storages.
class COMPONENT_EXPORT(NETWORK_SERVICE) SharedDictionaryStorageIsolationKey {
 public:
  // Creates a SharedDictionaryStorageIsolationKey. Returns nullopt when
  // `frame_origin` is opaque or `network_isolation_key` is transient.
  static absl::optional<SharedDictionaryStorageIsolationKey> MaybeCreate(
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key);

  const url::Origin& frame_origin() const { return frame_origin_; }
  const net::NetworkIsolationKey& network_isolation_key() const {
    return network_isolation_key_;
  }

  ~SharedDictionaryStorageIsolationKey();

  SharedDictionaryStorageIsolationKey(
      const SharedDictionaryStorageIsolationKey& other);
  SharedDictionaryStorageIsolationKey(
      SharedDictionaryStorageIsolationKey&& other);
  SharedDictionaryStorageIsolationKey& operator=(
      const SharedDictionaryStorageIsolationKey& other);
  SharedDictionaryStorageIsolationKey& operator=(
      SharedDictionaryStorageIsolationKey&& other);

  bool operator==(const SharedDictionaryStorageIsolationKey& other) const {
    return std::tie(frame_origin_, network_isolation_key_) ==
           std::tie(other.frame_origin_, other.network_isolation_key_);
  }
  bool operator!=(const SharedDictionaryStorageIsolationKey& other) const {
    return !(*this == other);
  }
  bool operator<(const SharedDictionaryStorageIsolationKey& other) const {
    return std::tie(frame_origin_, network_isolation_key_) <
           std::tie(other.frame_origin_, other.network_isolation_key_);
  }

 private:
  SharedDictionaryStorageIsolationKey(
      const url::Origin& frame_origin,
      const net::NetworkIsolationKey& network_isolation_key);

  url::Origin frame_origin_;
  net::NetworkIsolationKey network_isolation_key_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_
