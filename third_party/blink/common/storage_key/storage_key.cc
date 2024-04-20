// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "net/base/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "url/gurl.h"

namespace {

// This enum represents the different type of encodable partitioning
// attributes. These values are persisted to disk. Entries should not be
// renumbered and numeric values should never be reused.
enum class EncodedAttribute : uint8_t {
  kTopLevelSite = 0,
  kNonceHigh = 1,
  kNonceLow = 2,
  kAncestorChainBit = 3,
  kTopLevelSiteOpaqueNonceHigh = 4,
  kTopLevelSiteOpaqueNonceLow = 5,
  kTopLevelSiteOpaquePrecursor = 6,
  kMaxValue = kTopLevelSiteOpaquePrecursor,
};

// Converts the attribute type into the separator + uint8_t byte
// serialization. E.x.: kTopLevelSite becomes "^0"
std::string SerializeAttributeSeparator(const EncodedAttribute type) {
  // Create a size 2 string, we'll overwrite the second char later.
  std::string ret(2, '^');
  char digit = static_cast<uint8_t>(type) + '0';
  ret[1] = digit;
  return ret;
}

// Converts the serialized separator into an EncodedAttribute enum.
// E.x.: "^0" becomes kTopLevelSite.
// Expects `in` to have a length of 2.
std::optional<EncodedAttribute> DeserializeAttributeSeparator(
    const std::string_view& in) {
  DCHECK_EQ(in.size(), 2U);
  uint8_t number = in[1] - '0';

  if (number > static_cast<uint8_t>(EncodedAttribute::kMaxValue)) {
    // Bad input, return std::nullopt to indicate an issue.
    return std::nullopt;
  }

  return static_cast<EncodedAttribute>(number);
}

// Returns true if there are at least 2 chars after the '^' in `in` and the
// second char is not '^'. Meaning that the substring is syntactically valid.
// This is to indicate that there is a valid separator with both a '^' and a
// uint8_t and some amount of encoded data. I.e.: "^09" has both a "^0" as the
// separator and '9' as the encoded data.
bool ValidSeparatorWithData(std::string_view in, size_t pos_of_caret) {
  if (in.length() > pos_of_caret + 2 && in[pos_of_caret + 2] != '^')
    return true;

  return false;
}

}  // namespace

