// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/ai_text_session.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using blink::mojom::blink::ModelStreamingResponseStatus;

AITextSession::Responder::Responder(blink::ScriptState* script_state)
    : resolver_(
          MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state)),
      receiver_(this, blink::ExecutionContext::From(script_state)) {
  SetContextLifecycleNotifier(ExecutionContext::From(script_state));
}

AITextSession::Responder::~Responder() = default;

void AITextSession::Responder::Trace(Visitor* visitor) const {
  ContextLifecycleObserver::Trace(visitor);
  visitor->Trace(resolver_);
  visitor->Trace(receiver_);
}

ScriptPromiseResolver<IDLString>* AITextSession::Responder::GetResolver() {
  return resolver_;
}

mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
AITextSession::Responder::BindNewPipeAndPassRemote(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return receiver_.BindNewPipeAndPassRemote(task_runner);
}

void AITextSession::Responder::OnResponse(ModelStreamingResponseStatus status,
                                          const WTF::String& text) {
  base::UmaHistogramEnumeration(AIMetrics::GetAISessionResponseStatusMetricName(
                                    AIMetrics::AISessionType::kText),
                                status);

  response_callback_count_++;

  if (status != ModelStreamingResponseStatus::kOngoing) {
    // When the status is not kOngoing, the promise should either be resolved
    // or rejected.
    if (status == ModelStreamingResponseStatus::kComplete) {
      resolver_->Resolve(response_);
    } else {
      resolver_->Reject(
          ConvertModelStreamingResponseErrorToDOMException(status));
    }
    // Record the per execution metrics and run the complete callback.
    base::UmaHistogramCounts1M(AIMetrics::GetAISessionResponseSizeMetricName(
                                   AIMetrics::AISessionType::kText),
                               int(response_.CharactersSizeInBytes()));
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseCallbackCountMetricName(
            AIMetrics::AISessionType::kText),
        response_callback_count_);
    Cleanup();
    return;
  }
  // When the status is kOngoing, update the response with the latest value.
  response_ = text;
}

void AITextSession::Responder::Cleanup() {
  resolver_ = nullptr;
  receiver_.reset();
  keep_alive_.Clear();
}

AITextSession::StreamingResponder::StreamingResponder(ScriptState* script_state)
    : UnderlyingSourceBase(script_state),
      script_state_(script_state),
      receiver_(this, ExecutionContext::From(script_state)) {}
AITextSession::StreamingResponder::~StreamingResponder() = default;

void AITextSession::StreamingResponder::Trace(Visitor* visitor) const {
  UnderlyingSourceBase::Trace(visitor);
  visitor->Trace(script_state_);
  visitor->Trace(receiver_);
}

mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
AITextSession::StreamingResponder::BindNewPipeAndPassRemote(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return receiver_.BindNewPipeAndPassRemote(task_runner);
}

ScriptPromiseUntyped AITextSession::StreamingResponder::Pull(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return ScriptPromiseUntyped::CastUndefined(script_state);
}

ScriptPromiseUntyped AITextSession::StreamingResponder::Cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  return ScriptPromiseUntyped::CastUndefined(script_state);
}

void AITextSession::StreamingResponder::OnResponse(
    ModelStreamingResponseStatus status,
    const WTF::String& text) {
  base::UmaHistogramEnumeration(AIMetrics::GetAISessionResponseStatusMetricName(
                                    AIMetrics::AISessionType::kText),
                                status);

  response_callback_count_++;

  if (status != ModelStreamingResponseStatus::kOngoing) {
    // When the status is not kOngoing, the controller of
    // ReadableStream should be closed.
    if (status == ModelStreamingResponseStatus::kComplete) {
      Controller()->Close();
    } else {
      Controller()->Error(
          ConvertModelStreamingResponseErrorToDOMException(status));
    }
    // Record the per execution metrics and run the complete callback.
    base::UmaHistogramCounts1M(AIMetrics::GetAISessionResponseSizeMetricName(
                                   AIMetrics::AISessionType::kText),
                               response_size_);
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseCallbackCountMetricName(
            AIMetrics::AISessionType::kText),
        response_callback_count_);
    return;
  }
  // When the status is kOngoing, update the response size and enqueue the
  // latest response.
  response_size_ = int(text.CharactersSizeInBytes());
  v8::HandleScope handle_scope(script_state_->GetIsolate());
  Controller()->Enqueue(V8String(script_state_->GetIsolate(), text));
}

AITextSession::AITextSession(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      text_session_remote_(context) {}

void AITextSession::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(text_session_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::AITextSession>
AITextSession::GetModelSessionReceiver() {
  return text_session_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

bool AITextSession::ThrowExceptionIfIsDestroyed(
    ExceptionState& exception_state) {
  if (is_destroyed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageSessionDestroyed);
  }
  return is_destroyed_;
}

ScriptPromise<IDLString> AITextSession::prompt(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionPrompt);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  Responder* responder = MakeGarbageCollected<Responder>(script_state);

  if (!ThrowExceptionIfIsDestroyed(exception_state)) {
    text_session_remote_->Prompt(
        input, responder->BindNewPipeAndPassRemote(task_runner_));
  }

  return responder->GetResolver()->Promise();
}

ReadableStream* AITextSession::promptStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionPromptStreaming);

  base::UmaHistogramCounts1M(AIMetrics::GetAISessionRequestSizeMetricName(
                                 AIMetrics::AISessionType::kText),
                             int(input.CharactersSizeInBytes()));

  StreamingResponder* streaming_responder =
      MakeGarbageCollected<StreamingResponder>(script_state);

  if (is_destroyed_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kExceptionMessageSessionDestroyed);
    return nullptr;
  }

  text_session_remote_->Prompt(
      input, streaming_responder->BindNewPipeAndPassRemote(task_runner_));

  // Set the high water mark to 1 so the backpressure will be applied on every
  // enqueue.
  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state, streaming_responder, 1);
}

ScriptPromise<AITextSession> AITextSession::clone(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<AITextSession>();
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionClone);

  ScriptPromiseResolver<AITextSession>* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<AITextSession>>(script_state);

  if (!ThrowExceptionIfIsDestroyed(exception_state)) {
    AITextSession* cloned_session = MakeGarbageCollected<AITextSession>(
        GetExecutionContext(), task_runner_);
    text_session_remote_->Fork(
        cloned_session->GetModelSessionReceiver(),
        WTF::BindOnce(
            [](ScriptPromiseResolver<AITextSession>* resolver,
               AITextSession* cloned_session, bool success) {
              if (success) {
                resolver->Resolve(cloned_session);
              } else {
                resolver->Reject(DOMException::Create(
                    kExceptionMessageUnableToCloneSession,
                    DOMException::GetErrorName(
                        DOMExceptionCode::kInvalidStateError)));
              }
            },
            WrapPersistent(resolver), WrapPersistent(cloned_session)));
  }

  return resolver->Promise();
}

void AITextSession::destroy(ScriptState* script_state,
                            ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return;
  }

  base::UmaHistogramEnumeration(
      AIMetrics::GetAIAPIUsageMetricName(AIMetrics::AISessionType::kText),
      AIMetrics::AIAPI::kSessionDestroy);

  if (!is_destroyed_) {
    is_destroyed_ = true;
    text_session_remote_->Destroy();
  }
}

}  // namespace blink
