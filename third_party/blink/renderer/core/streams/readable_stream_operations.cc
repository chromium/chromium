// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_operations.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

base::Optional<bool> BooleanOperationWithRethrow(
    ScriptState* script_state,
    ScriptValue value,
    const char* operation,
    ExceptionState& exception_state) {
  DCHECK(!value.IsEmpty());

  if (!value.IsObject())
    return false;

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {value.V8Value()};
  v8::Local<v8::Value> local_value;
  bool result_bool = false;

  if (!V8ScriptRunner::CallExtra(script_state, operation, args)
           .ToLocal(&local_value) ||
      !local_value->BooleanValue(script_state->GetContext()).To(&result_bool)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return base::nullopt;
  }

  DCHECK(!block.HasCaught());
  return result_bool;
}

// Performs |operation| on |value|, catching any exceptions. This is for use in
// DCHECK(). It is unsafe for general use because it ignores errors. Returns
// |fallback_value|, which must be chosen so that the DCHECK() passed if an
// exception was thrown, so that the behaviour matches a release build.
bool BooleanOperationForDCheck(ScriptState* script_state,
                               ScriptValue value,
                               const char* operation,
                               bool fallback_value) {
  v8::Local<v8::Value> args[] = {value.V8Value()};
  v8::Local<v8::Value> result_value;
  bool result_bool = false;
  v8::TryCatch block(script_state->GetIsolate());
  if (V8ScriptRunner::CallExtra(script_state, operation, args)
          .ToLocal(&result_value) &&
      result_value->BooleanValue(script_state->GetContext()).To(&result_bool)) {
    DCHECK(!block.HasCaught());
    return result_bool;
  }
  DCHECK(block.HasCaught() ||
         script_state->GetIsolate()->IsExecutionTerminating());
  return fallback_value;
}

// Performs IsReadableStreamDefaultReader(value), catching exceptions. Should
// only be used in DCHECK(). Returns true on exception.
bool IsDefaultReaderForDCheck(ScriptState* script_state, ScriptValue value) {
  return BooleanOperationForDCheck(script_state, value,
                                   "IsReadableStreamDefaultReader", true);
}

}  // namespace

ScriptValue ReadableStreamOperations::CreateReadableStream(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    ScriptValue strategy) {
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Value> js_underlying_source =
      ToV8(underlying_source, script_state);
  v8::Local<v8::Value> js_strategy = strategy.V8Value();
  v8::Local<v8::Value> args[] = {js_underlying_source, js_strategy};
  return ScriptValue(
      script_state,
      V8ScriptRunner::CallExtra(
          script_state, "createReadableStreamWithExternalController", args));
}

ScriptValue ReadableStreamOperations::CreateCountQueuingStrategy(
    ScriptState* script_state,
    size_t high_water_mark) {
  ScriptState::Scope scope(script_state);

  v8::Local<v8::Value> args[] = {
      v8::Number::New(script_state->GetIsolate(), high_water_mark)};
  return ScriptValue(
      script_state,
      V8ScriptRunner::CallExtra(script_state,
                                "createBuiltInCountQueuingStrategy", args));
}

ScriptValue ReadableStreamOperations::GetReader(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {stream.V8Value()};
  ScriptValue result(
      script_state,
      V8ScriptRunner::CallExtra(script_state,
                                "AcquireReadableStreamDefaultReader", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  DCHECK(!result.IsEmpty() ||
         script_state->GetIsolate()->IsExecutionTerminating());
  return result;
}

base::Optional<bool> ReadableStreamOperations::IsReadableStream(
    ScriptState* script_state,
    ScriptValue value,
    ExceptionState& exception_state) {
  return BooleanOperationWithRethrow(script_state, value, "IsReadableStream",
                                     exception_state);
}

bool ReadableStreamOperations::IsReadableStreamForDCheck(
    ScriptState* script_state,
    ScriptValue value) {
  return BooleanOperationForDCheck(script_state, value, "IsReadableStream",
                                   true);
}

base::Optional<bool> ReadableStreamOperations::IsDisturbed(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamDisturbed", exception_state);
}

bool ReadableStreamOperations::IsDisturbedForDCheck(ScriptState* script_state,
                                                    ScriptValue stream) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationForDCheck(script_state, stream,
                                   "IsReadableStreamDisturbed", false);
}

base::Optional<bool> ReadableStreamOperations::IsLocked(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(script_state, stream,
                                     "IsReadableStreamLocked", exception_state);
}

bool ReadableStreamOperations::IsLockedForDCheck(ScriptState* script_state,
                                                 ScriptValue stream) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationForDCheck(script_state, stream,
                                   "IsReadableStreamLocked", false);
}

