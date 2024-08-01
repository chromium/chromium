// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/android/media_codec_bridge_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/extend.h"
#include "base/containers/to_vector.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/video_frame.h"
#include "media/parsers/h264_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

using testing::IsNull;
using testing::NotNull;

namespace {

// The first frame of
// http://www.html5rocks.com/en/tutorials/audio/quick/test.mp3
unsigned char test_mp3[] = {
    0xff, 0xfb, 0xd2, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0d, 0x20, 0x00, 0x00, 0x00, 0x2a, 0x7e, 0x40,
    0xc0, 0x19, 0x4a, 0x80, 0x0d, 0x60, 0x48, 0x1b, 0x40, 0xf7, 0xbd, 0xb9,
    0xd9, 0x40, 0x6f, 0x82, 0x01, 0x8b, 0x17, 0xa0, 0x80, 0xc5, 0x01, 0xad,
    0x9a, 0xd3, 0x00, 0x12, 0xc0, 0x72, 0x93, 0x67, 0xd0, 0x03, 0x6f, 0xa4,
    0xc0, 0xc3, 0x23, 0xee, 0x9b, 0xc0, 0xcc, 0x02, 0xa0, 0xa1, 0x30, 0x0c,
    0x52, 0x2d, 0xfd, 0x6e, 0x08, 0x83, 0x60, 0x40, 0x46, 0x06, 0x4b, 0x20,
    0x82, 0x82, 0x7f, 0xd4, 0x81, 0xe7, 0x00, 0x64, 0x20, 0x18, 0xec, 0xc2,
    0x06, 0x57, 0x0f, 0x81, 0x93, 0x0b, 0x00, 0x66, 0xe3, 0xb7, 0xe8, 0x32,
    0x6e, 0xf0, 0x32, 0xb0, 0x58, 0x0c, 0x7c, 0x3a, 0x03, 0x22, 0x14, 0x80,
    0xc9, 0x01, 0x80, 0x30, 0x20, 0x14, 0x0c, 0x96, 0x73, 0xfe, 0x9f, 0x6c,
    0x0c, 0xd2, 0x25, 0x0f, 0xdc, 0x0c, 0x32, 0x43, 0x03, 0x27, 0x87, 0xc0,
    0xc2, 0xc0, 0x20, 0xfc, 0x42, 0xc5, 0xff, 0xff, 0xd4, 0x80, 0x01, 0x01,
    0x80, 0xc3, 0x81, 0x01, 0x95, 0x03, 0x28, 0x82, 0xc0, 0xc3, 0x01, 0xa1,
    0x06, 0x81, 0x87, 0xc2, 0x40, 0x64, 0xc1, 0xf0, 0x12, 0x02, 0xff, 0xf6,
    0x5b, 0x9f, 0x44, 0xdc, 0xdd, 0x0b, 0x38, 0x59, 0xe0, 0x31, 0x71, 0x60,
    0x0c, 0xb4, 0x22, 0x03, 0x3b, 0x96, 0x40, 0xc8, 0x63, 0x90, 0x0a, 0x23,
    0x81, 0x9e, 0x4c, 0x20, 0x65, 0xb3, 0x18, 0x19, 0x6c, 0x42, 0x06, 0x36,
    0x1d, 0x01, 0x90, 0x87, 0xdf, 0xff, 0xd0, 0x65, 0xa6, 0xea, 0x66, 0xfd,
    0x40, 0x0c, 0x48, 0x03, 0x1a, 0x09, 0x01, 0x21, 0x98, 0x19, 0x2c, 0x36,
    0x06, 0x43, 0x21, 0x81, 0x92, 0xca, 0x60, 0x64, 0x70, 0xb8, 0x19, 0x20,
    0x6c, 0x02, 0x83, 0x80, 0xcb, 0x60, 0x65, 0x32, 0x28, 0x18, 0x64, 0x24,
    0x06, 0x3a, 0x0c, 0x00, 0xe1, 0x00, 0x18, 0xd0, 0x35, 0xff, 0xff, 0xff,
    0xe8, 0x32, 0xef, 0xb2, 0x90, 0x65, 0xbb, 0xdd, 0x94, 0x82, 0x0b, 0x4c,
    0xfa, 0x25, 0xf3, 0x74, 0x13, 0x0f, 0xf8, 0x19, 0x28, 0x84, 0x06, 0x36,
    0x11, 0x01, 0x20, 0x80, 0x18, 0xb4, 0x52, 0x0e, 0x15, 0x00, 0x30, 0x50,
    0x0c, 0x84, 0x32, 0x03, 0x11, 0x04, 0x03, 0x48, 0x04, 0x00, 0x00, 0x31,
    0x21, 0x00, 0x0c, 0x84, 0x18, 0x03, 0x07, 0x85, 0x40, 0xc6, 0xa5, 0x70,
    0x32, 0xb8, 0x7c, 0x0c, 0x54, 0x04, 0x00, 0xd0, 0x08, 0x59, 0x58, 0x18,
    0x20, 0x14, 0x06, 0x30, 0x30, 0x01, 0x9b, 0x86, 0x00, 0x6b, 0x54, 0xa8,
    0x19, 0x8c, 0x2a, 0x06, 0x16, 0x09, 0x01, 0xa0, 0xd0, 0xa0, 0x69, 0x74,
    0xb8, 0x19, 0xc4, 0x4a, 0xa3, 0xda, 0x9d, 0x1e, 0x4f, 0x05, 0xc0, 0x5b,
    0x0b, 0x03, 0xc2, 0x76, 0xa3, 0x4f, 0xb9, 0x16, 0xc2, 0x70, 0x41, 0x07,
    0xa0, 0x84, 0x16, 0x38, 0x4a, 0xc8, 0xaf, 0xee, 0x7f, 0x93, 0xb5, 0x5c,
    0x39, 0x1e, 0x29, 0xd9, 0x8c, 0x80, 0xb5, 0x80, 0xe6, 0x85, 0xb2, 0x99,
    0x68, 0x85, 0x46, 0x91, 0x60, 0xdb, 0x06, 0xfa, 0x38, 0x7a, 0xc7, 0xac,
    0x85, 0xa8, 0xd3, 0xe6, 0x99, 0x3b, 0x66, 0x43, 0x23, 0x1f, 0x84, 0xe1,
    0x65, 0x5e, 0xbc, 0x84, 0x18, 0x62, 0xe6, 0x42, 0x0b, 0x82, 0xe4, 0xd3,
    0x42, 0xd2, 0x05, 0x81, 0x4e, 0xe4, 0x9f, 0x8c, 0xc8, 0x7f, 0xa3, 0xe0,
    0x8d, 0xf1, 0x0f, 0x38, 0xe5, 0x3f, 0xc4, 0x2c, 0x24, 0x65, 0x8d, 0xb9,
    0x58, 0xac, 0x39, 0x0e, 0x37, 0x99, 0x2e, 0x85, 0xe0, 0xb7, 0x98, 0x41,
    0x20, 0x38, 0x1b, 0x95, 0x07, 0xfa, 0xa8, 0x9c, 0x21, 0x0f, 0x13, 0x8c,
    0xa5, 0xc1, 0x76, 0xae, 0x0b, 0xc1, 0x30, 0x27, 0x08, 0xc1, 0xf6, 0x4d,
    0xce, 0xb4, 0x41, 0x38, 0x1e, 0x82, 0x10, 0x74, 0x45, 0x91, 0x90, 0xff,
    0x41, 0x8b, 0x62, 0x1a, 0x71, 0xb6, 0x45, 0x63, 0x8c, 0xce, 0xb8, 0x54,
    0x1b, 0xe8, 0x5d, 0x9e, 0x35, 0x9d, 0x6c, 0xac, 0xe8, 0x83, 0xa1, 0xe9,
    0x3f, 0x13, 0x74, 0x11, 0x04, 0x10, 0xf1, 0x37, 0x38, 0xc6, 0x00, 0x60,
    0x27, 0x48, 0x38, 0x85, 0x92, 0x76, 0xb7, 0xf3, 0xa7, 0x1c, 0x4b, 0xf9,
    0x3b, 0x5a, 0x88, 0xac, 0x60, 0x1b, 0x85, 0x81, 0x16, 0xab, 0x44, 0x17,
    0x08, 0x2e, 0x0f, 0xd4, 0xe2, 0xde, 0x49, 0xc9, 0xe1, 0xc0, 0xc0, 0xa0,
    0x7e, 0x73, 0xa1, 0x67, 0xf8, 0xf5, 0x9f, 0xc4, 0x21, 0x50, 0x4f, 0x05,
    0x2c, 0xfc, 0x5c, 0xaa, 0x85, 0xb0, 0xfa, 0x67, 0x80, 0x7e, 0x0f, 0xfd,
    0x92, 0x30, 0xd5, 0xa0, 0xd4, 0x05, 0xdd, 0x06, 0x68, 0x1d, 0x6e, 0x4e,
    0x8b, 0x79, 0xd6, 0xfc, 0xff, 0x2e, 0x6e, 0x7c, 0xba, 0x03, 0x90, 0xd4,
    0x25, 0x65, 0x8e, 0xe7, 0x3a, 0xd1, 0xd6, 0xdc, 0xf0, 0xbe, 0x12, 0xc4,
    0x31, 0x08, 0x16, 0x70, 0x31, 0x85, 0x61, 0x38, 0x27, 0x0a, 0x91, 0x5f,
    0x03, 0x38, 0xeb, 0x37, 0x13, 0x48, 0x41, 0xbe, 0x7f, 0x04, 0x70, 0x62,
    0x2b, 0x15, 0x91, 0x67, 0x63, 0x4f, 0xad, 0xa7, 0x1d, 0x3f, 0x44, 0x17,
    0x02, 0x08, 0x0d, 0xf2, 0xfc, 0x03, 0xa0, 0x74, 0x21, 0x8b, 0x07, 0x3a,
    0x8d, 0x0f, 0x54, 0x58, 0x94, 0x12, 0xc5, 0x62, 0x18, 0xb9, 0x42, 0xf0,
    0x6c, 0x73, 0xa0, 0x92, 0xad, 0x27, 0x1c, 0x20, 0x0f, 0xc1, 0xca, 0x44,
    0x87, 0x47, 0xc5, 0x43, 0x23, 0x01, 0xda, 0x23, 0xe2, 0x89, 0x38, 0x9f,
    0x1f, 0x8d, 0x8c, 0xc6, 0x95, 0xa3, 0x34, 0x21, 0x21, 0x2d, 0x49, 0xea,
    0x4b, 0x05, 0x85, 0xf5, 0x58, 0x25, 0x13, 0xcd, 0x51, 0x19, 0x1a, 0x88,
    0xa6, 0x83, 0xd6, 0xd0, 0xbc, 0x25, 0x19, 0x1c, 0x92, 0x12, 0x44, 0x5d,
    0x1c, 0x04, 0xf1, 0x99, 0xdf, 0x92, 0x8e, 0x09, 0x85, 0xf3, 0x88, 0x82,
    0x4c, 0x22, 0x17, 0xc5, 0x25, 0x23, 0xed, 0x78, 0xf5, 0x41, 0xd1, 0xe9,
    0x8a, 0xb3, 0x52, 0xd1, 0x3d, 0x79, 0x81, 0x4d, 0x31, 0x24, 0xf9, 0x38,
    0x96, 0xbc, 0xf4, 0x8c, 0x25, 0xe9, 0xf2, 0x73, 0x94, 0x85, 0xc2, 0x61,
    0x6a, 0x34, 0x68, 0x65, 0x78, 0x87, 0xa6, 0x4f};
static const size_t kDecodedAudioLengthInBytes = 9216u;

}  // namespace

