// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/isolation_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

// Returns true if there are at least 2 chars after the '^' in `in` and the
// second char is not '^'. Meaning that the substring is syntactically valid.
// This is to indicate that there is a valid separator with both a '^' and a
// uint8_t and some amount of encoded data. I.e.: "^09" has both a "^0" as the
// separator and '9' as the encoded data.
bool ValidSeparatorWithData(base::StringPiece in, size_t pos_of_caret) {
  if (in.length() > pos_of_caret + 2 && in[pos_of_caret + 2] != '^')
    return true;

  return false;
}

}  // namespace

namespace blink {

// static
absl::optional<StorageKey> StorageKey::Deserialize(base::StringPiece in) {
  using EncodedAttribute = StorageKey::EncodedAttribute;
  // As per the Serialize() call, we have to expect the
  // following structure: <StorageKey 'key'.origin> + "/" + [ "^0" + <StorageKey
  // `key`.top_level_site> ] The brackets indicate an optional component.
  // - or -
  // <StorageKey 'key'.origin> + "/" + "^1" + <StorageKey 'nonce'.High64Bits> +
  // "^2" + <StorageKey 'nonce'.Low64Bits>

  // Let's check for a delimiting caret. The presence of a caret means this key
  // is partitioned.

  // More than 2 carets indicates a malformed input.
  if (std::count(in.begin(), in.end(), '^') > 2)
    return absl::nullopt;

  size_t pos = in.find_first_of('^');

  url::Origin key_origin;
  net::SchemefulSite key_top_level_site;
  std::unique_ptr<base::UnguessableToken> nonce;

  if (pos == std::string::npos) {
    // Only the origin is serialized.

    key_origin = url::Origin::Create(GURL(in));

    // In this case the top_level_site is implicitly the same site as the
    // origin.
    key_top_level_site = net::SchemefulSite(key_origin);

    // There is no nonce.

    if (key_origin.opaque())
      return absl::nullopt;

    return StorageKey(key_origin, key_top_level_site);
  }

  if (!ValidSeparatorWithData(in, pos))
    return absl::nullopt;

  // Otherwise the key is partitioned, let's see what it's partitioned by.
  EncodedAttribute encoded_attribute =
      DeserializeAttributeSeparator(in.substr(pos, 2));

  switch (encoded_attribute) {
    case EncodedAttribute::kTopLevelSite: {
      // A top-level site is serialized.

      // The origin is the portion up to, but not including, the caret
      // separator.
      key_origin = url::Origin::Create(GURL(in.substr(0, pos)));

      // The top_level_site is the portion beyond the separator.
      key_top_level_site =
          net::SchemefulSite(GURL(in.substr(pos + 2, std::string::npos)));

      // There is no nonce.

      // In addition to checking for opaque-ness, check to make sure that the
      // origin is not same-site with the top-level site (i.e.: A first-party
      // StorageKey). This is important because we specifically do not serialize
      // the top-level site portion of a 1p StorageKey for backwards
      // compatibility reasons, meaning that such an input is malformed.
      if (key_origin.opaque() || key_top_level_site.opaque() ||
          key_top_level_site == net::SchemefulSite(key_origin)) {
        return absl::nullopt;
      }

      return StorageKey(key_origin, key_top_level_site);
    }
    case EncodedAttribute::kNonceHigh: {
      // A nonce is serialized.
      // There should be two carets separators, let's grab the second.
      size_t pos_2 = in.find_first_of('^', pos + 2);

      // Make sure we found the next separator, it's valid, that it's the
      // correct attribute.
      if (pos_2 == std::string::npos || !ValidSeparatorWithData(in, pos_2) ||
          DeserializeAttributeSeparator(in.substr(pos_2, 2)) !=
              EncodedAttribute::kNonceLow)
        return absl::nullopt;

      // The origin is the portion up to, but not including, the first
      // separator.
      key_origin = url::Origin::Create(GURL(in.substr(0, pos)));

      // The first high 64 bits of the nonce are next, between the two
      // separators.
      int length_of_high = pos_2 - (pos + 2);
      std::string high_digits =
          static_cast<std::string>(in.substr(pos + 2, length_of_high));
      // The low 64 bits are last, after the second separator.
      std::string low_digits =
          static_cast<std::string>(in.substr(pos_2 + 2, std::string::npos));

      uint64_t nonce_high = 0;
      uint64_t nonce_low = 0;

      if (!base::StringToUint64(high_digits, &nonce_high))
        return absl::nullopt;

      if (!base::StringToUint64(low_digits, &nonce_low))
        return absl::nullopt;

      nonce = std::make_unique<base::UnguessableToken>(
          base::UnguessableToken::Deserialize(nonce_high, nonce_low));

      if (key_origin.opaque() || !nonce || nonce->is_empty())
        return absl::nullopt;

      return StorageKey::CreateWithNonce(key_origin, *nonce);
    }
    default: {
      // Malformed input case. We saw a separator that we don't understand
      // or one in the wrong order.
      return absl::nullopt;
    }
  }
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
StorageKey StorageKey::CreateWithOptionalNonce(
    const url::Origin& origin,
    const net::SchemefulSite& top_level_site,
    const base::UnguessableToken* nonce) {
  DCHECK(!nonce || !nonce->is_empty());
  return StorageKey(origin, top_level_site, nonce);
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
  using EncodedAttribute = StorageKey::EncodedAttribute;
  DCHECK(!origin_.opaque());
  DCHECK(!top_level_site_.opaque());

  // If the storage key has a nonce then we need to serialize the key to fit the
  // following scheme:
  //
  // <StorageKey 'key'.origin> + "/" + "^1" + <StorageKey 'nonce'.High64Bits> +
  // "^2" + <StorageKey 'nonce'.Low64Bits>
  if (nonce_.has_value()) {
    return origin_.GetURL().spec() +
           SerializeAttributeSeparator(EncodedAttribute::kNonceHigh) +
           base::NumberToString(nonce_->GetHighForSerialization()) +
           SerializeAttributeSeparator(EncodedAttribute::kNonceLow) +
           base::NumberToString(nonce_->GetLowForSerialization());
  }

  // Else if storage partitioning is enabled we need to serialize the key to fit
  // the following scheme:
  //
  // <StorageKey 'key'.origin> + "/" + [ "^0" + <StorageKey
  // `key`.top_level_site> ]
  //
  // The top_level_site is optional (indicated by the square brackets) if it's
  // the same site as the origin in order to enable backwards compatibility for
  // 1p contexts. Meaning that in the case of a 1p context the serialization
  // structure is the same as if features::kThirdPartyStoragePartitioning was
  // disabled.
  if (IsThirdPartyStoragePartitioningEnabled() &&
      top_level_site_ != net::SchemefulSite(origin_)) {
    return origin_.GetURL().spec() +
           SerializeAttributeSeparator(EncodedAttribute::kTopLevelSite) +
           top_level_site_.Serialize();
  }

  return origin_.GetURL().spec();
}

std::string StorageKey::SerializeForLocalStorage() const {
  DCHECK(!origin_.opaque());

  // If this is a third-party StorageKey we'll use the standard serialization
  // scheme.
  if (nonce_.has_value())
    return Serialize();

  if (IsThirdPartyStoragePartitioningEnabled() &&
      top_level_site_ != net::SchemefulSite(origin_)) {
    return Serialize();
  }

  // Otherwise localStorage expects a slightly different scheme, so call that.
  //
  // TODO(https://crbug.com/1199077): Since the deserialization function can
  // handle Serialize() or SerializeForLocalStorage(), investigate whether we
  // can change the serialization scheme for 1p localStorage without a
  // migration.
  return origin_.Serialize();
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

// static
std::string StorageKey::SerializeAttributeSeparator(
    const StorageKey::EncodedAttribute type) {
  // Create a size 2 string, we'll overwrite the second char later.
  std::string ret(2, '^');
  char digit = static_cast<uint8_t>(type) + '0';
  ret[1] = digit;
  return ret;
}

// static
StorageKey::EncodedAttribute StorageKey::DeserializeAttributeSeparator(
    const base::StringPiece& in) {
  DCHECK_EQ(in.size(), 2U);
  uint8_t number = in[1] - '0';

  if (number >= static_cast<uint8_t>(StorageKey::EncodedAttribute::kMax)) {
    // Bad input, return kMax to indicate an issue.
    return StorageKey::EncodedAttribute::kMax;
  }

  return static_cast<StorageKey::EncodedAttribute>(number);
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
