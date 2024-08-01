// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// A mocked version instance of SodaInstaller for testing purposes.
class MockSodaInstaller : public speech::SodaInstaller {
 public:
  MockSodaInstaller();
  MockSodaInstaller(const MockSodaInstaller&) = delete;
  MockSodaInstaller& operator=(const MockSodaInstaller&) = delete;
  ~MockSodaInstaller() override;

  MOCK_METHOD(base::FilePath, GetSodaBinaryPath, (), (const, override));
  MOCK_METHOD(base::FilePath,
              GetLanguagePath,
              (const std::string&),
              (const, override));
  MOCK_METHOD(void,
              InstallLanguage,
              (const std::string&, PrefService*),
              (override));
  MOCK_METHOD(void,
              UninstallLanguage,
              (const std::string&, PrefService*),
              (override));
  MOCK_METHOD(std::vector<std::string>,
              GetAvailableLanguages,
              (),
              (const, override));
  MOCK_METHOD(void, InstallSoda, (PrefService*), (override));
  MOCK_METHOD(void, UninstallSoda, (PrefService*), (override));
};

class MockOnDeviceWebSpeechRecognitionService
    : public media::mojom::SpeechRecognitionContext,
      public media::mojom::SpeechRecognitionRecognizer {
 public:
  explicit MockOnDeviceWebSpeechRecognitionService(
      content::BrowserContext* browser_context);
  MockOnDeviceWebSpeechRecognitionService(
      const MockOnDeviceWebSpeechRecognitionService&) = delete;
  MockOnDeviceWebSpeechRecognitionService& operator=(
      const MockOnDeviceWebSpeechRecognitionService&) = delete;
  ~MockOnDeviceWebSpeechRecognitionService() override;

  // SpeechRecognitionService
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver);

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;
  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override;

  // media::mojom::SpeechRecognitionRecognizer:
  MOCK_METHOD(void,
              SendAudioToSpeechRecognitionService,
              (media::mojom::AudioDataS16Ptr data),
              (override));
  MOCK_METHOD(void, OnLanguageChanged, (const std::string& lang), (override));
  MOCK_METHOD(void, OnMaskOffensiveWordsChanged, (bool changed), (override));
  void MarkDone() override;

  // Methods for testing plumbing to SpeechRecognitionRecognizerClient.
  void SendSpeechRecognitionResult(
      const media::SpeechRecognitionResult& result);

  void SendSpeechRecognitionError();

  base::WeakPtr<MockOnDeviceWebSpeechRecognitionService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnRecognizerClientDisconnected();

  void OnSpeechRecognitionRecognitionEventCallback(bool success);

  const raw_ptr<content::BrowserContext> browser_context_;

  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient>
      recognizer_client_remote_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizer>
      recognizer_receiver_{this};

  base::WeakPtrFactory<MockOnDeviceWebSpeechRecognitionService> weak_factory_{
      this};
};

class FakeSpeechRecognitionManagerDelegate
    : public SpeechRecognitionManagerDelegate {
 public:
  explicit FakeSpeechRecognitionManagerDelegate(
      MockOnDeviceWebSpeechRecognitionService* service)
      : speech_service_(service) {}

  ~FakeSpeechRecognitionManagerDelegate() override = default;

  // SpeechRecognitionManagerDelegate:
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  SpeechRecognitionEventListener* GetEventListener() override;
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;

  void Reset(MockOnDeviceWebSpeechRecognitionService* service);

 private:
  raw_ptr<MockOnDeviceWebSpeechRecognitionService> speech_service_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_FAKE_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
