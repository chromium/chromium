/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_periodic_wave_constraints.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/analyser_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/biquad_filter_node.h"
#include "third_party/blink/renderer/modules/webaudio/channel_merger_node.h"
#include "third_party/blink/renderer/modules/webaudio/channel_splitter_node.h"
#include "third_party/blink/renderer/modules/webaudio/constant_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/convolver_node.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/modules/webaudio/delay_node.h"
#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_node.h"
#include "third_party/blink/renderer/modules/webaudio/gain_node.h"
#include "third_party/blink/renderer/modules/webaudio/iir_filter_node.h"
#include "third_party/blink/renderer/modules/webaudio/inspector_web_audio_agent.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/panner_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"
#include "third_party/blink/renderer/modules/webaudio/stereo_panner_node.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_node.h"
#include "third_party/blink/renderer/platform/audio/fft_frame.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {

// Constructor for rendering to the audio hardware.
BaseAudioContext::BaseAudioContext(LocalDOMWindow* window,
                                   enum ContextType context_type)
    : ActiveScriptWrappable<BaseAudioContext>({}),
      ExecutionContextLifecycleStateObserver(window),
      InspectorHelperMixin(*AudioGraphTracer::FromWindow(*window), String()),
      destination_node_(nullptr),
      task_runner_(window->GetTaskRunner(TaskType::kInternalMedia)),
      deferred_task_handler_(DeferredTaskHandler::Create(
          window->GetTaskRunner(TaskType::kInternalMedia))),
      periodic_wave_sine_(nullptr),
      periodic_wave_square_(nullptr),
      periodic_wave_sawtooth_(nullptr),
      periodic_wave_triangle_(nullptr) {}

BaseAudioContext::~BaseAudioContext() {
  {
    // We may need to destroy summing junctions, which must happen while this
    // object is still valid and with the graph lock held.
    DeferredTaskHandler::GraphAutoLocker locker(this);
    destination_handler_ = nullptr;
  }

  GetDeferredTaskHandler().ContextWillBeDestroyed();
}

void BaseAudioContext::Initialize() {
  if (IsDestinationInitialized()) {
    return;
  }

  audio_worklet_ = MakeGarbageCollected<AudioWorklet>(this);

  if (destination_node_) {
    destination_node_->Handler().Initialize();
    // TODO(crbug.com/863951).  The audio thread needs some things from the
    // destination handler like the currentTime.  But the audio thread
    // shouldn't access the `destination_node_` since it's an Oilpan object.
    // Thus, get the destination handler, a non-oilpan object, so we can get
    // the items directly from the handler instead of through the destination
    // node.
    destination_handler_ = &destination_node_->GetAudioDestinationHandler();

    // The AudioParams in the listener need access to the destination node, so
    // only create the listener if the destination node exists.
    listener_ = MakeGarbageCollected<AudioListener>(*this);

    FFTFrame::Initialize(sampleRate());

    // Report the context construction to the inspector.
    ReportDidCreate();
  }
}

void BaseAudioContext::Clear() {
  // Make a note that we've cleared out the context so that there's no pending
  // activity.
  is_cleared_ = true;
}

void BaseAudioContext::Uninitialize() {
  DCHECK(IsMainThread());

  if (!IsDestinationInitialized()) {
    return;
  }

  // Report the inspector that the context will be destroyed.
  ReportWillBeDestroyed();

  // This stops the audio thread and all audio rendering.
  if (destination_node_) {
    destination_node_->Handler().Uninitialize();
  }

  // Remove tail nodes since the context is done.
  GetDeferredTaskHandler().FinishTailProcessing();

  // Get rid of the sources which may still be playing.
  ReleaseActiveSourceNodes();

  // Reject any pending resolvers before we go away.
  RejectPendingResolvers();

  DCHECK(listener_);
  listener_->Handler().WaitForHRTFDatabaseLoaderThreadCompletion();

  Clear();

  DCHECK(!is_resolving_resume_promises_);
  DCHECK_EQ(pending_promises_resolvers_.size(), 0u);
}

void BaseAudioContext::Dispose() {
  // BaseAudioContext is going away, so remove the context from the orphan
  // handlers.
  GetDeferredTaskHandler().ClearContextFromOrphanHandlers();
}

