// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/audio_device_listener_mac.h"

#include <CoreAudio/AudioHardware.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class AudioDeviceListenerMacTest : public testing::Test {
 public:
  AudioDeviceListenerMacTest() {
    // It's important to create the device listener from the message loop in
    // order to ensure we don't end up with unbalanced TaskObserver calls.
    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&AudioDeviceListenerMacTest::CreateDeviceListener,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  virtual ~AudioDeviceListenerMacTest() {
    // It's important to destroy the device listener from the message loop in
    // order to ensure we don't end up with unbalanced TaskObserver calls.
    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&AudioDeviceListenerMacTest::DestroyDeviceListener,
                   base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void CreateDeviceListener() {
    // Force a post task using BindToCurrentLoop() to ensure device listener
    // internals are working correctly.
    device_listener_.reset(new AudioDeviceListenerMac(
        BindToCurrentLoop(
            base::Bind(&AudioDeviceListenerMacTest::OnDeviceChange,
                       base::Unretained(this))),
        true /* monitor_default_input */, true /* monitor_addition_removal */));
  }

  void DestroyDeviceListener() { device_listener_.reset(); }

  bool ListenerIsValid() { return !device_listener_->listener_cb_.is_null(); }

  bool SimulateEvent(const AudioObjectPropertyAddress& address) {
    // Include multiple addresses to ensure only a single device change event
    // occurs.
    const AudioObjectPropertyAddress addresses[] = {
        address,
        {kAudioHardwarePropertySleepingIsAllowed,
         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster}};

    OSStatus status = device_listener_->OnEvent(
        kAudioObjectSystemObject, 2, addresses,
        device_listener_->default_output_listener_.get());
    if (status != noErr)
      return false;

    device_listener_->OnEvent(kAudioObjectSystemObject, 2, addresses,
                              device_listener_->default_input_listener_.get());
    if (status != noErr)
      return false;

    device_listener_->OnEvent(
        kAudioObjectSystemObject, 2, addresses,
        device_listener_->addition_removal_listener_.get());
    return status == noErr;
  }

  bool SimulateDefaultOutputDeviceChange() {
    return SimulateEvent(
        AudioDeviceListenerMac::kDefaultOutputDeviceChangePropertyAddress);
  }

  bool SimulateDefaultInputDeviceChange() {
    return SimulateEvent(
        AudioDeviceListenerMac::kDefaultInputDeviceChangePropertyAddress);
  }

  bool SimulateDeviceAdditionRemoval() {
    return SimulateEvent(AudioDeviceListenerMac::kDevicesPropertyAddress);
  }

  MOCK_METHOD0(OnDeviceChange, void());

 protected:
  base::MessageLoop message_loop_;
  std::unique_ptr<AudioDeviceListenerMac> device_listener_;

  DISALLOW_COPY_AND_ASSIGN(AudioDeviceListenerMacTest);
};

// Simulate a device change event and ensure we get the right callback.
TEST_F(AudioDeviceListenerMacTest, Events) {
  ASSERT_TRUE(ListenerIsValid());
  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultOutputDeviceChange());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDefaultInputDeviceChange());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, OnDeviceChange()).Times(1);
  ASSERT_TRUE(SimulateDeviceAdditionRemoval());
  base::RunLoop().RunUntilIdle();
}

}  // namespace media
