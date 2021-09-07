// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/public_key.h"

#include <utility>

#include "base/strings/string_number_conversions.h"

namespace content {

PublicKey::PublicKey(std::string id, std::vector<uint8_t> key)
    : id(std::move(id)), key(std::move(key)) {}

PublicKey::PublicKey(const PublicKey& other) = default;
PublicKey& PublicKey::operator=(const PublicKey& other) = default;

PublicKey::~PublicKey() = default;

PublicKeysForOrigin::PublicKeysForOrigin() = default;

PublicKeysForOrigin::PublicKeysForOrigin(url::Origin origin,
                                         std::vector<PublicKey> keys)
    : origin(std::move(origin)), keys(std::move(keys)) {}

PublicKeysForOrigin::PublicKeysForOrigin(const PublicKeysForOrigin& other) =
    default;

PublicKeysForOrigin& PublicKeysForOrigin::operator=(
    const PublicKeysForOrigin& other) = default;

PublicKeysForOrigin::~PublicKeysForOrigin() = default;

std::ostream& operator<<(std::ostream& out, const PublicKey& public_key) {
  out << "id: " << public_key.id << ", key: 0x"
      << base::HexEncode(public_key.key);
  return out;
}

}  // namespace content