namespace media {

#define SKIP_TEST_IF_HW_H264_IS_NOT_AVAILABLE()                        \
  do {                                                                 \
    if (!MediaCodecUtil::IsH264EncoderAvailable()) {                   \
      VLOG(0) << "Could not run test - h264 not supported on device."; \
      return;                                                          \
    }                                                                  \
  } while (0)

enum PixelFormat {
  // Subset of MediaCodecInfo.CodecCapabilities.
  COLOR_FORMAT_YUV420_PLANAR = 19,
  COLOR_FORMAT_YUV420_SEMIPLANAR = 21,
};

static const int kPresentationTimeBase = 100;
static const int kMaxInputPts = kPresentationTimeBase + 2;

static inline const base::TimeDelta InfiniteTimeOut() {
  return base::Microseconds(-1);
}

void DecodeMediaFrame(MediaCodecBridge* media_codec,
                      const uint8_t* data,
                      size_t data_size,
                      const base::TimeDelta input_presentation_timestamp,
                      const base::TimeDelta initial_timestamp_lower_bound) {
  base::TimeDelta input_pts = input_presentation_timestamp;
  base::TimeDelta timestamp = initial_timestamp_lower_bound;
  base::TimeDelta new_timestamp;
  for (int i = 0; i < 10; ++i) {
    int input_buf_index = -1;
    MediaCodecResult result =
        media_codec->DequeueInputBuffer(InfiniteTimeOut(), &input_buf_index);
    ASSERT_TRUE(result.is_ok());

    media_codec->QueueInputBuffer(input_buf_index, data, data_size,
                                  input_presentation_timestamp);

    size_t unused_offset = 0;
    size_t size = 0;
    bool eos = false;
    int output_buf_index = -1;
    result = media_codec->DequeueOutputBuffer(
        InfiniteTimeOut(), &output_buf_index, &unused_offset, &size,
        &new_timestamp, &eos, nullptr);

    if (result.is_ok() && output_buf_index > 0) {
      media_codec->ReleaseOutputBuffer(output_buf_index, false);
    }
    // Output time stamp should not be smaller than old timestamp.
    ASSERT_TRUE(new_timestamp >= timestamp);
    input_pts += base::Microseconds(33000);
    timestamp = new_timestamp;
  }
}

// Performs basic, codec-specific sanity checks on the encoded H264 frame:
// - as to key frames, correct sequences of H.264 NALUs (SPS before PPS and
//   before slices).
// - as to non key frames, contain no SPS/PPS infront.
void H264Validate(const uint8_t* frame, size_t size) {
  H264Parser h264_parser;
  h264_parser.SetStream(frame, static_cast<off_t>(size));
  bool seen_sps = false;
  bool seen_pps = false;

  while (1) {
    H264NALU nalu;
    H264Parser::Result result;

    result = h264_parser.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;
    ASSERT_THAT(result, H264Parser::kOk);

    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice: {
        ASSERT_TRUE(seen_sps);
        ASSERT_TRUE(seen_pps);
        break;
      }

      case H264NALU::kNonIDRSlice: {
        ASSERT_FALSE(seen_sps);
        ASSERT_FALSE(seen_pps);
        break;
      }

      case H264NALU::kSPS: {
        int sps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser.ParseSPS(&sps_id));
        seen_sps = true;
        break;
      }

      case H264NALU::kPPS: {
        ASSERT_TRUE(seen_sps);
        int pps_id;
        ASSERT_EQ(H264Parser::kOk, h264_parser.ParsePPS(&pps_id));
        seen_pps = true;
        break;
      }

      default:
        break;
    }
  }
}

