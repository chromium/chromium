// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_timestamp.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextlatencycategory_double.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_sink_info.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

namespace {

BASE_FEATURE(kWebAudioSetSinkEchoCancellation,
             "WebAudioSetSinkEchoCancellation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Number of AudioContexts still alive.  It's incremented when an
// AudioContext is created and decremented when the context is closed.
unsigned hardware_context_count = 0;

// A context ID that is incremented for each context that is created.
// This initializes the internal id for the context.
unsigned context_id = 0;

// When the client does not have enough permission, the outputLatency property
// is quantized by 8ms to reduce the precision for privacy concerns.
constexpr double kOutputLatencyQuatizingFactor = 0.008;

// When the client has enough permission, the outputLatency property gets
// 1ms precision.
constexpr double kOutputLatencyMaxPrecisionFactor = 0.001;

// Operations tracked in the WebAudio.AudioContext.Operation histogram.
enum class AudioContextOperation {
  kCreate,
  kClose,
  kDelete,
  kMaxValue = kDelete
};

void RecordAudioContextOperation(AudioContextOperation operation) {
  base::UmaHistogramEnumeration("WebAudio.AudioContext.Operation", operation);
}

const char* LatencyCategoryToString(
    WebAudioLatencyHint::AudioContextLatencyCategory category) {
  switch (category) {
    case WebAudioLatencyHint::kCategoryInteractive:
      return "interactive";
    case WebAudioLatencyHint::kCategoryBalanced:
      return "balanced";
    case WebAudioLatencyHint::kCategoryPlayback:
      return "playback";
    case WebAudioLatencyHint::kCategoryExact:
      return "exact";
    case WebAudioLatencyHint::kLastValue:
      return "invalid";
  }
}

String GetAudioContextLogString(const WebAudioLatencyHint& latency_hint,
                                absl::optional<float> sample_rate) {
  StringBuilder builder;
  builder.AppendFormat("AudioContext({latency_hint=%s}",
                       LatencyCategoryToString(latency_hint.Category()));
  if (latency_hint.Category() == WebAudioLatencyHint::kCategoryExact) {
    builder.AppendFormat(", {seconds=%.3f}", latency_hint.Seconds());
  }
  if (sample_rate.has_value()) {
    builder.AppendFormat(", {sample_rate=%.0f}", sample_rate.value());
  }
  builder.Append(String(")"));
  return builder.ToString();
}

bool IsAudible(const AudioBus* rendered_data) {
  // Compute the energy in each channel and sum up the energy in each channel
  // for the total energy.
  float energy = 0;

  uint32_t data_size = rendered_data->length();
  for (uint32_t k = 0; k < rendered_data->NumberOfChannels(); ++k) {
    const float* data = rendered_data->Channel(k)->Data();
    float channel_energy;
    vector_math::Vsvesq(data, 1, &channel_energy, data_size);
    energy += channel_energy;
  }

  return energy > 0;
}

using blink::SetSinkIdResolver;

}  // namespace

AudioContext* AudioContext::Create(ExecutionContext* context,
                                   const AudioContextOptions* context_options,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  if (!window.GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create AudioContext on a detached document.");
    return nullptr;
  }

  window.CountUseOnlyInCrossOriginIframe(
      WebFeature::kAudioContextCrossOriginIframe);

  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
  switch (context_options->latencyHint()->GetContentType()) {
    case V8UnionAudioContextLatencyCategoryOrDouble::ContentType::
        kAudioContextLatencyCategory:
      latency_hint =
          WebAudioLatencyHint(context_options->latencyHint()
                                  ->GetAsAudioContextLatencyCategory()
                                  .AsString());
      break;
    case V8UnionAudioContextLatencyCategoryOrDouble::ContentType::kDouble:
      // This should be the requested output latency in seconds, without taking
      // into account double buffering (same as baseLatency).
      latency_hint =
          WebAudioLatencyHint(context_options->latencyHint()->GetAsDouble());

      base::UmaHistogramTimes("WebAudio.AudioContext.latencyHintMilliSeconds",
                              base::Seconds(latency_hint.Seconds()));
  }

  base::UmaHistogramEnumeration(
      "WebAudio.AudioContext.latencyHintCategory", latency_hint.Category(),
      WebAudioLatencyHint::AudioContextLatencyCategory::kLastValue);

  // This value can be `nullopt` when there's no user-provided options.
  absl::optional<float> sample_rate;
  if (context_options->hasSampleRate()) {
    sample_rate = context_options->sampleRate();
  }

  // The empty string means the default audio device.
  auto frame_token = window.GetLocalFrameToken();
  WebAudioSinkDescriptor sink_descriptor(String(""), frame_token);

  if (window.IsSecureContext() && context_options->hasSinkId()) {
    if (context_options->sinkId()->IsString()) {
      sink_descriptor = WebAudioSinkDescriptor(
          context_options->sinkId()->GetAsString(), frame_token);
    } else {
      // Create a descriptor that represents a silent sink device.
      sink_descriptor = WebAudioSinkDescriptor(frame_token);
    }
  }

  // Validate options before trying to construct the actual context.
  if (sample_rate.has_value() &&
      !audio_utilities::IsValidAudioBufferSampleRate(sample_rate.value())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "hardware sample rate", sample_rate.value(),
            audio_utilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("WebAudio.AudioContext.CreateTime");
  AudioContext* audio_context = MakeGarbageCollected<AudioContext>(
      window, latency_hint, sample_rate, sink_descriptor);
  ++hardware_context_count;
  audio_context->UpdateStateIfNeeded();

  // This starts the audio thread. The destination node's
  // provideInput() method will now be called repeatedly to render
  // audio.  Each time provideInput() is called, a portion of the
  // audio stream is rendered. Let's call this time period a "render
  // quantum". NOTE: for now AudioContext does not need an explicit
  // startRendering() call from JavaScript.  We may want to consider
  // requiring it for symmetry with OfflineAudioContext.
  audio_context->MaybeAllowAutoplayWithUnlockType(
      AutoplayUnlockType::kContextConstructor);
  if (audio_context->IsAllowedToStart()) {
    audio_context->StartRendering();
    audio_context->SetContextState(kRunning);
  }
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::AudioContext(): %u #%u\n",
          audio_context, audio_context->context_id_, hardware_context_count);
#endif

  base::UmaHistogramSparse("WebAudio.AudioContext.MaxChannelsAvailable",
                           audio_context->destination()->maxChannelCount());

  probe::DidCreateAudioContext(&window);

  return audio_context;
}

AudioContext::AudioContext(LocalDOMWindow& window,
                           const WebAudioLatencyHint& latency_hint,
                           absl::optional<float> sample_rate,
                           WebAudioSinkDescriptor sink_descriptor)
    : BaseAudioContext(&window, kRealtimeContext),
      context_id_(context_id++),
      audio_context_manager_(&window),
      permission_service_(&window),
      permission_receiver_(this, &window),
      sink_descriptor_(sink_descriptor),
      v8_sink_id_(
          MakeGarbageCollected<V8UnionAudioSinkInfoOrString>(String(""))),
      media_device_service_(&window),
      media_device_service_receiver_(this, &window) {
  RecordAudioContextOperation(AudioContextOperation::kCreate);
  SendLogMessage(GetAudioContextLogString(latency_hint, sample_rate));

  // TODO(http://crbug.com/1410553) update the echo cancellation reference
  // if the client explicitly specified the sink and there are no issuess
  // accessing it.
  destination_node_ = RealtimeAudioDestinationNode::Create(
      this, sink_descriptor_, latency_hint, sample_rate);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      CHECK(window.document());
      if (window.document()->IsPrerendering()) {
        // In prerendering, the AudioContext will not start even if the
        // AutoplayPolicy permits it. the context will resume automatically
        // once the page is activated. See:
        // https://wicg.github.io/nav-speculation/prerendering.html#web-audio-patch
        autoplay_status_ = AutoplayStatus::kFailed;
        blocked_by_prerendering_ = true;
        window.document()->AddPostPrerenderingActivationStep(
            WTF::BindOnce(&AudioContext::ResumeOnPrerenderActivation,
                          WrapWeakPersistent(this)));
      }
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
      // kUserGestureRequire policy only applies to cross-origin iframes for Web
      // Audio.
      if (window.GetFrame() &&
          window.GetFrame()->IsCrossOriginToOutermostMainFrame()) {
        autoplay_status_ = AutoplayStatus::kFailed;
        user_gesture_required_ = true;
      }
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      autoplay_status_ = AutoplayStatus::kFailed;
      user_gesture_required_ = true;
      break;
  }

