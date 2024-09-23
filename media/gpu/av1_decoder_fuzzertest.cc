// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <tuple>

#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/av1_picture.h"

namespace {

class FakeAV1Accelerator : public media::AV1Decoder::AV1Accelerator {
 public:
  FakeAV1Accelerator() = default;
  ~FakeAV1Accelerator() override = default;
  FakeAV1Accelerator(const FakeAV1Accelerator&) = delete;
  FakeAV1Accelerator& operator=(const FakeAV1Accelerator&) = delete;

  // media::AV1Decoder::AV1Accelerator implementation.
  scoped_refptr<media::AV1Picture> CreateAV1Picture(bool apply_grain) override {
    return base::MakeRefCounted<media::AV1Picture>();
  }
  Status SubmitDecode(const media::AV1Picture& pic,
                      const libgav1::ObuSequenceHeader& sequence_header,
                      const media::AV1ReferenceFrameVector& ref_frames,
                      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
                      base::span<const uint8_t> data) override {
    return Status::kOk;
  }
  bool OutputPicture(const media::AV1Picture& pic) override { return true; }
};

}  // namespace

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  media::AV1Decoder decoder(std::make_unique<FakeAV1Accelerator>(),
                            media::AV1PROFILE_PROFILE_MAIN);

  // Split the input in two: we'll create a DecoderBuffer from each half. This
  // allows us to Decode(), Reset(), and Decode() again for more coverage.
  for (int i = 0; i < 2; ++i) {
    size_t size_to_consume = i == 0 ? size / 2 : (size - size / 2);
    std::vector<uint8_t> decoder_buffer_data =
        fuzzed_data_provider.ConsumeBytes<uint8_t>(size_to_consume);
    if (decoder_buffer_data.empty())
      continue;
    // The *|decoder_buffer| can be destroyed at the end of each iteration
    // because Reset() is expected to ensure that the current DecoderBuffer
    // won't be needed after that.
    scoped_refptr<media::DecoderBuffer> decoder_buffer =
        media::DecoderBuffer::CopyFrom(decoder_buffer_data);
    decoder.SetStream(i, *decoder_buffer);

    // Decode should consume all the data unless it returns kConfigChange, and
    // in that case it needs to be called again.
    while (true) {
      if (decoder.Decode() != media::AcceleratedVideoDecoder::kConfigChange)
        break;
    }
    decoder.Reset();
  }
  std::ignore = decoder.Flush();

  return 0;
}