void EncodeMediaFrame(MediaCodecBridge* media_codec,
                      const uint8_t* src_data,
                      const int width,
                      const int height,
                      const base::TimeDelta input_timestamp) {
  int input_buf_index = -1;
  MediaCodecResult result =
      media_codec->DequeueInputBuffer(InfiniteTimeOut(), &input_buf_index);
  ASSERT_TRUE(result.is_ok());

  uint8_t* buffer = nullptr;
  size_t capacity = 0;
  result = media_codec->GetInputBuffer(input_buf_index, &buffer, &capacity);
  ASSERT_TRUE(result.is_ok());

  int stride, yplane_height;
  gfx::Size encoded_size;
  result = media_codec->GetInputFormat(&stride, &yplane_height, &encoded_size);
  ASSERT_TRUE(result.is_ok());

  const gfx::Size uv_plane_size = VideoFrame::PlaneSizeInSamples(
      PIXEL_FORMAT_NV12, VideoFrame::Plane::kUV, encoded_size);
  const size_t src_size =
      // size of Y-plane plus padding till UV-plane
      stride * yplane_height +
      // size of all UV-plane lines but the last one
      (uv_plane_size.height() - 1) * stride +
      // size of the very last line in UV-plane (it's not padded to full stride)
      uv_plane_size.width() * 2;
  ASSERT_LE(src_size, capacity);

  // Convert to NV12 because H264 encoder is created with color format
  // COLOR_FormatYUV420SemiPlanar, both in main code path and unittest here.
  bool converted =
      !libyuv::I420ToNV12(src_data, width, src_data + width * height, width / 2,
                          src_data + width * height * 5 / 4, width / 2, buffer,
                          stride, buffer + stride * yplane_height, stride,
                          encoded_size.width(), encoded_size.height());
  ASSERT_TRUE(converted);

  result = media_codec->QueueInputBuffer(input_buf_index, nullptr, src_size,
                                         input_timestamp);
  ASSERT_TRUE(result.is_ok());

  int32_t buf_index = -1;
  size_t offset = 0;
  size_t output_size;
  bool key_frame = false;
  do {
    result = media_codec->DequeueOutputBuffer(InfiniteTimeOut(), &buf_index,
                                              &offset, &output_size, nullptr,
                                              nullptr, &key_frame);
    EXPECT_NE(result.code(), MediaCodecResult::Codes::kError);
  } while (buf_index < 0);
  ASSERT_TRUE(result.is_ok() && buf_index >= 0);

  std::unique_ptr<uint8_t[]> output_data =
      std::make_unique<uint8_t[]>(output_size);
  result = media_codec->CopyFromOutputBuffer(buf_index, offset,
                                             output_data.get(), output_size);
  ASSERT_TRUE(result.is_ok());

  H264Validate(output_data.get(), output_size);

  media_codec->ReleaseOutputBuffer(buf_index, false);
}