  Initialize();

  // Compute the base latency now and cache the value since it doesn't change
  // once the context is constructed.  We need the destination to be initialized
  // so we have to compute it here.
  //
  // TODO(hongchan): Due to the incompatible constructor between
  // AudioDestinationNode and RealtimeAudioDestinationNode, casting directly
  // from `destination()` is impossible. This is a temporary workaround until
  // the refactoring is completed.
  RealtimeAudioDestinationHandler& destination_handler =
      static_cast<RealtimeAudioDestinationHandler&>(
          destination()->GetAudioDestinationHandler());
  base_latency_ = destination_handler.GetFramesPerBuffer() /
                  static_cast<double>(sampleRate());
  SendLogMessage(String::Format("%s => (base latency=%.3f seconds))", __func__,
                                base_latency_));

  // Perform the initial permission check for the output latency precision.
  auto microphone_permission_name = mojom::blink::PermissionName::AUDIO_CAPTURE;
  ConnectToPermissionService(&window,
                             permission_service_.BindNewPipeAndPassReceiver(
                                 window.GetTaskRunner(TaskType::kPermission)));
  permission_service_->HasPermission(
      CreatePermissionDescriptor(microphone_permission_name),
      WTF::BindOnce(&AudioContext::DidInitialPermissionCheck,
                    WrapPersistent(this),
                    CreatePermissionDescriptor(microphone_permission_name)));

