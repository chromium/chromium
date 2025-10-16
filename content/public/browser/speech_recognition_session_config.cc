// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_config.h"

namespace content {

SpeechRecognitionSessionConfig::SpeechRecognitionSessionConfig() = default;

SpeechRecognitionSessionConfig::SpeechRecognitionSessionConfig(
    const SpeechRecognitionSessionConfig& other) = default;

SpeechRecognitionSessionConfig::~SpeechRecognitionSessionConfig() {
}

}  // namespace content
