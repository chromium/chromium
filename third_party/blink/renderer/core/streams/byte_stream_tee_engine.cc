// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/byte_stream_tee_engine.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/read_into_request.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ByteStreamTeeEngine::PullAlgorithm final : public StreamAlgorithm {
 public:
  PullAlgorithm(ByteStreamTeeEngine* engine, int branch)
      : engine_(engine), branch_(branch) {
    DCHECK(branch == 0 || branch == 1);
  }

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
    // This implements both pull1Algorithm and pull2Algorithm as they are
    // identical except for the index they operate on. Standard comments are
    // from pull1Algorithm.
    // 17. Let pull1Algorithm be the following steps:
    //   a. If reading is true,
    ExceptionState exception_state(script_state->GetIsolate());
    if (engine_->reading_) {
      //     i. Set readAgainForBranch1 to true.
      engine_->read_again_for_branch_[branch_] = true;
      //     ii. Return a promise resolved with undefined.
      return PromiseResolveWithUndefined(script_state);
    }
    //   b. Set reading to true.
    engine_->reading_ = true;
    //   c. Let byobRequest be !
    //   ReadableByteStreamControllerGetBYOBRequest(branch1.[[controller]]).
    ReadableStreamBYOBRequest* byob_request =
        ReadableByteStreamController::GetBYOBRequest(
            engine_->controller_[branch_]);
    //   d. If byobRequest is null, perform pullWithDefaultReader.
    if (!byob_request) {
      engine_->PullWithDefaultReader(script_state, exception_state);
    } else {
      //   e. Otherwise, perform pullWithBYOBReader, given byobRequest.[[view]]
      //   and false.
      engine_->PullWithBYOBReader(script_state, byob_request->view(), branch_,
                                  exception_state);
    }
    //   f. Return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<ByteStreamTeeEngine> engine_;
  const int branch_;
};

class ByteStreamTeeEngine::CancelAlgorithm final : public StreamAlgorithm {
 public:
  CancelAlgorithm(ByteStreamTeeEngine* engine, int branch)
      : engine_(engine), branch_(branch) {
    DCHECK(branch == 0 || branch == 1);
  }

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
    // This implements both cancel1Algorithm and cancel2Algorithm as they are
    // identical except for the index they operate on. Standard comments are
    // from cancel1Algorithm.
    // 19. Let cancel1Algorithm be the following steps, taking a reason
    // argument:
    auto* isolate = script_state->GetIsolate();
    //   a. Set canceled1 to true.
    engine_->canceled_[branch_] = true;
    //   b. Set reason1 to reason.
    DCHECK_EQ(argc, 1);
    engine_->reason_[branch_].Reset(isolate, argv[0]);
    //   c. If canceled2 is true,
    const int other_branch = 1 - branch_;
    if (engine_->canceled_[other_branch]) {
      //     i. Let compositeReason be ! CreateArrayFromList(« reason1, reason2
      //     »).
      v8::Local<v8::Value> reason[] = {engine_->reason_[0].Get(isolate),
                                       engine_->reason_[1].Get(isolate)};
      v8::Local<v8::Value> composite_reason =
          v8::Array::New(script_state->GetIsolate(), reason, 2);
      //     ii. Let cancelResult be ! ReadableStreamCancel(stream,
      //     compositeReason).
      auto cancel_result = ReadableStream::Cancel(
          script_state, engine_->stream_, composite_reason);
      //     iii. Resolve cancelPromise with cancelResult.
      engine_->cancel_promise_->Resolve(cancel_result);
    }
    //   d. Return cancelPromise.
    return engine_->cancel_promise_->V8Promise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<ByteStreamTeeEngine> engine_;
  const int branch_;
};

class ByteStreamTeeEngine::ByteTeeReadRequest final : public ReadRequest {
 public:
  explicit ByteTeeReadRequest(ByteStreamTeeEngine* engine) : engine_(engine) {}

