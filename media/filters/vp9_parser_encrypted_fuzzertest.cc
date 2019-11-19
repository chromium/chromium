// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/numerics/safe_conversions.h"

#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"
#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }
};

Environment* env = new Environment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  std::string key_id = data_provider.ConsumeBytesAsString(4);
  std::string iv = data_provider.ConsumeBytesAsString(16);

  media::Vp9Parser vp9_parser(data_provider.ConsumeBool());

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

  const uint8_t* ivf_payload = nullptr;
  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_file_header;
  media::IvfFrameHeader ivf_frame_header;

  if (!ivf_parser.Initialize(data, size, &ivf_file_header))
    return 0;

  // Parse until the end of stream/unsupported stream/error in stream is found.
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &ivf_payload)) {
    media::Vp9FrameHeader vp9_frame_header;
    vp9_parser.SetStream(
        ivf_payload, ivf_frame_header.frame_size,
        media::DecryptConfig::CreateCencConfig(key_id, iv, subsamples));
    // TODO(kcwu): further fuzzing the case of Vp9Parser::kAwaitingRefresh.
    std::unique_ptr<media::DecryptConfig> null_config;
    gfx::Size allocate_size;
    while (vp9_parser.ParseNextFrame(&vp9_frame_header, &allocate_size,
                                     &null_config) == media::Vp9Parser::kOk) {
      // Repeat until all frames processed.
    }
  }

  return 0;
}
