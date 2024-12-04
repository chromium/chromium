// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include <CoreAudio/AudioHardware.h>

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Return;

namespace media {

class AudioDeviceListenerMacUnderTest final : public AudioDeviceListenerMac {
 public:
  AudioDeviceListenerMacUnderTest(base::RepeatingClosure listener_cb,
                                  bool monitor_output_sample_rate_changes,
                                  bool monitor_default_input,
                                  bool monitor_addition_removal,
                                  bool monitor_sources)
      : AudioDeviceListenerMac(std::move(listener_cb),
                               monitor_output_sample_rate_changes,
                               monitor_default_input,
                               monitor_addition_removal,
                               monitor_sources) {}

  ~AudioDeviceListenerMacUnderTest() final = default;

  MOCK_METHOD0(GetAllAudioDeviceIDs, std::vector<AudioObjectID>());
  MOCK_METHOD1(IsOutputDevice, bool(AudioObjectID));
  MOCK_METHOD2(GetDeviceSource, std::optional<uint32_t>(AudioObjectID, bool));

  OSStatus AddPropertyListener(AudioObjectID inObjectID,
                               const AudioObjectPropertyAddress* inAddress,
                               AudioObjectPropertyListenerProc inListener,
                               void* inClientData) final {
    return noErr;
  }
  OSStatus RemovePropertyListener(AudioObjectID inObjectID,
                                  const AudioObjectPropertyAddress* inAddress,
                                  AudioObjectPropertyListenerProc inListener,
                                  void* inClientData) final {
    return noErr;
  }
};

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
         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain}};

    for (void* context : contexts) {
      OSStatus status = AudioDeviceListenerMac::SimulateEventForTesting(
          kAudioObjectSystemObject, 2, addresses, context);
      if (status != noErr)
        return false;
    }
    return true;
  }

  static bool SimulateDeviceEvent(AudioObjectID id,
                                  std::vector<void*>& contexts,
                                  const AudioObjectPropertyAddress& address) {
    const AudioObjectPropertyAddress addresses[] = {address};
    for (void* context : contexts) {
      OSStatus status = AudioDeviceListenerMac::SimulateEventForTesting(
          id, 1, addresses, context);
      if (status != noErr)
        return false;
    }
    return true;
  }

  static bool SimulateSampleRateChange(AudioObjectID id,
                                       std::vector<void*>& contexts) {
    return SimulateDeviceEvent(
        id, contexts, AudioDeviceListenerMac::kPropertyOutputSampleRateChanged);
  }

  static bool SimulateOutputSourceChange(AudioObjectID id,
                                         std::vector<void*>& contexts) {
    return SimulateDeviceEvent(
        id, contexts, AudioDeviceListenerMac::kPropertyOutputSourceChanged);
  }

  static bool SimluateInputSourceChange(AudioObjectID id,
                                        std::vector<void*>& contexts) {
    return SimulateDeviceEvent(
        id, contexts, AudioDeviceListenerMac::kPropertyInputSourceChanged);
  }

  static void CreatePropertyListeners(AudioDeviceListenerMac* device_listener) {
    return device_listener->CreatePropertyListeners();
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
TEST_F(AudioDeviceListenerMacTest, Events_DeviceMonitoring) {
  auto device_listener = AudioDeviceListenerMac::Create(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/false,
      /*monitor_default_input=*/true, /*monitor_addition_removal=*/true,
      /*monitor_sources*/ false);

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

TEST_F(AudioDeviceListenerMacTest, Events_DefaultOutput) {
  auto device_listener = AudioDeviceListenerMac::Create(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/false,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/false,
      /*monitor_sources*/ false);

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());
  // Default output.
  EXPECT_EQ(property_listeners.size(), 1u);

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange(property_listeners));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnDeviceChange()).Times(0);
  ASSERT_TRUE(SimulateDefaultInputDeviceChange(property_listeners));
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest, EventsNotProcessedAfterLisneterDeletion) {
  auto device_listener = AudioDeviceListenerMac::Create(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/false,
      /*monitor_default_input=*/true, /*monitor_addition_removal=*/true,
      /*monitor_sources*/ false);

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

TEST_F(AudioDeviceListenerMacTest, SampleRateChangeSubscription) {
  auto device_listener = std::make_unique<AudioDeviceListenerMacUnderTest>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/true,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/false,
      /*monitor_sources*/ false);

  AudioDeviceListenerMacUnderTest& system_audio_mock = *device_listener.get();

  EXPECT_CALL(system_audio_mock, GetAllAudioDeviceIDs())
      .WillOnce(Return(std::vector<AudioObjectID>{1, 2, 3, 4}));

  EXPECT_CALL(system_audio_mock, IsOutputDevice(1)).WillOnce(Return(true));
  EXPECT_CALL(system_audio_mock, IsOutputDevice(2)).WillOnce(Return(false));
  EXPECT_CALL(system_audio_mock, IsOutputDevice(3)).WillOnce(Return(true));
  EXPECT_CALL(system_audio_mock, IsOutputDevice(4)).WillOnce(Return(false));

  CreatePropertyListeners(device_listener.get());

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());

  // Default output, addition-removal and two output devices
  EXPECT_EQ(property_listeners.size(), 4u);

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  // Output.
  SimulateSampleRateChange(1, property_listeners);
  // Not output - no callback.
  SimulateSampleRateChange(2, property_listeners);

  device_listener.reset();

  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest,
       SampleRateChangeSubscriptionUpdatedWhenDevicesAddedRemoved) {
  auto device_listener = std::make_unique<AudioDeviceListenerMacUnderTest>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/true,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/false,
      /*monitor_sources*/ false);

  AudioDeviceListenerMacUnderTest& system_audio_mock = *device_listener.get();

  EXPECT_CALL(system_audio_mock, GetAllAudioDeviceIDs())
      .WillOnce(Return(std::vector<AudioObjectID>{1}))
      .WillOnce(Return(std::vector<AudioObjectID>{}))
      .WillOnce(Return(std::vector<AudioObjectID>{1}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 2}));

  EXPECT_CALL(system_audio_mock, IsOutputDevice(1))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(system_audio_mock, IsOutputDevice(2)).WillOnce(Return(true));

  // We only change the subscription, nothing notification-worthy.
  EXPECT_CALL(*this, OnDeviceChange()).Times(0);

  CreatePropertyListeners(device_listener.get());

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());

  // Default output, addition-removal and one output device
  EXPECT_EQ(property_listeners.size(), 3u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and no output device
  EXPECT_EQ(property_listeners.size(), 2u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and one output device
  EXPECT_EQ(property_listeners.size(), 3u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and two output device
  EXPECT_EQ(property_listeners.size(), 4u);

  device_listener.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest,
       SampleRateChangeNotificationsForAddedDevices) {
  auto device_listener = std::make_unique<AudioDeviceListenerMacUnderTest>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/true,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/false,
      /*monitor_sources*/ false);

  AudioDeviceListenerMacUnderTest& system_audio_mock = *device_listener.get();

  EXPECT_CALL(system_audio_mock, GetAllAudioDeviceIDs())
      .WillOnce(Return(std::vector<AudioObjectID>{}))
      .WillOnce(Return(std::vector<AudioObjectID>{1}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 2}));

  EXPECT_CALL(system_audio_mock, IsOutputDevice(1))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(system_audio_mock, IsOutputDevice(2)).WillOnce(Return(true));

  CreatePropertyListeners(device_listener.get());

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and none output device
  EXPECT_EQ(property_listeners.size(), 2u);

  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(0);
    SimulateSampleRateChange(1, property_listeners);
    SimulateSampleRateChange(2, property_listeners);
    SimulateSampleRateChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and one output device
  EXPECT_EQ(property_listeners.size(), 3u);

  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(1);
    SimulateSampleRateChange(1, property_listeners);
    SimulateSampleRateChange(2, property_listeners);
    SimulateSampleRateChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and two output device
  EXPECT_EQ(property_listeners.size(), 4u);

  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(2);
    SimulateSampleRateChange(1, property_listeners);
    SimulateSampleRateChange(2, property_listeners);
    SimulateSampleRateChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  device_listener.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest,
       SourceChangeSubscriptionUpdatedWhenDevicesAddedRemoved) {
  auto device_listener = std::make_unique<AudioDeviceListenerMacUnderTest>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/false,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/true,
      /*monitor_sources*/ true);

  AudioDeviceListenerMacUnderTest& system_audio_mock = *device_listener.get();

  EXPECT_CALL(system_audio_mock, GetAllAudioDeviceIDs())
      .WillOnce(Return(std::vector<AudioObjectID>{1}))
      .WillOnce(Return(std::vector<AudioObjectID>{}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 2}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 3}));

  // Device 1 is an input device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(1, false))
      .Times(3)
      .WillRepeatedly(Return(std::optional<uint32_t>()));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(1, true))
      .Times(3)
      .WillRepeatedly(Return(123));

  // Device 2 is an output device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(2, false))
      .WillOnce(Return(123));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(2, true))
      .WillOnce(Return(std::optional<uint32_t>()));

  // Device 3 is both an input and output device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(3, false))
      .WillOnce(Return(123));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(3, true))
      .WillOnce(Return(123));

  // We add or remove devices three times, expect a call for each of them.
  EXPECT_CALL(*this, OnDeviceChange()).Times(3);

  CreatePropertyListeners(device_listener.get());

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());

  // Default output, addition-removal and one device source
  EXPECT_EQ(property_listeners.size(), 3u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and no device source
  EXPECT_EQ(property_listeners.size(), 2u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and two device sources
  EXPECT_EQ(property_listeners.size(), 4u);

  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and three device sources
  EXPECT_EQ(property_listeners.size(), 5u);

  device_listener.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AudioDeviceListenerMacTest, SourceChangeNotifications) {
  auto device_listener = std::make_unique<AudioDeviceListenerMacUnderTest>(
      base::BindRepeating(&AudioDeviceListenerMacTest::OnDeviceChange,
                          base::Unretained(this)),
      /*monitor_output_sample_rate_changes=*/false,
      /*monitor_default_input=*/false, /*monitor_addition_removal=*/true,
      /*monitor_sources*/ true);

  AudioDeviceListenerMacUnderTest& system_audio_mock = *device_listener.get();

  EXPECT_CALL(system_audio_mock, GetAllAudioDeviceIDs())
      .WillOnce(Return(std::vector<AudioObjectID>{1}))
      .WillOnce(Return(std::vector<AudioObjectID>{}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 2}))
      .WillOnce(Return(std::vector<AudioObjectID>{1, 3}));

  // Device 1 is an input device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(1, false))
      .Times(3)
      .WillRepeatedly(Return(std::optional<uint32_t>()));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(1, true))
      .Times(3)
      .WillRepeatedly(Return(123));

  // Device 2 is an output device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(2, false))
      .WillOnce(Return(123));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(2, true))
      .WillOnce(Return(std::optional<uint32_t>()));

  // Device 3 is both an input and output device.
  EXPECT_CALL(system_audio_mock, GetDeviceSource(3, false))
      .WillOnce(Return(123));
  EXPECT_CALL(system_audio_mock, GetDeviceSource(3, true))
      .WillOnce(Return(123));

  CreatePropertyListeners(device_listener.get());

  std::vector<void*> property_listeners =
      GetPropertyListeners(device_listener.get());

  // Default output, addition-removal and one device source
  EXPECT_EQ(property_listeners.size(), 3u);
  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(1);
    SimluateInputSourceChange(1, property_listeners);
    SimluateInputSourceChange(2, property_listeners);
    SimluateInputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnDeviceChange()).Times(0);
    SimulateOutputSourceChange(1, property_listeners);
    SimulateOutputSourceChange(2, property_listeners);
    SimulateOutputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_CALL(*this, OnDeviceChange());
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and no device source
  EXPECT_EQ(property_listeners.size(), 2u);
  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(0);
    SimluateInputSourceChange(1, property_listeners);
    SimluateInputSourceChange(2, property_listeners);
    SimluateInputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnDeviceChange()).Times(0);
    SimulateOutputSourceChange(1, property_listeners);
    SimulateOutputSourceChange(2, property_listeners);
    SimulateOutputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_CALL(*this, OnDeviceChange());
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and two device sources
  EXPECT_EQ(property_listeners.size(), 4u);
  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(1);
    SimluateInputSourceChange(1, property_listeners);
    SimluateInputSourceChange(2, property_listeners);
    SimluateInputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnDeviceChange()).Times(1);
    SimulateOutputSourceChange(1, property_listeners);
    SimulateOutputSourceChange(2, property_listeners);
    SimulateOutputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_CALL(*this, OnDeviceChange());
  ASSERT_TRUE(SimulateDeviceAdditionRemoval(property_listeners));
  property_listeners = GetPropertyListeners(device_listener.get());
  // Default output, addition-removal and three device sources
  EXPECT_EQ(property_listeners.size(), 5u);
  {
    EXPECT_CALL(*this, OnDeviceChange()).Times(2);
    SimluateInputSourceChange(1, property_listeners);
    SimluateInputSourceChange(2, property_listeners);
    SimluateInputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
    EXPECT_CALL(*this, OnDeviceChange()).Times(1);
    SimulateOutputSourceChange(1, property_listeners);
    SimulateOutputSourceChange(2, property_listeners);
    SimulateOutputSourceChange(3, property_listeners);
    testing::Mock::VerifyAndClearExpectations(this);
  }

  device_listener.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