AudioDecoderConfig NewAudioConfig(
    AudioCodec codec,
    std::vector<uint8_t> extra_data = std::vector<uint8_t>(),
    base::TimeDelta seek_preroll = base::TimeDelta(),
    int64_t codec_delay = 0) {
  AudioDecoderConfig config;
  config.Initialize(codec, kSampleFormatPlanarF32, CHANNEL_LAYOUT_STEREO, 44100,
                    extra_data, EncryptionScheme::kUnencrypted, seek_preroll,
                    codec_delay);
  return config;
}

TEST(MediaCodecBridgeTest, CreateH264Decoder) {
  VideoCodecConfig config;
  config.codec = VideoCodec::kH264;
  config.codec_type = CodecType::kAny;
  config.initial_expected_coded_size = gfx::Size(640, 480);

  MediaCodecBridgeImpl::CreateVideoDecoder(config);
}

TEST(MediaCodecBridgeTest, DoNormal) {
  std::unique_ptr<media::MediaCodecBridge> media_codec =
      MediaCodecBridgeImpl::CreateAudioDecoder(NewAudioConfig(AudioCodec::kMP3),
                                               nullptr);
  ASSERT_THAT(media_codec, NotNull());

  int input_buf_index = -1;
  MediaCodecResult result =
      media_codec->DequeueInputBuffer(InfiniteTimeOut(), &input_buf_index);
  ASSERT_TRUE(result.is_ok());
  ASSERT_GE(input_buf_index, 0);

  int64_t input_pts = kPresentationTimeBase;
  media_codec->QueueInputBuffer(input_buf_index, test_mp3, sizeof(test_mp3),
                                base::Microseconds(++input_pts));

  result = media_codec->DequeueInputBuffer(InfiniteTimeOut(), &input_buf_index);
  media_codec->QueueInputBuffer(input_buf_index, test_mp3, sizeof(test_mp3),
                                base::Microseconds(++input_pts));

  result = media_codec->DequeueInputBuffer(InfiniteTimeOut(), &input_buf_index);
  media_codec->QueueEOS(input_buf_index);

  input_pts = kPresentationTimeBase;
  bool eos = false;
  size_t total_size = 0;
  while (!eos) {
    size_t unused_offset = 0;
    size_t size = 0;
    base::TimeDelta timestamp;
    int output_buf_index = -1;
    result = media_codec->DequeueOutputBuffer(InfiniteTimeOut(),
                                              &output_buf_index, &unused_offset,
                                              &size, &timestamp, &eos, nullptr);
    switch (result.code()) {
      case MediaCodecResult::Codes::kTryAgainLater:
        FAIL();

      case MediaCodecResult::Codes::kOutputFormatChanged:
        continue;

      case MediaCodecResult::Codes::kOutputBuffersChanged:
        continue;

      default:
        break;
    }
    ASSERT_GE(output_buf_index, 0);
    EXPECT_LE(1u, size);
    total_size += size;
  }
  EXPECT_EQ(kDecodedAudioLengthInBytes, total_size);
  ASSERT_LE(input_pts, kMaxInputPts);
}

