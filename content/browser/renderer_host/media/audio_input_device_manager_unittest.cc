// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_device_manager.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

using testing::InSequence;

namespace content {

class MockAudioInputDeviceManagerListener
    : public MediaStreamProviderListener {
 public:
  MockAudioInputDeviceManagerListener() {}
  ~MockAudioInputDeviceManagerListener() override {}

  MOCK_METHOD2(Opened,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Closed,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));
  MOCK_METHOD2(Aborted,
               void(blink::mojom::MediaStreamType,
                    const base::UnguessableToken&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioInputDeviceManagerListener);
};

// TODO(henrika): there are special restrictions for Android since
// AudioInputDeviceManager::Open() must be called on the audio thread.
// This test suite must be modified to run on Android.
// Flaky on Linux. See http://crbug.com/867397.
#if defined(OS_ANDROID) || defined(OS_LINUX)
#define MAYBE_AudioInputDeviceManagerTest DISABLED_AudioInputDeviceManagerTest
#else
#define MAYBE_AudioInputDeviceManagerTest AudioInputDeviceManagerTest
#endif

class MAYBE_AudioInputDeviceManagerTest : public testing::Test {
 public:
  MAYBE_AudioInputDeviceManagerTest() {}

 protected:
  virtual void Initialize() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    // AudioInputDeviceManager accesses AudioSystem from IO thread, so it never
    // runs on the same thread with it, even on Mac.
    audio_manager_ = media::AudioManager::CreateForTesting(
        std::make_unique<media::AudioThreadImpl>());
    // Flush the message loop to ensure proper initialization of AudioManager.
    base::RunLoop().RunUntilIdle();

    // Use fake devices.
    devices_.emplace_back(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                          "fake_device_1", "Fake Device 1");
    devices_.emplace_back(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                          "fake_device_2", "Fake Device 2");
  }

  void SetUp() override {
    Initialize();

    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    manager_ = new AudioInputDeviceManager(audio_system_.get());
    audio_input_listener_ =
        std::make_unique<MockAudioInputDeviceManagerListener>();
    manager_->RegisterListener(audio_input_listener_.get());

    // Wait until we get the list.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    manager_->UnregisterListener(audio_input_listener_.get());
    audio_manager_->Shutdown();
  }

  void WaitForOpenCompletion() {
    media::WaitableMessageLoopEvent event;
    audio_manager_->GetTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), event.GetClosure());
    // Runs the loop and waits for the audio thread to call event's
    // closure.
    event.RunAndWait();
    base::RunLoop().RunUntilIdle();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  scoped_refptr<AudioInputDeviceManager> manager_;
  std::unique_ptr<MockAudioInputDeviceManagerListener> audio_input_listener_;
  blink::MediaStreamDevices devices_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MAYBE_AudioInputDeviceManagerTest);
};

// Opens and closes the devices.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenAndCloseDevice) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  for (blink::MediaStreamDevices::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter) {
    // Opens/closes the devices.
    base::UnguessableToken session_id = manager_->Open(*iter);

    // Expected mock call with expected return value.
    EXPECT_CALL(
        *audio_input_listener_,
        Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);
    // Waits for the callback.
    WaitForOpenCompletion();

    manager_->Close(session_id);
    EXPECT_CALL(
        *audio_input_listener_,
        Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, session_id))
        .Times(1);

    // Waits for the callback.
    base::RunLoop().RunUntilIdle();
  }
}

// Opens multiple devices at one time and closes them later.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenMultipleDevices) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  std::unique_ptr<base::UnguessableToken[]> session_id(
      new base::UnguessableToken[devices_.size()]);

  // Opens the devices in a loop.
  for (blink::MediaStreamDevices::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Opens the devices.
    session_id[index] = manager_->Open(*iter);

    // Expected mock call with expected returned value.
    EXPECT_CALL(*audio_input_listener_,
                Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                       session_id[index]))
        .Times(1);

    // Waits for the callback.
    WaitForOpenCompletion();
  }

  // Checks if the session_ids are unique.
  for (size_t i = 0; i < devices_.size() - 1; ++i) {
    for (size_t k = i + 1; k < devices_.size(); ++k) {
      EXPECT_TRUE(session_id[i] != session_id[k]);
    }
  }

  for (size_t i = 0; i < devices_.size(); ++i) {
    // Closes the devices.
    manager_->Close(session_id[i]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                       session_id[i]))
        .Times(1);

    // Waits for the callback.
    base::RunLoop().RunUntilIdle();
  }
}

