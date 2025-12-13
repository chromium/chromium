// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "v8/include/v8.h"

namespace blink {

void UnderlyingSinkBase::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
}

ScriptPromise<IDLUndefined> UnderlyingSinkStartAlgorithm::Run(
    ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto result = sink_->start(script_state, PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    return ScriptPromise<IDLUndefined>::Reject(script_state,
                                               try_catch.Exception());
  }
  return result;
}

void UnderlyingSinkStartAlgorithm::Trace(Visitor* visitor) const {
  StreamStartAlgorithm::Trace(visitor);
  visitor->Trace(sink_);
}

ScriptPromise<IDLUndefined> UnderlyingSinkWriteAlgorithm::Run(
    ScriptState* script_state,
    base::span<v8::Local<v8::Value>> argv) {
  DCHECK_EQ(argv.size(), 1u);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto result = sink_->write(script_state,
                             ScriptValue(script_state->GetIsolate(), argv[0]),
                             PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    return ScriptPromise<IDLUndefined>::Reject(script_state,
                                               try_catch.Exception());
  }
  return result;
}

void UnderlyingSinkWriteAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(sink_);
}

ScriptPromise<IDLUndefined> UnderlyingSinkCloseAlgorithm::Run(
    ScriptState* script_state,
    base::span<v8::Local<v8::Value>> argv) {
  DCHECK_EQ(argv.size(), 0u);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto result = sink_->close(script_state, PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    return ScriptPromise<IDLUndefined>::Reject(script_state,
                                               try_catch.Exception());
  }
  return result;
}

void UnderlyingSinkCloseAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(sink_);
}

ScriptPromise<IDLUndefined> UnderlyingSinkAbortAlgorithm::Run(
    ScriptState* script_state,
    base::span<v8::Local<v8::Value>> argv) {
  DCHECK_EQ(argv.size(), 1u);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  auto result = sink_->abort(script_state,
                             ScriptValue(script_state->GetIsolate(), argv[0]),
                             PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    return ScriptPromise<IDLUndefined>::Reject(script_state,
                                               try_catch.Exception());
  }
  return result;
}

void UnderlyingSinkAbortAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(sink_);
}

}  // namespace blink
