// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"
#include "base/android/build_info.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// These will come from mockable BuildInfo, once it exists.
using base::android::SDK_VERSION_KITKAT;
using base::android::SDK_VERSION_LOLLIPOP;
using base::android::SDK_VERSION_LOLLIPOP_MR1;
using base::android::SDK_VERSION_MARSHMALLOW;
using base::android::SDK_VERSION_NOUGAT;
using base::android::SDK_VERSION_NOUGAT_MR1;

class MediaCodecUtilTest : public testing::Test {
 public:
  MediaCodecUtilTest() {}
  ~MediaCodecUtilTest() override {}

 public:
  DISALLOW_COPY_AND_ASSIGN(MediaCodecUtilTest);
};

TEST_F(MediaCodecUtilTest, TestCodecAvailableIfNewerVersion) {
  // Test models that should be available above some sdk level.
  // We probably don't need to test them all; we're more concerned that the
  // blacklist code is doing the right thing with the entries it has rather than
  // the map contents are right.
  struct {
    const char* model;
    int last_bad_sdk;
  } devices[] = {{"LGMS330", SDK_VERSION_LOLLIPOP_MR1},

                 {"GT-I9100", SDK_VERSION_KITKAT},
                 {"GT-I9300", SDK_VERSION_KITKAT},
                 {"GT-N7000", SDK_VERSION_KITKAT},
                 {"GT-N7100", SDK_VERSION_KITKAT},
                 {"A6600", SDK_VERSION_KITKAT},
                 {"A6800", SDK_VERSION_KITKAT},
                 {"GT-S7262", SDK_VERSION_KITKAT},
                 {"GT-S5282", SDK_VERSION_KITKAT},
                 {"GT-I8552", SDK_VERSION_KITKAT},

                 {"always_works", 0},  // Some codec that works everywhere.
                 {nullptr, 0}};

  for (int sdk = SDK_VERSION_KITKAT; sdk <= SDK_VERSION_NOUGAT; sdk++) {
    for (int i = 0; devices[i].model; i++) {
      bool supported =
          MediaCodecUtil::IsMediaCodecAvailableFor(sdk, devices[i].model);

      // Make sure that this model is supported if and only if |sdk| is
      // newer than |last_bad_sdk|.
      ASSERT_TRUE(supported == (sdk > devices[i].last_bad_sdk))
          << " model: " << devices[i].model << " sdk: " << sdk;
    }
  }
}

TEST_F(MediaCodecUtilTest, TestCbcsAvailableIfNewerVersion) {
  EXPECT_FALSE(
      MediaCodecUtil::PlatformSupportsCbcsEncryption(SDK_VERSION_MARSHMALLOW));
  EXPECT_FALSE(
      MediaCodecUtil::PlatformSupportsCbcsEncryption(SDK_VERSION_NOUGAT));
  EXPECT_TRUE(
      MediaCodecUtil::PlatformSupportsCbcsEncryption(SDK_VERSION_NOUGAT_MR1));
}

}  // namespace media
