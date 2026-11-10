// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_util.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_speech_recognition_event_listener.h"
#include "media/audio/audio_system.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {

using testing::_;
using testing::NiceMock;

namespace {

class TestAudioSystem : public media::AudioSystem {
 public:
  void GetInputStreamParameters(const std::string& device_id,
                                OnAudioParamsCallback on_params_cb) override {
    std::move(on_params_cb)
        .Run(media::AudioParameters::UnavailableDeviceParams());
  }
  void GetOutputStreamParameters(const std::string& device_id,
                                 OnAudioParamsCallback on_params_cb) override {}
  void HasInputDevices(OnBoolCallback on_has_devices_cb) override {}
  void HasOutputDevices(OnBoolCallback on_has_devices_cb) override {}
  void GetDeviceDescriptions(
      bool for_input,
      OnDeviceDescriptionsCallback on_descriptions_cb) override {}
  void GetAssociatedOutputDeviceID(
      const std::string& input_device_id,
      OnDeviceIdCallback on_device_id_cb) override {}
  void GetInputDeviceInfo(
      const std::string& input_device_id,
      OnInputDeviceInfoCallback on_input_device_info_cb) override {}
};

class DeferredRecognitionDelegate
    : public FakeSpeechRecognitionManagerDelegate {
 public:
  DeferredRecognitionDelegate()
      : FakeSpeechRecognitionManagerDelegate(nullptr) {}

  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override {
    pending_callback_ = std::move(callback);
  }

  SpeechRecognitionEventListener* GetEventListener() override {
    return nullptr;
  }

  bool HasPendingCallback() const { return !pending_callback_.is_null(); }

  base::OnceCallback<void(bool ask_user, bool is_allowed)> TakeCallback() {
    return std::move(pending_callback_);
  }

 private:
  base::OnceCallback<void(bool ask_user, bool is_allowed)> pending_callback_;
};

}  // namespace

class SpeechRecognitionManagerImplTest
    : public RenderViewHostTestHarness,
      public media::mojom::SpeechRecognitionSessionClient {
 public:
  SpeechRecognitionManagerImplTest() = default;
  ~SpeechRecognitionManagerImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    manager_ = absl::WrapUnique(
        new SpeechRecognitionManagerImpl(&audio_system_, nullptr));
  }

  void TearDown() override {
    receiver_.reset();
    manager_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void SetDelegate(std::unique_ptr<SpeechRecognitionManagerDelegate> delegate) {
    manager_->delegate_ = std::move(delegate);
  }

  // media::mojom::SpeechRecognitionSessionClient:
  void ResultRetrieved(std::vector<media::mojom::WebSpeechRecognitionResultPtr>
                           results) override {}
  void ErrorOccurred(media::mojom::SpeechRecognitionErrorPtr error) override {
    error_ = error->code;
  }
  void Started() override {}
  void AudioStarted() override {}
  void SoundStarted() override {}
  void SoundEnded() override {}
  void AudioEnded() override {}
  void Ended() override { ended_ = true; }

 private:
  // Set up the SODA on device speech recognition feature flags.
  base::test::ScopedFeatureList feature_1_{media::kOnDeviceWebSpeech};
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList feature_2_{
      ash::features::kOnDeviceSpeechRecognition};
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  TestAudioSystem audio_system_;
  speech::MockSodaInstaller mock_soda_installer_;
  bool on_device_speech_recognition_supported_ =
      speech::IsOnDeviceSpeechRecognitionSupported();
  std::unique_ptr<SpeechRecognitionManagerImpl> manager_;
  mojo::Receiver<media::mojom::SpeechRecognitionSessionClient> receiver_{this};
  NiceMock<MockSpeechRecognitionEventListener> listener_;
  media::mojom::SpeechRecognitionErrorCode error_ =
      media::mojom::SpeechRecognitionErrorCode::kNone;
  bool ended_ = false;
};