// Opens a non-existing device.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenNotExistingDevice) {
  InSequence s;

  blink::mojom::MediaStreamType stream_type =
      blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  blink::MediaStreamDevice dummy_device(stream_type, device_id, device_name);

  base::UnguessableToken session_id = manager_->Open(dummy_device);
  EXPECT_CALL(
      *audio_input_listener_,
      Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);

  // Waits for the callback.
  WaitForOpenCompletion();
}

// Opens default device twice.
TEST_F(MAYBE_AudioInputDeviceManagerTest, OpenDeviceTwice) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  // Opens and closes the default device twice.
  base::UnguessableToken first_session_id = manager_->Open(devices_.front());
  base::UnguessableToken second_session_id = manager_->Open(devices_.front());

  // Expected mock calls with expected returned values.
  EXPECT_NE(first_session_id, second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     second_session_id))
      .Times(1);
  // Waits for the callback.
  WaitForOpenCompletion();

  manager_->Close(first_session_id);
  manager_->Close(second_session_id);
  EXPECT_CALL(*audio_input_listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     first_session_id))
      .Times(1);
  EXPECT_CALL(*audio_input_listener_,
              Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                     second_session_id))
      .Times(1);
  // Waits for the callback.
  base::RunLoop().RunUntilIdle();
}

// Accesses then closes the sessions after opening the devices.
TEST_F(MAYBE_AudioInputDeviceManagerTest, AccessAndCloseSession) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  int index = 0;
  std::unique_ptr<base::UnguessableToken[]> session_id(
      new base::UnguessableToken[devices_.size()]);

  // Loops through the devices and calls Open()/Close()/GetOpenedDeviceById
  // for each device.
  for (blink::MediaStreamDevices::const_iterator iter = devices_.begin();
       iter != devices_.end(); ++iter, ++index) {
    // Note that no DeviceStopped() notification for Event Handler as we have
    // stopped the device before calling close.
    session_id[index] = manager_->Open(*iter);
    EXPECT_CALL(*audio_input_listener_,
                Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                       session_id[index]))
        .Times(1);
    WaitForOpenCompletion();

    const blink::MediaStreamDevice* device =
        manager_->GetOpenedDeviceById(session_id[index]);
    DCHECK(device);
    EXPECT_EQ(iter->id, device->id);
    manager_->Close(session_id[index]);
    EXPECT_CALL(*audio_input_listener_,
                Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                       session_id[index]))
        .Times(1);
    base::RunLoop().RunUntilIdle();
  }
}

// Access an invalid session.
TEST_F(MAYBE_AudioInputDeviceManagerTest, AccessInvalidSession) {
  InSequence s;

  // Opens the first device.
  blink::MediaStreamDevices::const_iterator iter = devices_.begin();
  base::UnguessableToken session_id = manager_->Open(*iter);
  EXPECT_CALL(
      *audio_input_listener_,
      Opened(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  WaitForOpenCompletion();

  // Access a non-opened device.
  // This should fail and return an empty blink::MediaStreamDevice.
  base::UnguessableToken invalid_session_id = base::UnguessableToken::Create();
  const blink::MediaStreamDevice* device =
      manager_->GetOpenedDeviceById(invalid_session_id);
  DCHECK(!device);

  manager_->Close(session_id);
  EXPECT_CALL(
      *audio_input_listener_,
      Closed(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE, session_id))
      .Times(1);
  base::RunLoop().RunUntilIdle();
}

class AudioInputDeviceManagerNoDevicesTest
    : public MAYBE_AudioInputDeviceManagerTest {
 public:
  AudioInputDeviceManagerNoDevicesTest() {}

 protected:
  void Initialize() override {
    // MockAudioManager has no input and no output audio devices.
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::AudioThreadImpl>());

    // Devices to request from AudioInputDeviceManager.
    devices_.emplace_back(blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE,
                          "tab_capture", "Tab capture");
    devices_.emplace_back(
        blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE,
        "desktop_capture", "Desktop capture");
    devices_.emplace_back(blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
                          "fake_device", "Fake Device");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManagerNoDevicesTest);
};

TEST_F(AudioInputDeviceManagerNoDevicesTest,
       ParametersValidWithoutAudioDevices) {
  ASSERT_FALSE(devices_.empty());

  InSequence s;

  for (const auto& device_request : devices_) {
    base::UnguessableToken session_id = manager_->Open(device_request);

    EXPECT_CALL(*audio_input_listener_, Opened(device_request.type, session_id))
        .Times(1);
    WaitForOpenCompletion();

    // Expects that device parameters stored by the manager are valid.
    const blink::MediaStreamDevice* device =
        manager_->GetOpenedDeviceById(session_id);
    EXPECT_TRUE(device->input.IsValid());

    manager_->Close(session_id);
    EXPECT_CALL(*audio_input_listener_, Closed(device_request.type, session_id))
        .Times(1);

    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace content
