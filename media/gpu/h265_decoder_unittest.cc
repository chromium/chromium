// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "media/base/test_data_util.h"
#include "media/gpu/h265_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Args;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Mock;
using ::testing::Return;
using ::testing::WithArg;

namespace media {
namespace {
constexpr char kFrame0[] = "bear-frame0.hevc";
}  // namespace

class MockH265Accelerator : public H265Decoder::H265Accelerator {
 public:
  MockH265Accelerator() = default;

  MOCK_METHOD2(SetStream,
               Status(base::span<const uint8_t> stream,
                      const DecryptConfig* decrypt_config));

  void Reset() override {}
};

// Test H265Decoder by feeding different h265 frame sequences and make sure it
// behaves as expected.
class H265DecoderTest : public ::testing::Test {
 public:
  H265DecoderTest() = default;

  void SetUp() override;

  // Sets the bitstreams to be decoded, frame by frame. The content of each
  // file is the encoded bitstream of a single video frame.
  void SetInputFrameFiles(const std::vector<std::string>& frame_files);

  // Keeps decoding the input bitstream set at |SetInputFrameFiles| until the
  // decoder has consumed all bitstreams or returned from
  // |H265Decoder::Decode|. Returns the same result as |H265Decoder::Decode|.
  AcceleratedVideoDecoder::DecodeResult Decode();

 protected:
  std::unique_ptr<H265Decoder> decoder_;
  MockH265Accelerator* accelerator_;

 private:
  base::queue<std::string> input_frame_files_;
  std::string bitstream_;
  scoped_refptr<DecoderBuffer> decoder_buffer_;
};

void H265DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH265Accelerator>();
  accelerator_ = mock_accelerator.get();
  decoder_.reset(new H265Decoder(std::move(mock_accelerator),
                                 VIDEO_CODEC_PROFILE_UNKNOWN));

  // Sets default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, SetStream(_, _))
      .WillByDefault(
          Return(H265Decoder::H265Accelerator::Status::kNotSupported));
}

void H265DecoderTest::SetInputFrameFiles(
    const std::vector<std::string>& input_frame_files) {
  for (auto f : input_frame_files)
    input_frame_files_.push(f);
}

AcceleratedVideoDecoder::DecodeResult H265DecoderTest::Decode() {
  while (true) {
    auto result = decoder_->Decode();
    int32_t bitstream_id = 0;
    if (result != AcceleratedVideoDecoder::kRanOutOfStreamData ||
        input_frame_files_.empty())
      return result;
    auto input_file = GetTestDataFilePath(input_frame_files_.front());
    input_frame_files_.pop();
    CHECK(base::ReadFileToString(input_file, &bitstream_));
    decoder_buffer_ = DecoderBuffer::CopyFrom(
        reinterpret_cast<const uint8_t*>(bitstream_.data()), bitstream_.size());
    EXPECT_NE(decoder_buffer_.get(), nullptr);
    decoder_->SetStream(bitstream_id++, *decoder_buffer_);
  }
}

// Test Cases

TEST_F(H265DecoderTest, DecodeSingleFrame) {
  SetInputFrameFiles({kFrame0});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  // TODO(jkardatzke): Test the rest of decoding.
}

}  // namespace media
