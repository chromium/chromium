// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_preamble.h"

namespace content {

SpeechRecognitionSessionPreamble::SpeechRecognitionSessionPreamble()
    : sample_rate(8000),
      sample_depth(2) {
}

SpeechRecognitionSessionPreamble::~SpeechRecognitionSessionPreamble() {
}

}  // namespace content
