// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/pipe_to_engine.h"

#include "third_party/blink/renderer/bindings/core/v8/promise_all.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/pipe_options.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class PipeToEngine::PipeToAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  PipeToAbortAlgorithm(PipeToEngine* engine, AbortSignal* signal)
      : engine_(engine), signal_(signal) {}
  ~PipeToAbortAlgorithm() override = default;

  void Run() override { engine_->AbortAlgorithm(signal_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    visitor->Trace(signal_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<PipeToEngine> engine_;
  Member<AbortSignal> signal_;
};

class PipeToEngine::PipeToReadRequest final : public ReadRequest {
 public:
  explicit PipeToReadRequest(PipeToEngine* instance) : instance_(instance) {}

  void ChunkSteps(ScriptState* script_state,
                  v8::Local<v8::Value> chunk,
                  ExceptionState&) const override {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state)->GetAgent()->event_loop();
    v8::Global<v8::Value> value(script_state->GetIsolate(), chunk);
    event_loop->EnqueueMicrotask(
        BindOnce(&PipeToEngine::ReadRequestChunkStepsBody,
                 WrapPersistent(instance_.Get()), WrapPersistent(script_state),
                 std::move(value)));
  }

  void CloseSteps(ScriptState* script_state) const override {
    instance_->ReadableClosed();
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    instance_->is_reading_ = false;
    if (instance_->is_shutting_down_) {
      // This function can be called during shutdown when the lock is
      // released. Exit early in that case.
      return;
    }
    instance_->ReadableError(
        instance_->Readable()->GetStoredError(script_state->GetIsolate()));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(instance_);
    ReadRequest::Trace(visitor);
  }

 private:
  Member<PipeToEngine> instance_;
};

// This implementation uses ScriptPromise::Then() extensively. Instead of
// creating  a dozen separate subclasses of ThenCallable<>, we use a single
// resolve and a single reject implementation and pass a method pointer at
// runtime to control the behaviour.
template <typename ReturnType>
class PipeToEngine::WrappedPromiseResolve final
    : public ThenCallable<IDLUndefined,
                          WrappedPromiseResolve<ReturnType>,
                          ReturnType> {
 public:
  using PromiseResolveReaction =
      std::conditional_t<std::is_same_v<IDLUndefined, ReturnType>,
                         void (PipeToEngine::*)(),
                         ScriptPromise<IDLUndefined> (PipeToEngine::*)()>;

  WrappedPromiseResolve(PipeToEngine* instance, PromiseResolveReaction method)
      : instance_(instance), method_(method) {}

  template <typename T = ReturnType>
    requires(std::is_same_v<T, IDLUndefined>)
  void React(ScriptState*) {
    (instance_->*method_)();
  }

  template <typename T = ReturnType>
    requires(std::is_same_v<T, IDLPromise<IDLUndefined>>)
  ScriptPromise<IDLUndefined> React(ScriptState*) {
    return (instance_->*method_)();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(instance_);
    ThenCallable<IDLUndefined, WrappedPromiseResolve<ReturnType>,
                 ReturnType>::Trace(visitor);
  }

 private:
  Member<PipeToEngine> instance_;
  PromiseResolveReaction method_;
};

class PipeToEngine::WrappedPromiseReject final
    : public ThenCallable<IDLAny, WrappedPromiseReject> {
 public:
  using PromiseRejectReaction = void (PipeToEngine::*)(v8::Local<v8::Value>);

  WrappedPromiseReject(PipeToEngine* instance, PromiseRejectReaction method)
      : instance_(instance), method_(method) {}

  void React(ScriptState*, ScriptValue value) {
    (instance_->*method_)(value.V8Value());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(instance_);
    ThenCallable<IDLAny, WrappedPromiseReject>::Trace(visitor);
  }

 private:
  Member<PipeToEngine> instance_;
  PromiseRejectReaction method_;
};

ScriptPromise<IDLUndefined> PipeToEngine::Start(
    ReadableStream* readable,
    WritableStream* destination,
    ExceptionState& exception_state) {
  // 1. Assert: source implements ReadableStream.
  DCHECK(readable);

  // 2. Assert: dest implements WritableStream.
  DCHECK(destination);

  // Not relevant to C++ implementation:
  // 3. Assert: preventClose, preventAbort, and preventCancel are all
  // booleans.

  // Already done by WebIDL bindings:
  // 4. If signal was not given, let signal be undefined.
  // 5. Assert: either signal is undefined, or signal implements AbortSignal.

  // 6. Assert: ! IsReadableStreamLocked(source) is false.
  DCHECK(!ReadableStream::IsLocked(readable));

  // 7. Assert: ! IsWritableStreamLocked(dest) is false.
  DCHECK(!WritableStream::IsLocked(destination));

  // 8. If source.[[controller]] implements ReadableByteStreamController, let
  //    reader be ! AcquireReadableStreamBYOBReader(source) or !
  //    AcquireReadableStreamDefaultReader(source), at the user agent's
  //    discretion.
  // 9. Otherwise, let reader be ! AcquireReadableStreamDefaultReader(source).
  reader_ = ReadableStream::AcquireDefaultReader(script_state_, readable,
                                                 exception_state);
  DCHECK(!exception_state.HadException());

  // 10. Let writer be ! AcquireWritableStreamDefaultWriter(dest).
  writer_ = WritableStream::AcquireDefaultWriter(script_state_, destination,
                                                 exception_state);
  DCHECK(!exception_state.HadException());

  // 11. Set source.[[disturbed]] to true.

  // 12. Let shuttingDown be false.
  DCHECK(!is_shutting_down_);

  // 13. Let promise be a new promise.
  promise_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state_);

  // 14. If signal is not undefined,
  if (auto* signal = pipe_options_->Signal()) {
    //   b. If signal is aborted, perform abortAlgorithm and
    //      return promise.
    if (signal->aborted()) {
      AbortAlgorithm(signal);
      return promise_->Promise();
    }

    //   c. Add abortAlgorithm to signal.
    abort_handle_ = signal->AddAlgorithm(
        MakeGarbageCollected<PipeToAbortAlgorithm>(this, signal));
  }

  // 15. In parallel ...
  // The rest of the algorithm is described in terms of a series of
  // constraints rather than as explicit steps.
  if (CheckInitialState()) {
    // Need to detect closing and error when we are not reading. This
    // corresponds to the following conditions from the standard:
    //     1. Errors must be propagated forward: if source.[[state]] is or
    //        becomes "errored", ...
    // and
    //     3. Closing must be propagated forward: if source.[[state]] is or
    //        becomes "closed", ...
    reader_->closed(script_state_)
        .Then(script_state_,
              MakeGarbageCollected<WrappedPromiseResolve<IDLUndefined>>(
                  this, &PipeToEngine::OnReaderClosed),
              MakeGarbageCollected<WrappedPromiseReject>(
                  this, &PipeToEngine::ReadableError));

    // Need to detect error when we are not writing. This corresponds to this
    // condition from the standard:
    //    2. Errors must be propagated backward: if dest.[[state]] is or
    //       becomes "errored", ...
    // We do not need to detect closure of the writable end of the pipe,
    // because we have it locked and so it can only be closed by us.
    writer_->closed(script_state_)
        .Catch(script_state_, MakeGarbageCollected<WrappedPromiseReject>(
                                  this, &PipeToEngine::WritableError));

    // Start the main read / write loop.
    HandleNextEvent();
  }

  // 16. Return promise.
  return promise_->Promise();
}

