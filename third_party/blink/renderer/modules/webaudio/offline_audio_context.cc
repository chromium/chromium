/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"

#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

namespace {

OfflineAudioContext* CreateOfflineAudioContext(
    ExecutionContext* context,
    unsigned number_of_channels,
    std::optional<unsigned> number_of_frames,
    float sample_rate,
    uint32_t render_quantum_frames,
    ExceptionState& exception_state) {
  // FIXME: add support for workers.
  auto* window = DynamicTo<LocalDOMWindow>(context);
  if (!window) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Workers are not supported.");
    return nullptr;
  }

  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot create OfflineAudioContext on a detached context.");
    return nullptr;
  }

  if (number_of_frames.has_value() && !number_of_frames.value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound<unsigned>(
            "number of frames", number_of_frames.value(), 1));
    return nullptr;
  }

  if (number_of_channels == 0 ||
      number_of_channels > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "number of channels", number_of_channels, 1,
            ExceptionMessages::kInclusiveBound,
            BaseAudioContext::MaxNumberOfChannels(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (!audio_utilities::IsValidAudioBufferSampleRate(sample_rate)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "sampleRate", sample_rate,
            audio_utilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (!audio_utilities::IsValidRenderQuantumSize(render_quantum_frames,
                                                 sample_rate)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "renderSizeHint", render_quantum_frames,
            audio_utilities::MinRenderQuantumSize(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxRenderQuantumSize(sample_rate),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("WebAudio.OfflineAudioContext.CreateTime");
  OfflineAudioContext* audio_context =
      MakeGarbageCollected<OfflineAudioContext>(
          window, number_of_channels, number_of_frames, sample_rate,
          exception_state, render_quantum_frames);

  if (audio_context->HasAllocationFailed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The audio context could not be created due to memory limitations.");
    return nullptr;
  }

  audio_context->UpdateStateIfNeeded();

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::OfflineAudioContext()\n",
          audio_context);
#endif
  return audio_context;
}

}  // namespace

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    unsigned number_of_channels,
    unsigned number_of_frames,
    float sample_rate,
    ExceptionState& exception_state) {
  return CreateOfflineAudioContext(
      context, number_of_channels, number_of_frames, sample_rate,
      /*render_quantum_frames=*/128, exception_state);
}

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    const OfflineAudioContextOptions* options,
    ExceptionState& exception_state) {
  if (!options->length().has_value() &&
      !RuntimeEnabledFeatures::OfflineAudioContextIncrementalRenderingEnabled(
          context)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::FailedToGet("length", "OfflineAudioContextOptions",
                                       "Required member is undefined."));
    return nullptr;
  }

  uint32_t render_quantum_frames = 128;
  if (RuntimeEnabledFeatures::WebAudioConfigurableRenderQuantumEnabled(
          context) &&
      options->hasRenderSizeHint()) {
    UseCounter::Count(context, WebFeature::kWebAudioRenderSizeHint);
    if (options->renderSizeHint()->IsUnsignedLong()) {
      render_quantum_frames = options->renderSizeHint()->GetAsUnsignedLong();
    }
  }
  return CreateOfflineAudioContext(context, options->numberOfChannels(),
                                   options->length(), options->sampleRate(),
                                   render_quantum_frames, exception_state);
}

OfflineAudioContext::OfflineAudioContext(
    LocalDOMWindow* window,
    unsigned number_of_channels,
    std::optional<uint32_t> number_of_frames,
    float sample_rate,
    ExceptionState& exception_state,
    uint32_t render_quantum_frames)
    : BaseAudioContext(window,
                       ContextType::kOfflineContext,
                       render_quantum_frames),
      total_render_frames_(number_of_frames) {
  destination_node_ = OfflineAudioDestinationNode::Create(
      this, number_of_channels, sample_rate);
  Initialize();
}

OfflineAudioContext::~OfflineAudioContext() {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::~OfflineAudioContext()\n",
          this);
#endif
}

void OfflineAudioContext::Trace(Visitor* visitor) const {
  visitor->Trace(start_rendering_resolvers_);
  visitor->Trace(pending_render_targets_);
  visitor->Trace(scheduled_suspends_);
  BaseAudioContext::Trace(visitor);
}

