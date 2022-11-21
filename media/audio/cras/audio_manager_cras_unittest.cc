// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "media/audio/cras/cras_util.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/limits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/logging.h"

using testing::StrictMock;

namespace media {

namespace {

class MockCrasUtil : public CrasUtil {
 public:
  MOCK_METHOD(std::vector<CrasDevice>,
              CrasGetAudioDevices,
              (DeviceType type),
              (override));
  MOCK_METHOD(int, CrasGetAecSupported, (), (override));
  MOCK_METHOD(int, CrasGetAecGroupId, (), (override));
  MOCK_METHOD(int, CrasGetDefaultOutputBufferSize, (), (override));
};

class MockAudioManagerCras : public AudioManagerCras {
 public:
  MockAudioManagerCras()
      : AudioManagerCras(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  ~MockAudioManagerCras() = default;
  void SetCrasUtil(std::unique_ptr<CrasUtil> util) {
    cras_util_ = std::move(util);
  }
  using AudioManagerCras::GetPreferredOutputStreamParameters;

 private:
  FakeAudioLogFactory fake_audio_log_factory_;
};

class AudioManagerCrasTest : public testing::Test {
 protected:
  AudioManagerCrasTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerCras>());
    base::RunLoop().RunUntilIdle();
  }
  ~AudioManagerCrasTest() override { mock_manager_->Shutdown(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<MockAudioManagerCras>> mock_manager_ = NULL;
};

TEST_F(AudioManagerCrasTest, HasAudioInputDevices) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kInput;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  auto ret = mock_manager_->HasAudioInputDevices();
  EXPECT_EQ(ret, true);
}

TEST_F(AudioManagerCrasTest, CheckDefaultNoDevice) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  AudioDeviceNames device_names;
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  mock_manager_->GetAudioInputDeviceNames(&device_names);
  EXPECT_EQ(device_names.empty(), true);
  mock_manager_->GetAudioOutputDeviceNames(&device_names);
  EXPECT_EQ(device_names.empty(), true);
}

TEST_F(AudioManagerCrasTest, CheckDefaultDevice) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  AudioDeviceNames device_names;
  CrasDevice dev;
  dev.type = DeviceType::kInput;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  mock_manager_->GetAudioInputDeviceNames(&device_names);
  EXPECT_EQ(device_names.size(), 2u);
}

TEST_F(AudioManagerCrasTest, MaxChannel) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kOutput;
  dev.id = 123;
  dev.max_supported_channels = 6;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetDefaultOutputBufferSize());
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));
  auto params = mock_manager_->GetPreferredOutputStreamParameters(
      "123", AudioParameters());
  EXPECT_EQ(params.channels(), 6);
}

const CrasDevice kInternalSpeaker(DeviceType::kOutput,
                                  0,
                                  0,
                                  2,
                                  true,
                                  false,
                                  "INTERNAL_SPEAKER",
                                  "Internal Speaker",
                                  "Fake Soundcard");

const CrasDevice kInternalMic(DeviceType::kInput,
                              1,
                              1,
                              2,
                              true,
                              false,
                              "INTERNAL_MIC",
                              "Internal Mic",
                              "Fake Soundcard");

const CrasDevice kHeadphone(DeviceType::kOutput,
                            2,
                            2,
                            2,
                            true,
                            false,
                            "HEADPHONE",
                            "Headphone",
                            "Fake Soundcard");

const CrasDevice kExternalMic(DeviceType::kInput,
                              3,
                              3,
                              2,
                              true,
                              false,
                              "MIC",
                              "Mic",
                              "Fake Soundcard");

const CrasDevice
    kUSB(DeviceType::kOutput, 4, 4, 2, true, false, "USB", "USB", "Fake USB");

const CrasDevice kUSB_6CH(DeviceType::kOutput,
                          5,
                          5,
                          6,
                          true,
                          false,
                          "USB",
                          "USB 6ch",
                          "Fake USB 6ch");

const CrasDevice kHDMI(DeviceType::kOutput,
                       6,
                       6,
                       8,
                       true,
                       false,
                       "HDMI",
                       "HDMI",
                       "Fake HDMI");
void CheckDeviceNames(const AudioDeviceNames& device_names,
                      const std::map<uint64_t, std::string>& expectation) {
  EXPECT_EQ(device_names.empty(), expectation.empty());
  if (device_names.empty())
    return;

  AudioDeviceNames::const_iterator it = device_names.begin();

  // The first device in the list should always be the default device.
  EXPECT_TRUE(AudioDeviceDescription::IsDefaultDevice(it->unique_id));
  it++;

  while (it != device_names.end()) {
    uint64_t key;
    EXPECT_TRUE(base::StringToUint64(it->unique_id, &key));
    EXPECT_TRUE(expectation.find(key) != expectation.end());
    EXPECT_EQ(expectation.find(key)->second, it->device_name);
    it++;
  }
}

