// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/device_activate_type.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

using ::ash::AudioDevice;
using ::ash::AudioDeviceList;
using ::ash::AudioNode;
using ::ash::AudioNodeList;
using ::ash::CrasAudioHandler;

const uint64_t kJabraSpeaker1Id = 30001;
const uint64_t kJabraSpeaker1StableDeviceId = 80001;
const uint64_t kJabraSpeaker2Id = 30002;
const uint64_t kJabraSpeaker2StableDeviceId = 80002;
const uint64_t kHDMIOutputId = 30003;
const uint64_t kHDMIOutputStabeDevicelId = 80003;
const uint64_t kJabraMic1Id = 40001;
const uint64_t kJabraMic1StableDeviceId = 90001;
const uint64_t kJabraMic2Id = 40002;
const uint64_t kJabraMic2StableDeviceId = 90002;
const uint64_t kWebcamMicId = 40003;
const uint64_t kWebcamMicStableDeviceId = 90003;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  uint64_t stable_id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const uint32_t kInputAudioEffect = 1;
const uint32_t kOutputAudioEffect = 0;

const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

const AudioNodeInfo kJabraSpeaker1 = {
    false, kJabraSpeaker1Id, kJabraSpeaker1StableDeviceId, "Jabra Speaker",
    "USB", "Jabra Speaker 1"};

const AudioNodeInfo kJabraSpeaker2 = {
    false, kJabraSpeaker2Id, kJabraSpeaker2StableDeviceId, "Jabra Speaker",
    "USB", "Jabra Speaker 2"};

const AudioNodeInfo kHDMIOutput = {
    false,         kHDMIOutputId, kHDMIOutputStabeDevicelId,
    "HDMI output", "HDMI",        "HDA Intel MID"};

const AudioNodeInfo kJabraMic1 = {
    true,        kJabraMic1Id, kJabraMic1StableDeviceId,
    "Jabra Mic", "USB",        "Jabra Mic 1"};

const AudioNodeInfo kJabraMic2 = {
    true,        kJabraMic2Id, kJabraMic2StableDeviceId,
    "Jabra Mic", "USB",        "Jabra Mic 2"};

const AudioNodeInfo kUSBCameraMic = {
    true,         kWebcamMicId, kWebcamMicStableDeviceId,
    "Webcam Mic", "USB",        "Logitech Webcam"};

AudioNode CreateAudioNode(const AudioNodeInfo& info, int version) {
  return AudioNode(
      info.is_input, info.id, version == 2,
      // stable_device_id_v1:
      info.stable_id,
      // stable_device_id_v2:
      version == 2 ? info.stable_id ^ 0xFFFF : 0, info.device_name, info.type,
      info.name, false, 0,
      info.is_input ? kInputMaxSupportedChannels : kOutputMaxSupportedChannels,
      info.is_input ? kInputAudioEffect : kOutputAudioEffect,
      info.is_input ? kInputNumberOfVolumeSteps : kOutputNumberOfVolumeSteps);
}

class AudioApiTest : public ShellApiTest {
 public:
  AudioApiTest() = default;

  AudioApiTest(const AudioApiTest&) = delete;
  AudioApiTest& operator=(const AudioApiTest&) = delete;

  ~AudioApiTest() override = default;

  void SetUp() override {
    session_feature_type_ = extensions::ScopedCurrentFeatureSessionType(
        extensions::mojom::FeatureSessionType::kKiosk);

    ShellApiTest::SetUp();
  }

  void ChangeAudioNodes(const AudioNodeList& audio_nodes) {
    ash::FakeCrasAudioClient::Get()->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    base::RunLoop().RunUntilIdle();
  }

  CrasAudioHandler* audio_handler() { return CrasAudioHandler::Get(); }

 protected:
  std::unique_ptr<base::AutoReset<extensions::mojom::FeatureSessionType>>
      session_feature_type_;
};

