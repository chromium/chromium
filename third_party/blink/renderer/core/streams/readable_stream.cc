// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_get_reader_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_writable_pair.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_stream_pipe_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_readablestreambyobreader_readablestreamdefaultreader.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/byte_stream_tee_engine.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/pipe_options.h"
#include "third_party/blink/renderer/core/streams/pipe_to_engine.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/read_into_request.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/tee_engine.h"
#include "third_party/blink/renderer/core/streams/transferable_streams.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Implements a pull algorithm that delegates to an UnderlyingByteSourceBase.
// This is used when creating a ReadableByteStream from C++.
class ReadableStream::PullAlgorithm final : public StreamAlgorithm {
 public:
  explicit PullAlgorithm(UnderlyingByteSourceBase* underlying_byte_source)
      : underlying_byte_source_(underlying_byte_source) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);
    DCHECK(controller_);
    ScriptPromiseUntyped promise;
    if (script_state->ContextIsValid()) {
      v8::TryCatch try_catch(script_state->GetIsolate());
      {
        // This is needed because the realm of the underlying source can be
        // different from the realm of the readable stream.
        ScriptState::Scope scope(underlying_byte_source_->GetScriptState());
        promise = underlying_byte_source_->Pull(
            controller_, PassThroughException(script_state->GetIsolate()));
      }
      if (try_catch.HasCaught()) {
        return PromiseReject(script_state, try_catch.Exception());
      }
    } else {
      return PromiseReject(script_state,
                           V8ThrowException::CreateTypeError(
                               script_state->GetIsolate(), "invalid realm"));
    }

    return promise.V8Promise();
  }

  // SetController() must be called before Run() is.
  void SetController(ReadableByteStreamController* controller) {
    controller_ = controller;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(underlying_byte_source_);
    visitor->Trace(controller_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<UnderlyingByteSourceBase> underlying_byte_source_;
  Member<ReadableByteStreamController> controller_;
};

// Implements a cancel algorithm that delegates to an UnderlyingByteSourceBase.
class ReadableStream::CancelAlgorithm final : public StreamAlgorithm {
 public:
  explicit CancelAlgorithm(UnderlyingByteSourceBase* underlying_byte_source)
      : underlying_byte_source_(underlying_byte_source) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    ScriptPromiseUntyped promise;
    if (script_state->ContextIsValid()) {
      v8::TryCatch try_catch(script_state->GetIsolate());
      {
        // This is needed because the realm of the underlying source can be
        // different from the realm of the readable stream.
        ScriptState::Scope scope(underlying_byte_source_->GetScriptState());
        promise = underlying_byte_source_->Cancel(argv[0]);
      }
      if (try_catch.HasCaught()) {
        return PromiseReject(script_state, try_catch.Exception());
      }
    } else {
      return PromiseReject(script_state,
                           V8ThrowException::CreateTypeError(
                               script_state->GetIsolate(), "invalid realm"));
    }

    return promise.V8Promise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(underlying_byte_source_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<UnderlyingByteSourceBase> underlying_byte_source_;
};

class ReadableStream::IterationSource final
    : public ReadableStream::IterationSourceBase {
 public:
  IterationSource(ScriptState* script_state,
                  Kind kind,
                  ReadableStreamDefaultReader* reader,
                  bool prevent_cancel)
      : ReadableStream::IterationSourceBase(script_state, kind),
        reader_(reader),
        prevent_cancel_(prevent_cancel) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(reader_);
    ReadableStream::IterationSourceBase::Trace(visitor);
  }

 protected:
  void GetNextIterationResult() override;
  void AsyncIteratorReturn(ScriptValue arg) override;

 private:
  friend class IterationReadRequest;

  void TryResolvePromise();

  Member<ReadableStreamDefaultReader> reader_;
  bool prevent_cancel_;
};

class ReadableStream::IterationReadRequest final : public ReadRequest {
 public:
  explicit IterationReadRequest(IterationSource* iteration_source)
      : iteration_source_(iteration_source) {}

  void ChunkSteps(ScriptState* script_state,
                  v8::Local<v8::Value> chunk,
                  ExceptionState& exception_state) const override {
    // 1. Resolve promise with chunk.
    iteration_source_->TakePendingPromiseResolver()->Resolve(
        iteration_source_->MakeIterationResult(
            ScriptValue(script_state->GetIsolate(), chunk)));
  }

  void CloseSteps(ScriptState* script_state) const override {
    // 1. Perform ! ReadableStreamDefaultReaderRelease(reader).
    ReadableStreamDefaultReader::Release(script_state,
                                         iteration_source_->reader_);
    // 2. Resolve promise with end of iteration.
    iteration_source_->TakePendingPromiseResolver()->Resolve(
        iteration_source_->MakeEndOfIteration());
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    // 1. Perform ! ReadableStreamDefaultReaderRelease(reader).
    ReadableStreamDefaultReader::Release(script_state,
                                         iteration_source_->reader_);
    // 2. Reject promise with e.
    iteration_source_->TakePendingPromiseResolver()->Reject(e);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(iteration_source_);
    ReadRequest::Trace(visitor);
  }

 private:
  Member<IterationSource> iteration_source_;
};

void ReadableStream::IterationSource::GetNextIterationResult() {
  DCHECK(HasPendingPromise());

  // https://streams.spec.whatwg.org/#ref-for-dfn-get-the-next-iteration-result
  // 2. Assert: reader.[[stream]] is not undefined.
  DCHECK(reader_->owner_readable_stream_);

  // 4. Let readRequest be a new read request.
  auto* read_request = MakeGarbageCollected<IterationReadRequest>(this);

  // 5. Perform ! ReadableStreamDefaultReaderRead(this, readRequest).
  ScriptState* script_state = GetScriptState();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kUnknown, "", "");
  ReadableStreamDefaultReader::Read(script_state, reader_, read_request,
                                    exception_state);
}

