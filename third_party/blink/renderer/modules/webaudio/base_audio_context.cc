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

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/modules/webaudio/analyser_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
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
#include "third_party/blink/renderer/modules/webaudio/delay_node.h"
#include "third_party/blink/renderer/modules/webaudio/dynamics_compressor_node.h"
#include "third_party/blink/renderer/modules/webaudio/gain_node.h"
#include "third_party/blink/renderer/modules/webaudio/iir_filter_node.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/oscillator_node.h"
#include "third_party/blink/renderer/modules/webaudio/panner_node.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave.h"
#include "third_party/blink/renderer/modules/webaudio/periodic_wave_constraints.h"
#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"
#include "third_party/blink/renderer/modules/webaudio/stereo_panner_node.h"
#include "third_party/blink/renderer/modules/webaudio/wave_shaper_node.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/uuid.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

BaseAudioContext* BaseAudioContext::Create(
    Document& document,
    const AudioContextOptions& context_options,
    ExceptionState& exception_state) {
  return AudioContext::Create(document, context_options, exception_state);
}

// Constructor for rendering to the audio hardware.
BaseAudioContext::BaseAudioContext(Document* document,
                                   enum ContextType context_type)
    : PausableObject(document),
      destination_node_(nullptr),
      uuid_(CreateCanonicalUUIDString()),
      is_cleared_(false),
      is_resolving_resume_promises_(false),
      has_posted_cleanup_task_(false),
      deferred_task_handler_(DeferredTaskHandler::Create(
          document->GetTaskRunner(TaskType::kInternalMedia))),
      context_state_(kSuspended),
      periodic_wave_sine_(nullptr),
      periodic_wave_square_(nullptr),
      periodic_wave_sawtooth_(nullptr),
      periodic_wave_triangle_(nullptr),
      output_position_(),
      task_runner_(document->GetTaskRunner(TaskType::kInternalMedia)) {}

BaseAudioContext::~BaseAudioContext() {
  {
    // We may need to destroy summing junctions, which must happen while this
    // object is still valid and with the graph lock held.
    GraphAutoLocker locker(this);
    destination_handler_ = nullptr;
  }

  GetDeferredTaskHandler().ContextWillBeDestroyed();
}

void BaseAudioContext::Initialize() {
  if (IsDestinationInitialized())
    return;

  FFTFrame::Initialize();

  audio_worklet_ = AudioWorklet::Create(this);

  if (destination_node_) {
    destination_node_->Handler().Initialize();
    // TODO(crbug.com/863951).  The audio thread needs some things from the
    // destination handler like the currentTime.  But the audio thread
    // shouldn't access the |destination_node_| since it's an Oilpan object.
    // Thus, get the destination handler, a non-oilpan object, so we can get
    // the items directly from the handler instead of through the destination
    // node.
    destination_handler_ = &destination_node_->GetAudioDestinationHandler();

    // The AudioParams in the listener need access to the destination node, so
    // only create the listener if the destination node exists.
    listener_ = AudioListener::Create(*this);
  }
}

void BaseAudioContext::Clear() {
  destination_node_.Clear();
  // The audio rendering thread is dead.  Nobody will schedule AudioHandler
  // deletion.  Let's do it ourselves.
  GetDeferredTaskHandler().ClearHandlersToBeDeleted();
  is_cleared_ = true;
}

void BaseAudioContext::Uninitialize() {
  DCHECK(IsMainThread());

  if (!IsDestinationInitialized())
    return;

  // This stops the audio thread and all audio rendering.
  if (destination_node_)
    destination_node_->Handler().Uninitialize();

  // Remove tail nodes since the context is done.
  GetDeferredTaskHandler().FinishTailProcessing();

  // Get rid of the sources which may still be playing.
  ReleaseActiveSourceNodes();

  // Reject any pending resolvers before we go away.
  RejectPendingResolvers();

  DCHECK(listener_);
  listener_->WaitForHRTFDatabaseLoaderThreadCompletion();

  Clear();

  DCHECK(!is_resolving_resume_promises_);
  DCHECK_EQ(resume_resolvers_.size(), 0u);
  DCHECK_EQ(active_source_nodes_.size(), 0u);
}

void BaseAudioContext::ContextDestroyed(ExecutionContext*) {
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
  return destination_node_;
}

void BaseAudioContext::ThrowExceptionForClosedState(
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "AudioContext has been closed.");
}

