// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_PREAMBLE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_PREAMBLE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {

// The preamble is the few seconds of audio before the speech recognition
// starts. This is used to contain trigger audio used to start a voice
// query, such as the 'Ok Google' hotword.
struct CONTENT_EXPORT SpeechRecognitionSessionPreamble
    : public base::RefCounted<SpeechRecognitionSessionPreamble> {
  SpeechRecognitionSessionPreamble();

  // Sampling rate (hz) for the preamble data. i.e. 44100, 32000, etc
  int sample_rate;

  // Bytes per sample.
  int sample_depth;

  // Audio data, in little-endian samples.
  std::vector<char> sample_data;

 private:
  friend class base::RefCounted<SpeechRecognitionSessionPreamble>;
  ~SpeechRecognitionSessionPreamble();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_PREAMBLE_H_
