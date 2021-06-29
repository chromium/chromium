// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <cctype>
#include <ostream>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace blink {

// static
absl::optional<StorageKey> StorageKey::Deserialize(base::StringPiece in) {
  StorageKey result(url::Origin::Create(GURL(in)));
  return result.origin_.opaque() ? absl::nullopt
                                 : absl::make_optional(std::move(result));
}

// static
StorageKey StorageKey::CreateFromStringForTesting(const std::string& origin) {
  absl::optional<StorageKey> result = Deserialize(origin);
  return result.value_or(StorageKey());
}

std::string StorageKey::Serialize() const {
  DCHECK(!origin_.opaque());
  return origin_.GetURL().spec();
}

std::string StorageKey::SerializeForLocalStorage() const {
  DCHECK(!origin_.opaque());
  return origin_.Serialize();
}

std::string StorageKey::GetDebugString() const {
  return base::StrCat({"{ origin: ", origin_.GetDebugString(), " }"});
}

std::string StorageKey::GetMemoryDumpString(size_t max_length) const {
  std::string memory_dump_str = origin_.Serialize().substr(0, max_length);
  base::ranges::replace_if(
      memory_dump_str.begin(), memory_dump_str.end(),
      [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); }, '_');
  return memory_dump_str;
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

std::ostream& operator<<(std::ostream& ostream, const StorageKey& sk) {
  return ostream << sk.GetDebugString();
}

}  // namespace blink
