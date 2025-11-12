// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 64 * 1024) {
    return 0;
  }

  // SAFETY: fuzzer ensures `data` is valid for `size` bytes.
  auto fuzz = UNSAFE_BUFFERS(crdtp::span<uint8_t>(data, size));

  // We need to handle whatever the parser parses. So, we handle the parsed
  // stuff with another CBOR encoder, just because it's conveniently available.
  std::vector<uint8_t> encoded;
  crdtp::Status status;
  std::unique_ptr<crdtp::ParserHandler> encoder =
      crdtp::cbor::NewCBOREncoder(&encoded, &status);

  crdtp::cbor::ParseCBOR(fuzz, encoder.get());

  return 0;
}
