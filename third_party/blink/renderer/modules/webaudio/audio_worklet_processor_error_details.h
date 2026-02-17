// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// A list of state regarding the error in AudioWorkletProcessor object.
enum class AudioWorkletProcessorErrorState : unsigned {
  // The constructor or the process method in the processor has not thrown any
  // exception.
  kNoError = 0,

  // An exception thrown from the construction failure.
  kConstructionError = 1,

  // An exception thrown from the process method.
  kProcessError = 2,

  // An exception thrown if the process method is undefined.
  kProcessMethodUndefinedError = 3,
};

struct AudioWorkletProcessorErrorDetails {
  AudioWorkletProcessorErrorState error_state =
      AudioWorkletProcessorErrorState::kNoError;
  String error_message = "";
  String source_url = "";
  int line_number = 0;
  int column_number = 0;
  int char_position = 0;

  AudioWorkletProcessorErrorDetails() = default;

  AudioWorkletProcessorErrorDetails(AudioWorkletProcessorErrorState error_state,
                                    const String& error_message,
                                    const String& source_url,
                                    int line_number,
                                    int column_number,
                                    int char_position)
      : error_state(error_state),
        error_message(error_message),
        source_url(source_url),
        line_number(line_number),
        column_number(column_number),
        char_position(char_position) {}

  // Copy constructor for thread-safe copying
  AudioWorkletProcessorErrorDetails(
      const AudioWorkletProcessorErrorDetails& other)
      : error_state(other.error_state),
        line_number(other.line_number),
        column_number(other.column_number),
        char_position(other.char_position) {
    DCHECK(!other.error_message.IsNull());
    DCHECK(!other.source_url.IsNull());

    error_message = String(other.error_message.Impl()->IsolatedCopy());
    source_url = String(other.source_url.Impl()->IsolatedCopy());
  }

  // Copy assignment operator for thread-safe copying
  AudioWorkletProcessorErrorDetails& operator=(
      const AudioWorkletProcessorErrorDetails& other) {
    if (this != &other) {
      DCHECK(!other.error_message.IsNull());
      DCHECK(!other.source_url.IsNull());

      error_state = other.error_state;
      error_message = String(other.error_message.Impl()->IsolatedCopy());
      source_url = String(other.source_url.Impl()->IsolatedCopy());
      line_number = other.line_number;
      column_number = other.column_number;
      char_position = other.char_position;
    }
    return *this;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_
