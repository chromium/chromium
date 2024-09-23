// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_message_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "media/audio/android/aaudio_stream_wrapper.h"
#include "media/audio/android/audio_manager_android.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <fuchsia/media/cpp/fidl_test_base.h>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "media/fuchsia/audio/fake_audio_capturer.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

namespace media {

#if BUILDFLAG(IS_FUCHSIA)
class FakeAudio : public fuchsia::media::testing::Audio_TestBase {
 public:
  FakeAudio()
      : audio_binding_(test_component_context_.additional_services(), this) {}

  // fuchsia::media::testing::Audio_TestBase
  void CreateAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request,
      bool is_loopback) override {
    capturer_.push_back(
        std::make_unique<FakeAudioCapturer>(std::move(request)));
  }
  void NotImplemented_(const std::string& name) override {
    FAIL() << "Unexpected call to: " << name;
  }

 private:
  base::TestComponentContextForProcess test_component_context_;
  base::ScopedServiceBinding<fuchsia::media::Audio> audio_binding_;
  std::vector<std::unique_ptr<FakeAudioCapturer>> capturer_;
};
#endif  // BUILDFLAG(IS_FUCHSIA)

// This class allows to find out if the callbacks are occurring as
// expected and if any error has been reported.
class TestInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  TestInputCallback(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)),
        callback_count_(0),
        had_error_(0) {}
  void OnData(const AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    if (!quit_closure_.is_null()) {
      ++callback_count_;
      if (callback_count_ >= 2) {
        std::move(quit_closure_).Run();
      }
    }
  }
  void OnError() override {
    if (!quit_closure_.is_null()) {
      ++had_error_;
      std::move(quit_closure_).Run();
    }
  }
  // Returns how many times OnData() has been called. This should not be called
  // until |quit_closure_| has run.
  int callback_count() const {
    DCHECK(quit_closure_.is_null());
    return callback_count_;
  }
  // Returns how many times the OnError callback was called. This should not be
  // called until |quit_closure_| has run.
  int had_error() const {
    DCHECK(quit_closure_.is_null());
    return had_error_;
  }

 private:
  base::OnceClosure quit_closure_;
  int callback_count_;
  int had_error_;
};

class AudioInputTest : public testing::TestWithParam<bool> {
 public:
  AudioInputTest()
      : message_loop_(base::MessagePumpType::UI),
        audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())),
        audio_input_stream_(nullptr) {
#if BUILDFLAG(IS_ANDROID)
    // The only parameter is used to enable/disable AAudio.
    should_use_aaudio_ = GetParam();
    if (should_use_aaudio_) {
      features_.InitAndEnableFeature(features::kUseAAudioInput);

      if (__builtin_available(android AAUDIO_MIN_API, *)) {
        aaudio_is_supported_ = true;
      }
    }
#endif
    base::RunLoop().RunUntilIdle();
  }

  AudioInputTest(const AudioInputTest&) = delete;
  AudioInputTest& operator=(const AudioInputTest&) = delete;

  ~AudioInputTest() override { audio_manager_->Shutdown(); }

 protected:
  bool InputDevicesAvailable() {
#if BUILDFLAG(IS_FUCHSIA)
    if (AudioDeviceInfoAccessorForTests(audio_manager_.get())
            .HasAudioInputDevices()) {
      return true;
    }
    // If the device has no audio input device, fake it.
    if (!fake_audio_) {
      fake_audio_ = std::make_unique<FakeAudio>();
    }
    return true;
#elif BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
    // TODO(crbug.com/40719640): macOS on ARM64 says it has devices, but won't
    // let any of them be opened or listed.
    return false;
#else
    return AudioDeviceInfoAccessorForTests(audio_manager_.get())
        .HasAudioInputDevices();
#endif
  }

  void MakeAudioInputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioInputTest::MakeAudioInputStream,
                                    base::Unretained(this)));
  }

  void CloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioInputStream::Close,
                                    base::Unretained(audio_input_stream_)));
    audio_input_stream_ = nullptr;
  }

  void OpenAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::BindOnce(&AudioInputTest::OpenAndClose, base::Unretained(this)));
  }

  void OpenStopAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(base::BindOnce(&AudioInputTest::OpenStopAndClose,
                                    base::Unretained(this)));
  }

  void OpenAndStartAudioInputStreamOnAudioThread(
      AudioInputStream::AudioInputCallback* sink) {
    RunOnAudioThread(base::BindOnce(&AudioInputTest::OpenAndStart,
                                    base::Unretained(this), sink));
  }

  void StopAndCloseAudioInputStreamOnAudioThread() {
    RunOnAudioThread(
        base::BindOnce(&AudioInputTest::StopAndClose, base::Unretained(this)));
  }

  void MakeAudioInputStream() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    AudioParameters params =
        AudioDeviceInfoAccessorForTests(audio_manager_.get())
            .GetInputStreamParameters(AudioDeviceDescription::kDefaultDeviceId);
    audio_input_stream_ = audio_manager_->MakeAudioInputStream(
        params, AudioDeviceDescription::kDefaultDeviceId,
        base::BindRepeating(&AudioInputTest::OnLogMessage,
                            base::Unretained(this)));
    ASSERT_TRUE(audio_input_stream_);
  }

  void OpenAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    ASSERT_TRUE(audio_input_stream_);
    EXPECT_EQ(audio_input_stream_->Open(),
              AudioInputStream::OpenOutcome::kSuccess);
    audio_input_stream_->Close();
    audio_input_stream_ = nullptr;
  }

  void OpenAndStart(AudioInputStream::AudioInputCallback* sink) {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    ASSERT_TRUE(audio_input_stream_);
    EXPECT_EQ(audio_input_stream_->Open(),
              AudioInputStream::OpenOutcome::kSuccess);
    audio_input_stream_->Start(sink);
  }

  void OpenStopAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    ASSERT_TRUE(audio_input_stream_);
    EXPECT_EQ(audio_input_stream_->Open(),
              AudioInputStream::OpenOutcome::kSuccess);
    audio_input_stream_->Stop();
    audio_input_stream_->Close();
    audio_input_stream_ = nullptr;
  }

  void StopAndClose() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    ASSERT_TRUE(audio_input_stream_);
    audio_input_stream_->Stop();
    audio_input_stream_->Close();
    audio_input_stream_ = nullptr;
  }

  // Synchronously runs the provided callback/closure on the audio thread.
  void RunOnAudioThread(base::OnceClosure closure) {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    std::move(closure).Run();
  }

  void OnLogMessage(const std::string& message) {}

  base::TestMessageLoop message_loop_;
