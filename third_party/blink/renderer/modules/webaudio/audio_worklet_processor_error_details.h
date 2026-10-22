// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_

#include <optional>

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-forward.h"

namespace blink {

// Categorizes the failure that stopped an AudioWorkletProcessor. Absence of an
// error is represented by `std::nullopt` rather than an enum, so every
// `AudioWorkletProcessorErrorDetails` instance describes an actual error.
enum class AudioWorkletProcessorErrorState {
  // The processor could not be constructed: the options failed to deserialize,
  // the constructor threw, or it did not return an AudioWorkletProcessor.
  kConstructionError,

  // Rendering failed: process() or its property getter threw, or the parameter
  // data could not be prepared.
  kProcessError,

  // The process property is missing or not callable (e.g. a non-function data
  // property or a getter returning a non-function); reported as a TypeError.
  kProcessMethodUndefinedError,
};

// Diagnostic details for an AudioWorkletProcessor failure, posted from the
// worklet or audio thread to the main thread, where AudioWorkletNode turns
// them into a `processorerror` ErrorEvent. Plain copies are safe to post
// across threads: `String` is thread-safe. Construct only via Create().
struct AudioWorkletProcessorErrorDetails {
  // What V8 reports about a caught exception.
  struct ScriptException {
    String message;
    // Empty when the script resource name is not a string (eval, Function).
    String source_url;
    int line_number = 0;
    int column_number = 0;
    int char_position = 0;
  };

  AudioWorkletProcessorErrorState error_state;
  String error_message;
  String source_url;
  int line_number = 0;
  int column_number = 0;
  int char_position = 0;

  // Populates `error_message` from `exception->message` when non-empty, or
  // formats a fallback message for `error_state` from `name` and `reason`.
  // When `exception` is present, copies its `source_url`, `line_number`,
  // `column_number`, and `char_position` into the returned details.
  static AudioWorkletProcessorErrorDetails Create(
      AudioWorkletProcessorErrorState error_state,
      const StringView& name,
      const StringView& reason,
      std::optional<ScriptException> exception = std::nullopt);

  // Returns the exception captured by `try_catch`, or nullopt when nothing was
  // caught or V8 provides no message. Must be called on the isolate's thread
  // while `try_catch` is live.
  static std::optional<ScriptException> ExtractScriptException(
      v8::Isolate* isolate,
      v8::Local<v8::Context> context,
      const v8::TryCatch& try_catch);

 private:
  AudioWorkletProcessorErrorDetails(AudioWorkletProcessorErrorState error_state,
                                    String error_message);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_ERROR_DETAILS_H_