  // Initializes MediaDeviceService and `output_device_ids_` only for a valid
  // device identifier that is not the default sink or a silent sink.
  if (sink_descriptor_.Type() ==
          WebAudioSinkDescriptor::AudioSinkType::kAudible &&
      !sink_descriptor_.SinkId().IsEmpty()) {
    InitializeMediaDeviceService();
  }

  // Initializes `v8_sink_id_` with the given `sink_descriptor_`.
  UpdateV8SinkId();
}

void AudioContext::Uninitialize() {
  DCHECK(IsMainThread());
  DCHECK_NE(hardware_context_count, 0u);
  SendLogMessage(String::Format("%s", __func__));
  --hardware_context_count;
  StopRendering();
  DidClose();
  RecordAutoplayMetrics();
  UninitializeMediaDeviceService();
  BaseAudioContext::Uninitialize();
}

AudioContext::~AudioContext() {
  RecordAudioContextOperation(AudioContextOperation::kDelete);

  // TODO(crbug.com/945379) Disable this DCHECK for now.  It's not terrible if
  // the autoplay metrics aren't recorded in some odd situations.  haraken@ said
  // that we shouldn't get here without also calling `Uninitialize()`, but it
  // can happen.  Until that is fixed, disable this DCHECK.

  // DCHECK(!autoplay_status_.has_value());
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::~AudioContext(): %u\n", this,
          context_id_);
#endif
}

