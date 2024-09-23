// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/av1_decoder.h"

#include <string.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/scoped_av_packet.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/gpu/av1_picture.h"
#include "media/media_buildflags.h"
#include "media/parsers/ivf_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/constants.h"
#include "third_party/libgav1/src/src/utils/types.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SaveArg;

namespace media {
namespace {

class FakeAV1Picture : public AV1Picture {
 public:
  FakeAV1Picture() = default;

 protected:
  ~FakeAV1Picture() override = default;

 private:
  scoped_refptr<AV1Picture> CreateDuplicate() override {
    return base::MakeRefCounted<FakeAV1Picture>();
  }
};

bool IsYUV420(int8_t subsampling_x, int8_t subsampling_y, bool is_monochrome) {
  return subsampling_x == 1 && subsampling_y == 1 && !is_monochrome;
}

MATCHER_P(SameAV1PictureInstance, av1_picture, "") {
  return &arg == av1_picture.get();
}

MATCHER_P2(MatchesFrameSizeAndRenderSize, frame_size, render_size, "") {
  const auto& frame_header = arg.frame_header;
  return base::strict_cast<int>(frame_header.width) == frame_size.width() &&
         base::strict_cast<int>(frame_header.height) == frame_size.height() &&
         base::strict_cast<int>(frame_header.render_width) ==
             render_size.width() &&
         base::strict_cast<int>(frame_header.render_height) ==
             render_size.height();
}

MATCHER_P4(MatchesFrameHeader,
           frame_size,
           render_size,
           show_existing_frame,
           show_frame,
           "") {
  const auto& frame_header = arg.frame_header;
  return base::strict_cast<int>(frame_header.width) == frame_size.width() &&
         base::strict_cast<int>(frame_header.height) == frame_size.height() &&
         base::strict_cast<int>(frame_header.render_width) ==
             render_size.width() &&
         base::strict_cast<int>(frame_header.render_height) ==
             render_size.height() &&
         frame_header.show_existing_frame == show_existing_frame &&
         frame_header.show_frame == show_frame;
}

MATCHER_P4(MatchesYUV420SequenceHeader,
           profile,
           bitdepth,
           max_frame_size,
           film_grain_params_present,
           "") {
  return arg.profile == profile && arg.color_config.bitdepth == bitdepth &&
         base::strict_cast<int>(arg.max_frame_width) ==
             max_frame_size.width() &&
         base::strict_cast<int>(arg.max_frame_height) ==
             max_frame_size.height() &&
         arg.film_grain_params_present == film_grain_params_present &&
         IsYUV420(arg.color_config.subsampling_x,
                  arg.color_config.subsampling_y,
                  arg.color_config.is_monochrome);
}

MATCHER(NonEmptyTileBuffers, "") {
  return !arg.empty();
}

MATCHER_P(MatchesFrameData, decoder_buffer, "") {
  return arg.data() == decoder_buffer->data() &&
         arg.size() == decoder_buffer->size();
}

class MockAV1Accelerator : public AV1Decoder::AV1Accelerator {
 public:
  MockAV1Accelerator() = default;
  ~MockAV1Accelerator() override = default;

  MOCK_METHOD1(CreateAV1Picture, scoped_refptr<AV1Picture>(bool));
  MOCK_METHOD5(SubmitDecode,
               Status(const AV1Picture&,
                      const libgav1::ObuSequenceHeader&,
                      const AV1ReferenceFrameVector&,
                      const libgav1::Vector<libgav1::TileBuffer>&,
                      base::span<const uint8_t>));
  MOCK_METHOD1(OutputPicture, bool(const AV1Picture&));
};

}  // namespace

class AV1DecoderTest : public ::testing::Test {
 public:
  using DecodeResult = AcceleratedVideoDecoder::DecodeResult;

  AV1DecoderTest() = default;
  ~AV1DecoderTest() override = default;
  void SetUp() override;
  std::vector<DecodeResult> Decode(scoped_refptr<DecoderBuffer> buffer);
  const libgav1::DecoderState* GetDecoderState() const;
  AV1ReferenceFrameVector& GetReferenceFrames() const;
  void Reset();
  scoped_refptr<DecoderBuffer> ReadDecoderBuffer(const std::string& fname);
  std::vector<scoped_refptr<DecoderBuffer>> ReadIVF(const std::string& fname);
  std::vector<scoped_refptr<DecoderBuffer>> ReadWebm(const std::string& fname);

 protected:
  base::FilePath GetTestFilePath(const std::string& fname) {
    base::FilePath file_path(
        base::FilePath(base::FilePath::kCurrentDirectory).AppendASCII(fname));
    if (base::PathExists(file_path)) {
      return file_path;
    }
    return GetTestDataFilePath(fname);
  }

  // Owned by |decoder_|.
  raw_ptr<MockAV1Accelerator, DanglingUntriaged> mock_accelerator_;

