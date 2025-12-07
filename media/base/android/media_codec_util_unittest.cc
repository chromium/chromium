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
}

}  // namespace media
