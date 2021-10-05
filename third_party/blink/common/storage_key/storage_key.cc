// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <cctype>
#include <ostream>
#include <tuple>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace blink {

// static
absl::optional<StorageKey> StorageKey::Deserialize(base::StringPiece in) {
  StorageKey result(url::Origin::Create(GURL(in)));
  return result.origin_.opaque() ? absl::nullopt
                                 : absl::make_optional(std::move(result));
}

// static
absl::optional<StorageKey> StorageKey::DeserializeForServiceWorker(
    base::StringPiece in) {
  // TODO(https://crbug.com/1199077): Figure out how to include `nonce_` in the
  // serialization.

  // As per the SerializeForServiceWorker() call, we have to expect the
  // following structure: <StorageKey 'key'.origin> + [ "^" + <StorageKey
  // `key`.top_level_site> ] The brackets indicate an optional component.

  url::Origin key_origin;
  net::SchemefulSite key_top_level_site;

  // Let's check for the delimiting caret, if it's there then a top_level_site
  // is included.
  size_t pos = in.find_first_of('^');
  if (pos != std::string::npos) {
    // The origin is the portion up to, but not including, the caret.
    key_origin = url::Origin::Create(GURL(in.substr(0, pos)));
    // The top_level_site is the portion beyond the caret.
    key_top_level_site =
        net::SchemefulSite(GURL(in.substr(pos + 1, std::string::npos)));
  } else {
    // In this case the top_level_site is implicitly the same site as the
    // origin.
    key_origin = url::Origin::Create(GURL(in));
    key_top_level_site = net::SchemefulSite(key_origin);
  }

  if (key_origin.opaque() || key_top_level_site.opaque())
    return absl::nullopt;

  return StorageKey(key_origin, key_top_level_site);
}

// static
StorageKey StorageKey::CreateFromStringForTesting(const std::string& origin) {
  absl::optional<StorageKey> result = Deserialize(origin);
  return result.value_or(StorageKey());
}

// static
bool StorageKey::IsThirdPartyStoragePartitioningEnabled() {
  return base::FeatureList::IsEnabled(features::kThirdPartyStoragePartitioning);
}

// static
StorageKey StorageKey::CreateWithNonce(const url::Origin& origin,
                                       const base::UnguessableToken& nonce) {
  DCHECK(!nonce.is_empty());
  return StorageKey(origin, net::SchemefulSite(origin), &nonce);
}

// static
blink::StorageKey StorageKey::FromNetIsolationInfo(
    const net::IsolationInfo& isolation_info) {
  DCHECK(isolation_info.frame_origin().has_value());
  DCHECK(isolation_info.top_frame_origin().has_value());
  return StorageKey(
      isolation_info.frame_origin().value(),
      net::SchemefulSite(isolation_info.top_frame_origin().value()),
      base::OptionalOrNullptr(isolation_info.nonce()));
}

std::string StorageKey::Serialize() const {
  DCHECK(!origin_.opaque());
  return origin_.GetURL().spec();
}

std::string StorageKey::SerializeForLocalStorage() const {
  // TODO(https://crbug.com/1199077): Figure out how to include `nonce_` in the
  // serialization.
  // TODO(https://crbug.com/1199077): Add top_level_site_ behind a feature.
  DCHECK(!origin_.opaque());
  return origin_.Serialize();
}

std::string StorageKey::SerializeForServiceWorker() const {
  // TODO(https://crbug.com/1199077): Figure out how to include `nonce_` in the
  // serialization.
  DCHECK(!origin_.opaque());
  DCHECK(!top_level_site_.opaque());

  // If storage partitioning is enabled we need to serialize the key to fit the
  // following structure: <StorageKey 'key'.origin> + [ "^" + <StorageKey
  // `key`.top_level_site> ]
  //
  // The top_level_site is optional if it's the same site as the origin in order
  // to enable backwards compatibility for 1p contexts.
  // In the case of a 1p context the serialization structure is therefore the
  // same as if features::kThirdPartyStoragePartitioning was disabled.

  if (IsThirdPartyStoragePartitioningEnabled() &&
      top_level_site_ != net::SchemefulSite(origin_)) {
    return origin_.GetURL().spec() + "^" + top_level_site_.Serialize();
  }

  return origin_.GetURL().spec();
}

std::string StorageKey::GetDebugString() const {
  return base::StrCat(
      {"{ origin: ", origin_.GetDebugString(),
       ", top-level site: ", top_level_site_.Serialize(),
       ", nonce: ", nonce_.has_value() ? nonce_->ToString() : "<null>", " }"});
}

std::string StorageKey::GetMemoryDumpString(size_t max_length) const {
  std::string memory_dump_str = origin_.Serialize().substr(0, max_length);

  if (max_length > memory_dump_str.length()) {
    memory_dump_str.append(top_level_site_.Serialize().substr(
        0, max_length - memory_dump_str.length()));
  }

  if (nonce_.has_value() && max_length > memory_dump_str.length()) {
    memory_dump_str.append(
        nonce_->ToString().substr(0, max_length - memory_dump_str.length()));
  }

  base::ranges::replace_if(
      memory_dump_str.begin(), memory_dump_str.end(),
      [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');
  return memory_dump_str;
}

const net::SiteForCookies StorageKey::ToNetSiteForCookies() const {
  if (!nonce_ &&
      net::registry_controlled_domains::SameDomainOrHost(
          origin_, url::Origin::Create(top_level_site_.GetURL()),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return net::SiteForCookies::FromUrl(top_level_site_.GetURL());
  }
  return net::SiteForCookies();
}

bool operator==(const StorageKey& lhs, const StorageKey& rhs) {
  return std::tie(lhs.origin_, lhs.top_level_site_, lhs.nonce_) ==
         std::tie(rhs.origin_, rhs.top_level_site_, rhs.nonce_);
}

bool operator!=(const StorageKey& lhs, const StorageKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const StorageKey& lhs, const StorageKey& rhs) {
  return std::tie(lhs.origin_, lhs.top_level_site_, lhs.nonce_) <
         std::tie(rhs.origin_, rhs.top_level_site_, rhs.nonce_);
}

std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk) {
  return ostream << sk.GetDebugString();
}

}  // namespace blink
