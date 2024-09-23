// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/fake_speech_recognition_manager_delegate.h"

#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockSodaInstaller::MockSodaInstaller() = default;

MockSodaInstaller::~MockSodaInstaller() = default;

MockOnDeviceWebSpeechRecognitionService::
    MockOnDeviceWebSpeechRecognitionService(
        content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

MockOnDeviceWebSpeechRecognitionService::
    ~MockOnDeviceWebSpeechRecognitionService() = default;

// SpeechRecognitionService
void MockOnDeviceWebSpeechRecognitionService::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  speech_recognition_contexts_.Add(this, std::move(receiver));
}

// media::mojom::SpeechRecognitionContext:
void MockOnDeviceWebSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  recognizer_receiver_.Bind(std::move(receiver));
  recognizer_client_remote_.Bind(std::move(client));
  recognizer_client_remote_.set_disconnect_handler(base::BindOnce(
      &MockOnDeviceWebSpeechRecognitionService::OnRecognizerClientDisconnected,
      base::Unretained(this)));
  std::move(callback).Run(false);
}

void MockOnDeviceWebSpeechRecognitionService::BindWebSpeechRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate,
    media::mojom::SpeechRecognitionOptionsPtr options,
    bool continuous) {
  NOTIMPLEMENTED();
}

// media::mojom::SpeechRecognitionRecognizer:
void MockOnDeviceWebSpeechRecognitionService::MarkDone() {
  recognizer_client_remote_->OnSpeechRecognitionStopped();
}

// Methods for testing plumbing to SpeechRecognitionRecognizerClient.
void MockOnDeviceWebSpeechRecognitionService::SendSpeechRecognitionResult(
    const media::SpeechRecognitionResult& result) {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());

  recognizer_client_remote_->OnSpeechRecognitionRecognitionEvent(
      result, base::BindOnce(&MockOnDeviceWebSpeechRecognitionService::
                                 OnSpeechRecognitionRecognitionEventCallback,
                             base::Unretained(this)));
}

void MockOnDeviceWebSpeechRecognitionService::SendSpeechRecognitionError() {
  ASSERT_TRUE(recognizer_client_remote_.is_bound());
  recognizer_client_remote_->OnSpeechRecognitionError();
}

void MockOnDeviceWebSpeechRecognitionService::OnRecognizerClientDisconnected() {
  recognizer_client_remote_.reset();
  recognizer_receiver_.reset();
  speech_recognition_contexts_.Clear();
}

void MockOnDeviceWebSpeechRecognitionService::
    OnSpeechRecognitionRecognitionEventCallback(bool success) {
  if (!success) {
    OnRecognizerClientDisconnected();
    base::RunLoop().RunUntilIdle();
  }
}

// SpeechRecognitionManagerDelegate
void FakeSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  NOTREACHED_IN_MIGRATION();
}

SpeechRecognitionEventListener*
FakeSpeechRecognitionManagerDelegate::GetEventListener() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void FakeSpeechRecognitionManagerDelegate::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (speech_service_) {
    speech_service_->BindSpeechRecognitionContext(std::move(receiver));
  }
}

void FakeSpeechRecognitionManagerDelegate::Reset(
    MockOnDeviceWebSpeechRecognitionService* service) {
  speech_service_ = service;
}

}  // namespace content
