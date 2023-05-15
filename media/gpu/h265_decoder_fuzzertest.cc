// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/h265_decoder.h"

namespace {

class FakeH265Accelerator : public media::H265Decoder::H265Accelerator {
 public:
  FakeH265Accelerator() = default;

  FakeH265Accelerator(const FakeH265Accelerator&) = delete;
  FakeH265Accelerator& operator=(const FakeH265Accelerator&) = delete;

  ~FakeH265Accelerator() override = default;

  // media::H265Decoder::H265Accelerator
  scoped_refptr<media::H265Picture> CreateH265Picture() override {
    return new media::H265Picture();
  }

  Status SubmitFrameMetadata(
      const media::H265SPS* sps,
      const media::H265PPS* pps,
      const media::H265SliceHeader* slice_hdr,
      const media::H265Picture::Vector& ref_pic_list,
      const media::H265Picture::Vector& ref_pic_set_lt_curr,
      const media::H265Picture::Vector& ref_pic_set_st_curr_after,
      const media::H265Picture::Vector& ref_pic_set_st_curr_before,
      scoped_refptr<media::H265Picture> pic) override {
    return Status::kOk;
  }
  Status SubmitSlice(
      const media::H265SPS* sps,
      const media::H265PPS* pps,
      const media::H265SliceHeader* slice_hdr,
      const media::H265Picture::Vector& ref_pic_list0,
      const media::H265Picture::Vector& ref_pic_list1,
      const media::H265Picture::Vector& ref_pic_set_lt_curr,
      const media::H265Picture::Vector& ref_pic_set_st_curr_after,
      const media::H265Picture::Vector& ref_pic_set_st_curr_before,
      scoped_refptr<media::H265Picture> pic,
      const uint8_t* data,
      size_t size,
      const std::vector<media::SubsampleEntry>& subsamples) override {
    return Status::kOk;
  }
  Status SubmitDecode(scoped_refptr<media::H265Picture> pic) override {
    return Status::kOk;
  }
  bool OutputPicture(scoped_refptr<media::H265Picture> pic) override {
    return true;
  }
  void Reset() override {}
  Status SetStream(base::span<const uint8_t> stream,
                   const media::DecryptConfig* decrypt_config) override {
    return Status::kOk;
  }
  bool IsChromaSamplingSupported(media::VideoChromaSampling format) override {
    return format == media::VideoChromaSampling::k420;
  }
};

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  media::H265Decoder decoder(std::make_unique<FakeH265Accelerator>(),
                             media::HEVCPROFILE_MAIN);
  scoped_refptr<media::DecoderBuffer> decoder_buffer =
      media::DecoderBuffer::CopyFrom(data, size);
  decoder.SetStream(1, *decoder_buffer);

  // Decode should consume all the data unless it returns kConfigChange, and in
  // that case it needs to be called again.
  while (true) {
    if (decoder.Decode() != media::AcceleratedVideoDecoder::kConfigChange)
      break;
  }

  return 0;
}