  std::unique_ptr<AV1Decoder> decoder_;
  int32_t bitstream_id_ = 0;
};

void AV1DecoderTest::SetUp() {
  auto accelerator = std::make_unique<MockAV1Accelerator>();
  mock_accelerator_ = accelerator.get();
  decoder_ = std::make_unique<AV1Decoder>(std::move(accelerator),
                                          VIDEO_CODEC_PROFILE_UNKNOWN);
}

std::vector<AcceleratedVideoDecoder::DecodeResult> AV1DecoderTest::Decode(
    scoped_refptr<DecoderBuffer> buffer) {
  if (buffer)
    decoder_->SetStream(bitstream_id_++, *buffer);

  std::vector<DecodeResult> results;
  DecodeResult res;
  do {
    res = decoder_->Decode();
    results.push_back(res);
  } while (res != DecodeResult::kDecodeError &&
           res != DecodeResult::kRanOutOfStreamData &&
           res != DecodeResult::kTryAgain);
  return results;
}

const libgav1::DecoderState* AV1DecoderTest::GetDecoderState() const {
  return decoder_->state_.get();
}

AV1ReferenceFrameVector& AV1DecoderTest::GetReferenceFrames() const {
  return decoder_->ref_frames_;
}

void AV1DecoderTest::Reset() {
  EXPECT_NE(decoder_->state_->current_frame_id, -1);
  EXPECT_TRUE(decoder_->parser_);
  EXPECT_EQ(decoder_->accelerator_.get(), mock_accelerator_);
  EXPECT_LT(base::checked_cast<AV1ReferenceFrameVector::size_type>(
                base::ranges::count(decoder_->ref_frames_, nullptr)),
            decoder_->ref_frames_.size());
  EXPECT_FALSE(decoder_->current_frame_header_);
  EXPECT_FALSE(decoder_->current_frame_);
  EXPECT_NE(decoder_->stream_id_, 0);
  EXPECT_TRUE(decoder_->stream_);
  EXPECT_GT(decoder_->stream_size_, 0u);

  decoder_->Reset();
  EXPECT_EQ(decoder_->state_->current_frame_id, -1);
  EXPECT_FALSE(decoder_->parser_);
  EXPECT_EQ(decoder_->accelerator_.get(), mock_accelerator_);
  EXPECT_EQ(base::checked_cast<AV1ReferenceFrameVector::size_type>(
                base::ranges::count(decoder_->ref_frames_, nullptr)),
            decoder_->ref_frames_.size());
  EXPECT_FALSE(decoder_->current_frame_header_);
  EXPECT_FALSE(decoder_->current_frame_);
  EXPECT_EQ(decoder_->stream_id_, 0);
  EXPECT_FALSE(decoder_->stream_);
  EXPECT_EQ(decoder_->stream_size_, 0u);
}

scoped_refptr<DecoderBuffer> AV1DecoderTest::ReadDecoderBuffer(
    const std::string& fname) {
  auto input_file = GetTestFilePath(fname);
  std::string bitstream;

  EXPECT_TRUE(base::ReadFileToString(input_file, &bitstream));
  auto buffer = DecoderBuffer::CopyFrom(base::as_byte_span(bitstream));
  EXPECT_TRUE(!!buffer);
  return buffer;
}

std::vector<scoped_refptr<DecoderBuffer>> AV1DecoderTest::ReadIVF(
    const std::string& fname) {
  std::string ivf_data;
  auto input_file = GetTestFilePath(fname);
  EXPECT_TRUE(base::ReadFileToString(input_file, &ivf_data));

  IvfParser ivf_parser;
  IvfFileHeader ivf_header{};
  EXPECT_TRUE(
      ivf_parser.Initialize(reinterpret_cast<const uint8_t*>(ivf_data.data()),
                            ivf_data.size(), &ivf_header));
  EXPECT_EQ(ivf_header.fourcc, /*AV01=*/0x31305641u);

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  IvfFrameHeader ivf_frame_header{};
  const uint8_t* data;
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &data)) {
    buffers.push_back(DecoderBuffer::CopyFrom(
        // TODO(crbug.com/40284755): `ParseNextFrame` should return a span.
        UNSAFE_TODO(base::span(data, ivf_frame_header.frame_size))));
  }
  return buffers;
}

