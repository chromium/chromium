// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/application_loopback_device_helper.h"

#include "base/process/process_handle.h"
#include "media/audio/audio_device_description.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace media {

#if BUILDFLAG(IS_WIN)
TEST(ApplicationLoopbackDeviceHelperTest, EncodeDecode) {
  uint32_t application_id = 12345;
  std::string device_id = CreateApplicationLoopbackDeviceId(application_id);
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  EXPECT_FALSE(IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id));
  uint32_t decoded_application_id =
      GetApplicationIdFromApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(application_id, decoded_application_id);
}

TEST(ApplicationLoopbackDeviceHelperTest, RestrictOwnAudio) {
  std::string device_id = CreateRestrictOwnAudioBrowserLoopbackDeviceId();
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  EXPECT_TRUE(IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id));
  uint32_t decoded_application_id =
      GetApplicationIdFromApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(static_cast<uint32_t>(base::GetCurrentProcId()),
            decoded_application_id);
}

#elif BUILDFLAG(IS_MAC)

TEST(ApplicationLoopbackDeviceHelperTest, AddAndExtractBundleId) {
  std::string bundle_id = "org.chromium";
  std::string device_id =
      CreateApplicationLoopbackDeviceId(bundle_id, std::nullopt);
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  EXPECT_FALSE(IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id));
  std::pair<std::string, std::optional<pid_t>> extracted_id =
      ParseApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(bundle_id, extracted_id.first);
  EXPECT_EQ(std::nullopt, extracted_id.second);
}

TEST(ApplicationLoopbackDeviceHelperTest,
     AddAndExtractBundleIdAndApplicationId) {
  std::string bundle_id = "org.chromium";
  pid_t application_id = 12345;
  std::string device_id =
      CreateApplicationLoopbackDeviceId(bundle_id, application_id);
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  EXPECT_FALSE(IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id));
  std::pair<std::string, std::optional<pid_t>> extracted_id =
      ParseApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(bundle_id, extracted_id.first);
  EXPECT_EQ(application_id, extracted_id.second);
}

TEST(ApplicationLoopbackDeviceHelperTest, RestrictOwnAudioWithBundleId) {
  std::string bundle_id = "org.chromium";
  pid_t application_id = 12345;
  std::string device_id =
      CreateRestrictOwnAudioBrowserLoopbackDeviceId(bundle_id, application_id);
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(device_id));
  EXPECT_TRUE(IsRestrictOwnAudioBrowserLoopbackDeviceId(device_id));
  std::pair<std::string, std::optional<pid_t>> extracted_id =
      ParseApplicationLoopbackDeviceId(device_id);
  EXPECT_EQ(bundle_id, extracted_id.first);
  EXPECT_EQ(application_id, extracted_id.second);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace media
