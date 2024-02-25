// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/snappy/src/snappy.h"

#include <cstdlib>

#define FUZZING_ASSERT(condition)                                      \
  if (!(condition)) {                                                  \
    fprintf(stderr, "%s\n", "Fuzzing Assertion Failure: " #condition); \
    abort();                                                           \
  }

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const char* uncompressed = reinterpret_cast<const char*>(data);
  std::string compressed;
  snappy::Compress(uncompressed, size, &compressed);
  FUZZING_ASSERT(
      snappy::IsValidCompressedBuffer(compressed.data(), compressed.size()));

  std::string uncompressed_after_compress;
  FUZZING_ASSERT(snappy::Uncompress(compressed.data(), compressed.size(),
                                    &uncompressed_after_compress));
  FUZZING_ASSERT(uncompressed_after_compress ==
                 std::string(uncompressed, size));

  return 0;
}
