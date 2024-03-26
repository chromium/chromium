// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/spki_hash.h"

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace net::transport_security_state {

SPKIHash::SPKIHash() = default;

SPKIHash::~SPKIHash() = default;

bool SPKIHash::FromString(std::string_view hash_string) {
  std::string_view base64_string;

  if (!base::StartsWith(hash_string, "sha256/",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }
  base64_string = hash_string.substr(7);

  std::string decoded;
  if (!base::Base64Decode(base64_string, &decoded)) {
    return false;
  }

  if (decoded.size() != size()) {
    return false;
  }

  memcpy(data_, decoded.data(), decoded.size());
  return true;
}

void SPKIHash::CalculateFromBytes(const uint8_t* input, size_t input_length) {
  SHA256(input, input_length, data_);
}

}  // namespace net::transport_security_state