OfflineAudioDestinationNode* OfflineAudioContext::destinationNode() const {
  return static_cast<OfflineAudioDestinationNode*>(
      BaseAudioContext::destinationNode());
}

std::optional<uint32_t> OfflineAudioContext::CalculateEffectiveChunkSize(
    std::optional<uint32_t> chunk_size) const {
  const uint32_t render_quantum_frames =
      GetDeferredTaskHandler().RenderQuantumFrames();

  uint64_t buffer_size;
  if (total_render_frames_.has_value()) {
    CHECK_LT(committed_frames_, total_render_frames_.value());
    uint32_t remaining = total_render_frames_.value() - committed_frames_;
    if (!chunk_size.has_value()) {
      // A finite render without a chunk size renders all remaining frames.
      return remaining;
    }
    // A finite render with a chunk size renders `chunk_size` frames.
    buffer_size = chunk_size.value();
  } else {
    // An indefinite render will render `chunk_size` frames if specified,
    // otherwise defaults to one render quantum.
    buffer_size = chunk_size.value_or(render_quantum_frames);
  }

  // Requested chunks are rounded up to a render quantum boundary.
  uint64_t remainder = buffer_size % render_quantum_frames;
  if (remainder != 0) {
    buffer_size += render_quantum_frames - remainder;
  }

  if (total_render_frames_.has_value()) {
    // The final finite chunk is trimmed to the exact remaining frame count.
    buffer_size = std::min<uint64_t>(
        buffer_size, total_render_frames_.value() - committed_frames_);
  } else if (!base::IsValueInRangeForNumericType<uint32_t>(buffer_size)) {
    return std::nullopt;
  }

  return base::checked_cast<uint32_t>(buffer_size);
}

ScriptPromise<AudioBuffer> OfflineAudioContext::startOfflineRendering(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // OfflineAudioContext might have been stopped by its execution context.
  // See: crbug.com/435867
  if (IsContextCleared() ||
      ContextState() == V8AudioContextState::Enum::kClosed) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call startRendering on an OfflineAudioContext in a stopped "
        "state.");
    return EmptyPromise();
  }

  if (!RuntimeEnabledFeatures::OfflineAudioContextIncrementalRenderingEnabled(
          GetExecutionContext())) {
    // If the context is not in the suspended state (i.e. running), reject the
    // promise.
    if (ContextState() != V8AudioContextState::Enum::kSuspended) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          StrCat({"cannot startRendering when an OfflineAudioContext is ",
                  state().AsStringView()}));
      return EmptyPromise();
    }

    // Can't call startRendering more than once. Return a rejected promise now.
    if (is_rendering_started_) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "cannot call startRendering more than once");
      return EmptyPromise();
    }

    DCHECK(!is_rendering_started_);
  }

  return startOfflineRendering(script_state, std::nullopt, exception_state);
}

ScriptPromise<AudioBuffer> OfflineAudioContext::startOfflineRendering(
    ScriptState* script_state,
    std::optional<uint32_t> chunk_size,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // OfflineAudioContext might have been stopped by its execution context.
  // See: crbug.com/435867
  if (IsContextCleared() ||
      ContextState() == V8AudioContextState::Enum::kClosed) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call startRendering on an OfflineAudioContext in a stopped "
        "state.");
    return EmptyPromise();
  }

  if (total_render_frames_.has_value() &&
      committed_frames_ >= total_render_frames_.value()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call startRendering on an OfflineAudioContext that has "
        "already rendered all frames.");
    return EmptyPromise();
  }

  if (chunk_size.has_value() && chunk_size.value() == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The requested chunk size cannot be zero.");
    return EmptyPromise();
  }

  // Allocate the AudioBuffer to hold the rendered result.
  std::optional<uint32_t> effective_chunk_size =
      CalculateEffectiveChunkSize(chunk_size);
  if (!effective_chunk_size.has_value()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The requested chunk size is too large.");
    return EmptyPromise();
  }

  uint32_t buffer_size = effective_chunk_size.value();
  float sample_rate = DestinationHandler().SampleRate();
  unsigned number_of_channels = DestinationHandler().NumberOfChannels();

  AudioBuffer* render_target = AudioBuffer::CreateUninitialized(
      number_of_channels, buffer_size, sample_rate);

  if (!render_target) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        StrCat({"startRendering failed to create AudioBuffer(",
                String::Number(number_of_channels), ", ",
                String::Number(buffer_size), ", ", String::Number(sample_rate),
                ")"}));
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<AudioBuffer>>(
      script_state, exception_state.GetContext());
  resolver->SuppressDetachCheck();

  if (total_render_frames_.has_value()) {
    committed_frames_ += buffer_size;
  }
  start_rendering_resolvers_.push_back(resolver);
  pending_render_targets_.push_back(render_target);

  if (pending_render_targets_.size() == 1) {
    DestinationHandler().EnsureOfflineRenderThreadInitialized();
    is_rendering_started_ = true;
    SetContextState(V8AudioContextState::Enum::kRunning);
    StartNextRender();
  }

  return resolver->Promise();
}