std::vector<scoped_refptr<DecoderBuffer>> AV1DecoderTest::ReadWebm(
    const std::string& fname) {
  std::string webm_data;
  auto input_file = GetTestFilePath(fname);
  EXPECT_TRUE(base::ReadFileToString(input_file, &webm_data));

  InMemoryUrlProtocol protocol(
      reinterpret_cast<const uint8_t*>(webm_data.data()), webm_data.size(),
      false);
  FFmpegGlue glue(&protocol);
  LOG_ASSERT(glue.OpenContext());
  int stream_index = -1;
  for (unsigned int i = 0; i < glue.format_context()->nb_streams; ++i) {
    const AVStream* stream = glue.format_context()->streams[i];
    const AVCodecParameters* codec_parameters = stream->codecpar;
    const AVMediaType codec_type = codec_parameters->codec_type;
    const AVCodecID codec_id = codec_parameters->codec_id;
    if (codec_type == AVMEDIA_TYPE_VIDEO && codec_id == AV_CODEC_ID_AV1) {
      stream_index = i;
      break;
    }
  }
  EXPECT_NE(stream_index, -1) << "No AV1 data found in " << input_file;

  std::vector<scoped_refptr<DecoderBuffer>> buffers;
  auto packet = ScopedAVPacket::Allocate();
  while (av_read_frame(glue.format_context(), packet.get()) >= 0) {
    if (packet->stream_index == stream_index) {
      buffers.push_back(DecoderBuffer::CopyFrom(AVPacketData(*packet)));
    }
    av_packet_unref(packet.get());
  }
  return buffers;
}

