// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_decoder.h"

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
#include "base/memory/scoped_refptr.h"
#include "media/base/test_data_util.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/gpu/h264_builder.h"
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
  parser.SetStream(full_data);
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
  MOCK_METHOD0(RequiresRefLists, bool());

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

  void ResetExpectations() {
    // Sets default behaviors for mock methods for convenience.
    ON_CALL(*accelerator_, CreateH264Picture()).WillByDefault([]() {
      return base::MakeRefCounted<H264Picture>();
    });
    ON_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
        .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
    ON_CALL(*accelerator_, SubmitDecode(_))
        .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
    ON_CALL(*accelerator_, OutputPicture(_)).WillByDefault(Return(true));
    ON_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
        .With(Args<6, 7>(SubsampleSizeMatches()))
        .WillByDefault(Return(H264Decoder::H264Accelerator::Status::kOk));
    EXPECT_CALL(*accelerator_, SetStream(_, _))
        .WillRepeatedly(
            Return(H264Decoder::H264Accelerator::Status::kNotSupported));
  }

 protected:
  std::vector<scoped_refptr<DecoderBuffer>> decoder_buffers_;
  std::unique_ptr<H264Decoder> decoder_;
  raw_ptr<MockH264Accelerator> accelerator_;

 private:
  base::queue<std::string> input_frame_files_;
};

void H264DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH264Accelerator>();
  EXPECT_CALL(*mock_accelerator, RequiresRefLists())
      .WillRepeatedly(Return(false));
  accelerator_ = mock_accelerator.get();
  decoder_ = std::make_unique<H264Decoder>(std::move(mock_accelerator),
                                           VIDEO_CODEC_PROFILE_UNKNOWN);
  ResetExpectations();
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
    std::string bitstream;
    CHECK(base::ReadFileToString(input_file, &bitstream));
    decoder_buffers_.push_back(
        DecoderBuffer::CopyFrom(base::as_byte_span(bitstream)));
    if (full_sample_encryption) {
      // We only use this in 2 tests, each use the same data where the offset to
      // the byte after the NALU type for the slice header is 669.
      constexpr int kOffsetToSliceHeader = 669;
      decoder_buffers_.back()->set_decrypt_config(
          DecryptConfig::CreateCencConfig(
              "kFakeKeyId", std::string(DecryptConfig::kDecryptionKeySize, 'x'),
              {SubsampleEntry(kOffsetToSliceHeader,
                              bitstream.size() - kOffsetToSliceHeader)}));
    }
    EXPECT_NE(decoder_buffers_.back().get(), nullptr);
    decoder_->SetStream(bitstream_id++, decoder_buffers_.back());
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
  ResetExpectations();

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

  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce([this](const std::vector<base::span<const uint8_t>>& data,
                       const std::vector<SubsampleEntry>& subsamples,
                       uint64_t /*secure_handle*/,
                       H264SliceHeader* slice_hdr_out) {
        return ParseSliceHeader(
            data, subsamples, accelerator_->last_sps_nalu_data,
            accelerator_->last_pps_nalu_data, slice_hdr_out);
      });

  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(true));
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

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
  ResetExpectations();

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
  ResetExpectations();

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

  decoder_buffers_.push_back(
      DecoderBuffer::CopyFrom(base::as_byte_span(bitstream)));
  ASSERT_NE(decoder_buffers_.back().get(), nullptr);
  decoder_buffers_.back()->set_decrypt_config(std::move(decrypt_config));
  decoder_->SetStream(0, decoder_buffers_.back());
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H264DecoderTest, ParseEncryptedSliceHeaderRetry) {
  SetInputFrameFiles({kBaselineFrame0});

  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode(true));

  // Try again, assuming key still not set. Only ParseEncryptedSliceHeader()
  // should be called again.
  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce(Return(H264Decoder::H264Accelerator::Status::kTryAgain));
  ASSERT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode(true));

  // Assume key has been provided now, next call to Decode() should proceed.
  EXPECT_CALL(*accelerator_, ParseEncryptedSliceHeader(_, _, _, _))
      .WillOnce([this](const std::vector<base::span<const uint8_t>>& data,
                       const std::vector<SubsampleEntry>& subsamples,
                       uint64_t /*secure_handle*/,
                       H264SliceHeader* slice_hdr_out) {
        return ParseSliceHeader(
            data, subsamples, accelerator_->last_sps_nalu_data,
            accelerator_->last_pps_nalu_data, slice_hdr_out);
      });

  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(true));
  EXPECT_EQ(gfx::Size(320, 192), decoder_->GetPicSize());
  EXPECT_EQ(H264PROFILE_BASELINE, decoder_->GetProfile());
  EXPECT_LE(9u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
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

// POC: Unvalidated first_mb_in_slice in subsequent slices reaches the
// accelerator.
//
// H264Decoder::PreprocessCurrentSlice() only validates first_mb_in_slice==0
// when IsNewPrimaryCodedPicture() returns true. For the second (and later)
// slice of a multi-slice picture, IsNewPrimaryCodedPicture() returns false
// and the value is forwarded to accelerator_->SubmitSlice() with no upper
// bound check. The H.264 spec (7.4.3) requires first_mb_in_slice <
// PicSizeInMbs; the H.265 parser enforces the equivalent condition.
//
// This test feeds a hand-crafted Annex-B stream:
//   SPS  : 320x240 (PicSizeInMbs = 20*15 = 300)
//   PPS
//   IDR slice 0 : first_mb_in_slice = 0      (passes the new-picture check)
//   IDR slice 1 : first_mb_in_slice = 65535  (same frame_num/idr_pic_id ->
//                                             IsNewPrimaryCodedPicture()=false)
TEST_F(H264DecoderTest, UnvalidatedFirstMbInSliceReachesAccelerator) {
  static constexpr uint8_t kStream[] = {
      0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1e, 0xda, 0x05, 0x07,
      0xe4, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x38, 0x80, 0x00, 0x00,
      0x00, 0x01, 0x65, 0x88, 0x84, 0xd5, 0x55, 0x40, 0x00, 0x00, 0x00,
      0x01, 0x65, 0x00, 0x00, 0x80, 0x00, 0x08, 0x84, 0xd5, 0x55, 0x40,
  };

  decoder_->SetStream(0, DecoderBuffer::CopyFrom(base::span(kStream)));

  // First Decode() processes the SPS and reports a config change.
  ASSERT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());
  EXPECT_EQ(gfx::Size(320, 240), decoder_->GetPicSize());

  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));

  // Second Decode() processes PPS + both IDR slices of the same picture.
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, decoder_->Decode());
}

