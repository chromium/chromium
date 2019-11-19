// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <cstring>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/gpu/h264_decoder.h"
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

const std::string kBaselineFrame0 = "bear-320x192-baseline-frame-0.h264";
const std::string kBaselineFrame1 = "bear-320x192-baseline-frame-1.h264";
const std::string kBaselineFrame2 = "bear-320x192-baseline-frame-2.h264";
const std::string kBaselineFrame3 = "bear-320x192-baseline-frame-3.h264";
const std::string kHighFrame0 = "bear-320x192-high-frame-0.h264";
const std::string kHighFrame1 = "bear-320x192-high-frame-1.h264";
const std::string kHighFrame2 = "bear-320x192-high-frame-2.h264";
const std::string kHighFrame3 = "bear-320x192-high-frame-3.h264";

// Checks whether the decrypt config in the picture matches the decrypt config
// passed to this matcher.
MATCHER_P(DecryptConfigMatches, decrypt_config, "") {
  return arg->decrypt_config()->Matches(*decrypt_config);
}

MATCHER(SubsampleSizeMatches, "Verify subsample sizes match buffer size") {
  const size_t buffer_size = ::testing::get<0>(arg);
  const std::vector<SubsampleEntry>& subsamples = ::testing::get<1>(arg);
  size_t subsample_total_size = 0;
  for (const auto& sample : subsamples) {
    subsample_total_size += sample.cypher_bytes;
    subsample_total_size += sample.clear_bytes;
  }
  return subsample_total_size == buffer_size;
}

// Given a H264NALU (arg0), compute the slice header and store a copy in
// both |arg1| and |slice_header|. This assumes that the NALU comes from
// kBaselineFrame0.
ACTION_P(ComputeSliceHeader, slice_header) {
  const H264NALU& slice_nalu = arg0;
  // |arg1| and |slice_header| are H264SliceHeader*.

  // Ideally we could just parse |slice_nalu|, but the parser needs additional
  // data (like SPS and PPS entries) which we don't have. So this simulates
  // parsing of |slice_nalu| by simply setting the appropriate fields

  // Zero out |slice_header| so there is no need to set a lot of default values.
  std::memset(slice_header, 0, sizeof(H264SliceHeader));

  // Extract the values directly from the H264NALU provided.
  slice_header->idr_pic_flag = (slice_nalu.nal_unit_type == 5);
  slice_header->nal_ref_idc = slice_nalu.nal_ref_idc;
  slice_header->nalu_data = slice_nalu.data;
  slice_header->nalu_size = slice_nalu.size;

  // Don't want to duplicate all the work of H264Parser.ParseSliceHeader(),
  // so the following were determined by looking at the slice header after
  // H264_Parser.ParseSliceHeader() was called on kBaselineFrame0.
  slice_header->header_bit_size = 0x24;
  slice_header->slice_type = 7;
  slice_header->slice_qp_delta = 8;
  slice_header->dec_ref_pic_marking_bit_size = 2u;

  // Now that we have created our local copy of the slice header, copy it into
  // |arg1| and return success.
  std::memcpy(arg1, slice_header, sizeof(H264SliceHeader));
  return H264Decoder::H264Accelerator::Status::kOk;
}

// Compare 2 H264SliceHeader objects for equality.
MATCHER_P(SliceHeaderMatches, slice_header, "Verify H264SliceHeader objects") {
  // Rather than match pointers, the contents must be the same.
  return std::memcmp(arg, slice_header, sizeof(H264SliceHeader)) == 0;
}

class MockH264Accelerator : public H264Decoder::H264Accelerator {
 public:
  MockH264Accelerator() = default;

