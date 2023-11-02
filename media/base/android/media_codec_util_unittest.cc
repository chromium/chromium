// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"
#include "base/android/build_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// These will come from mockable BuildInfo, once it exists.
using base::android::SDK_VERSION_NOUGAT;
using base::android::SDK_VERSION_NOUGAT_MR1;

class MediaCodecUtilTest : public testing::Test {
 public:
  MediaCodecUtilTest() {}

  MediaCodecUtilTest(const MediaCodecUtilTest&) = delete;
  MediaCodecUtilTest& operator=(const MediaCodecUtilTest&) = delete;

  ~MediaCodecUtilTest() override {}

 public:
};

TEST_F(MediaCodecUtilTest, TestCbcsAvailableIfNewerVersion) {
  EXPECT_FALSE(
      MediaCodecUtil::PlatformSupportsCbcsEncryption(SDK_VERSION_NOUGAT));
  EXPECT_TRUE(
      MediaCodecUtil::PlatformSupportsCbcsEncryption(SDK_VERSION_NOUGAT_MR1));
}

}  // namespace media
