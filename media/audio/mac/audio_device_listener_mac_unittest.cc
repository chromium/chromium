// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include <CoreAudio/AudioHardware.h>

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioDeviceListenerMacTest : public testing::Test {
 public:
  AudioDeviceListenerMacTest() = default;

  AudioDeviceListenerMacTest(const AudioDeviceListenerMacTest&) = delete;
  AudioDeviceListenerMacTest& operator=(const AudioDeviceListenerMacTest&) =
      delete;

  ~AudioDeviceListenerMacTest() override = default;

  static bool SimulateEvent(const AudioObjectPropertyAddress& address,
                            std::vector<void*>& contexts) {
    // Include multiple addresses to ensure only a single device change event
    // occurs.
    const AudioObjectPropertyAddress addresses[] = {
        address,
        {kAudioHardwarePropertySleepingIsAllowed,
         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster}};

    for (void* context : contexts) {
      OSStatus status = AudioDeviceListenerMac::SimulateEventForTesting(
          kAudioObjectSystemObject, 2, addresses, context);
      if (status != noErr)
        return false;
    }
    return true;
  }

  static std::vector<void*> GetPropertyListeners(
      AudioDeviceListenerMac* device_listener) {
    return device_listener->GetPropertyListenersForTesting();
  }

  static bool SimulateDefaultOutputDeviceChange(std::vector<void*>& contexts) {
    return SimulateEvent(
        AudioDeviceListenerMac::kDefaultOutputDeviceChangePropertyAddress,
        contexts);
  }

  static bool SimulateDefaultInputDeviceChange(std::vector<void*>& contexts) {
    return SimulateEvent(
        AudioDeviceListenerMac::kDefaultInputDeviceChangePropertyAddress,
        contexts);
  }

  static bool SimulateDeviceAdditionRemoval(std::vector<void*>& contexts) {
    return SimulateEvent(AudioDeviceListenerMac::kDevicesPropertyAddress,
                         contexts);
  }

  MOCK_METHOD0(OnDeviceChange, void());

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<AudioDeviceListenerMac> device_listener_;
};

// Simulate a device change event and ensure we get the right callback.
TEST_F(AudioDeviceListenerMacTest, Events) {
  auto device_listener = std::make_unique<AudioDeviceListenerMac>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_default_input=*/true, /*monitor_addition_removal=*/true);

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());
  // Default output, default input, addition-removal.
  EXPECT_EQ(property_listeners.size(), 3u);

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(property_listeners));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultInputDeviceChange(property_listeners));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  base::RunLoop().RunUntilIdle();

  device_listener.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest, EventsNotProcessedAfterLisneterDeletion) {
  auto device_listener = std::make_unique<AudioDeviceListenerMac>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_default_input=*/true, /*monitor_addition_removal=*/true);

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());
  // Default output, default input, addition-removal.
  EXPECT_EQ(property_listeners.size(), 3u);

  // AudioDeviceListenerMac is destroyed, but property listener destructions are
  // delayed. Notifications on them should still work, but should not result
  // in OnDeviceChange call.
  device_listener.reset();

  EXPECT_CALL(*this, OnDeviceChange()).Times(0);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(property_listeners));
  ASSERT_TRUE(SimulateDefaultInputDeviceChange(property_listeners));
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));

  base::RunLoop().RunUntilIdle();
  // Now all property listeners are destroyed.
}

}  // namespace media