TEST_F(H264DecoderTest, ModifyReferencePicList_CompactionTime) {
  // Seed the decoder DPB and `curr_pic_` with a long term ref pic.
  scoped_refptr<H264Picture> ref_pic = base::MakeRefCounted<H264Picture>();
  ref_pic->pic_num = 11;
  ref_pic->long_term_pic_num = 11;
  ref_pic->long_term = true;
  ref_pic->ref = true;
  decoder_->SetCurrPicForTesting(ref_pic);
  decoder_->StoreDPBPicForTesting(ref_pic);

  // Prime the reference pic list with pictures {#10, #11, #12}
  H264Picture::Vector temp_ref_pic_list0;
  H264Picture::Vector temp_ref_pic_list1;
  for (int ltpn = 10; ltpn < 13; ltpn++) {
    scoped_refptr<H264Picture> pic = base::MakeRefCounted<H264Picture>();
    pic->long_term = true;
    pic->ref = true;
    pic->long_term_pic_num = ltpn;
    temp_ref_pic_list0.push_back(pic);
  }

  H264SliceHeader slice_hdr;
  // Resize the list to the size requested in the slice header.
  // Note that per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to
  // indicate there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference and will be
  // initialized to nullptr, which must be handled by clients.
  // `temp_ref_pic_list_0` has 3 elements, and add an extra null.
  slice_hdr.num_ref_idx_l0_active_minus1 = 3;
  slice_hdr.ref_pic_list_modification_flag_l0 = true;

  // We want to bring pic 11 to the front now.
  slice_hdr.ref_list_l0_modifications[0].modification_of_pic_nums_idc = 2;
  slice_hdr.ref_list_l0_modifications[0].long_term_pic_num = 11;

  // modification code "3" means "end of modification code list".
  slice_hdr.ref_list_l0_modifications[1].modification_of_pic_nums_idc = 3;

  ASSERT_TRUE(decoder_->ModifyReferencePicListsForTesting(
      &slice_hdr, &temp_ref_pic_list0, &temp_ref_pic_list1));

  ASSERT_EQ(temp_ref_pic_list0.size(), 4u);
  ASSERT_EQ(temp_ref_pic_list0[0]->long_term_pic_num, 11);
  ASSERT_EQ(temp_ref_pic_list0[1]->long_term_pic_num, 10);
  ASSERT_EQ(temp_ref_pic_list0[2]->long_term_pic_num, 12);
  ASSERT_EQ(temp_ref_pic_list0[3], nullptr);
}

