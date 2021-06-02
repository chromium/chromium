// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include "url/gurl.h"

namespace blink {

// static
StorageKey StorageKey::Deserialize(const std::string& in) {
  return StorageKey(url::Origin::Create(GURL(in)));
}

// static
StorageKey StorageKey::CreateFromStringForTesting(const std::string& origin) {
  return Deserialize(origin);
}

std::string StorageKey::Serialize() const {
  DCHECK(!opaque());
  return origin_.GetURL().spec();
}

bool operator==(const StorageKey& lhs, const StorageKey& rhs) {
  return lhs.origin_ == rhs.origin_;
}

bool operator!=(const StorageKey& lhs, const StorageKey& rhs) {
  return !(lhs == rhs);
}

bool operator<(const StorageKey& lhs, const StorageKey& rhs) {
  return lhs.origin_ < rhs.origin_;
}

}  // namespace blink