TEST_F(AV1DecoderTest, DecodeInvalidOBU) {
  std::string kInvalidData = "ThisIsInvalidData";
  auto kInvalidBuffer =
      DecoderBuffer::CopyFrom(base::as_byte_span(kInvalidData));
  std::vector<DecodeResult> results = Decode(kInvalidBuffer);
  std::vector<DecodeResult> expected = {DecodeResult::kDecodeError};
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeEmptyOBU) {
  auto kEmptyBuffer = base::MakeRefCounted<DecoderBuffer>(0);
  std::vector<DecodeResult> results = Decode(kEmptyBuffer);
  std::vector<DecodeResult> expected = {DecodeResult::kRanOutOfStreamData};
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeOneIFrame) {
  constexpr gfx::Size kFrameSize(320, 240);
  constexpr gfx::Size kRenderSize(320, 240);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  const std::string kIFrame("av1-I-frame-320x240");
  scoped_refptr<DecoderBuffer> i_frame_buffer = ReadDecoderBuffer(kIFrame);
  ASSERT_TRUE(!!i_frame_buffer);
  auto av1_picture = base::MakeRefCounted<AV1Picture>();
  ::testing::InSequence s;
  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
      .WillOnce(Return(av1_picture));
  EXPECT_CALL(
      *mock_accelerator_,
      SubmitDecode(
          MatchesFrameHeader(kFrameSize, kRenderSize,
                             /*show_existing_frame=*/false,
                             /*show_frame=*/true),
          MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                      /*film_grain_params_present=*/false),
          _, NonEmptyTileBuffers(), MatchesFrameData(i_frame_buffer)))
      .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
  EXPECT_CALL(*mock_accelerator_,
              OutputPicture(SameAV1PictureInstance(av1_picture)))
      .WillOnce(Return(true));
  std::vector<DecodeResult> results = Decode(i_frame_buffer);
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange,
                                        DecodeResult::kRanOutOfStreamData};
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeSimpleStream) {
  constexpr gfx::Size kFrameSize(320, 240);
  constexpr gfx::Size kRenderSize(320, 240);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  const std::string kSimpleStream("bear-av1.webm");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadWebm(kSimpleStream);
  ASSERT_FALSE(buffers.empty());
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange};
  std::vector<DecodeResult> results;
  for (auto buffer : buffers) {
    ::testing::InSequence sequence;
    auto av1_picture = base::MakeRefCounted<AV1Picture>();
    EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
        .WillOnce(Return(av1_picture));
    EXPECT_CALL(
        *mock_accelerator_,
        SubmitDecode(
            MatchesFrameHeader(kFrameSize, kRenderSize,
                               /*show_existing_frame=*/false,
                               /*show_frame=*/true),
            MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                        /*film_grain_params_present=*/false),
            _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
        .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
    EXPECT_CALL(*mock_accelerator_,
                OutputPicture(SameAV1PictureInstance(av1_picture)))
        .WillOnce(Return(true));
    for (DecodeResult r : Decode(buffer))
      results.push_back(r);
    expected.push_back(DecodeResult::kRanOutOfStreamData);
    testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeShowExistingPictureStream) {
  constexpr gfx::Size kFrameSize(208, 144);
  constexpr gfx::Size kRenderSize(208, 144);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  constexpr size_t kDecodedFrames = 10;
  constexpr size_t kOutputFrames = 10;
  const std::string kShowExistingFrameStream("av1-show_existing_frame.ivf");
  std::vector<scoped_refptr<DecoderBuffer>> buffers =
      ReadIVF(kShowExistingFrameStream);
  ASSERT_FALSE(buffers.empty());

  // TODO(hiroh): Test what's unique about the show_existing_frame path.
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange};
  std::vector<DecodeResult> results;
  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
      .Times(kDecodedFrames)
      .WillRepeatedly(Return(base::MakeRefCounted<FakeAV1Picture>()));
  EXPECT_CALL(
      *mock_accelerator_,
      SubmitDecode(
          MatchesFrameSizeAndRenderSize(kFrameSize, kRenderSize),
          MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                      /*film_grain_params_present=*/false),
          _, NonEmptyTileBuffers(), _))
      .Times(kDecodedFrames)
      .WillRepeatedly(Return(AV1Decoder::AV1Accelerator::Status::kOk));
  EXPECT_CALL(*mock_accelerator_, OutputPicture(_))
      .Times(kOutputFrames)
      .WillRepeatedly(Return(true));

  for (auto buffer : buffers) {
    for (DecodeResult r : Decode(buffer))
      results.push_back(r);
    expected.push_back(DecodeResult::kRanOutOfStreamData);
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, Decode10bitStream) {
  const std::string k10bitStream("bear-av1-320x180-10bit.webm");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadWebm(k10bitStream);
  ASSERT_FALSE(buffers.empty());
  constexpr gfx::Size kFrameSize(320, 180);
  constexpr gfx::Size kRenderSize(320, 180);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange};
  std::vector<DecodeResult> results;
  for (auto buffer : buffers) {
    ::testing::InSequence sequence;
    auto av1_picture = base::MakeRefCounted<AV1Picture>();
    EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
        .WillOnce(Return(av1_picture));
    EXPECT_CALL(
        *mock_accelerator_,
        SubmitDecode(
            MatchesFrameHeader(kFrameSize, kRenderSize,
                               /*show_existing_frame=*/false,
                               /*show_frame=*/true),
            MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/10, kFrameSize,
                                        /*film_grain_params_present=*/false),
            _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
        .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
    EXPECT_CALL(*mock_accelerator_,
                OutputPicture(SameAV1PictureInstance(av1_picture)))
        .WillOnce(Return(true));
    for (DecodeResult r : Decode(buffer)) {
      results.push_back(r);
    }
    expected.push_back(DecodeResult::kRanOutOfStreamData);
    testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeTemporalLayerStream) {
  constexpr gfx::Size kFrameSize(640, 360);
  constexpr gfx::Size kRenderSize(640, 360);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  const std::string kTLStream("av1-svc-L1T2.ivf");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadIVF(kTLStream);
  ASSERT_FALSE(buffers.empty());
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange};
  std::vector<DecodeResult> results;
  for (auto buffer : buffers) {
    ::testing::InSequence sequence;
    auto av1_picture = base::MakeRefCounted<AV1Picture>();
    EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
        .WillOnce(Return(av1_picture));
    EXPECT_CALL(
        *mock_accelerator_,
        SubmitDecode(
            MatchesFrameHeader(kFrameSize, kRenderSize,
                               /*show_existing_frame=*/false,
                               /*show_frame=*/true),
            MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                        /*film_grain_params_present=*/false),
            _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
        .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
    EXPECT_CALL(*mock_accelerator_,
                OutputPicture(SameAV1PictureInstance(av1_picture)))
        .WillOnce(Return(true));
    for (DecodeResult r : Decode(buffer))
      results.push_back(r);
    expected.push_back(DecodeResult::kRanOutOfStreamData);
    testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, DecodeSVCStream) {
  const std::string kSVCStream("av1-svc-L2T2.ivf");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadIVF(kSVCStream);
  ASSERT_FALSE(buffers.empty());
  std::vector<DecodeResult> expected = {DecodeResult::kDecodeError};
  EXPECT_EQ(Decode(buffers[0]), expected);
  // Once AV1Decoder gets into an error state, Decode() returns kDecodeError
  // until Reset().
  EXPECT_EQ(Decode(buffers[1]), expected);
}

TEST_F(AV1DecoderTest, DenyDecodeNonYUV420) {
  const std::string kYUV444Stream("blackwhite_yuv444p-frame.av1.ivf");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadIVF(kYUV444Stream);
  ASSERT_EQ(buffers.size(), 1u);
  std::vector<DecodeResult> expected = {DecodeResult::kDecodeError};
  EXPECT_EQ(Decode(buffers[0]), expected);
  // Once AV1Decoder gets into an error state, Decode() returns kDecodeError
  // until Reset().
  EXPECT_EQ(Decode(buffers[0]), expected);
}

TEST_F(AV1DecoderTest, DecodeFilmGrain) {
  // Note: This video also contains show_existing_frame.
  const std::string kFilmGrainStream("av1-film_grain.ivf");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadIVF(kFilmGrainStream);
  ASSERT_FALSE(buffers.empty());
  constexpr size_t kDecodedFrames = 11;
  constexpr size_t kOutputFrames = 10;
  constexpr gfx::Size kFrameSize(352, 288);
  constexpr gfx::Size kRenderSize(352, 288);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange};
  std::vector<DecodeResult> results;

  // TODO(hiroh): test that CreateAV1Picture is called with the right parameter
  // which depends on the frame
  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(_))
      .Times(kDecodedFrames)
      .WillRepeatedly(Return(base::MakeRefCounted<FakeAV1Picture>()));
  EXPECT_CALL(
      *mock_accelerator_,
      SubmitDecode(
          MatchesFrameSizeAndRenderSize(kFrameSize, kRenderSize),
          MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                      /*film_grain_params_present=*/true),
          _, NonEmptyTileBuffers(), _))
      .Times(kDecodedFrames)
      .WillRepeatedly(Return(AV1Decoder::AV1Accelerator::Status::kOk));
  EXPECT_CALL(*mock_accelerator_, OutputPicture(_))
      .Times(kOutputFrames)
      .WillRepeatedly(Return(true));

  for (auto buffer : buffers) {
    for (DecodeResult r : Decode(buffer))
      results.push_back(r);
    expected.push_back(DecodeResult::kRanOutOfStreamData);
  }
  EXPECT_EQ(results, expected);
}

