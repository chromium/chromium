// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/tee_engine.h"

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

v8::MaybeLocal<v8::Value> TeeEngine::StructuredClone(
    ScriptState* script_state,
    v8::Local<v8::Value> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-structuredclone
  v8::Context::Scope scope(script_state->GetContext());
  v8::Isolate* isolate = script_state->GetIsolate();

  // 1. Let serialized be ? StructuredSerialize(v).
  scoped_refptr<SerializedScriptValue> serialized =
      SerializedScriptValue::Serialize(
          isolate, chunk,
          SerializedScriptValue::SerializeOptions(
              SerializedScriptValue::kNotForStorage),
          exception_state);
  if (exception_state.HadException()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "chunk could not be cloned.");
    return v8::MaybeLocal<v8::Value>();
  }

  // 2. Return ? StructuredDeserialize(serialized, the current Realm).
  return serialized->Deserialize(isolate);
}

class TeeEngine::PullAlgorithm final : public StreamAlgorithm {
 public:
  explicit PullAlgorithm(TeeEngine* engine) : engine_(engine) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int,
                             v8::Local<v8::Value>[]) override {
    // https://streams.spec.whatwg.org/#readable-stream-tee
    // 13. Let pullAlgorithm be the following steps:
    //   a. If reading is true,
    if (engine_->reading_) {
      //      i. Set readAgain to true.
      engine_->read_again_ = true;
      //      ii. Return a promise resolved with undefined.
      return PromiseResolveWithUndefined(script_state);
    }

    ExceptionState exception_state(script_state->GetIsolate(),
                                   v8::ExceptionContext::kUnknown, "", "");

    //   b. Set reading to true.
    engine_->reading_ = true;
    //   c. Let readRequest be a read request with the following items:
    auto* read_request = MakeGarbageCollected<TeeReadRequest>(engine_);
    //   d. Perform ! ReadableStreamDefaultReaderRead(reader, readRequest).
    ReadableStreamDefaultReader::Read(script_state, engine_->reader_,
                                      read_request, exception_state);
    //   e. Return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  class TeeReadRequest final : public ReadRequest {
   public:
    explicit TeeReadRequest(TeeEngine* engine) : engine_(engine) {}

    void ChunkSteps(ScriptState* script_state,
                    v8::Local<v8::Value> chunk,
                    ExceptionState&) const override {
      scoped_refptr<scheduler::EventLoop> event_loop =
          ExecutionContext::From(script_state)->GetAgent()->event_loop();
      v8::Global<v8::Value> value(script_state->GetIsolate(), chunk);
      event_loop->EnqueueMicrotask(
          WTF::BindOnce(&TeeReadRequest::ChunkStepsBody, WrapPersistent(this),
                        WrapPersistent(script_state), std::move(value)));
    }

    void CloseSteps(ScriptState* script_state) const override {
      // 1. Set reading to false.
      engine_->reading_ = false;

      // 2. If canceled1 is false, perform !
      // ReadableStreamDefaultControllerClose(branch1.[[controller]]).
      // 3. If canceled2 is false, perform !
      // ReadableStreamDefaultControllerClose(branch2.[[controller]]).
      for (int branch = 0; branch < 2; ++branch) {
        if (!engine_->canceled_[branch] &&
            ReadableStreamDefaultController::CanCloseOrEnqueue(
                engine_->controller_[branch])) {
          ReadableStreamDefaultController::Close(script_state,
                                                 engine_->controller_[branch]);
        }
      }

      // 4. If canceled1 is false or canceled2 is false, resolve
      // cancelPromise with undefined.
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
      // This is called in a microtask, the ScriptState needs to be put back
      // in scope.
      ScriptState::Scope scope(script_state);
      v8::Isolate* isolate = script_state->GetIsolate();
      v8::TryCatch try_catch(isolate);
      // 1. Set readAgain to false.
      engine_->read_again_ = false;

      // 2. Let chunk1 and chunk2 be chunk.
      std::array<v8::Local<v8::Value>, 2> chunk;
      chunk[0] = value.Get(isolate);
      chunk[1] = chunk[0];

      // 3. If canceled2 is false and cloneForBranch2 is true,
      if (!engine_->canceled_[1] && engine_->clone_for_branch2_) {
        //   a. Let cloneResult be StructuredClone(chunk2).
        v8::MaybeLocal<v8::Value> clone_result_maybe = engine_->StructuredClone(
            script_state, chunk[1], PassThroughException(isolate));
        v8::Local<v8::Value> clone_result;
        //   b. If cloneResult is an abrupt completion,
        if (!clone_result_maybe.ToLocal(&clone_result)) {
          CHECK(try_catch.HasCaught());
          v8::Local<v8::Value> exception = try_catch.Exception();
          //     i. Perform !
          //     ReadableStreamDefaultControllerError(branch1.[[controller]],
          //     cloneResult.[[Value]]).
          ReadableStreamDefaultController::Error(
              script_state, engine_->controller_[0], exception);
          //     ii. Perform !
          //     ReadableStreamDefaultControllerError(branch2.[[controller]],
          //     cloneResult.[[Value]]).
          ReadableStreamDefaultController::Error(
              script_state, engine_->controller_[1], exception);
          //     iii. Resolve cancelPromise with !
          //     ReadableStreamCancel(stream, cloneResult.[[Value]]).
          engine_->cancel_promise_->Resolve(ReadableStream::Cancel(
              script_state, engine_->stream_, exception));
          //     iv. Return.
          return;
        } else {
          DCHECK(!try_catch.HasCaught());
          //   c. Otherwise, set chunk2 to cloneResult.[[Value]].
          chunk[1] = clone_result;
        }
      }

      // 4. If canceled1 is false, perform !
      // ReadableStreamDefaultControllerEnqueue(branch1.[[controller]], chunk1).
      // 5. If canceled2 is false, perform !
      // ReadableStreamDefaultControllerEnqueue(branch2.[[controller]], chunk2).
      for (int branch = 0; branch < 2; ++branch) {
        if (!engine_->canceled_[branch] &&
            ReadableStreamDefaultController::CanCloseOrEnqueue(
                engine_->controller_[branch])) {
          ReadableStreamDefaultController::Enqueue(
              script_state, engine_->controller_[branch], chunk[branch],
              PassThroughException(isolate));
          if (try_catch.HasCaught()) {
            // Instead of returning a rejection, which is inconvenient here,
            // call ControllerError(). The only difference this makes is that it
            // happens synchronously, but that should not be observable.
            ReadableStreamDefaultController::Error(script_state,
                                                   engine_->controller_[branch],
                                                   try_catch.Exception());
            return;
          }
        }
      }

      // 6. Set reading to false.
      engine_->reading_ = false;

      // 7. If readAgain is true, perform pullAlgorithm.
      if (engine_->read_again_) {
        auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(engine_);
        pull_algorithm->Run(script_state, 0, nullptr);
      }
    }

    Member<TeeEngine> engine_;
  };

  Member<TeeEngine> engine_;
};