TEST(MediaCodecBridgeTest, InvalidVorbisHeader) {
  // The first byte of the header is not 0x02.
  std::vector<uint8_t> invalid_first_byte = {{0x00, 0xff, 0xff, 0xff, 0xff}};
  ASSERT_THAT(
      MediaCodecBridgeImpl::CreateAudioDecoder(
          NewAudioConfig(AudioCodec::kVorbis, invalid_first_byte), nullptr),
      IsNull());

  // Size of the header is too large.
  size_t large_size = 8 * 1024 * 1024 + 2;
  std::vector<uint8_t> large_header(large_size, 0xff);
  large_header.front() = 0x02;
  large_header.back() = 0xfe;
  ASSERT_THAT(MediaCodecBridgeImpl::CreateAudioDecoder(
                  NewAudioConfig(AudioCodec::kVorbis, large_header), nullptr),
              IsNull());
}

TEST(MediaCodecBridgeTest, InvalidOpusHeader) {
  std::vector<uint8_t> dummy_extra_data = {{0, 0}};

  // Codec Delay is < 0.
  ASSERT_THAT(MediaCodecBridgeImpl::CreateAudioDecoder(
                  NewAudioConfig(AudioCodec::kOpus, dummy_extra_data,
                                 base::TimeDelta(), -1),
                  nullptr),
              IsNull());

  // Seek Preroll is < 0.
  ASSERT_THAT(MediaCodecBridgeImpl::CreateAudioDecoder(
                  NewAudioConfig(AudioCodec::kOpus, dummy_extra_data,
                                 base::Microseconds(-1)),
                  nullptr),
              IsNull());
}

