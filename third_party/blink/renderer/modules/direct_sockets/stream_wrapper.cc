// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

StreamWrapper::StreamWrapper(ScriptState* script_state)
    : script_state_(script_state) {}

StreamWrapper::~StreamWrapper() = default;

ScriptValue StreamWrapper::CreateException(ScriptState* script_state,
                                           DOMExceptionCode code,
                                           const String& message) {
  return ScriptValue(script_state->GetIsolate(),
                     V8ThrowDOMException::CreateOrEmpty(
                         script_state->GetIsolate(), code, message));
}

void StreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
}

ReadableStreamWrapper::ReadableStreamWrapper(ScriptState* script_state)
    : StreamWrapper(script_state) {}

bool ReadableStreamWrapper::Locked() const {
  return ReadableStream::IsLocked(readable_);
}

void ReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(readable_);
  StreamWrapper::Trace(visitor);
}

void ReadableStreamWrapper::InitSourceAndReadable(UnderlyingSource* source,
                                                  size_t high_water_mark) {
  source_ = source;
  ScriptState::Scope scope(GetScriptState());
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      GetScriptState(), source, high_water_mark);
}

ReadableStreamDefaultControllerWithScriptScope*
ReadableStreamWrapper::Controller() const {
  return source_->Controller();
}

WritableStreamWrapper::WritableStreamWrapper(ScriptState* script_state)
    : StreamWrapper(script_state) {}

bool WritableStreamWrapper::Locked() const {
  return WritableStream::IsLocked(writable_);
}

void WritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(sink_);
  visitor->Trace(writable_);
  StreamWrapper::Trace(visitor);
}

void WritableStreamWrapper::InitSinkAndWritable(UnderlyingSink* sink,
                                                size_t high_water_mark) {
  sink_ = sink;
  ScriptState::Scope scope(GetScriptState());
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      GetScriptState(), sink, high_water_mark);
}

WritableStreamDefaultController* WritableStreamWrapper::Controller() const {
  return sink_->Controller();
}

ReadableStreamWrapper::UnderlyingSource::UnderlyingSource(
    ScriptState* script_state,
    ReadableStreamWrapper* readable_stream_wrapper)
    : UnderlyingSourceBase(script_state),
      readable_stream_wrapper_(readable_stream_wrapper) {}

ScriptPromise ReadableStreamWrapper::UnderlyingSource::Start(
    ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise ReadableStreamWrapper::UnderlyingSource::pull(
    ScriptState* script_state) {
  readable_stream_wrapper_->Pull();
  return ScriptPromise::CastUndefined(script_state);
}

void ReadableStreamWrapper::UnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(readable_stream_wrapper_);
  UnderlyingSourceBase::Trace(visitor);
}

WritableStreamWrapper::UnderlyingSink::UnderlyingSink(
    WritableStreamWrapper* writable_stream_wrapper)
    : writable_stream_wrapper_(writable_stream_wrapper) {}

ScriptPromise WritableStreamWrapper::UnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController*,
    ExceptionState&) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise WritableStreamWrapper::UnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController*,
    ExceptionState& exception_state) {
  return writable_stream_wrapper_->Write(chunk, exception_state);
}

ScriptPromise WritableStreamWrapper::UnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  return close(script_state, exception_state);
}

void WritableStreamWrapper::UnderlyingSink::Trace(Visitor* visitor) const {
  visitor->Trace(writable_stream_wrapper_);
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
