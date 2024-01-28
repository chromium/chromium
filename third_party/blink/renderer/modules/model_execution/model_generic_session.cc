// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a promise.
class ModelGenericSession::Responder final
    : public GarbageCollected<ModelGenericSession::Responder>,
      public blink::mojom::blink::ModelStreamingResponder {
 public:
  explicit Responder(ScriptState* script_state)
      : resolver_(MakeGarbageCollected<ScriptPromiseResolver>(script_state)) {}
  ~Responder() override = default;

  void Trace(Visitor* visitor) const { visitor->Trace(resolver_); }

  ScriptPromise GetPromise() { return resolver_->Promise(); }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnResponse(mojom::blink::ModelStreamingResponseStatus status,
                  const WTF::String& text) override {
    switch (status) {
      case mojom::blink::ModelStreamingResponseStatus::kOngoing: {
        response_ = text;
        break;
      }
      case mojom::blink::ModelStreamingResponseStatus::kComplete: {
        resolver_->Resolve(response_);
        break;
      }
      case mojom::blink::ModelStreamingResponseStatus::kError: {
        resolver_->Reject();
      }
    }
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  WTF::String response_;
};

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a ReadableStream.
class ModelGenericSession::StreamingResponder final
    : public UnderlyingSourceBase,
      public blink::mojom::blink::ModelStreamingResponder {
 public:
  explicit StreamingResponder(ScriptState* script_state)
      : UnderlyingSourceBase(script_state), script_state_(script_state) {}
  ~StreamingResponder() override = default;

  void Trace(Visitor* visitor) const override {
    UnderlyingSourceBase::Trace(visitor);
    visitor->Trace(script_state_);
  }

  // `UnderlyingSourceBase` implementation.
  ScriptPromise Pull(ScriptState* script_state,
                     ExceptionState& exception_state) override {
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise Cancel(ScriptState* script_state,
                       ScriptValue reason,
                       ExceptionState& exception_state) override {
    return ScriptPromise::CastUndefined(script_state);
  }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnResponse(mojom::blink::ModelStreamingResponseStatus status,
                  const WTF::String& text) override {
    switch (status) {
      case mojom::blink::ModelStreamingResponseStatus::kOngoing: {
        v8::HandleScope handle_scope(script_state_->GetIsolate());
        Controller()->Enqueue(V8String(script_state_->GetIsolate(), text));
        break;
      }
      case mojom::blink::ModelStreamingResponseStatus::kComplete: {
        Controller()->Close();
        break;
      }
      case mojom::blink::ModelStreamingResponseStatus::kError: {
        // TODO(crbug.com/1520700): raise the proper exception based on the spec
        // after the prototype phase.
        Controller()->Error(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotReadableError, "Model execution error"));
      }
    }
  }

 private:
  Member<ScriptState> script_state_;
};

ModelGenericSession::ModelGenericSession(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner) {}

void ModelGenericSession::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(model_session_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::ModelGenericSession>
ModelGenericSession::GetModelSessionReceiver() {
  return model_session_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

ScriptPromise ModelGenericSession::execute(ScriptState* script_state,
                                           const WTF::String& input,
                                           ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return ScriptPromise();
  }

  ModelGenericSession::Responder* responder =
      MakeGarbageCollected<ModelGenericSession::Responder>(script_state);

  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder,
                   ModelGenericSession::Responder>
      receiver{responder, nullptr};

  model_session_remote_->Execute(
      input, receiver.BindNewPipeAndPassRemote(task_runner_));
  return responder->GetPromise();
}

ReadableStream* ModelGenericSession::executeStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The execution context is not valid.");
    return nullptr;
  }

  ModelGenericSession::StreamingResponder* responder =
      MakeGarbageCollected<ModelGenericSession::StreamingResponder>(
          script_state);

  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder,
                   ModelGenericSession::StreamingResponder>
      receiver{responder, nullptr};

  model_session_remote_->Execute(
      input, receiver.BindNewPipeAndPassRemote(task_runner_));

  // Set the high water mark to 1 so the backpressure will be applied on every
  // enqueue.
  return ReadableStream::CreateWithCountQueueingStrategy(script_state,
                                                         responder, 1);
}

}  // namespace blink
