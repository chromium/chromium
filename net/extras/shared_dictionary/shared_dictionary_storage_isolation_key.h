// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_
#define NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_

#include "base/component_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace net {
class IsolationInfo;

// Key used to isolate shared dictionary storages.
class COMPONENT_EXPORT(NET_EXTRAS) SharedDictionaryStorageIsolationKey {
 public:
  // Creates a SharedDictionaryStorageIsolationKey. Returns nullopt when
  // `frame_origin` or `top_frame_origin` of `isolation_info` is not set or
  // opaque, or `nonce` is set.
  static absl::optional<SharedDictionaryStorageIsolationKey> MaybeCreate(
      const net::IsolationInfo& isolation_info);

  SharedDictionaryStorageIsolationKey(const url::Origin& frame_origin,
                                      const net::SchemefulSite& top_frame_site);

  const url::Origin& frame_origin() const { return frame_origin_; }
  const net::SchemefulSite top_frame_site() const { return top_frame_site_; }

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
    return std::tie(frame_origin_, top_frame_site_) ==
           std::tie(other.frame_origin_, other.top_frame_site_);
  }
  bool operator!=(const SharedDictionaryStorageIsolationKey& other) const {
    return !(*this == other);
  }
  bool operator<(const SharedDictionaryStorageIsolationKey& other) const {
    return std::tie(frame_origin_, top_frame_site_) <
           std::tie(other.frame_origin_, other.top_frame_site_);
  }

 private:
  url::Origin frame_origin_;
  net::SchemefulSite top_frame_site_;
};

}  // namespace net

#endif  // NET_EXTRAS_SHARED_DICTIONARY_SHARED_DICTIONARY_STORAGE_ISOLATION_KEY_H_
