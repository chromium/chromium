// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h264_decoder.h"

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"

namespace {

class FakeH264Accelerator : public media::H264Decoder::H264Accelerator {
 public:
  FakeH264Accelerator() = default;

  FakeH264Accelerator(const FakeH264Accelerator&) = delete;
  FakeH264Accelerator& operator=(const FakeH264Accelerator&) = delete;

  ~FakeH264Accelerator() override = default;

  // media::H264Decoder::H264Accelerator
  scoped_refptr<media::H264Picture> CreateH264Picture() override {
    return new media::H264Picture();
  }

  Status SubmitFrameMetadata(const media::H264SPS* sps,
                             const media::H264PPS* pps,
                             const media::H264DPB& dpb,
                             const media::H264Picture::Vector& ref_pic_listp0,
                             const media::H264Picture::Vector& ref_pic_listb0,
                             const media::H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<media::H264Picture> pic) override {
    return Status::kOk;
  }
  Status SubmitSlice(
      const media::H264PPS* pps,
      const media::H264SliceHeader* slice_hdr,
      const media::H264Picture::Vector& ref_pic_list0,
      const media::H264Picture::Vector& ref_pic_list1,
      scoped_refptr<media::H264Picture> pic,
      const uint8_t* data,
      size_t size,
      const std::vector<media::SubsampleEntry>& subsamples) override {
    return Status::kOk;
  }
  Status SubmitDecode(scoped_refptr<media::H264Picture> pic) override {
    return Status::kOk;
  }
  bool OutputPicture(scoped_refptr<media::H264Picture> pic) override {
    return true;
  }
  void Reset() override {}
  Status SetStream(base::span<const uint8_t> stream,
                   const media::DecryptConfig* decrypt_config) override {
    return Status::kOk;
  }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    return 0;
  }

  media::H264Decoder decoder(std::make_unique<FakeH264Accelerator>(),
                             media::H264PROFILE_MAIN);
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
