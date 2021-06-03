// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include "url/gurl.h"

namespace blink {

// static
absl::optional<StorageKey> StorageKey::Deserialize(base::StringPiece in) {
  StorageKey result(url::Origin::Create(GURL(in)));
  return result.opaque() ? absl::nullopt
                         : absl::make_optional(std::move(result));
}

// static
StorageKey StorageKey::CreateFromStringForTesting(const std::string& origin) {
  absl::optional<StorageKey> result = Deserialize(origin);
  return result.value_or(StorageKey());
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
