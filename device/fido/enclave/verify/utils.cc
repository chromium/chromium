// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device::enclave {

namespace {

bssl::UniquePtr<EVP_PKEY> ParseKey(base::span<const uint8_t> public_key) {
  CBS cbs;
  CBS_init(&cbs, public_key.data(), public_key.size());
  bssl::UniquePtr<EVP_PKEY> parsed_key(EVP_parse_public_key(&cbs));
  if (!parsed_key || CBS_len(&cbs) != 0 ||
      EVP_PKEY_id(parsed_key.get()) != EVP_PKEY_EC) {
    return nullptr;
  }
  return parsed_key;
}

bssl::UniquePtr<EC_KEY> GetECKey(base::span<const uint8_t> public_key) {
  bssl::UniquePtr<EVP_PKEY> parsed_key = ParseKey(public_key);
  bssl::UniquePtr<EC_KEY> ec_key(EVP_PKEY_get1_EC_KEY(parsed_key.get()));
  if (!ec_key || EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key.get())) !=
                     NID_X9_62_prime256v1) {
    return nullptr;
  }
  return ec_key;
}

}  // namespace

constexpr std::string_view kPemHeader = "-----BEGIN PUBLIC KEY-----";
constexpr std::string_view kPemFooter = "-----END PUBLIC KEY-----";

bool LooksLikePem(std::string_view maybe_pem) {
  std::string_view trimmed =
      base::TrimWhitespaceASCII(maybe_pem, base::TrimPositions::TRIM_ALL);
  return trimmed.starts_with(kPemHeader) && trimmed.ends_with(kPemFooter);
}

base::expected<std::vector<uint8_t>, std::string> ConvertPemToRaw(
    std::string_view public_key_pem) {
  std::string trimmed(
      base::TrimWhitespaceASCII(public_key_pem, base::TrimPositions::TRIM_ALL));
  if (!LooksLikePem(trimmed)) {
    return base::unexpected("Could not find the expected header or footer.");
  }
  base::ReplaceSubstringsAfterOffset(&trimmed, 0, kPemHeader, "");
  base::ReplaceSubstringsAfterOffset(&trimmed, 0, kPemFooter, "");
  base::ReplaceSubstringsAfterOffset(&trimmed, 0, "\n", "");
  std::string tempResult;
  if (!base::Base64Decode(trimmed, &tempResult)) {
    return base::unexpected("Base64 decoding failed");
  }
  std::vector<uint8_t> resultVector(tempResult.begin(), tempResult.end());
  return resultVector;
}

std::string ConvertRawToPem(base::span<const uint8_t> public_key) {
  std::string before(public_key.begin(), public_key.end());
  std::string encoded = base::Base64Encode(before);
  std::vector<char> tempResult(kPemHeader.begin(), kPemHeader.end());
  for (unsigned long i = 0; i < encoded.length(); i++) {
    if (i % 64 == 0) {
      tempResult.push_back('\n');
    }
    tempResult.push_back(encoded[i]);
  }
  tempResult.push_back('\n');
  std::string result(tempResult.begin(), tempResult.end());
  result = result + std::string(kPemFooter) + "\n";
  return result;
}

base::expected<void, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    VerifySignatureRaw(base::span<const uint8_t> signature,
                       base::span<const uint8_t> contents,
                       base::span<const uint8_t> public_key) {
  bssl::UniquePtr<EC_KEY> ec_key(GetECKey(public_key));
  if (!ec_key) {
    return base::unexpected("Could not parse public_key");
  }
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(contents.data()),
         contents.size(), digest);
  if (!ECDSA_verify(/*type=*/0, digest, sizeof(digest), signature.data(),
                    signature.size(), ec_key.get())) {
    return base::unexpected("Could not verify the signature");
  }
  return base::expected<void, std::string>();
}

base::expected<bool, std::string> EqualKeys(
    base::span<const uint8_t> public_key_a,
    base::span<const uint8_t> public_key_b) {
  bssl::UniquePtr<EVP_PKEY> parsed_key_a = ParseKey(public_key_a);
  bssl::UniquePtr<EVP_PKEY> parsed_key_b = ParseKey(public_key_b);

  if (!parsed_key_a || !parsed_key_b) {
    return base::unexpected("Could not parse the passed key(s)");
  }
  int res = EVP_PKEY_cmp(parsed_key_a.get(), parsed_key_b.get());
  if (res < 0) {
    return base::unexpected("Error comparing keys");
  }
  return res;
}

}  // namespace device::enclave
