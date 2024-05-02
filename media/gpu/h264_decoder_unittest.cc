// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string.h>

#include <cstring>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "media/base/test_data_util.h"
#include "media/gpu/h264_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Args;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::MakeMatcher;
using ::testing::Matcher;
using ::testing::MatcherInterface;
using ::testing::MatchResultListener;
using ::testing::Mock;
using ::testing::Return;

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
const std::string k10BitFrame0 = "bear-320x180-10bit-frame-0.h264";
const std::string k10BitFrame1 = "bear-320x180-10bit-frame-1.h264";
const std::string k10BitFrame2 = "bear-320x180-10bit-frame-2.h264";
const std::string k10BitFrame3 = "bear-320x180-10bit-frame-3.h264";
const std::string kYUV444Frame = "blackwhite_yuv444p-frame.h264";

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

// Emulates encrypted slice header parsing. We don't actually encrypt the data
// so we can easily do this by just parsing it.
H264Decoder::H264Accelerator::Status ParseSliceHeader(
    const std::vector<base::span<const uint8_t>>& data,
    const std::vector<SubsampleEntry>& subsamples,
    const std::vector<uint8_t>& sps_nalu_data,
    const std::vector<uint8_t>& pps_nalu_data,
    H264SliceHeader* slice_hdr_out) {
  // Construct the bitstream for parsing.
  std::vector<uint8_t> full_data;
  const std::vector<uint8_t> start_code = {0u, 0u, 1u};
  full_data.insert(full_data.end(), start_code.begin(), start_code.end());
  full_data.insert(full_data.end(), sps_nalu_data.begin(), sps_nalu_data.end());
  full_data.insert(full_data.end(), start_code.begin(), start_code.end());
  full_data.insert(full_data.end(), pps_nalu_data.begin(), pps_nalu_data.end());
  for (const auto& span : data) {
    full_data.insert(full_data.end(), start_code.begin(), start_code.end());
    full_data.insert(full_data.end(), span.begin(), span.end());
  }
  H264Parser parser;
  parser.SetStream(full_data.data(), full_data.size());
  while (true) {
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream)
      break;
    EXPECT_EQ(H264Parser::kOk, res);
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        int sps_id;
        EXPECT_EQ(H264Parser::kOk, parser.ParseSPS(&sps_id));
        break;
      case H264NALU::kPPS:
        int pps_id;
        EXPECT_EQ(H264Parser::kOk, parser.ParsePPS(&pps_id));
        break;
      case H264NALU::kIDRSlice:  // fallthrough
      case H264NALU::kNonIDRSlice:
        EXPECT_EQ(H264Parser::kOk,
                  parser.ParseSliceHeader(nalu, slice_hdr_out));
        slice_hdr_out->full_sample_encryption = true;
        break;
    }
  }
  return H264Decoder::H264Accelerator::Status::kOk;
}

class MockH264Accelerator : public H264Decoder::H264Accelerator {
 public:
  MockH264Accelerator() = default;

  MOCK_METHOD0(CreateH264Picture, scoped_refptr<H264Picture>());

  MOCK_METHOD1(SubmitDecode, Status(scoped_refptr<H264Picture> pic));
  MOCK_METHOD4(ParseEncryptedSliceHeader,
               Status(const std::vector<base::span<const uint8_t>>& data,
                      const std::vector<SubsampleEntry>& subsamples,
                      uint64_t secure_handle,
                      H264SliceHeader* slice_hdr_out));
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

  void ProcessSPS(const H264SPS* sps,
                  base::span<const uint8_t> sps_nalu_data) override {
    last_sps_nalu_data.assign(sps_nalu_data.begin(), sps_nalu_data.end());
  }

  void ProcessPPS(const H264PPS* pps,
                  base::span<const uint8_t> pps_nalu_data) override {
    last_pps_nalu_data.assign(pps_nalu_data.begin(), pps_nalu_data.end());
  }

  std::vector<uint8_t> last_sps_nalu_data;
  std::vector<uint8_t> last_pps_nalu_data;
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
  // |H264Decoder::Decode|. If |full_sample_encryption| is true, then it sets
  // a DecryptConfig for the the DecoderBuffer that indicates all but the first
  // byte are encrypted. Returns the same result as |H264Decoder::Decode|.
  AcceleratedVideoDecoder::DecodeResult Decode(
      bool full_sample_encryption = false);

 protected:
  std::unique_ptr<H264Decoder> decoder_;
  raw_ptr<MockH264Accelerator> accelerator_;

 private:
  base::queue<std::string> input_frame_files_;
  std::string bitstream_;
  scoped_refptr<DecoderBuffer> decoder_buffer_;
};

void H264DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH264Accelerator>();
  accelerator_ = mock_accelerator.get();
  decoder_ = std::make_unique<H264Decoder>(std::move(mock_accelerator),
                                           VIDEO_CODEC_PROFILE_UNKNOWN);

  // Sets default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, CreateH264Picture()).WillByDefault([]() {
    return new H264Picture();
  });
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

