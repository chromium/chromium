// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_service_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(AudioServiceUtilsTest, ConvertStreamTypeFromMojom) {
  EXPECT_EQ(ConvertStreamTypeFromMojom(crosapi::mojom::StreamType::kNone),
            api::audio::StreamType::kNone);
  EXPECT_EQ(ConvertStreamTypeFromMojom(crosapi::mojom::StreamType::kInput),
            api::audio::StreamType::kInput);
  EXPECT_EQ(ConvertStreamTypeFromMojom(crosapi::mojom::StreamType::kOutput),
            api::audio::StreamType::kOutput);
}

TEST(AudioServiceUtilsTest, ConvertStreamTypeToMojom) {
  EXPECT_EQ(crosapi::mojom::StreamType::kNone,
            ConvertStreamTypeToMojom(api::audio::StreamType::kNone));
  EXPECT_EQ(crosapi::mojom::StreamType::kInput,
            ConvertStreamTypeToMojom(api::audio::StreamType::kInput));
  EXPECT_EQ(crosapi::mojom::StreamType::kOutput,
            ConvertStreamTypeToMojom(api::audio::StreamType::kOutput));
}

TEST(AudioServiceUtilsTest, ConvertDeviceTypeFromMojom) {
  const int mojom_max_value =
      static_cast<int>(crosapi::mojom::DeviceType::kMaxValue);
  const int ext_max_value = static_cast<int>(api::audio::DeviceType::kMaxValue);

  ASSERT_EQ(ext_max_value, mojom_max_value);

  for (int i = 0; i <= mojom_max_value; ++i) {
    EXPECT_EQ(
        ConvertDeviceTypeFromMojom(static_cast<crosapi::mojom::DeviceType>(i)),
        static_cast<api::audio::DeviceType>(i));
  }
}

TEST(AudioServiceUtilsTest, ConvertDeviceTypeToMojom) {
  const int mojom_max_value =
      static_cast<int>(crosapi::mojom::DeviceType::kMaxValue);
  const int ext_max_value = static_cast<int>(api::audio::DeviceType::kMaxValue);

  ASSERT_EQ(ext_max_value, mojom_max_value);

  for (int i = 0; i <= ext_max_value; ++i) {
    EXPECT_EQ(ConvertDeviceTypeToMojom(static_cast<api::audio::DeviceType>(i)),
              static_cast<crosapi::mojom::DeviceType>(i));
  }
}

TEST(AudioServiceUtilsTest, ConvertDeviceFilterFromMojomNull) {
  crosapi::mojom::DeviceFilterPtr empty;
  EXPECT_EQ(ConvertDeviceFilterFromMojom(empty), nullptr);
}

TEST(AudioServiceUtilsTest, ConvertDeviceFilterFromMojomActiveState) {
  auto input = crosapi::mojom::DeviceFilter::New();
  std::unique_ptr<api::audio::DeviceFilter> result;

  input->includedActiveState =
      crosapi::mojom::DeviceFilter::ActiveState::kUnset;
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && !result->is_active);

  input->includedActiveState =
      crosapi::mojom::DeviceFilter::ActiveState::kInactive;
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && result->is_active && (*result->is_active == false));

  input->includedActiveState =
      crosapi::mojom::DeviceFilter::ActiveState::kActive;
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && result->is_active && (*result->is_active == true));
}

TEST(AudioServiceUtilsTest, ConvertDeviceFilterFromMojomStreamTypes) {
  auto input = crosapi::mojom::DeviceFilter::New();
  std::unique_ptr<api::audio::DeviceFilter> result;

  // first input without stream types
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && !result->stream_types);

  // empty stream types vector
  input->includedStreamTypes = std::vector<crosapi::mojom::StreamType>();
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && result->stream_types && result->stream_types->empty());

  // nonempty stream types vector
  input->includedStreamTypes = {crosapi::mojom::StreamType::kInput,
                                crosapi::mojom::StreamType::kOutput,
                                crosapi::mojom::StreamType::kInput};
  std::vector<api::audio::StreamType> expected = {
      api::audio::StreamType::kInput, api::audio::StreamType::kOutput,
      api::audio::StreamType::kInput};
  result = ConvertDeviceFilterFromMojom(input);
  EXPECT_TRUE(result && result->stream_types &&
              (*result->stream_types == expected));
}

// crosapi::mojom::DeviceFilterPtr ConvertDeviceFilterToMojom(
//     const api::audio::DeviceFilter* filter);

TEST(AudioServiceUtilsTest, ConvertDeviceFilterToMojomNull) {
  auto expected = crosapi::mojom::DeviceFilter::New();
  EXPECT_EQ(ConvertDeviceFilterToMojom(nullptr), expected);
}