void AudioContext::Trace(Visitor* visitor) const {
  visitor->Trace(close_resolver_);
  visitor->Trace(audio_context_manager_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_receiver_);
  visitor->Trace(set_sink_id_resolvers_);
  visitor->Trace(media_device_service_);
  visitor->Trace(media_device_service_receiver_);
  visitor->Trace(v8_sink_id_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise AudioContext::suspendContext(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (ContextState() == kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot suspend a closed AudioContext."));
  }

  suspended_by_user_ = true;

  // Stop rendering now.
  if (destination()) {
    SuspendRendering();
  }

  // Probe reports the suspension only when the promise is resolved.
  probe::DidSuspendAudioContext(GetExecutionContext());

  // Since we don't have any way of knowing when the hardware actually stops,
  // we'll just resolve the promise now.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise AudioContext::resumeContext(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (ContextState() == kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot resume a closed AudioContext."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  // If we're already running, just resolve; nothing else needs to be done.
  if (ContextState() == kRunning) {
    resolver->Resolve();
    return promise;
  }

  suspended_by_user_ = false;

  // Restart the destination node to pull on the audio graph.
  if (destination()) {
    MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kContextResume);
    if (IsAllowedToStart()) {
      // Do not set the state to running here.  We wait for the
      // destination to start to set the state.
      StartRendering();

      // Probe reports only when the user gesture allows the audio rendering.
      probe::DidResumeAudioContext(GetExecutionContext());
    }
  }

  // Save the resolver which will get resolved when the destination node starts
  // pulling on the graph again.
  {
    GraphAutoLocker locker(this);
    resume_resolvers_.push_back(resolver);
  }

  return promise;
}

bool AudioContext::IsPullingAudioGraph() const {
  DCHECK(IsMainThread());

  if (!destination()) {
    return false;
  }

  RealtimeAudioDestinationHandler& destination_handler =
      static_cast<RealtimeAudioDestinationHandler&>(
          destination()->GetAudioDestinationHandler());

  // The realtime context is pulling on the audio graph if the realtime
  // destination allows it.
  return destination_handler.IsPullingAudioGraphAllowed();
}

AudioTimestamp* AudioContext::getOutputTimestamp(
    ScriptState* script_state) const {
  AudioTimestamp* result = AudioTimestamp::Create();

  DCHECK(IsMainThread());
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    return result;
  }

  if (!destination()) {
    result->setContextTime(0.0);
    result->setPerformanceTime(0.0);
    return result;
  }

  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  AudioIOPosition position = OutputPosition();

  // The timestamp of what is currently being played (contextTime) cannot be
  // later than what is being rendered. (currentTime)
  if (position.position > currentTime()) {
    position.position = currentTime();
  }

  double performance_time = performance->MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks() + base::Seconds(position.timestamp));
  if (performance_time < 0.0) {
    performance_time = 0.0;
  }

  result->setContextTime(position.position);
  result->setPerformanceTime(performance_time);
  return result;
}

ScriptPromise AudioContext::closeContext(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  if (ContextState() == kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot close a closed AudioContext."));
  }

  close_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = close_resolver_->Promise();

  // Stops the rendering, but it doesn't release the resources here.
  StopRendering();

  // The promise from closing context resolves immediately after this function.
  DidClose();

  probe::DidCloseAudioContext(GetExecutionContext());
  RecordAudioContextOperation(AudioContextOperation::kClose);

  return promise;
}

void AudioContext::DidClose() {
  SetContextState(kClosed);

  if (close_resolver_) {
    close_resolver_->Resolve();
  }

  // Reject all pending resolvers for setSinkId() before closing AudioContext.
  for (auto& set_sink_id_resolver : set_sink_id_resolvers_) {
    set_sink_id_resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Cannot resolve pending promise from setSinkId(), AudioContext is "
        "going away"));
  }
  set_sink_id_resolvers_.clear();
}

bool AudioContext::IsContextCleared() const {
  return close_resolver_ || BaseAudioContext::IsContextCleared();
}

void AudioContext::StartRendering() {
  DCHECK(IsMainThread());
  SendLogMessage(String::Format("%s", __func__));

  if (!keep_alive_) {
    keep_alive_ = this;
  }
  BaseAudioContext::StartRendering();
}

void AudioContext::StopRendering() {
  DCHECK(IsMainThread());
  DCHECK(destination());
  SendLogMessage(String::Format("%s", __func__));

  // It is okay to perform the following on a suspended AudioContext because
  // this method gets called from ExecutionContext::ContextDestroyed() meaning
  // the AudioContext is already unreachable from the user code.
  if (ContextState() != kClosed) {
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(kClosed);
    GetDeferredTaskHandler().ClearHandlersToBeDeleted();
    keep_alive_.Clear();
  }
}

void AudioContext::SuspendRendering() {
  DCHECK(IsMainThread());
  DCHECK(destination());
  SendLogMessage(String::Format("%s", __func__));

  if (ContextState() == kRunning) {
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(kSuspended);
  }
}

double AudioContext::baseLatency() const {
  DCHECK(IsMainThread());
  DCHECK(destination());

  return base_latency_;
}

double AudioContext::outputLatency() const {
  DCHECK(IsMainThread());
  DCHECK(destination());

  GraphAutoLocker locker(this);

  double factor = GetOutputLatencyQuantizingFactor();
  return std::round(output_position_.hardware_output_latency / factor) * factor;
}