AudioBuffer* BaseAudioContext::createBuffer(unsigned number_of_channels,
                                            size_t number_of_frames,
                                            float sample_rate,
                                            ExceptionState& exception_state) {
  // It's ok to call createBuffer, even if the context is closed because the
  // AudioBuffer doesn't really "belong" to any particular context.

  AudioBuffer* buffer = AudioBuffer::Create(
      number_of_channels, number_of_frames, sample_rate, exception_state);

  if (buffer) {
    // Only record the data if the creation succeeded.
    DEFINE_STATIC_LOCAL(SparseHistogram, audio_buffer_channels_histogram,
                        ("WebAudio.AudioBuffer.NumberOfChannels"));

    // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
    // at 48kHz).  The number of buckets is fairly arbitrary.
    DEFINE_STATIC_LOCAL(CustomCountHistogram, audio_buffer_length_histogram,
                        ("WebAudio.AudioBuffer.Length", 1, 1000000, 50));
    // The limits are the min and max AudioBuffer sample rates currently
    // supported.  We use explicit values here instead of
    // audio_utilities::minAudioBufferSampleRate() and
    // audio_utilities::maxAudioBufferSampleRate().  The number of buckets is
    // fairly arbitrary.
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, audio_buffer_sample_rate_histogram,
        ("WebAudio.AudioBuffer.SampleRate384kHz", 3000, 384000, 60));

    audio_buffer_channels_histogram.Sample(number_of_channels);
    audio_buffer_length_histogram.Count(number_of_frames);
    audio_buffer_sample_rate_histogram.Count(sample_rate);

    // Compute the ratio of the buffer rate and the context rate so we know
    // how often the buffer needs to be resampled to match the context.  For
    // the histogram, we multiply the ratio by 100 and round to the nearest
    // integer.  If the context is closed, don't record this because we
    // don't have a sample rate for closed context.
    if (!IsContextClosed()) {
      // The limits are choosen from 100*(3000/384000) = 0.78125 and
      // 100*(384000/3000) = 12800, where 3000 and 384000 are the current
      // min and max sample rates possible for an AudioBuffer.  The number
      // of buckets is fairly arbitrary.
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, audio_buffer_sample_rate_ratio_histogram,
          ("WebAudio.AudioBuffer.SampleRateRatio384kHz", 1, 12800, 50));
      float ratio = 100 * sample_rate / this->sampleRate();
      audio_buffer_sample_rate_ratio_histogram.Count(
          static_cast<int>(0.5 + ratio));
    }
  }

  return buffer;
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, nullptr, nullptr,
                         exception_state);
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    V8DecodeSuccessCallback* success_callback,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, success_callback, nullptr,
                         exception_state);
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  v8::Isolate* isolate = script_state->GetIsolate();
  WTF::ArrayBufferContents buffer_contents;
  // Detach the audio array buffer from the main thread and start
  // async decoding of the data.
  if (audio_data->IsNeuterable(isolate) &&
      audio_data->Transfer(isolate, buffer_contents)) {
    DOMArrayBuffer* audio = DOMArrayBuffer::Create(buffer_contents);

    decode_audio_resolvers_.insert(resolver);

    audio_decoder_.DecodeAsync(
        audio, sampleRate(), ToV8PersistentCallbackFunction(success_callback),
        ToV8PersistentCallbackFunction(error_callback), resolver, this);
  } else {
    // If audioData is already detached (neutered) we need to reject the
    // promise with an error.
    DOMException* error =
        DOMException::Create(DOMExceptionCode::kDataCloneError,
                             "Cannot decode detached ArrayBuffer");
    resolver->Reject(error);
    if (error_callback) {
      error_callback->InvokeAndReportException(this, error);
    }
  }

  return promise;
}