TEST(AudioServiceUtilsTest, ConvertDeviceFilterToMojomActiveState) {
  crosapi::mojom::DeviceFilterPtr result;
  auto input = std::make_unique<api::audio::DeviceFilter>();

  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && (result->includedActiveState ==
                         crosapi::mojom::DeviceFilter::ActiveState::kUnset));

  input->is_active = false;
  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && (result->includedActiveState ==
                         crosapi::mojom::DeviceFilter::ActiveState::kInactive));

  input->is_active = true;
  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && (result->includedActiveState ==
                         crosapi::mojom::DeviceFilter::ActiveState::kActive));
}

TEST(AudioServiceUtilsTest, ConvertDeviceFilterToMojomStreamTypes) {
  crosapi::mojom::DeviceFilterPtr result;
  auto input = std::make_unique<api::audio::DeviceFilter>();

  input->stream_types = std::nullopt;
  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && !result->includedStreamTypes);

  input->stream_types.emplace();
  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && result->includedStreamTypes &&
              result->includedStreamTypes->empty());

  std::vector<crosapi::mojom::StreamType> expected = {
      crosapi::mojom::StreamType::kOutput, crosapi::mojom::StreamType::kInput,
      crosapi::mojom::StreamType::kInput};
  input->stream_types.emplace({api::audio::StreamType::kOutput,
                               api::audio::StreamType::kInput,
                               api::audio::StreamType::kInput});
  result = ConvertDeviceFilterToMojom(input.get());
  EXPECT_TRUE(result && result->includedStreamTypes &&
              (*result->includedStreamTypes == expected));
}

TEST(AudioServiceUtilsTest, ConvertAudioDeviceInfoFromMojom) {
  const std::string test_id = "test_id";
  const std::string test_display_name = "test_display_name";
  const std::string test_device_name = "test_device_name";
  const bool test_is_active = true;
  const int test_level = 33;
  const std::string test_stable_device_id = "test_stable_device_id";

  auto input = crosapi::mojom::AudioDeviceInfo::New();
  input->deviceName = test_device_name;
  input->deviceType = crosapi::mojom::DeviceType::kBluetooth;
  input->displayName = test_display_name;
  input->id = test_id;
  input->isActive = test_is_active;
  input->level = test_level;
  input->stableDeviceId = test_stable_device_id;
  input->streamType = crosapi::mojom::StreamType::kOutput;

  auto result = ConvertAudioDeviceInfoFromMojom(input);
  EXPECT_EQ(result.id, test_id);
  EXPECT_EQ(result.stream_type, api::audio::StreamType::kOutput);
  EXPECT_EQ(result.device_type, api::audio::DeviceType::kBluetooth);
  EXPECT_EQ(result.display_name, test_display_name);
  EXPECT_EQ(result.device_name, test_device_name);
  EXPECT_EQ(result.is_active, test_is_active);
  EXPECT_EQ(result.level, test_level);
  EXPECT_TRUE(result.stable_device_id &&
              (*result.stable_device_id == test_stable_device_id));
}

TEST(AudioServiceUtilsTest, ConvertAudioDeviceInfoToMojom) {
  const std::string test_id = "0";
  const std::string test_display_name = "Test Microphone";
  const std::string test_device_name = "mic555";
  const bool test_is_active = false;
  const int test_level = 50;
  const std::string test_stable_device_id = "5";

  api::audio::AudioDeviceInfo input;
  input.id = test_id;
  input.stream_type = api::audio::StreamType::kInput;
  input.device_type = api::audio::DeviceType::kMic;
  input.display_name = test_display_name;
  input.device_name = test_device_name;
  input.is_active = test_is_active;
  input.level = test_level;
  input.stable_device_id = test_stable_device_id;

  auto result = ConvertAudioDeviceInfoToMojom(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->id, test_id);
  EXPECT_EQ(result->streamType, crosapi::mojom::StreamType::kInput);
  EXPECT_EQ(result->deviceType, crosapi::mojom::DeviceType::kMic);
  EXPECT_EQ(result->displayName, test_display_name);
  EXPECT_EQ(result->deviceName, test_device_name);
  EXPECT_EQ(result->isActive, test_is_active);
  EXPECT_EQ(result->level, test_level);
  EXPECT_TRUE(result->stableDeviceId &&
              (*result->stableDeviceId == test_stable_device_id));
}

TEST(AudioServiceUtilsTest, ConvertDeviceIdListsToMojomNull) {
  auto result = ConvertDeviceIdListsToMojom(nullptr, nullptr);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->inputs.empty());
  EXPECT_TRUE(result->outputs.empty());
}

TEST(AudioServiceUtilsTest, ConvertDeviceIdListsToMojomInputs) {
  std::vector<std::string> test_inputs = {"id0", "id1", "test_id"};
  auto result = ConvertDeviceIdListsToMojom(&test_inputs, nullptr);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->inputs, test_inputs);
  EXPECT_TRUE(result->outputs.empty());
}

TEST(AudioServiceUtilsTest, ConvertDeviceIdListsToMojomOutputs) {
  std::vector<std::string> test_outputs = {"out0", "out1"};
  auto result = ConvertDeviceIdListsToMojom(nullptr, &test_outputs);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->inputs.empty());
  EXPECT_EQ(result->outputs, test_outputs);
}

}  // namespace extensions
