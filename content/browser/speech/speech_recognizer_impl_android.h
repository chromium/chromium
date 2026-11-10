// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_ANDROID_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"

namespace content {

class SpeechRecognitionEventListener;

class CONTENT_EXPORT SpeechRecognizerImplAndroid : public SpeechRecognizer {
 public:
  class UiPeer;

  SpeechRecognizerImplAndroid(SpeechRecognitionEventListener* listener,
                              int session_id);

  SpeechRecognizerImplAndroid(const SpeechRecognizerImplAndroid&) = delete;
  SpeechRecognizerImplAndroid& operator=(const SpeechRecognizerImplAndroid&) =
      delete;

  // SpeechRecognizer methods.
  void StartRecognition(const std::string& device_id) override;
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override;
  void AbortRecognition() override;
  void StopAudioCapture() override;
  bool IsActive() const override;
  bool IsCapturingAudio() const override;

 private:
  friend class SpeechRecognizerImplAndroidTest;

  enum State {
    STATE_IDLE = 0,
    STATE_CAPTURING_AUDIO,
    STATE_AWAITING_FINAL_RESULT,
    STATE_ENDED
  };

  void OnAudioStartOnIOThread();
  void OnSoundStartOnIOThread();
  void OnSoundEndOnIOThread();
  void OnAudioEndOnIOThread();
  void OnRecognitionResultsOnIOThread(
      std::vector<media::mojom::WebSpeechRecognitionResultPtr> results);
  void OnRecognitionErrorOnIOThread(int32_t error);
  void OnRecognitionEndOnIOThread();

  ~SpeechRecognizerImplAndroid() override;

  State state_;
  std::unique_ptr<UiPeer, BrowserThread::DeleteOnUIThread> ui_peer_;
  base::WeakPtrFactory<SpeechRecognizerImplAndroid> io_weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_ANDROID_H_
