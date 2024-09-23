// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vp8_decoder.h"

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/vp8_picture.h"

namespace {

class FakeVP8Accelerator : public media::VP8Decoder::VP8Accelerator {
 public:
  FakeVP8Accelerator() = default;

  FakeVP8Accelerator(const FakeVP8Accelerator&) = delete;
  FakeVP8Accelerator& operator=(const FakeVP8Accelerator&) = delete;

  ~FakeVP8Accelerator() override = default;

  // media::VP8Decoder::VP8Accelerator
  scoped_refptr<media::VP8Picture> CreateVP8Picture() override {
    return new media::VP8Picture();
  }
  bool SubmitDecode(
      scoped_refptr<media::VP8Picture> pic,
      const media::Vp8ReferenceFrameVector& reference_frames) override {
    return true;
  }
  bool OutputPicture(scoped_refptr<media::VP8Picture> pic) override {
    return true;
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    return 0;
  }

  media::VP8Decoder decoder(std::make_unique<FakeVP8Accelerator>());
  auto external_memory =
      std::make_unique<media::ExternalMemoryAdapterForTesting>(
          base::make_span(data, size));
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      media::DecoderBuffer::FromExternalMemory(std::move(external_memory));
  decoder.SetStream(1, *decoder_buffer);

  size_t retry_count = 0;
  while (true) {
    switch (decoder.Decode()) {
      case media::AcceleratedVideoDecoder::DecodeResult::kConfigChange:
        break;
      case media::AcceleratedVideoDecoder::DecodeResult::kTryAgain:
        if (++retry_count > 3) {
          return 0;
        }
        break;
      default:
        return 0;
    }
  }
}