class TeeEngine::CancelAlgorithm final : public StreamAlgorithm {
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
    engine_->reason_[branch_].Reset(isolate, argv[0]);

    const int other_branch = 1 - branch_;

    // c. If canceled2 is true,
    if (engine_->canceled_[other_branch]) {
      // i. Let compositeReason be ! CreateArrayFromList(« reason1, reason2 »).
      v8::Local<v8::Value> reason[] = {engine_->reason_[0].Get(isolate),
                                       engine_->reason_[1].Get(isolate)};
      v8::Local<v8::Value> composite_reason =
          v8::Array::New(script_state->GetIsolate(), reason, 2);

      // ii. Let cancelResult be ! ReadableStreamCancel(stream,
      //    compositeReason).
      auto cancel_result = ReadableStream::Cancel(
          script_state, engine_->stream_, composite_reason);

      // iii. Resolve cancelPromise with cancelResult.
      engine_->cancel_promise_->Resolve(cancel_result);
    }
    return engine_->cancel_promise_->V8Promise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(engine_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TeeEngine> engine_;
  const int branch_;
};

void TeeEngine::Start(ScriptState* script_state,
                      ReadableStream* stream,
                      bool clone_for_branch2,
                      ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaulttee
  //  1. Assert: stream implements ReadableStream.
  DCHECK(stream);
  stream_ = stream;

  // 2. Assert: cloneForBranch2 is a boolean.
  clone_for_branch2_ = clone_for_branch2;

  // 3. Let reader be ? AcquireReadableStreamDefaultReader(stream).
  reader_ = ReadableStream::AcquireDefaultReader(script_state, stream,
                                                 exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // These steps are performed by the constructor:
  //  4. Let reading be false.
  DCHECK(!reading_);

  //  5. Let readAgain be false.
  DCHECK(!read_again_);

  //  6. Let canceled1 be false.
  DCHECK(!canceled_[0]);

  //  7. Let canceled2 be false.
  DCHECK(!canceled_[1]);

  //  8. Let reason1 be undefined.
  DCHECK(reason_[0].IsEmpty());

  //  9. Let reason2 be undefined.
  DCHECK(reason_[1].IsEmpty());

  // 10. Let branch1 be undefined.
  DCHECK(!branch_[0]);

  // 11. Let branch2 be undefined.
  DCHECK(!branch_[1]);

  // 12. Let cancelPromise be a new promise.
  cancel_promise_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLPromise<IDLUndefined>>>(
          script_state);

  // 13. Let pullAlgorithm be the following steps:
  // (steps are defined in PullAlgorithm::Run()).
  auto* pull_algorithm = MakeGarbageCollected<PullAlgorithm>(this);

  // 14. Let cancel1Algorithm be the following steps, taking a reason argument:
  // (see CancelAlgorithm::Run()).
  auto* cancel1_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 0);

  // 15. Let cancel2Algorithm be the following steps, taking a reason argument:
  // (both algorithms share a single implementation).
  auto* cancel2_algorithm = MakeGarbageCollected<CancelAlgorithm>(this, 1);

  // 16. Let startAlgorithm be an algorithm that returns undefined.
  auto* start_algorithm = CreateTrivialStartAlgorithm();

  auto* size_algorithm = CreateDefaultSizeAlgorithm();

  // 17. Set branch1 to ! CreateReadableStream(startAlgorithm, pullAlgorithm,
  //   cancel1Algorithm).
  branch_[0] = ReadableStream::Create(script_state, start_algorithm,
                                      pull_algorithm, cancel1_algorithm, 1.0,
                                      size_algorithm, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 18. Set branch2 to ! CreateReadableStream(startAlgorithm, pullAlgorithm,
  //   cancel2Algorithm).
  branch_[1] = ReadableStream::Create(script_state, start_algorithm,
                                      pull_algorithm, cancel2_algorithm, 1.0,
                                      size_algorithm, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  for (int branch = 0; branch < 2; ++branch) {
    ReadableStreamController* controller =
        branch_[branch]->readable_stream_controller_;
    // We just created the branches above. It is obvious that the controllers
    // are default controllers.
    controller_[branch] = To<ReadableStreamDefaultController>(controller);
  }

  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(TeeEngine* engine) : engine_(engine) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> r) override {
      // 18. Upon rejection of reader.[[closedPromise]] with reason r,
      //   a. Perform ! ReadableStreamDefaultControllerError(branch1.
      //      [[readableStreamController]], r).
      ReadableStreamDefaultController::Error(script_state,
                                             engine_->controller_[0], r);

      //   b. Perform ! ReadableStreamDefaultControllerError(branch2.
      //      [[readableStreamController]], r).
      ReadableStreamDefaultController::Error(script_state,
                                             engine_->controller_[1], r);

      // TODO(ricea): Implement https://github.com/whatwg/streams/pull/1045 so
      // this step can be numbered correctly.
      // If canceled1 is false or canceled2 is false, resolve |cancelPromise|
      // with undefined.
      if (!engine_->canceled_[0] || !engine_->canceled_[1]) {
        engine_->cancel_promise_->Resolve();
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(engine_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<TeeEngine> engine_;
  };

  // 19. Upon rejection of reader.[[closedPromise]] with reason r,
  StreamThenPromise(
      script_state->GetContext(), reader_->closed(script_state).V8Promise(),
      nullptr,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectFunction>(this)));

  // Step "20. Return « branch1, branch2 »."
  // is performed by the caller.
}

}  // namespace blink