// TODO(b/175895249): Test in isolation each of the conditions that trigger a
// kConfigChange event.
TEST_F(AV1DecoderTest, ConfigChange) {
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  constexpr auto kMediaProfile = VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
  const std::string kSimpleStreams[] = {"bear-av1.webm",
                                        "bear-av1-480x360.webm"};
  constexpr gfx::Size kFrameSizes[] = {{320, 240}, {480, 360}};
  constexpr gfx::Size kRenderSizes[] = {{320, 240}, {480, 360}};
  std::vector<DecodeResult> expected;
  std::vector<DecodeResult> results;
  for (size_t i = 0; i < std::size(kSimpleStreams); ++i) {
    std::vector<scoped_refptr<DecoderBuffer>> buffers =
        ReadWebm(kSimpleStreams[i]);
    ASSERT_FALSE(buffers.empty());
    expected.push_back(DecodeResult::kConfigChange);
    for (auto buffer : buffers) {
      ::testing::InSequence sequence;
      auto av1_picture = base::MakeRefCounted<AV1Picture>();
      EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
          .WillOnce(Return(av1_picture));
      EXPECT_CALL(
          *mock_accelerator_,
          SubmitDecode(MatchesFrameHeader(kFrameSizes[i], kRenderSizes[i],
                                          /*show_existing_frame=*/false,
                                          /*show_frame=*/true),
                       MatchesYUV420SequenceHeader(
                           kProfile, /*bitdepth=*/8, kFrameSizes[i],
                           /*film_grain_params_present=*/false),
                       _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
          .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
      EXPECT_CALL(*mock_accelerator_,
                  OutputPicture(SameAV1PictureInstance(av1_picture)))
          .WillOnce(Return(true));
      for (DecodeResult r : Decode(buffer))
        results.push_back(r);
      expected.push_back(DecodeResult::kRanOutOfStreamData);
      EXPECT_EQ(decoder_->GetProfile(), kMediaProfile);
      EXPECT_EQ(decoder_->GetPicSize(), kFrameSizes[i]);
      EXPECT_EQ(decoder_->GetVisibleRect(), gfx::Rect(kRenderSizes[i]));
      EXPECT_EQ(decoder_->GetBitDepth(), 8u);
      testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
    }
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, Reset) {
  constexpr gfx::Size kFrameSize(320, 240);
  constexpr gfx::Size kRenderSize(320, 240);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  constexpr auto kMediaProfile = VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
  constexpr uint8_t kBitDepth = 8u;
  const std::string kSimpleStream("bear-av1.webm");
  std::vector<DecodeResult> expected;
  std::vector<DecodeResult> results;

  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadWebm(kSimpleStream);
  ASSERT_FALSE(buffers.empty());
  expected.push_back(DecodeResult::kConfigChange);
  for (int k = 0; k < 2; k++) {
    for (auto buffer : buffers) {
      ::testing::InSequence sequence;
      auto av1_picture = base::MakeRefCounted<AV1Picture>();
      EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
          .WillOnce(Return(av1_picture));
      EXPECT_CALL(
          *mock_accelerator_,
          SubmitDecode(
              MatchesFrameHeader(kFrameSize, kRenderSize,
                                 /*show_existing_frame=*/false,
                                 /*show_frame=*/true),
              MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                          /*film_grain_params_present=*/false),
              _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
          .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
      EXPECT_CALL(*mock_accelerator_,
                  OutputPicture(SameAV1PictureInstance(av1_picture)))
          .WillOnce(Return(true));
      for (DecodeResult r : Decode(buffer))
        results.push_back(r);
      expected.push_back(DecodeResult::kRanOutOfStreamData);
      EXPECT_EQ(decoder_->GetProfile(), kMediaProfile);
      EXPECT_EQ(decoder_->GetPicSize(), kFrameSize);
      EXPECT_EQ(decoder_->GetVisibleRect(), gfx::Rect(kRenderSize));
      EXPECT_EQ(decoder_->GetBitDepth(), kBitDepth);
      testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
    }

    Reset();
    // Ensures Reset() doesn't clear the stored stream states.
    EXPECT_EQ(decoder_->GetProfile(), kMediaProfile);
    EXPECT_EQ(decoder_->GetPicSize(), kFrameSize);
    EXPECT_EQ(decoder_->GetVisibleRect(), gfx::Rect(kRenderSize));
    EXPECT_EQ(decoder_->GetBitDepth(), kBitDepth);
  }
  EXPECT_EQ(results, expected);
}

TEST_F(AV1DecoderTest, ResetAndConfigChange) {
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  constexpr auto kMediaProfile = VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
  const std::string kSimpleStreams[] = {"bear-av1.webm",
                                        "bear-av1-480x360.webm"};
  constexpr gfx::Size kFrameSizes[] = {{320, 240}, {480, 360}};
  constexpr gfx::Size kRenderSizes[] = {{320, 240}, {480, 360}};
  constexpr uint8_t kBitDepth = 8u;
  std::vector<DecodeResult> expected;
  std::vector<DecodeResult> results;

  for (size_t i = 0; i < std::size(kSimpleStreams); ++i) {
    std::vector<scoped_refptr<DecoderBuffer>> buffers =
        ReadWebm(kSimpleStreams[i]);
    ASSERT_FALSE(buffers.empty());
    expected.push_back(DecodeResult::kConfigChange);
    for (auto buffer : buffers) {
      ::testing::InSequence sequence;
      auto av1_picture = base::MakeRefCounted<AV1Picture>();
      EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
          .WillOnce(Return(av1_picture));
      EXPECT_CALL(
          *mock_accelerator_,
          SubmitDecode(MatchesFrameHeader(kFrameSizes[i], kRenderSizes[i],
                                          /*show_existing_frame=*/false,
                                          /*show_frame=*/true),
                       MatchesYUV420SequenceHeader(
                           kProfile, /*bitdepth=*/8, kFrameSizes[i],
                           /*film_grain_params_present=*/false),
                       _, NonEmptyTileBuffers(), MatchesFrameData(buffer)))
          .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
      EXPECT_CALL(*mock_accelerator_,
                  OutputPicture(SameAV1PictureInstance(av1_picture)))
          .WillOnce(Return(true));
      for (DecodeResult r : Decode(buffer))
        results.push_back(r);
      expected.push_back(DecodeResult::kRanOutOfStreamData);
      EXPECT_EQ(decoder_->GetProfile(), kMediaProfile);
      EXPECT_EQ(decoder_->GetPicSize(), kFrameSizes[i]);
      EXPECT_EQ(decoder_->GetVisibleRect(), gfx::Rect(kRenderSizes[i]));
      EXPECT_EQ(decoder_->GetBitDepth(), kBitDepth);
      testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
    }

    Reset();
    // Ensures Reset() doesn't clear the stored stream states.
    EXPECT_EQ(decoder_->GetProfile(), kMediaProfile);
    EXPECT_EQ(decoder_->GetPicSize(), kFrameSizes[i]);
    EXPECT_EQ(decoder_->GetVisibleRect(), gfx::Rect(kRenderSizes[i]));
    EXPECT_EQ(decoder_->GetBitDepth(), kBitDepth);
  }
  EXPECT_EQ(results, expected);
}

// This test ensures that the AV1Decoder fails gracefully if for some reason,
// the reference frame state tracked by AV1Decoder becomes inconsistent with the
// state tracked by libgav1.
TEST_F(AV1DecoderTest, InconsistentReferenceFrameState) {
  const std::string kSimpleStream("bear-av1.webm");
  std::vector<scoped_refptr<DecoderBuffer>> buffers = ReadWebm(kSimpleStream);
  ASSERT_GE(buffers.size(), 2u);

  // In this test stream, the first frame is an intra frame and the second one
  // is not. Let's start by decoding the first frame and inspecting the
  // reference frame state.
  {
    ::testing::InSequence sequence;
    auto av1_picture = base::MakeRefCounted<AV1Picture>();
    EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
        .WillOnce(Return(av1_picture));

    AV1ReferenceFrameVector ref_frames;
    EXPECT_CALL(*mock_accelerator_,
                SubmitDecode(SameAV1PictureInstance(av1_picture), _, _, _, _))
        .WillOnce(DoAll(SaveArg<2>(&ref_frames),
                        Return(AV1Decoder::AV1Accelerator::Status::kOk)));
    EXPECT_CALL(*mock_accelerator_,
                OutputPicture(SameAV1PictureInstance(av1_picture)))
        .WillOnce(Return(true));

    // Before decoding, let's make sure that libgav1 doesn't think any reference
    // frames are valid.
    const libgav1::DecoderState* decoder_state = GetDecoderState();
    ASSERT_TRUE(decoder_state);
    EXPECT_EQ(base::ranges::count(decoder_state->reference_frame, nullptr),
              base::checked_cast<long>(decoder_state->reference_frame.size()));

    // And to be consistent, AV1Decoder should not be tracking any reference
    // frames yet.
    const AV1ReferenceFrameVector& internal_ref_frames = GetReferenceFrames();
    EXPECT_EQ(base::ranges::count(internal_ref_frames, nullptr),
              base::checked_cast<long>(internal_ref_frames.size()));

    // Now try to decode one frame and make sure that the frame is intra.
    std::vector<DecodeResult> expected = {DecodeResult::kConfigChange,
                                          DecodeResult::kRanOutOfStreamData};
    std::vector<DecodeResult> results = Decode(buffers[0]);
    EXPECT_EQ(results, expected);
    EXPECT_TRUE(libgav1::IsIntraFrame(av1_picture->frame_header.frame_type));

    // SubmitDecode() should have received the reference frames before they were
    // updated. That means that it should have received no reference frames
    // since this SubmitDecode() refers to the first frame.
    EXPECT_EQ(base::ranges::count(ref_frames, nullptr),
              base::checked_cast<long>(ref_frames.size()));

    // Now let's inspect the current state of things (which is after the
    // reference frames have been updated): libgav1 should have decided that all
    // reference frames are valid.
    ASSERT_TRUE(decoder_state);
    EXPECT_EQ(base::ranges::count(decoder_state->reference_frame, nullptr), 0);

    // And to be consistent, all the reference frames tracked by the AV1Decoder
    // should also be valid and they should be pointing to the only AV1Picture
    // so far.
    EXPECT_TRUE(base::ranges::all_of(
        internal_ref_frames,
        [&av1_picture](const scoped_refptr<AV1Picture>& ref_frame) {
          return ref_frame.get() == av1_picture.get();
        }));
    testing::Mock::VerifyAndClearExpectations(mock_accelerator_);
  }

  // Now we will purposefully mess up the reference frame state tracked by the
  // AV1Decoder by removing one of the reference frames. This should cause the
  // decode of the second frame to fail because the AV1Decoder should detect the
  // inconsistency.
  GetReferenceFrames()[1] = nullptr;
  auto av1_picture = base::MakeRefCounted<AV1Picture>();
  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
      .WillOnce(Return(av1_picture));
  std::vector<DecodeResult> expected = {DecodeResult::kDecodeError};
  std::vector<DecodeResult> results = Decode(buffers[1]);
  EXPECT_EQ(results, expected);

  // Just for rigor, let's check the state at the moment of failure. First, the
  // current frame should be an inter frame (and its header should have been
  // stored in the AV1Picture).
  EXPECT_EQ(av1_picture->frame_header.frame_type, libgav1::kFrameInter);

  // Next, let's check the reference frames that frame needs.
  for (int8_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; ++i)
    EXPECT_EQ(av1_picture->frame_header.reference_frame_index[i], i);

  // Finally, let's check that libgav1 thought that all the reference frames
  // were valid.
  const libgav1::DecoderState* decoder_state = GetDecoderState();
  ASSERT_TRUE(decoder_state);
  EXPECT_EQ(base::ranges::count(decoder_state->reference_frame, nullptr), 0);
}

TEST_F(AV1DecoderTest, TryAgainSubmitDecode) {
  constexpr gfx::Size kFrameSize(320, 240);
  constexpr gfx::Size kRenderSize(320, 240);
  constexpr auto kProfile = libgav1::BitstreamProfile::kProfile0;
  const std::string kIFrame("av1-I-frame-320x240");
  scoped_refptr<DecoderBuffer> i_frame_buffer = ReadDecoderBuffer(kIFrame);
  ASSERT_TRUE(!!i_frame_buffer);
  auto av1_picture = base::MakeRefCounted<AV1Picture>();
  ::testing::InSequence s;
  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(/*apply_grain=*/false))
      .WillOnce(Return(av1_picture));
  EXPECT_CALL(
      *mock_accelerator_,
      SubmitDecode(
          MatchesFrameHeader(kFrameSize, kRenderSize,
                             /*show_existing_frame=*/false,
                             /*show_frame=*/true),
          MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                      /*film_grain_params_present=*/false),
          _, NonEmptyTileBuffers(), MatchesFrameData(i_frame_buffer)))
      .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kTryAgain));
  EXPECT_CALL(*mock_accelerator_, OutputPicture(_)).Times(0);
  std::vector<DecodeResult> results = Decode(i_frame_buffer);
  std::vector<DecodeResult> expected = {DecodeResult::kConfigChange,
                                        DecodeResult::kTryAgain};
  EXPECT_EQ(results, expected);

  testing::Mock::VerifyAndClearExpectations(mock_accelerator_);

  // Now try again and have it succeed.
  EXPECT_CALL(
      *mock_accelerator_,
      SubmitDecode(
          MatchesFrameHeader(kFrameSize, kRenderSize,
                             /*show_existing_frame=*/false,
                             /*show_frame=*/true),
          MatchesYUV420SequenceHeader(kProfile, /*bitdepth=*/8, kFrameSize,
                                      /*film_grain_params_present=*/false),
          _, NonEmptyTileBuffers(), MatchesFrameData(i_frame_buffer)))
      .WillOnce(Return(AV1Decoder::AV1Accelerator::Status::kOk));
  EXPECT_CALL(*mock_accelerator_,
              OutputPicture(SameAV1PictureInstance(av1_picture)))
      .WillOnce(Return(true));
  results = Decode(nullptr);
  expected = {DecodeResult::kRanOutOfStreamData};
  EXPECT_EQ(results, expected);
}

