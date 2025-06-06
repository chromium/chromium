// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device.h"

#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::android {

TEST(AudioAndroidAudioDeviceTest, CreateDefaultDevice) {
  const AudioDevice device = AudioDevice::Default();
  EXPECT_TRUE(device.IsDefault());
  EXPECT_EQ(device.GetId(), AudioDeviceId::Default());
  EXPECT_EQ(device.GetType(), AudioDeviceType::kUnknown);
  EXPECT_EQ(device.GetAssociatedScoDevice(), std::nullopt);
}

TEST(AudioAndroidAudioDeviceTest, CreateDeviceWithDefaultId) {
  constexpr AudioDeviceId id = AudioDeviceId::Default();
  constexpr AudioDeviceType type = AudioDeviceType::kBuiltinSpeaker;

  const AudioDevice device(id, type);
  EXPECT_TRUE(device.IsDefault());
  EXPECT_EQ(device.GetId(), id);
  EXPECT_EQ(device.GetType(), type);
  EXPECT_EQ(device.GetAssociatedScoDevice(), std::nullopt);
}

TEST(AudioAndroidAudioDeviceTest, CreateDeviceWithNonDefaultId) {
  const AudioDeviceId id = AudioDeviceId::NonDefault(100).value();
  constexpr AudioDeviceType type = AudioDeviceType::kBuiltinMic;

  const AudioDevice device(id, type);
  EXPECT_FALSE(device.IsDefault());
  EXPECT_EQ(device.GetId(), id);
  EXPECT_EQ(device.GetType(), type);
  EXPECT_EQ(device.GetAssociatedScoDevice(), std::nullopt);
}

TEST(AudioAndroidAudioDeviceTest, SetAndGetAssociatedScoDevice) {
  const AudioDeviceId a2dp_id = AudioDeviceId::NonDefault(100).value();
  const AudioDeviceId sco_id = AudioDeviceId::NonDefault(200).value();

  AudioDevice device = AudioDevice(a2dp_id, AudioDeviceType::kBluetoothA2dp);
  device.SetAssociatedScoDeviceId(sco_id);

  const std::optional<AudioDevice> sco_device = device.GetAssociatedScoDevice();
  ASSERT_TRUE(sco_device.has_value());
  EXPECT_EQ(sco_device->GetId(), sco_id);
  EXPECT_EQ(sco_device->GetType(), AudioDeviceType::kBluetoothSco);
}

}  // namespace media::android
