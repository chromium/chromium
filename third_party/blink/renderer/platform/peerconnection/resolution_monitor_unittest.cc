// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/resolution_monitor.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/parsers/ivf_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
const media::VideoCodec kCodecs[] = {
    media::VideoCodec::kH264,
    media::VideoCodec::kVP8,
    media::VideoCodec::kVP9,
    media::VideoCodec::kAV1,
};

class ResolutionMonitorTestWithInvalidFrame
    : public ::testing::TestWithParam<media::VideoCodec> {
 protected:
  std::string kInvalidData = "This is invalid data and causes a parser error";
};

TEST_P(ResolutionMonitorTestWithInvalidFrame, ReturnNullOpt) {
  const media::VideoCodec codec = GetParam();
  auto invalid_buffer =
      media::DecoderBuffer::CopyFrom(base::as_byte_span(kInvalidData));
  invalid_buffer->set_is_key_frame(true);

  auto resolution_monitor = ResolutionMonitor::Create(codec);
  ASSERT_TRUE(resolution_monitor);
  EXPECT_FALSE(resolution_monitor->GetResolution(*invalid_buffer));
}

INSTANTIATE_TEST_SUITE_P(,
                         ResolutionMonitorTestWithInvalidFrame,
                         ::testing::ValuesIn(kCodecs));

struct FrameData {
  std::string file_name;
  media::VideoCodec codec;
  gfx::Size resolution;
};

class ResolutionMonitorTestWithValidFrame
    : public ::testing::TestWithParam<FrameData> {};

TEST_P(ResolutionMonitorTestWithValidFrame, ReturnExpectedResolution) {
  const auto param = GetParam();
  auto buffer = media::ReadTestDataFile(param.file_name);
  ASSERT_TRUE(buffer);
  buffer->set_is_key_frame(true);

  auto resolution_monitor = ResolutionMonitor::Create(param.codec);
  ASSERT_TRUE(resolution_monitor);
  EXPECT_EQ(resolution_monitor->GetResolution(*buffer), param.resolution);
}

const FrameData kH264Frames[] = {
    // 320x180 because we acquire visible size here.
    {"h264-320x180-frame-0", media::VideoCodec::kH264, gfx::Size(320, 180)},
    {"bear-320x192-baseline-frame-0.h264", media::VideoCodec::kH264,
     gfx::Size(320, 192)},
    {"bear-320x192-high-frame-0.h264", media::VideoCodec::kH264, gfx::Size(320, 192)},
};

const FrameData kVP8Frames[] = {
    {"vp8-I-frame-160x240", media::VideoCodec::kVP8, gfx::Size(160, 240)},
    {"vp8-I-frame-320x120", media::VideoCodec::kVP8, gfx::Size(320, 120)},
    {"vp8-I-frame-320x240", media::VideoCodec::kVP8, gfx::Size(320, 240)},
    {"vp8-I-frame-320x480", media::VideoCodec::kVP8, gfx::Size(320, 480)},
    {"vp8-I-frame-640x240", media::VideoCodec::kVP8, gfx::Size(640, 240)},
};

const FrameData kVP9Frames[] = {
    {"vp9-I-frame-1280x720", media::VideoCodec::kVP9, gfx::Size(1280, 720)},
    {"vp9-I-frame-320x240", media::VideoCodec::kVP9, gfx::Size(320, 240)},
};

const FrameData kAV1Frames[] = {
    {"av1-I-frame-320x240", media::VideoCodec::kAV1, gfx::Size(320, 240)},
    {"av1-I-frame-1280x720", media::VideoCodec::kAV1, gfx::Size(1280, 720)},
    {"av1-monochrome-I-frame-320x240-8bpp", media::VideoCodec::kAV1,
     gfx::Size(320, 240)},
};

INSTANTIATE_TEST_SUITE_P(H264,
                         ResolutionMonitorTestWithValidFrame,
                         ::testing::ValuesIn(kH264Frames));
INSTANTIATE_TEST_SUITE_P(VP8,
                         ResolutionMonitorTestWithValidFrame,
                         ::testing::ValuesIn(kVP8Frames));
INSTANTIATE_TEST_SUITE_P(VP9,
                         ResolutionMonitorTestWithValidFrame,
                         ::testing::ValuesIn(kVP9Frames));
INSTANTIATE_TEST_SUITE_P(AV1,
                         ResolutionMonitorTestWithValidFrame,
                         ::testing::ValuesIn(kAV1Frames));

