// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_SPEECH_RECOGNITION_EVENT_LISTENER_H_
#define CONTENT_TEST_MOCK_SPEECH_RECOGNITION_EVENT_LISTENER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockSpeechRecognitionEventListener
    : public SpeechRecognitionEventListener {
 public:
  MockSpeechRecognitionEventListener();
  MockSpeechRecognitionEventListener(
      const MockSpeechRecognitionEventListener&) = delete;
  MockSpeechRecognitionEventListener& operator=(
      const MockSpeechRecognitionEventListener&) = delete;
  ~MockSpeechRecognitionEventListener() override;

  MOCK_METHOD(void, OnRecognitionStart, (int));
  MOCK_METHOD(void, OnAudioStart, (int));
  MOCK_METHOD(void, OnSoundStart, (int));
  MOCK_METHOD(void, OnSoundEnd, (int));
  MOCK_METHOD(void, OnAudioEnd, (int));
  MOCK_METHOD(
      void,
      OnRecognitionResults,
      (int, const std::vector<media::mojom::WebSpeechRecognitionResultPtr>&));
  MOCK_METHOD(void,
              OnRecognitionError,
              (int, const media::mojom::SpeechRecognitionError&));
  MOCK_METHOD(void, OnAudioLevelsChange, (int, float, float));
  MOCK_METHOD(void, OnRecognitionEnd, (int));

  base::WeakPtr<MockSpeechRecognitionEventListener> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSpeechRecognitionEventListener> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_SPEECH_RECOGNITION_EVENT_LISTENER_H_