void BaseAudioContext::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  // Don't need to do anything for an offline context.
  if (!HasRealtimeConstraint()) {
    return;
  }

  if (state == mojom::FrameLifecycleState::kRunning) {
    destination()->GetAudioDestinationHandler().Resume();
  } else if (state == mojom::FrameLifecycleState::kFrozen ||
             state == mojom::FrameLifecycleState::kFrozenAutoResumeMedia) {
    destination()->GetAudioDestinationHandler().Pause();
  }
}

void BaseAudioContext::ContextDestroyed() {
  destination()->GetAudioDestinationHandler().ContextDestroyed();
  Uninitialize();
}

bool BaseAudioContext::HasPendingActivity() const {
  // As long as AudioWorklet has a pending task from worklet script loading,
  // the BaseAudioContext needs to stay.
  if (audioWorklet() && audioWorklet()->HasPendingTasks()) {
    return true;
  }

  // There's no pending activity if the audio context has been cleared.
  return !is_cleared_;
}

AudioDestinationNode* BaseAudioContext::destination() const {
  // Cannot be called from the audio thread because this method touches objects
  // managed by Oilpan, and the audio thread is not managed by Oilpan.
  DCHECK(!IsAudioThread());
  return destination_node_.Get();
}

void BaseAudioContext::WarnIfContextClosed(const AudioHandler* handler) const {
  DCHECK(handler);

  if (IsContextCleared() && GetExecutionContext()) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kWarning,
            "Construction of " + handler->NodeTypeName() +
                " is not useful when context is closed."));
  }
}

void BaseAudioContext::WarnForConnectionIfContextClosed() const {
  if (IsContextCleared() && GetExecutionContext()) {
    GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<
                                             ConsoleMessage>(
        mojom::ConsoleMessageSource::kOther,
        mojom::ConsoleMessageLevel::kWarning,
        "Connecting nodes after the context has been closed is not useful."));
  }
}

AudioBuffer* BaseAudioContext::createBuffer(uint32_t number_of_channels,
                                            uint32_t number_of_frames,
                                            float sample_rate,
                                            ExceptionState& exception_state) {
  // It's ok to call createBuffer, even if the context is closed because the
  // AudioBuffer doesn't really "belong" to any particular context.

  AudioBuffer* buffer = AudioBuffer::Create(
      number_of_channels, number_of_frames, sample_rate, exception_state);

  // Only record the data if the creation succeeded.
  if (buffer) {
    base::UmaHistogramSparse("WebAudio.AudioBuffer.NumberOfChannels",
                             number_of_channels);

    // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
    // at 48kHz).  The number of buckets is fairly arbitrary.
    base::UmaHistogramCounts1M("WebAudio.AudioBuffer.Length", number_of_frames);

    // The limits are the min and max AudioBuffer sample rates currently
    // supported.  We use explicit values here instead of
    // audio_utilities::minAudioBufferSampleRate() and
    // audio_utilities::maxAudioBufferSampleRate().  The number of buckets is
    // fairly arbitrary.
    base::UmaHistogramCustomCounts("WebAudio.AudioBuffer.SampleRate384kHz",
                                   sample_rate, 3000, 384000, 60);

    // Compute the ratio of the buffer rate and the context rate so we know
    // how often the buffer needs to be resampled to match the context.  For
    // the histogram, we multiply the ratio by 100 and round to the nearest
    // integer.  If the context is closed, don't record this because we
    // don't have a sample rate for closed context.
    if (!IsContextCleared()) {
      // The limits are choosen from 100*(3000/384000) = 0.78125 and
      // 100*(384000/3000) = 12800, where 3000 and 384000 are the current
      // min and max sample rates possible for an AudioBuffer.  The number
      // of buckets is fairly arbitrary.
      float ratio = 100 * sample_rate / sampleRate();
      base::UmaHistogramCustomCounts(
          "WebAudio.AudioBuffer.SampleRateRatio384kHz",
          static_cast<int>(0.5 + ratio), 1, 12800, 50);
    }
  }

  return buffer;
}

ScriptPromise<AudioBuffer> BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, nullptr, nullptr,
                         exception_state);
}

ScriptPromise<AudioBuffer> BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    V8DecodeSuccessCallback* success_callback,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, success_callback, nullptr,
                         exception_state);
}

