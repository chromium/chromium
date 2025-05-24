// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device_id.h"

#include <aaudio/AAudio.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace media::android {

TEST(AudioAndroidAudioDeviceIdTest, CreateDefaultDeviceId) {
  const AudioDeviceId id = AudioDeviceId::Default();
  EXPECT_TRUE(id.IsDefault());
}

TEST(AudioAndroidAudioDeviceIdTest, ConvertDefaultDeviceIdToAAudioDeviceId) {
  const AudioDeviceId id = AudioDeviceId::Default();
  EXPECT_EQ(id.ToAAudioDeviceId(), AAUDIO_UNSPECIFIED);
}

TEST(AudioAndroidAudioDeviceIdTest,
     CreateNonDefaultDeviceIdAndConvertToAAudioDeviceId) {
  const std::optional<AudioDeviceId> id = AudioDeviceId::NonDefault(100);
  ASSERT_TRUE(id.has_value());
  EXPECT_FALSE(id->IsDefault());
  EXPECT_EQ(id->ToAAudioDeviceId(), 100);
}

TEST(AudioAndroidAudioDeviceIdTest, ParseDefaultDeviceId) {
  for (std::string id_string : {"", "default"}) {
    const std::optional<AudioDeviceId> id = AudioDeviceId::Parse(id_string);
    ASSERT_TRUE(id.has_value());
    EXPECT_TRUE(id->IsDefault());
  }
}

TEST(AudioAndroidAudioDeviceIdTest,
     ParseValidNonDefaultDeviceIdAndConvertToAAudioDeviceId) {
  const std::optional<AudioDeviceId> id = AudioDeviceId::Parse("100");
  ASSERT_TRUE(id.has_value());
  EXPECT_FALSE(id->IsDefault());
  EXPECT_EQ(id->ToAAudioDeviceId(), 100);
}

TEST(AudioAndroidAudioDeviceIdTest, ParseInvalidNonDefaultDeviceId) {
  for (const std::string& id_string : {"0", "999999999999", " 4", "x"}) {
    const std::optional<AudioDeviceId> id = AudioDeviceId::Parse(id_string);
    EXPECT_FALSE(id.has_value());
  }
}

}  // namespace media::android