TEST_F(AudioManagerCrasTest, EnumerateInputDevices) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  std::map<uint64_t, std::string> expectation;
  devices.emplace_back(kInternalMic);
  devices.emplace_back(kExternalMic);
  expectation[kInternalMic.id] = kInternalMic.name;
  expectation[kExternalMic.id] = kExternalMic.name;

  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));

  AudioDeviceNames device_names;
  mock_manager_->GetAudioInputDeviceNames(&device_names);
  CheckDeviceNames(device_names, expectation);
}

TEST_F(AudioManagerCrasTest, EnumerateOutputDevices) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  std::map<uint64_t, std::string> expectation;
  devices.emplace_back(kInternalSpeaker);
  devices.emplace_back(kHeadphone);
  expectation[kInternalSpeaker.id] = kInternalSpeaker.name;
  expectation[kHeadphone.id] = kHeadphone.name;

  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));

  AudioDeviceNames device_names;
  mock_manager_->GetAudioOutputDeviceNames(&device_names);
  CheckDeviceNames(device_names, expectation);
}

AudioParameters GetPreferredOutputStreamParameters(
    const ChannelLayoutConfig& channel_layout_config,
    int32_t user_buffer_size = 0) {
  // Generated AudioParameters should follow the same rule as in
  // AudioManagerCras::GetPreferredOutputStreamParameters().
  int sample_rate = 48000;  // kDefaultSampleRate
  int32_t buffer_size = user_buffer_size;
  if (buffer_size == 0)  // Not user-provided.
    buffer_size = 512;   // kDefaultOutputBufferSize
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, channel_layout_config,
      sample_rate, buffer_size,
      AudioParameters::HardwareCapabilities(limits::kMinAudioBufferSize,
                                            limits::kMaxAudioBufferSize));
}

TEST_F(AudioManagerCrasTest, CheckOutputStreamParameters) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  devices.emplace_back(kInternalSpeaker);
  devices.emplace_back(kUSB_6CH);
  devices.emplace_back(kHDMI);

  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  EXPECT_CALL(*util, CrasGetDefaultOutputBufferSize())
      .WillRepeatedly(testing::Return(512));
  mock_manager_->SetCrasUtil(std::move(util));

  AudioParameters params, golden_params;

  // channel_layout:
  //   kInternalSpeaker (2-channel): CHANNEL_LAYOUT_STEREO
  //   kUSB_6CH (6-channel): CHANNEL_LAYOUT_5_1
  //   HDMI (8-channel): CHANNEL_LAYOUT_7_1
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kInternalSpeaker.id), AudioParameters());
  golden_params =
      GetPreferredOutputStreamParameters(ChannelLayoutConfig::Stereo());
  EXPECT_TRUE(params.Equals(golden_params));
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kUSB_6CH.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_5_1>());
  EXPECT_TRUE(params.Equals(golden_params));
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kHDMI.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_7_1>());
  EXPECT_TRUE(params.Equals(golden_params));

  // Set user-provided audio buffer size by command line, then check the buffer
  // size in stream parameters is equal to the user-provided one.
  int argc = 2;
  char const* argv0 = "dummy";
  char const* argv1 = "--audio-buffer-size=2048";
  const char* argv[] = {argv0, argv1, 0};
  base::CommandLine::Reset();
  EXPECT_TRUE(base::CommandLine::Init(argc, argv));
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kInternalSpeaker.id), AudioParameters());
  golden_params =
      GetPreferredOutputStreamParameters(ChannelLayoutConfig::Stereo(), 2048);
  EXPECT_TRUE(params.Equals(golden_params));
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kUSB_6CH.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_5_1>(),
      2048);
  EXPECT_TRUE(params.Equals(golden_params));
  params = mock_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kHDMI.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_7_1>(),
      2048);
  EXPECT_TRUE(params.Equals(golden_params));
}

TEST_F(AudioManagerCrasTest, LookupDefaultInputDeviceWithProperGroupId) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  devices.emplace_back(kInternalMic);
  devices.emplace_back(kExternalMic);
  devices[1].active = true;

  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));

  auto default_group_id =
      mock_manager_->GetGroupIDInput(mock_manager_->GetDefaultInputDeviceID());
  auto expected_group_id =
      mock_manager_->GetGroupIDInput(base::NumberToString(kExternalMic.id));
  EXPECT_EQ(default_group_id, expected_group_id);
}

TEST_F(AudioManagerCrasTest, LookupDefaultOutputDeviceWithProperGroupId) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  devices.emplace_back(kInternalSpeaker);
  devices.emplace_back(kHeadphone);
  devices[1].active = true;

  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  mock_manager_->SetCrasUtil(std::move(util));

  auto default_group_id =
      mock_manager_->GetGroupIDOutput(mock_manager_->GetDefaultOutputDeviceID());
  auto expected_group_id =
      mock_manager_->GetGroupIDOutput(base::NumberToString(kHeadphone.id));
  EXPECT_EQ(default_group_id, expected_group_id);
}

}  // namespace

}  // namespace media
