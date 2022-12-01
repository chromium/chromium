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
      nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt),
      ancestor_chain_bit_(StorageKey::IsThirdPartyStoragePartitioningEnabled()
                              ? ancestor_chain_bit
                              : mojom::blink::AncestorChainBit::kSameSite),
      ancestor_chain_bit_if_third_party_enabled_(ancestor_chain_bit) {
  DCHECK(origin_);
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
                         BlinkSchemefulSite(origin) == top_level_site
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
