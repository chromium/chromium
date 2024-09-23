// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/decoder_status.h"
#include "media/base/media_util.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
constexpr const char kCategory[] = "media";

base::AtomicSequenceNumber g_sequence_num_for_counters;
}  // namespace

// static
template <typename Traits>
const CodecTraceNames* DecoderTemplate<Traits>::GetTraceNames() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecTraceNames, trace_names,
                                  (Traits::GetName()));
  return &trace_names;
}

template <typename Traits>
DecoderTemplate<Traits>::DecoderTemplate(ScriptState* script_state,
                                         const InitType* init,
                                         ExceptionState& exception_state)
    : ActiveScriptWrappable<DecoderTemplate<Traits>>({}),
      ReclaimableCodec(ReclaimableCodec::CodecType::kDecoder,
                       ExecutionContext::From(script_state)),
      script_state_(script_state),
      state_(V8CodecState::Enum::kUnconfigured),
      trace_counter_id_(g_sequence_num_for_counters.GetNext()) {
  DVLOG(1) << __func__;
  DCHECK(init->hasOutput());
  DCHECK(init->hasError());

  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);

  main_thread_task_runner_ =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  logger_ = std::make_unique<CodecLogger<media::DecoderStatus>>(
      context, main_thread_task_runner_);

  logger_->log()->SetProperty<media::MediaLogProperty::kFrameUrl>(
      context->Url().GetString().Ascii());

  output_cb_ = init->output();
  error_cb_ = init->error();
}

template <typename Traits>
DecoderTemplate<Traits>::~DecoderTemplate() {
  DVLOG(1) << __func__;
  base::UmaHistogramSparse(
      String::Format("Blink.WebCodecs.%s.FinalStatus", Traits::GetName())
          .Ascii()
          .c_str(),
      static_cast<int>(logger_->status_code()));
}

template <typename Traits>
uint32_t DecoderTemplate<Traits>::decodeQueueSize() {
  return num_pending_decodes_;
}

template <typename Traits>
bool DecoderTemplate<Traits>::IsClosed() {
  return state_ == V8CodecState::Enum::kClosed;
}

template <typename Traits>
HardwarePreference DecoderTemplate<Traits>::GetHardwarePreference(
    const ConfigType&) {
  return HardwarePreference::kNoPreference;
}

template <typename Traits>
bool DecoderTemplate<Traits>::GetLowDelayPreference(const ConfigType&) {
  return false;
}

template <typename Traits>
void DecoderTemplate<Traits>::SetHardwarePreference(HardwarePreference) {}

template <typename Traits>
void DecoderTemplate<Traits>::configure(const ConfigType* config,
                                        ExceptionState& exception_state) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  String js_error_message;
  if (!IsValidConfig(*config, &js_error_message)) {
    exception_state.ThrowTypeError(js_error_message);
    return;
  }

  std::optional<MediaConfigType> media_config =
      MakeMediaConfig(*config, &js_error_message);

  // Audio/VideoDecoder don't yet support encryption.
  if (media_config && media_config->is_encrypted()) {
    js_error_message = "Encrypted content is not supported";
    media_config = std::nullopt;
  }

  MarkCodecActive();

  state_ = V8CodecState(V8CodecState::Enum::kConfigured);
  require_key_frame_ = true;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  if (media_config.has_value()) {
    request->media_config = std::make_unique<MediaConfigType>(*media_config);
  } else {
    request->js_error_message = js_error_message;
  }
  request->reset_generation = reset_generation_;
  request->hw_pref = GetHardwarePreference(*config);
  request->low_delay = GetLowDelayPreference(*config);
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::decode(const InputType* chunk,
                                     ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "decode", exception_state))
    return;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kDecode;
  request->reset_generation = reset_generation_;

  auto status_or_buffer = MakeInput(*chunk, require_key_frame_);
  if (status_or_buffer.has_value()) {
    request->decoder_buffer = std::move(status_or_buffer).value();
    require_key_frame_ = false;
  } else {
    request->status = std::move(status_or_buffer).error();
    if (request->status == media::DecoderStatus::Codes::kKeyFrameRequired) {
      exception_state.ThrowDOMException(DOMExceptionCode::kDataError,
                                        request->status.message().c_str());
      return;
    }
  }
  MarkCodecActive();

  requests_.push_back(request);
  ++num_pending_decodes_;
  ProcessRequests();
}