#if BUILDFLAG(IS_FUCHSIA)
  std::unique_ptr<FakeAudio> fake_audio_;
#endif  // BUILDFLAG(IS_FUCHSIA)
  std::unique_ptr<AudioManager> audio_manager_;
  raw_ptr<AudioInputStream> audio_input_stream_;

  bool should_use_aaudio_ = false;
  bool aaudio_is_supported_ = false;
#if BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList features_;
#endif
};

// Test create and close of an AudioInputStream without recording audio.
TEST_P(AudioInputTest, CreateAndClose) {
  if (should_use_aaudio_ && !aaudio_is_supported_) {
    return;
  }

  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  CloseAudioInputStreamOnAudioThread();
}

// Test create, open and close of an AudioInputStream without recording audio.
// TODO(crbug.com/40262701): This test is failing on ios-blink-dbg-fyi bot.
#if BUILDFLAG(IS_IOS)
#define MAYBE_OpenAndClose DISABLED_OpenAndClose
#else
#define MAYBE_OpenAndClose OpenAndClose
#endif
TEST_P(AudioInputTest, MAYBE_OpenAndClose) {
  if (should_use_aaudio_ && !aaudio_is_supported_) {
    return;
  }

  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  OpenAndCloseAudioInputStreamOnAudioThread();
}

// Test create, open, stop and close of an AudioInputStream without recording.
// TODO(crbug.com/40262701): This test is failing on ios-blink-dbg-fyi bot.
#if BUILDFLAG(IS_IOS)
#define MAYBE_OpenStopAndClose DISABLED_OpenStopAndClose
#else
#define MAYBE_OpenStopAndClose OpenStopAndClose
#endif
TEST_P(AudioInputTest, MAYBE_OpenStopAndClose) {
  if (should_use_aaudio_ && !aaudio_is_supported_) {
    return;
  }

  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();
  OpenStopAndCloseAudioInputStreamOnAudioThread();
}

// Test a normal recording sequence using an AudioInputStream.
// Very simple test which starts capturing and verifies that recording starts.
// TODO(crbug.com/40262701): This test is failing on ios-blink-dbg-fyi bot.
#if BUILDFLAG(IS_IOS)
#define MAYBE_Record DISABLED_Record
#else
#define MAYBE_Record Record
#endif
TEST_P(AudioInputTest, MAYBE_Record) {
  if (should_use_aaudio_ && !aaudio_is_supported_) {
    return;
  }

  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  MakeAudioInputStreamOnAudioThread();

  base::RunLoop run_loop;
  TestInputCallback test_callback(run_loop.QuitClosure());
  OpenAndStartAudioInputStreamOnAudioThread(&test_callback);

  run_loop.Run();
  EXPECT_GE(test_callback.callback_count(), 2);
  EXPECT_FALSE(test_callback.had_error());

  StopAndCloseAudioInputStreamOnAudioThread();
}

// The test parameter is only relevant on Android. It controls whether or not we
// allow the use of AAudio.
INSTANTIATE_TEST_SUITE_P(Base, AudioInputTest, testing::Values(false));

#if BUILDFLAG(IS_ANDROID)
// Run tests with AAudio enabled. On Android P and below, these tests should not
// run, as we only use AAudio on Q+.
INSTANTIATE_TEST_SUITE_P(AAudio, AudioInputTest, testing::Values(true));
#endif

}  // namespace media