  void ChunkSteps(ScriptState* script_state,
                  v8::Local<v8::Value> chunk,
                  ExceptionState&) const override {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state)->GetAgent()->event_loop();
    v8::Global<v8::Value> value(script_state->GetIsolate(), chunk);
    event_loop->EnqueueMicrotask(
        WTF::BindOnce(&ByteTeeReadRequest::ChunkStepsBody, WrapPersistent(this),
                      WrapPersistent(script_state), std::move(value)));
  }

  void CloseSteps(ScriptState* script_state) const override {
    // 1. Set reading to false.
    engine_->reading_ = false;
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::TryCatch try_catch(isolate);
    // 2. If canceled1 is false, perform !
    // ReadableByteStreamControllerClose(branch1.[[controller]]).
    // 3. If canceled2 is false, perform !
    // ReadableByteStreamControllerClose(branch2.[[controller]]).
    for (int branch = 0; branch < 2; ++branch) {
      if (!engine_->canceled_[branch]) {
        engine_->controller_[branch]->Close(script_state,
                                            engine_->controller_[branch]);
        if (try_catch.HasCaught()) {
          // Instead of returning a rejection, which is inconvenient here,
          // call ControllerError(). The only difference this makes is that it
          // happens synchronously, but that should not be observable.
          ReadableByteStreamController::Error(script_state,
                                              engine_->controller_[branch],
                                              try_catch.Exception());
          return;
        }
      }
    }
    // 4. If branch1.[[controller]].[[pendingPullIntos]] is not empty, perform
    // ! ReadableByteStreamControllerRespond(branch1.[[controller]], 0).
    // 5. If branch2.[[controller]].[[pendingPullIntos]] is not empty, perform
    // ! ReadableByteStreamControllerRespond(branch2.[[controller]], 0).
    for (int branch = 0; branch < 2; ++branch) {
      if (!engine_->controller_[branch]->pending_pull_intos_.empty()) {
        ReadableByteStreamController::Respond(script_state,
                                              engine_->controller_[branch], 0,
                                              PassThroughException(isolate));
        if (try_catch.HasCaught()) {
          // Instead of returning a rejection, which is inconvenient here,
          // call ControllerError(). The only difference this makes is that it
          // happens synchronously, but that should not be observable.
          ReadableByteStreamController::Error(script_state,
                                              engine_->controller_[branch],
                                              try_catch.Exception());
          return;
        }
      }
    }
    // 6. If canceled1 is false or canceled2 is false, resolve cancelPromise
    // with undefined.
    if (!engine_->canceled_[0] || !engine_->canceled_[1]) {
      engine_->cancel_promise_->Resolve();
    }
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    // 1. Set reading to false.
    engine_->reading_ = false;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    ReadRequest::Trace(visitor);
  }

 private:
  void ChunkStepsBody(ScriptState* script_state,
                      v8::Global<v8::Value> value) const {
    ScriptState::Scope scope(script_state);
    v8::Isolate* isolate = script_state->GetIsolate();
    // 1. Set readAgainForBranch1 to false.
    engine_->read_again_for_branch_[0] = false;
    // 2. Set readAgainForBranch2 to false.
    engine_->read_again_for_branch_[1] = false;

    ExceptionState exception_state(isolate);

    // 3. Let chunk1 and chunk2 be chunk.
    NotShared<DOMUint8Array> buffer_view =
        NativeValueTraits<NotShared<DOMUint8Array>>::NativeValue(
            isolate, value.Get(isolate), exception_state);
    std::array<NotShared<DOMUint8Array>, 2> chunk = {buffer_view, buffer_view};

    // 4. If canceled1 is false and canceled2 is false,
    if (!engine_->canceled_[0] && !engine_->canceled_[1]) {
      //   a. Let cloneResult be CloneAsUint8Array(chunk).
      auto* clone_result = engine_->CloneAsUint8Array(buffer_view.Get());
      //   b. If cloneResult is an abrupt completion,
      //     i. Perform !
      //     ReadableByteStreamControllerError(branch1.[[controller]],
      //     cloneResult.[[Value]]).
      //     ii. Perform !
      //     ReadableByteStreamControllerError(branch2.[[controller]],
      //     cloneResult.[[Value]]).
      //     iii. Resolve cancelPromise with !
      //     ReadableStreamCancel(stream, cloneResult.[[Value]]).
      //     iv. Return.
      //   This is not needed as DOMArrayBuffer::Create(), which is used in
      //   CloneAsUint8Array(), is designed to crash if it cannot allocate the
      //   memory.

      //   c. Otherwise, set chunk2 to cloneResult.[[Value]].
      chunk[1] = NotShared<DOMUint8Array>(clone_result);
    }

    // 5. If canceled1 is false, perform !
    // ReadableByteStreamControllerEnqueue(branch1.[[controller]], chunk1).
    // 6. If canceled2 is false, perform !
    // ReadableByteStreamControllerEnqueue(branch2.[[controller]], chunk2).
    for (int branch = 0; branch < 2; ++branch) {
      if (!engine_->canceled_[branch]) {
        v8::TryCatch try_catch(isolate);
        ReadableByteStreamController::Enqueue(
            script_state, engine_->controller_[branch], chunk[branch],
            PassThroughException(isolate));
        if (try_catch.HasCaught()) {
          // Instead of returning a rejection, which is inconvenient here,
          // call ControllerError(). The only difference this makes is that it
          // happens synchronously, but that should not be observable.
          ReadableByteStreamController::Error(script_state,
                                              engine_->controller_[branch],
                                              try_catch.Exception());
          return;
        }
      }
    }

    // 7. Set reading to false.
    engine_->reading_ = false;

    // 8. If readAgainForBranch1 is true, perform pull1Algorithm.
    if (engine_->read_again_for_branch_[0]) {
      auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(engine_, 0);
      pull_algorithm->Run(script_state, 0, nullptr);
      // 9. Otherwise, if readAgainForBranch2 is true, perform pull2Algorithm.
    } else if (engine_->read_again_for_branch_[1]) {
      auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(engine_, 1);
      pull_algorithm->Run(script_state, 0, nullptr);
    }
  }

  Member<ByteStreamTeeEngine> engine_;
};

