// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/ai/exception_helpers.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a promise.
class Responder final : public GarbageCollected<Responder>,
                        public mojom::blink::ModelStreamingResponder,
                        public ContextLifecycleObserver {
 public:
  Responder(ScriptState* script_state,
            AbortSignal* signal,
            ScriptPromiseResolver<IDLString>* resolver,
            AIMetrics::AISessionType session_type,
            base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
                complete_callback)
      : script_state_(script_state),
        resolver_(resolver),
        receiver_(this, ExecutionContext::From(script_state)),
        abort_signal_(signal),
        session_type_(session_type),
        complete_callback_(std::move(complete_callback)) {
    SetContextLifecycleNotifier(ExecutionContext::From(script_state));
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(
          WTF::BindOnce(&Responder::OnAborted, WrapWeakPersistent(this)));
    }
  }
  ~Responder() override = default;
  Responder(const Responder&) = delete;
  Responder& operator=(const Responder&) = delete;

  void Trace(Visitor* visitor) const override {
    ContextLifecycleObserver::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(resolver_);
    visitor->Trace(receiver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  ScriptPromise<IDLString> GetPromise() { return resolver_->Promise(); }

  mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
  BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return receiver_.BindNewPipeAndPassRemote(task_runner);
  }

  // `mojom::blink::ModelStreamingResponder` implementation.
  void OnStreaming(const String& text) override {
    RecordResponseStatusMetrics(
        mojom::blink::ModelStreamingResponseStatus::kOngoing);
    response_callback_count_++;
    // Update the response with the latest value.
    response_ = text;
  }

  void OnCompletion(
      mojom::blink::ModelExecutionContextInfoPtr context_info) override {
    RecordResponseStatusMetrics(
        mojom::blink::ModelStreamingResponseStatus::kComplete);
    response_callback_count_++;

    resolver_->Resolve(response_);
    if (context_info && complete_callback_) {
      std::move(complete_callback_).Run(std::move(context_info));
    }
    RecordResponseMetrics();
    Cleanup();
  }

  void OnError(mojom::blink::ModelStreamingResponseStatus status) override {
    RecordResponseStatusMetrics(status);
    response_callback_count_++;
    resolver_->Reject(ConvertModelStreamingResponseErrorToDOMException(status));
    RecordResponseMetrics();
    Cleanup();
  }

  // ContextLifecycleObserver implementation.
  void ContextDestroyed() override { Cleanup(); }

 private:
  void OnAborted() {
    if (!resolver_) {
      return;
    }
    resolver_->Reject(abort_signal_->reason(script_state_));
    Cleanup();
  }

  void RecordResponseStatusMetrics(
      mojom::blink::ModelStreamingResponseStatus status) {
    base::UmaHistogramEnumeration(
        AIMetrics::GetAISessionResponseStatusMetricName(session_type_), status);
  }

  void RecordResponseMetrics() {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseSizeMetricName(session_type_),
        int(response_.CharactersSizeInBytes()));
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseCallbackCountMetricName(session_type_),
        response_callback_count_);
  }

  void Cleanup() {
    resolver_ = nullptr;
    receiver_.reset();
    keep_alive_.Clear();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

  Member<ScriptState> script_state_;
  Member<ScriptPromiseResolver<IDLString>> resolver_;
  String response_;
  int response_callback_count_ = 0;
  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder, Responder>
      receiver_;
  SelfKeepAlive<Responder> keep_alive_{this};
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  const AIMetrics::AISessionType session_type_;
  // The callback will be invoked once when the responder receive the first
  // `kComplete`.
  base::OnceCallback<void(
      mojom::blink::ModelExecutionContextInfoPtr context_info)>
      complete_callback_;
};

