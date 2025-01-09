// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/prf_input.h"

#include <array>

#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {
bool CBORToPRFValue(const cbor::Value& v, std::array<uint8_t, 32>* out) {
  if (!v.is_bytestring()) {
    return false;
  }
  return fido_parsing_utils::ExtractArray(v.GetBytestring(), 0, out);
}

// HashPRFValue hashes a PRF evaluation point with a fixed prefix in order to
// separate the set of points that a website can evaluate. See
// https://w3c.github.io/webauthn/#prf-extension.
std::array<uint8_t, 32> HashPRFValue(base::span<const uint8_t> value) {
  constexpr char kPrefix[] = "WebAuthn PRF";

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  // This deliberately includes the terminating NUL.
  SHA256_Update(&ctx, kPrefix, sizeof(kPrefix));
  SHA256_Update(&ctx, value.data(), value.size());

  std::array<uint8_t, 32> digest;
  SHA256_Final(digest.data(), &ctx);
  return digest;
}
}  // namespace

PRFInput::PRFInput() = default;
PRFInput::PRFInput(const PRFInput&) = default;
PRFInput::PRFInput(PRFInput&&) = default;
PRFInput& PRFInput::operator=(const PRFInput&) = default;
PRFInput::~PRFInput() = default;

// static
std::optional<PRFInput> PRFInput::FromCBOR(const cbor::Value& v) {
  if (!v.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& map = v.GetMap();
  const auto first_it = map.find(cbor::Value(kExtensionPRFFirst));
  if (first_it == map.end()) {
    return std::nullopt;
  }

  PRFInput ret;
  if (!CBORToPRFValue(first_it->second, &ret.salt1)) {
    return std::nullopt;
  }

  const auto second_it = map.find(cbor::Value(kExtensionPRFSecond));
  if (second_it != map.end()) {
    ret.salt2.emplace();
    if (!CBORToPRFValue(second_it->second, &ret.salt2.value())) {
      return std::nullopt;
    }
  }
  return ret;
}

cbor::Value::MapValue PRFInput::ToCBOR() const {
  cbor::Value::MapValue ret;
  ret.emplace(kExtensionPRFFirst,
              std::vector<uint8_t>(this->salt1.begin(), this->salt1.end()));
  if (this->salt2) {
    ret.emplace(kExtensionPRFSecond,
                std::vector<uint8_t>(this->salt2->begin(), this->salt2->end()));
  }
  return ret;
}

void PRFInput::HashInputsIntoSalts() {
  this->salt1 = HashPRFValue(this->input1);
  if (this->input2) {
    this->salt2 = HashPRFValue(*(this->input2));
  }
}

}  // namespace device