// This test verifies that AV1 videos which are encoded using reference frame
// scaling can be decoded completely. Reference frame scaling allows resolution
// changes in a video stream without requiring a key frame. Please refer to
// the chromium bug https://issues.chromium.org/issues/338251332 for details.
TEST_F(AV1DecoderTest, DecodeWithFrameSizeChange) {
  // The AV1 test video has three different frame sizes.
  // It starts out with width and height of 1920 x 1080 (100 frames). The
  // video is then scaled down to 1280 x 720 (100 frames) and eventually scaled
  // down again to 960 x 540 (100 frames).
  constexpr gfx::Size kFrameSize(1920, 1080);
  constexpr gfx::Size kRenderSize(1920, 1080);
  constexpr int kOriginalFrameSizeCount = 100;

  constexpr gfx::Size kNewFrameSize1(1280, 720);
  constexpr gfx::Size kNewRenderSize1(1280, 720);
  constexpr int kNewFrameSize1Count = 100;

  constexpr gfx::Size kNewFrameSize2(960, 540);
  constexpr gfx::Size kNewRenderSize2(960, 540);
  constexpr int kNewFrameSize2Count = 100;

  // The number of buffers to be decoded.
  constexpr size_t kExpectedBuffers = 300;

  std::vector<scoped_refptr<DecoderBuffer>> buffers =
      ReadIVF("reference-frame-scaling-test.ivf");
  EXPECT_EQ(buffers.size(), kExpectedBuffers);

  auto av1_picture = base::MakeRefCounted<AV1Picture>();

  EXPECT_CALL(*mock_accelerator_, CreateAV1Picture(_))
      .Times(buffers.size())
      .WillRepeatedly(Return(av1_picture));

  // Set up three sets of expectations for the expected frame and render sizes
  // as defined above.
  EXPECT_CALL(*mock_accelerator_, SubmitDecode(MatchesFrameSizeAndRenderSize(
                                                   kFrameSize, kRenderSize),
                                               _, _, _, _))
      .Times(kOriginalFrameSizeCount)
      .WillRepeatedly(Return(AV1Decoder::AV1Accelerator::Status::kOk));

  EXPECT_CALL(*mock_accelerator_,
              SubmitDecode(MatchesFrameSizeAndRenderSize(kNewFrameSize1,
                                                         kNewRenderSize1),
                           _, _, _, _))
      .Times(kNewFrameSize1Count)
      .WillRepeatedly(Return(AV1Decoder::AV1Accelerator::Status::kOk));

  EXPECT_CALL(*mock_accelerator_,
              SubmitDecode(MatchesFrameSizeAndRenderSize(kNewFrameSize2,
                                                         kNewRenderSize2),
                           _, _, _, _))
      .Times(kNewFrameSize2Count)
      .WillRepeatedly(Return(AV1Decoder::AV1Accelerator::Status::kOk));

  EXPECT_CALL(*mock_accelerator_, OutputPicture(MatchesFrameSizeAndRenderSize(
                                      kFrameSize, kRenderSize)))
      .Times(kOriginalFrameSizeCount)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_accelerator_, OutputPicture(MatchesFrameSizeAndRenderSize(
                                      kNewFrameSize1, kNewRenderSize1)))
      .Times(kNewFrameSize1Count)
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_accelerator_, OutputPicture(MatchesFrameSizeAndRenderSize(
                                      kNewFrameSize2, kNewRenderSize2)))
      .Times(kNewFrameSize2Count)
      .WillRepeatedly(Return(true));

  std::vector<DecodeResult> results;

  for (size_t i = 0; i < buffers.size(); ++i) {
    auto buffer_results = Decode(buffers[i]);
    results.insert(results.end(), buffer_results.begin(), buffer_results.end());
  }

  // Verify that we don't have any decoding errors.
  EXPECT_THAT(results,
              testing::Not(testing::Contains(DecodeResult::kDecodeError)));
}

// TODO(hiroh): Add more tests: reference frame tracking, render size change,
// profile change, bit depth change, render size different than the frame size,
// visible rectangle change in the middle of video sequence, reset while waiting
// for buffers, flushing.
}  // namespace media
