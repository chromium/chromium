// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_abort_signal.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_reader.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/stream_pipe_options.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/transferable_streams.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

ReadableStream::PipeOptions::PipeOptions()
    : prevent_close_(false), prevent_abort_(false), prevent_cancel_(false) {}

ReadableStream::PipeOptions::PipeOptions(const StreamPipeOptions* options)
    : prevent_close_(options->hasPreventClose() ? options->preventClose()
                                                : false),
      prevent_abort_(options->hasPreventAbort() ? options->preventAbort()
                                                : false),
      prevent_cancel_(options->hasPreventCancel() ? options->preventCancel()
                                                  : false),
      signal_(options->hasSignal() ? options->signal() : nullptr) {}

void ReadableStream::PipeOptions::Trace(Visitor* visitor) const {
  visitor->Trace(signal_);
}

bool ReadableStream::PipeOptions::GetBoolean(ScriptState* script_state,
                                             v8::Local<v8::Object> dictionary,
                                             const char* property_name,
                                             ExceptionState& exception_state) {
  auto* isolate = script_state->GetIsolate();
  v8::TryCatch block(isolate);
  v8::Local<v8::Value> property_value;
  if (!dictionary
           ->Get(script_state->GetContext(),
                 V8AtomicString(isolate, property_name))
           .ToLocal(&property_value)) {
    exception_state.RethrowV8Exception(block.Exception());
    return false;
  }
  return property_value->ToBoolean(isolate)->Value();
}