ScriptPromise AudioContext::setSinkId(
    ScriptState* script_state,
    const V8UnionAudioSinkOptionsOrString* v8_sink_id,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  TRACE_EVENT0("webaudio", "AudioContext::setSinkId");

  // setSinkId invoked from a detached document should throw kInvalidStateError
  // DOMException.
  if (!GetExecutionContext()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot proceed setSinkId on a detached document."));
  }

  // setSinkId invoked from a closed AudioContext should throw
  // kInvalidStateError DOMException.
  if (ContextState() == kClosed) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Cannot proceed setSinkId on a closed AudioContext."));
  }

  SetSinkIdResolver* resolver =
      SetSinkIdResolver::Create(script_state, *this, *v8_sink_id);
  ScriptPromise promise = resolver->Promise();

  set_sink_id_resolvers_.push_back(resolver);

  // Lazily initializes MediaDeviceService upon setSinkId() call.
  if (!is_media_device_service_initialized_) {
    InitializeMediaDeviceService();
  } else {
    // MediaDeviceService is initialized, so we can start a resolver if it is
    // the only request in the queue.
    if (set_sink_id_resolvers_.size() == 1) {
      resolver->Start();
    }
  }

  return promise;
}

MediaElementAudioSourceNode* AudioContext::createMediaElementSource(
    HTMLMediaElement* media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaElementAudioSourceNode::Create(*this, *media_element,
                                             exception_state);
}

MediaStreamAudioSourceNode* AudioContext::createMediaStreamSource(
    MediaStream* media_stream,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaStreamAudioSourceNode::Create(*this, *media_stream,
                                            exception_state);
}

MediaStreamAudioDestinationNode* AudioContext::createMediaStreamDestination(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Set number of output channels to stereo by default.
  return MediaStreamAudioDestinationNode::Create(*this, 2, exception_state);
}

void AudioContext::NotifySourceNodeStart() {
  DCHECK(IsMainThread());

  // Do nothing when the context is already closed. (crbug.com/1292101)
  if (ContextState() == AudioContextState::kClosed) {
    return;
  }

  source_node_started_ = true;
  if (!user_gesture_required_) {
    return;
  }

  MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kSourceNodeStart);

  if (ContextState() == AudioContextState::kSuspended && !suspended_by_user_ &&
      IsAllowedToStart()) {
    StartRendering();
    SetContextState(kRunning);
  }
}

AutoplayPolicy::Type AudioContext::GetAutoplayPolicy() const {
  LocalDOMWindow* window = GetWindow();
  DCHECK(window);

  auto autoplay_policy =
      AutoplayPolicy::GetAutoplayPolicyForDocument(*window->document());

  if (autoplay_policy ==
          AutoplayPolicy::Type::kDocumentUserActivationRequired &&
      RuntimeEnabledFeatures::AutoplayIgnoresWebAudioEnabled()) {
// When ignored, the policy is different on Android compared to Desktop.
#if BUILDFLAG(IS_ANDROID)
    return AutoplayPolicy::Type::kUserGestureRequired;
#else
    // Force no user gesture required on desktop.
    return AutoplayPolicy::Type::kNoUserGestureRequired;
#endif
  }

  return autoplay_policy;
}

bool AudioContext::AreAutoplayRequirementsFulfilled() const {
  DCHECK(GetWindow());

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      return true;
    case AutoplayPolicy::Type::kUserGestureRequired:
      return LocalFrame::HasTransientUserActivation(GetWindow()->GetFrame());
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      return AutoplayPolicy::IsDocumentAllowedToPlay(*GetWindow()->document());
  }

  NOTREACHED();
  return false;
}

void AudioContext::MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType type) {
  if (!user_gesture_required_ || !AreAutoplayRequirementsFulfilled()) {
    return;
  }

  DCHECK(!autoplay_status_.has_value() ||
         autoplay_status_ != AutoplayStatus::kSucceeded);

  user_gesture_required_ = false;
  autoplay_status_ = AutoplayStatus::kSucceeded;

  DCHECK(!autoplay_unlock_type_.has_value());
  autoplay_unlock_type_ = type;
}

bool AudioContext::IsAllowedToStart() const {
  if (blocked_by_prerendering_) {
    // In prerendering, the AudioContext will not start rendering. See:
    // https://wicg.github.io/nav-speculation/prerendering.html#web-audio-patch
    return false;
  }

  if (!user_gesture_required_) {
    return true;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  DCHECK(window);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      NOTREACHED();
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
      DCHECK(window->GetFrame());
      DCHECK(window->GetFrame()->IsCrossOriginToOutermostMainFrame());
      window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) from a user gesture event handler. https://goo.gl/7K7WLu"));
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) after a user gesture on the page. https://goo.gl/7K7WLu"));
      break;
  }

  return false;
}

