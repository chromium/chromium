// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media::android {

TEST(AudioAndroidAudioDeviceTypeTest, ConvertFromKnownIntValue) {
  const int value = 2;
  const AudioDeviceType expected_type = AudioDeviceType::kBuiltinSpeaker;

  const std::optional<AudioDeviceType> actual_type =
      IntToAudioDeviceType(value);
  ASSERT_TRUE(actual_type.has_value());
  EXPECT_EQ(actual_type, expected_type);
}

TEST(AudioAndroidAudioDeviceTypeTest, ConvertFromUnknownIntValue) {
  for (const int value : {-1, 100000}) {
    const std::optional<AudioDeviceType> actual_type =
        IntToAudioDeviceType(value);
    EXPECT_FALSE(actual_type.has_value());
  }
}

}  // namespace media::android
