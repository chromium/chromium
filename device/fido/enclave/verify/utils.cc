// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/strings/string_util.h"

namespace device::enclave {

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

std::string ConvertRawToPem(std::vector<uint8_t> public_key) {
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

}  // namespace device::enclave
