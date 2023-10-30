// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/body_stream_buffer_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise BodyStreamBufferUnderlyingSource::Pull(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_EQ(script_state, script_state_);
  if (!body_stream_buffer_->consumer_) {
    // This is a speculative workaround for a crash. See
    // https://crbug.com/773525.
    // TODO(yhirano): Remove this branch or have a better comment.
    return ScriptPromise::CastUndefined(script_state);
  }

  if (body_stream_buffer_->stream_needs_more_) {
    return ScriptPromise::CastUndefined(script_state);
  }
  body_stream_buffer_->stream_needs_more_ = true;
  if (!body_stream_buffer_->in_process_data_) {
    body_stream_buffer_->ProcessData(exception_state);
  }
  if (exception_state.HadException()) {
    return ScriptPromise::Reject(script_state, exception_state);
  }
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise BodyStreamBufferUnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState&) {
  DCHECK_EQ(script_state, script_state_);
  Controller()->Close();
  body_stream_buffer_->CancelConsumer();
  return ScriptPromise::CastUndefined(script_state);
}

void BodyStreamBufferUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(body_stream_buffer_);
  UnderlyingSourceBase::Trace(visitor);
}

}  // namespace blink
