// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/model_execution/exception_helpers.h"
#include "third_party/blink/renderer/modules/model_execution/model_execution_metrics.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

using mojom::blink::ModelStreamingResponseStatus;

// TODO(crbug.com/1520700): update this to different DOMException once the
// list finalized.
const char* ConvertModelStreamingResponseErrorToString(
    ModelStreamingResponseStatus error) {
  switch (error) {
    case ModelStreamingResponseStatus::kErrorUnknown:
      return "Unknown error.";
    case ModelStreamingResponseStatus::kErrorInvalidRequest:
      return "The request was invalid.";
    case ModelStreamingResponseStatus::kErrorRequestThrottled:
      return "The request was throttled.";
    case ModelStreamingResponseStatus::kErrorPermissionDenied:
      return "User permission errors such as not signed-in or not allowed to "
             "execute model.";
    case ModelStreamingResponseStatus::kErrorGenericFailure:
      return "Other generic failures.";
    case ModelStreamingResponseStatus::kErrorRetryableError:
      return "Retryable error occurred in server.";
    case ModelStreamingResponseStatus::kErrorNonRetryableError:
      return "Non-retryable error occurred in server.";
    case ModelStreamingResponseStatus::kErrorUnsupportedLanguage:
      return "Unsupported.";
    case ModelStreamingResponseStatus::kErrorFiltered:
      return "Bad response.";
    case ModelStreamingResponseStatus::kErrorDisabled:
      return "Response was disabled.";
    case ModelStreamingResponseStatus::kErrorCancelled:
      return "The request was cancelled.";
    case ModelStreamingResponseStatus::kOngoing:
    case ModelStreamingResponseStatus::kComplete:
      NOTREACHED();
      return "";
  }
}

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a promise.
class ModelGenericSession::Responder final
    : public GarbageCollected<ModelGenericSession::Responder>,
      public blink::mojom::blink::ModelStreamingResponder {
 public:
  explicit Responder(ScriptState* script_state)
      : resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(
            script_state)),
        receiver_(this, ExecutionContext::From(script_state)) {}
  ~Responder() override = default;

  void Trace(Visitor* visitor) const {
    visitor->Trace(resolver_);
    visitor->Trace(receiver_);
  }

  ScriptPromise<IDLString> GetPromise() { return resolver_->Promise(); }

  mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
  BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return receiver_.BindNewPipeAndPassRemote(task_runner);
  }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnResponse(ModelStreamingResponseStatus status,
                  const WTF::String& text) override {
    base::UmaHistogramEnumeration(
        ModelExecutionMetrics::GetModelExecutionSessionResponseStatusMetricName(
            ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
        status);

    response_callback_count_++;

    if (status != ModelStreamingResponseStatus::kOngoing) {
      // When the status is not kOngoing, the promise should either be resolved
      // or rejected.
      if (status == ModelStreamingResponseStatus::kComplete) {
        resolver_->Resolve(response_);
      } else {
        resolver_->Reject(ConvertModelStreamingResponseErrorToString(status));
      }
      // Record the per execution metrics and run the complete callback.
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::GetModelExecutionSessionResponseSizeMetricName(
              ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          int(response_.CharactersSizeInBytes()));
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::
              GetModelExecutionSessionResponseCallbackCountMetricName(
                  ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_callback_count_);
      keep_alive_.Clear();
      return;
    }
    // When the status is kOngoing, update the response with the latest value.
    response_ = text;
  }

 private:
  Member<ScriptPromiseResolver<IDLString>> resolver_;
  WTF::String response_;
  int response_callback_count_;

  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder, Responder>
      receiver_;
  SelfKeepAlive<Responder> keep_alive_;
};

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a ReadableStream.
class ModelGenericSession::StreamingResponder final
    : public UnderlyingSourceBase,
      public blink::mojom::blink::ModelStreamingResponder {
 public:
  explicit StreamingResponder(ScriptState* script_state)
      : UnderlyingSourceBase(script_state),
        script_state_(script_state),
        receiver_(this, ExecutionContext::From(script_state)) {}
  ~StreamingResponder() override = default;

  void Trace(Visitor* visitor) const override {
    UnderlyingSourceBase::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(receiver_);
  }

  mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
  BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return receiver_.BindNewPipeAndPassRemote(task_runner);
  }

  // `UnderlyingSourceBase` implementation.
  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState& exception_state) override {
    return ScriptPromiseUntyped::CastUndefined(script_state);
  }

  ScriptPromiseUntyped Cancel(ScriptState* script_state,
                              ScriptValue reason,
                              ExceptionState& exception_state) override {
    return ScriptPromiseUntyped::CastUndefined(script_state);
  }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnResponse(ModelStreamingResponseStatus status,
                  const WTF::String& text) override {
    base::UmaHistogramEnumeration(
        ModelExecutionMetrics::GetModelExecutionSessionResponseStatusMetricName(
            ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
        status);

    response_callback_count_++;

    if (status != ModelStreamingResponseStatus::kOngoing) {
      // When the status is not kOngoing, the controller of
      // ReadableStream should be closed.
      if (status == ModelStreamingResponseStatus::kComplete) {
        Controller()->Close();
      } else {
        Controller()->Error(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotReadableError,
            ConvertModelStreamingResponseErrorToString(status)));
      }
      // Record the per execution metrics and run the complete callback.
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::GetModelExecutionSessionResponseSizeMetricName(
              ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_size_);
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::
              GetModelExecutionSessionResponseCallbackCountMetricName(
                  ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_callback_count_);
      keep_alive_.Clear();
      return;
    }
    // When the status is kOngoing, update the response size and enqueue the
    // latest response.
    response_size_ = int(text.CharactersSizeInBytes());
    v8::HandleScope handle_scope(script_state_->GetIsolate());
    Controller()->Enqueue(V8String(script_state_->GetIsolate(), text));
  }

 private:
  int response_size_;
  int response_callback_count_;
  Member<ScriptState> script_state_;
  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder,
                   StreamingResponder>
      receiver_;
  SelfKeepAlive<StreamingResponder> keep_alive_;
};

