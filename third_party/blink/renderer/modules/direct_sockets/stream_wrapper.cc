// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

class ForwardingUnderlyingSource : public UnderlyingSourceBase {
 public:
  explicit ForwardingUnderlyingSource(
      ReadableStreamDefaultWrapper* readable_stream_wrapper)
      : UnderlyingSourceBase(readable_stream_wrapper->GetScriptState()),
        readable_stream_wrapper_(readable_stream_wrapper) {}

  ScriptPromiseUntyped Start(ScriptState* script_state,
                             ExceptionState&) override {
    readable_stream_wrapper_->SetController(Controller());
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState&) override {
    readable_stream_wrapper_->Pull();
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromiseUntyped Cancel(ScriptState* script_state,
                              ScriptValue reason,
                              ExceptionState&) override {
    readable_stream_wrapper_->CloseStream();
    return ToResolvedUndefinedPromise(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_stream_wrapper_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  const Member<ReadableStreamDefaultWrapper> readable_stream_wrapper_;
};

class ForwardingUnderlyingByteSource : public UnderlyingByteSourceBase {
 public:
  explicit ForwardingUnderlyingByteSource(
      ReadableByteStreamWrapper* readable_stream_wrapper)
      : readable_stream_wrapper_(readable_stream_wrapper) {}

  ScriptPromise<IDLUndefined> Pull(ReadableByteStreamController* controller,
                                   ExceptionState&) override {
    DCHECK_EQ(readable_stream_wrapper_->Controller(), controller);
    readable_stream_wrapper_->Pull();
    return ToResolvedUndefinedPromise(GetScriptState());
  }

  ScriptPromise<IDLUndefined> Cancel() override {
    readable_stream_wrapper_->CloseStream();
    return ToResolvedUndefinedPromise(GetScriptState());
  }

  ScriptPromise<IDLUndefined> Cancel(v8::Local<v8::Value> reason) override {
    return Cancel();
  }

  ScriptState* GetScriptState() override {
    return readable_stream_wrapper_->GetScriptState();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(readable_stream_wrapper_);
    UnderlyingByteSourceBase::Trace(visitor);
  }

 private:
  const Member<ReadableByteStreamWrapper> readable_stream_wrapper_;
};

class ForwardingUnderlyingSink : public UnderlyingSinkBase {
 public:
  explicit ForwardingUnderlyingSink(
      WritableStreamWrapper* writable_stream_wrapper)
      : writable_stream_wrapper_(writable_stream_wrapper) {}

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController* controller,
                                    ExceptionState&) override {
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

    writable_stream_wrapper_->SetController(controller);
    abort_handle_ = Controller()->signal()->AddAlgorithm(
        MakeGarbageCollected<AbortAlgorithm>(writable_stream_wrapper_));
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState*,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController* controller,
                                    ExceptionState& exception_state) override {
    DCHECK_EQ(writable_stream_wrapper_->Controller(), controller);
    return writable_stream_wrapper_->Write(chunk, exception_state);
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    writable_stream_wrapper_->CloseStream();
    abort_handle_.Clear();
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState& exception_state) override {
    return close(script_state, exception_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(writable_stream_wrapper_);
    visitor->Trace(abort_handle_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  const Member<WritableStreamWrapper> writable_stream_wrapper_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
};

}  // namespace

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
  visitor->Trace(readable_);
  StreamWrapper::Trace(visitor);
}

ReadableStreamDefaultWrapper::ReadableStreamDefaultWrapper(
    ScriptState* script_state)
    : ReadableStreamWrapper(script_state) {}

void ReadableStreamDefaultWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(controller_);
  ReadableStreamWrapper::Trace(visitor);
}

// static
UnderlyingSourceBase*
ReadableStreamDefaultWrapper::MakeForwardingUnderlyingSource(
    ReadableStreamDefaultWrapper* readable_stream_wrapper) {
  return MakeGarbageCollected<ForwardingUnderlyingSource>(
      readable_stream_wrapper);
}

ReadableByteStreamWrapper::ReadableByteStreamWrapper(ScriptState* script_state)
    : ReadableStreamWrapper(script_state) {}

void ReadableByteStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(source_);
  visitor->Trace(controller_);
  ReadableStreamWrapper::Trace(visitor);
}

// static
UnderlyingByteSourceBase*
ReadableByteStreamWrapper::MakeForwardingUnderlyingByteSource(
    ReadableByteStreamWrapper* readable_stream_wrapper) {
  return MakeGarbageCollected<ForwardingUnderlyingByteSource>(
      readable_stream_wrapper);
}

WritableStreamWrapper::WritableStreamWrapper(ScriptState* script_state)
    : StreamWrapper(script_state) {}

bool WritableStreamWrapper::Locked() const {
  return WritableStream::IsLocked(writable_.Get());
}

void WritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(sink_);
  visitor->Trace(writable_);
  visitor->Trace(controller_);
  StreamWrapper::Trace(visitor);
}

// static
UnderlyingSinkBase* WritableStreamWrapper::MakeForwardingUnderlyingSink(
    WritableStreamWrapper* writable_stream_wrapper) {
  return MakeGarbageCollected<ForwardingUnderlyingSink>(
      writable_stream_wrapper);
}

}  // namespace blink
