// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SHARED_DICTIONARY_SHARED_DICTIONARY_ISOLATION_KEY_H_
#define NET_SHARED_DICTIONARY_SHARED_DICTIONARY_ISOLATION_KEY_H_

#include <optional>

#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

namespace net {
class IsolationInfo;
class NetworkIsolationKey;

// Key used to isolate shared dictionary storages.
class NET_EXPORT SharedDictionaryIsolationKey {
 public:
  // Creates a SharedDictionaryIsolationKey. Returns nullopt when
  // `frame_origin` or `top_frame_origin` of `isolation_info` is not set or
  // opaque, or `nonce` is set.
  static std::optional<SharedDictionaryIsolationKey> MaybeCreate(
      const IsolationInfo& isolation_info);

  // Creates a SharedDictionaryIsolationKey. Returns nullopt when
  // `frame_origin` or `top_frame_origin` of `network_isolation_key` is not set
  // or opaque, or `nonce` of `network_isolation_key` is set.
  static std::optional<SharedDictionaryIsolationKey> MaybeCreate(
      const NetworkIsolationKey& network_isolation_key,
      const std::optional<url::Origin>& frame_origin);

  SharedDictionaryIsolationKey() = default;
  SharedDictionaryIsolationKey(const url::Origin& frame_origin,
                               const SchemefulSite& top_frame_site);

  const url::Origin& frame_origin() const { return frame_origin_; }
  const SchemefulSite& top_frame_site() const { return top_frame_site_; }

  ~SharedDictionaryIsolationKey();

  SharedDictionaryIsolationKey(const SharedDictionaryIsolationKey& other);
  SharedDictionaryIsolationKey(SharedDictionaryIsolationKey&& other);
  SharedDictionaryIsolationKey& operator=(
      const SharedDictionaryIsolationKey& other);
  SharedDictionaryIsolationKey& operator=(SharedDictionaryIsolationKey&& other);

  bool operator==(const SharedDictionaryIsolationKey& other) const {
    return std::tie(frame_origin_, top_frame_site_) ==
           std::tie(other.frame_origin_, other.top_frame_site_);
  }
  bool operator!=(const SharedDictionaryIsolationKey& other) const {
    return !(*this == other);
  }
  bool operator<(const SharedDictionaryIsolationKey& other) const {
    return std::tie(frame_origin_, top_frame_site_) <
           std::tie(other.frame_origin_, other.top_frame_site_);
  }

 private:
  url::Origin frame_origin_;
  SchemefulSite top_frame_site_;
};

}  // namespace net

#endif  // NET_SHARED_DICTIONARY_SHARED_DICTIONARY_ISOLATION_KEY_H_
