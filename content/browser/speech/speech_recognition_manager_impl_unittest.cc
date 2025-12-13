// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/soda/mock_soda_installer.h"
#include "components/soda/soda_util.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/mock_speech_recognition_event_listener.h"
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
using testing::InvokeWithoutArgs;
using testing::NiceMock;

class SpeechRecognitionManagerImplTest
    : public testing::Test,
      public media::mojom::SpeechRecognitionSessionClient {
 public:
  SpeechRecognitionManagerImplTest() = default;
  ~SpeechRecognitionManagerImplTest() override = default;

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
  BrowserTaskEnvironment environment_;

 protected:
  speech::MockSodaInstaller mock_soda_installer_;
  bool on_device_speech_recognition_supported_ =
      speech::IsOnDeviceSpeechRecognitionSupported();
  std::unique_ptr<SpeechRecognitionManagerImpl> manager_ =
      absl::WrapUnique(new SpeechRecognitionManagerImpl(nullptr, nullptr));
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
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kDownloadable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguagesNotAvailable) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();

  // Set available languages of SODA to be empty.
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() { return std::vector<std::string>(); }));

  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kUnavailable);
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguageNotInstalled) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();

  // Set available languages of SODA to "en-US".
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

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
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

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
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() {
        std::vector<std::string> langs;
        langs.push_back("en-US");
        return langs;
      }));

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
      .WillRepeatedly(
          InvokeWithoutArgs([]() { return std::vector<std::string>(); }));
  EXPECT_EQ(speech::GetSodaAvailabilityStatus("en-US"),
            media::mojom::AvailabilityStatus::kUnavailable);

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt,
                          true);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::
                         kLanguageNotSupported &&
           ended_;
  }));
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

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ ==
               media::mojom::SpeechRecognitionErrorCode::kPhrasesNotSupported &&
           ended_;
  }));
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

}  // namespace content