// PipeToEngine implements PipeTo(). All standard steps in this class come from
// https://streams.spec.whatwg.org/#readable-stream-pipe-to
//
// This implementation is simple but suboptimal because it uses V8 promises to
// drive its asynchronous state machine, allocating a lot of temporary V8
// objects as a result.
//
// TODO(ricea): Create internal versions of ReadableStreamDefaultReader::Read()
// and WritableStreamDefaultWriter::Write() to bypass promise creation and so
// reduce the number of allocations on the hot path.
class ReadableStream::PipeToEngine final
    : public GarbageCollected<PipeToEngine> {
 public:
  PipeToEngine(ScriptState* script_state, PipeOptions* pipe_options)
      : script_state_(script_state), pipe_options_(pipe_options) {}

  // This is the main entrypoint for ReadableStreamPipeTo().
  ScriptPromise Start(ReadableStream* readable, WritableStream* destination) {
    // 1. Assert: ! IsReadableStream(source) is true.
    DCHECK(readable);

    // 2. Assert: ! IsWritableStream(dest) is true.
    DCHECK(destination);

    // Not relevant to C++ implementation:
    // 3. Assert: Type(preventClose) is Boolean, Type(preventAbort) is Boolean,
    //    and Type(preventCancel) is Boolean.

    // TODO(ricea): Implement |signal|.
    // 4. Assert: signal is undefined or signal is an instance of the
    //    AbortSignal interface.

    // 5. Assert: ! IsReadableStreamLocked(source) is false.
    DCHECK(!ReadableStream::IsLocked(readable));

    // 6. Assert: ! IsWritableStreamLocked(dest) is false.
    DCHECK(!WritableStream::IsLocked(destination));

    auto* isolate = script_state_->GetIsolate();
    ExceptionState exception_state(isolate, ExceptionState::kUnknownContext, "",
                                   "");

    // 7. If !
    //    IsReadableByteStreamController(source.[[readableStreamController]]) is
    //    true, let reader be either ! AcquireReadableStreamBYOBReader(source)
    //    or ! AcquireReadableStreamDefaultReader(source), at the user agent’s
    //    discretion.
    // 8. Otherwise, let reader be ! AcquireReadableStreamDefaultReader(source).
    reader_ = ReadableStream::AcquireDefaultReader(script_state_, readable,
                                                   false, exception_state);
    DCHECK(!exception_state.HadException());

    // 9. Let writer be ! AcquireWritableStreamDefaultWriter(dest).
    writer_ = WritableStream::AcquireDefaultWriter(script_state_, destination,
                                                   exception_state);
    DCHECK(!exception_state.HadException());

    // 10. Let shuttingDown be false.
    DCHECK(!is_shutting_down_);

    // 11. Let promise be a new promise.
    promise_ = MakeGarbageCollected<StreamPromiseResolver>(script_state_);

    // 12. If signal is not undefined,
    if (auto* signal = pipe_options_->Signal()) {
      //   b. If signal’s aborted flag is set, perform abortAlgorithm and
      //      return promise.
      if (signal->aborted()) {
        AbortAlgorithm();
        return promise_->GetScriptPromise(script_state_);
      }

      //   c. Add abortAlgorithm to signal.
      signal->AddAlgorithm(
          WTF::Bind(&PipeToEngine::AbortAlgorithm, WrapWeakPersistent(this)));
    }

    // 13. In parallel ...
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
      ThenPromise(reader_->ClosedPromise()->V8Promise(isolate),
                  &PipeToEngine::OnReaderClosed, &PipeToEngine::ReadableError);

      // Need to detect error when we are not writing. This corresponds to this
      // condition from the standard:
      //    2. Errors must be propagated backward: if dest.[[state]] is or
      //       becomes "errored", ...
      // We do not need to detect closure of the writable end of the pipe,
      // because we have it locked and so it can only be closed by us.
      ThenPromise(writer_->ClosedPromise()->V8Promise(isolate), nullptr,
                  &PipeToEngine::WritableError);

      // Start the main read / write loop.
      HandleNextEvent(Undefined());
    }

    // 14. Return promise.
    return promise_->GetScriptPromise(script_state_);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(script_state_);
    visitor->Trace(pipe_options_);
    visitor->Trace(reader_);
    visitor->Trace(writer_);
    visitor->Trace(promise_);
    visitor->Trace(last_write_);
    visitor->Trace(shutdown_error_);
  }

 private:
  // The implementation uses method pointers to maximise code reuse.

  // |Action| represents an action that can be passed to the "Shutdown with an
  // action" operation. Each Action is implemented as a method which delegates
  // to some abstract operation, inferring the arguments from the state of
  // |this|.
  using Action = v8::Local<v8::Promise> (PipeToEngine::*)();

  // This implementation uses ThenPromise() 7 times. Instead of creating a dozen
  // separate subclasses of ScriptFunction, we use a single implementation and
  // pass a method pointer at runtime to control the behaviour. Most
  // PromiseReaction methods don't need to return a value, but because some do,
  // the rest have to return undefined so that they can have the same method
  // signature. Similarly, many of the methods ignore the argument that is
  // passed to them.
  using PromiseReaction =
      v8::Local<v8::Value> (PipeToEngine::*)(v8::Local<v8::Value>);

  class WrappedPromiseReaction final : public PromiseHandlerWithValue {
   public:
    WrappedPromiseReaction(ScriptState* script_state,
                           PipeToEngine* instance,
                           PromiseReaction method)
        : PromiseHandlerWithValue(script_state),
          instance_(instance),
          method_(method) {}

    v8::Local<v8::Value> CallWithLocal(v8::Local<v8::Value> value) override {
      return (instance_->*method_)(value);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(instance_);
      ScriptFunction::Trace(visitor);
    }

   private:
    Member<PipeToEngine> instance_;
    PromiseReaction method_;
  };

  // Checks the state of the streams and executes the shutdown handlers if
  // necessary. Returns true if piping can continue.
  bool CheckInitialState() {
    auto* isolate = script_state_->GetIsolate();
    const auto state = Readable()->state_;

    // Both streams can be errored or closed. To perform the right action the
    // order of the checks must match the standard: "the following conditions
    // must be applied in order." This method only checks the initial state;
    // detection of state changes elsewhere is done through checking promise
    // reactions.

    // a. Errors must be propagated forward: if source.[[state]] is or
    //    becomes "errored",
    if (state == kErrored) {
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
    if (state == kClosed) {
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

  void AbortAlgorithm() {
    // a. Let abortAlgorithm be the following steps:
    //    i. Let error be a new "AbortError" DOMException.
    v8::Local<v8::Value> error = V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kAbortError,
        "Pipe aborted.");

    // Steps ii. to iv. are implemented in AbortAlgorithmAction.

    //    v. Shutdown with an action consisting of getting a promise to wait for
    //       all of the actions in actions, and with error.
    ShutdownWithAction(&PipeToEngine::AbortAlgorithmAction, error);
  }

  v8::Local<v8::Promise> AbortAlgorithmAction() {
    v8::Local<v8::Value> error =
        shutdown_error_.NewLocal(script_state_->GetIsolate());

    // ii. Let actions be an empty ordered set.
    HeapVector<ScriptPromise> actions;

    // This method runs later than the equivalent steps in the standard. This
    // means that it is safe to do the checks of the state of the destination
    // and source synchronously, simplifying the logic.

    // iii. If preventAbort is false, append the following action to actions:
    //      1. If dest.[[state]] is "writable", return !
    //         WritableStreamAbort(dest, error).
    //      2. Otherwise, return a promise resolved with undefined.
    if (!pipe_options_->PreventAbort() && Destination()->IsWritable()) {
      actions.push_back(ScriptPromise(
          script_state_,
          WritableStream::Abort(script_state_, Destination(), error)));
    }

    //  iv. If preventCancel is false, append the following action action to
    //      actions:
    //      1. If source.[[state]] is "readable", return !
    //         ReadableStreamCancel(source, error).
    //      2. Otherwise, return a promise resolved with undefined.
    if (!pipe_options_->PreventCancel() && IsReadable(Readable())) {
      actions.push_back(ScriptPromise(
          script_state_,
          ReadableStream::Cancel(script_state_, Readable(), error)));
    }

    return ScriptPromise::All(script_state_, actions)
        .V8Value()
        .As<v8::Promise>();
  }

  // HandleNextEvent() has an unused argument and return value because it is a
  // PromiseReaction. HandleNextEvent() and ReadFulfilled() call each other
  // asynchronously in a loop until the pipe completes.
  v8::Local<v8::Value> HandleNextEvent(v8::Local<v8::Value>) {
    DCHECK(!is_reading_);
    if (is_shutting_down_) {
      return Undefined();
    }

    base::Optional<double> desired_size = writer_->GetDesiredSizeInternal();
    if (!desired_size.has_value()) {
      // This can happen if abort() is queued but not yet started when
      // pipeTo() is called. In that case [[storedError]] is not set yet, and
      // we need to wait until it is before we can cancel the pipe. Once
      // [[storedError]] has been set, the rejection handler set on the writer
      // closed promise above will detect it, so all we need to do here is
      // nothing.
      return Undefined();
    }

    if (desired_size.value() <= 0) {
      // Need to wait for backpressure to go away.
      ThenPromise(
          writer_->ReadyPromise()->V8Promise(script_state_->GetIsolate()),
          &PipeToEngine::HandleNextEvent, &PipeToEngine::WritableError);
      return Undefined();
    }

    is_reading_ = true;
    ThenPromise(ReadableStreamReader::Read(script_state_, reader_)
                    ->V8Promise(script_state_->GetIsolate()),
                &PipeToEngine::ReadFulfilled, &PipeToEngine::ReadRejected);
    return Undefined();
  }

  v8::Local<v8::Value> ReadFulfilled(v8::Local<v8::Value> result) {
    is_reading_ = false;
    DCHECK(result->IsObject());
    auto* isolate = script_state_->GetIsolate();
    v8::Local<v8::Value> value;
    bool done = false;
    bool unpack_succeeded =
        V8UnpackIteratorResult(script_state_, result.As<v8::Object>(), &done)
            .ToLocal(&value);
    DCHECK(unpack_succeeded);
    if (done) {
      ReadableClosed();
      return Undefined();
    }
    const auto write =
        WritableStreamDefaultWriter::Write(script_state_, writer_, value);
    last_write_.Set(isolate, write);
    ThenPromise(write, nullptr, &PipeToEngine::WritableError);
    HandleNextEvent(Undefined());
    return Undefined();
  }

  v8::Local<v8::Value> ReadRejected(v8::Local<v8::Value>) {
    is_reading_ = false;
    ReadableError(Readable()->GetStoredError(script_state_->GetIsolate()));
    return Undefined();
  }

  // If read() is in progress, then wait for it to tell us that the stream is
  // closed so that we write all the data before shutdown.
  v8::Local<v8::Value> OnReaderClosed(v8::Local<v8::Value>) {
    if (!is_reading_) {
      ReadableClosed();
    }
    return Undefined();
  }

  // 1. Errors must be propagated forward: if source.[[state]] is or
  //    becomes "errored", then
  v8::Local<v8::Value> ReadableError(v8::Local<v8::Value> error) {
    // This function can be called during shutdown when the lock is released.
    // Exit early in that case.
    if (is_shutting_down_) {
      return Undefined();
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
    return Undefined();
  }

  // 2. Errors must be propagated backward: if dest.[[state]] is or becomes
  //    "errored", then
  v8::Local<v8::Value> WritableError(v8::Local<v8::Value> error) {
    // This function can be called during shutdown when the lock is released.
    // Exit early in that case.
    if (is_shutting_down_) {
      return Undefined();
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
    return Undefined();
  }

  // 3. Closing must be propagated forward: if source.[[state]] is or
  //    becomes "closed", then
  void ReadableClosed() {
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

  // 4. Closing must be propagated backward: if !
  //    WritableStreamCloseQueuedOrInFlight(dest) is true or dest.[[state]] is
  //    "closed", then
  void WritableStartedClosed() {
    // a. Assert: no chunks have been read or written.
    // This is trivially true because this method is only called from
    // CheckInitialState().

    // b. Let destClosed be a new TypeError.
    const auto dest_closed = v8::Exception::TypeError(
        V8String(script_state_->GetIsolate(), "Destination stream closed"));

    // c. If preventCancel is false, shutdown with an action of !
    //    ReadableStreamCancel(source, destClosed) and with destClosed.
    if (!pipe_options_->PreventCancel()) {
      ShutdownWithAction(&PipeToEngine::ReadableStreamCancelAction,
                         dest_closed);
    } else {
      // d. Otherwise, shutdown with destClosed.
      Shutdown(dest_closed);
    }
  }

  // * Shutdown with an action: if any of the above requirements ask to shutdown
  //   with an action |action|, optionally with an error |originalError|, then:
  void ShutdownWithAction(Action action,
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
      shutdown_error_.Set(script_state_->GetIsolate(), original_error_local);
    }
    v8::Local<v8::Promise> p;

    // c. If dest.[[state]] is "writable" and !
    //    WritableStreamCloseQueuedOrInFlight(dest) is false,
    if (ShouldWriteQueuedChunks()) {
      //  i. If any chunks have been read but not yet written, write them to
      //     dest.
      // ii. Wait until every chunk that has been read has been written
      //     (i.e. the corresponding promises have settled).
      p = ThenPromise(WriteQueuedChunks(), &PipeToEngine::InvokeShutdownAction);
    } else {
      // d. Let p be the result of performing action.
      p = InvokeShutdownAction();
    }

    // e. Upon fulfillment of p, finalize, passing along originalError if it
    //    was given.
    // f. Upon rejection of p with reason newError, finalize with newError.
    ThenPromise(p, &PipeToEngine::FinalizeWithOriginalErrorIfSet,
                &PipeToEngine::FinalizeWithNewError);
  }

  // * Shutdown: if any of the above requirements or steps ask to shutdown,
  //   optionally with an error error, then:
  void Shutdown(v8::MaybeLocal<v8::Value> error_maybe) {
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
        shutdown_error_.Set(script_state_->GetIsolate(), error);
      }

      //  i. If any chunks have been read but not yet written, write them to
      //     dest.
      // ii. Wait until every chunk that has been read has been written
      //     (i.e. the corresponding promises have settled).
      // d. Finalize, passing along error if it was given.
      ThenPromise(WriteQueuedChunks(),
                  &PipeToEngine::FinalizeWithOriginalErrorIfSet);
    } else {
      // d. Finalize, passing along error if it was given.
      Finalize(error_maybe);
    }
  }

  // Calls Finalize(), using the stored shutdown error rather than the value
  // that was passed.
  v8::Local<v8::Value> FinalizeWithOriginalErrorIfSet(v8::Local<v8::Value>) {
    v8::MaybeLocal<v8::Value> error_maybe;
    if (!shutdown_error_.IsEmpty()) {
      error_maybe = shutdown_error_.NewLocal(script_state_->GetIsolate());
    }
    Finalize(error_maybe);
    return Undefined();
  }

  // Calls Finalize(), using the value that was passed as the error.
  v8::Local<v8::Value> FinalizeWithNewError(v8::Local<v8::Value> new_error) {
    Finalize(new_error);
    return Undefined();
  }

  // * Finalize: both forms of shutdown will eventually ask to finalize,
  //   optionally with an error error, which means to perform the following
  //   steps:
  void Finalize(v8::MaybeLocal<v8::Value> error_maybe) {
    // a. Perform ! WritableStreamDefaultWriterRelease(writer).
    WritableStreamDefaultWriter::Release(script_state_, writer_);

    // b. Perform ! ReadableStreamReaderGenericRelease(reader).
    ReadableStreamReader::GenericRelease(script_state_, reader_);

    // TODO(ricea): Implement signal.
    // c. If signal is not undefined, remove abortAlgorithm from signal.

    v8::Local<v8::Value> error;
    if (error_maybe.ToLocal(&error)) {
      // d. If error was given, reject promise with error.
      promise_->Reject(script_state_, error);
    } else {
      // e. Otherwise, resolve promise with undefined.
      promise_->ResolveWithUndefined(script_state_);
    }
  }

  bool ShouldWriteQueuedChunks() const {
    // "If dest.[[state]] is "writable" and !
    // WritableStreamCloseQueuedOrInFlight(dest) is false"
    return Destination()->IsWritable() &&
           !WritableStream::CloseQueuedOrInFlight(Destination());
  }

  v8::Local<v8::Promise> WriteQueuedChunks() {
    if (!last_write_.IsEmpty()) {
      // "Wait until every chunk that has been read has been written (i.e.
      // the corresponding promises have settled)"
      // This implies that we behave the same whether the promise fulfills or
      // rejects. IgnoreErrors() will convert a rejection into a successful
      // resolution.
      return ThenPromise(last_write_.NewLocal(script_state_->GetIsolate()),
                         nullptr, &PipeToEngine::IgnoreErrors);
    }
    return PromiseResolveWithUndefined(script_state_);
  }

  v8::Local<v8::Value> IgnoreErrors(v8::Local<v8::Value>) {
    return Undefined();
  }

  // InvokeShutdownAction(), version for calling directly.
  v8::Local<v8::Promise> InvokeShutdownAction() {
    return (this->*shutdown_action_)();
  }

  // InvokeShutdownAction(), version for use as a PromiseReaction.
  v8::Local<v8::Value> InvokeShutdownAction(v8::Local<v8::Value>) {
    return InvokeShutdownAction();
  }

  v8::Local<v8::Value> ShutdownError() const {
    DCHECK(!shutdown_error_.IsEmpty());
    return shutdown_error_.NewLocal(script_state_->GetIsolate());
  }

  v8::Local<v8::Promise> WritableStreamAbortAction() {
    return WritableStream::Abort(script_state_, Destination(), ShutdownError());
  }

  v8::Local<v8::Promise> ReadableStreamCancelAction() {
    return ReadableStream::Cancel(script_state_, Readable(), ShutdownError());
  }

  v8::Local<v8::Promise>
  WritableStreamDefaultWriterCloseWithErrorPropagationAction() {
    return WritableStreamDefaultWriter::CloseWithErrorPropagation(script_state_,
                                                                  writer_);
  }

  // Reduces the visual noise when we are returning an undefined value.
  v8::Local<v8::Value> Undefined() {
    return v8::Undefined(script_state_->GetIsolate());
  }

  WritableStream* Destination() { return writer_->OwnerWritableStream(); }

  const WritableStream* Destination() const {
    return writer_->OwnerWritableStream();
  }

  ReadableStream* Readable() { return reader_->owner_readable_stream_; }

  // Performs promise.then(on_fulfilled, on_rejected). It behaves like
  // StreamPromiseThen(). Only the types are different.
  v8::Local<v8::Promise> ThenPromise(v8::Local<v8::Promise> promise,
                                     PromiseReaction on_fulfilled,
                                     PromiseReaction on_rejected = nullptr) {
    return StreamThenPromise(
        script_state_->GetContext(), promise,
        on_fulfilled ? MakeGarbageCollected<WrappedPromiseReaction>(
                           script_state_, this, on_fulfilled)
                     : nullptr,
        on_rejected ? MakeGarbageCollected<WrappedPromiseReaction>(
                          script_state_, this, on_rejected)
                    : nullptr);
  }

  Member<ScriptState> script_state_;
  Member<PipeOptions> pipe_options_;
  Member<ReadableStreamReader> reader_;
  Member<WritableStreamDefaultWriter> writer_;
  Member<StreamPromiseResolver> promise_;
  TraceWrapperV8Reference<v8::Promise> last_write_;
  Action shutdown_action_;
  TraceWrapperV8Reference<v8::Value> shutdown_error_;
  bool is_shutting_down_ = false;
  bool is_reading_ = false;

  DISALLOW_COPY_AND_ASSIGN(PipeToEngine);
};

class ReadableStream::TeeEngine final : public GarbageCollected<TeeEngine> {
 public:
  TeeEngine() = default;

  // Create the streams and start copying data.
  void Start(ScriptState*, ReadableStream*, ExceptionState&);

  // Branch1() and Branch2() are null until Start() is called.
  ReadableStream* Branch1() const { return branch_[0]; }
  ReadableStream* Branch2() const { return branch_[1]; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(stream_);
    visitor->Trace(reader_);
    visitor->Trace(reason_[0]);
    visitor->Trace(reason_[1]);
    visitor->Trace(branch_[0]);
    visitor->Trace(branch_[1]);
    visitor->Trace(controller_[0]);
    visitor->Trace(controller_[1]);
    visitor->Trace(cancel_promise_);
  }

 private:
  class PullAlgorithm;
  class CancelAlgorithm;

  Member<ReadableStream> stream_;
  Member<ReadableStreamReader> reader_;
  Member<StreamPromiseResolver> cancel_promise_;
  bool closed_ = false;

  // The standard contains a number of pairs of variables with one for each
  // stream. These are implemented as arrays here. While they are 1-indexed in
  // the standard, they are 0-indexed here; ie. "canceled_[0]" here corresponds
  // to "canceled1" in the standard.
  bool canceled_[2] = {false, false};
  TraceWrapperV8Reference<v8::Value> reason_[2];
  Member<ReadableStream> branch_[2];
  Member<ReadableStreamDefaultController> controller_[2];

  DISALLOW_COPY_AND_ASSIGN(TeeEngine);
};

class ReadableStream::TeeEngine::PullAlgorithm final : public StreamAlgorithm {
 public:
  explicit PullAlgorithm(TeeEngine* engine) : engine_(engine) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int,
                             v8::Local<v8::Value>[]) override {
    // https://streams.spec.whatwg.org/#readable-stream-tee
    // 12. Let pullAlgorithm be the following steps:
    //   a. Return the result of transforming ! ReadableStreamDefaultReaderRead(
    //      reader) with a fulfillment handler which takes the argument result
    //      and performs the following steps:
    return StreamThenPromise(
        script_state->GetContext(),
        ReadableStreamReader::Read(script_state, engine_->reader_)
            ->V8Promise(script_state->GetIsolate()),
        MakeGarbageCollected<ResolveFunction>(script_state, engine_));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  class ResolveFunction final : public PromiseHandler {
   public:
    ResolveFunction(ScriptState* script_state, TeeEngine* engine)
        : PromiseHandler(script_state), engine_(engine) {}

    void CallWithLocal(v8::Local<v8::Value> result) override {
      //    i. If closed is true, return.
      if (engine_->closed_) {
        return;
      }

      //   ii. Assert: Type(result) is Object.
      DCHECK(result->IsObject());

      auto* script_state = GetScriptState();
      auto* isolate = script_state->GetIsolate();

      //  iii. Let done be ! Get(result, "done").
      //   vi. Let value be ! Get(result, "value").
      // The precise order of operations is not important here, because |result|
      // is guaranteed to have own properties of "value" and "done" and so the
      // "Get" operations cannot have side-effects.
      v8::Local<v8::Value> value;
      bool done = false;
      bool unpack_succeeded =
          V8UnpackIteratorResult(script_state, result.As<v8::Object>(), &done)
              .ToLocal(&value);
      CHECK(unpack_succeeded);

      //   vi. Assert: Type(done) is Boolean.
      //    v. If done is true,
      if (done) {
        //    1. If canceled1 is false,
        //        a. Perform ! ReadableStreamDefaultControllerClose(branch1.
        //           [[readableStreamController]]).
        //    2. If canceled2 is false,
        //        b. Perform ! ReadableStreamDefaultControllerClose(branch2.
        //           [[readableStreamController]]).
        for (int branch = 0; branch < 2; ++branch) {
          if (!engine_->canceled_[branch] &&
              ReadableStreamDefaultController::CanCloseOrEnqueue(
                  engine_->controller_[branch])) {
            ReadableStreamDefaultController::Close(
                script_state, engine_->controller_[branch]);
          }
        }

        // TODO(ricea): Implement https://github.com/whatwg/streams/pull/1045 so
        // this step can be numbered correctly.
        // Resolve |cancelPromise| with undefined.
        engine_->cancel_promise_->ResolveWithUndefined(script_state);

        //    3. Set closed to true.
        engine_->closed_ = true;

        //    4. Return.
        return;
      }
      ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                     "", "");
      //  vii. Let value1 and value2 be value.
      // viii. If canceled2 is false and cloneForBranch2 is true, set value2 to
      //       ? StructuredDeserialize(? StructuredSerialize(value2), the
      //       current Realm Record).
      // TODO(ricea): Support cloneForBranch2

      //   ix. If canceled1 is false, perform ?
      //       ReadableStreamDefaultControllerEnqueue(branch1.
      //       [[readableStreamController]], value1).
      //    x. If canceled2 is false, perform ?
      //       ReadableStreamDefaultControllerEnqueue(branch2.
      //       [[readableStreamController]], value2).
      for (int branch = 0; branch < 2; ++branch) {
        if (!engine_->canceled_[branch] &&
            ReadableStreamDefaultController::CanCloseOrEnqueue(
                engine_->controller_[branch])) {
          ReadableStreamDefaultController::Enqueue(script_state,
                                                   engine_->controller_[branch],
                                                   value, exception_state);
          if (exception_state.HadException()) {
            // Instead of returning a rejection, which is inconvenient here,
            // call ControllerError(). The only difference this makes is that
            // it happens synchronously, but that should not be observable.
            ReadableStreamDefaultController::Error(
                script_state, engine_->controller_[branch],
                exception_state.GetException());
            exception_state.ClearException();
            return;
          }
        }
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(engine_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<TeeEngine> engine_;
  };

  Member<TeeEngine> engine_;
};

class ReadableStream::TeeEngine::CancelAlgorithm final
    : public StreamAlgorithm {
 public:
  CancelAlgorithm(TeeEngine* engine, int branch)
      : engine_(engine), branch_(branch) {
    DCHECK(branch == 0 || branch == 1);
  }

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#readable-stream-tee
    // This implements both cancel1Algorithm and cancel2Algorithm as they are
    // identical except for the index they operate on. Standard comments are
    // from cancel1Algorithm.
    // 13. Let cancel1Algorithm be the following steps, taking a reason
    //     argument:
    auto* isolate = script_state->GetIsolate();

    // a. Set canceled1 to true.
    engine_->canceled_[branch_] = true;
    DCHECK_EQ(argc, 1);

    // b. Set reason1 to reason.
    engine_->reason_[branch_].Set(isolate, argv[0]);

    const int other_branch = 1 - branch_;

    // c. If canceled2 is true,
    if (engine_->canceled_[other_branch]) {
      // i. Let compositeReason be ! CreateArrayFromList(« reason1, reason2 »).
      v8::Local<v8::Value> reason[] = {engine_->reason_[0].NewLocal(isolate),
                                       engine_->reason_[1].NewLocal(isolate)};
      v8::Local<v8::Value> composite_reason =
          v8::Array::New(script_state->GetIsolate(), reason, 2);

      // ii. Let cancelResult be ! ReadableStreamCancel(stream,
      //    compositeReason).
      auto cancel_result = ReadableStream::Cancel(
          script_state, engine_->stream_, composite_reason);

      // iii. Resolve cancelPromise with cancelResult.
      engine_->cancel_promise_->Resolve(script_state, cancel_result);
    }
    return engine_->cancel_promise_->V8Promise(isolate);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TeeEngine> engine_;
  const int branch_;
};

void ReadableStream::TeeEngine::Start(ScriptState* script_state,
                                      ReadableStream* stream,
                                      ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-stream-tee
  //  1. Assert: ! IsReadableStream(stream) is true.
  DCHECK(stream);

  // TODO(ricea):  2. Assert: Type(cloneForBranch2) is Boolean.

  stream_ = stream;

  // 3. Let reader be ? AcquireReadableStreamDefaultReader(stream).
  reader_ = ReadableStream::AcquireDefaultReader(script_state, stream, false,
                                                 exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // These steps are performed by the constructor:
  //  4. Let closed be false.
  DCHECK(!closed_);

  //  5. Let canceled1 be false.
  DCHECK(!canceled_[0]);

  //  6. Let canceled2 be false.
  DCHECK(!canceled_[1]);

  //  7. Let reason1 be undefined.
  DCHECK(reason_[0].IsEmpty());

  //  8. Let reason2 be undefined.
  DCHECK(reason_[1].IsEmpty());

  //  9. Let branch1 be undefined.
  DCHECK(!branch_[0]);

  // 10. Let branch2 be undefined.
  DCHECK(!branch_[1]);

  // 11. Let cancelPromise be a new promise.
  cancel_promise_ = MakeGarbageCollected<StreamPromiseResolver>(script_state);

  // 12. Let pullAlgorithm be the following steps:
  // (steps are defined in PullAlgorithm::Run()).
  auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(this);

  // 13. Let cancel1Algorithm be the following steps, taking a reason argument:
  // (see CancelAlgorithm::Run()).
  auto* cancel1_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 0);

  // 14. Let cancel2Algorithm be the following steps, taking a reason argument:
  // (both algorithms share a single implementation).
  auto* cancel2_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 1);

  // 15. Let startAlgorithm be an algorithm that returns undefined.
  auto* start_algorithm = CreateTrivialStartAlgorithm();

  auto* size_algorithm = CreateDefaultSizeAlgorithm();

  // 16. Set branch1 to ! CreateReadableStream(startAlgorithm, pullAlgorithm,
  //   cancel1Algorithm).
  branch_[0] = ReadableStream::Create(script_state, start_algorithm,
                                      pull_algorithm, cancel1_algorithm, 1.0,
                                      size_algorithm, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 17. Set branch2 to ! CreateReadableStream(startAlgorithm, pullAlgorithm,
  //   cancel2Algorithm).
  branch_[1] = ReadableStream::Create(script_state, start_algorithm,
                                      pull_algorithm, cancel2_algorithm, 1.0,
                                      size_algorithm, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  for (int branch = 0; branch < 2; ++branch) {
    controller_[branch] = branch_[branch]->readable_stream_controller_;
  }

  class RejectFunction final : public PromiseHandler {
   public:
    RejectFunction(ScriptState* script_state, TeeEngine* engine)
        : PromiseHandler(script_state), engine_(engine) {}

    void CallWithLocal(v8::Local<v8::Value> r) override {
      // 18. Upon rejection of reader.[[closedPromise]] with reason r,
      //   a. Perform ! ReadableStreamDefaultControllerError(branch1.
      //      [[readableStreamController]], r).
      ReadableStreamDefaultController::Error(GetScriptState(),
                                             engine_->controller_[0], r);

      //   b. Perform ! ReadableStreamDefaultControllerError(branch2.
      //      [[readableStreamController]], r).
      ReadableStreamDefaultController::Error(GetScriptState(),
                                             engine_->controller_[1], r);

      // TODO(ricea): Implement https://github.com/whatwg/streams/pull/1045 so
      // this step can be numbered correctly.
      // Resolve |cancelPromise| with undefined.
      engine_->cancel_promise_->ResolveWithUndefined(GetScriptState());
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(engine_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<TeeEngine> engine_;
  };

  // 18. Upon rejection of reader.[[closedPromise]] with reason r,
  StreamThenPromise(
      script_state->GetContext(),
      reader_->closed_promise_->V8Promise(script_state->GetIsolate()), nullptr,
      MakeGarbageCollected<RejectFunction>(script_state, this));

  // Step "19. Return « branch1, branch2 »."
  // is performed by the caller.
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return Create(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ExceptionState& exception_state) {
  return Create(script_state, underlying_source,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       ScriptValue underlying_source,
                                       ScriptValue strategy,
                                       ExceptionState& exception_state) {
  auto* stream = MakeGarbageCollected<ReadableStream>();
  stream->InitInternal(script_state, underlying_source, strategy, false,
                       exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return stream;
}

ReadableStream* ReadableStream::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark) {
  auto* isolate = script_state->GetIsolate();

  // It's safer to use a workalike rather than a real CountQueuingStrategy
  // object. We use the default "size" function as it is implemented in C++ and
  // so much faster than calling into JavaScript. Since the create object has a
  // null prototype, there is no danger of us finding some other "size" function
  // via the prototype chain.
  v8::Local<v8::Name> high_water_mark_string =
      V8AtomicString(isolate, "highWaterMark");
  v8::Local<v8::Value> high_water_mark_value =
      v8::Number::New(isolate, high_water_mark);

  auto strategy_object =
      v8::Object::New(isolate, v8::Null(isolate), &high_water_mark_string,
                      &high_water_mark_value, 1);

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 "ReadableStream");

  v8::Local<v8::Value> underlying_source_v8 =
      ToV8(underlying_source, script_state);

  auto* stream = MakeGarbageCollected<ReadableStream>();
  stream->InitInternal(
      script_state,
      ScriptValue(script_state->GetIsolate(), underlying_source_v8),
      ScriptValue(script_state->GetIsolate(), strategy_object), true,
      exception_state);

  if (exception_state.HadException()) {
    exception_state.ClearException();
    DLOG(WARNING)
        << "Ignoring an exception in CreateWithCountQueuingStrategy().";
  }

  return stream;
}

ReadableStream* ReadableStream::Create(ScriptState* script_state,
                                       StreamStartAlgorithm* start_algorithm,
                                       StreamAlgorithm* pull_algorithm,
                                       StreamAlgorithm* cancel_algorithm,
                                       double high_water_mark,
                                       StrategySizeAlgorithm* size_algorithm,
                                       ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#create-readable-stream
  // All arguments are compulsory in this implementation, so the first two steps
  // are skipped:
  // 1. If highWaterMark was not passed, set it to 1.
  // 2. If sizeAlgorithm was not passed, set it to an algorithm that returns 1.

  // 3. Assert: ! IsNonNegativeNumber(highWaterMark) is true.
  DCHECK_GE(high_water_mark, 0);

  // 4. Let stream be ObjectCreate(the original value of ReadableStream's
  //    prototype property).
  auto* stream = MakeGarbageCollected<ReadableStream>();

  // 5. Perform ! InitializeReadableStream(stream).
  Initialize(stream);

  // 6. Let controller be ObjectCreate(the original value of
  //    ReadableStreamDefaultController's prototype property).
  auto* controller = MakeGarbageCollected<ReadableStreamDefaultController>();

  // 7. Perform ? SetUpReadableStreamDefaultController(stream, controller,
  //    startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //    sizeAlgorithm).
  ReadableStreamDefaultController::SetUp(
      script_state, stream, controller, start_algorithm, pull_algorithm,
      cancel_algorithm, high_water_mark, size_algorithm, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 8. Return stream.
  return stream;
}

ReadableStream::ReadableStream() = default;

ReadableStream::~ReadableStream() = default;

bool ReadableStream::locked() const {
  // https://streams.spec.whatwg.org/#rs-locked
  // 2. Return ! IsReadableStreamLocked(this).
  return IsLocked(this);
}

ScriptPromise ReadableStream::cancel(ScriptState* script_state,
                                     ExceptionState& exception_state) {
  return cancel(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ScriptPromise ReadableStream::cancel(ScriptState* script_state,
                                     ScriptValue reason,
                                     ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-cancel
  // 2. If ! IsReadableStreamLocked(this) is true, return a promise rejected
  //    with a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot cancel a locked stream");
    return ScriptPromise();
  }

  // 3. Return ! ReadableStreamCancel(this, reason).
  v8::Local<v8::Promise> result = Cancel(script_state, this, reason.V8Value());
  return ScriptPromise(script_state, result);
}

ReadableStreamDefaultReader* ReadableStream::getReader(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // 2. If mode is undefined, return ? AcquireReadableStreamDefaultReader(this,
  //    true).
  return AcquireDefaultReader(script_state, this, true, exception_state);
}

ReadableStreamDefaultReader* ReadableStream::getReader(
    ScriptState* script_state,
    ScriptValue options,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // Since we don't support byob readers, the only thing
  // GetReaderValidateOptions() needs to do is throw an exception if
  // |options.mode| is invalid.
  GetReaderValidateOptions(script_state, options, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return getReader(script_state, exception_state);
}

ScriptValue ReadableStream::pipeThrough(ScriptState* script_state,
                                        ScriptValue transform_stream,
                                        ExceptionState& exception_state) {
  return pipeThrough(script_state, transform_stream,
                     StreamPipeOptions::Create(), exception_state);
}

// https://streams.spec.whatwg.org/#rs-pipe-through
ScriptValue ReadableStream::pipeThrough(ScriptState* script_state,
                                        ScriptValue transform_stream,
                                        const StreamPipeOptions* options,
                                        ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-pipe-through
  // The first part of this function implements the unpacking of the {readable,
  // writable} argument to the method.
  v8::Local<v8::Value> pair_value = transform_stream.V8Value();
  v8::Local<v8::Context> context = script_state->GetContext();

  constexpr char kWritableIsNotWritableStream[] =
      "parameter 1's 'writable' property is not a WritableStream.";
  constexpr char kReadableIsNotReadableStream[] =
      "parameter 1's 'readable' property is not a ReadableStream.";
  constexpr char kWritableIsLocked[] = "parameter 1's 'writable' is locked.";

  v8::Local<v8::Object> pair;
  if (!pair_value->ToObject(context).ToLocal(&pair)) {
    exception_state.ThrowTypeError(kWritableIsNotWritableStream);
    return ScriptValue();
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> writable, readable;
  {
    v8::TryCatch block(isolate);
    if (!pair->Get(context, V8String(isolate, "writable")).ToLocal(&writable)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
    DCHECK(!block.HasCaught());

    if (!pair->Get(context, V8String(isolate, "readable")).ToLocal(&readable)) {
      exception_state.RethrowV8Exception(block.Exception());
      return ScriptValue();
    }
    DCHECK(!block.HasCaught());
  }

  // 2. If ! IsWritableStream(_writable_) is *false*, throw a *TypeError*
  //    exception.
  WritableStream* writable_stream =
      V8WritableStream::ToImplWithTypeCheck(isolate, writable);
  if (!writable_stream) {
    exception_state.ThrowTypeError(kWritableIsNotWritableStream);
    return ScriptValue();
  }

  // 3. If ! IsReadableStream(_readable_) is *false*, throw a *TypeError*
  //    exception.
  if (!V8ReadableStream::HasInstance(readable, isolate)) {
    exception_state.ThrowTypeError(kReadableIsNotReadableStream);
    return ScriptValue();
  }

  // 4. If signal is not undefined, and signal is not an instance of the
  //    AbortSignal interface, throw a TypeError exception.
  auto* pipe_options = MakeGarbageCollected<PipeOptions>(options);

  // 5. If ! IsReadableStreamLocked(*this*) is *true*, throw a *TypeError*
  //    exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return ScriptValue();
  }

  // 6. If ! IsWritableStreamLocked(_writable_) is *true*, throw a *TypeError*
  //    exception.
  if (WritableStream::IsLocked(writable_stream)) {
    exception_state.ThrowTypeError(kWritableIsLocked);
    return ScriptValue();
  }

  // 7. Let _promise_ be ! ReadableStreamPipeTo(*this*, _writable_,
  //    _preventClose_, _preventAbort_, _preventCancel_,
  //   _signal_).

  ScriptPromise promise =
      PipeTo(script_state, this, writable_stream, pipe_options);

  // 8. Set _promise_.[[PromiseIsHandled]] to *true*.
  promise.MarkAsHandled();

  // 9. Return _readable_.
  return ScriptValue(script_state->GetIsolate(), readable);
}

ScriptPromise ReadableStream::pipeTo(ScriptState* script_state,
                                     WritableStream* destination,
                                     ExceptionState& exception_state) {
  return pipeTo(script_state, destination, StreamPipeOptions::Create(),
                exception_state);
}

ScriptPromise ReadableStream::pipeTo(ScriptState* script_state,
                                     WritableStream* destination,
                                     const StreamPipeOptions* options,
                                     ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-pipe-to
  // 1. If ! IsReadableStreamLocked(this) is true, return a promise rejected
  //    with a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return ScriptPromise();
  }

  // 2. If ! IsWritableStreamLocked(destination) is true, return a promise
  //    rejected with a TypeError exception.
  if (WritableStream::IsLocked(destination)) {
    exception_state.ThrowTypeError("Cannot pipe to a locked stream");
    return ScriptPromise();
  }

  // 3. Let signal be options["signal"] if it exists, or undefined otherwise.
  auto* pipe_options = MakeGarbageCollected<PipeOptions>(options);

  // 4. Return ! ReadableStreamPipeTo(this, destination,
  //    options["preventClose"], options["preventAbort"],
  //    options["preventCancel"], signal).
  return PipeTo(script_state, this, destination, pipe_options);
}

ScriptValue ReadableStream::tee(ScriptState* script_state,
                                ExceptionState& exception_state) {
  return CallTeeAndReturnBranchArray(script_state, this, exception_state);
}

// Unlike in the standard, this is defined as a separate method from the
// constructor. This prevents problems when garbage collection happens
// re-entrantly during construction.
void ReadableStream::InitInternal(ScriptState* script_state,
                                  ScriptValue raw_underlying_source,
                                  ScriptValue raw_strategy,
                                  bool created_by_ua,
                                  ExceptionState& exception_state) {
  if (!created_by_ua) {
    // TODO(ricea): Move this to IDL once blink::ReadableStreamOperations is
    // no longer using the public constructor.
    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kReadableStreamConstructor);
  }

  // https://streams.spec.whatwg.org/#rs-constructor
  //  1. Perform ! InitializeReadableStream(this).
  Initialize(this);

  // The next part of this constructor corresponds to the object conversions
  // that are implicit in the definition in the standard.
  DCHECK(!raw_underlying_source.IsEmpty());
  DCHECK(!raw_strategy.IsEmpty());

  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  v8::Local<v8::Object> underlying_source;
  ScriptValueToObject(script_state, raw_underlying_source, &underlying_source,
                      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 2. Let size be ? GetV(strategy, "size").
  // 3. Let highWaterMark be ? GetV(strategy, "highWaterMark").
  StrategyUnpacker strategy_unpacker(script_state, raw_strategy,
                                     exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 4. Let type be ? GetV(underlyingSource, "type").
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Value> type;
  if (!underlying_source->Get(context, V8AtomicString(isolate, "type"))
           .ToLocal(&type)) {
    exception_state.RethrowV8Exception(try_catch.Exception());
    return;
  }

  if (!type->IsUndefined()) {
    // 5. Let typeString be ? ToString(type).
    v8::Local<v8::String> type_string;
    if (!type->ToString(context).ToLocal(&type_string)) {
      exception_state.RethrowV8Exception(try_catch.Exception());
      return;
    }

    // 6. If typeString is "bytes",
    if (type_string == V8AtomicString(isolate, "bytes")) {
      // TODO(ricea): Implement bytes type.
      exception_state.ThrowRangeError("bytes type is not yet implemented");
      return;
    }

    // 8. Otherwise, throw a RangeError exception.
    exception_state.ThrowRangeError("Invalid type is specified");
    return;
  }

  // 7. Otherwise, if type is undefined,
  //   a. Let sizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(size).
  auto* size_algorithm =
      strategy_unpacker.MakeSizeAlgorithm(script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  DCHECK(size_algorithm);

  //   b. If highWaterMark is undefined, let highWaterMark be 1.
  //   c. Set highWaterMark to ? ValidateAndNormalizeHighWaterMark(
  //      highWaterMark).
  double high_water_mark =
      strategy_unpacker.GetHighWaterMark(script_state, 1, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 4. Perform ? SetUpReadableStreamDefaultControllerFromUnderlyingSource
  //  (this, underlyingSource, highWaterMark, sizeAlgorithm).
  ReadableStreamDefaultController::SetUpFromUnderlyingSource(
      script_state, this, underlying_source, high_water_mark, size_algorithm,
      exception_state);
}

//
// Readable stream abstract operations
//
ReadableStreamReader* ReadableStream::AcquireDefaultReader(
    ScriptState* script_state,
    ReadableStream* stream,
    bool for_author_code,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#acquire-readable-stream-reader
  // for_author_code is compulsory in this implementation
  // 1. If forAuthorCode was not passed, set it to false.

  // 2. Let reader be ? Construct(ReadableStreamDefaultReader, « stream »).
  auto* reader = MakeGarbageCollected<ReadableStreamReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 3. Set reader.[[forAuthorCode]] to forAuthorCode.
  reader->for_author_code_ = for_author_code;

  // 4. Return reader.
  return reader;
}

void ReadableStream::Initialize(ReadableStream* stream) {
  // Fields are initialised by the constructor, so we only check that they were
  // initialised correctly.
  // https://streams.spec.whatwg.org/#initialize-readable-stream
  // 1. Set stream.[[state]] to "readable".
  CHECK_EQ(stream->state_, kReadable);
  // 2. Set stream.[[reader]] and stream.[[storedError]] to undefined.
  DCHECK(!stream->reader_);
  DCHECK(stream->stored_error_.IsEmpty());
  // 3. Set stream.[[disturbed]] to false.
  DCHECK(!stream->is_disturbed_);
}

// TODO(domenic): cloneForBranch2 argument from spec not supported yet
void ReadableStream::Tee(ScriptState* script_state,
                         ReadableStream** branch1,
                         ReadableStream** branch2,
                         ExceptionState& exception_state) {
  auto* engine = MakeGarbageCollected<TeeEngine>();
  engine->Start(script_state, this, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // Instead of returning a List like ReadableStreamTee in the standard, the
  // branches are returned via output parameters.
  *branch1 = engine->Branch1();
  *branch2 = engine->Branch2();
}

void ReadableStream::LockAndDisturb(ScriptState* script_state) {
  if (reader_) {
    return;
  }

  ReadableStreamReader* reader = GetReaderNotForAuthorCode(script_state);
  DCHECK(reader);

  is_disturbed_ = true;
}

void ReadableStream::Serialize(ScriptState* script_state,
                               MessagePort* port,
                               ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-transfer
  // 1. If ! IsReadableStreamLocked(value) is true, throw a "DataCloneError"
  //    DOMException.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot transfer a locked stream");
    return;
  }

  // Done by SerializedScriptValue::TransferReadableStream():
  // 2. Let port1 be a new MessagePort in the current Realm.
  // 3. Let port2 be a new MessagePort in the current Realm.
  // 4. Entangle port1 and port2.

  // 5. Let writable be a new WritableStream in the current Realm.
  // 6. Perform ! SetUpCrossRealmTransformWritable(writable, port1).
  auto* writable =
      CreateCrossRealmTransformWritable(script_state, port, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 7. Let promise be ! ReadableStreamPipeTo(value, writable, false, false,
  //    false).
  auto promise =
      PipeTo(script_state, this, writable, MakeGarbageCollected<PipeOptions>());

  // 8. Set promise.[[PromiseIsHandled]] to true.
  promise.MarkAsHandled();

  // This step is done in a roundabout way by the caller:
  // 9. Set dataHolder.[[port]] to ! StructuredSerializeWithTransfer(port2,
  //    « port2 »).
}

ReadableStream* ReadableStream::Deserialize(ScriptState* script_state,
                                            MessagePort* port,
                                            ExceptionState& exception_state) {
  // We need to execute JavaScript to call "Then" on v8::Promises. We will not
  // run author code.
  v8::Isolate::AllowJavascriptExecutionScope allow_js(
      script_state->GetIsolate());

  // https://streams.spec.whatwg.org/#rs-transfer
  // These steps are done by V8ScriptValueDeserializer::ReadDOMObject().
  // 1. Let deserializedRecord be !
  //    StructuredDeserializeWithTransfer(dataHolder.[[port]], the current
  //    Realm).
  // 2. Let port be deserializedRecord.[[Deserialized]].

  // 3. Perform ! SetUpCrossRealmTransformReadable(value, port).
  // In the standard |value| contains an uninitialized ReadableStream. In the
  // implementation, we create the stream here.
  auto* readable =
      CreateCrossRealmTransformReadable(script_state, port, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return readable;
}

ReadableStreamDefaultReader* ReadableStream::GetReaderNotForAuthorCode(
    ScriptState* script_state) {
  DCHECK(!IsLocked(this));

  // Since the stream is not locked, AcquireDefaultReader cannot fail.
  NonThrowableExceptionState exception_state(__FILE__, __LINE__);
  return AcquireDefaultReader(script_state, this, false, exception_state);
}

ScriptPromise ReadableStream::PipeTo(ScriptState* script_state,
                                     ReadableStream* readable,
                                     WritableStream* destination,
                                     PipeOptions* pipe_options) {
  auto* engine = MakeGarbageCollected<PipeToEngine>(script_state, pipe_options);
  return engine->Start(readable, destination);
}

v8::Local<v8::Value> ReadableStream::GetStoredError(
    v8::Isolate* isolate) const {
  return stored_error_.NewLocal(isolate);
}

void ReadableStream::Trace(Visitor* visitor) const {
  visitor->Trace(readable_stream_controller_);
  visitor->Trace(reader_);
  visitor->Trace(stored_error_);
  ScriptWrappable::Trace(visitor);
}

//
// Abstract Operations Used By Controllers
//

StreamPromiseResolver* ReadableStream::AddReadRequest(ScriptState* script_state,
                                                      ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-add-read-request
  // 1. Assert: ! IsReadableStreamDefaultReader(stream.[[reader]]) is true.
  DCHECK(stream->reader_);

  // 2. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);

  // 3. Let promise be a new promise.
  auto* promise = MakeGarbageCollected<StreamPromiseResolver>(script_state);

  // This implementation stores promises directly in |read_requests_| rather
  // than wrapping them in a Record.
  // 4. Let readRequest be Record {[[promise]]: promise}.
  // 5. Append readRequest as the last element of stream.[[reader]].
  //  [[readRequests]].
  stream->reader_->read_requests_.push_back(promise);

  // 6. Return promise.
  return promise;
}

v8::Local<v8::Promise> ReadableStream::Cancel(ScriptState* script_state,
                                              ReadableStream* stream,
                                              v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#readable-stream-cancel
  // 1. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  // 2. If stream.[[state]] is "closed", return a promise resolved with
  //    undefined.
  const auto state = stream->state_;
  if (state == kClosed) {
    return PromiseResolveWithUndefined(script_state);
  }

  // 3. If stream.[[state]] is "errored", return a promise rejected with stream.
  //    [[storedError]].
  if (state == kErrored) {
    return PromiseReject(script_state,
                         stream->GetStoredError(script_state->GetIsolate()));
  }

  // 4. Perform ! ReadableStreamClose(stream).
  Close(script_state, stream);

  // 5. Let sourceCancelPromise be ! stream.[[readableStreamController]].
  //    [[CancelSteps]](reason).
  v8::Local<v8::Promise> source_cancel_promise =
      stream->readable_stream_controller_->CancelSteps(script_state, reason);

  class ReturnUndefinedFunction final : public PromiseHandler {
   public:
    explicit ReturnUndefinedFunction(ScriptState* script_state)
        : PromiseHandler(script_state) {}

    // The method does nothing; the default value of undefined is returned to
    // JavaScript.
    void CallWithLocal(v8::Local<v8::Value>) override {}
  };

  // 6. Return the result of transforming sourceCancelPromise with a
  //    fulfillment handler that returns undefined.
  return StreamThenPromise(
      script_state->GetContext(), source_cancel_promise,
      MakeGarbageCollected<ReturnUndefinedFunction>(script_state));
}

void ReadableStream::Close(ScriptState* script_state, ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-close
  // 1. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);

  // 2. Set stream.[[state]] to "closed".
  stream->state_ = kClosed;

  // 3. Let reader be stream.[[reader]].
  ReadableStreamReader* reader = stream->reader_;

  // 4. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // TODO(ricea): Support BYOB readers.
  // 5. If ! IsReadableStreamDefaultReader(reader) is true,
  //   a. Repeat for each readRequest that is an element of reader.
  //      [[readRequests]],
  HeapDeque<Member<StreamPromiseResolver>> requests;
  requests.Swap(reader->read_requests_);
  for (StreamPromiseResolver* promise : requests) {
    //   i. Resolve readRequest.[[promise]] with !
    //      ReadableStreamCreateReadResult(undefined, true, reader.
    //      [[forAuthorCode]]).
    promise->Resolve(script_state,
                     CreateReadResult(script_state,
                                      v8::Undefined(script_state->GetIsolate()),
                                      true, reader->for_author_code_));
  }

  //   b. Set reader.[[readRequests]] to an empty List.
  //      This is not required since we've already called Swap().

  // 6. Resolve reader.[[closedPromise]] with undefined.
  reader->closed_promise_->ResolveWithUndefined(script_state);
}

v8::Local<v8::Value> ReadableStream::CreateReadResult(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    bool done,
    bool for_author_code) {
  // https://streams.spec.whatwg.org/#readable-stream-create-read-result
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  auto value_string = V8AtomicString(isolate, "value");
  auto done_string = V8AtomicString(isolate, "done");
  auto done_value = v8::Boolean::New(isolate, done);
  // 1. Let prototype be null.
  // 2. If forAuthorCode is true, set prototype to %ObjectPrototype%.
  // This implementation doesn't use a |prototype| variable, instead using
  // different code paths depending on the value of |for_author_code|.
  if (for_author_code) {
    // 4. Let obj be ObjectCreate(prototype).
    auto obj = v8::Object::New(isolate);

    // 5. Perform CreateDataProperty(obj, "value", value).
    obj->CreateDataProperty(context, value_string, value).Check();

    // 6. Perform CreateDataProperty(obj, "done", done).
    obj->CreateDataProperty(context, done_string, done_value).Check();

    // 7. Return obj.
    return obj;
  }

  // When |for_author_code| is false, we can perform all the steps in a single
  // call to V8.

  // 4. Let obj be ObjectCreate(prototype).
  // 5. Perform CreateDataProperty(obj, "value", value).
  // 6. Perform CreateDataProperty(obj, "done", done).
  // 7. Return obj.
  // TODO(ricea): Is it possible to use this optimised API in both cases?
  v8::Local<v8::Name> names[2] = {value_string, done_string};
  v8::Local<v8::Value> values[2] = {value, done_value};

  static_assert(base::size(names) == base::size(values),
                "names and values arrays must be the same size");
  return v8::Object::New(isolate, v8::Null(isolate), names, values,
                         base::size(names));
}

void ReadableStream::Error(ScriptState* script_state,
                           ReadableStream* stream,
                           v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-stream-error
  // 2. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);
  auto* isolate = script_state->GetIsolate();

  // 3. Set stream.[[state]] to "errored".
  stream->state_ = kErrored;

  // 4. Set stream.[[storedError]] to e.
  stream->stored_error_.Set(isolate, e);

  // 5. Let reader be stream.[[reader]].
  ReadableStreamReader* reader = stream->reader_;

  // 6. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // 7. If ! IsReadableStreamDefaultReader(reader) is true,
  // TODO(ricea): Support BYOB readers.
  //   a. Repeat for each readRequest that is an element of reader.
  //      [[readRequests]],
  for (StreamPromiseResolver* promise : reader->read_requests_) {
    //   i. Reject readRequest.[[promise]] with e.
    promise->Reject(script_state, e);
  }

  //   b. Set reader.[[readRequests]] to a new empty List.
  reader->read_requests_.clear();

  // 9. Reject reader.[[closedPromise]] with e.
  reader->closed_promise_->Reject(script_state, e);

  // 10. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed_promise_->MarkAsHandled(isolate);
}

void ReadableStream::FulfillReadRequest(ScriptState* script_state,
                                        ReadableStream* stream,
                                        v8::Local<v8::Value> chunk,
                                        bool done) {
  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
  // 1. Let reader be stream.[[reader]].
  ReadableStreamReader* reader = stream->reader_;

  // 2. Let readRequest be the first element of reader.[[readRequests]].
  StreamPromiseResolver* read_request = reader->read_requests_.front();

  // 3. Remove readIntoRequest from reader.[[readIntoRequests]], shifting all
  //    other elements downward (so that the second becomes the first, and so
  //    on).
  reader->read_requests_.pop_front();

  // 4. Resolve readIntoRequest.[[promise]] with !
  //    ReadableStreamCreateReadResult(chunk, done, reader.[[forAuthorCode]]).
  read_request->Resolve(
      script_state, ReadableStream::CreateReadResult(script_state, chunk, done,
                                                     reader->for_author_code_));
}

int ReadableStream::GetNumReadRequests(const ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
  // 1. Return the number of elements in stream.[[reader]].[[readRequests]].
  return stream->reader_->read_requests_.size();
}

//
// TODO(ricea): Functions for transferable streams.
//

void ReadableStream::GetReaderValidateOptions(ScriptState* script_state,
                                              ScriptValue options,
                                              ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // The unpacking of |options| is indicated as part of the signature of the
  // function in the standard.
  v8::TryCatch block(script_state->GetIsolate());
  v8::Local<v8::Value> mode;
  v8::Local<v8::String> mode_string;
  v8::Local<v8::Context> context = script_state->GetContext();
  if (options.V8Value()->IsUndefined()) {
    mode = v8::Undefined(script_state->GetIsolate());
  } else {
    v8::Local<v8::Object> v8_options;
    if (!options.V8Value()->ToObject(context).ToLocal(&v8_options)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
    if (!v8_options->Get(context, V8String(script_state->GetIsolate(), "mode"))
             .ToLocal(&mode)) {
      exception_state.RethrowV8Exception(block.Exception());
      return;
    }
  }

  // 3. Set mode to ? ToString(mode).
  if (!mode->ToString(context).ToLocal(&mode_string)) {
    exception_state.RethrowV8Exception(block.Exception());
    return;
  }

  // 4. If mode is "byob", return ? AcquireReadableStreamBYOBReader(this, true).
  if (ToCoreString(mode_string) == "byob") {
    // TODO(ricea): Support BYOB readers.
    exception_state.ThrowTypeError("invalid mode");
    return;
  }

  if (!mode->IsUndefined()) {
    // 5. Throw a RangeError exception.
    exception_state.ThrowRangeError("invalid mode");
    return;
  }
}

ScriptValue ReadableStream::CallTeeAndReturnBranchArray(
    ScriptState* script_state,
    ReadableStream* readable,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-tee
  v8::Isolate* isolate = script_state->GetIsolate();
  ReadableStream* branch1 = nullptr;
  ReadableStream* branch2 = nullptr;

  // 2. Let branches be ? ReadableStreamTee(this, false).
  readable->Tee(script_state, &branch1, &branch2, exception_state);

  if (!branch1 || !branch2)
    return ScriptValue();

  DCHECK(!exception_state.HadException());

  // 3. Return ! CreateArrayFromList(branches).
  v8::TryCatch block(isolate);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Array> array = v8::Array::New(isolate, 2);
  v8::Local<v8::Object> global = context->Global();

  v8::Local<v8::Value> v8_branch1 = ToV8(branch1, global, isolate);
  if (v8_branch1.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  v8::Local<v8::Value> v8_branch2 = ToV8(branch2, global, isolate);
  if (v8_branch1.IsEmpty()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (array->Set(context, V8String(isolate, "0"), v8_branch1).IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  if (array->Set(context, V8String(isolate, "1"), v8_branch2).IsNothing()) {
    exception_state.RethrowV8Exception(block.Exception());
    return ScriptValue();
  }
  return ScriptValue(script_state->GetIsolate(), array);
}

}  // namespace blink
