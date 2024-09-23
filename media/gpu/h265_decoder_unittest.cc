// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h265_decoder.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/check.h"
#include "base/containers/extend.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/types/optional_util.h"
#include "media/base/test_data_util.h"
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
constexpr char kSpsPps[] = "bear-sps-pps.hevc";
constexpr char kFrame0[] = "bear-frame0.hevc";
constexpr char kFrame1[] = "bear-frame1.hevc";
constexpr char kFrame2[] = "bear-frame2.hevc";
constexpr char kFrame3[] = "bear-frame3.hevc";
constexpr char kFrame4[] = "bear-frame4.hevc";
constexpr char kFrame5[] = "bear-frame5.hevc";
constexpr char k10BitFrame0[] = "bear-320x180-10bit-frame-0.hevc";
constexpr char k10BitFrame1[] = "bear-320x180-10bit-frame-1.hevc";
constexpr char k10BitFrame2[] = "bear-320x180-10bit-frame-2.hevc";
constexpr char k10BitFrame3[] = "bear-320x180-10bit-frame-3.hevc";
constexpr char kYUV444Frame[] = "blackwhite_yuv444p-frame.hevc";

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

// To have better description on mismatch.
class HasPocMatcher : public MatcherInterface<scoped_refptr<H265Picture>> {
 public:
  explicit HasPocMatcher(int expected_poc) : expected_poc_(expected_poc) {}

  bool MatchAndExplain(scoped_refptr<H265Picture> p,
                       MatchResultListener* listener) const override {
    if (p->pic_order_cnt_val_ == expected_poc_)
      return true;
    *listener << "with poc: " << p->pic_order_cnt_val_;
    return false;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "with poc " << expected_poc_;
  }

 private:
  int expected_poc_;
};

Matcher<scoped_refptr<H265Picture>> HasPoc(int expected_poc) {
  return MakeMatcher(new HasPocMatcher(expected_poc));
}

}  // namespace

class MockH265Accelerator : public H265Decoder::H265Accelerator {
 public:
  MockH265Accelerator() = default;

  MOCK_METHOD0(CreateH265Picture, scoped_refptr<H265Picture>());
  MOCK_METHOD8(SubmitFrameMetadata,
               Status(const H265SPS* sps,
                      const H265PPS* pps,
                      const H265SliceHeader* slice_hdr,
                      const H265Picture::Vector& ref_pic_list,
                      const H265Picture::Vector& ref_pic_set_lt_curr,
                      const H265Picture::Vector& ref_pic_set_st_curr_after,
                      const H265Picture::Vector& ref_pic_set_st_curr_before,
                      scoped_refptr<H265Picture> pic));
  MOCK_METHOD(Status,
              SubmitSlice,
              (const H265SPS* sps,
               const H265PPS* pps,
               const H265SliceHeader* slice_hdr,
               const H265Picture::Vector& ref_pic_list0,
               const H265Picture::Vector& ref_pic_list1,
               const H265Picture::Vector& ref_pic_set_lt_curr,
               const H265Picture::Vector& ref_pic_set_st_curr_after,
               const H265Picture::Vector& ref_pic_set_st_curr_before,
               scoped_refptr<H265Picture> pic,
               const uint8_t* data,
               size_t size,
               const std::vector<SubsampleEntry>& subsamples));
  MOCK_METHOD1(SubmitDecode, Status(scoped_refptr<H265Picture> pic));
  MOCK_METHOD1(OutputPicture, bool(scoped_refptr<H265Picture>));
  MOCK_METHOD2(SetStream,
               Status(base::span<const uint8_t> stream,
                      const DecryptConfig* decrypt_config));
  bool IsChromaSamplingSupported(VideoChromaSampling format) override {
    return format == VideoChromaSampling::k420;
  }
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
  // If |set_stream_expect| is true, it will setup EXPECT_CALL for SetStream.
  AcceleratedVideoDecoder::DecodeResult Decode(bool set_stream_expect = true);

 protected:
  std::unique_ptr<H265Decoder> decoder_;
  raw_ptr<MockH265Accelerator> accelerator_;

 private:
  base::queue<std::string> input_frame_files_;
  std::vector<uint8_t> bitstream_;
  scoped_refptr<DecoderBuffer> decoder_buffer_;
};

