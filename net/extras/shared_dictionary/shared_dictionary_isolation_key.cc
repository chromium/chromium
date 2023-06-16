// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/shared_dictionary/shared_dictionary_isolation_key.h"

#include "net/base/isolation_info.h"

namespace net {

// static
absl::optional<SharedDictionaryIsolationKey>
SharedDictionaryIsolationKey::MaybeCreate(
    const net::IsolationInfo& isolation_info) {
  if (!isolation_info.frame_origin() ||
      isolation_info.frame_origin()->opaque() ||
      !isolation_info.top_frame_origin() ||
      isolation_info.top_frame_origin()->opaque() ||
      isolation_info.nonce().has_value()) {
    return absl::nullopt;
  }
  return SharedDictionaryIsolationKey(
      *isolation_info.frame_origin(),
      net::SchemefulSite(*isolation_info.top_frame_origin()));
}

SharedDictionaryIsolationKey::SharedDictionaryIsolationKey(
    const url::Origin& frame_origin,
    const net::SchemefulSite& top_frame_site)
    : frame_origin_(frame_origin), top_frame_site_(top_frame_site) {}

SharedDictionaryIsolationKey::~SharedDictionaryIsolationKey() = default;

SharedDictionaryIsolationKey::SharedDictionaryIsolationKey(
    const SharedDictionaryIsolationKey& other) = default;

SharedDictionaryIsolationKey::SharedDictionaryIsolationKey(
    SharedDictionaryIsolationKey&& other) = default;

SharedDictionaryIsolationKey& SharedDictionaryIsolationKey::operator=(
    const SharedDictionaryIsolationKey& other) = default;

SharedDictionaryIsolationKey& SharedDictionaryIsolationKey::operator=(
    SharedDictionaryIsolationKey&& other) = default;

}  // namespace net
