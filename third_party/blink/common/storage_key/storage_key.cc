// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <cctype>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
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
  // `key`.top_level_site> + "^3" + <StorageKey `key`.ancestor_chain_bit> ]
  // The brackets indicate an optional component.
  // - or -
  // <StorageKey 'key'.origin> + "/" + "^1" + <StorageKey 'nonce'.High64Bits> +
  // "^2" + <StorageKey 'nonce'.Low64Bits>

  // Let's check for a delimiting caret. The presence of a caret means this key
  // is partitioned.

  // More than two encoded attributes (delimited by carets) indicates a
  // malformed input.
  if (base::ranges::count(in, '^') > 2)
    return absl::nullopt;

  size_t pos_first_caret = in.find_first_of('^');
  size_t pos_last_caret = in.find_last_of('^');

  url::Origin key_origin;
  net::SchemefulSite key_top_level_site;
  std::unique_ptr<base::UnguessableToken> nonce;
  blink::mojom::AncestorChainBit ancestor_chain_bit;

  if (pos_first_caret == std::string::npos) {
    // Only the origin is serialized.

    key_origin = url::Origin::Create(GURL(in));

    // In this case the top_level_site is implicitly the same site as the
    // origin.
    key_top_level_site = net::SchemefulSite(key_origin);

    // There is no nonce.

    if (key_origin.opaque())
      return absl::nullopt;

    return StorageKey(key_origin, key_top_level_site, nullptr,
                      blink::mojom::AncestorChainBit::kSameSite);
  }

  if (!ValidSeparatorWithData(in, pos_first_caret))
    return absl::nullopt;

  // Otherwise the key is partitioned, let's see what it's partitioned by.
  absl::optional<EncodedAttribute> first_attribute =
      DeserializeAttributeSeparator(in.substr(pos_first_caret, 2));
  if (!first_attribute.has_value())
    return absl::nullopt;

  switch (first_attribute.value()) {
    case EncodedAttribute::kTopLevelSite: {
      // A top-level site is serialized.

      // The origin is the portion up to, but not including, the caret
      // separator.
      key_origin = url::Origin::Create(GURL(in.substr(0, pos_first_caret)));

      // The top_level_site is the portion between the two separators.
      int length_of_site = pos_last_caret - (pos_first_caret + 2);
      key_top_level_site = net::SchemefulSite(
          GURL(in.substr(pos_first_caret + 2, length_of_site)));

      // There is no nonce.

      // Make sure we found the last separator, it's valid, that it's the
      // correct attribute.
      if (pos_last_caret == std::string::npos ||
          !ValidSeparatorWithData(in, pos_last_caret))
        return absl::nullopt;

      absl::optional<EncodedAttribute> last_attribute =
          DeserializeAttributeSeparator(in.substr(pos_last_caret, 2));
      if (!last_attribute.has_value() ||
          last_attribute.value() != EncodedAttribute::kAncestorChainBit)
        return absl::nullopt;

      // The ancestor_chain_bit is the portion beyond the last separator.
      int raw_bit;
      if (!base::StringToInt(in.substr(pos_last_caret + 2, std::string::npos),
                             &raw_bit))
        return absl::nullopt;

      // If the integer conversion results in a value outside the enumerated
      // indices of [0,1]
      if (raw_bit < 0 || raw_bit > 1)
        return absl::nullopt;
      ancestor_chain_bit = static_cast<blink::mojom::AncestorChainBit>(raw_bit);

      // In addition to checking for opaque-ness, if the AncestorChainBit is
      // marked as kSameSite, check to make sure that the origin is not
      // same-site with the top-level site (i.e.: A first-party StorageKey).
      // This is important because we specifically do not serialize the
      // top-level site portion of a 1p StorageKey for backwards compatibility
      // reasons, meaning that such an input is malformed.
      if (key_origin.opaque() || key_top_level_site.opaque() ||
          (ancestor_chain_bit == blink::mojom::AncestorChainBit::kSameSite &&
           key_top_level_site == net::SchemefulSite(key_origin)))
        return absl::nullopt;

      return StorageKey(key_origin, key_top_level_site, nullptr,
                        ancestor_chain_bit);
    }
    case EncodedAttribute::kNonceHigh: {
      // A nonce is serialized.
      // There should be three caret separators, let's grab the second
      // (the separator between high and low nonce).
      size_t pos_second_caret = in.find_first_of('^', pos_first_caret + 2);

      // Make sure we found the next separator, it's valid, that it's the
      // correct attribute.
      if (pos_second_caret == std::string::npos ||
          !ValidSeparatorWithData(in, pos_second_caret))
        return absl::nullopt;

      absl::optional<EncodedAttribute> second_attribute =
          DeserializeAttributeSeparator(in.substr(pos_second_caret, 2));
      if (!second_attribute.has_value() ||
          second_attribute.value() != EncodedAttribute::kNonceLow)
        return absl::nullopt;

      // The origin is the portion up to, but not including, the first
      // separator.
      key_origin = url::Origin::Create(GURL(in.substr(0, pos_first_caret)));

      // The first high 64 bits of the nonce are next, between the two
      // separators.
      int length_of_high = pos_second_caret - (pos_first_caret + 2);
      int length_of_low = pos_last_caret - (pos_second_caret + 2);
      std::string high_digits = static_cast<std::string>(
          in.substr(pos_first_caret + 2, length_of_high));
      // The low 64 bits are last, after the second separator.
      std::string low_digits = static_cast<std::string>(
          in.substr(pos_second_caret + 2, length_of_low));

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
StorageKey StorageKey::CreateForTesting(const url::Origin& origin,
                                        const url::Origin& top_level_origin) {
  auto top_level_site = net::SchemefulSite(top_level_origin);
  return StorageKey(origin, std::move(top_level_site), nullptr,
                    top_level_site == net::SchemefulSite(origin)
                        ? blink::mojom::AncestorChainBit::kSameSite
                        : blink::mojom::AncestorChainBit::kCrossSite);
}

// static
StorageKey StorageKey::CreateForTesting(
    const url::Origin& origin,
    const net::SchemefulSite& top_level_site) {
  return StorageKey(origin, top_level_site, nullptr,
                    top_level_site == net::SchemefulSite(origin)
                        ? blink::mojom::AncestorChainBit::kSameSite
                        : blink::mojom::AncestorChainBit::kCrossSite);
}

// static
bool StorageKey::IsThirdPartyStoragePartitioningEnabled() {
  return base::FeatureList::IsEnabled(
      net::features::kThirdPartyStoragePartitioning);
}

// static
StorageKey StorageKey::CreateWithNonce(const url::Origin& origin,
                                       const base::UnguessableToken& nonce) {
  DCHECK(!nonce.is_empty());
  // The AncestorChainBit is not applicable to StorageKeys with a non-empty
  // nonce, so they are initialized to be kSameSite.
  return StorageKey(origin, net::SchemefulSite(origin), &nonce,
                    blink::mojom::AncestorChainBit::kSameSite);
}

// static
StorageKey StorageKey::CreateWithOptionalNonce(
    const url::Origin& origin,
    const net::SchemefulSite& top_level_site,
    const base::UnguessableToken* nonce,
    blink::mojom::AncestorChainBit ancestor_chain_bit) {
  DCHECK(!nonce || !nonce->is_empty());
  return StorageKey(origin, top_level_site, nonce, ancestor_chain_bit);
}

// static
StorageKey StorageKey::CreateFromOriginAndIsolationInfo(
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info) {
  return CreateWithOptionalNonce(
      origin, net::SchemefulSite(isolation_info.top_frame_origin().value()),
      base::OptionalToPtr(isolation_info.nonce()),
      isolation_info.site_for_cookies().IsNull()
          ? blink::mojom::AncestorChainBit::kCrossSite
          : blink::mojom::AncestorChainBit::kSameSite);
}

StorageKey StorageKey::WithOrigin(const url::Origin& origin) const {
  return CreateWithOptionalNonce(origin, top_level_site_,
                                 base::OptionalToPtr(nonce_),
                                 ancestor_chain_bit_);
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
  //
  // Note that we intentionally do not include the AncestorChainBit in
  // serialization with nonce formats as that information is not applicable
  // (similar to top-level-site).
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
  // `key`.top_level_site> + "^3" + <StorageKey `key`.ancestor_chain_bit> ]
  //
  // The top_level_site is optional (indicated by the square brackets) if it's
  // the same site as the origin and kSameSite in order to enable backwards
  // compatibility for 1p contexts. Meaning that in the case of a 1p context the
  // serialization structure is the same as if
  // features::kThirdPartyStoragePartitioning was disabled.
  if (IsThirdPartyStoragePartitioningEnabled() &&
      (top_level_site_ != net::SchemefulSite(origin_) ||
       ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kCrossSite)) {
    return base::StrCat(
        {origin_.GetURL().spec(),
         SerializeAttributeSeparator(EncodedAttribute::kTopLevelSite),
         top_level_site_.Serialize(),
         SerializeAttributeSeparator(EncodedAttribute::kAncestorChainBit),
         base::NumberToString(static_cast<int>(ancestor_chain_bit_))});
  }

  return origin_.GetURL().spec();
}

std::string StorageKey::SerializeForLocalStorage() const {
  DCHECK(!origin_.opaque());

  // If this is a third-party StorageKey we'll use the standard serialization
  // scheme when partitioning is enabled or if there is a nonce.
  if (nonce_.has_value() ||
      (IsThirdPartyContext() && IsThirdPartyStoragePartitioningEnabled())) {
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
       ", nonce: ", nonce_.has_value() ? nonce_->ToString() : "<null>",
       ", ancestor chain bit: ",
       ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kSameSite
           ? "Same-Site"
           : "Cross-Site",
       " }"});
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

  if (max_length > memory_dump_str.length()) {
    std::string ancestor_full_string =
        ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kSameSite
            ? "Same-Site"
            : "Cross-Site";
    memory_dump_str.append(
        ancestor_full_string.substr(0, max_length - memory_dump_str.length()));
  }

  base::ranges::replace_if(
      memory_dump_str.begin(), memory_dump_str.end(),
      [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');
  return memory_dump_str;
}

const net::SiteForCookies StorageKey::ToNetSiteForCookies() const {
  if (nonce_ ||
      ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kCrossSite) {
    // If any of the ancestor frames are cross-site to `origin_` then the
    // SiteForCookies should be null. The existence of `nonce_` means the same
    // thing.
    return net::SiteForCookies();
  }

  // The `ancestor_chain_bit_` being kSameSite should already indicate that the
  // `top_level_site_` and `origin_` are same-site.
  DCHECK(top_level_site_ == net::SchemefulSite(origin_));
  return net::SiteForCookies(top_level_site_);
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
absl::optional<StorageKey::EncodedAttribute>
StorageKey::DeserializeAttributeSeparator(const base::StringPiece& in) {
  DCHECK_EQ(in.size(), 2U);
  uint8_t number = in[1] - '0';

  if (number > static_cast<uint8_t>(StorageKey::EncodedAttribute::kMaxValue)) {
    // Bad input, return absl::nullopt to indicate an issue.
    return absl::nullopt;
  }

  return static_cast<StorageKey::EncodedAttribute>(number);
}

// static
bool StorageKey::ShouldSkipKeyDueToPartitioning(
    const std::string& reg_key_string) {
  // Don't skip anything if storage partitioning is enabled.
  if (IsThirdPartyStoragePartitioningEnabled())
    return false;

  // Determine if there is a valid attribute encoded with a caret
  size_t pos_first_caret = reg_key_string.find_first_of('^');
  if (pos_first_caret != std::string::npos &&
      ValidSeparatorWithData(reg_key_string, pos_first_caret)) {
    absl::optional<EncodedAttribute> attribute = DeserializeAttributeSeparator(
        reg_key_string.substr(pos_first_caret, 2));
    // Do skip if partitioning is disabled and we detect a top-level site
    // serialization scheme:
    if (attribute.has_value() &&
        attribute == StorageKey::EncodedAttribute::kTopLevelSite)
      return true;
  }
  // If otherwise first-party, nonce, or corrupted, don't skip.
  return false;
}

const absl::optional<net::CookiePartitionKey> StorageKey::ToCookiePartitionKey()
    const {
  return net::CookiePartitionKey::FromStorageKeyComponents(top_level_site_,
                                                           nonce_);
}

bool StorageKey::MatchesOriginForTrustedStorageDeletion(
    const url::Origin& origin) const {
  if (!IsThirdPartyStoragePartitioningEnabled())
    return origin_ == origin;
  // TODO(crbug.com/1382138): Address wss:// and https:// resulting in different
  // SchemefulSites.
  return (ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kSameSite)
             ? (origin_ == origin)
             : (top_level_site_ == net::SchemefulSite(origin));
}

bool operator==(const StorageKey& lhs, const StorageKey& rhs) {
  return std::tie(lhs.origin_, lhs.top_level_site_, lhs.nonce_,
                  lhs.ancestor_chain_bit_) ==
         std::tie(rhs.origin_, rhs.top_level_site_, rhs.nonce_,
                  rhs.ancestor_chain_bit_);
}

bool operator!=(const StorageKey& lhs, const StorageKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const StorageKey& lhs, const StorageKey& rhs) {
  return std::tie(lhs.origin_, lhs.top_level_site_, lhs.nonce_,
                  lhs.ancestor_chain_bit_) <
         std::tie(rhs.origin_, rhs.top_level_site_, rhs.nonce_,
                  rhs.ancestor_chain_bit_);
}

std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk) {
  return ostream << sk.GetDebugString();
}

}  // namespace blink