TEST(MediaCodecBridgeTest, PresentationTimestampsDoNotDecrease) {
  if (!MediaCodecUtil::IsVp8DecoderAvailable()) {
    VLOG(0) << "Could not run test - VP8 not supported on device.";
    return;
  }

  VideoCodecConfig config;
  config.codec = VideoCodec::kVP8;
  config.codec_type = CodecType::kAny;
  config.initial_expected_coded_size = gfx::Size(320, 240);

  auto media_codec = MediaCodecBridgeImpl::CreateVideoDecoder(config);
  ASSERT_THAT(media_codec, NotNull());
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("vp8-I-frame-320x240");
  DecodeMediaFrame(media_codec.get(), buffer->data(), buffer->size(),
                   base::TimeDelta(), base::TimeDelta());

  // Simulate a seek to 10 seconds, and each chunk has 2 I-frames.
  std::vector<uint8_t> chunk = base::ToVector(base::span(*buffer));
  base::Extend(chunk, base::span(*buffer));
  media_codec->Flush();
  DecodeMediaFrame(media_codec.get(), &chunk[0], chunk.size(),
                   base::Microseconds(10000000), base::Microseconds(9900000));

  // Simulate a seek to 5 seconds.
  media_codec->Flush();
  DecodeMediaFrame(media_codec.get(), &chunk[0], chunk.size(),
                   base::Microseconds(5000000), base::Microseconds(4900000));
}

