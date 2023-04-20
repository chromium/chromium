// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_isolation_key.h"

namespace network {

// static
absl::optional<SharedDictionaryStorageIsolationKey>
SharedDictionaryStorageIsolationKey::MaybeCreate(
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key) {
  if (frame_origin.opaque() || network_isolation_key.IsTransient()) {
    return absl::nullopt;
  }
  return SharedDictionaryStorageIsolationKey(frame_origin,
                                             network_isolation_key);
}

SharedDictionaryStorageIsolationKey::SharedDictionaryStorageIsolationKey(
    const url::Origin& frame_origin,
    const net::NetworkIsolationKey& network_isolation_key)
    : frame_origin_(frame_origin),
      network_isolation_key_(network_isolation_key) {}

SharedDictionaryStorageIsolationKey::~SharedDictionaryStorageIsolationKey() =
    default;

SharedDictionaryStorageIsolationKey::SharedDictionaryStorageIsolationKey(
    const SharedDictionaryStorageIsolationKey& other) = default;

SharedDictionaryStorageIsolationKey::SharedDictionaryStorageIsolationKey(
    SharedDictionaryStorageIsolationKey&& other) = default;

SharedDictionaryStorageIsolationKey&
SharedDictionaryStorageIsolationKey::operator=(
    const SharedDictionaryStorageIsolationKey& other) = default;

SharedDictionaryStorageIsolationKey&
SharedDictionaryStorageIsolationKey::operator=(
    SharedDictionaryStorageIsolationKey&& other) = default;

}  // namespace network
