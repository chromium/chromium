// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/prf_input.h"

#include <array>

#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
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

std::vector<uint8_t> PRFInput::EvaluateHMAC(
    base::span<const uint8_t> hmac_key,
    const std::array<uint8_t, 32>& hmac_salt1,
    const std::optional<std::array<uint8_t, 32>>& hmac_salt2) {
  uint8_t hmac_result[SHA256_DIGEST_LENGTH];
  unsigned hmac_out_length;
  HMAC(EVP_sha256(), hmac_key.data(), hmac_key.size(), hmac_salt1.data(),
       hmac_salt1.size(), hmac_result, &hmac_out_length);
  CHECK_EQ(hmac_out_length, sizeof(hmac_result));

  std::vector<uint8_t> outputs;
  outputs.insert(outputs.end(), std::begin(hmac_result), std::end(hmac_result));

  if (hmac_salt2) {
    HMAC(EVP_sha256(), hmac_key.data(), hmac_key.size(), hmac_salt2->data(),
         hmac_salt2->size(), hmac_result, &hmac_out_length);
    CHECK_EQ(hmac_out_length, sizeof(hmac_result));
    outputs.insert(outputs.end(), std::begin(hmac_result),
                   std::end(hmac_result));
  }
  return outputs;
}

std::vector<uint8_t> PRFInput::EvaluateHMAC(
    base::span<const uint8_t> hmac_key) const {
  return EvaluateHMAC(hmac_key, this->salt1, this->salt2);
}

}  // namespace device