void AudioContext::RecordAutoplayMetrics() {
  if (!autoplay_status_.has_value() || !GetWindow()) {
    return;
  }

  ukm::UkmRecorder* ukm_recorder = GetWindow()->UkmRecorder();
  DCHECK(ukm_recorder);
  ukm::builders::Media_Autoplay_AudioContext(GetWindow()->UkmSourceID())
      .SetStatus(static_cast<int>(autoplay_status_.value()))
      .SetUnlockType(autoplay_unlock_type_
                         ? static_cast<int>(autoplay_unlock_type_.value())
                         : -1)
      .SetSourceNodeStarted(source_node_started_)
      .Record(ukm_recorder);

  // Record autoplay_status_ value.
  base::UmaHistogramEnumeration("WebAudio.Autoplay", autoplay_status_.value());

  if (GetWindow()->GetFrame() &&
      GetWindow()->GetFrame()->IsCrossOriginToOutermostMainFrame()) {
    base::UmaHistogramEnumeration("WebAudio.Autoplay.CrossOrigin",
                                  autoplay_status_.value());
  }

  autoplay_status_.reset();

  // Record autoplay_unlock_type_ value.
  if (autoplay_unlock_type_.has_value()) {
    base::UmaHistogramEnumeration("WebAudio.Autoplay.UnlockType",
                                  autoplay_unlock_type_.value());

    autoplay_unlock_type_.reset();
  }
}

void AudioContext::ContextDestroyed() {
  permission_receiver_.reset();
  Uninitialize();
}

bool AudioContext::HasPendingActivity() const {
  // There's activity if the context is is not closed.  Suspended contexts count
  // as having activity even though they are basically idle with nothing going
  // on.  However, they can be resumed at any time, so we don't want contexts
  // going away prematurely.
  return
      ((ContextState() != kClosed) && BaseAudioContext::HasPendingActivity()) ||
      permission_receiver_.is_bound();
}

bool AudioContext::HandlePreRenderTasks(const AudioIOPosition* output_position,
                                        const AudioCallbackMetric* metric) {
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

    // Update the dirty state of the AudioListenerHandler.
    listener()->Handler().UpdateState();

    // Update output timestamp and metric.
    output_position_ = *output_position;
    callback_metric_ = *metric;

    unlock();
  }

  // Realtime context ignores the return result, but return true, just in case.
  return true;
}

void AudioContext::NotifyAudibleAudioStarted() {
  DCHECK(IsMainThread());

  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextAudiblePlaybackStarted(context_id_);
  }
}

void AudioContext::HandlePostRenderTasks() {
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
}

void AudioContext::HandleAudibility(AudioBus* destination_bus) {
  DCHECK(IsAudioThread());

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
          CrossThreadBindOnce(&AudioContext::NotifyAudibleAudioStarted,
                              WrapCrossThreadPersistent(this)));
    } else {
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioContext::NotifyAudibleAudioStopped,
                              WrapCrossThreadPersistent(this)));
    }
  }
}

void AudioContext::ResolvePromisesForUnpause() {
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

AudioIOPosition AudioContext::OutputPosition() const {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(this);
  return output_position_;
}

void AudioContext::NotifyAudibleAudioStopped() {
  DCHECK(IsMainThread());

  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextAudiblePlaybackStopped(context_id_);
  }
}

void AudioContext::EnsureAudioContextManagerService() {
  if (audio_context_manager_.is_bound() || !GetWindow()) {
    return;
  }

  GetWindow()->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      mojo::GenericPendingReceiver(
          audio_context_manager_.BindNewPipeAndPassReceiver(
              GetWindow()->GetTaskRunner(TaskType::kInternalMedia))));

  audio_context_manager_.set_disconnect_handler(
      WTF::BindOnce(&AudioContext::OnAudioContextManagerServiceConnectionError,
                    WrapWeakPersistent(this)));
}

void AudioContext::OnAudioContextManagerServiceConnectionError() {
  audio_context_manager_.reset();
}