AcceleratedVideoDecoder::DecodeResult H264DecoderTest::Decode(
    bool full_sample_encryption) {
  while (true) {
    auto result = decoder_->Decode();
    int32_t bitstream_id = 0;
    if (result != AcceleratedVideoDecoder::kRanOutOfStreamData ||
        input_frame_files_.empty())
      return result;
    auto input_file = GetTestDataFilePath(input_frame_files_.front());
    input_frame_files_.pop();
    CHECK(base::ReadFileToString(input_file, &bitstream_));
    decoder_buffer_ = DecoderBuffer::CopyFrom(base::as_byte_span(bitstream_));
    if (full_sample_encryption) {
      // We only use this in 2 tests, each use the same data where the offset to
      // the byte after the NALU type for the slice header is 669.
      constexpr int kOffsetToSliceHeader = 669;
      decoder_buffer_->set_decrypt_config(DecryptConfig::CreateCencConfig(
          "kFakeKeyId", std::string(DecryptConfig::kDecryptionKeySize, 'x'),
          {SubsampleEntry(kOffsetToSliceHeader,
                          bitstream_.size() - kOffsetToSliceHeader)}));
    }
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
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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

// This is for CENCv1 full sample encryption.
TEST_F(H264DecoderTest, DecodeSingleEncryptedFrame) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(true));
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
        .WillOnce([this](const std::vector<base::span<const uint8_t>>& data,
                         const std::vector<SubsampleEntry>& subsamples,
                         uint64_t /*secure_handle*/,
                         H264SliceHeader* slice_hdr_out) {
          return ParseSliceHeader(
              data, subsamples, accelerator_->last_sps_nalu_data,
              accelerator_->last_pps_nalu_data, slice_hdr_out);
        });
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
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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

TEST_F(H264DecoderTest, Decode10BitStream) {
  SetInputFrameFiles({k10BitFrame0, k10BitFrame1, k10BitFrame2, k10BitFrame3});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(gfx::Rect(320, 180), decoder_->GetVisibleRect());
  EXPECT_EQ(H264PROFILE_HIGH10PROFILE, decoder_->GetProfile());
  EXPECT_EQ(10u, decoder_->GetBitDepth());
  EXPECT_LE(14u, decoder_->GetRequiredNumOfPictures());

  // One picture will be kept in the DPB for reordering. The second picture
  // should be outputted after feeding the third and fourth frames.
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _)).Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _)).Times(4);

  Expectation decode_poc0, decode_poc2, decode_poc4, decode_poc6;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    decode_poc6 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(6)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(4)));
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

TEST_F(H264DecoderTest, OutputPictureFailureCausesDecodeToFail) {
  // Provide enough data that Decode() will try to output a frame.
  SetInputFrameFiles({
      kBaselineFrame0,
      kBaselineFrame1,
  });
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_CALL(*accelerator_, OutputPicture(_)).WillRepeatedly(Return(false));
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode());
}

TEST_F(H264DecoderTest, DecodeProfileHigh) {
  SetInputFrameFiles({kHighFrame0, kHighFrame1, kHighFrame2, kHighFrame3});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_HIGH, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_LE(16u, decoder_->GetRequiredNumOfPictures());

  // Two pictures will be kept in the DPB for reordering. The first picture
  // should be outputted after feeding the third frame.
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

TEST_F(H264DecoderTest, DenyDecodeNonYUV420) {
  // YUV444 frame causes kDecodeError.
  SetInputFrameFiles({kYUV444Frame});
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode());
}

TEST_F(H264DecoderTest, SwitchBaselineToHigh) {
  SetInputFrameFiles({
      kBaselineFrame0, kHighFrame0, kHighFrame1, kHighFrame2, kHighFrame3,
  });
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_HIGH, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_HIGH, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_LE(16u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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

TEST_F(H264DecoderTest, SwitchYUV420ToNonYUV420) {
  SetInputFrameFiles({kBaselineFrame0, kYUV444Frame});
  // The first frame, YUV420, is decoded with no error.
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(_));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  // The second frame, YUV444, causes kDecodeError.
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode());
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
      {static_cast<uint32_t>(bitstream.size()), 0},
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

  auto buffer = DecoderBuffer::CopyFrom(base::as_byte_span(bitstream));
  ASSERT_NE(buffer.get(), nullptr);
  buffer->set_decrypt_config(std::move(decrypt_config));
  decoder_->SetStream(0, *buffer);
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, ParseEncryptedSliceHeaderRetry) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(true));
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode(true));

  // Try again, assuming key still not set. Only ParseEncryptedSliceHeader()
  // should be called again.
  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode(true));

  // Assume key has been provided now, next call to Decode() should proceed.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
        .WillOnce([this](const std::vector<base::span<const uint8_t>>& data,
                         const std::vector<SubsampleEntry>& subsamples,
                         uint64_t /*secure_handle*/,
                         H264SliceHeader* slice_hdr_out) {
          return ParseSliceHeader(
              data, subsamples, accelerator_->last_sps_nalu_data,
              accelerator_->last_pps_nalu_data, slice_hdr_out);
        });
    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(true));

  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitFrameMetadataRetry) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitSliceRetry) {
  SetInputFrameFiles({kBaselineFrame0});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SubmitDecodeRetry) {
  SetInputFrameFiles({kBaselineFrame0, kBaselineFrame1});
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
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
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(2)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(2)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, SetStreamRetry) {
  SetInputFrameFiles({kBaselineFrame0});

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kOk));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;

    EXPECT_CALL(*accelerator_, CreateH264Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(WithPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(WithPoc(0)));
  }
  ASSERT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  ASSERT_TRUE(decoder_->Flush());
}

}  // namespace
}  // namespace media