  MOCK_METHOD0(CreateH264Picture, scoped_refptr<H264Picture>());
  MOCK_METHOD1(SubmitDecode, Status(scoped_refptr<H264Picture> pic));
  MOCK_METHOD7(SubmitFrameMetadata,
               Status(const H264SPS* sps,
                      const H264PPS* pps,
                      const H264DPB& dpb,
                      const H264Picture::Vector& ref_pic_listp0,
                      const H264Picture::Vector& ref_pic_listb0,
                      const H264Picture::Vector& ref_pic_listb1,
                      scoped_refptr<H264Picture> pic));
  MOCK_METHOD8(SubmitSlice,
               Status(const H264PPS* pps,
                      const H264SliceHeader* slice_hdr,
                      const H264Picture::Vector& ref_pic_list0,
                      const H264Picture::Vector& ref_pic_list1,
                      scoped_refptr<H264Picture> pic,
                      const uint8_t* data,
                      size_t size,
                      const std::vector<SubsampleEntry>& subsamples));
  MOCK_METHOD1(OutputPicture, bool(scoped_refptr<H264Picture> pic));
  MOCK_METHOD2(SetStream,
               Status(base::span<const uint8_t> stream,
                      const DecryptConfig* decrypt_config));

  void Reset() override {}
};

// Test H264Decoder by feeding different of h264 frame sequences and make
// sure it behaves as expected.
class H264DecoderTest : public ::testing::Test {
 public:
  H264DecoderTest() = default;

  void SetUp() override;

  // Sets the bitstreams to be decoded, frame by frame. The content of each
  // file is the encoded bitstream of a single video frame.
  void SetInputFrameFiles(const std::vector<std::string>& frame_files);

  // Keeps decoding the input bitstream set at |SetInputFrameFiles| until the
  // decoder has consumed all bitstreams or returned from
  // |H264Decoder::Decode|. Returns the same result as |H264Decoder::Decode|.
  AcceleratedVideoDecoder::DecodeResult Decode();

 protected:
  std::unique_ptr<H264Decoder> decoder_;
  MockH264Accelerator* accelerator_;

 private:
  base::queue<std::string> input_frame_files_;
  std::string bitstream_;
  scoped_refptr<DecoderBuffer> decoder_buffer_;
};

void H264DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH264Accelerator>();
  accelerator_ = mock_accelerator.get();
  decoder_.reset(new H264Decoder(std::move(mock_accelerator)));

  // Sets default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, CreateH264Picture()).WillByDefault(Invoke([]() {
    return new H264Picture();
  }));
  ON_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
      .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
  ON_CALL(*accelerator_, SubmitDecode(_))
      .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
  ON_CALL(*accelerator_, OutputPicture(_)).WillByDefault(Return(true));
  ON_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .With(Args<6, 7>(SubsampleSizeMatches()))
      .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
  ON_CALL(*accelerator_, SetStream(_, _))
      .WillByDefault(
          Return(H264Decoder::H264Accelerator::Status::kNotSupported));
}

void H264DecoderTest::SetInputFrameFiles(
    const std::vector<std::string>& input_frame_files) {
  for (auto f : input_frame_files)
    input_frame_files_.push(f);
}