bool PipeToEngine::CheckInitialState() {
  auto* isolate = script_state_->GetIsolate();
  const auto state = Readable()->state_;

  // Both streams can be errored or closed. To perform the right action the
  // order of the checks must match the standard: "the following conditions
  // must be applied in order." This method only checks the initial state;
  // detection of state changes elsewhere is done through checking promise
  // reactions.

  // a. Errors must be propagated forward: if source.[[state]] is or
  //    becomes "errored",
  if (state == ReadableStream::kErrored) {
    ReadableError(Readable()->GetStoredError(isolate));
    return false;
  }

  // 2. Errors must be propagated backward: if dest.[[state]] is or becomes
  //    "errored",
  if (Destination()->IsErrored()) {
    WritableError(Destination()->GetStoredError(isolate));
    return false;
  }

  // 3. Closing must be propagated forward: if source.[[state]] is or
  //    becomes "closed", then
  if (state == ReadableStream::kClosed) {
    ReadableClosed();
    return false;
  }

  // 4. Closing must be propagated backward: if !
  //    WritableStreamCloseQueuedOrInFlight(dest) is true or dest.[[state]]
  //    is "closed",
  if (Destination()->IsClosingOrClosed()) {
    WritableStartedClosed();
    return false;
  }

  return true;
}

void PipeToEngine::AbortAlgorithm(AbortSignal* signal) {
  // a. Let abortAlgorithm be the following steps:
  //    i. Let error be signal's abort reason.
  v8::Local<v8::Value> error = signal->reason(script_state_).V8Value();

  // Steps ii. to iv. are implemented in AbortAlgorithmAction.

  //    v. Shutdown with an action consisting of getting a promise to wait for
  //       all of the actions in actions, and with error.
  ShutdownWithAction(&PipeToEngine::AbortAlgorithmAction, error);
}