namespace blink {

// static
std::optional<StorageKey> StorageKey::Deserialize(std::string_view in) {
  // As per the Serialize() call, we have to expect one of the following
  // structures:
  // <StorageKey `key`.origin> + "/" + "^1" + <StorageKey
  // `key`.nonce.High64Bits> + "^2" + <StorageKey `key`.nonce.Low64Bits>
  // - or -
  // <StorageKey `key`.origin> + "/"
  // - or -
  // <StorageKey `key`.origin> + "/" + "^3" + <StorageKey
  // `key`.ancestor_chain_bit>
  // - or -
  // <StorageKey `key`.origin> + "/" + "^0" + <StorageKey `key`.top_level_site>
  // - or -
  // <StorageKey `key`.origin> + "/" + ^4" + <StorageKey
  // `key`.top_level_site.nonce.High64Bits> + "^5" + <StorageKey
  // `key`.top_level_site.nonce.Low64Bits>  + "^6" + <StorageKey
  // `key`.top_level_site.precursor>
  //
  // See Serialize() for more information.

  // Let's check for a delimiting caret. The presence of a caret means this key
  // is partitioned.

  // More than three encoded attributes (delimited by carets) indicates a
  // malformed input.
  if (base::ranges::count(in, '^') > 3) {
    return std::nullopt;
  }

  const size_t pos_first_caret = in.find_first_of('^');
  const size_t pos_second_caret =
      pos_first_caret == std::string::npos
          ? std::string::npos
          : in.find_first_of('^', pos_first_caret + 1);
  const size_t pos_third_caret =
      pos_second_caret == std::string::npos
          ? std::string::npos
          : in.find_first_of('^', pos_second_caret + 1);

  url::Origin key_origin;
  net::SchemefulSite key_top_level_site;
  std::optional<base::UnguessableToken> nonce;
  blink::mojom::AncestorChainBit ancestor_chain_bit;

  if (pos_first_caret == std::string::npos) {
    // Only the origin is serialized.

    key_origin = url::Origin::Create(GURL(in));

    // In this case the top_level_site is implicitly the same site as the
    // origin.
    key_top_level_site = net::SchemefulSite(key_origin);

    // There is no nonce.

    // The origin should not be opaque and the serialization should be
    // reversible.
    if (key_origin.opaque() || key_origin.GetURL().spec() != in) {
      return std::nullopt;
    }

    return StorageKey(key_origin, key_top_level_site, nullptr,
                      blink::mojom::AncestorChainBit::kSameSite,
                      /*third_party_partitioning_allowed=*/false);
  }

  if (!ValidSeparatorWithData(in, pos_first_caret))
    return std::nullopt;

  // Otherwise the key is partitioned, let's see what it's partitioned by.
  std::optional<EncodedAttribute> first_attribute =
      DeserializeAttributeSeparator(in.substr(pos_first_caret, 2));
  if (!first_attribute.has_value())
    return std::nullopt;

  switch (first_attribute.value()) {
    case EncodedAttribute::kTopLevelSite: {
      // Cross-Origin keys cannot be read if partitioning is off.
      if (!IsThirdPartyStoragePartitioningEnabled()) {
        return std::nullopt;
      }

      // A top-level site is serialized and has only one encoded attribute.
      if (pos_second_caret != std::string::npos) {
        return std::nullopt;
      }

      // The origin is the portion up to, but not including, the caret
      // separator.
      const std::string_view origin_substr = in.substr(0, pos_first_caret);
      key_origin = url::Origin::Create(GURL(origin_substr));

      // The origin should not be opaque and the serialization should be
      // reversible.
      if (key_origin.opaque() || key_origin.GetURL().spec() != origin_substr) {
        return std::nullopt;
      }

      // The top_level_site is the portion beyond the first separator.
      int length_of_site = pos_second_caret - (pos_first_caret + 2);
      const std::string_view top_level_site_substr =
          in.substr(pos_first_caret + 2, length_of_site);
      key_top_level_site = net::SchemefulSite(GURL(top_level_site_substr));

      // The top level site should not be opaque and the serialization should be
      // reversible.
      if (key_top_level_site.opaque() ||
          key_top_level_site.Serialize() != top_level_site_substr) {
        return std::nullopt;
      }

      // There is no nonce or ancestor chain bit.

      // Neither should be opaque and they cannot match as that would mean
      // we should have simply encoded the origin and the input is malformed.
      if (key_origin.opaque() || key_top_level_site.opaque() ||
          net::SchemefulSite(key_origin) == key_top_level_site) {
        return std::nullopt;
      }

      // The ancestor chain bit must be CrossSite as that's an invariant
      // when the origin and top level site don't match.
      // TODO(crbug.com/1199077): Deserialize should always be able to make 3p
      // keys and shouldn't depend on the state of partitioning (because we
      // don't want to inadvertently turn two 3p keys into the same 1p key).
      // Unfortunately, some tests (and potentially code) depend on this. Here,
      // and below, should be changed to true and the dependencies on this
      // behavior should be removed.
      return StorageKey(key_origin, key_top_level_site, nullptr,
                        blink::mojom::AncestorChainBit::kCrossSite,
                        IsThirdPartyStoragePartitioningEnabled());
    }
    case EncodedAttribute::kAncestorChainBit: {
      // Same-Origin kCrossSite keys cannot be read if partitioning is off.
      if (!IsThirdPartyStoragePartitioningEnabled()) {
        return std::nullopt;
      }

      // An ancestor chain bit is serialized and has only one encoded attribute.
      if (pos_second_caret != std::string::npos) {
        return std::nullopt;
      }

      // The origin is the portion up to, but not including, the caret
      // separator.
      const std::string_view origin_substr = in.substr(0, pos_first_caret);
      key_origin = url::Origin::Create(GURL(origin_substr));

      // The origin should not be opaque and the serialization should be
      // reversible.
      if (key_origin.opaque() || key_origin.GetURL().spec() != origin_substr) {
        return std::nullopt;
      }

      // The ancestor_chain_bit is the portion beyond the first separator.
      int raw_bit;
      const std::string_view raw_bit_substr =
          in.substr(pos_first_caret + 2, std::string::npos);
      if (!base::StringToInt(raw_bit_substr, &raw_bit)) {
        return std::nullopt;
      }

      // If the integer conversion results in a value outside the enumerated
      // indices of [0,1] or trimmed leading 0s we must reject the key.
      if (raw_bit < 0 || raw_bit > 1 || raw_bit_substr.size() > 1) {
        return std::nullopt;
      }
      ancestor_chain_bit = static_cast<blink::mojom::AncestorChainBit>(raw_bit);

      // There is no nonce or top level site.

      // The origin shouldn't be opaque and ancestor chain bit must be CrossSite
      // as otherwise should have simply encoded the origin and the input is
      // malformed.
      if (ancestor_chain_bit != blink::mojom::AncestorChainBit::kCrossSite) {
        return std::nullopt;
      }

      // This format indicates the top level site matches the origin.
      return StorageKey(key_origin, net::SchemefulSite(key_origin), nullptr,
                        ancestor_chain_bit,
                        IsThirdPartyStoragePartitioningEnabled());
    }
    case EncodedAttribute::kNonceHigh: {
      // A nonce is serialized and has only two encoded attributes.
      if (pos_third_caret != std::string::npos) {
        return std::nullopt;
      }

      // Make sure we found the next separator, it's valid, that it's the
      // correct attribute.
      if (pos_second_caret == std::string::npos ||
          !ValidSeparatorWithData(in, pos_second_caret))
        return std::nullopt;

      std::optional<EncodedAttribute> second_attribute =
          DeserializeAttributeSeparator(in.substr(pos_second_caret, 2));
      if (!second_attribute.has_value() ||
          second_attribute.value() != EncodedAttribute::kNonceLow)
        return std::nullopt;

      // The origin is the portion up to, but not including, the first
      // separator.
      const std::string_view origin_substr = in.substr(0, pos_first_caret);
      key_origin = url::Origin::Create(GURL(origin_substr));

      // The origin should not be opaque and the serialization should be
      // reversible.
      if (key_origin.opaque() || key_origin.GetURL().spec() != origin_substr) {
        return std::nullopt;
      }

      // The first high 64 bits of the nonce are next, between the two
      // separators.
      int length_of_high = pos_second_caret - (pos_first_caret + 2);
      std::string_view high_digits =
          in.substr(pos_first_caret + 2, length_of_high);
      // The low 64 bits are last, after the second separator.
      std::string_view low_digits = in.substr(pos_second_caret + 2);

      uint64_t nonce_high = 0;
      uint64_t nonce_low = 0;

      if (!base::StringToUint64(high_digits, &nonce_high))
        return std::nullopt;

      if (!base::StringToUint64(low_digits, &nonce_low))
        return std::nullopt;

      // The key is corrupted if there are extra 0s in front of the nonce.
      if (base::NumberToString(nonce_high) != high_digits ||
          base::NumberToString(nonce_low) != low_digits) {
        return std::nullopt;
      }

      nonce = base::UnguessableToken::Deserialize(nonce_high, nonce_low);

      if (!nonce.has_value()) {
        return std::nullopt;
      }

      // This constructor makes a copy of the nonce, so getting the raw pointer
      // is safe.
      // Note: The partitioning allowed value is irrelevant with a nonce,
      // `false` was chosen arbitrarily.
      return StorageKey(key_origin, net::SchemefulSite(key_origin),
                        &nonce.value(),
                        blink::mojom::AncestorChainBit::kCrossSite,
                        /*third_party_partitioning_allowed=*/false);
    }
    case EncodedAttribute::kTopLevelSiteOpaqueNonceHigh: {
      // An opaque `top_level_site` is serialized.

      // Cross-Origin keys cannot be read if partitioning is off.
      if (!IsThirdPartyStoragePartitioningEnabled()) {
        return std::nullopt;
      }

      // Make sure we found the next separator, it's valid, that it's the
      // correct attribute.
      if (pos_second_caret == std::string::npos ||
          !ValidSeparatorWithData(in, pos_second_caret)) {
        return std::nullopt;
      }

      std::optional<EncodedAttribute> second_attribute =
          DeserializeAttributeSeparator(in.substr(pos_second_caret, 2));
      if (!second_attribute.has_value() ||
          second_attribute.value() !=
              EncodedAttribute::kTopLevelSiteOpaqueNonceLow) {
        return std::nullopt;
      }

      // The origin is the portion up to, but not including, the first
      // separator.
      const std::string_view origin_substr = in.substr(0, pos_first_caret);
      key_origin = url::Origin::Create(GURL(origin_substr));

      // The origin should not be opaque and the serialization should be
      // reversible.
      if (key_origin.opaque() || key_origin.GetURL().spec() != origin_substr) {
        return std::nullopt;
      }

      // The first high 64 bits of the sites's nonce are next, between the first
      // separators.
      int length_of_high = pos_second_caret - (pos_first_caret + 2);
      std::string_view high_digits =
          in.substr(pos_first_caret + 2, length_of_high);
      // The low 64 bits are next, after the second separator.
      int length_of_low = pos_third_caret - (pos_second_caret + 2);
      std::string_view low_digits =
          in.substr(pos_second_caret + 2, length_of_low);

      uint64_t nonce_high = 0;
      uint64_t nonce_low = 0;

      if (!base::StringToUint64(high_digits, &nonce_high)) {
        return std::nullopt;
      }

      if (!base::StringToUint64(low_digits, &nonce_low)) {
        return std::nullopt;
      }

      // The key is corrupted if there are extra 0s in front of the nonce.
      if (base::NumberToString(nonce_high) != high_digits ||
          base::NumberToString(nonce_low) != low_digits) {
        return std::nullopt;
      }

      const std::optional<base::UnguessableToken> site_nonce =
          base::UnguessableToken::Deserialize(nonce_high, nonce_low);

      // The nonce must have content.
      if (!site_nonce) {
        return std::nullopt;
      }

      // Make sure we found the final separator, it's valid, that it's the
      // correct attribute.
      if (pos_third_caret == std::string::npos ||
          (in.size() - pos_third_caret) < 2) {
        return std::nullopt;
      }

      std::optional<EncodedAttribute> third_attribute =
          DeserializeAttributeSeparator(in.substr(pos_third_caret, 2));
      if (!third_attribute.has_value() ||
          third_attribute.value() !=
              EncodedAttribute::kTopLevelSiteOpaquePrecursor) {
        return std::nullopt;
      }

      // The precursor is the rest of the input.
      const std::string_view url_precursor_substr =
          in.substr(pos_third_caret + 2);
      const GURL url_precursor(url_precursor_substr);
      const url::SchemeHostPort tuple_precursor(url_precursor);

      // The precursor must be empry or valid, and the serialization should be
      // reversible.
      if ((!url_precursor.is_empty() && !tuple_precursor.IsValid()) ||
          tuple_precursor.Serialize() != url_precursor_substr) {
        return std::nullopt;
      }

      // This constructor makes a copy of the site's nonce, so getting the raw
      // pointer is safe.
      return StorageKey(
          key_origin,
          net::SchemefulSite(url::Origin(url::Origin::Nonce(site_nonce.value()),
                                         tuple_precursor)),
          nullptr, blink::mojom::AncestorChainBit::kCrossSite,
          IsThirdPartyStoragePartitioningEnabled());
    }
    default: {
      // Malformed input case. We saw a separator that we don't understand
      // or one in the wrong order.
      return std::nullopt;
    }
  }
}

// static
std::optional<StorageKey> StorageKey::DeserializeForLocalStorage(
    std::string_view in) {
  // We have to support the local storage specific variant that lacks the
  // trailing slash.
  const url::Origin maybe_origin = url::Origin::Create(GURL(in));
  if (!maybe_origin.opaque()) {
    if (maybe_origin.Serialize() == in) {
      return StorageKey(maybe_origin, net::SchemefulSite(maybe_origin), nullptr,
                        blink::mojom::AncestorChainBit::kSameSite,
                        /*third_party_partitioning_allowed=*/false);
    } else if (maybe_origin.GetURL().spec() == in) {
      // This first party key was passed in with a trailing slash. This is
      // required in Deserialize() but improper for DeserializeForLocalStorage()
      // and must be rejected.
      return std::nullopt;
    }
  }

  // Otherwise we fallback on base deserialization.
  return Deserialize(in);
}

// static
StorageKey StorageKey::CreateFromStringForTesting(const std::string& origin) {
  return CreateFirstParty(url::Origin::Create(GURL(origin)));
}

// static
// Keep consistent with BlinkStorageKey::FromWire().
bool StorageKey::FromWire(
    const url::Origin& origin,
    const net::SchemefulSite& top_level_site,
    const net::SchemefulSite& top_level_site_if_third_party_enabled,
    const std::optional<base::UnguessableToken>& nonce,
    blink::mojom::AncestorChainBit ancestor_chain_bit,
    blink::mojom::AncestorChainBit ancestor_chain_bit_if_third_party_enabled,
    StorageKey& out) {
  // We need to build a different key to prevent overriding `out` if the result
  // isn't valid.
  StorageKey maybe_out;
  maybe_out.origin_ = origin;
  maybe_out.top_level_site_ = top_level_site;
  maybe_out.top_level_site_if_third_party_enabled_ =
      top_level_site_if_third_party_enabled;
  maybe_out.nonce_ = nonce;
  maybe_out.ancestor_chain_bit_ = ancestor_chain_bit;
  maybe_out.ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_if_third_party_enabled;
  if (maybe_out.IsValid()) {
    out = maybe_out;
    return true;
  }
  return false;
}

// static
bool StorageKey::IsThirdPartyStoragePartitioningEnabled() {
  return base::FeatureList::IsEnabled(
      net::features::kThirdPartyStoragePartitioning);
}

// static
StorageKey StorageKey::CreateFirstParty(const url::Origin& origin) {
  return StorageKey(origin, net::SchemefulSite(origin), nullptr,
                    origin.opaque() ? blink::mojom::AncestorChainBit::kCrossSite
                                    : blink::mojom::AncestorChainBit::kSameSite,
                    /*third_party_partitioning_allowed=*/false);
}

// static
StorageKey StorageKey::CreateWithNonce(const url::Origin& origin,
                                       const base::UnguessableToken& nonce) {
  // The AncestorChainBit is not applicable to StorageKeys with a non-empty
  // nonce, so they are initialized to be kCrossSite.
  // Note: The partitioning allowed value is irrelevant with a nonce, `false`
  // was chosen arbitrarily.
  return StorageKey(origin, net::SchemefulSite(origin), &nonce,
                    blink::mojom::AncestorChainBit::kCrossSite,
                    /*third_party_partitioning_allowed=*/false);
}

// static
StorageKey StorageKey::Create(const url::Origin& origin,
                              const net::SchemefulSite& top_level_site,
                              blink::mojom::AncestorChainBit ancestor_chain_bit,
                              bool third_party_partitioning_allowed) {
  return StorageKey(origin, top_level_site, nullptr, ancestor_chain_bit,
                    third_party_partitioning_allowed);
}

// static
StorageKey StorageKey::CreateFromOriginAndIsolationInfo(
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info) {
  if (isolation_info.nonce()) {
    // If the nonce is set we can use the simpler construction path.
    return CreateWithNonce(origin, *isolation_info.nonce());
  }

  blink::mojom::AncestorChainBit ancestor_chain_bit =
      blink::mojom::AncestorChainBit::kCrossSite;
  net::SchemefulSite top_level_site =
      net::SchemefulSite(isolation_info.top_frame_origin().value());
  // If the origin or top_level_site is opaque the ancestor chain bit will be
  // CrossSite. Otherwise if the top level site matches the new origin and the
  // site for cookies isn't empty it must be SameSite.
  if (!origin.opaque() && !top_level_site.opaque() &&
      net::SchemefulSite(origin) == top_level_site &&
      !isolation_info.site_for_cookies().IsNull()) {
    ancestor_chain_bit = blink::mojom::AncestorChainBit::kSameSite;
  }
  return Create(origin, top_level_site, ancestor_chain_bit,
                IsThirdPartyStoragePartitioningEnabled());
}

StorageKey StorageKey::WithOrigin(const url::Origin& origin) const {
  net::SchemefulSite top_level_site = top_level_site_;
  net::SchemefulSite top_level_site_if_third_party_enabled =
      top_level_site_if_third_party_enabled_;
  blink::mojom::AncestorChainBit ancestor_chain_bit = ancestor_chain_bit_;
  blink::mojom::AncestorChainBit ancestor_chain_bit_if_third_party_enabled =
      ancestor_chain_bit_if_third_party_enabled_;

  if (nonce_) {
    // If the nonce is set we have to update the top level site to match origin
    // as that's an invariant.
    top_level_site = net::SchemefulSite(origin);
    top_level_site_if_third_party_enabled = top_level_site;
  } else if (!top_level_site_.opaque()) {
    // If `top_level_site_` is opaque then so is
    // `top_level_site_if_third_party_enabled` and we don't need to explicitly
    // check it. The ancestor chain bit also doesn't need to be changed in this
    // case.

    // Only adjust the ancestor chain bit if it's currently kSameSite but the
    // new origin and top level site don't match. Note that the ACB might not
    // necessarily be kSameSite if the TLS and origin do match, so we won't
    // adjust the other way.
    if (ancestor_chain_bit == blink::mojom::AncestorChainBit::kSameSite &&
        net::SchemefulSite(origin) != top_level_site_) {
      ancestor_chain_bit = blink::mojom::AncestorChainBit::kCrossSite;
    }

    if (ancestor_chain_bit_if_third_party_enabled ==
            blink::mojom::AncestorChainBit::kSameSite &&
        net::SchemefulSite(origin) != top_level_site_if_third_party_enabled) {
      ancestor_chain_bit_if_third_party_enabled =
          blink::mojom::AncestorChainBit::kCrossSite;
    }
  }

  StorageKey out = *this;
  out.origin_ = origin;
  out.top_level_site_ = top_level_site;
  out.top_level_site_if_third_party_enabled_ =
      top_level_site_if_third_party_enabled;
  out.ancestor_chain_bit_ = ancestor_chain_bit;
  out.ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_if_third_party_enabled;
  DCHECK(out.IsValid());
  return out;
}

StorageKey::StorageKey(const url::Origin& origin,
                       const net::SchemefulSite& top_level_site,
                       const base::UnguessableToken* nonce,
                       blink::mojom::AncestorChainBit ancestor_chain_bit,
                       bool third_party_partitioning_allowed)
    : origin_(origin),
      top_level_site_(third_party_partitioning_allowed
                          ? top_level_site
                          : net::SchemefulSite(origin)),
      top_level_site_if_third_party_enabled_(top_level_site),
      nonce_(base::OptionalFromPtr(nonce)),
      ancestor_chain_bit_(third_party_partitioning_allowed ? ancestor_chain_bit
                          : (nonce || origin.opaque())
                              ? blink::mojom::AncestorChainBit::kCrossSite
                              : blink::mojom::AncestorChainBit::kSameSite),
      ancestor_chain_bit_if_third_party_enabled_(ancestor_chain_bit) {
  DCHECK(IsValid());
}

std::string StorageKey::Serialize() const {
  DCHECK(!origin_.opaque());

  // If the storage key has a nonce, implying the top_level_site is the same as
  // origin and ancestor_chain_bit is kCrossSite, then we need to serialize the
  // key to fit the following scheme:
  //
  // Case 0: <StorageKey `key`.origin> + "/" + "^1" + <StorageKey
  // `key`.nonce.High64Bits> + "^2" + <StorageKey `key`.nonce.Low64Bits>
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
  // one of the following schemes:
  //
  // Case 1: If the ancestor_chain_bit is kSameSite or partitioning is disabled:
  //
  // <StorageKey `key`.origin> + "/"
  //
  // Case 2: If the origin matches top_level_site and the ancestor_chain_bit is
  // kCrossSite:
  //
  // <StorageKey `key`.origin> + "/" + "^3" + <StorageKey
  // `key`.ancestor_chain_bit>
  //
  // Case 3: If the origin doesn't match top_level_site (implying
  // ancestor_chain_bit is kCrossSite):
  //
  // <StorageKey `key`.origin> + "/" + "^0" + <StorageKey `key`.top_level_site>
  //
  // Case 4: If the top_level_site is opaque (implying ancestor_chain_bit is
  // kCrossSite):
  //
  // <StorageKey `key`.origin> + "/" + ^4" + <StorageKey
  // `key`.top_level_site.nonce.High64Bits> + "^5" + <StorageKey
  // `key`.top_level_site.nonce.Low64Bits>  + "^6" + <StorageKey
  // `key`.top_level_site.precursor>
  if (IsThirdPartyStoragePartitioningEnabled() &&
      ancestor_chain_bit_ == blink::mojom::AncestorChainBit::kCrossSite) {
    if (top_level_site_.opaque()) {
      // Case 4.
      return base::StrCat({
          origin_.GetURL().spec(),
          SerializeAttributeSeparator(
              EncodedAttribute::kTopLevelSiteOpaqueNonceHigh),
          base::NumberToString(top_level_site_.internal_value()
                                   .GetNonceForSerialization()
                                   ->GetHighForSerialization()),
          SerializeAttributeSeparator(
              EncodedAttribute::kTopLevelSiteOpaqueNonceLow),
          base::NumberToString(top_level_site_.internal_value()
                                   .GetNonceForSerialization()
                                   ->GetLowForSerialization()),
          SerializeAttributeSeparator(
              EncodedAttribute::kTopLevelSiteOpaquePrecursor),
          top_level_site_.internal_value()
              .GetTupleOrPrecursorTupleIfOpaque()
              .Serialize(),
      });
    } else if (top_level_site_ == net::SchemefulSite(origin_)) {
      // Case 2.
      return base::StrCat({
          origin_.GetURL().spec(),
          SerializeAttributeSeparator(EncodedAttribute::kAncestorChainBit),
          base::NumberToString(static_cast<int>(ancestor_chain_bit_)),
      });
    } else {
      // Case 3.
      return base::StrCat({
          origin_.GetURL().spec(),
          SerializeAttributeSeparator(EncodedAttribute::kTopLevelSite),
          top_level_site_.Serialize(),
      });
    }
  }

  // Case 1.
  return origin_.GetURL().spec();
}

std::string StorageKey::SerializeForLocalStorage() const {
  DCHECK(!origin_.opaque());

  // If this is a third-party StorageKey we'll use the standard serialization
  // scheme when partitioning is enabled or if there is a nonce.
  if (IsThirdPartyContext()) {
    return Serialize();
  }

  // Otherwise localStorage expects a slightly different scheme, so call that.
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
      [](char c) {
        return !absl::ascii_isalnum(static_cast<unsigned char>(c));
      },
      '_');
  return memory_dump_str;
}