void ReadableStream::IterationSource::AsyncIteratorReturn(ScriptValue arg) {
  DCHECK(HasPendingPromise());

  // https://streams.spec.whatwg.org/#ref-for-asynchronous-iterator-return
  // 2. Assert: reader.[[stream]] is not undefined.
  DCHECK(reader_->owner_readable_stream_);
  // 3. Assert: reader.[[readRequests]] is empty, as the async iterator
  //    machinery guarantees that any previous calls to next() have settled
  //    before this is called.
  DCHECK(reader_->read_requests_.empty());

  ScriptState* script_state = GetScriptState();
  // 4. If iterator's prevent cancel is false:
  if (!prevent_cancel_) {
    // 4.1. Let result be ! ReadableStreamReaderGenericCancel(reader, arg).
    auto result = ReadableStreamGenericReader::GenericCancel(
        script_state, reader_, arg.V8Value());
    // 4.2. Perform ! ReadableStreamDefaultReaderRelease(reader).
    ReadableStreamDefaultReader::Release(script_state, reader_);
    // 4.3. Return result.
    TakePendingPromiseResolver()->Resolve(result.V8Value());
    return;
  }

  // 5. Perform ! ReadableStreamDefaultReaderRelease(reader).
  ReadableStreamDefaultReader::Release(script_state, reader_);

  // 6. Return a promise resolved with undefined.
  TakePendingPromiseResolver()->Resolve(
      v8::Undefined(script_state->GetIsolate()));
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
  return CreateWithCountQueueingStrategy(script_state, underlying_source,
                                         high_water_mark,
                                         AllowPerChunkTransferring(false),
                                         /*optimizer=*/nullptr);
}

ReadableStream* ReadableStream::CreateWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark,
    AllowPerChunkTransferring allow_per_chunk_transferring,
    std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer) {
  auto* isolate = script_state->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  auto* stream = MakeGarbageCollected<ReadableStream>();
  stream->InitWithCountQueueingStrategy(
      script_state, underlying_source, high_water_mark,
      allow_per_chunk_transferring, std::move(optimizer), IGNORE_EXCEPTION);
  return stream;
}