ScriptPromise<AudioBuffer> BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);

  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot decode audio data: The document is no longer active.");
    return EmptyPromise();
  }

  v8::Isolate* isolate = script_state->GetIsolate();
  ArrayBufferContents buffer_contents;
  DOMException* dom_exception = nullptr;
  // Detach the audio array buffer from the main thread and start
  // async decoding of the data.
  if (!audio_data->IsDetachable(isolate) || audio_data->IsDetached()) {
    // If audioData is already detached (neutered) we need to reject the
    // promise with an error.
    dom_exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataCloneError,
        "Cannot decode detached ArrayBuffer");
    // Fall through in order to invoke the error_callback.
  } else if (!audio_data->Transfer(isolate, buffer_contents,
                                   IGNORE_EXCEPTION)) {
    // Transfer may throw a TypeError, which is not a DOMException. However, the
    // spec requires throwing a DOMException with kDataCloneError. Hence ignore
    // that exception and throw a DOMException instead.
    // https://webaudio.github.io/web-audio-api/#dom-baseaudiocontext-decodeaudiodata
    dom_exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kDataCloneError, "Cannot transfer the ArrayBuffer");
    // Fall through in order to invoke the error_callback.
  } else {  // audio_data->Transfer succeeded.
    DOMArrayBuffer* audio = DOMArrayBuffer::Create(buffer_contents);

    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<AudioBuffer>>(
        script_state, exception_state.GetContext());
    auto promise = resolver->Promise();
    decode_audio_resolvers_.insert(resolver);

    audio_decoder_.DecodeAsync(audio, sampleRate(), success_callback,
                               error_callback, resolver, this, exception_state);
    return promise;
  }

  // Forward the exception to the callback.
  DCHECK(dom_exception);
  if (error_callback) {
    error_callback->InvokeAndReportException(this, dom_exception);
  }

  return ScriptPromise<AudioBuffer>::RejectWithDOMException(script_state,
                                                            dom_exception);
}

void BaseAudioContext::HandleDecodeAudioData(
    AudioBuffer* audio_buffer,
    ScriptPromiseResolver<AudioBuffer>* resolver,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    ExceptionContext exception_context) {
  DCHECK(IsMainThread());
  DCHECK(resolver);

  ScriptState* resolver_script_state = resolver->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }
  ScriptState::Scope script_state_scope(resolver_script_state);

  if (audio_buffer) {
    // Resolve promise successfully and run the success callback
    resolver->Resolve(audio_buffer);
    if (success_callback) {
      success_callback->InvokeAndReportException(this, audio_buffer);
    }
  } else {
    // Reject the promise and run the error callback
    auto* dom_exception = MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kEncodingError, "Unable to decode audio data");
    resolver->Reject(dom_exception);
    if (error_callback) {
      error_callback->InvokeAndReportException(this, dom_exception);
    }
  }

  // Resolving a promise above can result in uninitializing/clearing of the
  // context. (e.g. dropping an iframe. See crbug.com/1350086)
  if (is_cleared_) {
    return;
  }

  // Otherwise the resolver should exist in the set. Check and remove it.
  DCHECK(decode_audio_resolvers_.Contains(resolver));
  decode_audio_resolvers_.erase(resolver);
}

AudioBufferSourceNode* BaseAudioContext::createBufferSource(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  AudioBufferSourceNode* node =
      AudioBufferSourceNode::Create(*this, exception_state);

  // Do not add a reference to this source node now. The reference will be added
  // when start() is called.

  return node;
}

