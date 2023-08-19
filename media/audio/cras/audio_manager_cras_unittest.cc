// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cras/audio_manager_cras.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/audio/cras/cras_util.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
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
  MOCK_METHOD(int, CrasGetNsSupported, (), (override));
  MOCK_METHOD(int, CrasGetAgcSupported, (), (override));
};

class AudioManagerCrasUnderTest : public AudioManagerCras {
 public:
  AudioManagerCrasUnderTest()
      : AudioManagerCras(std::make_unique<TestAudioThread>(),
                         &fake_audio_log_factory_) {}
  ~AudioManagerCrasUnderTest() = default;
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
    audio_manager_.reset(new StrictMock<AudioManagerCrasUnderTest>());
    base::RunLoop().RunUntilIdle();
  }
  ~AudioManagerCrasTest() override { audio_manager_->Shutdown(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<StrictMock<AudioManagerCrasUnderTest>> audio_manager_ = NULL;
};

TEST_F(AudioManagerCrasTest, HasAudioInputDevices) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kInput;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kInput))
      .WillOnce(testing::Return(devices));
  audio_manager_->SetCrasUtil(std::move(util));
  auto ret = audio_manager_->HasAudioInputDevices();
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
  audio_manager_->SetCrasUtil(std::move(util));
  audio_manager_->GetAudioInputDeviceNames(&device_names);
  EXPECT_EQ(device_names.empty(), true);
  audio_manager_->GetAudioOutputDeviceNames(&device_names);
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
  audio_manager_->SetCrasUtil(std::move(util));
  audio_manager_->GetAudioInputDeviceNames(&device_names);
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
  audio_manager_->SetCrasUtil(std::move(util));
  auto params = audio_manager_->GetPreferredOutputStreamParameters(
      "123", AudioParameters());
  EXPECT_EQ(params.channels(), 6);
}

TEST_F(AudioManagerCrasTest, UnsupportedMaxChannelsDefaultsToStereo) {
  std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
  std::vector<CrasDevice> devices;
  CrasDevice dev;
  dev.type = DeviceType::kOutput;
  dev.id = 123;
  dev.max_supported_channels = 100;
  devices.emplace_back(dev);
  EXPECT_CALL(*util, CrasGetDefaultOutputBufferSize());
  EXPECT_CALL(*util, CrasGetAudioDevices(DeviceType::kOutput))
      .WillRepeatedly(testing::Return(devices));
  audio_manager_->SetCrasUtil(std::move(util));
  auto params = audio_manager_->GetPreferredOutputStreamParameters(
      "123", AudioParameters());
  EXPECT_EQ(params.channel_layout(), ChannelLayout::CHANNEL_LAYOUT_STEREO);
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
  if (device_names.empty()) {
    return;
  }

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
  audio_manager_->SetCrasUtil(std::move(util));

  AudioDeviceNames device_names;
  audio_manager_->GetAudioInputDeviceNames(&device_names);
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
  audio_manager_->SetCrasUtil(std::move(util));

  AudioDeviceNames device_names;
  audio_manager_->GetAudioOutputDeviceNames(&device_names);
  CheckDeviceNames(device_names, expectation);
}

