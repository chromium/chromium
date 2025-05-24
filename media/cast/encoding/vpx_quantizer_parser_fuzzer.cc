// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/vpx_quantizer_parser.h"

#include <stdint.h>

#include <tuple>

#include "base/containers/span.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::ignore = media::cast::ParseVpxHeaderQuantizer(
      // SAFETY: data is validated by the fuzzer runtime. Any AV crashes here
      // will result in a fuzzing bug, not a runtime issue.
      UNSAFE_BUFFERS(base::span<const uint8_t>(data, size)));
  return 0;
}
