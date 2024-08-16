// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/speech/speech_recognition_manager_impl.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/soda/soda_util.h"
#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace content {

using testing::_;
using testing::InvokeWithoutArgs;

class SpeechRecognitionManagerImplTest : public testing::Test {
 public:
  SpeechRecognitionManagerImplTest();
  ~SpeechRecognitionManagerImplTest() override = default;

  // testing::Test methods.
  void SetUp() override;
  void TearDown() override;

 protected:
  MockSodaInstaller mock_soda_installer_;
  bool on_device_speech_recognition_supported_;

 private:
  // Set SODA On-Device speech recognition features flags.
  base::test::ScopedFeatureList scoped_feature_list_;
};

SpeechRecognitionManagerImplTest::SpeechRecognitionManagerImplTest() {
  // Setup the SODA On-Device feature flags.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/
      {
          media::kOnDeviceWebSpeech,
#if BUILDFLAG(IS_CHROMEOS_ASH)
          ash::features::kOnDeviceSpeechRecognition,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      },
      /*disabled_features=*/{});
}

void SpeechRecognitionManagerImplTest::SetUp() {
  on_device_speech_recognition_supported_ =
      speech::IsOnDeviceSpeechRecognitionSupported();
}

void SpeechRecognitionManagerImplTest::TearDown() {}

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

}  // namespace content