AudioParameters GetPreferredOutputStreamParameters(
    const ChannelLayoutConfig& channel_layout_config,
    int32_t user_buffer_size = 0) {
  // Generated AudioParameters should follow the same rule as in
  // AudioManagerCras::GetPreferredOutputStreamParameters().
  int sample_rate = 48000;  // kDefaultSampleRate
  int32_t buffer_size = user_buffer_size;
  if (buffer_size == 0) {  // Not user-provided.
    buffer_size = 512;     // kDefaultOutputBufferSize
  }
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
  audio_manager_->SetCrasUtil(std::move(util));

  AudioParameters params, golden_params;

  // channel_layout:
  //   kInternalSpeaker (2-channel): CHANNEL_LAYOUT_STEREO
  //   kUSB_6CH (6-channel): CHANNEL_LAYOUT_5_1
  //   HDMI (8-channel): CHANNEL_LAYOUT_7_1
  params = audio_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kInternalSpeaker.id), AudioParameters());
  golden_params =
      GetPreferredOutputStreamParameters(ChannelLayoutConfig::Stereo());
  EXPECT_TRUE(params.Equals(golden_params));
  params = audio_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kUSB_6CH.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_5_1>());
  EXPECT_TRUE(params.Equals(golden_params));
  params = audio_manager_->GetPreferredOutputStreamParameters(
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
  params = audio_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kInternalSpeaker.id), AudioParameters());
  golden_params =
      GetPreferredOutputStreamParameters(ChannelLayoutConfig::Stereo(), 2048);
  EXPECT_TRUE(params.Equals(golden_params));
  params = audio_manager_->GetPreferredOutputStreamParameters(
      base::NumberToString(kUSB_6CH.id), AudioParameters());
  golden_params = GetPreferredOutputStreamParameters(
      ChannelLayoutConfig::FromLayout<ChannelLayout::CHANNEL_LAYOUT_5_1>(),
      2048);
  EXPECT_TRUE(params.Equals(golden_params));
  params = audio_manager_->GetPreferredOutputStreamParameters(
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
  audio_manager_->SetCrasUtil(std::move(util));

  auto default_group_id = audio_manager_->GetGroupIDInput(
      audio_manager_->GetDefaultInputDeviceID());
  auto expected_group_id =
      audio_manager_->GetGroupIDInput(base::NumberToString(kExternalMic.id));
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
  audio_manager_->SetCrasUtil(std::move(util));

  auto default_group_id = audio_manager_->GetGroupIDOutput(
      audio_manager_->GetDefaultOutputDeviceID());
  auto expected_group_id =
      audio_manager_->GetGroupIDOutput(base::NumberToString(kHeadphone.id));
  EXPECT_EQ(default_group_id, expected_group_id);
}

constexpr int kAecTestGroupId = 9;
constexpr int kNoAecFlaggedGroupId = 0;

bool ExperimentalAecActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;
}

bool AecActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::ECHO_CANCELLER;
}

bool NsActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::NOISE_SUPPRESSION;
}

bool AgcActive(const AudioParameters& params) {
  return params.effects() & AudioParameters::AUTOMATIC_GAIN_CONTROL;
}

bool DspAecAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_ECHO_CANCELLER &&
         params.effects() & AudioParameters::ECHO_CANCELLER;
}

bool DspNsAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_NOISE_SUPPRESSION &&
         params.effects() & AudioParameters::NOISE_SUPPRESSION;
}

bool DspAgcAllowed(const AudioParameters& params) {
  return params.effects() & AudioParameters::ALLOW_DSP_AUTOMATIC_GAIN_CONTROL &&
         params.effects() & AudioParameters::AUTOMATIC_GAIN_CONTROL;
}

class AudioManagerCrasTestAEC
    : public AudioManagerCrasTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 protected:
  void SetUp() override {
    std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
    auto aec_supported = std::get<0>(GetParam());
    auto aec_group = std::get<1>(GetParam());
    auto ns_supported = std::get<2>(GetParam());
    auto agc_supported = std::get<3>(GetParam());

    EXPECT_CALL(*util, CrasGetAecSupported())
        .WillOnce(testing::Return(aec_supported));
    EXPECT_CALL(*util, CrasGetAecGroupId())
        .WillOnce(testing::Return(aec_group));
    EXPECT_CALL(*util, CrasGetNsSupported())
        .WillOnce(testing::Return(ns_supported));
    EXPECT_CALL(*util, CrasGetAgcSupported())
        .WillOnce(testing::Return(agc_supported));

    audio_manager_->SetCrasUtil(std::move(util));
  }
};