ModelGenericSession::ModelGenericSession(
    ExecutionContext* context,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ExecutionContextClient(context),
      task_runner_(task_runner),
      model_session_remote_(context) {}

void ModelGenericSession::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(model_session_remote_);
}

mojo::PendingReceiver<blink::mojom::blink::ModelGenericSession>
ModelGenericSession::GetModelSessionReceiver() {
  return model_session_remote_.BindNewPipeAndPassReceiver(task_runner_);
}

ScriptPromise<IDLString> ModelGenericSession::execute(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return ScriptPromise<IDLString>();
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kSessionExecute);

  base::UmaHistogramCounts1M(
      ModelExecutionMetrics::GetModelExecutionSessionRequestSizeMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      int(input.CharactersSizeInBytes()));

  Responder* responder = MakeGarbageCollected<Responder>(script_state);
  model_session_remote_->Execute(
      input, responder->BindNewPipeAndPassRemote(task_runner_));

  return responder->GetPromise();
}

ReadableStream* ModelGenericSession::executeStreaming(
    ScriptState* script_state,
    const WTF::String& input,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    ThrowInvalidContextException(exception_state);
    return nullptr;
  }

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kSessionExecuteStreaming);

  base::UmaHistogramCounts1M(
      ModelExecutionMetrics::GetModelExecutionSessionRequestSizeMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      int(input.CharactersSizeInBytes()));

  StreamingResponder* streaming_responder =
      MakeGarbageCollected<StreamingResponder>(script_state);

  model_session_remote_->Execute(
      input, streaming_responder->BindNewPipeAndPassRemote(task_runner_));

  // Set the high water mark to 1 so the backpressure will be applied on every
  // enqueue.
  return ReadableStream::CreateWithCountQueueingStrategy(
      script_state, streaming_responder, 1);
}

}  // namespace blink