// Implementation of blink::mojom::blink::ModelStreamingResponder that
// handles the streaming output of the model execution, and returns the full
// result through a ReadableStream.
class StreamingResponder final
    : public UnderlyingSourceBase,
      public blink::mojom::blink::ModelStreamingResponder {
 public:
  StreamingResponder(
      ScriptState* script_state,
      AbortSignal* signal,
      AIMetrics::AISessionType session_type,
      base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
          complete_callback)
      : UnderlyingSourceBase(script_state),
        script_state_(script_state),
        receiver_(this, ExecutionContext::From(script_state)),
        abort_signal_(signal),
        session_type_(session_type),
        complete_callback_(std::move(complete_callback)) {
    if (abort_signal_) {
      CHECK(!abort_signal_->aborted());
      abort_handle_ = abort_signal_->AddAlgorithm(WTF::BindOnce(
          &StreamingResponder::OnAborted, WrapWeakPersistent(this)));
    }
  }
  ~StreamingResponder() override = default;

  StreamingResponder(const StreamingResponder&) = delete;
  StreamingResponder& operator=(const StreamingResponder&) = delete;

  void Trace(Visitor* visitor) const override {
    UnderlyingSourceBase::Trace(visitor);
    visitor->Trace(script_state_);
    visitor->Trace(receiver_);
    visitor->Trace(abort_signal_);
    visitor->Trace(abort_handle_);
  }

  mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
  BindNewPipeAndPassRemote(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return receiver_.BindNewPipeAndPassRemote(task_runner);
  }

  ReadableStream* CreateReadableStream() {
    // Set the high water mark to 1 so the backpressure will be applied on every
    // enqueue.
    return ReadableStream::CreateWithCountQueueingStrategy(script_state_, this,
                                                           1);
  }

  // `UnderlyingSourceBase` implementation.
  ScriptPromise<IDLUndefined> Pull(ScriptState* script_state,
                                   ExceptionState& exception_state) override {
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> Cancel(ScriptState* script_state,
                                     ScriptValue reason,
                                     ExceptionState& exception_state) override {
    return ToResolvedUndefinedPromise(script_state);
  }

  // `blink::mojom::blink::ModelStreamingResponder` implementation.
  void OnStreaming(

      const String& text) override {
    RecordResponseStatusMetrics(
        mojom::blink::ModelStreamingResponseStatus::kOngoing);
    // Update the response info and enqueue the latest response.
    response_callback_count_++;
    response_size_ = int(text.CharactersSizeInBytes());
    v8::HandleScope handle_scope(script_state_->GetIsolate());
    Controller()->Enqueue(V8String(script_state_->GetIsolate(), text));
  }

  void OnCompletion(
      mojom::blink::ModelExecutionContextInfoPtr context_info) override {
    RecordResponseStatusMetrics(
        mojom::blink::ModelStreamingResponseStatus::kComplete);
    response_callback_count_++;
    Controller()->Close();
    if (context_info && complete_callback_) {
      std::move(complete_callback_).Run(std::move(context_info));
    }
    RecordResponseMetrics();
    Cleanup();
    return;
  }

  void OnError(ModelStreamingResponseStatus status) override {
    RecordResponseStatusMetrics(status);
    response_callback_count_++;
    Controller()->Error(
        ConvertModelStreamingResponseErrorToDOMException(status));
    RecordResponseMetrics();
    Cleanup();
  }

 private:
  void OnAborted() {
    // TODO(crbug.com/374879795): fix the abort handling for streaming
    // responder.
    Controller()->Error(DOMException::Create(
        kExceptionMessageRequestAborted,
        DOMException::GetErrorName(DOMExceptionCode::kAbortError)));
    Cleanup();
  }

  void RecordResponseStatusMetrics(
      mojom::blink::ModelStreamingResponseStatus status) {
    base::UmaHistogramEnumeration(
        AIMetrics::GetAISessionResponseStatusMetricName(session_type_), status);
  }

  void RecordResponseMetrics() {
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseSizeMetricName(session_type_),
        response_size_);
    base::UmaHistogramCounts1M(
        AIMetrics::GetAISessionResponseCallbackCountMetricName(session_type_),
        response_callback_count_);
  }

  void Cleanup() {
    script_state_ = nullptr;
    receiver_.reset();
    if (abort_handle_) {
      abort_signal_->RemoveAlgorithm(abort_handle_);
      abort_handle_ = nullptr;
    }
  }

  int response_size_ = 0;
  int response_callback_count_ = 0;
  Member<ScriptState> script_state_;
  HeapMojoReceiver<blink::mojom::blink::ModelStreamingResponder,
                   StreamingResponder>
      receiver_;
  Member<AbortSignal> abort_signal_;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  const AIMetrics::AISessionType session_type_;
  // The callback will be invoked once when the responder receive the first
  // `kComplete`.
  base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
      complete_callback_;
};

}  // namespace

mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>
CreateModelExecutionResponder(
    ScriptState* script_state,
    AbortSignal* signal,
    ScriptPromiseResolver<IDLString>* resolver,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AIMetrics::AISessionType session_type,
    base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
        complete_callback) {
  Responder* responder = MakeGarbageCollected<Responder>(
      script_state, signal, resolver, session_type,
      std::move(complete_callback));
  return responder->BindNewPipeAndPassRemote(task_runner);
}

std::tuple<ReadableStream*,
           mojo::PendingRemote<blink::mojom::blink::ModelStreamingResponder>>
CreateModelExecutionStreamingResponder(
    ScriptState* script_state,
    AbortSignal* signal,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    AIMetrics::AISessionType session_type,
    base::OnceCallback<void(mojom::blink::ModelExecutionContextInfoPtr)>
        complete_callback) {
  StreamingResponder* streaming_responder =
      MakeGarbageCollected<StreamingResponder>(
          script_state, signal, session_type, std::move(complete_callback));
  return std::make_tuple(
      streaming_responder->CreateReadableStream(),
      streaming_responder->BindNewPipeAndPassRemote(task_runner));
}

}  // namespace blink