INSTANTIATE_TEST_SUITE_P(
    AllInputParameters,
    AudioManagerCrasTestAEC,
    ::testing::Combine(::testing::Values(false, true),
                       ::testing::Values(kNoAecFlaggedGroupId, kAecTestGroupId),
                       ::testing::Values(false, true),
                       ::testing::Values(false, true)));

TEST_P(AudioManagerCrasTestAEC, DefaultBehavior) {
  AudioParameters params = audio_manager_->GetInputStreamParameters("");
  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  auto aec_supported = std::get<0>(GetParam());
  auto ns_supported = std::get<2>(GetParam());
  auto agc_supported = std::get<3>(GetParam());

  // The current implementation is such that noise suppression and gain
  // control are not applied in CRAS if a tuned AEC is used.
  EXPECT_EQ(NsActive(params), ns_supported && (!aec_supported));
  EXPECT_EQ(AgcActive(params), agc_supported && (!aec_supported));
}

TEST_P(AudioManagerCrasTestAEC, DefaultBehaviorSystemAecEnforcedByPolicy) {
  base::test::ScopedFeatureList feature_list;

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSystemAecEnabled);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(AecActive(params));
}

TEST_P(AudioManagerCrasTestAEC,
       BehaviorWithCrOSEnforceSystemAecDisabledButEnforcedByPolicy) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndDisableFeature(media::kCrOSSystemAEC);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSystemAecEnabled);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(AecActive(params));
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAecDisallowed) {
  base::test::ScopedFeatureList feature_list;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  disabled_features.emplace_back(media::kCrOSEnforceSystemAec);
  disabled_features.emplace_back(media::kCrOSSystemAEC);
  feature_list.InitWithFeatures(enabled_features, disabled_features);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_FALSE(AecActive(params));
  EXPECT_FALSE(NsActive(params));
  EXPECT_FALSE(AgcActive(params));
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAecNsAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kCrOSEnforceSystemAecNsAgc);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAecNsAndAecAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{media::kCrOSEnforceSystemAecNs, {}},
       {media::kCrOSEnforceSystemAecAgc, {}}},
      {});
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}

TEST_P(AudioManagerCrasTestAEC,
       BehaviorWithCrOSEnforceSystemAecNsAgcAndDisallowedSystemAec) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{media::kCrOSEnforceSystemAecNsAgc, {}}}, {{media::kCrOSSystemAEC}});
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_TRUE(AgcActive(params));
  }
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAecNs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kCrOSEnforceSystemAecNs);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());
  auto agc_supported = std::get<3>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_TRUE(NsActive(params));
    EXPECT_EQ(AgcActive(params), agc_supported);
  }
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAecAgc) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kCrOSEnforceSystemAecAgc);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());
  auto ns_supported = std::get<2>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_EQ(NsActive(params), ns_supported);
    EXPECT_TRUE(AgcActive(params));
  }
}

TEST_P(AudioManagerCrasTestAEC, BehaviorWithCrOSEnforceSystemAec) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kCrOSEnforceSystemAec);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_supported = std::get<0>(GetParam());
  auto ns_supported = std::get<2>(GetParam());
  auto agc_supported = std::get<3>(GetParam());

  EXPECT_TRUE(ExperimentalAecActive(params));
  EXPECT_TRUE(AecActive(params));
  if (aec_supported) {
    EXPECT_FALSE(NsActive(params));
    EXPECT_FALSE(AgcActive(params));
  } else {
    EXPECT_EQ(NsActive(params), ns_supported);
    EXPECT_EQ(AgcActive(params), agc_supported);
  }
}

