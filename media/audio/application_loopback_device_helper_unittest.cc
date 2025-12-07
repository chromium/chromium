// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/application_loopback_device_helper.h"

#include "media/audio/audio_device_description.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace media {

TEST(ApplicationLoopbackDeviceHelperTest, EncodeDecode) {
  uint32_t application_id = 12345;
  std::string device_id = CreateApplicationLoopbackDeviceId(application_id);
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  uint32_t decoded_application_id =
      GetApplicationIdFromApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(application_id, decoded_application_id);
}

}  // namespace media