template <typename Traits>
ScriptPromise<IDLUndefined> DecoderTemplate<Traits>::flush(
    ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return EmptyPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return EmptyPromise();

  MarkCodecActive();

  require_key_frame_ = true;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state_);
  request->resolver = resolver;
  request->reset_generation = reset_generation_;
  requests_.push_back(request);
  ProcessRequests();
  return resolver->Promise();
}

template <typename Traits>
void DecoderTemplate<Traits>::reset(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  MarkCodecActive();

  ResetAlgorithm();
}

template <typename Traits>
void DecoderTemplate<Traits>::close(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  Shutdown();
}

template <typename Traits>
void DecoderTemplate<Traits>::ProcessRequests() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsClosed());
  while (!pending_request_ && !requests_.empty()) {
    Request* request = requests_.front();

    // Skip processing for requests that are canceled by a recent reset().
    if (MaybeAbortRequest(request)) {
      requests_.pop_front();
      continue;
    }

    TraceQueueSizes();

    DCHECK_EQ(request->reset_generation, reset_generation_);
    switch (request->type) {
      case Request::Type::kConfigure:
        if (!ProcessConfigureRequest(request))
          return;
        break;
      case Request::Type::kDecode:
        if (!ProcessDecodeRequest(request))
          return;
        break;
      case Request::Type::kFlush:
        if (!ProcessFlushRequest(request))
          return;
        break;
      case Request::Type::kReset:
        if (!ProcessResetRequest(request))
          return;
        break;
    }
    requests_.pop_front();
  }

  TraceQueueSizes();
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessConfigureRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);

  if (decoder() &&
      pending_decodes_.size() + 1 >
          static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder()))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // TODO(sandersd): Record this configuration as pending but don't apply it
  // until there is a decode request.
  pending_request_ = request;
  pending_request_->StartTracing();

  if (!request->media_config) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(&DecoderTemplate<Traits>::Shutdown,
                      WrapWeakPersistent(this),
                      WrapPersistent(MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kNotSupportedError,
                          request->js_error_message))));
    return false;
  }

  if (gpu_factories_.has_value()) {
    ContinueConfigureWithGpuFactories(request, gpu_factories_.value());
  } else if (Traits::kNeedsGpuFactories) {
    RetrieveGpuFactoriesWithKnownDecoderSupport(CrossThreadBindOnce(
        &DecoderTemplate<Traits>::ContinueConfigureWithGpuFactories,
        MakeUnwrappingCrossThreadHandle(this),
        MakeUnwrappingCrossThreadHandle(request)));
  } else {
    ContinueConfigureWithGpuFactories(request, nullptr);
  }
  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::ContinueConfigureWithGpuFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request);
  DCHECK_EQ(request->type, Request::Type::kConfigure);

  if (IsClosed()) {
    return;
  }

  gpu_factories_ = gpu_factories;

  if (MaybeAbortRequest(request)) {
    DCHECK_EQ(request, pending_request_);
    pending_request_.Release()->EndTracing();
    return;
  }

  if (!decoder()) {
    decoder_ = Traits::CreateDecoder(*ExecutionContext::From(script_state_),
                                     gpu_factories_.value(), logger_->log());
    if (!decoder()) {
      Shutdown(MakeOperationError(
          "Internal error: Could not create decoder.",
          media::DecoderStatus::Codes::kFailedToCreateDecoder));
      return;
    }

    SetHardwarePreference(request->hw_pref.value());
    // Processing continues in OnInitializeDone().
    // Note: OnInitializeDone() must not call ProcessRequests() reentrantly,
    // which can happen if InitializeDecoder() calls it synchronously.
    initializing_sync_ = true;
    Traits::InitializeDecoder(
        *decoder(), request->low_delay.value(), *request->media_config,
        WTF::BindOnce(&DecoderTemplate::OnInitializeDone,
                      WrapWeakPersistent(this)),
        WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this),
                           reset_generation_));
    initializing_sync_ = false;
    return;
  }

  // Processing continues in OnFlushDone().
  decoder()->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::BindOnce(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessDecodeRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kDecode);
  DCHECK_GT(num_pending_decodes_, 0u);

  if (!decoder()) {
    Shutdown(MakeEncodingError("Decoding error: no decoder found.",
                               media::DecoderStatus::Codes::kNotInitialized));
    return false;
  }

  if (pending_decodes_.size() + 1 >
      static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder()))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // The request may be invalid, if so report that now.
  if (!request->decoder_buffer || request->decoder_buffer->empty()) {
    if (request->status.is_ok()) {
      Shutdown(MakeEncodingError("Null or empty decoder buffer.",
                                 media::DecoderStatus::Codes::kFailed));
    } else {
      Shutdown(MakeEncodingError("Decoder error.", request->status));
    }

    return false;
  }

  // Submit for decoding.
  //
  // |pending_decode_id_| must not be 0 nor max because it HashMap reserves
  // these values for "emtpy" and "deleted".
  while (++pending_decode_id_ == 0 ||
         pending_decode_id_ == std::numeric_limits<uint32_t>::max() ||
         pending_decodes_.Contains(pending_decode_id_))
    ;
  pending_decodes_.Set(pending_decode_id_, request);
  --num_pending_decodes_;
  ScheduleDequeueEvent();

  if (media::MediaTraceIsEnabled()) {
    request->decode_trace = std::make_unique<media::ScopedDecodeTrace>(
        GetTraceNames()->decode.c_str(), *request->decoder_buffer);
  }

  decoder()->Decode(
      std::move(request->decoder_buffer),
      WTF::BindOnce(&DecoderTemplate::OnDecodeDone, WrapWeakPersistent(this),
                    pending_decode_id_));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessFlushRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kFlush);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);

  // flush() can only be called when state = "configured", in which case we
  // should always have a decoder.
  DCHECK(decoder());

  if (pending_decodes_.size() + 1 >
      static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder()))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnFlushDone().
  pending_request_ = request;
  pending_request_->StartTracing();

  decoder()->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::BindOnce(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessResetRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kReset);
  DCHECK_GT(reset_generation_, 0u);

  // Signal [[codec implementation]] to cease producing output for the previous
  // configuration.
  if (decoder()) {
    pending_request_ = request;
    pending_request_->StartTracing();

    // Processing continues in OnResetDone().
    decoder()->Reset(
        WTF::BindOnce(&DecoderTemplate::OnResetDone, WrapWeakPersistent(this)));
  }

  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::Shutdown(DOMException* exception) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsClosed())
    return;

  TRACE_EVENT1(kCategory, GetTraceNames()->shutdown.c_str(), "has_exception",
               !!exception);

  shutting_down_ = true;
  shutting_down_due_to_error_ = exception;

  // Abort pending work (otherwise it will never complete)
  if (pending_request_) {
    if (pending_request_->resolver) {
      pending_request_->resolver.Release()->Reject(
          exception
              ? exception
              : MakeGarbageCollected<DOMException>(
                    DOMExceptionCode::kAbortError, "Aborted due to close()"));
    }

    pending_request_.Release()->EndTracing(/*shutting_down=*/true);
  }

  // Abort all upcoming work.
  ResetAlgorithm();
  ReleaseCodecPressure();

  // Store the error callback so that we can use it after clearing state.
  V8WebCodecsErrorCallback* error_cb = error_cb_.Get();

  // Prevent any new public API calls during teardown.
  // This should make it safe to call into JS synchronously.
  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  // Prevent any late callbacks running.
  output_cb_.Release();
  error_cb_.Release();

  // Prevent any further logging from being reported.
  logger_->Neuter();

  // Clear decoding and JS-visible queue state. Use DeleteSoon() to avoid
  // deleting decoder_ when its callback (e.g. OnDecodeDone()) may be below us
  // in the stack.
  main_thread_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));

  if (pending_request_) {
    // This request was added as part of calling ResetAlgorithm above. However,
    // OnResetDone() will never execute, since we are now in a kClosed state,
    // and |decoder_| has been reset.
    DCHECK_EQ(pending_request_->type, Request::Type::kReset);
    pending_request_.Release()->EndTracing(/*shutting_down=*/true);
  }

  bool trace_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kCategory, &trace_enabled);
  if (trace_enabled) {
    for (auto& pending_decode : pending_decodes_)
      pending_decode.value->decode_trace.reset();
  }

  pending_decodes_.clear();
  num_pending_decodes_ = 0;
  ScheduleDequeueEvent();

  if (exception) {
    error_cb->InvokeAndReportException(nullptr, exception);
  }
}

