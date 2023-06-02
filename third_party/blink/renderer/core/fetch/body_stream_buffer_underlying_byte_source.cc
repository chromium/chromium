// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer_underlying_byte_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise BodyStreamBufferUnderlyingByteSource::Pull(
    ReadableByteStreamController* controller,
    ExceptionState&) {
  if (!body_stream_buffer_->consumer_) {
    // This is a speculative workaround for a crash. See
    // https://crbug.com/773525.
    // TODO(yhirano): Remove this branch or have a better comment.
    return ScriptPromise::CastUndefined(GetScriptState());
  }

  if (body_stream_buffer_->stream_needs_more_) {
    return ScriptPromise::CastUndefined(GetScriptState());
  }
  body_stream_buffer_->stream_needs_more_ = true;
  if (!body_stream_buffer_->in_process_data_) {
    body_stream_buffer_->ProcessData();
  }
  return ScriptPromise::CastUndefined(GetScriptState());
}

ScriptPromise BodyStreamBufferUnderlyingByteSource::Cancel(
    ExceptionState& exception_state) {
  return Cancel(v8::Undefined(GetScriptState()->GetIsolate()), exception_state);
}

ScriptPromise BodyStreamBufferUnderlyingByteSource::Cancel(
    v8::Local<v8::Value> reason,
    ExceptionState& exception_state) {
  ReadableStreamController* controller =
      body_stream_buffer_->Stream()->GetController();
  DCHECK(controller->IsByteStreamController());
  ReadableByteStreamController* byte_controller =
      To<ReadableByteStreamController>(controller);
  byte_controller->Close(GetScriptState(), byte_controller, exception_state);
  DCHECK(!exception_state.HadException());
  body_stream_buffer_->CancelConsumer();
  return ScriptPromise::CastUndefined(GetScriptState());
}

ScriptState* BodyStreamBufferUnderlyingByteSource::GetScriptState() {
  return script_state_;
}

void BodyStreamBufferUnderlyingByteSource::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(body_stream_buffer_);
  UnderlyingByteSourceBase::Trace(visitor);
}

}  // namespace blink