base::Optional<bool> ReadableStreamOperations::IsReadable(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamReadable", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsClosed(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(script_state, stream,
                                     "IsReadableStreamClosed", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsErrored(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  return BooleanOperationWithRethrow(
      script_state, stream, "IsReadableStreamErrored", exception_state);
}

base::Optional<bool> ReadableStreamOperations::IsReadableStreamDefaultReader(
    ScriptState* script_state,
    ScriptValue value,
    ExceptionState& exception_state) {
  return BooleanOperationWithRethrow(
      script_state, value, "IsReadableStreamDefaultReader", exception_state);
}

ScriptPromise ReadableStreamOperations::DefaultReaderRead(
    ScriptState* script_state,
    ScriptValue reader) {
  DCHECK(IsDefaultReaderForDCheck(script_state, reader));

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {reader.V8Value()};
  v8::MaybeLocal<v8::Value> maybe_result = V8ScriptRunner::CallExtra(
      script_state, "ReadableStreamDefaultReaderRead", args);
  if (maybe_result.IsEmpty()) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    return ScriptPromise::Reject(script_state, block.Exception());
  }
  return ScriptPromise::Cast(script_state, maybe_result.ToLocalChecked());
}

void ReadableStreamOperations::Tee(ScriptState* script_state,
                                   ScriptValue stream,
                                   ScriptValue* new_stream1,
                                   ScriptValue* new_stream2,
                                   ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  DCHECK(!IsLockedForDCheck(script_state, stream));

  v8::Local<v8::Value> args[] = {stream.V8Value()};

  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> result;
  if (!V8ScriptRunner::CallExtra(script_state, "ReadableStreamTee", args)
           .ToLocal(&result)) {
    DCHECK(block.HasCaught() ||
           script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  DCHECK(result->IsArray());
  v8::Local<v8::Array> branches = result.As<v8::Array>();
  DCHECK_EQ(2u, branches->Length());

  ScriptValue result1(script_state,
                      branches->Get(script_state->GetContext(), 0));
  DCHECK(!result1.IsEmpty());
  DCHECK(IsReadableStreamForDCheck(script_state, result1));

  ScriptValue result2(script_state,
                      branches->Get(script_state->GetContext(), 1));
  DCHECK(!result2.IsEmpty());
  DCHECK(IsReadableStreamForDCheck(script_state, result2));

  *new_stream1 = std::move(result1);
  *new_stream2 = std::move(result2);
}

MessagePort* ReadableStreamOperations::ReadableStreamSerialize(
    ScriptState* script_state,
    ScriptValue stream,
    ExceptionState& exception_state) {
  DCHECK(IsReadableStreamForDCheck(script_state, stream));
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> args[] = {stream.V8Value()};
  ScriptValue result(
      script_state,
      V8ScriptRunner::CallExtra(script_state, "ReadableStreamSerialize", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return nullptr;
  }
  if (result.IsEmpty()) {
    DCHECK(script_state->GetIsolate()->IsExecutionTerminating());
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Serialize failed because execution is terminating");
    return nullptr;
  }

  return V8MessagePort::ToImpl(
      result.V8Value()->ToObject(script_state->GetContext()).ToLocalChecked());
}

ScriptValue ReadableStreamOperations::ReadableStreamDeserialize(
    ScriptState* script_state,
    MessagePort* port,
    ExceptionState& exception_state) {
  DCHECK(port);
  auto* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Value> port_v8 = ToV8(port, context->Global(), isolate);
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> args[] = {port_v8};
  ScriptValue result(script_state,
                     V8ScriptRunner::CallExtra(
                         script_state, "ReadableStreamDeserialize", args));
  if (block.HasCaught()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (result.IsEmpty()) {
    DCHECK(script_state->GetIsolate()->IsExecutionTerminating());
    return ScriptValue();
  }
  DCHECK(IsReadableStreamForDCheck(script_state, result));
  return result;
}

}  // namespace blink