void AudioContext::SendLogMessage(const String& message) {
  WebRtcLogMessage(String::Format("[WA]AC::%s [state=%s]",
                                  message.Utf8().c_str(),
                                  state().Utf8().c_str())
                       .Utf8());
}

AudioCallbackMetric AudioContext::GetCallbackMetric() const {
  // Return a copy under the graph lock because returning a reference would
  // allow seeing the audio thread changing the struct values. This method
  // gets called once per second and the size of the struct is small, so
  // creating a copy is acceptable here.
  GraphAutoLocker locker(this);
  return callback_metric_;
}

void AudioContext::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  microphone_permission_status_ = status;
}

void AudioContext::DidInitialPermissionCheck(
    mojom::blink::PermissionDescriptorPtr descriptor,
    mojom::blink::PermissionStatus status) {
  if (descriptor->name == mojom::blink::PermissionName::AUDIO_CAPTURE &&
      status == mojom::blink::PermissionStatus::GRANTED) {
    // If the initial permission check is successful, the current implementation
    // avoids listening the future permission change in this AudioContext's
    // lifetime. This is acceptable because the current UI pattern asks to
    // reload the page when the permission is taken away.
    microphone_permission_status_ = status;
    permission_receiver_.reset();
    return;
  }

  // The initial permission check failed, start listening the future permission
  // change.
  DCHECK(permission_service_.is_bound());
  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      GetExecutionContext()->GetTaskRunner(TaskType::kPermission));
  permission_service_->AddPermissionObserver(
      CreatePermissionDescriptor(mojom::blink::PermissionName::AUDIO_CAPTURE),
      microphone_permission_status_,
      std::move(observer));
}

double AudioContext::GetOutputLatencyQuantizingFactor() const {
  return microphone_permission_status_ ==
      mojom::blink::PermissionStatus::GRANTED
      ? kOutputLatencyMaxPrecisionFactor
      : kOutputLatencyQuatizingFactor;
}

void AudioContext::NotifySetSinkIdBegins() {
  DCHECK(IsMainThread());

  // This performs step 5 to 9 from the second part of setSinkId() algorithm:
  // https://webaudio.github.io/web-audio-api/#dom-audiocontext-setsinkid-domstring-or-audiosinkoptions-sinkid
  sink_transition_flag_was_running_ = ContextState() != kSuspended;
  destination()->GetAudioDestinationHandler().StopRendering();
  if (sink_transition_flag_was_running_) {
    SetContextState(kSuspended);
  }
}

void AudioContext::NotifySetSinkIdIsDone(
    WebAudioSinkDescriptor pending_sink_descriptor) {
  DCHECK(IsMainThread());

  sink_descriptor_ = pending_sink_descriptor;
  if (sink_descriptor_.Type() ==
          WebAudioSinkDescriptor::AudioSinkType::kAudible &&
      base::FeatureList::IsEnabled(kWebAudioSetSinkEchoCancellation)) {
    // Note: in order to not break echo cancellation of PeerConnection audio, we
    // are heavily relying on the fact that setSinkId() path of AudioContext is
    // not triggered unless the sink ID is explicitly specified. It assumes we
    // don't end up here when AudioContext is being created with the default
    // device.
    if (auto* execution_context = GetExecutionContext()) {
      PeerConnectionDependencyFactory::From(*execution_context)
          .GetWebRtcAudioDevice()
          ->SetOutputDeviceForAec(sink_descriptor_.SinkId());
    }
  }

  // This performs steps 11 and 12 from the second part of the setSinkId()
  // algorithm:
  // https://webaudio.github.io/web-audio-api/#dom-audiocontext-setsinkid-domstring-or-audiosinkoptions-sinkid
  UpdateV8SinkId();
  DispatchEvent(*Event::Create(event_type_names::kSinkchange));
  if (sink_transition_flag_was_running_) {
    SetContextState(kRunning);
    sink_transition_flag_was_running_ = false;
  }
}

void AudioContext::InitializeMediaDeviceService() {
  auto* execution_context = GetExecutionContext();

  execution_context->GetBrowserInterfaceBroker().GetInterface(
      media_device_service_.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kInternalMediaRealTime)));

  media_device_service_->AddMediaDevicesListener(
      /* audio input */ true,
      /* video input */ false,
      /* audio output */ true,
      media_device_service_receiver_.BindNewPipeAndPassRemote(
          execution_context->GetTaskRunner(TaskType::kInternalMediaRealTime)));

  is_media_device_service_initialized_ = true;

  media_device_service_->EnumerateDevices(
      /* audio input */ false,
      /* video input */ false,
      /* audio output */ true,
      /* request_video_input_capabilities */ false,
      /* request_audio_input_capabilities */ false,
      WTF::BindOnce(&AudioContext::DevicesEnumerated,
                    WrapWeakPersistent(this)));
}

