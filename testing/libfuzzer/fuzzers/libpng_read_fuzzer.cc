// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#define PNG_INTERNAL
#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/libpng/png.h"

void* limited_malloc(png_structp, png_alloc_size_t size) {
  // libpng may allocate large amounts of memory that the fuzzer reports as
  // an error. In order to silence these errors, make libpng fail when trying
  // to allocate a large amount.
  // This number is chosen to match the default png_user_chunk_malloc_max.
  if (size > 8000000)
    return nullptr;

  return malloc(size);
}

void default_free(png_structp, png_voidp ptr) {
  return free(ptr);
}

static const int kPngHeaderSize = 8;

// Entry point for LibFuzzer.
// Roughly follows the libpng book example:
// http://www.libpng.org/pub/png/book/chapter13.html
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  if (data.size() < kPngHeaderSize) {
    return 0;
  }

  if (png_sig_cmp(data.data(), 0, kPngHeaderSize)) {
    // not a PNG.
    return 0;
  }

  png_structp png_ptr = png_create_read_struct
    (PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  assert(png_ptr);

#ifdef MEMORY_SANITIZER
  // To avoid OOM with MSan (crbug.com/648073). These values are recommended as
  // safe settings by https://github.com/glennrp/libpng/blob/libpng16/pngusr.dfa
  png_set_user_limits(png_ptr, 65535, 65535);
#endif

  // Not all potential OOM are due to images with large widths and heights.
  // Use a custom allocator that fails for large allocations.
  png_set_mem_fn(png_ptr, nullptr, limited_malloc, default_free);

  png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);

  png_infop info_ptr = png_create_info_struct(png_ptr);
  assert(info_ptr);

  absl::Cleanup struct_deleter = [&png_ptr, &info_ptr] {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
  };

  if (setjmp(png_jmpbuf(png_ptr))) {
    return 0;
  }

  png_set_progressive_read_fn(png_ptr, nullptr, nullptr, nullptr, nullptr);
  png_process_data(png_ptr, info_ptr, const_cast<uint8_t*>(data.data()),
                   data.size());

  return 0;
}
