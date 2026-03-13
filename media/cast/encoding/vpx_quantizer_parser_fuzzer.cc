// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/vpx_quantizer_parser.h"

#include <stdint.h>

#include <tuple>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  std::ignore = media::cast::ParseVpxHeaderQuantizer(data);
  return 0;
}