template <typename Traits>
void DecoderTemplate<Traits>::ResetAlgorithm() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == V8CodecState::Enum::kUnconfigured)
    return;

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);

  // Increment reset counter to cause older pending requests to be rejected. See
  // ProcessRequests().
  reset_generation_++;

  // Any previous pending decode will be filtered by ProcessRequests(). Reset
  // the count immediately to report the correct value in decodeQueueSize().
  num_pending_decodes_ = 0;
  ScheduleDequeueEvent();

  // Since configure is always required after reset we can drop any cached
  // configuration.
  active_config_.reset();

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kReset;
  request->reset_generation = reset_generation_;
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnFlushDone(media::DecoderStatus status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK(pending_request_->type == Request::Type::kConfigure ||
         pending_request_->type == Request::Type::kFlush);

  if (!status.is_ok()) {
    Shutdown(MakeEncodingError("Error during flush.", status));
    return;
  }

  // If reset() has been called during the Flush(), we can skip reinitialization
  // since the client is required to do so manually.
  const bool is_flush = pending_request_->type == Request::Type::kFlush;
  if (is_flush && MaybeAbortRequest(pending_request_)) {
    pending_request_.Release()->EndTracing();
    ProcessRequests();
    return;
  }

  if (!is_flush)
    SetHardwarePreference(pending_request_->hw_pref.value());

  // Processing continues in OnInitializeDone().
  Traits::InitializeDecoder(
      *decoder(), is_flush ? low_delay_ : pending_request_->low_delay.value(),
      is_flush ? *active_config_ : *pending_request_->media_config,
      WTF::BindOnce(&DecoderTemplate::OnInitializeDone,
                    WrapWeakPersistent(this)),
      WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this),
                         reset_generation_));
}

