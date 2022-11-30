// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

class SpeechRecognitionEventListener;

// Handles speech recognition for a session (identified by |session_id|).
class CONTENT_EXPORT SpeechRecognizer
    : public base::RefCountedThreadSafe<SpeechRecognizer> {
 public:
  SpeechRecognizer(SpeechRecognitionEventListener* listener, int session_id)
      : listener_(listener), session_id_(session_id) {
    DCHECK(listener_);
  }

  SpeechRecognizer(const SpeechRecognizer&) = delete;
  SpeechRecognizer& operator=(const SpeechRecognizer&) = delete;

  virtual void StartRecognition(const std::string& device_id) = 0;
  virtual void AbortRecognition() = 0;
  virtual void StopAudioCapture() = 0;
  virtual bool IsActive() const = 0;
  virtual bool IsCapturingAudio() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<SpeechRecognizer>;

  virtual ~SpeechRecognizer() {}
  SpeechRecognitionEventListener* listener() const { return listener_; }
  int session_id() const { return session_id_; }

 private:
  raw_ptr<SpeechRecognitionEventListener> listener_;
  int session_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
