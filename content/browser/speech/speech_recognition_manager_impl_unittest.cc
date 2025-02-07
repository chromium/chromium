// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "components/soda/soda_util.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
#include "content/public/test/browser_task_environment.h"
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

class SpeechRecognitionManagerImplTest
    : public testing::Test,
      public media::mojom::SpeechRecognitionSessionClient {
 public:
  SpeechRecognitionManagerImplTest() = default;
  ~SpeechRecognitionManagerImplTest() override = default;

  void SetUp() override {
    // Set up the SODA on device speech recognition feature flags.
    scoped_feature_list_.InitWithFeatures(
        {
            media::kOnDeviceWebSpeech,
#if BUILDFLAG(IS_CHROMEOS)
            ash::features::kOnDeviceSpeechRecognition,
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        {});

    on_device_speech_recognition_supported_ =
        speech::IsOnDeviceSpeechRecognitionSupported();

    manager_ =
        absl::WrapUnique(new SpeechRecognitionManagerImpl(nullptr, nullptr));
  }

  void TearDown() override { manager_.reset(); }

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

 protected:
  MockSodaInstaller mock_soda_installer_;
  bool on_device_speech_recognition_supported_;
  std::unique_ptr<SpeechRecognitionManagerImpl> manager_;
  mojo::Receiver<media::mojom::SpeechRecognitionSessionClient> receiver_{this};
  media::mojom::SpeechRecognitionErrorCode error_ =
      media::mojom::SpeechRecognitionErrorCode::kNone;
  bool ended_ = false;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment environment_;
};

TEST_F(SpeechRecognitionManagerImplTest, SodaNotInstalled) {
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

  EXPECT_FALSE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));
}

TEST_F(SpeechRecognitionManagerImplTest, SodaLanguagesNotAvailable) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  mock_soda_installer_.NotifySodaInstalledForTesting();

  // Set available languages of SODA to be empty.
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillOnce(InvokeWithoutArgs([]() { return std::vector<std::string>(); }));

  EXPECT_FALSE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));
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

  EXPECT_FALSE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));
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

  EXPECT_TRUE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));
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

  EXPECT_TRUE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));
}

TEST_F(SpeechRecognitionManagerImplTest, LanguageNotSupportedError) {
  if (!on_device_speech_recognition_supported_) {
    return;
  }

  SpeechRecognitionSessionConfig config;
  config.on_device = true;
  config.allow_cloud_fallback = false;
  config.language = "en-US";

  // Set available languages of SODA to be empty.
  EXPECT_CALL(mock_soda_installer_, GetAvailableLanguages())
      .WillRepeatedly(
          InvokeWithoutArgs([]() { return std::vector<std::string>(); }));
  EXPECT_FALSE(speech::IsOnDeviceSpeechRecognitionAvailable("en-US"));

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::
                         kLanguageNotSupported &&
           ended_;
  }));
}

TEST_F(SpeechRecognitionManagerImplTest, RecognitionContextNotSupportedError) {
  SpeechRecognitionSessionConfig config;
  config.on_device = false;
  config.language = "en-US";
  config.recognition_context = media::SpeechRecognitionRecognitionContext();

  manager_->CreateSession(config, mojo::NullReceiver(),
                          receiver_.BindNewPipeAndPassRemote(), std::nullopt);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return error_ == media::mojom::SpeechRecognitionErrorCode::
                         kRecognitionContextNotSupported &&
           ended_;
  }));
}

}  // namespace content
