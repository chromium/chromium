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

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_offline_audio_context_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    unsigned number_of_channels,
    unsigned number_of_frames,
    float sample_rate,
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

  if (!number_of_frames) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound<unsigned>(
            "number of frames", number_of_frames, 1));
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

  SCOPED_UMA_HISTOGRAM_TIMER("WebAudio.OfflineAudioContext.CreateTime");
  OfflineAudioContext* audio_context =
      MakeGarbageCollected<OfflineAudioContext>(window, number_of_channels,
                                                number_of_frames, sample_rate,
                                                exception_state);
  audio_context->UpdateStateIfNeeded();

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::OfflineAudioContext()\n",
          audio_context);
#endif
  return audio_context;
}

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    const OfflineAudioContextOptions* options,
    ExceptionState& exception_state) {
  OfflineAudioContext* offline_context =
      Create(context, options->numberOfChannels(), options->length(),
             options->sampleRate(), exception_state);

  return offline_context;
}

OfflineAudioContext::OfflineAudioContext(LocalDOMWindow* window,
                                         unsigned number_of_channels,
                                         uint32_t number_of_frames,
                                         float sample_rate,
                                         ExceptionState& exception_state)
    : BaseAudioContext(window, kOfflineContext),
      total_render_frames_(number_of_frames) {
  destination_node_ = OfflineAudioDestinationNode::Create(
      this, number_of_channels, number_of_frames, sample_rate);
  Initialize();
}

OfflineAudioContext::~OfflineAudioContext() {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::~OfflineAudioContext()\n",
          this);
#endif
}

void OfflineAudioContext::Trace(Visitor* visitor) const {
  visitor->Trace(complete_resolver_);
  visitor->Trace(scheduled_suspends_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise<AudioBuffer> OfflineAudioContext::startOfflineRendering(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Calling close() on an OfflineAudioContext is not supported/allowed,
  // but it might well have been stopped by its execution context.
  // See: crbug.com/435867
  if (IsContextCleared() || ContextState() == AudioContextState::kClosed) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call startRendering on an OfflineAudioContext in a stopped "
        "state.");
    return EmptyPromise();
  }

  // If the context is not in the suspended state (i.e. running), reject the
  // promise.
  if (ContextState() != AudioContextState::kSuspended) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot startRendering when an OfflineAudioContext is " + state());
    return EmptyPromise();
  }

  // Can't call startRendering more than once.  Return a rejected promise now.
  if (is_rendering_started_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot call startRendering more than once");
    return EmptyPromise();
  }

  DCHECK(!is_rendering_started_);

  complete_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<AudioBuffer>>(
      script_state, exception_state.GetContext());

  // Allocate the AudioBuffer to hold the rendered result.
  float sample_rate = DestinationHandler().SampleRate();
  unsigned number_of_channels = DestinationHandler().NumberOfChannels();

  AudioBuffer* render_target = AudioBuffer::CreateUninitialized(
      number_of_channels, total_render_frames_, sample_rate);

  if (!render_target) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "startRendering failed to create AudioBuffer(" +
            String::Number(number_of_channels) + ", " +
            String::Number(total_render_frames_) + ", " +
            String::Number(sample_rate) + ")");
    return EmptyPromise();
  }

  // Start rendering and return the promise.
  is_rendering_started_ = true;
  SetContextState(kRunning);
  static_cast<OfflineAudioDestinationNode*>(destination())
      ->SetDestinationBuffer(render_target);
  DestinationHandler().InitializeOfflineRenderThread(render_target);
  DestinationHandler().StartRendering();

  return complete_resolver_->Promise();
}