TEST_F(H264DecoderTest, ModifyReferencePicList_CompactionWithNullEntry) {
  // Seed the decoder DPB and `curr_pic_` with a long term ref pic.
  scoped_refptr<H264Picture> ref_pic = base::MakeRefCounted<H264Picture>();
  ref_pic->pic_num = 11;
  ref_pic->long_term_pic_num = 11;
  ref_pic->long_term = true;
  ref_pic->ref = true;
  decoder_->SetCurrPicForTesting(ref_pic);
  decoder_->StoreDPBPicForTesting(ref_pic);

  // Prime the reference pic list with pictures {#10, #11, #12, #13}
  H264Picture::Vector temp_ref_pic_list0;
  H264Picture::Vector temp_ref_pic_list1;
  for (int ltpn = 10; ltpn < 14; ltpn++) {
    scoped_refptr<H264Picture> pic = base::MakeRefCounted<H264Picture>();
    pic->long_term = true;
    pic->ref = true;
    pic->long_term_pic_num = ltpn;
    temp_ref_pic_list0.push_back(pic);
  }

  // remove entry #12
  temp_ref_pic_list0[2] = nullptr;

  H264SliceHeader slice_hdr;
  // Resize the list to the size requested in the slice header.
  // Note that per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to
  // indicate there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference and will be
  // initialized to nullptr, which must be handled by clients.
  // `temp_ref_pic_list_0` has 4 elements, plus an extra null at the end.
  slice_hdr.num_ref_idx_l0_active_minus1 = 4;
  slice_hdr.ref_pic_list_modification_flag_l0 = true;

  // We want to bring pic 11 to the front now.
  slice_hdr.ref_list_l0_modifications[0].modification_of_pic_nums_idc = 2;
  slice_hdr.ref_list_l0_modifications[0].long_term_pic_num = 11;

  // modification code "3" means "end of modification code list".
  slice_hdr.ref_list_l0_modifications[1].modification_of_pic_nums_idc = 3;

  ASSERT_TRUE(decoder_->ModifyReferencePicListsForTesting(
      &slice_hdr, &temp_ref_pic_list0, &temp_ref_pic_list1));

  // The null entry is treated as an entry, and is shifted accordingly.
  ASSERT_EQ(temp_ref_pic_list0.size(), 5u);
  ASSERT_EQ(temp_ref_pic_list0[0]->long_term_pic_num, 11);
  ASSERT_EQ(temp_ref_pic_list0[1]->long_term_pic_num, 10);
  ASSERT_EQ(temp_ref_pic_list0[2], nullptr);
  ASSERT_EQ(temp_ref_pic_list0[3]->long_term_pic_num, 13);
  ASSERT_EQ(temp_ref_pic_list0[4], nullptr);
}

TEST_F(H264DecoderTest,
       ModifyReferencePicList_CompactionWithInterspersedNullsAndDuplicates) {
  // Seed the decoder DPB and `curr_pic_` with a long term ref pic.
  scoped_refptr<H264Picture> ref_pic = base::MakeRefCounted<H264Picture>();
  ref_pic->pic_num = 11;
  ref_pic->long_term_pic_num = 11;
  ref_pic->long_term = true;
  ref_pic->ref = true;
  decoder_->SetCurrPicForTesting(ref_pic);
  decoder_->StoreDPBPicForTesting(ref_pic);

  // Prime the reference pic list with pictures {#10, #11, #12, #13}
  H264Picture::Vector temp_ref_pic_list0;
  H264Picture::Vector temp_ref_pic_list1;
  std::vector<int> ltpns = {10, -1, 11, 12, 13, -1, 18};

  for (int ltpn : ltpns) {
    if (ltpn == -1) {
      temp_ref_pic_list0.push_back(nullptr);
    } else {
      scoped_refptr<H264Picture> pic = base::MakeRefCounted<H264Picture>();
      pic->long_term = true;
      pic->ref = true;
      pic->long_term_pic_num = ltpn;
      temp_ref_pic_list0.push_back(pic);
    }
  }

  H264SliceHeader slice_hdr;
  // Resize the list to the size requested in the slice header.
  // Note that per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to
  // indicate there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference and will be
  // initialized to nullptr, which must be handled by clients.
  slice_hdr.num_ref_idx_l0_active_minus1 = ltpns.size();
  slice_hdr.ref_pic_list_modification_flag_l0 = true;

  // We want to bring pic 11 to the front now.
  slice_hdr.ref_list_l0_modifications[0].modification_of_pic_nums_idc = 2;
  slice_hdr.ref_list_l0_modifications[0].long_term_pic_num = 11;

  // modification code "3" means "end of modification code list".
  slice_hdr.ref_list_l0_modifications[1].modification_of_pic_nums_idc = 3;

  ASSERT_TRUE(decoder_->ModifyReferencePicListsForTesting(
      &slice_hdr, &temp_ref_pic_list0, &temp_ref_pic_list1));

  // The null entries are treated as entries, and is shifted accordingly. The
  // duplicate '11' ltpns are removed.
  ASSERT_EQ(temp_ref_pic_list0.size(), 8u);

  ASSERT_EQ(temp_ref_pic_list0[0]->long_term_pic_num, 11);
  ASSERT_EQ(temp_ref_pic_list0[1]->long_term_pic_num, 10);
  ASSERT_EQ(temp_ref_pic_list0[2], nullptr);
  ASSERT_EQ(temp_ref_pic_list0[3]->long_term_pic_num, 12);
  ASSERT_EQ(temp_ref_pic_list0[4]->long_term_pic_num, 13);
  ASSERT_EQ(temp_ref_pic_list0[5], nullptr);
  ASSERT_EQ(temp_ref_pic_list0[6]->long_term_pic_num, 18);
  ASSERT_EQ(temp_ref_pic_list0[7], nullptr);
}