void H265DecoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH265Accelerator>();
  accelerator_ = mock_accelerator.get();
  decoder_.reset(new H265Decoder(std::move(mock_accelerator),
                                 VIDEO_CODEC_PROFILE_UNKNOWN));

  // Sets default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, CreateH265Picture()).WillByDefault(Invoke([]() {
    return new H265Picture();
  }));
  ON_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .WillByDefault(Return(H265Decoder::H265Accelerator::Status::kOk));
  ON_CALL(*accelerator_, SubmitDecode(_))
      .WillByDefault(Return(H265Decoder::H265Accelerator::Status::kOk));
  ON_CALL(*accelerator_, OutputPicture(_)).WillByDefault(Return(true));
  ON_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .With(Args<10, 11>(SubsampleSizeMatches()))
      .WillByDefault(Return(H265Decoder::H265Accelerator::Status::kOk));
  ON_CALL(*accelerator_, SetStream(_, _))
      .WillByDefault(
          Return(H265Decoder::H265Accelerator::Status::kNotSupported));
}

void H265DecoderTest::SetInputFrameFiles(
    const std::vector<std::string>& input_frame_files) {
  for (auto f : input_frame_files)
    input_frame_files_.push(f);
}

AcceleratedVideoDecoder::DecodeResult H265DecoderTest::Decode(
    bool set_stream_expect) {
  while (true) {
    auto result = decoder_->Decode();
    int32_t bitstream_id = 0;
    if (result != AcceleratedVideoDecoder::kRanOutOfStreamData ||
        input_frame_files_.empty())
      return result;
    auto input_file = GetTestDataFilePath(input_frame_files_.front());
    input_frame_files_.pop();
    CHECK(
        base::OptionalUnwrapTo(base::ReadFileToBytes(input_file), bitstream_));
    decoder_buffer_ = DecoderBuffer::CopyFrom(bitstream_);
    EXPECT_NE(decoder_buffer_.get(), nullptr);
    if (set_stream_expect)
      EXPECT_CALL(*accelerator_, SetStream(_, _));
    decoder_->SetStream(bitstream_id++, *decoder_buffer_);
  }
}

TEST_F(H265DecoderTest, DecodeSingleFrame) {
  SetInputFrameFiles({kSpsPps, kFrame0});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  // Also test running out of surfaces.
  EXPECT_CALL(*accelerator_, CreateH265Picture()).WillOnce(Return(nullptr));
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfSurfaces, Decode());
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&*accelerator_));

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(1);
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0))).Times(1);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0)));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, SkipNonIDRFrames) {
  SetInputFrameFiles({kSpsPps, kFrame1, kFrame2, kFrame0});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(1);
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0))).Times(1);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0)));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, DecodeProfileMain) {
  SetInputFrameFiles(
      {kSpsPps, kFrame0, kFrame1, kFrame2, kFrame3, kFrame4, kFrame5});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(6);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .Times(6);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .Times(6);

  Expectation decode_poc0, decode_poc1, decode_poc2, decode_poc3, decode_poc4,
      decode_poc8;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)));
    decode_poc4 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(4)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(2)));
    decode_poc1 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(1)));
    decode_poc3 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(3)));
    decode_poc8 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(8)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(1))).After(decode_poc1);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(3))).After(decode_poc3);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(4))).After(decode_poc4);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(8))).After(decode_poc8);
  }

  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, Decode10BitStream) {
  SetInputFrameFiles({k10BitFrame0, k10BitFrame1, k10BitFrame2, k10BitFrame3});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(gfx::Rect(320, 180), decoder_->GetVisibleRect());
  EXPECT_EQ(HEVCPROFILE_MAIN10, decoder_->GetProfile());
  EXPECT_EQ(10u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(4);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .Times(4);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .Times(4);

  // Two pictures will be kept in the DPB for reordering. The second and third
  // pictures should be outputted after feeding the fourth frame.
  Expectation decode_poc0, decode_poc1, decode_poc2, decode_poc3;
  {
    InSequence decode_order;
    decode_poc0 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)));
    decode_poc3 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(3)));
    decode_poc2 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(2)));
    decode_poc1 = EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(1)));
  }
  {
    InSequence display_order;
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0))).After(decode_poc0);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(1))).After(decode_poc1);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(2))).After(decode_poc2);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(3))).After(decode_poc3);
  }

  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, DenyDecodeNonYUV420) {
  // YUV444 frame causes kDecodeError.
  SetInputFrameFiles({kYUV444Frame});
  ASSERT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode());
}