ScriptPromise<IDLUndefined> PipeToEngine::AbortAlgorithmAction() {
  v8::Local<v8::Value> error = shutdown_error_.Get(script_state_->GetIsolate());

  // ii. Let actions be an empty ordered set.
  HeapVector<MemberScriptPromise<IDLUndefined>> actions;

  // This method runs later than the equivalent steps in the standard. This
  // means that it is safe to do the checks of the state of the destination
  // and source synchronously, simplifying the logic.

  // iii. If preventAbort is false, append the following action to actions:
  //      1. If dest.[[state]] is "writable", return !
  //         WritableStreamAbort(dest, error).
  //      2. Otherwise, return a promise resolved with undefined.
  if (!pipe_options_->PreventAbort() && Destination()->IsWritable()) {
    actions.push_back(
        WritableStream::Abort(script_state_, Destination(), error));
  }

  //  iv. If preventCancel is false, append the following action action to
  //      actions:
  //      1. If source.[[state]] is "readable", return !
  //         ReadableStreamCancel(source, error).
  //      2. Otherwise, return a promise resolved with undefined.
  if (!pipe_options_->PreventCancel() &&
      ReadableStream::IsReadable(Readable())) {
    actions.push_back(ReadableStream::Cancel(script_state_, Readable(), error));
  }

  return PromiseAll<IDLUndefined>::Create(script_state_.Get(), actions);
}

void PipeToEngine::HandleNextEvent() {
  DCHECK(!is_reading_);
  if (is_shutting_down_) {
    return;
  }

  std::optional<double> desired_size = writer_->GetDesiredSizeInternal();
  if (!desired_size.has_value()) {
    // This can happen if abort() is queued but not yet started when
    // pipeTo() is called. In that case [[storedError]] is not set yet, and
    // we need to wait until it is before we can cancel the pipe. Once
    // [[storedError]] has been set, the rejection handler set on the writer
    // closed promise above will detect it, so all we need to do here is
    // nothing.
    return;
  }

  if (desired_size.value() <= 0) {
    // Need to wait for backpressure to go away.
    writer_->ready(script_state_)
        .Then(script_state_,
              MakeGarbageCollected<WrappedPromiseResolve<IDLUndefined>>(
                  this, &PipeToEngine::HandleNextEvent),
              MakeGarbageCollected<WrappedPromiseReject>(
                  this, &PipeToEngine::WritableError));
    return;
  }

  is_reading_ = true;
  auto* read_request = MakeGarbageCollected<PipeToReadRequest>(this);
  ReadableStreamDefaultReader::Read(
      script_state_, reader_, read_request,
      PassThroughException(script_state_->GetIsolate()));
}

void PipeToEngine::ReadRequestChunkStepsBody(ScriptState* script_state,
                                             v8::Global<v8::Value> chunk) {
  // This is needed because this method runs as an enqueued microtask, so the
  // isolate needs a current context.
  ScriptState::Scope scope(script_state);
  is_reading_ = false;
  const auto write = WritableStreamDefaultWriter::Write(
      script_state, writer_, chunk.Get(script_state->GetIsolate()),
      PassThroughException(script_state_->GetIsolate()));
  write.Catch(script_state_, MakeGarbageCollected<WrappedPromiseReject>(
                                 this, &PipeToEngine::WritableError));
  last_write_ = write;
  HandleNextEvent();
}

void PipeToEngine::OnReaderClosed() {
  if (!is_reading_) {
    ReadableClosed();
  }
}

void PipeToEngine::ReadableError(v8::Local<v8::Value> error) {
  // This function can be called during shutdown when the lock is released.
  // Exit early in that case.
  if (is_shutting_down_) {
    return;
  }

  // a. If preventAbort is false, shutdown with an action of !
  //    WritableStreamAbort(dest, source.[[storedError]]) and with
  //    source.[[storedError]].
  DCHECK(error->SameValue(
      Readable()->GetStoredError(script_state_->GetIsolate())));
  if (!pipe_options_->PreventAbort()) {
    ShutdownWithAction(&PipeToEngine::WritableStreamAbortAction, error);
  } else {
    // b. Otherwise, shutdown with source.[[storedError]].
    Shutdown(error);
  }
  return;
}

