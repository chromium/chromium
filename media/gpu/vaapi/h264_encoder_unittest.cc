// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/h264_encoder.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

AcceleratedVideoEncoder::Config kDefaultAVEConfig{10};

VideoEncodeAccelerator::Config kDefaultVEAConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    H264PROFILE_BASELINE,
    14000000 /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    base::nullopt /* gop_length */,
    base::nullopt /* h264 output level*/,
    VideoEncodeAccelerator::Config::StorageType::kShmem,
    VideoEncodeAccelerator::Config::ContentType::kCamera);

class MockH264Accelerator : public H264Encoder::Accelerator {
 public:
  MockH264Accelerator() = default;
  MOCK_METHOD1(
      GetPicture,
      scoped_refptr<H264Picture>(AcceleratedVideoEncoder::EncodeJob* job));
  MOCK_METHOD3(SubmitPackedHeaders,
               bool(AcceleratedVideoEncoder::EncodeJob*,
                    scoped_refptr<H264BitstreamBuffer>,
                    scoped_refptr<H264BitstreamBuffer>));
  MOCK_METHOD7(SubmitFrameParameters,
               bool(AcceleratedVideoEncoder::EncodeJob*,
                    const H264Encoder::EncodeParams&,
                    const H264SPS&,
                    const H264PPS&,
                    scoped_refptr<H264Picture>,
                    const std::list<scoped_refptr<H264Picture>>&,
                    const std::list<scoped_refptr<H264Picture>>&));
};
}  // namespace

class H264EncoderTest : public ::testing::Test {
 public:
  H264EncoderTest() = default;
  void SetUp() override;

  void ExpectLevel(uint8_t level) { EXPECT_EQ(encoder_->level_, level); }

 protected:
  std::unique_ptr<H264Encoder> encoder_;
  MockH264Accelerator* accelerator_;
};

void H264EncoderTest::SetUp() {
  auto mock_accelerator = std::make_unique<MockH264Accelerator>();
  accelerator_ = mock_accelerator.get();
  encoder_ = std::make_unique<H264Encoder>(std::move(mock_accelerator));

  // Set default behaviors for mock methods for convenience.
  ON_CALL(*accelerator_, GetPicture(_))
      .WillByDefault(Invoke([](AcceleratedVideoEncoder::EncodeJob*) {
        return new H264Picture();
      }));
  ON_CALL(*accelerator_, SubmitPackedHeaders(_, _, _))
      .WillByDefault(Return(true));
  ON_CALL(*accelerator_, SubmitFrameParameters(_, _, _, _, _, _, _))
      .WillByDefault(Return(true));
}

TEST_F(H264EncoderTest, Initialize) {
  VideoEncodeAccelerator::Config vea_config = kDefaultVEAConfig;
  AcceleratedVideoEncoder::Config ave_config = kDefaultAVEConfig;
  EXPECT_TRUE(encoder_->Initialize(vea_config, ave_config));
  // Profile is unspecified, H264Encoder will select the default level, 4.0.
  // 4.0 will be proper with |vea_config|'s values.
  ExpectLevel(H264SPS::kLevelIDC4p0);

  // Initialize with 4k size. The level will be adjusted to 5.1 by H264Encoder.
  vea_config.input_visible_size.SetSize(3840, 2160);
  EXPECT_TRUE(encoder_->Initialize(vea_config, ave_config));
  ExpectLevel(H264SPS::kLevelIDC5p1);
}

}  // namespace media