TEST_F(H264DecoderTest, HandleLargeFrameNumGap) {
  auto mock_accelerator = std::make_unique<MockH264Accelerator>();
  EXPECT_CALL(*mock_accelerator, RequiresRefLists())
      .WillRepeatedly(Return(true));
  accelerator_ = mock_accelerator.get();
  decoder_ = std::make_unique<H264Decoder>(std::move(mock_accelerator),
                                           VIDEO_CODEC_PROFILE_UNKNOWN);

  H26xAnnexBBitstreamBuilder builder;

  H264SPS sps = {};
  sps.seq_parameter_set_id = 0;
  sps.profile_idc = H264SPS::kProfileIDCMain;
  sps.level_idc = H264SPS::kLevelIDC5p1;
  sps.log2_max_frame_num_minus4 = 12;  // MaxFrameNum = 65536
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.pic_width_in_mbs_minus1 = 39;
  sps.pic_height_in_map_units_minus1 = 29;
  sps.frame_mbs_only_flag = true;
  sps.max_num_ref_frames = 1;
  sps.gaps_in_frame_num_value_allowed_flag = true;
  BuildPackedH264SPS(builder, sps);

  H264PPS pps = {};
  pps.pic_parameter_set_id = 0;
  pps.seq_parameter_set_id = 0;
  BuildPackedH264PPS(builder, sps, pps);

  // IDR Slice (frame_num = 0)
  builder.AppendBits(32, 0x00000001);  // start code
  builder.Flush();
  builder.AppendBits(1, 0);                    // forbidden_zero_bit
  builder.AppendBits(2, 3);                    // nal_ref_idc
  builder.AppendBits(5, H264NALU::kIDRSlice);  // nal_unit_type

  builder.AppendUE(0);  // first_mb_in_slice
  builder.AppendUE(7);  // slice_type = I (7)
  builder.AppendUE(0);  // pic_parameter_set_id
  builder.AppendBits(sps.log2_max_frame_num_minus4 + 4, 0);  // frame_num
  builder.AppendUE(0);                                       // idr_pic_id
  builder.AppendBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4, 0);  // poc_lsb

  builder.AppendBool(false);  // no_output_of_prior_pics_flag
  builder.AppendBool(false);  // long_term_reference_flag

  builder.AppendSE(0);       // slice_qp_delta
  builder.AppendBool(true);  // byte alignment bit
  builder.Flush();

  // Non-IDR Slice (frame_num = 60000)
  builder.AppendBits(32, 0x00000001);  // start code
  builder.Flush();
  builder.AppendBits(1, 0);                       // forbidden_zero_bit
  builder.AppendBits(2, 3);                       // nal_ref_idc
  builder.AppendBits(5, H264NALU::kNonIDRSlice);  // nal_unit_type

  builder.AppendUE(0);  // first_mb_in_slice
  builder.AppendUE(5);  // slice_type = P (5)
  builder.AppendUE(0);  // pic_parameter_set_id
  builder.AppendBits(sps.log2_max_frame_num_minus4 + 4, 60000);  // frame_num
  builder.AppendBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4, 2);  // poc_lsb

  builder.AppendBool(false);  // num_ref_idx_active_override_flag
  builder.AppendBool(true);   // ref_pic_list_modification_flag_l0
  builder.AppendUE(0);        // modification_of_pic_nums_idc (subtract)
  builder.AppendUE(0);        // abs_diff_pic_num_minus1 (subtract 1)
  builder.AppendUE(3);        // modification_of_pic_nums_idc (end)
  builder.AppendBool(false);  // adaptive_ref_pic_marking_mode_flag

  builder.AppendSE(0);       // slice_qp_delta
  builder.AppendBool(true);  // byte alignment bit
  builder.Flush();

  auto buffer = DecoderBuffer::CopyFrom(builder.data());

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, CreateH264Picture()).WillRepeatedly([]() {
    return base::MakeRefCounted<H264Picture>();
  });
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitDecode(_))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, OutputPicture(_)).WillRepeatedly(Return(true));

  decoder_->SetStream(1, buffer);

  // Decode config change
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());

  // Decode the rest of the stream
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
}

