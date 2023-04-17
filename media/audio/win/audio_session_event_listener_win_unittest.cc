// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_session_event_listener_win.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static bool DevicesAvailable() {
  return media::CoreAudioUtil::IsSupported() &&
         media::CoreAudioUtil::NumberOfActiveDevices(eRender) > 0;
}

}  // namespace

namespace media {

class AudioSessionEventListenerTest : public testing::Test {
 public:
  AudioSessionEventListenerTest() {
    if (!DevicesAvailable()) {
      return;
    }

    audio_client_ =
        CoreAudioUtil::CreateClient(std::string(), eRender, eConsole);
    CHECK(audio_client_);

    // AudioClient must be initialized to succeed in getting the session.
    WAVEFORMATEXTENSIBLE format;
    EXPECT_TRUE(SUCCEEDED(
        CoreAudioUtil::GetSharedModeMixFormat(audio_client_.Get(), &format)));

    // Perform a shared-mode initialization without event-driven buffer
    // handling.
    uint32_t endpoint_buffer_size = 0;
    HRESULT hr = CoreAudioUtil::SharedModeInitialize(
        audio_client_.Get(), &format, nullptr, 0, &endpoint_buffer_size,
        nullptr);
    EXPECT_TRUE(SUCCEEDED(hr));

    hr = audio_client_->GetService(IID_PPV_ARGS(&audio_session_control_));
    EXPECT_EQ(hr, S_OK);

    listener_ = Microsoft::WRL::Make<AudioSessionEventListener>(
        base::BindOnce(&AudioSessionEventListenerTest::OnSessionDisconnected,
                       base::Unretained(this)));

    if (audio_session_control_) {
      hr = audio_session_control_->RegisterAudioSessionNotification(
          listener_.Get());
      EXPECT_EQ(hr, S_OK);
    }
  }

  ~AudioSessionEventListenerTest() override {
    if (audio_session_control_ && listener_) {
      HRESULT hr = audio_session_control_->UnregisterAudioSessionNotification(
          listener_.Get());
      EXPECT_EQ(hr, S_OK);
    }
  }

  void SimulateSessionDisconnected() {
    listener_->OnSessionDisconnected(DisconnectReasonDeviceRemoval);
  }

  MOCK_METHOD0(OnSessionDisconnected, void());

 protected:
  base::win::ScopedCOMInitializer com_init_;
  Microsoft::WRL::ComPtr<IAudioClient> audio_client_;
  Microsoft::WRL::ComPtr<IAudioSessionControl> audio_session_control_;
  Microsoft::WRL::ComPtr<AudioSessionEventListener> listener_;
};

TEST_F(AudioSessionEventListenerTest, Works) {
  ABORT_AUDIO_TEST_IF_NOT(DevicesAvailable());

  EXPECT_CALL(*this, OnSessionDisconnected());
  SimulateSessionDisconnected();

  // Calling it again shouldn't crash, but also shouldn't make any more calls.
  EXPECT_CALL(*this, OnSessionDisconnected()).Times(0);
  SimulateSessionDisconnected();
}

}  // namespace media
