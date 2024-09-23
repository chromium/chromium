// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vp9_decoder.h"

#include <stddef.h>

#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/vp9_picture.h"

namespace {

class FakeVP9Accelerator : public media::VP9Decoder::VP9Accelerator {
 public:
  FakeVP9Accelerator() = default;

  FakeVP9Accelerator(const FakeVP9Accelerator&) = delete;
  FakeVP9Accelerator& operator=(const FakeVP9Accelerator&) = delete;

  ~FakeVP9Accelerator() override = default;

  // media::VP9Decoder::VP9Accelerator
  scoped_refptr<media::VP9Picture> CreateVP9Picture() override {
    return new media::VP9Picture();
  }
  Status SubmitDecode(
      scoped_refptr<media::VP9Picture> pic,
      const media::Vp9SegmentationParams& segm_params,
      const media::Vp9LoopFilterParams& lf_params,
      const media::Vp9ReferenceFrameVector& reference_frames) override {
    return Status::kOk;
  }
  bool OutputPicture(scoped_refptr<media::VP9Picture> pic) override {
    return true;
  }
  bool NeedsCompressedHeaderParsed() const override { return true; }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    return 0;
  }

  media::VP9Decoder decoder(std::make_unique<FakeVP9Accelerator>(),
                            media::VP9PROFILE_PROFILE0);
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
