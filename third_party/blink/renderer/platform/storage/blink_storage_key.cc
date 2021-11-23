// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

#include <ostream>

#include "base/stl_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"

namespace blink {

BlinkStorageKey::BlinkStorageKey()
    : BlinkStorageKey(SecurityOrigin::CreateUniqueOpaque(),
                      BlinkSchemefulSite(),
                      nullptr) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin)
    : BlinkStorageKey(std::move(origin), nullptr) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                                 const BlinkSchemefulSite& top_level_site)
    : BlinkStorageKey(std::move(origin), top_level_site, nullptr) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                                 const base::UnguessableToken* nonce)
    : BlinkStorageKey(origin, BlinkSchemefulSite(origin), nonce) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                                 const BlinkSchemefulSite& top_level_site,
                                 const base::UnguessableToken* nonce)
    : origin_(origin),
      top_level_site_(
          blink::StorageKey::IsThirdPartyStoragePartitioningEnabled()
              ? top_level_site
              : BlinkSchemefulSite(origin)),
      nonce_(nonce ? absl::make_optional(*nonce) : absl::nullopt) {
  DCHECK(origin_);
}

// static
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

BlinkStorageKey::BlinkStorageKey(const StorageKey& storage_key)
    : BlinkStorageKey(
          SecurityOrigin::CreateFromUrlOrigin(storage_key.origin()),
          BlinkSchemefulSite(storage_key.top_level_site()),
          storage_key.nonce() ? &storage_key.nonce().value() : nullptr) {}

BlinkStorageKey::operator StorageKey() const {
  return StorageKey::CreateWithOptionalNonce(
      origin_->ToUrlOrigin(), static_cast<net::SchemefulSite>(top_level_site_),
      base::OptionalOrNullptr(nonce_));
}

String BlinkStorageKey::ToDebugString() const {
  return "{ origin: " + GetSecurityOrigin()->ToString() +
         ", top-level site: " + top_level_site_.Serialize() + ", nonce: " +
         (GetNonce().has_value() ? String::FromUTF8(GetNonce()->ToString())
                                 : "<null>") +
         " }";
}

bool operator==(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  DCHECK(lhs.GetSecurityOrigin());
  DCHECK(rhs.GetSecurityOrigin());

  return lhs.GetSecurityOrigin()->IsSameOriginWith(
             rhs.GetSecurityOrigin().get()) &&
         lhs.GetNonce() == rhs.GetNonce() &&
         lhs.GetTopLevelSite() == rhs.GetTopLevelSite();
}

bool operator!=(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const BlinkStorageKey& key) {
  return ostream << key.ToDebugString();
}

}  // namespace blink