void BaseAudioContext::HandleDecodeAudioData(
    AudioBuffer* audio_buffer,
    ScriptPromiseResolver* resolver,
    V8PersistentCallbackFunction<V8DecodeSuccessCallback>* success_callback,
    V8PersistentCallbackFunction<V8DecodeErrorCallback>* error_callback) {
  DCHECK(IsMainThread());

  if (audio_buffer) {
    // Resolve promise successfully and run the success callback
    resolver->Resolve(audio_buffer);
    if (success_callback)
      success_callback->InvokeAndReportException(this, audio_buffer);
  } else {
    // Reject the promise and run the error callback
    DOMException* error = DOMException::Create(DOMExceptionCode::kEncodingError,
                                               "Unable to decode audio data");
    resolver->Reject(error);
    if (error_callback)
      error_callback->InvokeAndReportException(this, error);
  }

  // We've resolved the promise.  Remove it now.
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
    size_t buffer_size,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t buffer_size,
    size_t number_of_input_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size,
                                     number_of_input_channels, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t buffer_size,
    size_t number_of_input_channels,
    size_t number_of_output_channels,
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
    size_t number_of_outputs,
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
    size_t number_of_inputs,
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
    const PeriodicWaveConstraints& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  bool disable = options.disableNormalization();

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
      if (!periodic_wave_sine_)
        periodic_wave_sine_ = PeriodicWave::CreateSine(sampleRate());
      return periodic_wave_sine_;
    case OscillatorHandler::SQUARE:
      // Initialize the table if necessary
      if (!periodic_wave_square_)
        periodic_wave_square_ = PeriodicWave::CreateSquare(sampleRate());
      return periodic_wave_square_;
    case OscillatorHandler::SAWTOOTH:
      // Initialize the table if necessary
      if (!periodic_wave_sawtooth_)
        periodic_wave_sawtooth_ = PeriodicWave::CreateSawtooth(sampleRate());
      return periodic_wave_sawtooth_;
    case OscillatorHandler::TRIANGLE:
      // Initialize the table if necessary
      if (!periodic_wave_triangle_)
        periodic_wave_triangle_ = PeriodicWave::CreateTriangle(sampleRate());
      return periodic_wave_triangle_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

String BaseAudioContext::state() const {
  // These strings had better match the strings for AudioContextState in
  // AudioContext.idl.
  switch (context_state_) {
    case kSuspended:
      return "suspended";
    case kRunning:
      return "running";
    case kClosed:
      return "closed";
  }
  NOTREACHED();
  return "";
}

void BaseAudioContext::SetContextState(AudioContextState new_state) {
  DCHECK(IsMainThread());

  // If there's no change in the current state, there's nothing that needs to be
  // done.
  if (new_state == context_state_) {
    return;
  }

  // Validate the transitions.  The valid transitions are Suspended->Running,
  // Running->Suspended, and anything->Closed.
  switch (new_state) {
    case kSuspended:
      DCHECK_EQ(context_state_, kRunning);
      break;
    case kRunning:
      DCHECK_EQ(context_state_, kSuspended);
      break;
    case kClosed:
      DCHECK_NE(context_state_, kClosed);
      break;
  }

  context_state_ = new_state;

  // Audibility checks only happen when the context is running so manual
  // notification is required when the context gets suspended or closed.
  if (was_audible_ && context_state_ != kRunning) {
    was_audible_ = false;
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBind(&BaseAudioContext::NotifyAudibleAudioStopped,
                        WrapCrossThreadPersistent(this)));
  }

  // Notify context that state changed
  if (GetExecutionContext()) {
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kMediaElementEvent)
        ->PostTask(FROM_HERE, WTF::Bind(&BaseAudioContext::NotifyStateChange,
                                        WrapPersistent(this)));
  }
}

void BaseAudioContext::NotifyStateChange() {
  DispatchEvent(*Event::Create(EventTypeNames::statechange));
}

void BaseAudioContext::NotifySourceNodeFinishedProcessing(
    AudioHandler* handler) {
  // This can be called from either the main thread or the audio thread.  The
  // mutex below protects access to |finished_source_handlers_| between the two
  // threads.
  MutexLocker lock(finished_source_handlers_mutex_);
  finished_source_handlers_.push_back(handler);
}

Document* BaseAudioContext::GetDocument() const {
  return To<Document>(GetExecutionContext());
}

void BaseAudioContext::NotifySourceNodeStartedProcessing(AudioNode* node) {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(this);

  active_source_nodes_.push_back(node);
  node->Handler().MakeConnection();
}

void BaseAudioContext::ReleaseActiveSourceNodes() {
  DCHECK(IsMainThread());
  for (auto& source_node : active_source_nodes_)
    source_node->Handler().BreakConnection();

  active_source_nodes_.clear();
}

void BaseAudioContext::HandleStoppableSourceNodes() {
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  if (finished_source_handlers_.size())
    ScheduleMainThreadCleanup();
}

void BaseAudioContext::HandlePreRenderTasks(
    const AudioIOPosition& output_position) {
  DCHECK(IsAudioThread());

  // At the beginning of every render quantum, try to update the internal
  // rendering graph state (from main thread changes).  It's OK if the tryLock()
  // fails, we'll just take slightly longer to pick up the changes.
  if (TryLock()) {
    GetDeferredTaskHandler().HandleDeferredTasks();

    ResolvePromisesForUnpause();

    // Check to see if source nodes can be stopped because the end time has
    // passed.
    HandleStoppableSourceNodes();

    // Update the dirty state of the listener.
    listener()->UpdateState();

    // Update output timestamp.
    output_position_ = output_position;

    unlock();
  }
}

