// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/android/audio_device.h"

#include <memory>
#include <optional>
#include <sstream>

#include "media/audio/android/audio_device_id.h"
#include "media/audio/android/audio_device_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using AudioDevice = media::android::AudioDevice;
using AudioDeviceId = media::android::AudioDeviceId;
using AudioDeviceType = media::android::AudioDeviceType;

namespace media::android {

std::ostream& operator<<(std::ostream& os, const AudioDevice& device);

namespace {

struct AudioDeviceParams {
  AudioDeviceId id;
  AudioDeviceType type;
  std::optional<std::string> name;
  std::optional<std::vector<int>> sample_rates;
  std::optional<AudioDevice> associated_sco_device;
  bool expected_is_default;
};

class AudioAndroidAudioDeviceParameterizedTest
    : public testing::TestWithParam<AudioDeviceParams> {
 public:
  AudioDevice CreateDevice() {
    auto& params = GetParam();

    AudioDevice device(params.id, params.type, params.name,
                       params.sample_rates);
    if (params.associated_sco_device.has_value()) {
      device.SetAssociatedScoDevice(
          std::make_unique<AudioDevice>(params.associated_sco_device.value()));
    }
    return device;
  }
};

std::string SampleRatesToString(std::optional<std::vector<int>> sample_rates) {
  if (!sample_rates.has_value()) {
    return "nullopt";
  }

  std::stringstream string;
  string << "[";
  bool first = true;
  for (int sample_rate : sample_rates.value()) {
    if (!first) {
      string << ", ";
    }
    string << sample_rate;
    first = false;
  }
  string << "]";
  return string.str();
}

std::ostream& operator<<(std::ostream& os, const AudioDeviceParams& params) {
  os << "{";
  os << "id: " << params.id.ToAAudioDeviceId();
  os << ", type: " << static_cast<int>(params.type);
  os << ", sample_rates: " << SampleRatesToString(params.sample_rates);
  os << ", associated_sco_device: ";
  if (params.associated_sco_device.has_value()) {
    os << params.associated_sco_device.value();
  } else {
    os << "nullopt";
  }
  os << ", expected_is_default: " << params.expected_is_default;
  os << "}";
  return os;
}

}  // namespace

bool operator==(const AudioDevice& left, const AudioDevice& right) {
  return left.GetId() == right.GetId() && left.GetType() == right.GetType() &&
         left.GetName() == right.GetName() &&
         left.GetSampleRates() == right.GetSampleRates() &&
         left.GetAssociatedScoDevice() == right.GetAssociatedScoDevice();
}

std::ostream& operator<<(std::ostream& os, const AudioDevice& device) {
  os << "{";
  os << "id: " << device.GetId().ToAAudioDeviceId();
  os << ", type: " << static_cast<int>(device.GetType());
  os << ", sample_rates: " << SampleRatesToString(device.GetSampleRates());
  os << ", associated_sco_device: ";
  if (device.GetAssociatedScoDevice().has_value()) {
    os << device.GetAssociatedScoDevice().value();
  } else {
    os << "nullopt";
  }
  os << "}";
  return os;
}

TEST(AudioAndroidAudioDeviceTest, CreateDefaultDevice) {
  const AudioDevice device = AudioDevice::Default();
  EXPECT_TRUE(device.IsDefault());
}

TEST_P(AudioAndroidAudioDeviceParameterizedTest, CreateDevice) {
  AudioDevice device = CreateDevice();

  EXPECT_EQ(device.IsDefault(), GetParam().expected_is_default);
  EXPECT_EQ(device.GetId(), GetParam().id);
  EXPECT_EQ(device.GetType(), GetParam().type);
  EXPECT_EQ(device.GetAssociatedScoDevice(), GetParam().associated_sco_device);
}

// Copy constructor is defined explicitly; ensure it behaves as expected.
TEST_P(AudioAndroidAudioDeviceParameterizedTest, ConstructCopy) {
  const AudioDevice device = CreateDevice();
  const AudioDevice device_copy(device);
  EXPECT_EQ(device, device_copy);
}

// Copy assignment operator is defined explicitly; ensure it behaves as
// expected.
TEST_P(AudioAndroidAudioDeviceParameterizedTest, AssignCopy) {
  const AudioDevice device = CreateDevice();
  AudioDevice device_copy = AudioDevice::Default();
  device_copy = device;
  EXPECT_EQ(device, device_copy);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    AudioAndroidAudioDeviceParameterizedTest,
    testing::Values(
        AudioDeviceParams{
            .id = AudioDeviceId::Default(),
            .type = AudioDeviceType::kUnknown,
            .name = std::nullopt,
            .sample_rates = std::nullopt,
            .associated_sco_device = std::nullopt,
            .expected_is_default = true,
        },
        AudioDeviceParams{
            .id = AudioDeviceId::NonDefault(100).value(),
            .type = AudioDeviceType::kBuiltinMic,
            .name = std::nullopt,
            .sample_rates = std::vector<int>(),
            .associated_sco_device = std::nullopt,
            .expected_is_default = false,
        },
        AudioDeviceParams{
            .id = AudioDeviceId::NonDefault(200).value(),
            .type = AudioDeviceType::kBuiltinSpeaker,
            .name = "Speaker",
            .sample_rates = std::vector<int>{1000, 2000, 3000, 4000, 5000},
            .associated_sco_device = std::nullopt,
            .expected_is_default = false,
        },
        AudioDeviceParams{.id = AudioDeviceId::NonDefault(100).value(),
                          .type = AudioDeviceType::kBluetoothA2dp,
                          .name = "A2DP",
                          .sample_rates = std::vector<int>{32000},
                          .associated_sco_device = AudioDevice(
                              AudioDeviceId::NonDefault(200).value(),
                              AudioDeviceType::kBluetoothSco,
                              "SCO",
                              std::vector<int>{16000}),
                          .expected_is_default = false}));

}  // namespace media::android
