// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp9_parser.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  FuzzedDataProvider data_provider(data.data(), data.size());
  std::string key_id = data_provider.ConsumeBytesAsString(4);
  std::string iv = data_provider.ConsumeBytesAsString(16);

  media::Vp9Parser vp9_parser;

  uint8_t subsample_count_upper_bound = 8;
  uint8_t subsamples_count =
      data_provider.ConsumeIntegral<uint8_t>() % subsample_count_upper_bound;
  std::vector<media::SubsampleEntry> subsamples;
  for (uint8_t entry = 0; entry < subsamples_count; ++entry) {
    if (data_provider.remaining_bytes() >= 2 * sizeof(uint32_t)) {
      uint32_t clear = data_provider.ConsumeIntegral<uint32_t>();
      uint32_t cipher = data_provider.ConsumeIntegral<uint32_t>();
      cipher &= 0xFFFFFFF0;  // make sure cipher is a multiple of 16.
      subsamples.push_back(media::SubsampleEntry(clear, cipher));
    }
  }

  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_file_header;
  media::IvfFrameHeader ivf_frame_header;

  if (!ivf_parser.Initialize(data, &ivf_file_header)) {
    return 0;
  }

  // Parse until the end of stream/unsupported stream/error in stream is found.
  for (auto ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header);
       !ivf_bytes.empty();
       ivf_bytes = ivf_parser.ParseNextFrame(&ivf_frame_header)) {
    media::Vp9FrameHeader vp9_frame_header;
    vp9_parser.SetStream(ivf_bytes, media::DecryptConfig::CreateCencConfig(
                                        key_id, iv, subsamples));
    std::unique_ptr<media::DecryptConfig> null_config;
    gfx::Size allocate_size;
    while (vp9_parser.ParseNextFrame(&vp9_frame_header, &allocate_size,
                                     &null_config) == media::Vp9Parser::kOk) {
      // Repeat until all frames processed.
    }
  }

  return 0;
}
