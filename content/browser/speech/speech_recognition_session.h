// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_SESSION_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_SESSION_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/speech/speech_recognizer.mojom.h"

namespace content {

// SpeechRecognitionSession implements the
// blink::mojom::SpeechRecognitionSession interface for a particular session. It
// also acts as a proxy for events sent from SpeechRecognitionManager, and
// forwards the events to the renderer using a
// mojo::Remote<SpeechRecognitionSessionClient> (that is passed from the render
// process).
class SpeechRecognitionSession : public blink::mojom::SpeechRecognitionSession,
                                 public SpeechRecognitionEventListener {
 public:
  explicit SpeechRecognitionSession(
      mojo::PendingRemote<blink::mojom::SpeechRecognitionSessionClient> client);
  ~SpeechRecognitionSession() override;
  base::WeakPtr<SpeechRecognitionSession> AsWeakPtr();

  void SetSessionId(int session_id) { session_id_ = session_id; }

  // blink::mojom::SpeechRecognitionSession implementation.
  void Abort() override;
  void StopCapture() override;

  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results)
      override;
  void OnRecognitionError(
      int session_id,
      const blink::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

 private:
  void ConnectionErrorHandler();

  int session_id_ = SpeechRecognitionManager::kSessionIDInvalid;
  mojo::Remote<blink::mojom::SpeechRecognitionSessionClient> client_;
  bool stopped_ = false;

  base::WeakPtrFactory<SpeechRecognitionSession> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_SESSION_H_