void PipeToEngine::WritableError(v8::Local<v8::Value> error) {
  // This function can be called during shutdown when the lock is released.
  // Exit early in that case.
  if (is_shutting_down_) {
    return;
  }

  // a. If preventCancel is false, shutdown with an action of !
  //    ReadableStreamCancel(source, dest.[[storedError]]) and with
  //    dest.[[storedError]].
  DCHECK(error->SameValue(
      Destination()->GetStoredError(script_state_->GetIsolate())));
  if (!pipe_options_->PreventCancel()) {
    ShutdownWithAction(&PipeToEngine::ReadableStreamCancelAction, error);
  } else {
    // b. Otherwise, shutdown with dest.[[storedError]].
    Shutdown(error);
  }
  return;
}

void PipeToEngine::ReadableClosed() {
  // a. If preventClose is false, shutdown with an action of !
  //    WritableStreamDefaultWriterCloseWithErrorPropagation(writer).
  if (!pipe_options_->PreventClose()) {
    ShutdownWithAction(
        &PipeToEngine::
            WritableStreamDefaultWriterCloseWithErrorPropagationAction,
        v8::MaybeLocal<v8::Value>());
  } else {
    // b. Otherwise, shutdown.
    Shutdown(v8::MaybeLocal<v8::Value>());
  }
}

void PipeToEngine::WritableStartedClosed() {
  // a. Assert: no chunks have been read or written.
  // This is trivially true because this method is only called from
  // CheckInitialState().

  // b. Let destClosed be a new TypeError.
  const auto dest_closed = v8::Exception::TypeError(
      V8String(script_state_->GetIsolate(), "Destination stream closed"));

  // c. If preventCancel is false, shutdown with an action of !
  //    ReadableStreamCancel(source, destClosed) and with destClosed.
  if (!pipe_options_->PreventCancel()) {
    ShutdownWithAction(&PipeToEngine::ReadableStreamCancelAction, dest_closed);
  } else {
    // d. Otherwise, shutdown with destClosed.
    Shutdown(dest_closed);
  }
}

void PipeToEngine::ShutdownWithAction(
    Action action,
    v8::MaybeLocal<v8::Value> original_error) {
  // a. If shuttingDown is true, abort these substeps.
  if (is_shutting_down_) {
    return;
  }

  // b. Set shuttingDown to true.
  is_shutting_down_ = true;

  // Store the action in case we need to call it asynchronously. This is safe
  // because the |is_shutting_down_| guard flag ensures that we can only reach
  // this assignment once.
  shutdown_action_ = action;

  // Store |original_error| as |shutdown_error_| if it was supplied.
  v8::Local<v8::Value> original_error_local;
  if (original_error.ToLocal(&original_error_local)) {
    shutdown_error_.Reset(script_state_->GetIsolate(), original_error_local);
  }
  ScriptPromise<IDLUndefined> p;

  // c. If dest.[[state]] is "writable" and !
  //    WritableStreamCloseQueuedOrInFlight(dest) is false,
  if (ShouldWriteQueuedChunks()) {
    //  i. If any chunks have been read but not yet written, write them to
    //     dest.
    // ii. Wait until every chunk that has been read has been written
    //     (i.e. the corresponding promises have settled).
    p = WriteQueuedChunks().Then(
        script_state_,
        MakeGarbageCollected<WrappedPromiseResolve<IDLPromise<IDLUndefined>>>(
            this, &PipeToEngine::InvokeShutdownAction));
  } else {
    // d. Let p be the result of performing action.
    p = InvokeShutdownAction();
  }

  // e. Upon fulfillment of p, finalize, passing along originalError if it
  //    was given.
  // f. Upon rejection of p with reason newError, finalize with newError.
  p.Then(script_state_,
         MakeGarbageCollected<WrappedPromiseResolve<IDLUndefined>>(
             this, &PipeToEngine::FinalizeWithOriginalErrorIfSet),
         MakeGarbageCollected<WrappedPromiseReject>(
             this, &PipeToEngine::FinalizeWithNewError));
}

void PipeToEngine::Shutdown(v8::MaybeLocal<v8::Value> error_maybe) {
  // a. If shuttingDown is true, abort these substeps.
  if (is_shutting_down_) {
    return;
  }

  // b. Set shuttingDown to true.
  is_shutting_down_ = true;

  // c. If dest.[[state]] is "writable" and !
  //    WritableStreamCloseQueuedOrInFlight(dest) is false,
  if (ShouldWriteQueuedChunks()) {
    // Need to stash the value of |error_maybe| since we are calling
    // Finalize() asynchronously.
    v8::Local<v8::Value> error;
    if (error_maybe.ToLocal(&error)) {
      shutdown_error_.Reset(script_state_->GetIsolate(), error);
    }

    //  i. If any chunks have been read but not yet written, write them to
    //     dest.
    // ii. Wait until every chunk that has been read has been written
    //     (i.e. the corresponding promises have settled).
    // d. Finalize, passing along error if it was given.
    WriteQueuedChunks().Then(
        script_state_,
        MakeGarbageCollected<WrappedPromiseResolve<IDLUndefined>>(
            this, &PipeToEngine::FinalizeWithOriginalErrorIfSet));
  } else {
    // d. Finalize, passing along error if it was given.
    Finalize(error_maybe);
  }
}