const net::SiteForCookies StorageKey::ToNetSiteForCookies() const {
  if (IsThirdPartyContext()) {
    // If any of the ancestor frames are cross-site to `origin_` then the
    // SiteForCookies should be null. The existence of `nonce_` means the same
    // thing.
    return net::SiteForCookies();
  }

  // Otherwise we are in a first party context.
  return net::SiteForCookies(top_level_site_);
}

const net::IsolationInfo StorageKey::ToPartialNetIsolationInfo() const {
  url::Origin top_frame_origin =
      IsFirstPartyContext() ? origin_
                            : url::Origin::Create(top_level_site_.GetURL());
  return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                    top_frame_origin, origin_,
                                    ToNetSiteForCookies(), nonce_);
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
    std::optional<EncodedAttribute> attribute = DeserializeAttributeSeparator(
        reg_key_string.substr(pos_first_caret, 2));
    // Do skip if partitioning is disabled and we detect a top-level site
    // serialization scheme (opaque or otherwise) or an ancestor chain bit:
    if (attribute.has_value() &&
        (attribute == EncodedAttribute::kTopLevelSite ||
         attribute == EncodedAttribute::kAncestorChainBit ||
         attribute == EncodedAttribute::kTopLevelSiteOpaqueNonceHigh)) {
      return true;
    }
  }
  // If otherwise first-party, nonce, or corrupted, don't skip.
  return false;
}

