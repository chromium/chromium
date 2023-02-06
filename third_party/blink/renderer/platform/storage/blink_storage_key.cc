// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

#include <ostream>

#include "base/types/optional_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"

namespace blink {

BlinkStorageKey::BlinkStorageKey()
    : BlinkStorageKey(SecurityOrigin::CreateUniqueOpaque(),
                      BlinkSchemefulSite(),
                      nullptr,
                      mojom::blink::AncestorChainBit::kSameSite) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin)
    : BlinkStorageKey(std::move(origin), nullptr) {}

// The AncestorChainBit is not applicable to StorageKeys with a non-empty
// nonce, so they are initialized to be kSameSite.
BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                                 const base::UnguessableToken* nonce)
    : BlinkStorageKey(origin,
                      BlinkSchemefulSite(origin),
                      nonce,
                      mojom::blink::AncestorChainBit::kSameSite) {}

BlinkStorageKey::BlinkStorageKey(
    scoped_refptr<const SecurityOrigin> origin,
    const BlinkSchemefulSite& top_level_site,
    const base::UnguessableToken* nonce,
    mojom::blink::AncestorChainBit ancestor_chain_bit)
    : origin_(origin),
      top_level_site_(StorageKey::IsThirdPartyStoragePartitioningEnabled()
                          ? top_level_site
                          : BlinkSchemefulSite(origin)),
      top_level_site_if_third_party_enabled_(top_level_site),
      nonce_(base::OptionalFromPtr(nonce)),
      ancestor_chain_bit_(StorageKey::IsThirdPartyStoragePartitioningEnabled()
                              ? ancestor_chain_bit
                              : mojom::blink::AncestorChainBit::kSameSite),
      ancestor_chain_bit_if_third_party_enabled_(ancestor_chain_bit) {
#if DCHECK_IS_ON()
  DCHECK(origin_);
  if (nonce) {
    // If we're setting a `nonce`, the `top_level_site` must be the same as
    // the `origin` and the `ancestor_chain_bit` must be kSameSite. We don't
    // serialize those pieces of information so have to check to prevent
    // mistaken reliance on what is supposed to be an invariant.
    DCHECK(!nonce->is_empty());
    DCHECK(top_level_site == BlinkSchemefulSite(origin));
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kSameSite);
  } else if (top_level_site.IsOpaque()) {
    // If we're setting an opaque `top_level_site`, the `ancestor_chain_bit`
    // must be kSameSite. We don't serialize that information so have to check
    // to prevent mistaken reliance on what is supposed to be an invariant.
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kSameSite);
  } else if (top_level_site != BlinkSchemefulSite(origin)) {
    // If `top_level_site` doesn't match `origin` then we must be making a
    // third-party StorageKey and `ancestor_chain_bit` must be kCrossSite.
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kCrossSite);
  }
#endif
}

// static
// The AncestorChainBit is not applicable to StorageKeys with a non-empty
// nonce, so they are initialized to be kSameSite.
BlinkStorageKey BlinkStorageKey::CreateWithNonce(
    scoped_refptr<const SecurityOrigin> origin,
    const base::UnguessableToken& nonce) {
  DCHECK(!nonce.is_empty());
  return BlinkStorageKey(std::move(origin), &nonce);
}

// static
BlinkStorageKey BlinkStorageKey::CreateFromStringForTesting(
    const WTF::String& origin) {
  return BlinkStorageKey(SecurityOrigin::CreateFromString(origin));
}

// static
BlinkStorageKey BlinkStorageKey::CreateForTesting(
    scoped_refptr<const SecurityOrigin> origin,
    const BlinkSchemefulSite& top_level_site) {
  return BlinkStorageKey(origin, top_level_site, nullptr,
                         (BlinkSchemefulSite(origin) == top_level_site ||
                          top_level_site.IsOpaque())
                             ? mojom::blink::AncestorChainBit::kSameSite
                             : mojom::blink::AncestorChainBit::kCrossSite);
}

BlinkStorageKey::BlinkStorageKey(const StorageKey& storage_key)
    : BlinkStorageKey(
          SecurityOrigin::CreateFromUrlOrigin(storage_key.origin()),
          BlinkSchemefulSite(
              storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning()
                  .top_level_site()),
          storage_key.nonce() ? &storage_key.nonce().value() : nullptr,
          storage_key.nonce()
              ? mojom::blink::AncestorChainBit::kSameSite
              : storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning()
                    .ancestor_chain_bit()) {
  // We use `CopyWithForceEnabledThirdPartyStoragePartitioning` to preserve the
  // partitioned values. The constructor on the other side restores the default
  // values if `kThirdPartyStoragePartitioning` is disabled.
}