template <typename Traits>
void DecoderTemplate<Traits>::OnInitializeDone(media::DecoderStatus status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK(pending_request_->type == Request::Type::kConfigure ||
         pending_request_->type == Request::Type::kFlush);

  const bool is_flush = pending_request_->type == Request::Type::kFlush;
  if (!status.is_ok()) {
    std::string error_message;
    if (is_flush) {
      error_message = "Error during initialize after flush.";
    } else if (status.code() ==
               media::DecoderStatus::Codes::kUnsupportedConfig) {
      error_message =
          "Unsupported configuration. Check isConfigSupported() prior to "
          "calling configure().";
    } else {
      error_message = "Decoder initialization error.";
    }
    Shutdown(MakeOperationError(error_message, status));
    return;
  }

  if (is_flush) {
    pending_request_->resolver.Release()->Resolve();
  } else {
    Traits::UpdateDecoderLog(*decoder(), *pending_request_->media_config,
                             logger_->log());

    if (decoder()->IsPlatformDecoder())
      ApplyCodecPressure();
    else
      ReleaseCodecPressure();

    low_delay_ = pending_request_->low_delay.value();
    active_config_ = std::move(pending_request_->media_config);
  }

  pending_request_.Release()->EndTracing();

  if (!initializing_sync_)
    ProcessRequests();
  else
    DCHECK(!is_flush);
}

template <typename Traits>
void DecoderTemplate<Traits>::OnDecodeDone(uint32_t id,
                                           media::DecoderStatus status) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsClosed())
    return;

  auto it = pending_decodes_.find(id);
  if (it != pending_decodes_.end()) {
    if (it->value->decode_trace)
      it->value->decode_trace->EndTrace(status);
    pending_decodes_.erase(it);
  }

  if (!status.is_ok() &&
      status.code() != media::DecoderStatus::Codes::kAborted) {
    Shutdown(MakeEncodingError("Decoding error.", std::move(status)));
    return;
  }

  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnResetDone() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kReset);

  pending_request_.Release()->EndTracing();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnOutput(uint32_t reset_generation,
                                       scoped_refptr<MediaOutputType> output) {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Suppress outputs belonging to an earlier reset_generation.
  if (reset_generation != reset_generation_)
    return;

  if (state_.AsEnum() != V8CodecState::Enum::kConfigured)
    return;

  auto* context = GetExecutionContext();
  if (!context)
    return;

  auto output_or_error = MakeOutput(std::move(output), context);

  if (!output_or_error.has_value()) {
    Shutdown(MakeEncodingError("Error creating output from decoded data",
                               std::move(output_or_error).error()));
    return;
  }

  OutputType* blink_output = std::move(output_or_error).value();

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     blink_output->timestamp());

  output_cb_->InvokeAndReportException(nullptr, blink_output);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());

  MarkCodecActive();
}

template <typename Traits>
void DecoderTemplate<Traits>::TraceQueueSizes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_COUNTER_ID2(kCategory, GetTraceNames()->requests_counter.c_str(),
                    trace_counter_id_, "decodes", num_pending_decodes_, "other",
                    requests_.size() - num_pending_decodes_);
}

