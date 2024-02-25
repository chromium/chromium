// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/model_generic_session.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/model_execution/model_session.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/model_execution/model_execution_metrics.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// TODO(crbug.com/1520700): update this to different DOMException once the list
// finalized.
const char* ConvertModelStreamingResponseErrorToString(
    mojom::blink::ModelStreamingResponseStatus error) {
  switch (error) {
    case mojom::blink::ModelStreamingResponseStatus::kErrorUnknown:
      return "Unknown error.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorInvalidRequest:
      return "The request was invalid.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorRequestThrottled:
      return "The request was throttled.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorPermissionDenied:
      return "User permission errors such as not signed-in or not allowed to "
             "execute model.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorGenericFailure:
      return "Other generic failures.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorRetryableError:
      return "Retryable error occurred in server.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorNonRetryableError:
      return "Non-retryable error occurred in server.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorUnsupportedLanguage:
      return "Unsupported.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorFiltered:
      return "Bad response.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorDisabled:
      return "Response was disabled.";
    case mojom::blink::ModelStreamingResponseStatus::kErrorCancelled:
      return "The request was cancelled.";
    case mojom::blink::ModelStreamingResponseStatus::kOngoing:
    case mojom::blink::ModelStreamingResponseStatus::kComplete:
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
      : resolver_(MakeGarbageCollected<ScriptPromiseResolver>(script_state)) {}
  ~Responder() override = default;

  void Trace(Visitor* visitor) const { visitor->Trace(resolver_); }

  ScriptPromise GetPromise() { return resolver_->Promise(); }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnResponse(mojom::blink::ModelStreamingResponseStatus status,
                  const WTF::String& text) override {
    base::UmaHistogramEnumeration(
        ModelExecutionMetrics::GetModelExecutionSessionResponseStatusMetricName(
            ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
        status);

    response_callback_count_++;
    auto record_response_metric = [&]() {
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::GetModelExecutionSessionResponseSizeMetricName(
              ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          int(response_.CharactersSizeInBytes()));
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::
              GetModelExecutionSessionResponseCallbackCountMetricName(
                  ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_callback_count_);
    };
    switch (status) {
      case mojom::blink::ModelStreamingResponseStatus::kOngoing:
        response_ = text;
        break;
      case mojom::blink::ModelStreamingResponseStatus::kComplete:
        record_response_metric();
        resolver_->Resolve(response_);
        break;
      default:
        record_response_metric();
        resolver_->Reject(ConvertModelStreamingResponseErrorToString(status));
    }
  }

 private:
  Member<ScriptPromiseResolver> resolver_;
  WTF::String response_;
  int response_callback_count_;
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
    base::UmaHistogramEnumeration(
        ModelExecutionMetrics::GetModelExecutionSessionResponseStatusMetricName(
            ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
        status);

    response_callback_count_++;
    auto record_response_metric = [&]() {
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::GetModelExecutionSessionResponseSizeMetricName(
              ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_size_);
      base::UmaHistogramCounts1M(
          ModelExecutionMetrics::
              GetModelExecutionSessionResponseCallbackCountMetricName(
                  ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
          response_callback_count_);
    };
    switch (status) {
      case mojom::blink::ModelStreamingResponseStatus::kOngoing: {
        response_size_ = int(text.CharactersSizeInBytes());
        v8::HandleScope handle_scope(script_state_->GetIsolate());
        Controller()->Enqueue(V8String(script_state_->GetIsolate(), text));
        break;
      }
      case mojom::blink::ModelStreamingResponseStatus::kComplete:
        record_response_metric();
        Controller()->Close();
        break;
      default:
        // TODO(crbug.com/1520700): raise the proper exception based on the spec
        // after the prototype phase.
        record_response_metric();
        Controller()->Error(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kNotReadableError,
            ConvertModelStreamingResponseErrorToString(status)));
    }
  }

 private:
  int response_size_;
  int response_callback_count_;
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

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kSessionExecute);

  base::UmaHistogramCounts1M(
      ModelExecutionMetrics::GetModelExecutionSessionRequestSizeMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      int(input.CharactersSizeInBytes()));

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

  base::UmaHistogramEnumeration(
      ModelExecutionMetrics::GetModelExecutionAPIUsageMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      ModelExecutionMetrics::ModelExecutionAPI::kSessionExecuteStreaming);

  base::UmaHistogramCounts1M(
      ModelExecutionMetrics::GetModelExecutionSessionRequestSizeMetricName(
          ModelExecutionMetrics::ModelExecutionSessionType::kGeneric),
      int(input.CharactersSizeInBytes()));

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
