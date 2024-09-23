// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "net/http/http_chunked_decoder.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* data_ptr = reinterpret_cast<const char*>(data);
  net::HttpChunkedDecoder decoder;

  // Feed data to decoder.FilterBuf() by blocks of "random" size.
  size_t block_size = 0;
  for (size_t offset = 0; offset < size; offset += block_size) {
    // Since there is no input for block_size values, but it should be strictly
    // determined, let's calculate these values using a couple of data bytes.
    uint8_t temp_block_size = data[offset] ^ data[size - offset - 1];

    // Let temp_block_size be in range from 0 to 0x3F (0b00111111).
    temp_block_size &= 0x3F;

    // XOR with previous block size to get different values for different data.
    block_size ^= temp_block_size;

    // Prevent infinite loop if block_size == 0.
    block_size = std::max(block_size, static_cast<size_t>(1));

    // Prevent out-of-bounds access.
    block_size = std::min(block_size, size - offset);

    // Create new buffer with current block of data and feed it to the decoder.
    std::vector<char> buffer(data_ptr + offset, data_ptr + offset + block_size);
    int result = decoder.FilterBuf(buffer.data(), buffer.size());
    if (result < 0)
      return 0;
  }

  return 0;
}