TEST(MediaCodecBridgeTest, CreateUnsupportedCodec) {
  EXPECT_THAT(MediaCodecBridgeImpl::CreateAudioDecoder(
                  NewAudioConfig(AudioCodec::kUnknown), nullptr),
              IsNull());

  VideoCodecConfig config;
  config.codec = VideoCodec::kUnknown;
  config.codec_type = CodecType::kAny;
  config.initial_expected_coded_size = gfx::Size(320, 240);
  EXPECT_THAT(MediaCodecBridgeImpl::CreateVideoDecoder(config), IsNull());
}

// Test MediaCodec HW H264 encoding and validate the format of encoded frames.
TEST(MediaCodecBridgeTest, H264VideoEncodeAndValidate) {
  SKIP_TEST_IF_HW_H264_IS_NOT_AVAILABLE();

  const int width = 640;
  const int height = 360;
  const int bit_rate = 300000;
  const int frame_rate = 30;
  const int i_frame_interval = 20;
  const std::set<int> supported_color_formats =
      MediaCodecUtil::GetEncoderColorFormats("video/avc");

  int color_format;
  if (supported_color_formats.count(COLOR_FORMAT_YUV420_SEMIPLANAR) > 0) {
    color_format = COLOR_FORMAT_YUV420_SEMIPLANAR;
  } else if (supported_color_formats.count(COLOR_FORMAT_YUV420_PLANAR) > 0) {
    color_format = COLOR_FORMAT_YUV420_PLANAR;
  } else {
    VLOG(0) << "Could not run test - YUV420_PLANAR and YUV420_SEMIPLANAR "
               "unavailable for h264 encode.";
    return;
  }

  std::unique_ptr<MediaCodecBridge> media_codec(
      MediaCodecBridgeImpl::CreateVideoEncoder(
          VideoCodec::kH264, gfx::Size(width, height), bit_rate, frame_rate,
          i_frame_interval, color_format));
  ASSERT_THAT(media_codec, NotNull());

  const char kSrcFileName[] = "bali_640x360_P420.yuv";
  base::FilePath src_file = GetTestDataFilePath(kSrcFileName);
  int64_t src_file_size = 0;
  ASSERT_TRUE(base::GetFileSize(src_file, &src_file_size));

  const VideoPixelFormat kInputFormat = PIXEL_FORMAT_I420;
  const int frame_size = static_cast<int>(
      VideoFrame::AllocationSize(kInputFormat, gfx::Size(width, height)));
  ASSERT_TRUE(frame_size > 0);
  ASSERT_TRUE(src_file_size % frame_size == 0U);

  const int num_frames = src_file_size / frame_size;
  base::File src(src_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::unique_ptr<uint8_t[]> frame_data =
      std::make_unique<uint8_t[]>(frame_size);
  ASSERT_THAT(
      src.Read(0, reinterpret_cast<char*>(frame_data.get()), frame_size),
      frame_size);

  // A monotonically-growing value.
  base::TimeDelta input_timestamp;

  // Src_file contains 1 frames. Encode it 3 times.
  for (int frame = 0; frame < num_frames && frame < 3; frame++) {
    input_timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / frame_rate);
    EncodeMediaFrame(media_codec.get(), frame_data.get(), width, height,
                     input_timestamp);
  }

  // Request key frame and encode 3 more frames. The second key frame should
  // also contain SPS/PPS NALUs.
  media_codec->RequestKeyFrameSoon();
  for (int frame = 0; frame < num_frames && frame < 3; frame++) {
    input_timestamp +=
        base::Microseconds(base::Time::kMicrosecondsPerSecond / frame_rate);
    EncodeMediaFrame(media_codec.get(), frame_data.get(), width, height,
                     input_timestamp);
  }
}

}  // namespace media
