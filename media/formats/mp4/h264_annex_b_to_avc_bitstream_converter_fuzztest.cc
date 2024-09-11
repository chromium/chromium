// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <memory>

#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  std::vector<uint8_t> output(size);
  size_t size_out;
  bool config_changed;
  media::H264AnnexBToAvcBitstreamConverter converter;
  base::span<const uint8_t> input(data, data + size);

  auto status =
      converter.ConvertChunk(input, output, &config_changed, &size_out);

  auto& config = converter.GetCurrentConfig();

  std::vector<uint8_t> avc_config(size);
  config.Serialize(avc_config);

  return 0;
}