template <typename Traits>
void DecoderTemplate<Traits>::DispatchDequeueEvent(Event* event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  probe::AsyncTask async_task(GetExecutionContext(),
                              event->async_task_context());
  dequeue_event_pending_ = false;
  DispatchEvent(*event);
}

template <typename Traits>
void DecoderTemplate<Traits>::ScheduleDequeueEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (dequeue_event_pending_)
    return;
  dequeue_event_pending_ = true;

  Event* event = Event::Create(event_type_names::kDequeue);
  event->SetTarget(this);
  event->async_task_context()->Schedule(GetExecutionContext(), event->type());

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&DecoderTemplate<Traits>::DispatchDequeueEvent,
                    WrapWeakPersistent(this), WrapPersistent(event)));
}

template <typename Traits>
ExecutionContext* DecoderTemplate<Traits>::GetExecutionContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

template <typename Traits>
void DecoderTemplate<Traits>::ContextDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deallocate resources and suppress late callbacks from media thread.
  Shutdown();
}

template <typename Traits>
void DecoderTemplate<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_cb_);
  visitor->Trace(error_cb_);
  visitor->Trace(requests_);
  visitor->Trace(pending_request_);
  visitor->Trace(pending_decodes_);
  visitor->Trace(shutting_down_due_to_error_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ReclaimableCodec::Trace(visitor);
}

template <typename Traits>
void DecoderTemplate<Traits>::OnCodecReclaimed(DOMException* exception) {
  TRACE_EVENT0(kCategory, GetTraceNames()->reclaimed.c_str());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_applying_codec_pressure());

  if (state_.AsEnum() == V8CodecState::Enum::kUnconfigured) {
    decoder_.reset();

    // This codec isn't holding on to any resources, and doesn't need to be
    // reclaimed.
    ReleaseCodecPressure();
    return;
  }

  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  Shutdown(exception);
}

template <typename Traits>
bool DecoderTemplate<Traits>::HasPendingActivity() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_request_ || !requests_.empty();
}

template <typename Traits>
bool DecoderTemplate<Traits>::MaybeAbortRequest(Request* request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request->reset_generation == reset_generation_) {
    return false;
  }

  if (request->resolver) {
    request->resolver.Release()->Reject(
        shutting_down_due_to_error_
            ? shutting_down_due_to_error_.Get()
            : MakeGarbageCollected<DOMException>(
                  DOMExceptionCode::kAbortError,
                  shutting_down_ ? "Aborted due to close()"
                                 : "Aborted due to reset()"));
  }
  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
}

template <typename Traits>
const char* DecoderTemplate<Traits>::Request::TraceNameFromType() {
  using RequestType = typename DecoderTemplate<Traits>::Request::Type;

  const CodecTraceNames* trace_names = DecoderTemplate<Traits>::GetTraceNames();

  switch (type) {
    case RequestType::kConfigure:
      return trace_names->configure.c_str();
    case RequestType::kDecode:
      return trace_names->decode.c_str();
    case RequestType::kFlush:
      return trace_names->flush.c_str();
    case RequestType::kReset:
      return trace_names->reset.c_str();
  }
  return "InvalidCodecTraceName";
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::StartTracing() {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategory, TraceNameFromType(), this);
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::EndTracing(bool shutting_down) {
#if DCHECK_IS_ON()
  DCHECK(is_tracing);
  is_tracing = false;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_END1(kCategory, TraceNameFromType(), this,
                                  "completed", !shutting_down);
}

template <typename Traits>
DOMException* DecoderTemplate<Traits>::MakeOperationError(
    std::string error_msg,
    media::DecoderStatus status) {
  if (!decoder_ || decoder_->IsPlatformDecoder()) {
    return logger_->MakeOperationError(std::move(error_msg), std::move(status));
  }
  return logger_->MakeSoftwareCodecOperationError(std::move(error_msg),
                                                  std::move(status));
}

template <typename Traits>
DOMException* DecoderTemplate<Traits>::MakeEncodingError(
    std::string error_msg,
    media::DecoderStatus status) {
  if (!decoder_ || decoder_->IsPlatformDecoder()) {
    return logger_->MakeEncodingError(std::move(error_msg), std::move(status));
  }
  return logger_->MakeSoftwareCodecEncodingError(std::move(error_msg),
                                                 std::move(status));
}

template class DecoderTemplate<AudioDecoderTraits>;
template class DecoderTemplate<VideoDecoderTraits>;

}  // namespace blink
