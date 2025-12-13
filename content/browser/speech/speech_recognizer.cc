// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_recognizer.h"

#include "content/public/browser/speech_recognition_event_listener.h"

namespace content {

SpeechRecognizer::SpeechRecognizer(SpeechRecognitionEventListener* listener,
                                   int session_id)
    : listener_(listener), session_id_(session_id) {
  DCHECK(listener_);
}

}  // namespace content
