// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_audio_controller_chromeos.h"

#include <stdint.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

using ::ash::AudioDevice;
using ::ash::AudioDeviceType;
using ::ash::AudioNode;
using ::ash::AudioNodeList;
using ::ash::CrasAudioHandler;

class ShellAudioControllerTest : public testing::Test {
 public:
  ShellAudioControllerTest() : next_node_id_(1) {
    ash::CrasAudioClient::InitializeFake();
    audio_client()->SetAudioNodesForTesting(AudioNodeList());
    CrasAudioHandler::InitializeForTesting();

    controller_ = std::make_unique<ShellAudioController>();
  }

  ShellAudioControllerTest(const ShellAudioControllerTest&) = delete;
  ShellAudioControllerTest& operator=(const ShellAudioControllerTest&) = delete;

  ~ShellAudioControllerTest() override {
    controller_.reset();
    CrasAudioHandler::Shutdown();
    ash::CrasAudioClient::Shutdown();
  }

 protected:
  // Fills a AudioNode for use by tests.
  AudioNode CreateNode(AudioDeviceType type) {
    AudioNode node;
    node.is_input = type == AudioDeviceType::kMic ||
                    type == AudioDeviceType::kInternalMic ||
                    type == AudioDeviceType::kKeyboardMic;
    node.id = next_node_id_++;
    node.type = AudioDevice::GetTypeString(type);
    return node;
  }

  // Changes the active state of the node with |id| in |nodes|.
  void SetNodeActive(AudioNodeList* nodes, uint64_t id, bool active) {
    for (AudioNodeList::iterator it = nodes->begin();
         it != nodes->end(); ++it) {
      if (it->id == id) {
        it->active = active;
        return;
      }
    }
    ASSERT_TRUE(false) << "Didn't find ID " << id;
  }

  ash::FakeCrasAudioClient* audio_client() {
    return ash::FakeCrasAudioClient::Get();
  }

  CrasAudioHandler* audio_handler() { return CrasAudioHandler::Get(); }

  std::unique_ptr<ShellAudioController> controller_;

  // Next audio node ID to be returned by CreateNode().
  uint64_t next_node_id_;
};

// Tests that higher-priority devices are activated as soon as they're
// connected.
TEST_F(ShellAudioControllerTest, SelectBestDevices) {
  AudioNode internal_speaker = CreateNode(AudioDeviceType::kInternalSpeaker);
  AudioNode internal_mic = CreateNode(AudioDeviceType::kInternalMic);
  AudioNode headphone = CreateNode(AudioDeviceType::kHeadphone);
  AudioNode external_mic = CreateNode(AudioDeviceType::kMic);

  // AudioDevice gives the headphone jack a higher priority than the internal
  // speaker and an external mic a higher priority than the internal mic, so we
  // should start out favoring headphones and the external mic.
  AudioNodeList all_nodes;
  all_nodes.push_back(internal_speaker);
  all_nodes.push_back(internal_mic);
  all_nodes.push_back(headphone);
  all_nodes.push_back(external_mic);
  audio_client()->SetAudioNodesAndNotifyObserversForTesting(all_nodes);
  EXPECT_EQ(headphone.id, audio_handler()->GetPrimaryActiveOutputNode());
  EXPECT_EQ(external_mic.id, audio_handler()->GetPrimaryActiveInputNode());

  // Unplug the headphones and mic and check that we switch to the internal
  // devices.
  AudioNodeList internal_nodes;
  internal_nodes.push_back(internal_speaker);
  internal_nodes.push_back(internal_mic);
  audio_client()->SetAudioNodesAndNotifyObserversForTesting(internal_nodes);
  EXPECT_EQ(internal_speaker.id, audio_handler()->GetPrimaryActiveOutputNode());
  EXPECT_EQ(internal_mic.id, audio_handler()->GetPrimaryActiveInputNode());

  // Switch back to the external devices. Mark the previously-activated internal
  // devices as being active so CrasAudioHandler doesn't complain.
  SetNodeActive(&all_nodes, internal_speaker.id, true);
  SetNodeActive(&all_nodes, internal_mic.id, true);
  audio_client()->SetAudioNodesAndNotifyObserversForTesting(all_nodes);
  EXPECT_EQ(headphone.id, audio_handler()->GetPrimaryActiveOutputNode());
  EXPECT_EQ(external_mic.id, audio_handler()->GetPrimaryActiveInputNode());
}

// Tests that active audio devices are unmuted and have correct initial volume.
TEST_F(ShellAudioControllerTest, InitialVolume) {
  AudioNodeList nodes;
  nodes.push_back(CreateNode(AudioDeviceType::kInternalSpeaker));
  nodes.push_back(CreateNode(AudioDeviceType::kInternalMic));
  audio_client()->SetAudioNodesAndNotifyObserversForTesting(nodes);

  EXPECT_FALSE(audio_handler()->IsOutputMuted());
  EXPECT_FALSE(audio_handler()->IsInputMuted());
  EXPECT_EQ(ash::AudioDevicesPrefHandler::kDefaultOutputVolumePercent,
            audio_handler()->GetOutputVolumePercent());

  EXPECT_EQ(75.0, audio_handler()->GetInputGainPercent());
}

}  // namespace extensions