void OfflineAudioContext::StartNextRender() {
  DCHECK(IsMainThread());
  DCHECK(!pending_render_targets_.empty());

  AudioBuffer* render_target = pending_render_targets_.front().Get();

  destinationNode()
      ->SetDestinationBuffer(render_target);
  DestinationHandler().SetSharedRenderTarget(render_target);
  DestinationHandler().StartRendering();
}

ScriptPromise<IDLUndefined> OfflineAudioContext::suspendContext(
    ScriptState* script_state,
    double when,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // If the rendering is finished, reject the promise.
  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "the rendering is already finished");
    return EmptyPromise();
  }

  // The specified suspend time is negative; reject the promise.
  if (when < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        StrCat({"negative suspend time (", String::Number(when),
                ") is not allowed"}));
    return EmptyPromise();
  }

  // The suspend time should be earlier than the total render frame. Skip this
  // check for an indefinite-length context.
  if (total_render_frames_.has_value()) {
    double total_render_duration = total_render_frames_.value() / sampleRate();
    if (total_render_duration <= when) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          StrCat({"cannot schedule a suspend at ",
                  String::NumberToStringEcmaScript(when),
                  " seconds because it is greater than or equal to the "
                  "total render duration of ",
                  String::Number(total_render_frames_.value()), " frames (",
                  String::NumberToStringEcmaScript(total_render_duration),
                  " seconds)"}));
      return EmptyPromise();
    }
  }

  // Find the sample frame and round up to the nearest render quantum
  // boundary.
  size_t frame = when * sampleRate();
  frame = audio_utilities::RoundUpToMultiple(
      frame, GetDeferredTaskHandler().RenderQuantumFrames());

  // The specified suspend time is in the past; reject the promise.
  if (frame < CurrentSampleFrame()) {
    size_t current_frame_clamped = CurrentSampleFrame();
    double current_time_clamped = currentTime();
    if (total_render_frames_.has_value()) {
      current_frame_clamped =
          std::min(current_frame_clamped,
                   static_cast<size_t>(total_render_frames_.value()));
      current_time_clamped =
          std::min(current_time_clamped, total_render_frames_.value() /
                                             static_cast<double>(sampleRate()));
    }
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        StrCat({"suspend(", String::Number(when),
                ") failed to suspend at frame ", String::Number(frame),
                " because it is earlier than the current frame of ",
                String::Number(current_frame_clamped), " (",
                String::Number(current_time_clamped), " seconds)"}));
    return EmptyPromise();
  }

  ScriptPromise<IDLUndefined> promise;

  {
    // Wait until the suspend map is available for the insertion. Here we should
    // use GraphAutoLocker because it locks the graph from the main thread.
    DeferredTaskHandler::GraphAutoLocker locker(GetDeferredTaskHandler());

    // If there is a duplicate suspension at the same quantized frame,
    // reject the promise.
    if (scheduled_suspends_.Contains(frame)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          StrCat({"cannot schedule more than one suspend at frame ",
                  String::Number(frame), " (", String::Number(when),
                  " seconds)"}));
      return EmptyPromise();
    }

    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        script_state, exception_state.GetContext());

    // When an OfflineAudioContext that has not started rendering is garbage
    // collected, it may be collected in the same cycle as its pending
    // suspend resolvers. In some GC configurations, the resolver's
    // pre-finalizer may run before the context's, triggering a DCHECK that
    // the resolver was not detached. Since OfflineAudioContext explicitly
    // detaches these resolvers in its own Dispose/DetachPendingResolvers
    // paths, it is safe to suppress this check.
    resolver->SuppressDetachCheck();

    promise = resolver->Promise();

    scheduled_suspends_.insert(frame, resolver);
  }

  {
    base::AutoLock suspend_frames_locker(suspend_frames_lock_);
    scheduled_suspend_frames_.insert(frame);
  }

  return promise;
}