void ReadableStream::InitWithCountQueueingStrategy(
    ScriptState* script_state,
    UnderlyingSourceBase* underlying_source,
    size_t high_water_mark,
    AllowPerChunkTransferring allow_per_chunk_transferring,
    std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer,
    ExceptionState& exception_state) {
  Initialize(this);
  auto* controller =
      MakeGarbageCollected<ReadableStreamDefaultController>(script_state);

  ReadableStreamDefaultController::SetUp(
      script_state, this, controller,
      MakeGarbageCollected<UnderlyingStartAlgorithm>(underlying_source,
                                                     controller),
      MakeGarbageCollected<UnderlyingPullAlgorithm>(underlying_source),
      MakeGarbageCollected<UnderlyingCancelAlgorithm>(underlying_source),
      high_water_mark, CreateDefaultSizeAlgorithm(), exception_state);

  allow_per_chunk_transferring_ = allow_per_chunk_transferring;
  transferring_optimizer_ = std::move(optimizer);
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

  // 4. Let stream be a new ReadableStream.
  auto* stream = MakeGarbageCollected<ReadableStream>();

  // 5. Perform ! InitializeReadableStream(stream).
  Initialize(stream);

  // 6. Let controller be a new ReadableStreamDefaultController.
  auto* controller =
      MakeGarbageCollected<ReadableStreamDefaultController>(script_state);

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

ReadableStream* ReadableStream::CreateByteStream(
    ScriptState* script_state,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* pull_algorithm,
    StreamAlgorithm* cancel_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
  // 1. Let stream be a new ReadableStream.
  auto* stream = MakeGarbageCollected<ReadableStream>();

  // 2. Perform ! InitializeReadableStream(stream).
  Initialize(stream);

  // 3. Let controller be a new ReadableByteStreamController.
  auto* controller = MakeGarbageCollected<ReadableByteStreamController>();

  // 4. Perform ? SetUpReadableByteStreamController(stream, controller,
  //    startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, undefined).
  ReadableByteStreamController::SetUp(script_state, stream, controller,
                                      start_algorithm, pull_algorithm,
                                      cancel_algorithm, 0, 0, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 5. Return stream.
  return stream;
}

// static
ReadableStream* ReadableStream::CreateByteStream(
    ScriptState* script_state,
    UnderlyingByteSourceBase* underlying_byte_source) {
  // https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
  // 1. Let stream be a new ReadableStream.
  auto* stream = MakeGarbageCollected<ReadableStream>();

  // Construction of the byte stream cannot fail because the trivial start
  // algorithm will not throw.
  NonThrowableExceptionState exception_state;
  InitByteStream(script_state, stream, underlying_byte_source, exception_state);

  // 5. Return stream.
  return stream;
}

void ReadableStream::InitByteStream(
    ScriptState* script_state,
    ReadableStream* stream,
    UnderlyingByteSourceBase* underlying_byte_source,
    ExceptionState& exception_state) {
  auto* pull_algorithm =
      MakeGarbageCollected<PullAlgorithm>(underlying_byte_source);
  auto* cancel_algorithm =
      MakeGarbageCollected<CancelAlgorithm>(underlying_byte_source);

  // Step 3 of
  // https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
  // 3. Let controller be a new ReadableByteStreamController.
  auto* controller = MakeGarbageCollected<ReadableByteStreamController>();

  InitByteStream(script_state, stream, controller,
                 CreateTrivialStartAlgorithm(), pull_algorithm,
                 cancel_algorithm, exception_state);
  DCHECK(!exception_state.HadException());

  pull_algorithm->SetController(controller);
}

void ReadableStream::InitByteStream(ScriptState* script_state,
                                    ReadableStream* stream,
                                    ReadableByteStreamController* controller,
                                    StreamStartAlgorithm* start_algorithm,
                                    StreamAlgorithm* pull_algorithm,
                                    StreamAlgorithm* cancel_algorithm,
                                    ExceptionState& exception_state) {
  // Step 2 and 4 of
  // https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
  // 2. Perform ! InitializeReadableStream(stream).
  Initialize(stream);

  // 4. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, 0, undefined).
  ReadableByteStreamController::SetUp(script_state, stream, controller,
                                      start_algorithm, pull_algorithm,
                                      cancel_algorithm, 0, 0, exception_state);
  if (exception_state.HadException()) {
    return;
  }
}

ReadableStream::ReadableStream() = default;

ReadableStream::~ReadableStream() = default;

bool ReadableStream::locked() const {
  // https://streams.spec.whatwg.org/#rs-locked
  // 2. Return ! IsReadableStreamLocked(this).
  return IsLocked(this);
}

ScriptPromise<IDLUndefined> ReadableStream::cancel(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return cancel(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ScriptPromise<IDLUndefined> ReadableStream::cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-cancel
  // 2. If ! IsReadableStreamLocked(this) is true, return a promise rejected
  //    with a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot cancel a locked stream");
    return EmptyPromise();
  }

  // 3. Return ! ReadableStreamCancel(this, reason).
  return Cancel(script_state, this, reason.V8Value());
}

V8ReadableStreamReader* ReadableStream::getReader(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  // 1. If options["mode"] does not exist, return ?
  // AcquireReadableStreamDefaultReader(this).
  ReadableStreamDefaultReader* reader =
      AcquireDefaultReader(script_state, this, exception_state);
  if (!reader)
    return nullptr;
  return MakeGarbageCollected<V8ReadableStreamReader>(reader);
}

V8ReadableStreamReader* ReadableStream::getReader(
    ScriptState* script_state,
    const ReadableStreamGetReaderOptions* options,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-get-reader
  if (options->hasMode()) {
    DCHECK_EQ(options->mode(), "byob");

    UseCounter::Count(ExecutionContext::From(script_state),
                      WebFeature::kReadableStreamBYOBReader);

    ReadableStreamBYOBReader* reader =
        AcquireBYOBReader(script_state, this, exception_state);
    if (!reader)
      return nullptr;
    return MakeGarbageCollected<V8ReadableStreamReader>(reader);
  }

  return getReader(script_state, exception_state);
}

ReadableStreamDefaultReader* ReadableStream::GetDefaultReaderForTesting(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* result = getReader(script_state, exception_state);
  if (!result)
    return nullptr;
  return result->GetAsReadableStreamDefaultReader();
}

ReadableStreamBYOBReader* ReadableStream::GetBYOBReaderForTesting(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* options = ReadableStreamGetReaderOptions::Create();
  options->setMode("byob");
  auto* result = getReader(script_state, options, exception_state);
  if (!result)
    return nullptr;
  return result->GetAsReadableStreamBYOBReader();
}

ReadableStream* ReadableStream::pipeThrough(ScriptState* script_state,
                                            ReadableWritablePair* transform,
                                            ExceptionState& exception_state) {
  return pipeThrough(script_state, transform, StreamPipeOptions::Create(),
                     exception_state);
}

// https://streams.spec.whatwg.org/#rs-pipe-through
ReadableStream* ReadableStream::pipeThrough(ScriptState* script_state,
                                            ReadableWritablePair* transform,
                                            const StreamPipeOptions* options,
                                            ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-pipe-through
  DCHECK(transform->hasReadable());
  ReadableStream* readable_stream = transform->readable();

  DCHECK(transform->hasWritable());
  WritableStream* writable_stream = transform->writable();

  // 1. If ! IsReadableStreamLocked(this) is true, throw a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return nullptr;
  }

  // 2. If ! IsWritableStreamLocked(transform["writable"]) is true, throw a
  //    TypeError exception.
  if (WritableStream::IsLocked(writable_stream)) {
    exception_state.ThrowTypeError("parameter 1's 'writable' is locked");
    return nullptr;
  }

  // 3. Let signal be options["signal"] if it exists, or undefined otherwise.
  auto* pipe_options = MakeGarbageCollected<PipeOptions>(options);

  // 4. Let promise be ! ReadableStreamPipeTo(this, transform["writable"],
  //    options["preventClose"], options["preventAbort"],
  //    options["preventCancel"], signal).
  auto promise = PipeTo(script_state, this, writable_stream, pipe_options,
                        exception_state);

  // 5. Set promise.[[PromiseIsHandled]] to true.
  promise.MarkAsHandled();

  // 6. Return transform["readable"].
  return readable_stream;
}

ScriptPromise<IDLUndefined> ReadableStream::pipeTo(
    ScriptState* script_state,
    WritableStream* destination,
    ExceptionState& exception_state) {
  return pipeTo(script_state, destination, StreamPipeOptions::Create(),
                exception_state);
}

ScriptPromise<IDLUndefined> ReadableStream::pipeTo(
    ScriptState* script_state,
    WritableStream* destination,
    const StreamPipeOptions* options,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-pipe-to
  // 1. If ! IsReadableStreamLocked(this) is true, return a promise rejected
  //    with a TypeError exception.
  if (IsLocked(this)) {
    exception_state.ThrowTypeError("Cannot pipe a locked stream");
    return EmptyPromise();
  }

  // 2. If ! IsWritableStreamLocked(destination) is true, return a promise
  //    rejected with a TypeError exception.
  if (WritableStream::IsLocked(destination)) {
    exception_state.ThrowTypeError("Cannot pipe to a locked stream");
    return EmptyPromise();
  }

  // 3. Let signal be options["signal"] if it exists, or undefined otherwise.
  auto* pipe_options = MakeGarbageCollected<PipeOptions>(options);

  // 4. Return ! ReadableStreamPipeTo(this, destination,
  //    options["preventClose"], options["preventAbort"],
  //    options["preventCancel"], signal).
  return PipeTo(script_state, this, destination, pipe_options, exception_state);
}

HeapVector<Member<ReadableStream>> ReadableStream::tee(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return CallTeeAndReturnBranchArray(script_state, this, false,
                                     exception_state);
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
  TryRethrowScope rethrow_scope(isolate, exception_state);
  v8::Local<v8::Value> type;
  if (!underlying_source->Get(context, V8AtomicString(isolate, "type"))
           .ToLocal(&type)) {
    return;
  }

  if (!type->IsUndefined()) {
    // 5. Let typeString be ? ToString(type).
    v8::Local<v8::String> type_string;
    if (!type->ToString(context).ToLocal(&type_string)) {
      return;
    }

    // 6. If typeString is "bytes",
    if (type_string->StringEquals(V8AtomicString(isolate, "bytes"))) {
      UseCounter::Count(ExecutionContext::From(script_state),
                        WebFeature::kReadableStreamWithByteSource);

      UnderlyingSource* underlying_source_dict =
          NativeValueTraits<UnderlyingSource>::NativeValue(
              script_state->GetIsolate(), raw_underlying_source.V8Value(),
              exception_state);
      if (!strategy_unpacker.IsSizeUndefined()) {
        exception_state.ThrowRangeError(
            "Cannot create byte stream with size() defined on the strategy");
        return;
      }
      double high_water_mark =
          strategy_unpacker.GetHighWaterMark(script_state, 0, exception_state);
      if (exception_state.HadException()) {
        return;
      }
      ReadableByteStreamController::SetUpFromUnderlyingSource(
          script_state, this, underlying_source, underlying_source_dict,
          high_water_mark, exception_state);
      return;
    }

    // 8. Otherwise, throw a RangeError exception.
    else {
      exception_state.ThrowRangeError("Invalid type is specified");
      return;
    }
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
ReadableStreamDefaultReader* ReadableStream::AcquireDefaultReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#acquire-readable-stream-reader
  // 1. Let reader by a new ReadableStreamDefaultReader.
  // 2. Perform ? SetUpReadableStreamReader(reader, stream).
  auto* reader = MakeGarbageCollected<ReadableStreamDefaultReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 3. Return reader.
  return reader;
}

ReadableStreamBYOBReader* ReadableStream::AcquireBYOBReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#acquire-readable-stream-byob-reader
  // 1. Let reader be a new ReadableStreamBYOBReader.
  // 2. Perform ? SetUpBYOBReader(reader, stream).
  auto* reader = MakeGarbageCollected<ReadableStreamBYOBReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  // 3. Return reader.
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

void ReadableStream::Tee(ScriptState* script_state,
                         ReadableStream** branch1,
                         ReadableStream** branch2,
                         bool clone_for_branch2,
                         ExceptionState& exception_state) {
  auto* engine = MakeGarbageCollected<TeeEngine>();
  engine->Start(script_state, this, clone_for_branch2, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // Instead of returning a List like ReadableStreamTee in the standard, the
  // branches are returned via output parameters.
  *branch1 = engine->Branch1();
  *branch2 = engine->Branch2();
}

void ReadableStream::ByteStreamTee(ScriptState* script_state,
                                   ReadableStream** branch1,
                                   ReadableStream** branch2,
                                   ExceptionState& exception_state) {
  auto* engine = MakeGarbageCollected<ByteStreamTeeEngine>();
  engine->Start(script_state, this, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // Instead of returning a List like ReadableByteStreamTee in the standard, the
  // branches are returned via output parameters.
  *branch1 = engine->Branch1();
  *branch2 = engine->Branch2();
}

void ReadableStream::LockAndDisturb(ScriptState* script_state) {
  if (reader_) {
    return;
  }

  DCHECK(!IsLocked(this));

  // Since the stream is not locked, AcquireDefaultReader cannot fail.
  NonThrowableExceptionState exception_state(__FILE__, __LINE__);
  ReadableStreamGenericReader* reader =
      AcquireDefaultReader(script_state, this, exception_state);
  DCHECK(reader);

  is_disturbed_ = true;
}

void ReadableStream::CloseStream(ScriptState* script_state,
                                 ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readablestream-close
  // 1. If stream.[[controller]] implements ReadableByteStreamController,
  if (auto* readable_byte_stream_controller =
          DynamicTo<ReadableByteStreamController>(
              readable_stream_controller_.Get())) {
    // 1. Perform ! ReadableByteStreamControllerClose(stream.[[controller]]).
    TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
    readable_byte_stream_controller->Close(script_state,
                                           readable_byte_stream_controller);
    if (rethrow_scope.HasCaught()) {
      return;
    }

    // 2. If stream.[[controller]].[[pendingPullIntos]] is not empty, perform !
    // ReadableByteStreamControllerRespond(stream.[[controller]], 0).
    if (readable_byte_stream_controller->pending_pull_intos_.size() > 0) {
      readable_byte_stream_controller->Respond(
          script_state, readable_byte_stream_controller, 0, exception_state);
    }
    if (exception_state.HadException()) {
      return;
    }
  }

  // 2. Otherwise, perform !
  // ReadableStreamDefaultControllerClose(stream.[[controller]]).
  else {
    auto* readable_stream_default_controller =
        To<ReadableStreamDefaultController>(readable_stream_controller_.Get());
    ReadableStreamDefaultController::Close(script_state,
                                           readable_stream_default_controller);
  }
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
  auto* writable = CreateCrossRealmTransformWritable(
      script_state, port, allow_per_chunk_transferring_, /*optimizer=*/nullptr,
      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 7. Let promise be ! ReadableStreamPipeTo(value, writable, false, false,
  //    false).
  auto promise = PipeTo(script_state, this, writable,
                        MakeGarbageCollected<PipeOptions>(), exception_state);

  // 8. Set promise.[[PromiseIsHandled]] to true.
  promise.MarkAsHandled();

  // This step is done in a roundabout way by the caller:
  // 9. Set dataHolder.[[port]] to ! StructuredSerializeWithTransfer(port2,
  //    « port2 »).
}

ReadableStream* ReadableStream::Deserialize(
    ScriptState* script_state,
    MessagePort* port,
    std::unique_ptr<ReadableStreamTransferringOptimizer> optimizer,
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
  auto* readable = CreateCrossRealmTransformReadable(
      script_state, port, std::move(optimizer), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return readable;
}

ScriptPromise<IDLUndefined> ReadableStream::PipeTo(
    ScriptState* script_state,
    ReadableStream* readable,
    WritableStream* destination,
    PipeOptions* pipe_options,
    ExceptionState& exception_state) {
  auto* engine = MakeGarbageCollected<PipeToEngine>(script_state, pipe_options);
  return engine->Start(readable, destination, exception_state);
}

v8::Local<v8::Value> ReadableStream::GetStoredError(
    v8::Isolate* isolate) const {
  return stored_error_.Get(isolate);
}

std::unique_ptr<ReadableStreamTransferringOptimizer>
ReadableStream::TakeTransferringOptimizer() {
  return std::move(transferring_optimizer_);
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

void ReadableStream::AddReadIntoRequest(ScriptState* script_state,
                                        ReadableStream* stream,
                                        ReadIntoRequest* readRequest) {
  // https://streams.spec.whatwg.org/#readable-stream-add-read-into-request
  // 1. Assert: stream.[[reader]] implements ReadableStreamBYOBReader.
  DCHECK(stream->reader_->IsBYOBReader());
  // 2. Assert: stream.[[state]] is "readable" or "closed".
  DCHECK(stream->state_ == kReadable || stream->state_ == kClosed);
  // 3. Append readRequest to stream.[[reader]].[[readIntoRequests]].
  ReadableStreamGenericReader* reader = stream->reader_;
  ReadableStreamBYOBReader* byob_reader = To<ReadableStreamBYOBReader>(reader);
  byob_reader->read_into_requests_.push_back(readRequest);
}

void ReadableStream::AddReadRequest(ScriptState* script_state,
                                    ReadableStream* stream,
                                    ReadRequest* read_request) {
  // https://streams.spec.whatwg.org/#readable-stream-add-read-request
  // 1. Assert: ! IsReadableStreamDefaultReader(stream.[[reader]]) is true.
  DCHECK(stream->reader_->IsDefaultReader());

  // 2. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);

  // 3. Append readRequest to stream.[[reader]].[[readRequests]].
  ReadableStreamGenericReader* reader = stream->reader_;
  ReadableStreamDefaultReader* default_reader =
      To<ReadableStreamDefaultReader>(reader);
  default_reader->read_requests_.push_back(read_request);
}

ScriptPromise<IDLUndefined> ReadableStream::Cancel(
    ScriptState* script_state,
    ReadableStream* stream,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#readable-stream-cancel
  // 1. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  // 2. If stream.[[state]] is "closed", return a promise resolved with
  //    undefined.
  const auto state = stream->state_;
  if (state == kClosed) {
    return ToResolvedUndefinedPromise(script_state);
  }

  // 3. If stream.[[state]] is "errored", return a promise rejected with stream.
  //    [[storedError]].
  if (state == kErrored) {
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, stream->GetStoredError(script_state->GetIsolate()));
  }

  // 4. Perform ! ReadableStreamClose(stream).
  Close(script_state, stream);

  // 5. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;

  // 6. If reader is not undefined and reader implements
  // ReadableStreamBYOBReader,
  if (reader && reader->IsBYOBReader()) {
    //   a. Let readIntoRequests be reader.[[readIntoRequests]].
    ReadableStreamBYOBReader* byob_reader =
        To<ReadableStreamBYOBReader>(reader);
    HeapDeque<Member<ReadIntoRequest>> read_into_requests;
    read_into_requests.Swap(byob_reader->read_into_requests_);

    //   b. Set reader.[[readIntoRequests]] to an empty list.
    //      This is not required since we've already called Swap().

    //   c. For each readIntoRequest of readIntoRequests,
    for (ReadIntoRequest* request : read_into_requests) {
      //     i. Perform readIntoRequest's close steps, given undefined.
      request->CloseSteps(script_state, nullptr);
    }
  }

  // 7. Let sourceCancelPromise be !
  // stream.[[controller]].[[CancelSteps]](reason).
  v8::Local<v8::Promise> source_cancel_promise =
      stream->readable_stream_controller_->CancelSteps(script_state, reason);

  enum FunctionType { kResolve, kReject };
  class ResolveUndefinedFunction final : public PromiseHandler {
   public:
    ResolveUndefinedFunction(ScriptPromiseResolver<IDLUndefined>* resolver,
                             FunctionType type)
        : resolver_(resolver), type_(type) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> value) override {
      if (type_ == kResolve) {
        resolver_->Resolve();
      } else {
        resolver_->Reject(value);
      }
    }

    void Trace(Visitor* visitor) const override {
      PromiseHandler::Trace(visitor);
      visitor->Trace(resolver_);
    }

   private:
    Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
    FunctionType type_;
  };

  // 8. Return the result of reacting to sourceCancelPromise with a
  //    fulfillment step that returns undefined.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  StreamThenPromise(
      script_state->GetContext(), source_cancel_promise,
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<ResolveUndefinedFunction>(resolver, kResolve)),
      MakeGarbageCollected<ScriptFunction>(
          script_state,
          MakeGarbageCollected<ResolveUndefinedFunction>(resolver, kReject)));
  return resolver->Promise();
}

void ReadableStream::Close(ScriptState* script_state, ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-close
  // 1. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);

  // 2. Set stream.[[state]] to "closed".
  stream->state_ = kClosed;

  // 3. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;

  // 4. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // Don't resolve promises if the context has been destroyed.
  if (ExecutionContext::From(script_state)->IsContextDestroyed())
    return;

  // 5. Resolve reader.[[closedPromise]] with undefined.
  reader->ClosedResolver()->Resolve();

  // 6. If reader implements ReadableStreamDefaultReader,
  if (reader->IsDefaultReader()) {
    //   a. Let readRequests be reader.[[readRequests]].
    HeapDeque<Member<ReadRequest>> requests;
    requests.Swap(To<ReadableStreamDefaultReader>(reader)->read_requests_);
    //   b. Set reader.[[readRequests]] to an empty list.`
    //      This is not required since we've already called Swap()

    //   c. For each readRequest of readRequests,
    for (ReadRequest* request : requests) {
      //     i. Perform readRequest’s close steps.
      request->CloseSteps(script_state);
    }
  }
}

void ReadableStream::Error(ScriptState* script_state,
                           ReadableStream* stream,
                           v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-stream-error
  // 1. Assert: stream.[[state]] is "readable".
  CHECK_EQ(stream->state_, kReadable);
  auto* isolate = script_state->GetIsolate();

  // 2. Set stream.[[state]] to "errored".
  stream->state_ = kErrored;

  // 3. Set stream.[[storedError]] to e.
  stream->stored_error_.Reset(isolate, e);

  // 4. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;

  // 5. If reader is undefined, return.
  if (!reader) {
    return;
  }

  // 6. Reject reader.[[closedPromise]] with e.
  reader->ClosedResolver()->Reject(ScriptValue(isolate, e));

  // 7. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed(script_state).MarkAsHandled();

  // 8. If reader implements ReadableStreamDefaultReader,
  if (reader->IsDefaultReader()) {
    //   a. Perform ! ReadableStreamDefaultReaderErrorReadRequests(reader, e).
    ReadableStreamDefaultReader* default_reader =
        To<ReadableStreamDefaultReader>(reader);
    ReadableStreamDefaultReader::ErrorReadRequests(script_state, default_reader,
                                                   e);
  } else {
    // 9. Otherwise,
    // a. Assert: reader implements ReadableStreamBYOBReader.
    DCHECK(reader->IsBYOBReader());
    // b. Perform ! ReadableStreamBYOBReaderErrorReadIntoRequests(reader, e).
    ReadableStreamBYOBReader* byob_reader =
        To<ReadableStreamBYOBReader>(reader);
    ReadableStreamBYOBReader::ErrorReadIntoRequests(script_state, byob_reader,
                                                    e);
  }
}

void ReadableStream::FulfillReadIntoRequest(ScriptState* script_state,
                                            ReadableStream* stream,
                                            DOMArrayBufferView* chunk,
                                            bool done,
                                            ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-into-request
  // 1. Assert: ! ReadableStreamHasBYOBReader(stream) is true.
  DCHECK(HasBYOBReader(stream));
  // 2. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;
  ReadableStreamBYOBReader* byob_reader = To<ReadableStreamBYOBReader>(reader);
  // 3. Assert: reader.[[readIntoRequests]] is not empty.
  DCHECK(!byob_reader->read_into_requests_.empty());
  // 4. Let readIntoRequest be reader.[[readIntoRequests]][0].
  ReadIntoRequest* read_into_request = byob_reader->read_into_requests_[0];
  // 5. Remove readIntoRequest from reader.[[readIntoRequests]].
  byob_reader->read_into_requests_.pop_front();
  // 6. If done is true, perform readIntoRequest’s close steps, given chunk.
  if (done) {
    read_into_request->CloseSteps(script_state, chunk);
  } else {
    // 7. Otherwise, perform readIntoRequest’s chunk steps, given chunk.
    read_into_request->ChunkSteps(script_state, chunk, exception_state);
  }
}

void ReadableStream::FulfillReadRequest(ScriptState* script_state,
                                        ReadableStream* stream,
                                        v8::Local<v8::Value> chunk,
                                        bool done,
                                        ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
  // 1. Assert: ! ReadableStreamHasDefaultReader(stream) is true.
  DCHECK(HasDefaultReader(stream));

  // 2. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;
  ReadableStreamDefaultReader* default_reader =
      To<ReadableStreamDefaultReader>(reader);

  // 3. Assert: reader.[[readRequests]] is not empty.
  DCHECK(!default_reader->read_requests_.empty());

  // 4. Let readRequest be reader.[[readRequests]][0].
  ReadRequest* read_request = default_reader->read_requests_[0];

  // 5. Remove readRequest from reader.[[readRequests]].
  default_reader->read_requests_.pop_front();

  // 6. If done is true, perform readRequest’s close steps.
  if (done) {
    read_request->CloseSteps(script_state);
  } else {
    // 7. Otherwise, perform readRequest’s chunk steps, given chunk.
    read_request->ChunkSteps(script_state, chunk, exception_state);
  }
}

int ReadableStream::GetNumReadIntoRequests(const ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-into-requests
  // 1. Assert: ! ReadableStreamHasBYOBReader(stream) is true.
  DCHECK(HasBYOBReader(stream));
  // 2. Return stream.[[reader]].[[readIntoRequests]]'s size.
  ReadableStreamGenericReader* reader = stream->reader_;
  return To<ReadableStreamBYOBReader>(reader)->read_into_requests_.size();
}

int ReadableStream::GetNumReadRequests(const ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
  // 1. Assert: ! ReadableStreamHasDefaultReader(stream) is true.
  DCHECK(HasDefaultReader(stream));
  // 2. Return the number of elements in stream.[[reader]].[[readRequests]].
  ReadableStreamGenericReader* reader = stream->reader_;
  return To<ReadableStreamDefaultReader>(reader)->read_requests_.size();
}

bool ReadableStream::HasBYOBReader(const ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-has-byob-reader
  // 1. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;

  // 2. If reader is undefined, return false.
  if (!reader) {
    return false;
  }

  // 3. If reader implements ReadableStreamBYOBReader, return true.
  // 4. Return false.
  return reader->IsBYOBReader();
}

bool ReadableStream::HasDefaultReader(const ReadableStream* stream) {
  // https://streams.spec.whatwg.org/#readable-stream-has-default-reader
  // 1. Let reader be stream.[[reader]].
  ReadableStreamGenericReader* reader = stream->reader_;

  // 2. If reader is undefined, return false.
  if (!reader) {
    return false;
  }

  // 3. If reader implements ReadableStreamDefaultReader, return true.
  // 4. Return false.
  return reader->IsDefaultReader();
}

HeapVector<Member<ReadableStream>> ReadableStream::CallTeeAndReturnBranchArray(
    ScriptState* script_state,
    ReadableStream* readable,
    bool clone_for_branch2,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-tee
  ReadableStream* branch1 = nullptr;
  ReadableStream* branch2 = nullptr;

  // 2. Let branches be ? ReadableStreamTee(this, false).
  if (readable->readable_stream_controller_->IsByteStreamController()) {
    readable->ByteStreamTee(script_state, &branch1, &branch2, exception_state);
  } else {
    DCHECK(readable->readable_stream_controller_->IsDefaultController());
    readable->Tee(script_state, &branch1, &branch2, clone_for_branch2,
                  exception_state);
  }

  if (!branch1 || !branch2)
    return HeapVector<Member<ReadableStream>>();

  DCHECK(!exception_state.HadException());

  // 3. Return ! CreateArrayFromList(branches).
  return HeapVector<Member<ReadableStream>>({branch1, branch2});
}

ReadableStream::IterationSourceBase* ReadableStream::CreateIterationSource(
    ScriptState* script_state,
    ReadableStream::IterationSourceBase::Kind kind,
    ReadableStreamIteratorOptions* options,
    ExceptionState& exception_state) {
  // 1. Let reader be ? AcquireReadableStreamDefaultReader(stream).
  ReadableStreamDefaultReader* reader =
      AcquireDefaultReader(script_state, this, exception_state);
  if (!reader) {
    return nullptr;
  }
  // 3. Let preventCancel be args[0]["preventCancel"].
  bool prevent_cancel = options->preventCancel();
  return MakeGarbageCollected<IterationSource>(script_state, kind, reader,
                                               prevent_cancel);
}

}  // namespace blink
