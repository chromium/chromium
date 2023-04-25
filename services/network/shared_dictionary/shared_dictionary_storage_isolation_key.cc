// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_storage_isolation_key.h"

#include "net/base/isolation_info.h"

namespace network {

// static
absl::optional<SharedDictionaryStorageIsolationKey>
SharedDictionaryStorageIsolationKey::MaybeCreate(
    const net::IsolationInfo& isolation_info) {
  if (!isolation_info.frame_origin() ||
      isolation_info.frame_origin()->opaque() ||
      !isolation_info.top_frame_origin() ||
      isolation_info.top_frame_origin()->opaque() ||
      isolation_info.nonce().has_value()) {
    return absl::nullopt;
  }
  return SharedDictionaryStorageIsolationKey(
      *isolation_info.frame_origin(),
      net::SchemefulSite(*isolation_info.top_frame_origin()));
}

SharedDictionaryStorageIsolationKey::SharedDictionaryStorageIsolationKey(
    const url::Origin& frame_origin,
    const net::SchemefulSite& top_frame_site)
    : frame_origin_(frame_origin), top_frame_site_(top_frame_site) {}

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
