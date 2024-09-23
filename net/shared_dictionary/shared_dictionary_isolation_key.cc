// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/shared_dictionary/shared_dictionary_isolation_key.h"

#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"

namespace net {

// static
std::optional<SharedDictionaryIsolationKey>
SharedDictionaryIsolationKey::MaybeCreate(const IsolationInfo& isolation_info) {
  if (!isolation_info.frame_origin() ||
      isolation_info.frame_origin()->opaque() ||
      !isolation_info.top_frame_origin() ||
      isolation_info.top_frame_origin()->opaque() ||
      isolation_info.nonce().has_value()) {
    return std::nullopt;
  }
  return SharedDictionaryIsolationKey(
      *isolation_info.frame_origin(),
      SchemefulSite(*isolation_info.top_frame_origin()));
}

// static
std::optional<SharedDictionaryIsolationKey>
SharedDictionaryIsolationKey::MaybeCreate(
    const NetworkIsolationKey& network_isolation_key,
    const std::optional<url::Origin>& frame_origin) {
  if (!frame_origin || frame_origin->opaque() ||
      !network_isolation_key.GetTopFrameSite() ||
      network_isolation_key.GetTopFrameSite()->opaque() ||
      network_isolation_key.GetNonce().has_value()) {
    return std::nullopt;
  }
  return SharedDictionaryIsolationKey(*frame_origin,
                                      *network_isolation_key.GetTopFrameSite());
}

SharedDictionaryIsolationKey::SharedDictionaryIsolationKey(
    const url::Origin& frame_origin,
    const SchemefulSite& top_frame_site)
    : frame_origin_(frame_origin), top_frame_site_(top_frame_site) {
  CHECK(!frame_origin.opaque());
  CHECK(!top_frame_site.opaque());
}

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
