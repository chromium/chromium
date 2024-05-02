// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp8_decoder.h"

#include <stdint.h>

#include <string>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace media {
namespace {

const std::string kNullFrame = "";
const std::string kIFrame = "vp8-I-frame-320x240";
const std::string kPFrame = "vp8-P-frame-320x240";
const std::string kCorruptFrame = "vp8-corrupt-I-frame";
constexpr gfx::Size kVideoSize(320, 240);
constexpr size_t kRequiredNumOfPictures = 8u;

class MockVP8Accelerator : public VP8Decoder::VP8Accelerator {
 public:
  MockVP8Accelerator() = default;

  MOCK_METHOD0(CreateVP8Picture, scoped_refptr<VP8Picture>());
  MOCK_METHOD2(SubmitDecode,
               bool(scoped_refptr<VP8Picture> pic,
                    const Vp8ReferenceFrameVector& reference_frames));
  MOCK_METHOD1(OutputPicture, bool(scoped_refptr<VP8Picture> pic));
};

// Test VP8Decoder by feeding different VP8 frame sequences and making sure it
// behaves as expected.
class VP8DecoderTest : public ::testing::Test {
 public:
  VP8DecoderTest() = default;

  void SetUp() override;

  AcceleratedVideoDecoder::DecodeResult Decode(std::string input_frame_file);

 protected:
  void SkipFrame() { bitstream_id_++; }
  void CompleteToDecodeFirstIFrame();

  std::unique_ptr<VP8Decoder> decoder_;
  raw_ptr<MockVP8Accelerator> accelerator_ = nullptr;

 private:
  void DecodeFirstIFrame();

  int32_t bitstream_id_ = 0;
};

void VP8DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockVP8Accelerator>();
  accelerator_ = mock_accelerator.get();
  decoder_.reset(new VP8Decoder(std::move(mock_accelerator)));

  // Sets default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, CreateVP8Picture())
      .WillByDefault(Return(new VP8Picture()));
  ON_CALL(*accelerator_, SubmitDecode(_, _)).WillByDefault(Return(true));
  ON_CALL(*accelerator_, OutputPicture(_)).WillByDefault(Return(true));

  DecodeFirstIFrame();
}

void VP8DecoderTest::DecodeFirstIFrame() {
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kNullFrame));
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(kIFrame));
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(kVideoSize, decoder_->GetPicSize());
  EXPECT_LE(kRequiredNumOfPictures, decoder_->GetRequiredNumOfPictures());
}

// DecodeFirstIFrame() allocates new surfaces so VP8Decoder::Decode() must be
// called again to complete to decode the first frame.
void VP8DecoderTest::CompleteToDecodeFirstIFrame() {
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateVP8Picture());
    EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
    EXPECT_CALL(*accelerator_, OutputPicture(_));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kNullFrame));
}

AcceleratedVideoDecoder::DecodeResult VP8DecoderTest::Decode(
    std::string input_frame_file) {
  std::string bitstream;
  scoped_refptr<DecoderBuffer> buffer;
  if (!input_frame_file.empty()) {
    auto input_file = GetTestDataFilePath(input_frame_file);
    EXPECT_TRUE(base::ReadFileToString(input_file, &bitstream));
    buffer = DecoderBuffer::CopyFrom(base::as_byte_span(bitstream));
    EXPECT_NE(buffer.get(), nullptr);
    decoder_->SetStream(bitstream_id_++, *buffer);
  }

  AcceleratedVideoDecoder::DecodeResult result = decoder_->Decode();
  if (input_frame_file.empty())
    return result;
  // Since |buffer| is destroyed in this function, Decode() must consume the
  // buffer by this Decode(). That happens if the return value is
  // kRanOutOfStreamData, kConfigChange , or kDecodeError (on failure).
  EXPECT_TRUE(result ==
                  AcceleratedVideoDecoder::DecodeResult::kRanOutOfStreamData ||
              result == AcceleratedVideoDecoder::DecodeResult::kConfigChange ||
              result == AcceleratedVideoDecoder::DecodeResult::kDecodeError);
  if (result != AcceleratedVideoDecoder::DecodeResult::kDecodeError)
    EXPECT_EQ(8u, decoder_->GetBitDepth());
  return result;
}

// Test Cases

TEST_F(VP8DecoderTest, DecodeSingleFrame) {
  CompleteToDecodeFirstIFrame();
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, FailCreatePicture) {
  EXPECT_CALL(*accelerator_, CreateVP8Picture()).WillOnce(Return(nullptr));
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfSurfaces, Decode(kNullFrame));
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, DecodeCorruptFrame) {
  CompleteToDecodeFirstIFrame();
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode(kCorruptFrame));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));

  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, DecodeIAndPFrames) {
  CompleteToDecodeFirstIFrame();

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateVP8Picture());
    EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
    EXPECT_CALL(*accelerator_, OutputPicture(_));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));

  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, DecodeIandMultiplePFrames) {
  CompleteToDecodeFirstIFrame();

  for (size_t i = 0; i < 5; i++) {
    {
      InSequence sequence;
      EXPECT_CALL(*accelerator_, CreateVP8Picture());
      EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
      EXPECT_CALL(*accelerator_, OutputPicture(_));
    }
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));
  }

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, DecodeMultipleIAndPFrames) {
  CompleteToDecodeFirstIFrame();

  for (size_t i = 0; i < 10; i++) {
    {
      InSequence sequence;
      EXPECT_CALL(*accelerator_, CreateVP8Picture());
      EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
      EXPECT_CALL(*accelerator_, OutputPicture(_));
    }
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData,
              Decode((i % 3) == 0 ? kIFrame : kPFrame));
  }

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(VP8DecoderTest, HaveSkippedFrames) {
  CompleteToDecodeFirstIFrame();

  SkipFrame();
  for (size_t i = 0; i < 5; i++) {
    // VP8Decoder::Decode() gives up decoding it and returns early.
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));
  }

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

// Verify that |decoder_| returns kDecodeError if too many kPFrames are received
// while expecting a kIFrame.
TEST_F(VP8DecoderTest, HaveSkippedFramesAtMaxNumOfSizeChangeFailures) {
  CompleteToDecodeFirstIFrame();

  SkipFrame();
  for (size_t i = 0;
       i < AcceleratedVideoDecoder::kVPxMaxNumOfSizeChangeFailures; i++) {
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode(kPFrame));

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

// Verify that new kIFrame recovers |decoder_| to decode the frame when the
// previous I frame is missing.
TEST_F(VP8DecoderTest, RecoverFromSkippedFrames) {
  CompleteToDecodeFirstIFrame();

  SkipFrame();
  for (size_t i = 0; i < 5; i++)
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));

  // The new I frame recovers to decode it correctly.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateVP8Picture());
    EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
    EXPECT_CALL(*accelerator_, OutputPicture(_));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kIFrame));

  for (size_t i = 0; i < 5; i++) {
    {
      InSequence sequence;
      EXPECT_CALL(*accelerator_, CreateVP8Picture());
      EXPECT_CALL(*accelerator_, SubmitDecode(_, _));
      EXPECT_CALL(*accelerator_, OutputPicture(_));
    }
    ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(kPFrame));
  }

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(accelerator_));
  ASSERT_TRUE(decoder_->Flush());
}

}  // namespace
}  // namespace media
