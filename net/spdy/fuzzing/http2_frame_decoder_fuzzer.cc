// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <list>
#include <vector>

#include "net/third_party/quiche/src/http2/decoder/http2_frame_decoder.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  http2::Http2FrameDecoder decoder;

  // Store all chunks in a function scope list, as the API requires the caller
  // to make sure the fragment chunks data is accessible during the whole
  // decoding process. |http2::DecodeBuffer| does not copy the data, it is just
  // a wrapper for the chunk provided in its constructor.
  std::list<std::vector<char>> all_chunks;
  while (fuzzed_data_provider.remaining_bytes() > 0) {
    size_t chunk_size = fuzzed_data_provider.ConsumeIntegralInRange(1, 32);
    all_chunks.emplace_back(
        fuzzed_data_provider.ConsumeBytes<char>(chunk_size));
    const auto& chunk = all_chunks.back();

    // http2::DecodeBuffer constructor does not accept nullptr buffer.
    if (chunk.data() == nullptr)
      continue;

    http2::DecodeBuffer frame_data(chunk.data(), chunk.size());
    decoder.DecodeFrame(&frame_data);
  }
  return 0;
}
