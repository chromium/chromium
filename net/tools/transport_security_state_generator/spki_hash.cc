// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/spki_hash.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/sha2.h"

namespace net::transport_security_state {

SPKIHash::SPKIHash() = default;

SPKIHash::~SPKIHash() = default;

bool SPKIHash::FromString(std::string_view hash_string) {
  std::optional<std::string_view> base64_string = base::RemovePrefix(
      hash_string, "sha256/", base::CompareCase::INSENSITIVE_ASCII);
  if (!base64_string) {
    return false;
  }

  std::optional<std::vector<uint8_t>> decoded =
      base::Base64Decode(*base64_string);
  if (!decoded || decoded->size() != size()) {
    return false;
  }

  base::span(data_).copy_from(*decoded);
  return true;
}

void SPKIHash::CalculateFromBytes(base::span<const uint8_t> bytes) {
  SHA256(bytes.data(), bytes.size(), data_.data());
}

}  // namespace net::transport_security_state