IN_PROC_BROWSER_TEST_F(AudioApiTest, Audio) {
  // Set up the audio nodes for testing.
  AudioNodeList audio_nodes = {
      CreateAudioNode(kJabraSpeaker1, 2), CreateAudioNode(kJabraSpeaker2, 2),
      CreateAudioNode(kHDMIOutput, 2),    CreateAudioNode(kJabraMic1, 2),
      CreateAudioNode(kJabraMic2, 2),     CreateAudioNode(kUSBCameraMic, 2)};

  ChangeAudioNodes(audio_nodes);

  EXPECT_TRUE(RunAppTest("api_test/audio")) << message_;
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnLevelChangedOutputDevice) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 2),
                               CreateAudioNode(kHDMIOutput, 2)};
  ChangeAudioNodes(audio_nodes);

  // Verify the jabra speaker is the active output device.
  AudioDevice device;
  EXPECT_TRUE(audio_handler()->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kJabraSpeaker1.id);

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded");
  ASSERT_TRUE(LoadApp("api_test/audio/volume_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Change output device volume.
  const int kVolume = 60;
  audio_handler()->SetOutputVolumePercent(kVolume);

  // Verify the output volume is changed to the designated value.
  EXPECT_EQ(kVolume, audio_handler()->GetOutputVolumePercent());
  EXPECT_EQ(kVolume,
            audio_handler()->GetOutputVolumePercentForDevice(device.id));

  // Verify the background app got the OnOutputNodeVolumeChanged event
  // with the expected node id and volume value.
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnOutputMuteChanged) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 2),
                               CreateAudioNode(kHDMIOutput, 2)};
  ChangeAudioNodes(audio_nodes);

  // Verify the jabra speaker is the active output device.
  AudioDevice device;
  EXPECT_TRUE(audio_handler()->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kJabraSpeaker1.id);

  // Mute the output.
  audio_handler()->SetOutputMute(true);
  EXPECT_TRUE(audio_handler()->IsOutputMuted());

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded");
  ASSERT_TRUE(LoadApp("api_test/audio/output_mute_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Un-mute the output.
  audio_handler()->SetOutputMute(false);
  EXPECT_FALSE(audio_handler()->IsOutputMuted());

  // Verify the background app got the OnMuteChanged event
  // with the expected output un-muted state.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnInputMuteChanged) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraMic1, 2),
                               CreateAudioNode(kUSBCameraMic, 2)};
  ChangeAudioNodes(audio_nodes);

  // Set the jabra mic to be the active input device.
  AudioDevice jabra_mic(CreateAudioNode(kJabraMic1, 2));
  audio_handler()->SwitchToDevice(jabra_mic, true,
                                  ash::DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kJabraMic1.id, audio_handler()->GetPrimaryActiveInputNode());

  // Un-mute the input.
  audio_handler()->SetInputMute(
      false, CrasAudioHandler::InputMuteChangeMethod::kOther);
  EXPECT_FALSE(audio_handler()->IsInputMuted());

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded");
  ASSERT_TRUE(LoadApp("api_test/audio/input_mute_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Mute the input.
  audio_handler()->SetInputMute(
      true, CrasAudioHandler::InputMuteChangeMethod::kOther);
  EXPECT_TRUE(audio_handler()->IsInputMuted());

  // Verify the background app got the OnMuteChanged event
  // with the expected input muted state.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnNodesChangedAddNodes) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 2),
                               CreateAudioNode(kJabraSpeaker2, 2)};
  ChangeAudioNodes(audio_nodes);
  const size_t init_device_size = audio_nodes.size();

  AudioDeviceList audio_devices;
  audio_handler()->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size, audio_devices.size());

  // Load background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded");
  ASSERT_TRUE(LoadApp("api_test/audio/add_nodes"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Plug in HDMI output.
  audio_nodes.push_back(CreateAudioNode(kHDMIOutput, 2));
  ChangeAudioNodes(audio_nodes);
  audio_handler()->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size + 1, audio_devices.size());

  // Verify the background app got the OnNodesChanged event
  // with the new node added.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnNodesChangedRemoveNodes) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraMic1, 2),
                               CreateAudioNode(kJabraMic2, 2),
                               CreateAudioNode(kUSBCameraMic, 2)};
  ChangeAudioNodes(audio_nodes);
  const size_t init_device_size = audio_nodes.size();

  AudioDeviceList audio_devices;
  audio_handler()->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size, audio_devices.size());

  // Load background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded");
  ASSERT_TRUE(LoadApp("api_test/audio/remove_nodes"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Remove camera mic.
  audio_nodes.erase(audio_nodes.begin() + init_device_size - 1);
  ChangeAudioNodes(audio_nodes);
  audio_handler()->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size - 1, audio_devices.size());

  // Verify the background app got the onNodesChanged event
  // with the last node removed.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
