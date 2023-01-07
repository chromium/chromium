// Copyright 2021 The Chromium Authors
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

PublicKey::PublicKey(PublicKey&& other) = default;
PublicKey& PublicKey::operator=(PublicKey&& other) = default;

PublicKey::~PublicKey() = default;

PublicKeyset::PublicKeyset(std::vector<PublicKey> keys,
                           base::Time fetch_time,
                           base::Time expiry_time)
    : keys(std::move(keys)),
      fetch_time(std::move(fetch_time)),
      expiry_time(std::move(expiry_time)) {}

PublicKeyset::PublicKeyset(const PublicKeyset& other) = default;
PublicKeyset& PublicKeyset::operator=(const PublicKeyset& other) = default;

PublicKeyset::PublicKeyset(PublicKeyset&& other) = default;
PublicKeyset& PublicKeyset::operator=(PublicKeyset&& other) = default;

PublicKeyset::~PublicKeyset() = default;

std::ostream& operator<<(std::ostream& out, const PublicKey& public_key) {
  out << "id: " << public_key.id << ", key: 0x"
      << base::HexEncode(public_key.key);
  return out;
}

}  // namespace content