void AudioContext::DevicesEnumerated(
    const Vector<Vector<WebMediaDeviceInfo>>& enumeration,
    Vector<mojom::blink::VideoInputDeviceCapabilitiesPtr>
        video_input_capabilities,
    Vector<mojom::blink::AudioInputDeviceCapabilitiesPtr>
        audio_input_capabilities) {
  Vector<WebMediaDeviceInfo> output_devices =
      enumeration[static_cast<wtf_size_t>(
          mojom::blink::MediaDeviceType::kMediaAudioOuput)];

  TRACE_EVENT1(
      "webaudio", "AudioContext::DevicesEnumerated", "DeviceEnumeration",
      audio_utilities::GetDeviceEnumerationForTracing(output_devices));

  OnDevicesChanged(mojom::blink::MediaDeviceType::kMediaAudioOuput,
                   output_devices);

  // Start the first resolver in the queue once `output_device_ids_` is
  // initialized from `OnDeviceChanged()` above.
  if (!set_sink_id_resolvers_.empty()) {
    set_sink_id_resolvers_.front()->Start();
  }
}

void AudioContext::OnDevicesChanged(mojom::blink::MediaDeviceType device_type,
                                    const Vector<WebMediaDeviceInfo>& devices) {
  if (device_type == mojom::blink::MediaDeviceType::kMediaAudioOuput) {
    output_device_ids_.clear();
    for (auto device : devices) {
      if (device.device_id == "default") {
        // Use the empty string to represent the default audio sink.
        output_device_ids_.insert(String(""));
      } else {
        output_device_ids_.insert(String::FromUTF8(device.device_id));
      }
    }
  }

  // On some platforms, unplugging the current audio device doesn't
  // automatically fallback to the default audio device. When the current
  // `sink_descriptor_` becomes invalid here, we need to manually call
  // `SetSinkDescriptor()` to fallback to the default audio output
  // device to keep the audio playing.
  if (!IsValidSinkDescriptor(sink_descriptor_)) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kInfo,
            "[AudioContext] Fallback to the default device due to an invalid"
            " audio device change. ("
            + String(sink_descriptor_.SinkId().Utf8()) + ")"));
    sink_descriptor_ = WebAudioSinkDescriptor(
        String(""),
        To<LocalDOMWindow>(GetExecutionContext())->GetLocalFrameToken());
    auto* destination_node = destination();
    if (destination_node) {
      static_cast<RealtimeAudioDestinationNode*>(destination_node)
          ->SetSinkDescriptor(sink_descriptor_, base::DoNothing());
    }
    UpdateV8SinkId();
  }
}

void AudioContext::UninitializeMediaDeviceService() {
  if (media_device_service_.is_bound()) {
    media_device_service_.reset();
  }
  if (media_device_service_receiver_.is_bound()) {
    media_device_service_receiver_.reset();
  }
  output_device_ids_.clear();
}

void AudioContext::UpdateV8SinkId() {
  if (sink_descriptor_.Type() ==
      WebAudioSinkDescriptor::AudioSinkType::kSilent) {
    v8_sink_id_->Set(AudioSinkInfo::Create(String("none")));
  } else {
    v8_sink_id_->Set(sink_descriptor_.SinkId());
  }
}

bool AudioContext::IsValidSinkDescriptor(
    const WebAudioSinkDescriptor& sink_descriptor) {
  return sink_descriptor.Type() ==
             WebAudioSinkDescriptor::AudioSinkType::kSilent ||
         output_device_ids_.Contains(sink_descriptor.SinkId());
}

void AudioContext::ResumeOnPrerenderActivation() {
  CHECK(blocked_by_prerendering_);
  blocked_by_prerendering_ = false;
  switch (ContextState()) {
    case kSuspended:
      StartRendering();
      break;
    case kRunning:
      NOTREACHED_NORETURN();
    case kClosed:
      break;
  }
}

}  // namespace blink