// Determine if the rendered data is audible.
static bool IsAudible(const AudioBus* rendered_data) {
  // Compute the energy in each channel and sum up the energy in each channel
  // for the total energy.
  float energy = 0;

  unsigned data_size = rendered_data->length();
  for (unsigned k = 0; k < rendered_data->NumberOfChannels(); ++k) {
    const float* data = rendered_data->Channel(k)->Data();
    float channel_energy;
    vector_math::Vsvesq(data, 1, &channel_energy, data_size);
    energy += channel_energy;
  }

  return energy > 0;
}

void BaseAudioContext::HandlePostRenderTasks(const AudioBus* destination_bus) {
  DCHECK(IsAudioThread());

  // Must use a tryLock() here too.  Don't worry, the lock will very rarely be
  // contended and this method is called frequently.  The worst that can happen
  // is that there will be some nodes which will take slightly longer than usual
  // to be deleted or removed from the render graph (in which case they'll
  // render silence).
  if (TryLock()) {
    // Take care of AudioNode tasks where the tryLock() failed previously.
    GetDeferredTaskHandler().BreakConnections();

    GetDeferredTaskHandler().HandleDeferredTasks();
    GetDeferredTaskHandler().RequestToDeleteHandlersOnMainThread();

    unlock();
  }

  // Notify browser if audible audio has started or stopped.
  if (HasRealtimeConstraint()) {
    // Detect silence (or not) for MEI
    bool is_audible = IsAudible(destination_bus);

    if (is_audible) {
      ++total_audible_renders_;
    }

    if (was_audible_ != is_audible) {
      // Audibility changed in this render, so report the change.
      was_audible_ = is_audible;
      if (is_audible) {
        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBind(&BaseAudioContext::NotifyAudibleAudioStarted,
                            WrapCrossThreadPersistent(this)));
      } else {
        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBind(&BaseAudioContext::NotifyAudibleAudioStopped,
                            WrapCrossThreadPersistent(this)));
      }
    }
  }
}

void BaseAudioContext::PerformCleanupOnMainThread() {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(this);

  if (is_resolving_resume_promises_) {
    for (auto& resolver : resume_resolvers_) {
      if (context_state_ == kClosed) {
        resolver->Reject(DOMException::Create(
            DOMExceptionCode::kInvalidStateError,
            "Cannot resume a context that has been closed"));
      } else {
        SetContextState(kRunning);
        resolver->Resolve();
      }
    }
    resume_resolvers_.clear();
    is_resolving_resume_promises_ = false;
  }

  if (active_source_nodes_.size()) {
    // Find AudioBufferSourceNodes to see if we can stop playing them.
    for (AudioNode* node : active_source_nodes_) {
      if (node->Handler().GetNodeType() ==
          AudioHandler::kNodeTypeAudioBufferSource) {
        AudioBufferSourceNode* source_node =
            static_cast<AudioBufferSourceNode*>(node);
        source_node->GetAudioBufferSourceHandler().HandleStoppableSourceNode();
      }
    }

    Vector<AudioHandler*> finished_handlers;
    {
      MutexLocker lock(finished_source_handlers_mutex_);
      finished_source_handlers_.swap(finished_handlers);
    }
    // Break the connection and release active nodes that have finished
    // playing.
    unsigned remove_count = 0;
    Vector<bool> removables;
    removables.resize(active_source_nodes_.size());
    for (AudioHandler* handler : finished_handlers) {
      for (unsigned i = 0; i < active_source_nodes_.size(); ++i) {
        if (handler == &active_source_nodes_[i]->Handler()) {
          handler->BreakConnectionWithLock();
          removables[i] = true;
          remove_count++;
          break;
        }
      }
    }

    // Copy over the surviving active nodes after removal.
    if (remove_count > 0) {
      HeapVector<Member<AudioNode>> actives;
      DCHECK_GE(active_source_nodes_.size(), remove_count);
      size_t initial_capacity =
          std::min(active_source_nodes_.size() - remove_count,
                   active_source_nodes_.size());
      actives.ReserveInitialCapacity(initial_capacity);
      for (unsigned i = 0; i < removables.size(); ++i) {
        if (!removables[i])
          actives.push_back(active_source_nodes_[i]);
      }
      active_source_nodes_.swap(actives);
    }
  }

  has_posted_cleanup_task_ = false;
}