class ByteStreamTeeEngine::ByteTeeReadIntoRequest final
    : public ReadIntoRequest {
 public:
  explicit ByteTeeReadIntoRequest(ByteStreamTeeEngine* engine,
                                  ReadableStream* byob_branch,
                                  ReadableStream* other_branch,
                                  bool for_branch_2)
      : engine_(engine),
        byob_branch_(byob_branch),
        other_branch_(other_branch),
        for_branch_2_(for_branch_2) {}

  void ChunkSteps(ScriptState* script_state,
                  DOMArrayBufferView* chunk,
                  ExceptionState&) const override {
    scoped_refptr<scheduler::EventLoop> event_loop =
        ExecutionContext::From(script_state)->GetAgent()->event_loop();
    event_loop->EnqueueMicrotask(WTF::BindOnce(
        &ByteTeeReadIntoRequest::ChunkStepsBody, WrapPersistent(this),
        WrapPersistent(script_state), WrapPersistent(chunk)));
  }

  void CloseSteps(ScriptState* script_state,
                  DOMArrayBufferView* chunk) const override {
    // 1. Set reading to false.
    engine_->reading_ = false;
    // 2. Let byobCanceled be canceled2 if forBranch2 is true, and canceled1
    //    otherwise.
    auto byob_canceled =
        for_branch_2_ ? engine_->canceled_[1] : engine_->canceled_[0];
    // 3. Let otherCanceled be canceled2 if forBranch2 is false, and canceled1
    //    otherwise.
    auto other_canceled =
        !for_branch_2_ ? engine_->canceled_[1] : engine_->canceled_[0];
    // 4. If byobCanceled is false, perform !
    //    ReadableByteStreamControllerClose(byobBranch.[[controller]]).
    if (!byob_canceled) {
      ReadableStreamController* controller =
          byob_branch_->readable_stream_controller_;
      ReadableByteStreamController* byte_controller =
          To<ReadableByteStreamController>(controller);
      byte_controller->Close(script_state, byte_controller);
    }
    // 5. If otherCanceled is false, perform !
    //    ReadableByteStreamControllerClose(otherBranch.[[controller]]).
    if (!other_canceled) {
      ReadableStreamController* controller =
          other_branch_->readable_stream_controller_;
      ReadableByteStreamController* byte_controller =
          To<ReadableByteStreamController>(controller);
      byte_controller->Close(script_state, byte_controller);
    }
    // 6. If chunk is not undefined,
    if (chunk) {
      //   a. Assert: chunk.[[ByteLength]] is 0.
      DCHECK_EQ(chunk->byteLength(), 0u);
      //   b. If byobCanceled is false, perform !
      //      ReadableByteStreamControllerRespondWithNewView(byobBranch.[[controller]],
      //      chunk).
      ExceptionState exception_state(script_state->GetIsolate());
      if (!byob_canceled) {
        ReadableStreamController* controller =
            byob_branch_->readable_stream_controller_;
        ReadableByteStreamController::RespondWithNewView(
            script_state, To<ReadableByteStreamController>(controller),
            NotShared<DOMArrayBufferView>(chunk), exception_state);
        DCHECK(!exception_state.HadException());
      }
      //   c. If otherCanceled is false and
      //      otherBranch.[[controller]].[[pendingPullIntos]] is not empty,
      //      perform !
      //      ReadableByteStreamControllerRespond(otherBranch.[[controller]],
      //      0).
      ReadableStreamController* controller =
          other_branch_->readable_stream_controller_;
      if (!other_canceled && !To<ReadableByteStreamController>(controller)
                                  ->pending_pull_intos_.empty()) {
        ReadableByteStreamController::Respond(
            script_state, To<ReadableByteStreamController>(controller), 0,
            exception_state);
        DCHECK(!exception_state.HadException());
      }
    }
    // 7. If byobCanceled is false or otherCanceled is false, resolve
    //    cancelPromise with undefined.
    if (!byob_canceled || !other_canceled) {
      engine_->cancel_promise_->Resolve();
    }
  }

  void ErrorSteps(ScriptState* script_state,
                  v8::Local<v8::Value> e) const override {
    // 1. Set reading to false.
    engine_->reading_ = false;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    visitor->Trace(byob_branch_);
    visitor->Trace(other_branch_);
    ReadIntoRequest::Trace(visitor);
  }

 private:
  void ChunkStepsBody(ScriptState* script_state,
                      DOMArrayBufferView* chunk) const {
    // This is called in a microtask, the ScriptState needs to be put back
    // in scope.
    ScriptState::Scope scope(script_state);
    // 1. Set readAgainForBranch1 to false.
    engine_->read_again_for_branch_[0] = false;
    // 2. Set readAgainForBranch2 to false.
    engine_->read_again_for_branch_[1] = false;
    // 3. Let byobCanceled be canceled2 if forBranch2 is true, and canceled1
    // otherwise.
    auto byob_canceled =
        for_branch_2_ ? engine_->canceled_[1] : engine_->canceled_[0];
    // 4. Let otherCanceled be canceled2 if forBranch2 is false, and canceled1
    // otherwise.
    auto other_canceled =
        !for_branch_2_ ? engine_->canceled_[1] : engine_->canceled_[0];
    // 5. If otherCanceled is false,
    ExceptionState exception_state(script_state->GetIsolate());
    if (!other_canceled) {
      //   a. Let cloneResult be CloneAsUint8Array(chunk).
      auto* clone_result = engine_->CloneAsUint8Array(chunk);
      //   b. If cloneResult is an abrupt completion,
      //     i. Perform !
      //     ReadableByteStreamControllerError(byobBranch.[[controller]],
      //     cloneResult.[[Value]]).
      //     ii. Perform !
      //     ReadableByteStreamControllerError(otherBranch.[[controller]],
      //     cloneResult.[[Value]]).
      //     iii. Resolve cancelPromise with !
      //     ReadableStreamCancel(stream, cloneResult.[[Value]]).
      //     iv. Return.
      //   This is not needed as DOMArrayBuffer::Create(), which is used in
      //   CloneAsUint8Array(), is designed to crash if it cannot allocate the
      //   memory.

      //   c. Otherwise, let clonedChunk be cloneResult.[[Value]].
      NotShared<DOMArrayBufferView> cloned_chunk =
          NotShared<DOMArrayBufferView>(clone_result);

      //   d. If byobCanceled is false, perform !
      //   ReadableByteStreamControllerRespondWithNewView(byobBranch.[[controller]],
      //   chunk).
      if (!byob_canceled) {
        ReadableStreamController* byob_controller =
            byob_branch_->readable_stream_controller_;
        ReadableByteStreamController::RespondWithNewView(
            script_state, To<ReadableByteStreamController>(byob_controller),
            NotShared<DOMArrayBufferView>(chunk), exception_state);
        DCHECK(!exception_state.HadException());
      }
      //   e. Perform !
      //   ReadableByteStreamControllerEnqueue(otherBranch.[[controller]],
      //   clonedChunk).
      ReadableStreamController* other_controller =
          other_branch_->readable_stream_controller_;
      ReadableByteStreamController::Enqueue(
          script_state, To<ReadableByteStreamController>(other_controller),
          cloned_chunk, exception_state);
      DCHECK(!exception_state.HadException());
      // 6. Otherwise, if byobCanceled is false, perform !
      // ReadableByteStreamControllerRespondWithNewView(byobBranch.[[controller]],
      // chunk).
    } else if (!byob_canceled) {
      ReadableStreamController* controller =
          byob_branch_->readable_stream_controller_;
      ReadableByteStreamController::RespondWithNewView(
          script_state, To<ReadableByteStreamController>(controller),
          NotShared<DOMArrayBufferView>(chunk), exception_state);
      DCHECK(!exception_state.HadException());
    }
    // 7. Set reading to false.
    engine_->reading_ = false;
    // 8. If readAgainForBranch1 is true, perform pull1Algorithm.
    if (engine_->read_again_for_branch_[0]) {
      auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(engine_, 0);
      pull_algorithm->Run(script_state, 0, nullptr);
      // 9. Otherwise, if readAgainForBranch2 is true, perform pull2Algorithm.
    } else if (engine_->read_again_for_branch_[1]) {
      auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(engine_, 1);
      pull_algorithm->Run(script_state, 0, nullptr);
    }
  }

  Member<ByteStreamTeeEngine> engine_;
  Member<ReadableStream> byob_branch_;
  Member<ReadableStream> other_branch_;
  bool for_branch_2_;
};

