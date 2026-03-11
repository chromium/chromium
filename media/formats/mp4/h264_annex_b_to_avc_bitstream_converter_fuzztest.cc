// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> input) {
  if (input.empty()) {
    return 0;
  }

  for (bool add_parameter_sets_in_bitstream : {false, true}) {
    std::vector<uint8_t> output(input.size());
    size_t size_out;
    bool config_changed;
    media::H264AnnexBToAvcBitstreamConverter converter(
        add_parameter_sets_in_bitstream);

    auto status =
        converter.ConvertChunk(input, output, &config_changed, &size_out);

    auto& config = converter.GetCurrentConfig();

    std::vector<uint8_t> avc_config(input.size());
    config.Serialize(avc_config);
  }

  return 0;
}