AcceleratedVideoDecoder::DecodeResult H264DecoderTest::Decode() {
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

// To have better description on mismatch.
class WithPocMatcher : public MatcherInterface<scoped_refptr<H264Picture>> {
 public:
  explicit WithPocMatcher(int expected_poc) : expected_poc_(expected_poc) {}

  bool MatchAndExplain(scoped_refptr<H264Picture> p,
                       MatchResultListener* listener) const override {
    if (p->pic_order_cnt == expected_poc_)
      return true;
    *listener << "with poc: " << p->pic_order_cnt;
    return false;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "with poc " << expected_poc_;
  }

 private:
  int expected_poc_;
};

inline Matcher<scoped_refptr<H264Picture>> WithPoc(int expected_poc) {
  return MakeMatcher(new WithPocMatcher(expected_poc));
}

// Test Cases

TEST_F(H264DecoderTest, DecodeSingleFrame) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  EXPECT_CALL(*accelerator_, CreateH264Picture()).WillOnce(Return(nullptr));
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfSurfaces, Decode());
  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&*accelerator_));

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(_));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SkipNonIDRFrames) {
  SetInputFrameFiles({kBaselineFrame1, kBaselineFrame2, kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, DecodeProfileBaseline) {
  SetInputFrameFiles({
      kBaselineFrame0, kBaselineFrame1, kBaselineFrame2, kBaselineFrame3,
  });
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(4);

  Expectation decode_poc0, decode_poc2, decode_poc4, decode_poc6;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(4)));
    decode_poc6 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(6)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(4))).After(decode_poc4);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(6))).After(decode_poc6);
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, DecodeProfileHigh) {
  SetInputFrameFiles({kHighFrame0, kHighFrame1, kHighFrame2, kHighFrame3});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(16u, decoder_->GetRequiredNumOfPictures());

  // Two pictures will be kept in DPB for reordering. The first picture should
  // be outputted after feeding the third frame.
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(4);

  Expectation decode_poc0, decode_poc2, decode_poc4, decode_poc6;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(4)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    decode_poc6 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(6)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(4))).After(decode_poc4);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(6))).After(decode_poc6);
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SwitchBaselineToHigh) {
  SetInputFrameFiles({
      kBaselineFrame0, kHighFrame0, kHighFrame1, kHighFrame2, kHighFrame3,
  });
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(16u, decoder_->GetRequiredNumOfPictures());

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&*accelerator_));

  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(4);

  Expectation decode_poc0, decode_poc2, decode_poc4, decode_poc6;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(4)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    decode_poc6 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(6)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(4))).After(decode_poc4);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(6))).After(decode_poc6);
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SwitchHighToBaseline) {
  SetInputFrameFiles({
      kHighFrame0, kBaselineFrame0, kBaselineFrame1, kBaselineFrame2,
      kBaselineFrame3,
  });
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(16u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  ASSERT_TRUE(Mock::VerifyAndClearExpectations(&*accelerator_));

  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(4);

  Expectation decode_poc0, decode_poc2, decode_poc4, decode_poc6;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(4)));
    decode_poc6 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(6)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(4))).After(decode_poc4);
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(6))).After(decode_poc6);
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

// Verify that the decryption config is passed to the accelerator.
TEST_F(H264DecoderTest, SetEncryptedStream) {
  std::string bitstream;
  auto input_file = GetTestDataFilePath(kBaselineFrame0);
  CHECK(base::ReadFileToString(input_file, &bitstream));

  const char kAnyKeyId[] = "any_16byte_keyid";
  const char kAnyIv[] = "any_16byte_iv___";
  const std::vector<SubsampleEntry> subsamples = {
      // No encrypted bytes. This test only checks whether the data is passed
      // thru to the acclerator so making this completely clear.
      {bitstream.size(), 0},
  };

  std::unique_ptr<DecryptConfig> decrypt_config =
      DecryptConfig::CreateCencConfig(kAnyKeyId, kAnyIv, subsamples);
  EXPECT_CALL(*accelerator_,
              SubmitFrameMetadata(_, _, _, _, _, _,
                                  DecryptConfigMatches(decrypt_config.get())))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_,
              SubmitDecode(DecryptConfigMatches(decrypt_config.get())))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kOk));

  auto buffer = DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(bitstream.data()), bitstream.size());
  ASSERT_NE(buffer.get(), nullptr);
  buffer->set_decrypt_config(std::move(decrypt_config));
  decoder_->SetStream(0, *buffer);
  EXPECT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, decoder_->Decode());
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitFrameMetadataRetry) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
        .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitFrameMetadata()
  // should be called again.
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should proceed.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitSliceRetry) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
        .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitSlice() should be
  // called again.
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should proceed.
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitDecodeRetry) {
  SetInputFrameFiles({kBaselineFrame0, kBaselineFrame1});
  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_))
        .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitDecode() should be
  // called again.
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*accelerator_, SubmitDecode(_))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should output
  // the first frame.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2)));
  }
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SetStreamRetry) {
  SetInputFrameFiles({kBaselineFrame0});

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kOk));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  ASSERT_EQ(AcceleratedVideoDecoder::kAllocateNewSurfaces, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;

    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_TRUE(decoder_->Flush());
}

}  // namespace
}  // namespace media
