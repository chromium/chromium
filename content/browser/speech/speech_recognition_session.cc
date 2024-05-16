// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognition_session.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

SpeechRecognitionSession::SpeechRecognitionSession(
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient> client)
    : client_(std::move(client)) {
  client_.set_disconnect_handler(
      base::BindOnce(&SpeechRecognitionSession::ConnectionErrorHandler,
                     base::Unretained(this)));
}

SpeechRecognitionSession::~SpeechRecognitionSession() {
  // If a connection error happens and the session hasn't been stopped yet,
  // abort it.
  if (!stopped_) {
    Abort();
  }
}

base::WeakPtr<SpeechRecognitionSession> SpeechRecognitionSession::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SpeechRecognitionSession::Abort() {
  SpeechRecognitionManager::GetInstance()->AbortSession(session_id_);
  stopped_ = true;
}

void SpeechRecognitionSession::StopCapture() {
  SpeechRecognitionManager::GetInstance()->StopAudioCaptureForSession(
      session_id_);
  stopped_ = true;
}

// -------- SpeechRecognitionEventListener interface implementation -----------

void SpeechRecognitionSession::OnRecognitionStart(int session_id) {
  client_->Started();
}

void SpeechRecognitionSession::OnAudioStart(int session_id) {
  client_->AudioStarted();
}

void SpeechRecognitionSession::OnSoundStart(int session_id) {
  client_->SoundStarted();
}

void SpeechRecognitionSession::OnSoundEnd(int session_id) {
  client_->SoundEnded();
}

void SpeechRecognitionSession::OnAudioEnd(int session_id) {
  client_->AudioEnded();
}

void SpeechRecognitionSession::OnRecognitionEnd(int session_id) {
  client_->Ended();
  stopped_ = true;
  client_.reset();
}

void SpeechRecognitionSession::OnRecognitionResults(
    int session_id,
    const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results) {
  client_->ResultRetrieved(mojo::Clone(results));
}

void SpeechRecognitionSession::OnRecognitionError(
    int session_id,
    const media::mojom::SpeechRecognitionError& error) {
  if (!client_.is_bound()) {
    return;
  }
  client_->ErrorOccurred(media::mojom::SpeechRecognitionError::New(error));
}

// The events below are currently not used by speech JS APIs implementation.
void SpeechRecognitionSession::OnAudioLevelsChange(int session_id,
                                                   float volume,
                                                   float noise_volume) {}

void SpeechRecognitionSession::ConnectionErrorHandler() {
  if (!stopped_) {
    Abort();
  }
}

}  // namespace content
