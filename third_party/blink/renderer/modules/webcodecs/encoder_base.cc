// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

template <typename Traits>
EncoderBase<Traits>::EncoderBase(ScriptState* script_state,
                                 const InitType* init,
                                 ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      state_(V8CodecState::Enum::kUnconfigured),
      script_state_(script_state) {
  // TODO(crbug.com/1151005): Use a real MediaLog in worker contexts too.
  if (IsMainThread()) {
    logger_ = std::make_unique<CodecLogger>(
        GetExecutionContext(), Thread::MainThread()->GetTaskRunner());
  } else {
    // This will create a logger backed by a NullMediaLog, which does nothing.
    logger_ = std::make_unique<CodecLogger>();
  }

  media::MediaLog* log = logger_->log();

  log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string(Traits::GetNameForDevTools()));
  log->SetProperty<media::MediaLogProperty::kFrameUrl>(
      GetExecutionContext()->Url().GetString().Ascii());

  output_callback_ = init->output();
  if (init->hasError())
    error_callback_ = init->error();
}

template <typename Traits>
EncoderBase<Traits>::~EncoderBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  if (!VerifyCodecSupport(parsed_config, exception_state)) {
    DCHECK(exception_state.HadException());
    return;
  }

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  if (media_encoder_ && active_config_ &&
      state_.AsEnum() == V8CodecState::Enum::kConfigured &&
      CanReconfigure(*active_config_, *parsed_config)) {
    request->type = Request::Type::kReconfigure;
  } else {
    state_ = V8CodecState(V8CodecState::Enum::kConfigured);
    request->type = Request::Type::kConfigure;
  }
  active_config_ = parsed_config;
  EnqueueRequest(request);
}

template <typename Traits>
void EncoderBase<Traits>::encode(FrameType* frame,
                                 const EncodeOptionsType* opts,
                                 ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "encode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "encode", exception_state))
    return;

  DCHECK(active_config_);
  auto* context = GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is destroyed.");
    return;
  }

  // This will fail if |frame| is already closed.
  auto* internal_frame = CloneFrame(frame, context);

  if (!internal_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Cannot encode closed frame.");
    return;
  }

  // At this point, we have "consumed" the frame, and will close the clone in
  // ProcessEncode().
  frame->close();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  request->type = Request::Type::kEncode;
  request->frame = internal_frame;
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

  ResetInternal();
  media_encoder_.reset();
  output_callback_.Clear();
  error_callback_.Clear();
}

template <typename Traits>
ScriptPromise EncoderBase<Traits>::flush(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return ScriptPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return ScriptPromise();

  Request* request = MakeGarbageCollected<Request>();
  request->resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
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

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);
  ResetInternal();
  media_encoder_.reset();
}

template <typename Traits>
void EncoderBase<Traits>::ResetInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reset_count_++;
  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    if (pending_req->resolver)
      pending_req->resolver.Release()->Resolve();
    if (pending_req->frame)
      pending_req->frame.Release()->close();
  }
  stall_request_processing_ = false;
}

template <typename Traits>
void EncoderBase<Traits>::HandleError(DOMException* ex) {
  if (state_.AsEnum() == V8CodecState::Enum::kClosed)
    return;

  // Save a temp before we clear the callback.
  V8WebCodecsErrorCallback* error_callback = error_callback_.Get();

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal();

  // Errors are permanent. Shut everything down.
  error_callback_.Clear();
  media_encoder_.reset();
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
  while (!requests_.empty() && !stall_request_processing_) {
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
        NOTREACHED();
    }
  }
}

template <typename Traits>
void EncoderBase<Traits>::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](EncoderBase<Traits>* self, Request* req,
                          media::Status status) {
    if (!self)
      return;
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    DCHECK(req);
    DCHECK(req->resolver);
    if (self->reset_count_ != req->reset_count) {
      req->resolver.Release()->Reject();
      return;
    }
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      self->HandleError(
          self->logger_->MakeException("Flushing error.", status));
      req->resolver.Release()->Reject();
    }
    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  stall_request_processing_ = true;
  media_encoder_->Flush(ConvertToBaseOnceCallback(
      CrossThreadBindOnce(done_callback, WrapCrossThreadWeakPersistent(this),
                          WrapCrossThreadPersistent(request))));
}

template <typename Traits>
void EncoderBase<Traits>::ContextDestroyed() {
  state_ = V8CodecState(V8CodecState::Enum::kClosed);
  logger_->Neuter();
  media_encoder_.reset();
}

template <typename Traits>
bool EncoderBase<Traits>::HasPendingActivity() const {
  return stall_request_processing_ || !requests_.empty();
}

template <typename Traits>
void EncoderBase<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(active_config_);
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

template <typename Traits>
void EncoderBase<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(frame);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}

template class EncoderBase<VideoEncoderTraits>;
template class EncoderBase<AudioEncoderTraits>;

}  // namespace blink
