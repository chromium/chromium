// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_description.h"

#include "media/audio/application_loopback_device_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(AudioDeviceDescriptionTest, LocalizedGenericLabelLeftUnchanged) {
  std::vector<AudioDeviceDescription> device_descriptions{
      AudioDeviceDescription("Super fantastic microphone", "uniqueId",
                             "groupId")};
  AudioDeviceDescription::LocalizeDeviceDescriptions(&device_descriptions);

  EXPECT_EQ(device_descriptions[0].device_name, "Super fantastic microphone");
}

TEST(AudioDeviceDescriptionTest, LocalizedUserNameInLabelIsSanitized) {
  std::vector<AudioDeviceDescription> device_descriptions{
      AudioDeviceDescription("User's AirPods", "uniqueId", "groupId")};
  AudioDeviceDescription::LocalizeDeviceDescriptions(&device_descriptions);

  EXPECT_EQ(device_descriptions[0].device_name, "AirPods");
}

TEST(AudioDeviceDescriptionTest, LocalizedUserNameInDefaultDeviceIsSanitized) {
  std::vector<AudioDeviceDescription> device_descriptions{
      AudioDeviceDescription("User's AirPods", "default", "groupId"),
      AudioDeviceDescription("User's AirPods", "uniqueId", "groupId"),
      AudioDeviceDescription("AirPods User Hands-Free AG Audio", "uniqueId",
                             "groupId"),
      AudioDeviceDescription("AirPods User Stereo", "uniqueId", "groupId"),
  };
  AudioDeviceDescription::LocalizeDeviceDescriptions(&device_descriptions);

  EXPECT_EQ(device_descriptions[0].device_name, "Default - AirPods");
  EXPECT_EQ(device_descriptions[1].device_name, "AirPods");
  EXPECT_EQ(device_descriptions[2].device_name, "AirPods Hands-Free AG Audio");
  EXPECT_EQ(device_descriptions[3].device_name, "AirPods Stereo");
}

TEST(AudioDeviceDescriptionTest, GetDefaultDeviceName) {
  auto default_name = AudioDeviceDescription::GetDefaultDeviceName();

  // Passing an empty string should return `default_name`.
  EXPECT_EQ(AudioDeviceDescription::GetDefaultDeviceName(std::string()),
            default_name);
  EXPECT_EQ(AudioDeviceDescription::GetDefaultDeviceName(""), default_name);

  std::string real_device_name = "Real Device Name";

  // `real_device_name` should be appended.
  EXPECT_EQ(AudioDeviceDescription::GetDefaultDeviceName(real_device_name),
            default_name + " - " + real_device_name);

  // This should contain "Real Device", without a null-terminator.
  std::string_view non_null_terminated_name =
      std::string_view(real_device_name).substr(0, 11);

  // Verify we properly handle a non-null terminated string_view.
  EXPECT_EQ(
      AudioDeviceDescription::GetDefaultDeviceName(non_null_terminated_name),
      default_name + " - " + std::string(non_null_terminated_name));
}

#if BUILDFLAG(IS_WIN)
TEST(AudioDeviceDescriptionTest, GetCommunicationsDeviceName) {
  auto communication_name =
      AudioDeviceDescription::GetCommunicationsDeviceName();

  // Passing an empty string should return `communication_name`.
  EXPECT_EQ(AudioDeviceDescription::GetCommunicationsDeviceName(std::string()),
            communication_name);
  EXPECT_EQ(AudioDeviceDescription::GetCommunicationsDeviceName(""),
            communication_name);

  std::string real_device_name = "Real Device Name";

  // `real_device_name` should be appended.
  EXPECT_EQ(
      AudioDeviceDescription::GetCommunicationsDeviceName(real_device_name),
      communication_name + " - " + real_device_name);

  std::string_view non_null_terminated_name =
      std::string_view(real_device_name).substr(0, 11);

  // Verify we properly handle a non-null terminated string_view.
  EXPECT_EQ(AudioDeviceDescription::GetCommunicationsDeviceName(
                non_null_terminated_name),
            communication_name + " - " + std::string(non_null_terminated_name));
}
#endif

TEST(AudioDeviceDescriptionTest, IsLoopbackDevice) {
  EXPECT_TRUE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kLoopbackInputDeviceId));
  EXPECT_TRUE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kLoopbackWithMuteDeviceId));
  EXPECT_TRUE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kLoopbackWithoutChromeId));
  EXPECT_TRUE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kApplicationLoopbackDeviceId));
  EXPECT_TRUE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kLoopbackAllDevicesId));
  EXPECT_FALSE(AudioDeviceDescription::IsLoopbackDevice(
      AudioDeviceDescription::kDefaultDeviceId));
}

TEST(AudioDeviceDescriptionTest, IsApplicationLoopbackDevice) {
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      AudioDeviceDescription::kApplicationLoopbackDeviceId));
  EXPECT_TRUE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      CreateApplicationLoopbackDeviceId(12345)));
  EXPECT_FALSE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      AudioDeviceDescription::kLoopbackInputDeviceId));
  EXPECT_FALSE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      AudioDeviceDescription::kLoopbackWithMuteDeviceId));
  EXPECT_FALSE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      AudioDeviceDescription::kLoopbackWithoutChromeId));
  EXPECT_FALSE(AudioDeviceDescription::IsApplicationLoopbackDevice(
      AudioDeviceDescription::kDefaultDeviceId));
}

}  // namespace media
