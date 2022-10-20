// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

StreamWrapper::StreamWrapper(ScriptState* script_state)
    : script_state_(script_state) {}

StreamWrapper::~StreamWrapper() = default;

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

ScriptPromise ReadableStreamWrapper::UnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue) {
  readable_stream_wrapper_->CloseStream();
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
  class AbortAlgorithm final : public AbortSignal::Algorithm {
   public:
    explicit AbortAlgorithm(WritableStreamWrapper* writable_stream_wrapper)
        : writable_stream_wrapper_(writable_stream_wrapper) {}

    void Run() override { writable_stream_wrapper_->OnAbortSignal(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(writable_stream_wrapper_);
      Algorithm::Trace(visitor);
    }

   private:
    Member<WritableStreamWrapper> writable_stream_wrapper_;
  };

  abort_handle_ = Controller()->signal()->AddAlgorithm(
      MakeGarbageCollected<AbortAlgorithm>(GetWritableStreamWrapper()));
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise WritableStreamWrapper::UnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController*,
    ExceptionState& exception_state) {
  return writable_stream_wrapper_->Write(chunk, exception_state);
}

ScriptPromise WritableStreamWrapper::UnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState&) {
  writable_stream_wrapper_->CloseStream();
  abort_handle_.Clear();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise WritableStreamWrapper::UnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue,
    ExceptionState& exception_state) {
  return close(script_state, exception_state);
}

void WritableStreamWrapper::UnderlyingSink::Trace(Visitor* visitor) const {
  visitor->Trace(writable_stream_wrapper_);
  visitor->Trace(abort_handle_);
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