TEST_F(H264DecoderTest, HandleNormalFrameNumGap) {
  H26xAnnexBBitstreamBuilder builder;

  H264SPS sps = {};
  sps.seq_parameter_set_id = 0;
  sps.profile_idc = H264SPS::kProfileIDCMain;
  sps.level_idc = H264SPS::kLevelIDC5p1;
  sps.log2_max_frame_num_minus4 = 12;  // MaxFrameNum = 65536
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.pic_width_in_mbs_minus1 = 39;
  sps.pic_height_in_map_units_minus1 = 29;
  sps.frame_mbs_only_flag = true;
  sps.max_num_ref_frames = 1;
  sps.gaps_in_frame_num_value_allowed_flag = true;
  BuildPackedH264SPS(builder, sps);

  H264PPS pps = {};
  pps.pic_parameter_set_id = 0;
  pps.seq_parameter_set_id = 0;
  BuildPackedH264PPS(builder, sps, pps);

  // IDR Slice (frame_num = 0)
  builder.AppendBits(32, 0x00000001);  // start code
  builder.Flush();
  builder.AppendBits(1, 0);                    // forbidden_zero_bit
  builder.AppendBits(2, 3);                    // nal_ref_idc
  builder.AppendBits(5, H264NALU::kIDRSlice);  // nal_unit_type

  builder.AppendUE(0);  // first_mb_in_slice
  builder.AppendUE(7);  // slice_type = I (7)
  builder.AppendUE(0);  // pic_parameter_set_id
  builder.AppendBits(sps.log2_max_frame_num_minus4 + 4, 0);  // frame_num
  builder.AppendUE(0);                                       // idr_pic_id
  builder.AppendBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4, 0);  // poc_lsb

  builder.AppendBool(false);  // no_output_of_prior_pics_flag
  builder.AppendBool(false);  // long_term_reference_flag

  builder.AppendSE(0);       // slice_qp_delta
  builder.AppendBool(true);  // byte alignment bit
  builder.Flush();

  // Non-IDR Slice (frame_num = 5)
  builder.AppendBits(32, 0x00000001);  // start code
  builder.Flush();
  builder.AppendBits(1, 0);                       // forbidden_zero_bit
  builder.AppendBits(2, 3);                       // nal_ref_idc
  builder.AppendBits(5, H264NALU::kNonIDRSlice);  // nal_unit_type

  builder.AppendUE(0);  // first_mb_in_slice
  builder.AppendUE(5);  // slice_type = P (5)
  builder.AppendUE(0);  // pic_parameter_set_id
  builder.AppendBits(sps.log2_max_frame_num_minus4 + 4, 5);  // frame_num
  builder.AppendBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4, 2);  // poc_lsb

  builder.AppendBool(false);  // num_ref_idx_active_override_flag
  builder.AppendBool(false);  // ref_pic_list_modification_flag_l0
  builder.AppendBool(false);  // adaptive_ref_pic_marking_mode_flag

  builder.AppendSE(0);       // slice_qp_delta
  builder.AppendBool(true);  // byte alignment bit
  builder.Flush();

  auto buffer = DecoderBuffer::CopyFrom(builder.data());

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  // CreateH264Picture should be called 2 times: 1 for IDR, 1 for non-IDR
  EXPECT_CALL(*accelerator_, CreateH264Picture()).Times(2).WillRepeatedly([]() {
    return base::MakeRefCounted<H264Picture>();
  });
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitDecode(_))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, OutputPicture(_)).WillRepeatedly(Return(true));

  decoder_->SetStream(1, buffer);

  // Decode config change
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());

  // Decode the rest of the stream
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
}

}  // namespace
TEST_F(H264DecoderTest, IgnoreUnreferencedSPS) {
  H26xAnnexBBitstreamBuilder builder;

  // SPS 0 (4K - The actual config)
  H264SPS sps0 = {};
  sps0.seq_parameter_set_id = 0;
  sps0.profile_idc = H264SPS::kProfileIDCMain;
  sps0.level_idc = H264SPS::kLevelIDC5p1;
  sps0.log2_max_frame_num_minus4 = 4;
  sps0.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps0.pic_width_in_mbs_minus1 = 3840 / 16 - 1;         // 239
  sps0.pic_height_in_map_units_minus1 = 2160 / 16 - 1;  // 134
  sps0.frame_mbs_only_flag = true;
  sps0.max_num_ref_frames = 1;
  BuildPackedH264SPS(builder, sps0);

  // SPS 1 (480p - The trap)
  H264SPS sps1 = {};
  sps1.seq_parameter_set_id = 1;
  sps1.profile_idc = H264SPS::kProfileIDCMain;
  sps1.level_idc = H264SPS::kLevelIDC5p1;
  sps1.log2_max_frame_num_minus4 = 4;
  sps1.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps1.pic_width_in_mbs_minus1 = 640 / 16 - 1;         // 39
  sps1.pic_height_in_map_units_minus1 = 480 / 16 - 1;  // 29
  sps1.frame_mbs_only_flag = true;
  sps1.max_num_ref_frames = 1;
  BuildPackedH264SPS(builder, sps1);

  // PPS 1 (References SPS 1 - The trap)
  H264PPS pps1 = {};
  pps1.pic_parameter_set_id = 1;
  pps1.seq_parameter_set_id = 1;
  BuildPackedH264PPS(builder, sps1, pps1);

  // PPS 0 (References SPS 0)
  H264PPS pps = {};
  pps.pic_parameter_set_id = 0;
  pps.seq_parameter_set_id = 0;
  BuildPackedH264PPS(builder, sps0, pps);

  // IDR Slice (References PPS 0)
  builder.AppendBits(32, 0x00000001);  // start code
  builder.Flush();
  builder.AppendBits(1, 0);                    // forbidden_zero_bit
  builder.AppendBits(2, 3);                    // nal_ref_idc
  builder.AppendBits(5, H264NALU::kIDRSlice);  // nal_unit_type

  builder.AppendUE(0);  // first_mb_in_slice
  builder.AppendUE(7);  // slice_type = I (7)
  builder.AppendUE(0);  // pic_parameter_set_id (PPS 0)
  builder.AppendBits(sps0.log2_max_frame_num_minus4 + 4, 0);  // frame_num
  builder.AppendUE(0);                                        // idr_pic_id
  builder.AppendBits(sps0.log2_max_pic_order_cnt_lsb_minus4 + 4,
                     0);  // pic_order_cnt_lsb

  builder.AppendBool(false);  // no_output_of_prior_pics_flag
  builder.AppendBool(false);  // long_term_reference_flag

  builder.AppendSE(0);       // slice_qp_delta
  builder.AppendBool(true);  // byte alignment bit
  builder.Flush();

  auto buffer = DecoderBuffer::CopyFrom(builder.data());

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, CreateH264Picture()).WillRepeatedly([]() {
    return base::MakeRefCounted<H264Picture>();
  });
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, SubmitDecode(_))
      .WillRepeatedly(Return(H264Decoder::H264Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_, OutputPicture(_)).WillRepeatedly(Return(true));

  decoder_->SetStream(1, buffer);

  // Decode until config change. It should lazily evaluate the 4K SPS when
  // processing the slice.
  auto res1 = decoder_->Decode();
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, res1);

  // The decoder should have evaluated SPS 0 (4K), not the later unreferenced
  // SPS 1 (480p).
  EXPECT_EQ(gfx::Size(3840, 2160), decoder_->GetPicSize());

  // Decode the rest of the stream: it should process the slice using the 4K
  // global state.
  auto res2 = decoder_->Decode();
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, res2);

  // The global state should remain uncorrupted (4K).
  EXPECT_EQ(gfx::Size(3840, 2160), decoder_->GetPicSize());
}

}  // namespace media
