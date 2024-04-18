// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/utils.h"

#include <string>

#include "base/strings/string_util.h"

namespace device::enclave {

constexpr char kPemHeader[] = "-----BEGIN PUBLIC KEY-----";
constexpr char kPemFooter[] = "-----END PUBLIC KEY-----";

bool LooksLikePem(std::string_view maybe_pem) {
  std::string_view trimmed =
      base::TrimWhitespaceASCII(maybe_pem, base::TrimPositions::TRIM_ALL);
  return trimmed.starts_with(kPemHeader) && trimmed.ends_with(kPemFooter);
}

}  // namespace device::enclave
