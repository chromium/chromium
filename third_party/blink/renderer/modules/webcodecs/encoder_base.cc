// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
constexpr const char kCategory[] = "media";

base::AtomicSequenceNumber g_sequence_num_for_counters;
}  // namespace

// static
template <typename Traits>
const CodecTraceNames* EncoderBase<Traits>::GetTraceNames() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecTraceNames, trace_names,
                                  (Traits::GetName()));
  return &trace_names;
}

template <typename Traits>
EncoderBase<Traits>::EncoderBase(ScriptState* script_state,
                                 const InitType* init,
                                 ExceptionState& exception_state)
    : ActiveScriptWrappable<EncoderBase<Traits>>({}),
      ReclaimableCodec(ReclaimableCodec::CodecType::kEncoder,
                       ExecutionContext::From(script_state)),
      state_(V8CodecState::Enum::kUnconfigured),
      script_state_(script_state),
      trace_counter_id_(g_sequence_num_for_counters.GetNext()) {
  auto* context = ExecutionContext::From(script_state);
  callback_runner_ = context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  logger_ = std::make_unique<CodecLogger<media::EncoderStatus>>(
      GetExecutionContext(), callback_runner_);

  media::MediaLog* log = logger_->log();
  logger_->SendPlayerNameInformation(*context, Traits::GetName());
  log->SetProperty<media::MediaLogProperty::kFrameUrl>(
      GetExecutionContext()->Url().GetString().Ascii());

  output_callback_ = init->output();
  if (init->hasError())
    error_callback_ = init->error();
}

template <typename Traits>
EncoderBase<Traits>::~EncoderBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramSparse(
      String::Format("Blink.WebCodecs.%s.FinalStatus", Traits::GetName())
          .Ascii()
          .c_str(),
      static_cast<int>(logger_->status_code()));
}

template <typename Traits>
void EncoderBase<Traits>::configure(const ConfigType* config,
                                    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "configure", exception_state))
    return;

  InternalConfigType* parsed_config = ParseConfig(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return;
  }

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  if (active_config_ && state_.AsEnum() == V8CodecState::Enum::kConfigured &&
      CanReconfigure(*active_config_, *parsed_config)) {
    request->type = Request::Type::kReconfigure;
  } else {
    state_ = V8CodecState(V8CodecState::Enum::kConfigured);
    request->type = Request::Type::kConfigure;
  }
  request->config = parsed_config;
  EnqueueRequest(request);
}

template <typename Traits>
void EncoderBase<Traits>::encode(InputType* input,
                                 const EncodeOptionsType* opts,
                                 ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "encode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "encode", exception_state))
    return;

  DCHECK(active_config_);

  // This will fail if |input| is already closed.
  // Remove exceptions relating to cloning closed input.
  auto* internal_input = input->clone(IGNORE_EXCEPTION);

  if (!internal_input) {
    exception_state.ThrowTypeError("Cannot encode closed input.");
    return;
  }

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  request->type = Request::Type::kEncode;
  request->input = internal_input;
  request->encodeOpts = opts;
  ++requested_encodes_;
  EnqueueRequest(request);
}

template <typename Traits>
void EncoderBase<Traits>::close(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted due to close()"));
  output_callback_.Clear();
  error_callback_.Clear();
}

template <typename Traits>
ScriptPromise<IDLUndefined> EncoderBase<Traits>::flush(
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return EmptyPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return EmptyPromise();

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state_);
  request->reset_count = reset_count_;
  request->type = Request::Type::kFlush;
  EnqueueRequest(request);
  return request->resolver->Promise();
}

template <typename Traits>
void EncoderBase<Traits>::reset(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  TRACE_EVENT0(kCategory, GetTraceNames()->reset.c_str());

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);
  ResetInternal(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted due to reset()"));
}

template <typename Traits>
void EncoderBase<Traits>::ResetInternal(DOMException* ex) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reset_count_++;

  if (blocking_request_in_progress_ &&
      blocking_request_in_progress_->resolver) {
    blocking_request_in_progress_->resolver.Release()->Reject(ex);
  }

  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    if (pending_req->resolver) {
      pending_req->resolver.Release()->Reject(ex);
    }
    if (pending_req->input) {
      pending_req->input.Release()->close();
    }
  }
  if (requested_encodes_ > 0) {
    requested_encodes_ = 0;
    ScheduleDequeueEvent();
  }

  blocking_request_in_progress_ = nullptr;

  // Schedule deletion of |media_encoder_| for later.
  // ResetInternal() might be called by an error reporting callback called by
  // |media_encoder_|. If we delete it now, this thread might come back up
  // the call stack and continue executing code belonging to deleted
  // |media_encoder_|.
  callback_runner_->DeleteSoon(FROM_HERE, std::move(media_encoder_));

  // This codec isn't holding on to any resources, and doesn't need to be
  // reclaimed.
  ReleaseCodecPressure();
}

template <typename Traits>
void EncoderBase<Traits>::QueueHandleError(DOMException* ex) {
  callback_runner_->PostTask(
      FROM_HERE, WTF::BindOnce(&EncoderBase<Traits>::HandleError,
                               WrapWeakPersistent(this), WrapPersistent(ex)));
}

template <typename Traits>
void EncoderBase<Traits>::HandleError(DOMException* ex) {
  if (state_.AsEnum() == V8CodecState::Enum::kClosed)
    return;

  TRACE_EVENT0(kCategory, GetTraceNames()->handle_error.c_str());

  // Save a temp before we clear the callback.
  V8WebCodecsErrorCallback* error_callback = error_callback_.Get();

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal(ex);

  // Errors are permanent. Shut everything down.
  error_callback_.Clear();
  output_callback_.Clear();

  // Prevent further logging.
  logger_->Neuter();

  if (!script_state_->ContextIsValid() || !error_callback)
    return;

  ScriptState::Scope scope(script_state_);

  error_callback->InvokeAndReportException(nullptr, ex);
}