ConstantSourceNode* BaseAudioContext::createConstantSource(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ConstantSourceNode::Create(*this, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    uint32_t buffer_size,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    uint32_t buffer_size,
    uint32_t number_of_input_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size,
                                     number_of_input_channels, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    uint32_t buffer_size,
    uint32_t number_of_input_channels,
    uint32_t number_of_output_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(
      *this, buffer_size, number_of_input_channels, number_of_output_channels,
      exception_state);
}

StereoPannerNode* BaseAudioContext::createStereoPanner(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return StereoPannerNode::Create(*this, exception_state);
}

BiquadFilterNode* BaseAudioContext::createBiquadFilter(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return BiquadFilterNode::Create(*this, exception_state);
}

WaveShaperNode* BaseAudioContext::createWaveShaper(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return WaveShaperNode::Create(*this, exception_state);
}

PannerNode* BaseAudioContext::createPanner(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return PannerNode::Create(*this, exception_state);
}

ConvolverNode* BaseAudioContext::createConvolver(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ConvolverNode::Create(*this, exception_state);
}

DynamicsCompressorNode* BaseAudioContext::createDynamicsCompressor(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DynamicsCompressorNode::Create(*this, exception_state);
}

AnalyserNode* BaseAudioContext::createAnalyser(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return AnalyserNode::Create(*this, exception_state);
}

GainNode* BaseAudioContext::createGain(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return GainNode::Create(*this, exception_state);
}

DelayNode* BaseAudioContext::createDelay(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DelayNode::Create(*this, exception_state);
}

DelayNode* BaseAudioContext::createDelay(double max_delay_time,
                                         ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DelayNode::Create(*this, max_delay_time, exception_state);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelSplitterNode::Create(*this, exception_state);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    uint32_t number_of_outputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelSplitterNode::Create(*this, number_of_outputs, exception_state);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelMergerNode::Create(*this, exception_state);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    uint32_t number_of_inputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelMergerNode::Create(*this, number_of_inputs, exception_state);
}

OscillatorNode* BaseAudioContext::createOscillator(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return OscillatorNode::Create(*this, "sine", nullptr, exception_state);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    const Vector<float>& real,
    const Vector<float>& imag,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return PeriodicWave::Create(*this, real, imag, false, exception_state);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    const Vector<float>& real,
    const Vector<float>& imag,
    const PeriodicWaveConstraints* options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  bool disable = options->disableNormalization();

  return PeriodicWave::Create(*this, real, imag, disable, exception_state);
}

IIRFilterNode* BaseAudioContext::createIIRFilter(
    Vector<double> feedforward_coef,
    Vector<double> feedback_coef,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return IIRFilterNode::Create(*this, feedforward_coef, feedback_coef,
                               exception_state);
}

PeriodicWave* BaseAudioContext::GetPeriodicWave(int type) {
  switch (type) {
    case OscillatorHandler::SINE:
      // Initialize the table if necessary
      if (!periodic_wave_sine_) {
        periodic_wave_sine_ = PeriodicWave::CreateSine(sampleRate());
      }
      return periodic_wave_sine_.Get();
    case OscillatorHandler::SQUARE:
      // Initialize the table if necessary
      if (!periodic_wave_square_) {
        periodic_wave_square_ = PeriodicWave::CreateSquare(sampleRate());
      }
      return periodic_wave_square_.Get();
    case OscillatorHandler::SAWTOOTH:
      // Initialize the table if necessary
      if (!periodic_wave_sawtooth_) {
        periodic_wave_sawtooth_ = PeriodicWave::CreateSawtooth(sampleRate());
      }
      return periodic_wave_sawtooth_.Get();
    case OscillatorHandler::TRIANGLE:
      // Initialize the table if necessary
      if (!periodic_wave_triangle_) {
        periodic_wave_triangle_ = PeriodicWave::CreateTriangle(sampleRate());
      }
      return periodic_wave_triangle_.Get();
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

String BaseAudioContext::state() const {
  // These strings had better match the strings for AudioContextState in
  // AudioContext.idl.
  switch (control_thread_state_) {
    case kSuspended:
      return "suspended";
    case kRunning:
      return "running";
    case kClosed:
      return "closed";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

void BaseAudioContext::SetContextState(AudioContextState new_state) {
  DCHECK(IsMainThread());

  // If there's no change in the current state, there's nothing that needs to be
  // done.
  if (new_state == control_thread_state_) {
    return;
  }

  // Validate the transitions.  The valid transitions are Suspended->Running,
  // Running->Suspended, and anything->Closed.
  switch (new_state) {
    case kSuspended:
      DCHECK_EQ(control_thread_state_, kRunning);
      break;
    case kRunning:
      DCHECK_EQ(control_thread_state_, kSuspended);
      break;
    case kClosed:
      DCHECK_NE(control_thread_state_, kClosed);
      break;
  }

  control_thread_state_ = new_state;

  if (new_state == kClosed) {
    GetDeferredTaskHandler().StopAcceptingTailProcessing();
  }

  // Notify context that state changed
  if (GetExecutionContext()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&BaseAudioContext::NotifyStateChange,
                                 WrapPersistent(this)));

    GraphTracer().DidChangeBaseAudioContext(this);
  }
}

void BaseAudioContext::NotifyStateChange() {
  DispatchEvent(*Event::Create(event_type_names::kStatechange));
}

void BaseAudioContext::NotifySourceNodeFinishedProcessing(
    AudioHandler* handler) {
  DCHECK(IsAudioThread());

  GetDeferredTaskHandler().GetFinishedSourceHandlers()->push_back(handler);
}

LocalDOMWindow* BaseAudioContext::GetWindow() const {
  return To<LocalDOMWindow>(GetExecutionContext());
}

void BaseAudioContext::NotifySourceNodeStartedProcessing(AudioNode* node) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(this);

  GetDeferredTaskHandler().GetActiveSourceHandlers()->insert(&node->Handler());
  node->Handler().MakeConnection();
}

void BaseAudioContext::ReleaseActiveSourceNodes() {
  DCHECK(IsMainThread());

  DeferredTaskHandler::GraphAutoLocker locker(this);

  for (auto source_handler :
       *GetDeferredTaskHandler().GetActiveSourceHandlers()) {
    source_handler->BreakConnectionWithLock();
  }
}

void BaseAudioContext::HandleStoppableSourceNodes() {
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  HashSet<scoped_refptr<AudioHandler>>* active_source_handlers =
      GetDeferredTaskHandler().GetActiveSourceHandlers();

  if (active_source_handlers->size()) {
    // Find source handlers to see if we can stop playing them.  Note: this
    // check doesn't have to be done every render quantum, if this checking
    // becomes to expensive.  It's ok to do this on a less frequency basis as
    // long as the active nodes eventually get stopped if they're done.
    for (auto handler : *active_source_handlers) {
      switch (handler->GetNodeType()) {
        case AudioHandler::kNodeTypeAudioBufferSource:
        case AudioHandler::kNodeTypeOscillator:
        case AudioHandler::kNodeTypeConstantSource: {
          AudioScheduledSourceHandler* source_handler =
              static_cast<AudioScheduledSourceHandler*>(handler.get());
          source_handler->HandleStoppableSourceNode();
          break;
        }
        default:
          break;
      }
    }
  }
}

void BaseAudioContext::PerformCleanupOnMainThread() {
  DCHECK(IsMainThread());

  // When a posted task is performed, the execution context might be gone.
  if (!GetExecutionContext()) {
    return;
  }

  DeferredTaskHandler::GraphAutoLocker locker(this);

  if (is_resolving_resume_promises_) {
    for (auto& resolver : pending_promises_resolvers_) {
      if (control_thread_state_ == kClosed) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Cannot resume a context that has been closed"));
      } else {
        SetContextState(kRunning);
        resolver->Resolve();
      }
    }
    pending_promises_resolvers_.clear();
    is_resolving_resume_promises_ = false;
  }

  has_posted_cleanup_task_ = false;
}

void BaseAudioContext::ScheduleMainThreadCleanup() {
  DCHECK(IsAudioThread());

  if (has_posted_cleanup_task_) {
    return;
  }
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&BaseAudioContext::PerformCleanupOnMainThread,
                          WrapCrossThreadPersistent(this)));
  has_posted_cleanup_task_ = true;
}