TEST_F(H265DecoderTest, OutputPictureFailureCausesDecodeToFail) {
  // Provide enough data that Decode() will try to output a frame.
  SetInputFrameFiles({kSpsPps, kFrame0, kFrame1, kFrame2});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(3);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .Times(3);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .Times(3);
  EXPECT_CALL(*accelerator_, SubmitDecode(_)).Times(3);
  EXPECT_CALL(*accelerator_, OutputPicture(_)).WillRepeatedly(Return(false));
  EXPECT_EQ(AcceleratedVideoDecoder::kDecodeError, Decode());
}

// Verify that the decryption config is passed to the accelerator.
TEST_F(H265DecoderTest, SetEncryptedStream) {
  std::vector<uint8_t> bitstream1, bitstream2;
  auto input_file1 = GetTestDataFilePath(kSpsPps);
  CHECK(base::OptionalUnwrapTo(base::ReadFileToBytes(input_file1), bitstream1));
  auto input_file2 = GetTestDataFilePath(kFrame0);
  CHECK(base::OptionalUnwrapTo(base::ReadFileToBytes(input_file2), bitstream2));
  std::vector<uint8_t> bitstream = bitstream1;
  base::Extend(bitstream, bitstream2);

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
              SubmitFrameMetadata(_, _, _, _, _, _, _,
                                  DecryptConfigMatches(decrypt_config.get())))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kOk));
  EXPECT_CALL(*accelerator_,
              SubmitDecode(DecryptConfigMatches(decrypt_config.get())))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kOk));

  auto buffer = DecoderBuffer::CopyFrom(bitstream);
  ASSERT_NE(buffer.get(), nullptr);
  buffer->set_decrypt_config(std::move(decrypt_config));
  decoder_->SetStream(0, *buffer);
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, decoder_->Decode());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, decoder_->Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, SubmitFrameMetadataRetry) {
  SetInputFrameFiles({kSpsPps, kFrame0});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
        .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitFrameMetadata()
  // should be called again.
  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should proceed.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0)));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());

  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, SubmitSliceRetry) {
  SetInputFrameFiles({kSpsPps, kFrame0});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitSlice() should be
  // called again.
  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should proceed.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0)));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, SubmitDecodeRetry) {
  SetInputFrameFiles({kSpsPps, kFrame0, kFrame1});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)))
        .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Try again, assuming key still not set. Only SubmitDecode() should be
  // called again.
  EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(0);
  EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain));
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode());

  // Assume key has been provided now, next call to Decode() should output
  // the first frame.
  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0)));
    EXPECT_CALL(*accelerator_, CreateH265Picture());
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _));
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(4)));
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0)));
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(4)));
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, SetStreamRetry) {
  SetInputFrameFiles({kSpsPps, kFrame0});

  EXPECT_CALL(*accelerator_, SetStream(_, _))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kTryAgain))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kOk))
      .WillOnce(Return(H265Decoder::H265Accelerator::Status::kOk));
  EXPECT_EQ(AcceleratedVideoDecoder::kTryAgain, Decode(false));

  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode(false));
  EXPECT_EQ(gfx::Size(320, 184), decoder_->GetPicSize());
  EXPECT_EQ(HEVCPROFILE_MAIN, decoder_->GetProfile());
  EXPECT_EQ(8u, decoder_->GetBitDepth());
  EXPECT_EQ(17u, decoder_->GetRequiredNumOfPictures());

  {
    InSequence sequence;
    EXPECT_CALL(*accelerator_, CreateH265Picture()).Times(1);
    EXPECT_CALL(*accelerator_, SubmitFrameMetadata(_, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitSlice(_, _, _, _, _, _, _, _, _, _, _, _))
        .Times(1);
    EXPECT_CALL(*accelerator_, SubmitDecode(HasPoc(0))).Times(1);
    EXPECT_CALL(*accelerator_, OutputPicture(HasPoc(0))).Times(1);
  }
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode(false));
  EXPECT_TRUE(decoder_->Flush());
}

TEST_F(H265DecoderTest, DecodeMultiFrameInput) {
  SetInputFrameFiles({"bear.hevc"});
  EXPECT_EQ(AcceleratedVideoDecoder::kConfigChange, Decode());
  EXPECT_EQ(AcceleratedVideoDecoder::kRanOutOfStreamData, Decode());
  EXPECT_TRUE(decoder_->Flush());
}

}  // namespace media
