// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"

#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace blink {

BlinkStorageKey::BlinkStorageKey()
    : BlinkStorageKey(SecurityOrigin::CreateUniqueOpaque(), nullptr) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin)
    : BlinkStorageKey(std::move(origin), nullptr) {}

BlinkStorageKey::BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                                 const base::UnguessableToken* nonce)
    : origin_(std::move(origin)),
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

BlinkStorageKey::BlinkStorageKey(const StorageKey& storage_key)
    : BlinkStorageKey(
          SecurityOrigin::CreateFromUrlOrigin(storage_key.origin()),
          storage_key.nonce() ? &storage_key.nonce().value() : nullptr) {}

BlinkStorageKey::operator StorageKey() const {
  return nonce_.has_value() ? StorageKey::CreateWithNonce(
                                  origin_->ToUrlOrigin(), nonce_.value())
                            : StorageKey(origin_->ToUrlOrigin());
}

String BlinkStorageKey::ToDebugString() const {
  return "{ origin: " + GetSecurityOrigin()->ToString() + ", nonce: " +
         (GetNonce().has_value() ? String::FromUTF8(GetNonce()->ToString())
                                 : "<null>") +
         " }";
}

bool operator==(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  DCHECK(lhs.GetSecurityOrigin());
  DCHECK(rhs.GetSecurityOrigin());
  return lhs.GetSecurityOrigin()->IsSameOriginWith(
             rhs.GetSecurityOrigin().get()) &&
         lhs.GetNonce() == rhs.GetNonce();
}

bool operator!=(const BlinkStorageKey& lhs, const BlinkStorageKey& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& ostream, const BlinkStorageKey& key) {
  return ostream << key.ToDebugString();
}

}  // namespace blink