void BaseAudioContext::ScheduleMainThreadCleanup() {
  if (has_posted_cleanup_task_)
    return;
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBind(&BaseAudioContext::PerformCleanupOnMainThread,
                      WrapCrossThreadPersistent(this)));
  has_posted_cleanup_task_ = true;
}

void BaseAudioContext::ResolvePromisesForUnpause() {
  // This runs inside the BaseAudioContext's lock when handling pre-render
  // tasks.
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  // Resolve any pending promises created by resume(). Only do this if we
  // haven't already started resolving these promises. This gets called very
  // often and it takes some time to resolve the promises in the main thread.
  if (!is_resolving_resume_promises_ && resume_resolvers_.size() > 0) {
    is_resolving_resume_promises_ = true;
    ScheduleMainThreadCleanup();
  }
}

void BaseAudioContext::RejectPendingDecodeAudioDataResolvers() {
  // Now reject any pending decodeAudioData resolvers
  for (auto& resolver : decode_audio_resolvers_)
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          "Audio context is going away"));
  decode_audio_resolvers_.clear();
}

AudioIOPosition BaseAudioContext::OutputPosition() {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(this);
  return output_position_;
}

void BaseAudioContext::RejectPendingResolvers() {
  DCHECK(IsMainThread());

  // Audio context is closing down so reject any resume promises that are still
  // pending.

  for (auto& resolver : resume_resolvers_) {
    resolver->Reject(DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                          "Audio context is going away"));
  }
  resume_resolvers_.clear();
  is_resolving_resume_promises_ = false;

  RejectPendingDecodeAudioDataResolvers();
}

const AtomicString& BaseAudioContext::InterfaceName() const {
  return EventTargetNames::AudioContext;
}

ExecutionContext* BaseAudioContext::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void BaseAudioContext::StartRendering() {
  // This is called for both online and offline contexts.  The caller
  // must set the context state appropriately. In particular, resuming
  // a context should wait until the context has actually resumed to
  // set the state.
  DCHECK(IsMainThread());
  DCHECK(destination_node_);

  if (context_state_ == kSuspended) {
    destination()->GetAudioDestinationHandler().StartRendering();
  }
}

void BaseAudioContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(destination_node_);
  visitor->Trace(listener_);
  visitor->Trace(active_source_nodes_);
  visitor->Trace(resume_resolvers_);
  visitor->Trace(decode_audio_resolvers_);
  visitor->Trace(periodic_wave_sine_);
  visitor->Trace(periodic_wave_square_);
  visitor->Trace(periodic_wave_sawtooth_);
  visitor->Trace(periodic_wave_triangle_);
  visitor->Trace(audio_worklet_);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

const SecurityOrigin* BaseAudioContext::GetSecurityOrigin() const {
  if (GetExecutionContext())
    return GetExecutionContext()->GetSecurityOrigin();

  return nullptr;
}

AudioWorklet* BaseAudioContext::audioWorklet() const {
  return audio_worklet_.Get();
}

void BaseAudioContext::NotifyWorkletIsReady() {
  DCHECK(IsMainThread());
  DCHECK(audioWorklet()->IsReady());

  {
    // |audio_worklet_thread_| is constantly peeked by the rendering thread,
    // So we protect it with the graph lock.
    GraphAutoLocker locker(this);

    // At this point, the WorkletGlobalScope must be ready so it is safe to keep
    // the reference to the AudioWorkletThread for the future worklet operation.
    audio_worklet_thread_ =
        audioWorklet()->GetMessagingProxy()->GetBackingWorkerThread();
  }

  // If the context is running or suspended, restart the destination to switch
  // the render thread with the worklet thread. Note that restarting can happen
  // right after the context construction.
  if (ContextState() != kClosed) {
    destination()->GetAudioDestinationHandler().RestartRendering();
  }
}

void BaseAudioContext::UpdateWorkletGlobalScopeOnRenderingThread() {
  DCHECK(!IsMainThread());

  if (TryLock()) {
    if (audio_worklet_thread_) {
      AudioWorkletGlobalScope* global_scope =
          To<AudioWorkletGlobalScope>(audio_worklet_thread_->GlobalScope());
      DCHECK(global_scope);
      global_scope->SetCurrentFrame(CurrentSampleFrame());
    }

    unlock();
  }
}

}  // namespace blink
