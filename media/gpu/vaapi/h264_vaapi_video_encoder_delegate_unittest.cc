// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/h264_vaapi_video_encoder_delegate.h"

#include <memory>

#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace media {
namespace {

VaapiVideoEncoderDelegate::Config kDefaultVEADelegateConfig{10};

VideoEncodeAccelerator::Config kDefaultVEAConfig(
    PIXEL_FORMAT_I420,
    gfx::Size(1280, 720),
    H264PROFILE_BASELINE,
    Bitrate::ConstantBitrate(14000000)
    /* = maximum bitrate in bits per second for level 3.1 */,
    VideoEncodeAccelerator::kDefaultFramerate,
    absl::nullopt /* gop_length */,
    absl::nullopt /* h264 output level*/,
    false /* is_constrained_h264 */,
    VideoEncodeAccelerator::Config::StorageType::kShmem,
    VideoEncodeAccelerator::Config::ContentType::kCamera);

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper() : VaapiWrapper(kEncodeConstantBitrate) {}

 protected:
  ~MockVaapiWrapper() override = default;
};

}  // namespace

class H264VaapiVideoEncoderDelegateTest : public ::testing::Test {
 public:
  H264VaapiVideoEncoderDelegateTest() = default;
  void SetUp() override;

  void ExpectLevel(uint8_t level) { EXPECT_EQ(encoder_->level_, level); }

  MOCK_METHOD0(OnError, void());

 protected:
  std::unique_ptr<H264VaapiVideoEncoderDelegate> encoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
};

void H264VaapiVideoEncoderDelegateTest::SetUp() {
  mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>();
  ASSERT_TRUE(mock_vaapi_wrapper_);

  encoder_ = std::make_unique<H264VaapiVideoEncoderDelegate>(
      mock_vaapi_wrapper_,
      base::BindRepeating(&H264VaapiVideoEncoderDelegateTest::OnError,
                          base::Unretained(this)));
  EXPECT_CALL(*this, OnError()).Times(0);
}

TEST_F(H264VaapiVideoEncoderDelegateTest, Initialize) {
  auto vea_config = kDefaultVEAConfig;
  const auto vea_delegate_config = kDefaultVEADelegateConfig;
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
  // Profile is unspecified, H264VaapiVideoEncoderDelegate will select the
  // default level, 4.0. 4.0 will be proper with |vea_config|'s values.
  ExpectLevel(H264SPS::kLevelIDC4p0);

  // Initialize with 4k size. The level will be adjusted to 5.1 by
  // H264VaapiVideoEncoderDelegate.
  vea_config.input_visible_size.SetSize(3840, 2160);
  EXPECT_TRUE(encoder_->Initialize(vea_config, vea_delegate_config));
  ExpectLevel(H264SPS::kLevelIDC5p1);
}

TEST_F(H264VaapiVideoEncoderDelegateTest, InitializeFailsTemporalLayerRequest) {
  auto vea_config = kDefaultVEAConfig;
  vea_config.spatial_layers.resize(1u);
  auto& sl = vea_config.spatial_layers[0];
  sl.width = vea_config.input_visible_size.width();
  sl.height = vea_config.input_visible_size.height();
  sl.bitrate_bps = vea_config.bitrate.target();
  sl.framerate = vea_config.initial_framerate.value_or(30);
  sl.max_qp = 30;
  sl.num_of_temporal_layers = 2u;
  EXPECT_FALSE(encoder_->Initialize(vea_config, kDefaultVEADelegateConfig));
}

}  // namespace media
