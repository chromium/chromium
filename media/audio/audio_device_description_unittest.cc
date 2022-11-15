// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_description.h"

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
      AudioDeviceDescription("User's AirPods", "uniqueId", "groupId")};
  AudioDeviceDescription::LocalizeDeviceDescriptions(&device_descriptions);

  EXPECT_EQ(device_descriptions[0].device_name, "Default - AirPods");
  EXPECT_EQ(device_descriptions[1].device_name, "AirPods");
}

}  // namespace media