void PipeToEngine::FinalizeWithOriginalErrorIfSet() {
  v8::MaybeLocal<v8::Value> error_maybe;
  if (!shutdown_error_.IsEmpty()) {
    error_maybe = shutdown_error_.Get(script_state_->GetIsolate());
  }
  Finalize(error_maybe);
}

void PipeToEngine::FinalizeWithNewError(v8::Local<v8::Value> new_error) {
  Finalize(new_error);
}

void PipeToEngine::Finalize(v8::MaybeLocal<v8::Value> error_maybe) {
  // a. Perform ! WritableStreamDefaultWriterRelease(writer).
  WritableStreamDefaultWriter::Release(script_state_, writer_);

  // b. If reader implements ReadableStreamBYOBReader, perform !
  // ReadableStreamBYOBReaderRelease(reader).
  if (reader_->IsBYOBReader()) {
    ReadableStreamGenericReader* reader = reader_;
    ReadableStreamBYOBReader* byob_reader =
        To<ReadableStreamBYOBReader>(reader);
    ReadableStreamBYOBReader::Release(script_state_, byob_reader);
  } else {
    // c. Otherwise, perform ! ReadableStreamDefaultReaderRelease(reader).
    DCHECK(reader_->IsDefaultReader());
    ReadableStreamGenericReader* reader = reader_;
    ReadableStreamDefaultReader* default_reader =
        To<ReadableStreamDefaultReader>(reader);
    ReadableStreamDefaultReader::Release(script_state_, default_reader);
  }

  // d. If signal is not undefined, remove abortAlgorithm from signal.
  //
  // An abort algorithm is only added if the signal provided to pipeTo is not
  // undefined *and* not aborted, which means `abort_handle_` can be null if
  // signal is not undefined.
  if (abort_handle_) {
    auto* signal = pipe_options_->Signal();
    DCHECK(signal);
    signal->RemoveAlgorithm(abort_handle_);
  }

  v8::Local<v8::Value> error;
  if (error_maybe.ToLocal(&error)) {
    // e. If error was given, reject promise with error.
    promise_->Reject(error);
  } else {
    // f. Otherwise, resolve promise with undefined.
    promise_->Resolve();
  }
}

bool PipeToEngine::ShouldWriteQueuedChunks() const {
  // "If dest.[[state]] is "writable" and !
  // WritableStreamCloseQueuedOrInFlight(dest) is false"
  return Destination()->IsWritable() &&
         !WritableStream::CloseQueuedOrInFlight(Destination());
}

ScriptPromise<IDLUndefined> PipeToEngine::WriteQueuedChunks() {
  if (!last_write_.IsEmpty()) {
    // "Wait until every chunk that has been read has been written (i.e.
    // the corresponding promises have settled)"
    // This implies that we behave the same whether the promise fulfills or
    // rejects. IgnoreErrors() will convert a rejection into a successful
    // resolution.
    return last_write_.Unwrap().Catch(
        script_state_, MakeGarbageCollected<WrappedPromiseReject>(
                           this, &PipeToEngine::IgnoreErrors));
  }
  return ToResolvedUndefinedPromise(script_state_);
}

ScriptPromise<IDLUndefined> PipeToEngine::WritableStreamAbortAction() {
  return WritableStream::Abort(script_state_, Destination(), ShutdownError());
}

ScriptPromise<IDLUndefined> PipeToEngine::ReadableStreamCancelAction() {
  return ReadableStream::Cancel(script_state_, Readable(), ShutdownError());
}

ScriptPromise<IDLUndefined>
PipeToEngine::WritableStreamDefaultWriterCloseWithErrorPropagationAction() {
  return WritableStreamDefaultWriter::CloseWithErrorPropagation(script_state_,
                                                                writer_);
}

WritableStream* PipeToEngine::Destination() {
  return writer_->OwnerWritableStream();
}

const WritableStream* PipeToEngine::Destination() const {
  return writer_->OwnerWritableStream();
}

ReadableStream* PipeToEngine::Readable() {
  return reader_->owner_readable_stream_;
}

}  // namespace blink
