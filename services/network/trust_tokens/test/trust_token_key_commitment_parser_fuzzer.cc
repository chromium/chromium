// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include <memory>
#include <string_view>

#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view string_input(reinterpret_cast<const char*>(data), size);
  TrustTokenKeyCommitmentParser().Parse(string_input);
  TrustTokenKeyCommitmentParser().ParseMultipleIssuers(string_input);
  return 0;
}

}  // namespace network
