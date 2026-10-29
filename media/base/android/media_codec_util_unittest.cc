// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"

#include "base/android/android_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MediaCodecUtilTest : public testing::Test {
 public:
  MediaCodecUtilTest() {}

  MediaCodecUtilTest(const MediaCodecUtilTest&) = delete;
  MediaCodecUtilTest& operator=(const MediaCodecUtilTest&) = delete;

  ~MediaCodecUtilTest() override {}

 public:
};

TEST_F(MediaCodecUtilTest, GuessCodedSizeAlignment) {
  EXPECT_EQ(std::nullopt,
            MediaCodecUtil::LookupCodedSizeAlignment("c2.fake.h264.decoder"));

  // Software AVC and HEVC decoders have a weird width-only alignment. This also
  // serves to test versioning of the alignment list.
  const gfx::Size kWeirdSoftwareAlignmentSv2(128, 2);
  EXPECT_EQ(kWeirdSoftwareAlignmentSv2,
            MediaCodecUtil::LookupCodedSizeAlignment(
                "c2.android.avc.decoder",
                base::android::android_info::SDK_VERSION_Sv2));
  EXPECT_EQ(kWeirdSoftwareAlignmentSv2,
            MediaCodecUtil::LookupCodedSizeAlignment(
                "c2.android.hevc.decoder",
                base::android::android_info::SDK_VERSION_Sv2));

  const gfx::Size kWeirdSoftwareAlignmentNougat(64, 2);
  EXPECT_EQ(kWeirdSoftwareAlignmentNougat,
            MediaCodecUtil::LookupCodedSizeAlignment(
                "c2.android.avc.decoder",
                base::android::android_info::SDK_VERSION_Q));
  EXPECT_EQ(kWeirdSoftwareAlignmentNougat,
            MediaCodecUtil::LookupCodedSizeAlignment(
                "c2.android.hevc.decoder",
                base::android::android_info::SDK_VERSION_Q));

  EXPECT_EQ(gfx::Size(64, 64),
            MediaCodecUtil::LookupCodedSizeAlignment("c2.unisoc.av1.decoder"));

  EXPECT_EQ(gfx::Size(16, 16), MediaCodecUtil::LookupCodedSizeAlignment(
                                   "c2.cros-codecs.vaapi.avc.decoder"));
  EXPECT_EQ(gfx::Size(8, 8), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.vaapi.hevc.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.vaapi.vp8.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.vaapi.vp9.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.vaapi.av1.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.v4l2.avc.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.v4l2.hevc.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.v4l2.vp9.decoder"));
  EXPECT_EQ(gfx::Size(1, 1), MediaCodecUtil::LookupCodedSizeAlignment(
                                 "c2.cros-codecs.v4l2.av1.decoder"));
}

TEST_F(MediaCodecUtilTest, EstimateVideoBufferSize) {
  // AVC with rounding
  EXPECT_EQ(60480u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kH264,
                                                            321, 240));

  // VP8 (no rounding)
  EXPECT_EQ(57780u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kVP8,
                                                            321, 240));

  // HEVC, VP9, AV1, DolbyVision (min compression ratio 4)
  EXPECT_EQ(28890u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kHEVC,
                                                            321, 240));
  EXPECT_EQ(28890u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kVP9,
                                                            321, 240));
  EXPECT_EQ(28890u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kAV1,
                                                            321, 240));
  EXPECT_EQ(28890u, MediaCodecUtil::EstimateVideoBufferSize(
                        VideoCodec::kDolbyVision, 321, 240));

  // Unknown codec
  EXPECT_EQ(0u, MediaCodecUtil::EstimateVideoBufferSize(VideoCodec::kUnknown,
                                                        321, 240));
}

}  // namespace media