// TODO(crbug.com/446260680): Disabled on Windows due to flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_SodaNotInstalled DISABLED_SodaNotInstalled
#else
#define MAYBE_SodaNotInstalled SodaNotInstalled
#endif
TEST_F(SpeechRecognitionManagerImplTest, MAYBE_SodaNotInstalled) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  // Set available languages of SODA to "en-US".
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages()).WillOnce([]() {
    std::vector<std::string> langs;
    langs.push_back("en-US");
    return langs;
  });

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kDownloadable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguagesNotAvailable) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();

  // Set available languages of SODA to be empty.
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages()).WillOnce([]() {
    return std::vector<std::string>();
  });

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kUnavailable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguageNotInstalled) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();

  // Set available languages of SODA to "en-US".
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages()).WillOnce([]() {
    std::vector<std::string> langs;
    langs.push_back("en-US");
    return langs;
  });

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kDownloadable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguageInstalled) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);

  // Set available languages of SODA to "en-US".
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages()).WillOnce([]() {
    std::vector<std::string> langs;
    langs.push_back("en-US");
    return langs;
  });

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kAvailable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLangcodeMatch) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();
  mock_soda_installer_.NotifySodaInstalledForTesting(
      speech::LanguageCode::kEnUs);

  // Set available languages of SODA to "en-US".
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages()).WillOnce([]() {
    std::vector<std::string> langs;
    langs.push_back("en-US");
    return langs;
  });

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kAvailable);
}

TEST_F(SpeechRecognitionManagerImplTest, LanguageNotSupportedError) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  SpeechRecognitionSessionConfig config;
  config.on_device = true;
  config.on_device_available = false;
  config.allow_cloud_fallback = false;
  config.language = "en-US";

  // Set available languages of SODA to be empty.
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillRepeatedly([]() { return std::vector<std::string>(); });
  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kUnavailable);

  base::HistogramTester histogram_tester;
  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt,
                          true);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::
                         kLanguageNotSupported &&
           ended_;
  }));
  histogram_tester.ExpectUniqueSample(
      "Accessibility.WebSpeech.SODA.ErrorOccurred",
      media::mojom::SpeechRecognitionErrorCode::kLanguageNotSupported, 1);
}

TEST_F(SpeechRecognitionManagerImplTest, AudioForwarderSampleRateTooHigh) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";

  std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config(
      std::in_place, mojo::NullReceiver(), /*channel_count=*/1,
      /*sample_rate=*/media::limits::kMaxSampleRate + 1);

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(),
                          audio_forwarder_config.value());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::kAudioCapture &&
           ended_;
  }));
}

TEST_F(SpeechRecognitionManagerImplTest, AudioForwarderSampleRateTooLow) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";

  std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config(
      std::in_place, mojo::NullReceiver(), /*channel_count=*/1,
      /*sample_rate=*/media::limits::kMinSampleRate - 1);

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(),
                          audio_forwarder_config.value());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::kAudioCapture &&
           ended_;
  }));
}

TEST_F(SpeechRecognitionManagerImplTest, AudioForwarderChannelCountTooLow) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";

  std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config(
      std::in_place, mojo::NullReceiver(), /*channel_count=*/0, 48000);

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(),
                          audio_forwarder_config.value());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::kAudioCapture &&
           ended_;
  }));
}

TEST_F(SpeechRecognitionManagerImplTest, AudioForwarderChannelCountTooHigh) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";

  std::optional<SpeechRecognitionAudioForwarderConfig> audio_forwarder_config(
      std::in_place, mojo::NullReceiver(),
      /*channel_count=*/media::limits::kMaxChannels + 1, 48000);

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(),
                          audio_forwarder_config.value());

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::kAudioCapture &&
           ended_;
  }));
}

TEST_F(SpeechRecognitionManagerImplTest, PhrasesNotSupportedError) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";
  config.recognition_context = media::SpeechRecognitionRecognitionContext();

  base::HistogramTester histogram_tester;
  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ ==
               media::mojom::SpeechRecognitionErrorCode::kPhrasesNotSupported &&
           ended_;
  }));
  histogram_tester.ExpectUniqueSample(
      "Accessibility.WebSpeech.Cloud.ErrorOccurred",
      media::mojom::SpeechRecognitionErrorCode::kPhrasesNotSupported, 1);
}

