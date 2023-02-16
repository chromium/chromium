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
                      mojom::blink::AncestorChainBit::kCrossSite) {}

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
                          : (nonce || origin->IsOpaque())
                              ? mojom::blink::AncestorChainBit::kCrossSite
                              : mojom::blink::AncestorChainBit::kSameSite),
      ancestor_chain_bit_if_third_party_enabled_(ancestor_chain_bit) {
#if DCHECK_IS_ON()
  DCHECK(origin_);
  if (nonce) {
    // If we're setting a `nonce`, the `top_level_site` must be the same as
    // the `origin` and the `ancestor_chain_bit` must be kCrossSite. We don't
    // serialize those pieces of information so have to check to prevent
    // mistaken reliance on what is supposed to be an invariant.
    DCHECK(!nonce->is_empty());
    DCHECK(top_level_site == BlinkSchemefulSite(origin));
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kCrossSite);
  } else if (top_level_site.IsOpaque()) {
    // If we're setting an opaque `top_level_site`, the `ancestor_chain_bit`
    // must be kCrossSite. We don't serialize that information so have to check
    // to prevent mistaken reliance on what is supposed to be an invariant.
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kCrossSite);
  } else if (top_level_site != BlinkSchemefulSite(origin)) {
    // If `top_level_site` doesn't match `origin` then we must be making a
    // third-party StorageKey and `ancestor_chain_bit` must be kCrossSite.
    DCHECK_EQ(ancestor_chain_bit, mojom::blink::AncestorChainBit::kCrossSite);
  }
#endif
}

// static
BlinkStorageKey BlinkStorageKey::CreateFirstParty(
    scoped_refptr<const SecurityOrigin> origin) {
  return BlinkStorageKey(origin, BlinkSchemefulSite(origin), nullptr,
                         origin->IsOpaque()
                             ? mojom::blink::AncestorChainBit::kCrossSite
                             : mojom::blink::AncestorChainBit::kSameSite);
}

// static
// The AncestorChainBit is not applicable to StorageKeys with a non-empty
// nonce, so they are initialized to be kCrossSite.
BlinkStorageKey BlinkStorageKey::CreateWithNonce(
    scoped_refptr<const SecurityOrigin> origin,
    const base::UnguessableToken& nonce) {
  return BlinkStorageKey(origin, BlinkSchemefulSite(origin), &nonce,
                         mojom::blink::AncestorChainBit::kCrossSite);
}

// static
BlinkStorageKey BlinkStorageKey::Create(
    scoped_refptr<const SecurityOrigin> origin,
    const BlinkSchemefulSite& top_level_site,
    mojom::blink::AncestorChainBit ancestor_chain_bit) {
  return BlinkStorageKey(origin, top_level_site, nullptr, ancestor_chain_bit);
}

// static
BlinkStorageKey BlinkStorageKey::CreateFromStringForTesting(
    const WTF::String& origin) {
  return BlinkStorageKey::CreateFirstParty(
      SecurityOrigin::CreateFromString(origin));
}

// static
BlinkStorageKey BlinkStorageKey::CreateForTesting(
    scoped_refptr<const SecurityOrigin> origin,
    const BlinkSchemefulSite& top_level_site) {
  return BlinkStorageKey(
      origin, top_level_site, nullptr,
      BlinkSchemefulSite(origin) == top_level_site && !top_level_site.IsOpaque()
          ? mojom::blink::AncestorChainBit::kSameSite
          : mojom::blink::AncestorChainBit::kCrossSite);
}

BlinkStorageKey::BlinkStorageKey(const StorageKey& storage_key)
    : origin_(SecurityOrigin::CreateFromUrlOrigin(storage_key.origin())),
      top_level_site_(BlinkSchemefulSite(storage_key.top_level_site())),
      top_level_site_if_third_party_enabled_(BlinkSchemefulSite(
          storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning()
              .top_level_site())),
      nonce_(storage_key.nonce()),
      ancestor_chain_bit_(storage_key.ancestor_chain_bit()),
      ancestor_chain_bit_if_third_party_enabled_(
          storage_key.CopyWithForceEnabledThirdPartyStoragePartitioning()
              .ancestor_chain_bit()) {
  // Because we're converting from a StorageKey, we'll assume `storage_key` was
  // constructed correctly and take its members directly. We do this since the
  // incoming StorageKey's state could depend on RuntimeFeatureState's state and
  // we'd be unable to properly recreate it by just looking at the feature flag.
}