const std::optional<net::CookiePartitionKey> StorageKey::ToCookiePartitionKey()
    const {
  return net::CookiePartitionKey::FromStorageKeyComponents(
      top_level_site_,
      net::CookiePartitionKey::BoolToAncestorChainBit(IsThirdPartyContext()),
      nonce_);
}

bool StorageKey::MatchesOriginForTrustedStorageDeletion(
    const url::Origin& origin) const {
  // TODO(crbug.com/1382138): Address wss:// and https:// resulting in different
  // SchemefulSites.
  // TODO(crbug.com/1410196): Test that StorageKeys corresponding to anonymous
  // iframes are handled appropriately here.
  return IsFirstPartyContext()
             ? (origin_ == origin)
             : (top_level_site_ == net::SchemefulSite(origin));
}

bool StorageKey::MatchesRegistrableDomainForTrustedStorageDeletion(
    std::string_view domain) const {
  // TODO(crbug.com/1410196): Test that StorageKeys corresponding to anonymous
  // iframes are handled appropriately here.
  return top_level_site_.registrable_domain_or_host() == domain;
}

bool StorageKey::ExactMatchForTesting(const StorageKey& other) const {
  return *this == other &&
         this->ancestor_chain_bit_if_third_party_enabled_ ==
             other.ancestor_chain_bit_if_third_party_enabled_ &&
         this->top_level_site_if_third_party_enabled_ ==
             other.top_level_site_if_third_party_enabled_;
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

bool StorageKey::IsValid() const {
  // If the key's origin is opaque ancestor_chain_bit* is always kCrossSite
  // no matter the value of the other members.
  if (origin_.opaque()) {
    if (ancestor_chain_bit_ != blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }
    if (ancestor_chain_bit_if_third_party_enabled_ !=
        blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  // If this key's "normal" members indicate a 3p key, then the
  // *_if_third_party_enabled counterparts must match them.
  if (!origin_.opaque() &&
      (top_level_site_ != net::SchemefulSite(origin_) ||
       ancestor_chain_bit_ != blink::mojom::AncestorChainBit::kSameSite)) {
    if (top_level_site_ != top_level_site_if_third_party_enabled_) {
      return false;
    }
    if (ancestor_chain_bit_ != ancestor_chain_bit_if_third_party_enabled_) {
      return false;
    }
  }

  // If top_level_site* is cross-site to origin, then ancestor_chain_bit* must
  // indicate that. An opaque top_level_site* must have a cross-site
  // ancestor_chain_bit*.
  if (top_level_site_ != net::SchemefulSite(origin_)) {
    if (ancestor_chain_bit_ != blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  if (top_level_site_if_third_party_enabled_ != net::SchemefulSite(origin_)) {
    if (ancestor_chain_bit_if_third_party_enabled_ !=
        blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  // If there is a nonce, all other values must indicate same-site to origin.
  if (nonce_) {
    if (nonce_->is_empty()) {
      return false;
    }
    if (top_level_site_ != net::SchemefulSite(origin_)) {
      return false;
    }

    if (top_level_site_if_third_party_enabled_ != net::SchemefulSite(origin_)) {
      return false;
    }

    if (ancestor_chain_bit_ != blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }

    if (ancestor_chain_bit_if_third_party_enabled_ !=
        blink::mojom::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  // If the state is not invalid, it must be valid!
  return true;
}

}  // namespace blink