class AudioManagerCrasTestDSP
    : public AudioManagerCrasTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 protected:
  void SetUp() override {
    std::unique_ptr<MockCrasUtil> util = std::make_unique<MockCrasUtil>();
    aec_on_dsp_allowed_ = std::get<0>(GetParam());
    ns_on_dsp_allowed_ = std::get<1>(GetParam());
    agc_on_dsp_allowed_ = std::get<2>(GetParam());

    if (aec_on_dsp_allowed_) {
      enabled_features_.emplace_back(media::kCrOSDspBasedAecAllowed);
    } else {
      disabled_features_.emplace_back(media::kCrOSDspBasedAecAllowed);
    }

    if (ns_on_dsp_allowed_) {
      enabled_features_.emplace_back(media::kCrOSDspBasedNsAllowed);
    } else {
      disabled_features_.emplace_back(media::kCrOSDspBasedNsAllowed);
    }

    if (agc_on_dsp_allowed_) {
      enabled_features_.emplace_back(media::kCrOSDspBasedAgcAllowed);
    } else {
      disabled_features_.emplace_back(media::kCrOSDspBasedAgcAllowed);
    }

    EXPECT_CALL(*util, CrasGetAecSupported()).WillOnce(testing::Return(false));
    EXPECT_CALL(*util, CrasGetAecGroupId()).WillOnce(testing::Return(0));
    EXPECT_CALL(*util, CrasGetNsSupported()).WillOnce(testing::Return(false));
    EXPECT_CALL(*util, CrasGetAgcSupported()).WillOnce(testing::Return(false));

    audio_manager_->SetCrasUtil(std::move(util));
  }
  std::vector<base::test::FeatureRef> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  bool aec_on_dsp_allowed_;
  bool ns_on_dsp_allowed_;
  bool agc_on_dsp_allowed_;
};

INSTANTIATE_TEST_SUITE_P(AllInputParameters,
                         AudioManagerCrasTestDSP,
                         ::testing::Combine(::testing::Values(false, true),
                                            ::testing::Values(false, true),
                                            ::testing::Values(false, true)));

TEST_P(AudioManagerCrasTestDSP, BehaviorWithoutAnyEnforcedEffects) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  auto aec_on_dsp_allowed = std::get<0>(GetParam());
  EXPECT_EQ(DspAecAllowed(params), aec_on_dsp_allowed);
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AudioManagerCrasTestDSP, BehaviorWithCrOSEnforceSystemAec) {
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(media::kCrOSEnforceSystemAec);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AudioManagerCrasTestDSP, BehaviorWithCrOSEnforceSystemAecNs) {
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(media::kCrOSEnforceSystemAecNs);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_TRUE(DspNsAllowed(params) && ns_on_dsp_allowed_ ||
              !DspNsAllowed(params) && !ns_on_dsp_allowed_);
  EXPECT_FALSE(DspAgcAllowed(params));
}

TEST_P(AudioManagerCrasTestDSP, BehaviorWithCrOSEnforceSystemAecAgc) {
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(media::kCrOSEnforceSystemAecAgc);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_FALSE(DspNsAllowed(params));
  EXPECT_TRUE(DspAgcAllowed(params) && agc_on_dsp_allowed_ ||
              !DspAgcAllowed(params) && !agc_on_dsp_allowed_);
}

TEST_P(AudioManagerCrasTestDSP, BehaviorWithCrOSEnforceSystemAecNsAgc) {
  base::test::ScopedFeatureList feature_list;
  enabled_features_.emplace_back(media::kCrOSEnforceSystemAecNsAgc);
  feature_list.InitWithFeatures(enabled_features_, disabled_features_);
  AudioParameters params = audio_manager_->GetInputStreamParameters("");

  EXPECT_TRUE(DspAecAllowed(params) && aec_on_dsp_allowed_ ||
              !DspAecAllowed(params) && !aec_on_dsp_allowed_);
  EXPECT_TRUE(DspNsAllowed(params) && ns_on_dsp_allowed_ ||
              !DspNsAllowed(params) && !ns_on_dsp_allowed_);
  EXPECT_TRUE(DspAgcAllowed(params) && agc_on_dsp_allowed_ ||
              !DspAgcAllowed(params) && !agc_on_dsp_allowed_);
}

}  // namespace

}  // namespace media