ScriptPromise<IDLUndefined> OfflineAudioContext::suspendContext(
    ScriptState* script_state,
    double when,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // If the rendering is finished, reject the promise.
  if (ContextState() == AudioContextState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "the rendering is already finished");
    return EmptyPromise();
  }

  // The specified suspend time is negative; reject the promise.
  if (when < 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "negative suspend time (" + String::Number(when) + ") is not allowed");
    return EmptyPromise();
  }

  // The suspend time should be earlier than the total render frame. If the
  // requested suspension time is equal to the total render frame, the promise
  // will be rejected.
  double total_render_duration = total_render_frames_ / sampleRate();
  if (total_render_duration <= when) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "cannot schedule a suspend at " +
            String::NumberToStringECMAScript(when) +
            " seconds because it is greater than "
            "or equal to the total "
            "render duration of " +
            String::Number(total_render_frames_) + " frames (" +
            String::NumberToStringECMAScript(total_render_duration) +
            " seconds)");
    return EmptyPromise();
  }

  // Find the sample frame and round up to the nearest render quantum
  // boundary.  This assumes the render quantum is a power of two.
  size_t frame = when * sampleRate();
  frame = GetDeferredTaskHandler().RenderQuantumFrames() *
          ((frame + GetDeferredTaskHandler().RenderQuantumFrames() - 1) /
           GetDeferredTaskHandler().RenderQuantumFrames());

  // The specified suspend time is in the past; reject the promise.
  if (frame < CurrentSampleFrame()) {
    size_t current_frame_clamped =
        std::min(CurrentSampleFrame(), static_cast<size_t>(length()));
    double current_time_clamped =
        std::min(currentTime(), length() / static_cast<double>(sampleRate()));
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "suspend(" + String::Number(when) + ") failed to suspend at frame " +
            String::Number(frame) + " because it is earlier than the current " +
            "frame of " + String::Number(current_frame_clamped) + " (" +
            String::Number(current_time_clamped) + " seconds)");
    return EmptyPromise();
  }

  ScriptPromise<IDLUndefined> promise;

  {
    // Wait until the suspend map is available for the insertion. Here we should
    // use GraphAutoLocker because it locks the graph from the main thread.
    DeferredTaskHandler::GraphAutoLocker locker(this);

    // If there is a duplicate suspension at the same quantized frame,
    // reject the promise.
    if (scheduled_suspends_.Contains(frame)) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "cannot schedule more than one suspend at frame " +
              String::Number(frame) + " (" + String::Number(when) +
              " seconds)");
      return EmptyPromise();
    }

    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        script_state, exception_state.GetContext());
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
  if (IsContextCleared() || ContextState() == AudioContextState::kClosed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "cannot resume a closed offline context");
    return EmptyPromise();
  }

  // If the context is already running, resolve the promise without altering
  // the current state or starting the rendering loop.
  if (ContextState() == AudioContextState::kRunning) {
    return ToResolvedUndefinedPromise(script_state);
  }

  DCHECK_EQ(ContextState(), AudioContextState::kSuspended);

  // If the context is suspended, resume rendering by setting the state to
  // "Running". and calling startRendering(). Note that resuming is possible
  // only after the rendering started.
  SetContextState(kRunning);
  DestinationHandler().StartRendering();

  // Resolve the promise immediately.
  return ToResolvedUndefinedPromise(script_state);
}

void OfflineAudioContext::FireCompletionEvent() {
  DCHECK(IsMainThread());

  // Context is finished, so remove any tail processing nodes; there's nowhere
  // for the output to go.
  GetDeferredTaskHandler().FinishTailProcessing();

  // We set the state to closed here so that the oncomplete event handler sees
  // that the context has been closed.
  SetContextState(kClosed);

  // Avoid firing the event if the document has already gone away.
  if (GetExecutionContext()) {
    AudioBuffer* rendered_buffer =
        static_cast<OfflineAudioDestinationNode*>(destination())
            ->DestinationBuffer();
    DCHECK(rendered_buffer);
    if (!rendered_buffer) {
      return;
    }

    // Call the offline rendering completion event listener and resolve the
    // promise too.
    DispatchEvent(*OfflineAudioCompletionEvent::Create(rendered_buffer));
    complete_resolver_->Resolve(rendered_buffer);
  } else {
    // The resolver should be rejected when the execution context is gone.
    complete_resolver_->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "the execution context does not exist"));
  }

  is_rendering_started_ = false;

  PerformCleanupOnMainThread();
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
    DeferredTaskHandler::OfflineGraphAutoLocker locker(this);
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
    DeferredTaskHandler::OfflineGraphAutoLocker locker(this);

    GetDeferredTaskHandler().BreakConnections();
    GetDeferredTaskHandler().HandleDeferredTasks();
    GetDeferredTaskHandler().RequestToDeleteHandlersOnMainThread();
  }
}

OfflineAudioDestinationHandler& OfflineAudioContext::DestinationHandler() {
  return static_cast<OfflineAudioDestinationHandler&>(
      destination()->GetAudioDestinationHandler());
}

void OfflineAudioContext::ResolveSuspendOnMainThread(size_t frame) {
  DCHECK(IsMainThread());

  // Suspend the context first. This will fire onstatechange event.
  SetContextState(kSuspended);

  {
    base::AutoLock locker(suspend_frames_lock_);
    DCHECK(scheduled_suspend_frames_.Contains(frame));
    scheduled_suspend_frames_.erase(frame);
  }

  {
    // Wait until the suspend map is available for the removal.
    DeferredTaskHandler::GraphAutoLocker locker(this);

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

  {
    base::AutoLock locker(suspend_frames_lock_);
    scheduled_suspend_frames_.clear();
  }

  {
    // Wait until the suspend map is available for removal.
    DeferredTaskHandler::GraphAutoLocker locker(this);

    // Offline context is going away so reject any promises that are still
    // pending.

    for (auto& pending_suspend_resolver : scheduled_suspends_) {
      pending_suspend_resolver.value->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, "Audio context is going away"));
    }

    scheduled_suspends_.clear();
    DCHECK_EQ(pending_promises_resolvers_.size(), 0u);
  }

  RejectPendingDecodeAudioDataResolvers();
}

bool OfflineAudioContext::IsPullingAudioGraph() const {
  DCHECK(IsMainThread());

  // For an offline context, we're rendering only while the context is running.
  // Unlike an AudioContext, there's no audio device that keeps pulling on graph
  // after the context has finished rendering.
  return ContextState() == BaseAudioContext::kRunning;
}

bool OfflineAudioContext::ShouldSuspend() {
  DCHECK(IsAudioThread());

  base::AutoLock locker(suspend_frames_lock_);
  return scheduled_suspend_frames_.Contains(CurrentSampleFrame());
}

bool OfflineAudioContext::HasPendingActivity() const {
  return is_rendering_started_;
}

}  // namespace blink
