// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_chunked_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  net::HttpChunkedDecoder decoder;

  // Feed data to decoder.FilterBuf() by blocks of "random" size.
  size_t block_size = 0;
  for (size_t offset = 0; offset < data.size(); offset += block_size) {
    // Since there is no input for block_size values, but it should be strictly
    // determined, let's calculate these values using a couple of data bytes.
    uint8_t temp_block_size = data[offset] ^ data[data.size() - offset - 1];

    // Let temp_block_size be in range from 0 to 0x3F (0b00111111).
    temp_block_size &= 0x3F;

    // XOR with previous block size to get different values for different data.
    block_size ^= temp_block_size;

    // Prevent infinite loop if block_size == 0.
    block_size = std::max(block_size, static_cast<size_t>(1));

    // Prevent out-of-bounds access.
    block_size = std::min(block_size, data.size() - offset);

    // Create new buffer with current block of data and feed it to the decoder.
    std::vector<uint8_t> buffer =
        base::ToVector(data.subspan(offset, block_size));
    int result = decoder.FilterBuf(buffer);
    if (result < 0)
      return 0;
  }

  return 0;
}
