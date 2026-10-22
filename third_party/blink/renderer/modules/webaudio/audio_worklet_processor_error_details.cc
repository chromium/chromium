// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_error_details.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-message.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace blink {

namespace {

String FormatErrorMessage(AudioWorkletProcessorErrorState error_state,
                          const StringView& name,
                          const StringView& reason) {
  switch (error_state) {
    case AudioWorkletProcessorErrorState::kConstructionError:
      return StrCat({name, ": ", reason});
    case AudioWorkletProcessorErrorState::kProcessError:
      return StrCat({name, " process(): ", reason});
    case AudioWorkletProcessorErrorState::kProcessMethodUndefinedError:
      return StrCat({"TypeError: ", name, " ", reason});
  }
  NOTREACHED();
}

}  // namespace

AudioWorkletProcessorErrorDetails::AudioWorkletProcessorErrorDetails(
    AudioWorkletProcessorErrorState error_state,
    String error_message)
    : error_state(error_state), error_message(std::move(error_message)) {}

// static
AudioWorkletProcessorErrorDetails AudioWorkletProcessorErrorDetails::Create(
    AudioWorkletProcessorErrorState error_state,
    const StringView& name,
    const StringView& reason,
    std::optional<ScriptException> exception) {
  String message = (exception && !exception->message.empty())
                       ? std::move(exception->message)
                       : FormatErrorMessage(error_state, name, reason);
  AudioWorkletProcessorErrorDetails details(error_state, std::move(message));
  if (exception) {
    details.source_url = std::move(exception->source_url);
    details.line_number = exception->line_number;
    details.column_number = exception->column_number;
    details.char_position = exception->char_position;
  }
  return details;
}

// static
std::optional<AudioWorkletProcessorErrorDetails::ScriptException>
AudioWorkletProcessorErrorDetails::ExtractScriptException(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    const v8::TryCatch& try_catch) {
  if (!try_catch.HasCaught()) {
    return std::nullopt;
  }

  v8::Local<v8::Message> message = try_catch.Message();
  if (message.IsEmpty()) {
    return std::nullopt;
  }

  ScriptException exception;
  exception.message = ToCoreStringWithNullCheck(isolate, message->Get());
  exception.line_number = message->GetLineNumber(context).FromMaybe(0);
  exception.column_number = message->GetStartColumn() + 1;
  exception.char_position = message->GetStartPosition();
  v8::Local<v8::Value> script_resource_name = message->GetScriptResourceName();
  if (!script_resource_name.IsEmpty() && script_resource_name->IsString()) {
    exception.source_url =
        ToCoreString(isolate, script_resource_name.As<v8::String>());
  }
  return exception;
}

}  // namespace blink