TEST_F(SpeechRecognitionManagerImplTest, ConfigEventListenerThrowsError) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";
  config.recognition_context = media::SpeechRecognitionRecognitionContext();
  config.event_listener = listener_.GetWeakPtr();

  EXPECT_CALL(
      listener_,
      OnRecognitionError(
          _, media::mojom::SpeechRecognitionError(
                 media::mojom::SpeechRecognitionErrorCode::kPhrasesNotSupported,
                 media::mojom::SpeechAudioErrorDetails::kNone)));
  EXPECT_CALL(listener_, OnRecognitionEnd(_));
  manager_->CreateSession(config, mojo::NullReceiver(), mojo::NullRemote(),
                          std::nullopt);
}

TEST_F(SpeechRecognitionManagerImplTest,
       StopCaptureDuringPermissionCheckPreventsStart) {
  auto delegate = std::make_unique<DeferredRecognitionDelegate>();
  DeferredRecognitionDelegate* delegate_ptr = delegate.get();
  SetDelegate(std::move(delegate));

  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";
  config.initial_context.global_id = main_rfh()->GetGlobalId();
  config.event_listener = listener_.GetWeakPtr();

  // Case 1: StopAudioCaptureForSession() arrives while
  // CheckRecognitionIsAllowed() is still pending. Running the permission
  // callback afterward must be ignored without requesting media access or
  // starting recognition.
  {
    const int session_id = manager_->CreateSession(config);
    manager_->StartSession(session_id);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return delegate_ptr->HasPendingCallback() &&
             SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                 main_rfh()->GetGlobalId()) == 1;
    }));

    auto permission_cb = delegate_ptr->TakeCallback();
    ASSERT_TRUE(permission_cb);

    EXPECT_CALL(listener_, OnRecognitionStart(session_id)).Times(0);
    EXPECT_CALL(listener_, OnRecognitionEnd(session_id)).Times(1);

    manager_->StopAudioCaptureForSession(session_id);
    std::move(permission_cb).Run(/*ask_user=*/true, /*is_allowed=*/true);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                 main_rfh()->GetGlobalId()) == 0;
    }));
  }

  // Case 2: RecognitionAllowedCallback() has already posted EVENT_START, and
  // StopAudioCaptureForSession() arrives before SessionStart() executes.
  {
    const int session_id = manager_->CreateSession(config);
    manager_->StartSession(session_id);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return delegate_ptr->HasPendingCallback() &&
             SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                 main_rfh()->GetGlobalId()) == 1;
    }));

    auto permission_cb = delegate_ptr->TakeCallback();
    ASSERT_TRUE(permission_cb);

    EXPECT_CALL(listener_, OnRecognitionStart(session_id)).Times(0);
    EXPECT_CALL(listener_, OnRecognitionEnd(session_id)).Times(1);

    std::move(permission_cb).Run(/*ask_user=*/false, /*is_allowed=*/true);
    manager_->StopAudioCaptureForSession(session_id);
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
                 main_rfh()->GetGlobalId()) == 0;
    }));
  }
}

TEST_F(SpeechRecognitionManagerImplTest, ResultsDroppedAfterAbortRequested) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";
  config.initial_context.global_id = main_rfh()->GetGlobalId();
  config.event_listener = listener_.GetWeakPtr();

  const int session_id = manager_->CreateSession(config);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               main_rfh()->GetGlobalId()) == 1;
  }));

  std::vector<media::mojom::WebSpeechRecognitionResultPtr> results;
  results.push_back(media::mojom::WebSpeechRecognitionResult::New());

  EXPECT_CALL(listener_, OnRecognitionResults(session_id, _)).Times(1);
  manager_->OnRecognitionResults(session_id, results);
  testing::Mock::VerifyAndClearExpectations(&listener_);

  // Once AbortSession() sets abort_requested, any recognition result already in
  // flight on the IO thread ahead of EVENT_ABORT must be dropped.
  EXPECT_CALL(listener_, OnRecognitionResults(session_id, _)).Times(0);
  EXPECT_CALL(listener_, OnRecognitionEnd(session_id)).Times(1);
  manager_->AbortSession(session_id);
  manager_->OnRecognitionResults(session_id, results);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return SpeechRecognitionManagerImpl::GetSessionTrackerCountForTesting(
               main_rfh()->GetGlobalId()) == 0;
  }));
}

}  // namespace content