void BaseAudioContext::RejectPendingDecodeAudioDataResolvers() {
  // Now reject any pending decodeAudioData resolvers
  for (auto& resolver : decode_audio_resolvers_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Audio context is going away"));
  }
  decode_audio_resolvers_.clear();
}

void BaseAudioContext::RejectPendingResolvers() {
  DCHECK(IsMainThread());

  // Audio context is closing down so reject any resume promises that are still
  // pending.

  for (auto& resolver : pending_promises_resolvers_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Audio context is going away"));
  }
  pending_promises_resolvers_.clear();
  is_resolving_resume_promises_ = false;

  RejectPendingDecodeAudioDataResolvers();
}

const AtomicString& BaseAudioContext::InterfaceName() const {
  return event_target_names::kAudioContext;
}

ExecutionContext* BaseAudioContext::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

void BaseAudioContext::StartRendering() {
  // This is called for both online and offline contexts.  The caller
  // must set the context state appropriately. In particular, resuming
  // a context should wait until the context has actually resumed to
  // set the state.
  DCHECK(IsMainThread());
  DCHECK(destination_node_);

  if (control_thread_state_ == kSuspended) {
    destination()->GetAudioDestinationHandler().StartRendering();
  }
}

void BaseAudioContext::Trace(Visitor* visitor) const {
  visitor->Trace(destination_node_);
  visitor->Trace(listener_);
  visitor->Trace(pending_promises_resolvers_);
  visitor->Trace(decode_audio_resolvers_);
  visitor->Trace(periodic_wave_sine_);
  visitor->Trace(periodic_wave_square_);
  visitor->Trace(periodic_wave_sawtooth_);
  visitor->Trace(periodic_wave_triangle_);
  visitor->Trace(audio_worklet_);
  InspectorHelperMixin::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

const SecurityOrigin* BaseAudioContext::GetSecurityOrigin() const {
  if (GetExecutionContext()) {
    return GetExecutionContext()->GetSecurityOrigin();
  }

  return nullptr;
}

AudioWorklet* BaseAudioContext::audioWorklet() const {
  return audio_worklet_.Get();
}

void BaseAudioContext::NotifyWorkletIsReady() {
  DCHECK(IsMainThread());
  DCHECK(audioWorklet()->IsReady());

  {
    // `audio_worklet_thread_` is constantly peeked by the rendering thread,
    // So we protect it with the graph lock.
    DeferredTaskHandler::GraphAutoLocker locker(this);

    // At this point, the WorkletGlobalScope must be ready so it is safe to keep
    // the reference to the AudioWorkletThread for the future worklet operation.
    audio_worklet_thread_ =
        audioWorklet()->GetMessagingProxy()->GetBackingWorkerThread();
  }

  switch (ContextState()) {
    case kRunning:
      // If the context is running, restart the destination to switch the render
      // thread with the worklet thread right away.
      destination()->GetAudioDestinationHandler().RestartRendering();
      break;
    case kSuspended:
      // For the suspended context, the destination will use the worklet task
      // runner for rendering. This also prevents the regular audio thread from
      // touching worklet-related objects by blocking an invalid transitory
      // state where the context state is suspended and the destination state is
      // running. See: crbug.com/1403515
      destination()->GetAudioDestinationHandler().PrepareTaskRunnerForWorklet();
      break;
    case kClosed:
      // When the context is closed, no preparation for the worklet operations
      // is necessary.
      return;
  }
}

void BaseAudioContext::UpdateWorkletGlobalScopeOnRenderingThread() {
  DCHECK(!IsMainThread());

  if (TryLock()) {
    // Even when `audio_worklet_thread_` is successfully assigned, the current
    // render thread could still be a thread of AudioOutputDevice.  Updates the
    // the global scope only when the thread affinity is correct.
    if (audio_worklet_thread_ && audio_worklet_thread_->IsCurrentThread()) {
      AudioWorkletGlobalScope* global_scope =
          To<AudioWorkletGlobalScope>(audio_worklet_thread_->GlobalScope());
      DCHECK(global_scope);
      global_scope->SetCurrentFrame(CurrentSampleFrame());
    }

    unlock();
  }
}

int32_t BaseAudioContext::MaxChannelCount() {
  DCHECK(IsMainThread());

  AudioDestinationNode* destination_node = destination();
  if (!destination_node ||
      !destination_node->GetAudioDestinationHandler().IsInitialized()) {
    return -1;
  }

  return destination_node->GetAudioDestinationHandler().MaxChannelCount();
}

int32_t BaseAudioContext::CallbackBufferSize() {
  DCHECK(IsMainThread());

  AudioDestinationNode* destination_node = destination();
  if (!destination_node ||
      !destination_node->GetAudioDestinationHandler().IsInitialized() ||
      !HasRealtimeConstraint()) {
    return -1;
  }

  RealtimeAudioDestinationHandler& destination_handler =
      static_cast<RealtimeAudioDestinationHandler&>(
          destination_node->GetAudioDestinationHandler());
  return destination_handler.GetCallbackBufferSize();
}

void BaseAudioContext::ReportDidCreate() {
  GraphTracer().DidCreateBaseAudioContext(this);
  destination_node_->ReportDidCreate();
  listener_->ReportDidCreate();
}

void BaseAudioContext::ReportWillBeDestroyed() {
  listener_->ReportWillBeDestroyed();
  destination_node_->ReportWillBeDestroyed();
  GraphTracer().WillDestroyBaseAudioContext(this);
}

bool BaseAudioContext::CheckExecutionContextAndThrowIfNecessary(
    ExceptionState& exception_state) {
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "The operation is not allowed on a detached frame or document because "
        "no execution context is available.");
    return false;
  }

  return true;
}

}  // namespace blink