template <typename Traits>
void EncoderBase<Traits>::EnqueueRequest(Request* request) {
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void EncoderBase<Traits>::ProcessRequests() {
  while (!requests_.empty() && ReadyToProcessNextRequest()) {
    TraceQueueSizes();

    Request* request = requests_.TakeFirst();
    DCHECK(request);
    switch (request->type) {
      case Request::Type::kConfigure:
        ProcessConfigure(request);
        break;
      case Request::Type::kReconfigure:
        ProcessReconfigure(request);
        break;
      case Request::Type::kEncode:
        ProcessEncode(request);
        break;
      case Request::Type::kFlush:
        ProcessFlush(request);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  TraceQueueSizes();
}

template <typename Traits>
bool EncoderBase<Traits>::ReadyToProcessNextRequest() {
  return !blocking_request_in_progress_;
}

template <typename Traits>
void EncoderBase<Traits>::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](EncoderBase<Traits>* self, Request* req,
                          media::EncoderStatus status) {
    DCHECK(req);

    if (!req->resolver) {
      // Some error occurred and this was resolved earlier.
      return;
    }

    if (!self) {
      req->resolver.Release()->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Aborted due to close()"));
      req->EndTracing(/*aborted=*/true);
      return;
    }

    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (self->reset_count_ != req->reset_count) {
      req->resolver.Release()->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Aborted due to reset()"));
      req->EndTracing(/*aborted=*/true);
      return;
    }
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      self->HandleError(self->logger_->MakeEncodingError("Flushing error.",
                                                         std::move(status)));
      DCHECK(!req->resolver);
    }
    req->EndTracing();

    self->blocking_request_in_progress_ = nullptr;
    self->ProcessRequests();
  };

  request->StartTracing();

  blocking_request_in_progress_ = request;
  media_encoder_->Flush(ConvertToBaseOnceCallback(CrossThreadBindOnce(
      done_callback, MakeUnwrappingCrossThreadWeakHandle(this),
      MakeUnwrappingCrossThreadHandle(request))));
}

template <typename Traits>
void EncoderBase<Traits>::OnCodecReclaimed(DOMException* exception) {
  TRACE_EVENT0(kCategory, GetTraceNames()->reclaimed.c_str());
  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  HandleError(exception);
}

template <typename Traits>
void EncoderBase<Traits>::ContextDestroyed() {
  state_ = V8CodecState(V8CodecState::Enum::kClosed);
  ResetInternal(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kAbortError, "Aborted due to close()"));
  logger_->Neuter();
}

template <typename Traits>
bool EncoderBase<Traits>::HasPendingActivity() const {
  return blocking_request_in_progress_ || !requests_.empty();
}

template <typename Traits>
void EncoderBase<Traits>::TraceQueueSizes() const {
  TRACE_COUNTER_ID2(kCategory, GetTraceNames()->requests_counter.c_str(),
                    trace_counter_id_, "encodes", requested_encodes_, "other",
                    requests_.size() - requested_encodes_);
}

template <typename Traits>
void EncoderBase<Traits>::DispatchDequeueEvent(Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  probe::AsyncTask async_task(GetExecutionContext(),
                              event->async_task_context());
  dequeue_event_pending_ = false;
  DispatchEvent(*event);
}

template <typename Traits>
void EncoderBase<Traits>::ScheduleDequeueEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dequeue_event_pending_)
    return;
  dequeue_event_pending_ = true;

  Event* event = Event::Create(event_type_names::kDequeue);
  event->SetTarget(this);
  event->async_task_context()->Schedule(GetExecutionContext(), event->type());

  callback_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&EncoderBase<Traits>::DispatchDequeueEvent,
                    WrapWeakPersistent(this), WrapPersistent(event)));
}

template <typename Traits>
ExecutionContext* EncoderBase<Traits>::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

template <typename Traits>
void EncoderBase<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(active_config_);
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  visitor->Trace(blocking_request_in_progress_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ReclaimableCodec::Trace(visitor);
}

template <typename Traits>
void EncoderBase<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(input);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
  visitor->Trace(config);
}

template <typename Traits>
const char* EncoderBase<Traits>::Request::TraceNameFromType() {
  using RequestType = typename EncoderBase<Traits>::Request::Type;

  const CodecTraceNames* trace_names = EncoderBase<Traits>::GetTraceNames();

  switch (type) {
    case RequestType::kConfigure:
      return trace_names->configure.c_str();
    case RequestType::kEncode:
      return trace_names->encode.c_str();
    case RequestType::kFlush:
      return trace_names->flush.c_str();
    case RequestType::kReconfigure:
      return trace_names->reconfigure.c_str();
  }
  return "InvalidCodecTraceName";
}

template <typename Traits>
void EncoderBase<Traits>::Request::StartTracingVideoEncode(
    bool is_keyframe,
    base::TimeDelta timestamp) {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(kCategory, TraceNameFromType(), this,
                                    "key_frame", is_keyframe, "timestamp",
                                    timestamp);
}

template <typename Traits>
void EncoderBase<Traits>::Request::StartTracing() {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategory, TraceNameFromType(), this);
}

template <typename Traits>
void EncoderBase<Traits>::Request::EndTracing(bool aborted) {
#if DCHECK_IS_ON()
  DCHECK(is_tracing);
  is_tracing = false;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_END1(kCategory, TraceNameFromType(), this,
                                  "aborted", aborted);
}

template class EncoderBase<VideoEncoderTraits>;
template class EncoderBase<AudioEncoderTraits>;

}  // namespace blink