void ByteStreamTeeEngine::ForwardReaderError(
    ScriptState* script_state,
    ReadableStreamGenericReader* this_reader) {
  // 14. Let forwardReaderError be the following steps, taking a thisReader
  // argument:
  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(ByteStreamTeeEngine* engine,
                            ReadableStreamGenericReader* reader)
        : engine_(engine), reader_(reader) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> r) override {
      //   a. Upon rejection of thisReader.[[closedPromise]] with reason r,
      //     i. If thisReader is not reader, return.
      if (engine_->reader_ != reader_) {
        return;
      }
      //     ii. Perform !
      //     ReadableByteStreamControllerError(branch1.[[controller]], r).
      ReadableByteStreamController::Error(script_state, engine_->controller_[0],
                                          r);
      //     iii. Perform !
      //     ReadableByteStreamControllerError(branch2.[[controller]], r).
      ReadableByteStreamController::Error(script_state, engine_->controller_[1],
                                          r);
      //     iv. If canceled1 is false or canceled2 is false, resolve
      //     cancelPromise with undefined.
      if (!engine_->canceled_[0] || !engine_->canceled_[1]) {
        engine_->cancel_promise_->Resolve();
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(engine_);
      visitor->Trace(reader_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<ByteStreamTeeEngine> engine_;
    Member<ReadableStreamGenericReader> reader_;
  };

  StreamThenPromise(script_state->GetContext(),
                    this_reader->closed(script_state).V8Promise(), nullptr,
                    MakeGarbageCollected<ScriptFunction>(
                        script_state, MakeGarbageCollected<RejectFunction>(
                                          this, this_reader)));
}

void ByteStreamTeeEngine::PullWithDefaultReader(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // 15. Let pullWithDefaultReader be the following steps:
  //   a. If reader implements ReadableStreamBYOBReader,
  if (reader_->IsBYOBReader()) {
    //     i. Assert: reader.[[readIntoRequests]] is empty.
    ReadableStreamGenericReader* reader = reader_;
    ReadableStreamBYOBReader* byob_reader =
        To<ReadableStreamBYOBReader>(reader);
    DCHECK(byob_reader->read_into_requests_.empty());
    //     ii. Perform ! ReadableStreamBYOBReaderRelease(reader).
    ReadableStreamBYOBReader::Release(script_state, byob_reader);
    //     iii. Set reader to ! AcquireReadableStreamDefaultReader(stream).
    reader_ = ReadableStream::AcquireDefaultReader(script_state, stream_,
                                                   exception_state);
    DCHECK(!exception_state.HadException());
    //     iv. Perform forwardReaderError, given reader.
    ForwardReaderError(script_state, reader_);
  }
  //   b. Let readRequest be a read request with the following items:
  auto* read_request = MakeGarbageCollected<ByteTeeReadRequest>(this);
  //   c. Perform ! ReadableStreamDefaultReaderRead(reader, readRequest).
  ReadableStreamGenericReader* reader = reader_;
  ReadableStreamDefaultReader::Read(script_state,
                                    To<ReadableStreamDefaultReader>(reader),
                                    read_request, exception_state);
}

void ByteStreamTeeEngine::PullWithBYOBReader(ScriptState* script_state,
                                             NotShared<DOMArrayBufferView> view,
                                             bool for_branch_2,
                                             ExceptionState& exception_state) {
  // 16. Let pullWithBYOBReader be the following steps, given view and
  // forBranch2:
  //   a. If reader implements ReadableStreamDefaultReader,
  if (reader_->IsDefaultReader()) {
    //     i. Assert: reader.[[readRequests]] is empty.
    ReadableStreamGenericReader* reader = reader_;
    ReadableStreamDefaultReader* default_reader =
        To<ReadableStreamDefaultReader>(reader);
    DCHECK(default_reader->read_requests_.empty());
    //     ii. Perform ! ReadableStreamDefaultReaderRelease(reader).
    ReadableStreamDefaultReader::Release(script_state, default_reader);
    //     iii. Set reader to ! AcquireReadableStreamBYOBReader(stream).
    reader_ = ReadableStream::AcquireBYOBReader(script_state, stream_,
                                                exception_state);
    DCHECK(!exception_state.HadException());
    //     iv. Perform forwardReaderError, given reader.
    ForwardReaderError(script_state, reader_);
  }
  //   b. Let byobBranch be branch2 if forBranch2 is true, and branch1
  //   otherwise.
  ReadableStream* byob_branch = for_branch_2 ? branch_[1] : branch_[0];
  //   c. Let otherBranch be branch2 if forBranch2 is false, and branch1
  //   otherwise.
  ReadableStream* other_branch = !for_branch_2 ? branch_[1] : branch_[0];
  //   d. Let readIntoRequest be a read-into request with the following items:
  auto* read_into_request = MakeGarbageCollected<ByteTeeReadIntoRequest>(
      this, byob_branch, other_branch, for_branch_2);
  //   e. Perform ! ReadableStreamBYOBReaderRead(reader, view,
  //   readIntoRequest).
  ReadableStreamGenericReader* reader = reader_;
  ReadableStreamBYOBReader::Read(script_state,
                                 To<ReadableStreamBYOBReader>(reader), view,
                                 read_into_request, exception_state);
  DCHECK(!exception_state.HadException());
}

DOMUint8Array* ByteStreamTeeEngine::CloneAsUint8Array(
    DOMArrayBufferView* chunk) {
  auto* cloned_buffer = DOMArrayBuffer::Create(chunk->ByteSpan());
  return DOMUint8Array::Create(cloned_buffer, 0, chunk->byteLength());
}

void ByteStreamTeeEngine::Start(ScriptState* script_state,
                                ReadableStream* stream,
                                ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamtee
  // 1. Assert: stream implements ReadableStream.
  DCHECK(stream);

  stream_ = stream;

  // 2. Assert: stream.[[controller]] implements ReadableByteStreamController.
  DCHECK(stream->readable_stream_controller_->IsByteStreamController());

  // 3. Let reader be ? AcquireReadableStreamDefaultReader(stream).
  reader_ = ReadableStream::AcquireDefaultReader(script_state, stream,
                                                 exception_state);

  // 4. Let reading be false.
  DCHECK(!reading_);

  // 5. Let readAgainForBranch1 be false.
  DCHECK(!read_again_for_branch_[0]);

  // 6. Let readAgainForBranch2 be false.
  DCHECK(!read_again_for_branch_[1]);

  // 7. Let canceled1 be false.
  DCHECK(!canceled_[0]);

  // 8. Let canceled2 be false.
  DCHECK(!canceled_[1]);

  // 9. Let reason1 be undefined.
  DCHECK(reason_[0].IsEmpty());

  // 10. Let reason2 be undefined.
  DCHECK(reason_[1].IsEmpty());

  // 11. Let branch1 be undefined.
  DCHECK(!branch_[0]);

  // 12. Let branch2 be undefined.
  DCHECK(!branch_[1]);

  // 13. Let cancelPromise be a new promise.
  cancel_promise_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLPromise<IDLUndefined>>>(
          script_state);

  // 17. Let pull1Algorithm be the following steps:
  // (See PullAlgorithm::Run()).
  auto* pull1_algorithm = MakeGarbageCollected<PullAlgorithm>(this, 0);

  // 18. Let pull2Algorithm be the following steps:
  // (both algorithms share a single implementation).
  auto* pull2_algorithm = MakeGarbageCollected<PullAlgorithm>(this, 1);

  // 19. Let cancel1Algorithm be the following steps, taking a reason argument:
  // (See CancelAlgorithm::Run()).
  auto* cancel1_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 0);

  // 20. Let cancel2Algorithm be the following steps, taking a reason argument:
  // (both algorithms share a single implementation).
  auto* cancel2_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 1);

  // 21. Let startAlgorithm be an algorithm that returns undefined.
  auto* start_algorithm = CreateTrivialStartAlgorithm();

  // 22. Set branch1 to ! CreateReadableByteStream(startAlgorithm,
  // pull1Algorithm, cancel1Algorithm).
  branch_[0] = ReadableStream::CreateByteStream(
      script_state, start_algorithm, pull1_algorithm, cancel1_algorithm,
      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 23. Set branch2 to ! CreateReadableByteStream(startAlgorithm,
  // pull2Algorithm, cancel2Algorithm).
  branch_[1] = ReadableStream::CreateByteStream(
      script_state, start_algorithm, pull2_algorithm, cancel2_algorithm,
      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  for (int branch = 0; branch < 2; ++branch) {
    ReadableStreamController* controller =
        branch_[branch]->readable_stream_controller_;
    // We just created the branches above. It is obvious that they are byte
    // stream controllers.
    controller_[branch] = To<ReadableByteStreamController>(controller);
  }

  // 24. Perform forwardReaderError, given reader.
  ForwardReaderError(script_state, reader_);

  // Step 25. Return « branch1, branch2 ».
  // is performed by the caller.
}

}  // namespace blink
