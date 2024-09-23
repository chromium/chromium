// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/hash/sha1.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data_ptr, size_t size) {
  // SAFETY: libfuzzer gives a valid pointer and size pair.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, size));
  base::SHA1Digest sha1 = base::SHA1Hash(data);
  (void)sha1;
  return 0;
}