ScriptPromise<IDLUndefined> OfflineAudioContext::resumeContext(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // If the rendering has not started, reject the promise.
  if (!is_rendering_started_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot resume an offline context that has not started");
    return EmptyPromise();
  }

  // If the context is in a closed state or it really is closed (cleared),
  // reject the promise.
  if (IsContextCleared() ||
      ContextState() == V8AudioContextState::Enum::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "cannot resume a closed offline context");
    return EmptyPromise();
  }

  // If the context is already running, resolve the promise without altering
  // the current state or starting the rendering loop.
  if (ContextState() == V8AudioContextState::Enum::kRunning) {
    return ToResolvedUndefinedPromise(script_state);
  }

  DCHECK_EQ(ContextState(), V8AudioContextState::Enum::kSuspended);

  // If the context is suspended, resume rendering by setting the state to
  // "Running". and calling startRendering(). Note that resuming is possible
  // only after the rendering started.
  SetContextState(V8AudioContextState::Enum::kRunning);
  DestinationHandler().StartRendering();

  // Resolve the promise immediately.
  return ToResolvedUndefinedPromise(script_state);
}

void OfflineAudioContext::OnChunkRendered() {
  DCHECK(IsMainThread());
  CHECK(GetExecutionContext());
  DCHECK(!start_rendering_resolvers_.empty());
  DCHECK(!pending_render_targets_.empty());

  Member<ScriptPromiseResolver<AudioBuffer>> resolver =
      start_rendering_resolvers_.TakeFirst();
  Member<AudioBuffer> rendered_buffer = pending_render_targets_.TakeFirst();

  CHECK(rendered_buffer);
  resolver->Resolve(rendered_buffer.Get());

  if (IsRenderComplete()) {
    CloseInternal();
    FireCompletionEvent(rendered_buffer.Get());
  } else if (!pending_render_targets_.empty()) {
    StartNextRender();
  }
}

bool OfflineAudioContext::IsRenderComplete() const {
  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return true;
  }

  if (!total_render_frames_.has_value()) {
    return false;
  }

  return committed_frames_ >= total_render_frames_.value() &&
         pending_render_targets_.empty();
}

void OfflineAudioContext::CloseInternal() {
  DCHECK(IsMainThread());

  // Context is finished, so remove any tail processing nodes; there's nowhere
  // for the output to go.
  GetDeferredTaskHandler().FinishTailProcessing();

  // We set the state to closed here so that the oncomplete event handler sees
  // that the context has been closed.
  SetContextState(V8AudioContextState::Enum::kClosed);
  DestinationHandler().StopRenderThread();
  is_rendering_started_ = false;
}

void OfflineAudioContext::FireCompletionEvent(AudioBuffer* rendered_buffer) {
  DCHECK(IsMainThread());

  // Avoid firing the event if the document has already gone away.
  if (GetExecutionContext()) {
    DCHECK(rendered_buffer);
    if (!rendered_buffer) {
      return;
    }

    // Call the offline rendering completion event listener.
    DispatchEvent(*OfflineAudioCompletionEvent::Create(rendered_buffer));
  }
}

bool OfflineAudioContext::HandlePreRenderTasks(
    uint32_t frames_to_process,
    const AudioIOPosition* output_position,
    const AudioCallbackMetric* metric,
    base::TimeDelta playout_delay,
    const media::AudioGlitchInfo& glitch_info) {
  // TODO(hongchan): passing `nullptr` as an argument is not a good
  // pattern. Consider rewriting this method/interface.
  DCHECK_EQ(output_position, nullptr);
  DCHECK_EQ(metric, nullptr);
  DCHECK_EQ(playout_delay, base::TimeDelta());
  DCHECK_EQ(glitch_info, media::AudioGlitchInfo());

  DCHECK(IsAudioThread());

  {
    // OfflineGraphAutoLocker here locks the audio graph for this scope.
    DeferredTaskHandler::GraphAutoLocker locker(GetDeferredTaskHandler());
    listener()->Handler().UpdateState();
    GetDeferredTaskHandler().HandleDeferredTasks();
    HandleStoppableSourceNodes();
  }

  return ShouldSuspend();
}

