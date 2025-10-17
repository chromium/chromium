// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_config.h"

namespace {
const uint32_t kDefaultMaxHypotheses = 1;
}

namespace content {

SpeechRecognitionSessionConfig::SpeechRecognitionSessionConfig()
    : filter_profanities(false),
      continuous(false),
      interim_results(false),
      max_hypotheses(kDefaultMaxHypotheses) {
}

SpeechRecognitionSessionConfig::SpeechRecognitionSessionConfig(
    const SpeechRecognitionSessionConfig& other) = default;

SpeechRecognitionSessionConfig::~SpeechRecognitionSessionConfig() {
}

}  // namespace content