std::vector<scoped_refptr<media::DecoderBuffer>> ReadIVF(const std::string& fname) {
  std::string ivf_data;
  auto input_file = media::GetTestDataFilePath(fname);
  EXPECT_TRUE(base::ReadFileToString(input_file, &ivf_data));

  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_header{};
  EXPECT_TRUE(
      ivf_parser.Initialize(reinterpret_cast<const uint8_t*>(ivf_data.data()),
                            ivf_data.size(), &ivf_header));

  std::vector<scoped_refptr<media::DecoderBuffer>> buffers;
  media::IvfFrameHeader ivf_frame_header{};
  const uint8_t* data;
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &data)) {
    buffers.push_back(media::DecoderBuffer::CopyFrom(
        // TODO(crbug.com/40284755): Spanify `ParseNextFrame`.
        UNSAFE_TODO(base::span(data, ivf_frame_header.frame_size))));
  }
  return buffers;
}

struct VideoData {
  std::string file_name;
  media::VideoCodec codec;
  gfx::Size resolution;
};

class ResolutionMonitorTestWithValidVideo
    : public ::testing::TestWithParam<VideoData> {};

TEST_P(ResolutionMonitorTestWithValidVideo, ReturnExpectedResolution) {
  const auto param = GetParam();
  auto buffers = ReadIVF(param.file_name);
  buffers[0]->set_is_key_frame(true);
  auto resolution_monitor = ResolutionMonitor::Create(param.codec);
  ASSERT_TRUE(resolution_monitor);
  for (const auto& buffer : buffers) {
    EXPECT_EQ(resolution_monitor->GetResolution(*buffer), param.resolution);
  }
}

const VideoData kVP8Videos[] = {
    {"test-25fps.vp8", media::VideoCodec::kVP8, gfx::Size(320, 240)},
    {"bear-1280x720.ivf", media::VideoCodec::kVP8, gfx::Size(1280, 720)},
};

const VideoData kVP9Videos[] = {
    {"test-25fps.vp9", media::VideoCodec::kVP9, gfx::Size(320, 240)},
    {"test-25fps.vp9_2", media::VideoCodec::kVP9, gfx::Size(320, 240)},
    {"bear-vp9.ivf", media::VideoCodec::kVP9, gfx::Size(320, 240)},
};

const VideoData kAV1Videos[] = {
    {"test-25fps.av1.ivf", media::VideoCodec::kAV1, gfx::Size(320, 240)},
    {"av1-show_existing_frame.ivf", media::VideoCodec::kAV1, gfx::Size(208, 144)},
    {"av1-svc-L1T2.ivf", media::VideoCodec::kAV1, gfx::Size(640, 360)},
};

INSTANTIATE_TEST_SUITE_P(VP8,
                         ResolutionMonitorTestWithValidVideo,
                         ::testing::ValuesIn(kVP8Videos));
INSTANTIATE_TEST_SUITE_P(VP9,
                         ResolutionMonitorTestWithValidVideo,
                         ::testing::ValuesIn(kVP9Videos));
INSTANTIATE_TEST_SUITE_P(AV1,
                         ResolutionMonitorTestWithValidVideo,
                         ::testing::ValuesIn(kAV1Videos));

TEST(ResolutionMonitorTestWithTruncatedH264, ZeroLengthNalUnit) {
  auto resolution_monitor = ResolutionMonitor::Create(media::VideoCodec::kH264);
  ASSERT_TRUE(resolution_monitor);
  const uint8_t invalid_data[] = {
      0x00, 0x00, 0x00, 0x01,  // Just a NAL header.
      0x00, 0x00, 0x00, 0x01,
      0x68,  // PPS since FindNaluIndices does not like just an empty NAL
             // header.
  };
  auto invalid_buffer = media::DecoderBuffer::CopyFrom(invalid_data);
  invalid_buffer->set_is_key_frame(true);
  EXPECT_EQ(resolution_monitor->GetResolution(*invalid_buffer), std::nullopt);
}

TEST(ResolutionMonitorTestWithTruncatedH264, IncompleteSps) {
  auto resolution_monitor = ResolutionMonitor::Create(media::VideoCodec::kH264);
  ASSERT_TRUE(resolution_monitor);
  const uint8_t invalid_data[] = {
      0x00, 0x00, 0x00, 0x01, 0x67,  // NAL header and type but no content.
  };
  auto invalid_buffer = media::DecoderBuffer::CopyFrom(invalid_data);
  invalid_buffer->set_is_key_frame(true);
  EXPECT_EQ(resolution_monitor->GetResolution(*invalid_buffer), std::nullopt);
}

}  // namespace

}  // namespace blink