void OfflineAudioContext::HandlePostRenderTasks() {
  DCHECK(IsAudioThread());

  // OfflineGraphAutoLocker here locks the audio graph for the same reason
  // above in `HandlePreRenderTasks()`.
  {
    DeferredTaskHandler::GraphAutoLocker locker(GetDeferredTaskHandler());

    GetDeferredTaskHandler().BreakConnections();
    GetDeferredTaskHandler().HandleDeferredTasks();
    GetDeferredTaskHandler().RequestToDeleteHandlersOnMainThread();
  }
}

OfflineAudioDestinationHandler& OfflineAudioContext::DestinationHandler() {
  return destinationNode()->GetAudioDestinationHandler();
}

void OfflineAudioContext::ResolveSuspendOnMainThread(size_t frame) {
  DCHECK(IsMainThread());

  // Suspend the context first. This will fire onstatechange event.
  SetContextState(V8AudioContextState::Enum::kSuspended);

  {
    base::AutoLock locker(suspend_frames_lock_);
    DCHECK(scheduled_suspend_frames_.Contains(frame));
    scheduled_suspend_frames_.erase(frame);
  }

  {
    // Wait until the suspend map is available for the removal.
    DeferredTaskHandler::GraphAutoLocker locker(GetDeferredTaskHandler());

    // If the context is going away, m_scheduledSuspends could have had all its
    // entries removed.  Check for that here.
    if (scheduled_suspends_.size()) {
      // `frame` must exist in the map.
      DCHECK(scheduled_suspends_.Contains(frame));

      SuspendMap::iterator it = scheduled_suspends_.find(frame);
      it->value->Resolve();

      scheduled_suspends_.erase(it);
    }
  }
}

void OfflineAudioContext::RejectPendingResolvers() {
  DCHECK(IsMainThread());

  for (auto& resolver : start_rendering_resolvers_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Audio context is going away"));
  }

  start_rendering_resolvers_.clear();
  pending_render_targets_.clear();

  {
    base::AutoLock locker(suspend_frames_lock_);
    scheduled_suspend_frames_.clear();
  }

  {
    // Wait until the suspend map is available for removal.
    DeferredTaskHandler::GraphAutoLocker locker(GetDeferredTaskHandler());

    // Offline context is going away so reject any promises that are still
    // pending.

    for (auto& pending_suspend_resolver : scheduled_suspends_) {
      pending_suspend_resolver.value->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, "Audio context is going away"));
    }

    scheduled_suspends_.clear();
  }

  BaseAudioContext::RejectPendingResolvers();
}

void OfflineAudioContext::DetachPendingResolvers() {
  DCHECK(IsMainThread());

  {
    base::AutoLock locker(suspend_frames_lock_);
    scheduled_suspend_frames_.clear();
  }

  for (auto& entry : scheduled_suspends_) {
    entry.value->SuppressDetachCheck();
  }
  scheduled_suspends_.clear();

  for (auto& resolver : start_rendering_resolvers_) {
    resolver->SuppressDetachCheck();
  }
  start_rendering_resolvers_.clear();
  pending_render_targets_.clear();

  BaseAudioContext::DetachPendingResolvers();
}

bool OfflineAudioContext::IsPullingAudioGraph() const {
  DCHECK(IsMainThread());

  // For an offline context, we're rendering only while the context is running.
  // Unlike an AudioContext, there's no audio device that keeps pulling on graph
  // after the context has finished rendering.
  return ContextState() == V8AudioContextState::Enum::kRunning;
}

bool OfflineAudioContext::ShouldSuspend() {
  DCHECK(IsAudioThread());

  base::AutoLock locker(suspend_frames_lock_);
  return scheduled_suspend_frames_.Contains(CurrentSampleFrame());
}

bool OfflineAudioContext::HasPendingActivity() const {
  return !pending_render_targets_.empty();
}

}  // namespace blink