BlinkStorageKey::operator StorageKey() const {
  StorageKey out;

  // We're using FromWire because it lets us set each field individually (which
  // the constructors do not), this is necessary because we want the keys to
  // have the same state.
  bool status = StorageKey::FromWire(
      origin_->ToUrlOrigin(), static_cast<net::SchemefulSite>(top_level_site_),
      static_cast<net::SchemefulSite>(top_level_site_if_third_party_enabled_),
      nonce_, ancestor_chain_bit_, ancestor_chain_bit_if_third_party_enabled_,
      out);
  DCHECK(status);
  return out;
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
  if (!origin->IsOpaque() &&
      (top_level_site != BlinkSchemefulSite(origin) ||
       ancestor_chain_bit != mojom::blink::AncestorChainBit::kSameSite)) {
    if (top_level_site != top_level_site_if_third_party_enabled) {
      return false;
    }
    if (ancestor_chain_bit != ancestor_chain_bit_if_third_party_enabled) {
      return false;
    }
  }

  // If top_level_site* is cross-site to origin, then ancestor_chain_bit* must
  // indicate that. An opaque top_level_site* must have a cross-site
  // ancestor_chain_bit*.
  if (top_level_site != BlinkSchemefulSite(origin)) {
    if (ancestor_chain_bit != mojom::blink::AncestorChainBit::kCrossSite) {
      return false;
    }
  }

  if (top_level_site_if_third_party_enabled != BlinkSchemefulSite(origin)) {
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

    if (ancestor_chain_bit != mojom::blink::AncestorChainBit::kCrossSite) {
      return false;
    }

    if (ancestor_chain_bit_if_third_party_enabled !=
        mojom::blink::AncestorChainBit::kCrossSite) {
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

BlinkStorageKey BlinkStorageKey::WithOrigin(
    scoped_refptr<const SecurityOrigin> origin) const {
  BlinkSchemefulSite top_level_site = top_level_site_;
  BlinkSchemefulSite top_level_site_if_third_party_enabled =
      top_level_site_if_third_party_enabled_;
  mojom::blink::AncestorChainBit ancestor_chain_bit = ancestor_chain_bit_;
  mojom::blink::AncestorChainBit ancestor_chain_bit_if_third_party_enabled =
      ancestor_chain_bit_if_third_party_enabled_;

  if (nonce_) {
    // If the nonce is set we have to update the top level site to match origin
    // as that's an invariant.
    top_level_site = BlinkSchemefulSite(origin);
    top_level_site_if_third_party_enabled = top_level_site;
  } else if (!top_level_site_.IsOpaque()) {
    // If `top_level_site_` is opaque then so is
    // `top_level_site_if_third_party_enabled` and we don't need to explicitly
    // check it.

    // Only adjust the ancestor chain bit if it's currently kSameSite but the
    // new origin and top level site don't match. Note that the ACB might not
    // necessarily be kSameSite if the TLS and origin do match, so we won't
    // adjust the other way.

    if (ancestor_chain_bit == mojom::blink::AncestorChainBit::kSameSite &&
        BlinkSchemefulSite(origin) != top_level_site_) {
      ancestor_chain_bit = mojom::blink::AncestorChainBit::kCrossSite;
    }

    if (ancestor_chain_bit_if_third_party_enabled ==
            mojom::blink::AncestorChainBit::kSameSite &&
        BlinkSchemefulSite(origin) != top_level_site_if_third_party_enabled) {
      ancestor_chain_bit_if_third_party_enabled =
          mojom::blink::AncestorChainBit::kCrossSite;
    }
  }

  BlinkStorageKey out = *this;
  out.origin_ = origin;
  out.top_level_site_ = top_level_site;
  out.top_level_site_if_third_party_enabled_ =
      top_level_site_if_third_party_enabled;
  out.ancestor_chain_bit_ = ancestor_chain_bit;
  out.ancestor_chain_bit_if_third_party_enabled_ =
      ancestor_chain_bit_if_third_party_enabled;

  return out;
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
  DCHECK(lhs.origin_);
  DCHECK(rhs.origin_);

  return lhs.origin_->IsSameOriginWith(rhs.origin_.get()) &&
         lhs.nonce_ == rhs.nonce_ &&
         lhs.top_level_site_ == rhs.top_level_site_ &&
         lhs.ancestor_chain_bit_ == rhs.ancestor_chain_bit_;
}

bool operator!=(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const BlinkStorageKey& key) {
  return ostream << key.ToDebugString();
}

}  // namespace blink
