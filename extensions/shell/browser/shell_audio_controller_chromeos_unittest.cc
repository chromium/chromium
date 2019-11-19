// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_audio_controller_chromeos.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_devices_pref_handler.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/audio_node.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::AudioDevice;
using chromeos::AudioNode;
using chromeos::AudioNodeList;

namespace extensions {

class ShellAudioControllerTest : public testing::Test {
 public:
  ShellAudioControllerTest() : next_node_id_(1) {
    chromeos::CrasAudioClient::InitializeFake();
    audio_client()->SetAudioNodesForTesting(AudioNodeList());
    chromeos::CrasAudioHandler::InitializeForTesting();

    controller_.reset(new ShellAudioController());
  }

  ~ShellAudioControllerTest() override {
    controller_.reset();
    chromeos::CrasAudioHandler::Shutdown();
    chromeos::CrasAudioClient::Shutdown();
  }

 protected:
  // Fills a AudioNode for use by tests.
  AudioNode CreateNode(chromeos::AudioDeviceType type) {
    AudioNode node;
    node.is_input =
        type == chromeos::AUDIO_TYPE_MIC ||
        type == chromeos::AUDIO_TYPE_INTERNAL_MIC ||
        type == chromeos::AUDIO_TYPE_KEYBOARD_MIC;
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

  chromeos::FakeCrasAudioClient* audio_client() {
    return chromeos::FakeCrasAudioClient::Get();
  }

  chromeos::CrasAudioHandler* audio_handler() {
    return chromeos::CrasAudioHandler::Get();
  }

  std::unique_ptr<ShellAudioController> controller_;

  // Next audio node ID to be returned by CreateNode().
  uint64_t next_node_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAudioControllerTest);
};

// Tests that higher-priority devices are activated as soon as they're
// connected.
TEST_F(ShellAudioControllerTest, SelectBestDevices) {
  AudioNode internal_speaker =
      CreateNode(chromeos::AUDIO_TYPE_INTERNAL_SPEAKER);
  AudioNode internal_mic = CreateNode(chromeos::AUDIO_TYPE_INTERNAL_MIC);
  AudioNode headphone = CreateNode(chromeos::AUDIO_TYPE_HEADPHONE);
  AudioNode external_mic = CreateNode(chromeos::AUDIO_TYPE_MIC);

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
  nodes.push_back(CreateNode(chromeos::AUDIO_TYPE_INTERNAL_SPEAKER));
  nodes.push_back(CreateNode(chromeos::AUDIO_TYPE_INTERNAL_MIC));
  audio_client()->SetAudioNodesAndNotifyObserversForTesting(nodes);

  EXPECT_FALSE(audio_handler()->IsOutputMuted());
  EXPECT_FALSE(audio_handler()->IsInputMuted());
  EXPECT_EQ(static_cast<double>(
                chromeos::AudioDevicesPrefHandler::kDefaultOutputVolumePercent),
            audio_handler()->GetOutputVolumePercent());

  // TODO(rkc): The default value for gain is wrong. http://crbug.com/442489
  EXPECT_EQ(75.0, audio_handler()->GetInputGainPercent());
}

}  // namespace extensions