BlinkStorageKey::operator StorageKey() const {
  // We use `top_level_site_if_third_party_enabled_` and
  // `ancestor_chain_bit_if_third_party_enabled_` to preserve the partitioned
  // values. The constructor on the other side restores the default values if
  // `kThirdPartyStoragePartitioning` is disabled.
  return StorageKey::CreateWithOptionalNonce(
      origin_->ToUrlOrigin(),
      static_cast<net::SchemefulSite>(top_level_site_if_third_party_enabled_),
      base::OptionalToPtr(nonce_), ancestor_chain_bit_if_third_party_enabled_);
}

// static
// Keep consistent with StorageKey::FromWire().
bool BlinkStorageKey::FromWire(
    scoped_refptr<const SecurityOrigin> origin,
    const BlinkSchemefulSite& top_level_site,
    const BlinkSchemefulSite& top_level_site_if_third_party_enabled,
    const absl::optional<base::UnguessableToken>& nonce,
    mojom::blink::AncestorChainBit ancestor_chain_bit,
    mojom::blink::AncestorChainBit ancestor_chain_bit_if_third_party_enabled,
    BlinkStorageKey& out) {
  // If this key's "normal" members indicate a 3p key, then the
  // *_if_third_party_enabled counterparts must match them.
  if (top_level_site != BlinkSchemefulSite(origin) ||
      ancestor_chain_bit != mojom::blink::AncestorChainBit::kSameSite) {
    if (top_level_site != top_level_site_if_third_party_enabled) {
      return false;
    }
    if (ancestor_chain_bit != ancestor_chain_bit_if_third_party_enabled) {
      return false;
    }
  }

  // If top_level_site* is cross-site to origin, then ancestor_chain_bit* must
  // indicate that. We can't know for sure at this point if opaque
  // top_level_sites have cross-site ancestor chain bits or not, so skip them.
  if (top_level_site != BlinkSchemefulSite(origin) &&
      !top_level_site.IsOpaque()) {
    if (ancestor_chain_bit != mojom::blink::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  if (top_level_site_if_third_party_enabled != BlinkSchemefulSite(origin) &&
      !top_level_site_if_third_party_enabled.IsOpaque()) {
    if (ancestor_chain_bit_if_third_party_enabled !=
        mojom::blink::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  // If there is a nonce, all other values must indicate same-site to origin.
  if (nonce) {
    if (top_level_site != BlinkSchemefulSite(origin)) {
      return false;
    }

    if (top_level_site_if_third_party_enabled != BlinkSchemefulSite(origin)) {
      return false;
    }

    if (ancestor_chain_bit != mojom::blink::AncestorChainBit::kSameSite) {
      return false;
    }

    if (ancestor_chain_bit_if_third_party_enabled !=
        mojom::blink::AncestorChainBit::kSameSite) {
      return false;
    }
  }

  // This key is well formed.
  out.origin_ = origin;
  out.top_level_site_ = top_level_site;
  out.top_level_site_if_third_party_enabled_ =
      top_level_site_if_third_party_enabled;
  out.nonce_ = nonce;
  out.ancestor_chain_bit_ = ancestor_chain_bit;
  out.ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_if_third_party_enabled;

  return true;
}

String BlinkStorageKey::ToDebugString() const {
  return "{ origin: " + GetSecurityOrigin()->ToString() +
         ", top-level site: " + top_level_site_.Serialize() + ", nonce: " +
         (GetNonce().has_value() ? String::FromUTF8(GetNonce()->ToString())
                                 : "<null>") +
         ", ancestor chain bit: " +
         (GetAncestorChainBit() == mojom::blink::AncestorChainBit::kSameSite
              ? "Same-Site"
              : "Cross-Site") +
         " }";
}

bool BlinkStorageKey::ExactMatchForTesting(const BlinkStorageKey& other) const {
  return *this == other &&
         this->ancestor_chain_bit_if_third_party_enabled_ ==
             other.ancestor_chain_bit_if_third_party_enabled_ &&
         this->top_level_site_if_third_party_enabled_ ==
             other.top_level_site_if_third_party_enabled_;
}

bool operator==(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  DCHECK(lhs.GetSecurityOrigin());
  DCHECK(rhs.GetSecurityOrigin());

  return lhs.GetSecurityOrigin()->IsSameOriginWith(
             rhs.GetSecurityOrigin().get()) &&
         lhs.GetNonce() == rhs.GetNonce() &&
         lhs.GetTopLevelSite() == rhs.GetTopLevelSite() &&
         lhs.GetAncestorChainBit() == rhs.GetAncestorChainBit();
}

bool operator!=(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const BlinkStorageKey& key) {
  return ostream << key.ToDebugString();
}

}  // namespace blink
