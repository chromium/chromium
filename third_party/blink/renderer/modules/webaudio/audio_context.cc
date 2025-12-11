// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include <atomic>

#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/base/output_device_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mediastream/media_devices.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_context_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_timestamp.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextlatencycategory_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiocontextrendersizecategory_unsignedlong.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_audiosinkinfo_string.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_playout_stats.h"
#include "third_party/blink/renderer/modules/webaudio/audio_sink_info.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/media/player_id_generator.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

namespace {

// Number of AudioContexts still alive.  It's incremented when an
// AudioContext is created and decremented when the context is closed.
unsigned hardware_context_count = 0;

// A context ID that is incremented for each context that is created.
// This initializes the internal id for the context.
uint32_t context_id = 0;

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AudioContextInterruptionType {
  kPlayAttemptWhileFrameHidden = 0,
  kFrameHiddenWhilePlaying = 1,
  kMaxValue = kFrameHiddenWhilePlaying
};

void RecordAudioContextOperation(AudioContextOperation operation) {
  base::UmaHistogramEnumeration("WebAudio.AudioContext.Operation", operation);
}

void RecordMediaPlaybackInterruptionType(AudioContextInterruptionType type) {
  base::UmaHistogramEnumeration(
      "WebAudio.AudioContext.MediaPlaybackWhileNotVisible.InterruptionType",
      type);
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
                                std::optional<float> sample_rate) {
  StringBuilder builder;
  builder.AppendFormat("({latency_hint=%s}",
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

}  // namespace

// Helper class that decides if the AudioPlayoutStats should be updated.
// It implements Privacy & Security mitigations as described here:
// https://wicg.github.io/audio-context-playout-stats/#mitigations
class AudioContext::StatsUpdateRestrictor {
 public:
  StatsUpdateRestrictor() : clock_(base::DefaultTickClock::GetInstance()) {}

  // Should only be called from the audio thread.
  bool StatUpdateAllowed() {
    // Stats should only be updated if the page is visible or the application
    // has audio capture permission.
    if (!(visible_.load(std::memory_order_relaxed) ||
          has_capture_permission_.load(std::memory_order_relaxed))) {
      return false;
    }

    static const base::TimeDelta kMinTimeBetweenStatUpdates = base::Seconds(1);
    base::TimeTicks now_time = clock_->NowTicks();
    if (now_time - last_update_time_ < kMinTimeBetweenStatUpdates) {
      return false;
    }

    last_update_time_ = now_time;
    return true;
  }

  void SetVisible(bool visible) {
    visible_.store(visible, std::memory_order_relaxed);
  }

  void SetCapturePermission(mojom::blink::PermissionStatus status) {
    has_capture_permission_.store(
        status == mojom::blink::PermissionStatus::GRANTED,
        std::memory_order_relaxed);
  }

  void SetClockForTesting(const base::TickClock* clock) { clock_ = clock; }

 private:
  // Only accessed on the audio thread.
  base::TimeTicks last_update_time_;
  raw_ptr<const base::TickClock> clock_;

  // `visible_` and `has_capture_permission_` are read on the audio thread and
  // set on the main thread, so they need to be atomic.
  std::atomic<bool> visible_ = false;
  std::atomic<bool> has_capture_permission_ = false;
};

AudioContext::SetSinkIdResolver::SetSinkIdResolver(
    ScriptState* script_state,
    AudioContext& audio_context,
    const V8UnionAudioSinkOptionsOrString& sink_id)
    : audio_context_(audio_context),
      resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state)) {
  DCHECK(IsMainThread());

  // Currently the only available AudioSinkOptions is a type of a silent sink,
  // which can be specified by an empty descriptor constructor.
  auto& frame_token = To<LocalDOMWindow>(audio_context_->GetExecutionContext())
                          ->GetLocalFrameToken();
  if (sink_id.GetContentType() ==
      V8UnionAudioSinkOptionsOrString::ContentType::kAudioSinkOptions) {
    sink_descriptor_ = WebAudioSinkDescriptor(frame_token);
  } else {
    sink_descriptor_ =
        WebAudioSinkDescriptor(sink_id.GetAsString(), frame_token);
  }

  TRACE_EVENT1("webaudio", "SetSinkIdResolver::SetSinkIdResolver",
               "sink_id (after setting sink_descriptor_)",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
}

void AudioContext::SetSinkIdResolver::Trace(Visitor* visitor) const {
  visitor->Trace(audio_context_);
  visitor->Trace(resolver_);
}

void AudioContext::SetSinkIdResolver::Start() {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::Start", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  if (!resolver_ || !resolver_->GetExecutionContext() || !audio_context_ ||
      audio_context_->IsContextCleared()) {
    // No point in rejecting promise, as it will bail out upon detached
    // context anyway.
    return;
  }

  // Refer to
  // https://webaudio.github.io/web-audio-api/#validating-sink-identifier for
  // sink_id/sink_descriptor validation steps.
  if (sink_descriptor_ == audio_context_->GetSinkDescriptor()) {
    OnSetSinkIdComplete(media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK);
  } else if (!audio_context_->IsValidSinkDescriptor(sink_descriptor_)) {
    OnSetSinkIdComplete(
        media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  } else {
    auto* audio_destination = audio_context_->destination();
    // A sanity check to make sure we have valid audio_destination node from
    // `audio_context_`.
    if (!audio_destination) {
      OnSetSinkIdComplete(
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL);
    } else {
      audio_context_->NotifySetSinkIdBegins();
      auto set_sink_id_completion_callback = blink::BindOnce(
          &SetSinkIdResolver::OnSetSinkIdComplete, WrapPersistent(this));
      auto set_sink_descriptor_callback = blink::BindOnce(
          &RealtimeAudioDestinationNode::SetSinkDescriptor,
          WrapWeakPersistent(
              static_cast<RealtimeAudioDestinationNode*>(audio_destination)),
          sink_descriptor_, std::move(set_sink_id_completion_callback));
      audio_context_->GetExecutionContext()
          ->GetTaskRunner(TaskType::kInternalMediaRealTime)
          ->PostTask(FROM_HERE, std::move(set_sink_descriptor_callback));
    }
  }
}

void AudioContext::SetSinkIdResolver::Resolve() {
  DCHECK(IsMainThread());
  DCHECK(resolver_);
  resolver_->Resolve();
  resolver_ = nullptr;
}

void AudioContext::SetSinkIdResolver::Reject(DOMException* exception) {
  DCHECK(IsMainThread());
  DCHECK(resolver_);
  resolver_->Reject(exception);
  resolver_ = nullptr;
}

void AudioContext::SetSinkIdResolver::Reject(v8::Local<v8::Value> value) {
  DCHECK(IsMainThread());
  DCHECK(resolver_);
  resolver_->Reject(value);
  resolver_ = nullptr;
}

ScriptPromise<IDLUndefined> AudioContext::SetSinkIdResolver::GetPromise() {
  DCHECK(IsMainThread());
  DCHECK(resolver_);
  return resolver_->Promise();
}

void AudioContext::SetSinkIdResolver::HandleOutputDeviceStatus(
    media::OutputDeviceStatus status) {
  ScriptState* script_state = resolver_->GetScriptState();
  ScriptState::Scope scope(script_state);
  switch (status) {
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_OK:
      if (audio_context_ && !audio_context_->IsContextCleared()) {
        // Update AudioContext's sink ID and fire the 'onsinkchange' event
        audio_context_->NotifySetSinkIdIsDone(sink_descriptor_);
      }
      Resolve();
      return;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotFoundError,
          StrCat({"AudioContext.setSinkId(): failed: the device ",
                  String(sink_descriptor_.SinkId()), " is not found."})));
      return;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kNotAllowedError,
          StrCat({"AudioContext.setSinkId() failed: access to the device ",
                  String(sink_descriptor_.SinkId()), " is not permitted."})));
      return;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kTimeoutError,
          StrCat({"AudioContext.setSinkId() failed: the request for device ",
                  String(sink_descriptor_.SinkId()), " is timed out."})));
      return;
    case media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL:
      Reject(V8ThrowDOMException::CreateOrEmpty(
          script_state->GetIsolate(), DOMExceptionCode::kInvalidStateError,
          StrCat({"AudioContext.setSinkId() failed: the device ",
                  String(sink_descriptor_.SinkId()), " is not available."})));
      return;
  }
  NOTREACHED();
}

void AudioContext::SetSinkIdResolver::OnSetSinkIdComplete(
    media::OutputDeviceStatus status) {
  TRACE_EVENT1("webaudio", "SetSinkIdResolver::OnSetSinkIdComplete", "sink_id",
               audio_utilities::GetSinkIdForTracing(sink_descriptor_));
  DCHECK(IsMainThread());

  if (!resolver_) {
    return;
  }

  auto* excecution_context = resolver_->GetExecutionContext();
  if (!excecution_context || excecution_context->IsContextDestroyed()) {
    return;
  }

  HandleOutputDeviceStatus(status);

  auto& resolvers = audio_context_->GetSetSinkIdResolver();
  resolvers.pop_front();
  if (!resolvers.empty() && (audio_context_->PendingDeviceListUpdates() == 0)) {
    // Prevent potential stack overflow under heavy load by scheduling the next
    // resolver start asynchronously instead of invoking it directly.
    auto next_start_task = blink::BindOnce(
        &SetSinkIdResolver::Start, WrapWeakPersistent(resolvers.front().Get()));
    audio_context_->GetExecutionContext()
        ->GetTaskRunner(TaskType::kInternalMediaRealTime)
        ->PostTask(FROM_HERE, std::move(next_start_task));
  }
}

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
  std::optional<float> sample_rate;
  if (context_options->hasSampleRate()) {
    sample_rate = context_options->sampleRate();
  }

  std::optional<uint32_t> render_quantum_frames = 128;
  if (RuntimeEnabledFeatures::WebAudioConfigurableRenderQuantumEnabled() &&
      context_options->hasRenderSizeHint()) {
    const auto* hint = context_options->renderSizeHint();
    switch (hint->GetContentType()) {
      case V8UnionAudioContextRenderSizeCategoryOrUnsignedLong::ContentType::
          kUnsignedLong:
        render_quantum_frames = hint->GetAsUnsignedLong();
        break;
      case V8UnionAudioContextRenderSizeCategoryOrUnsignedLong::ContentType::
          kAudioContextRenderSizeCategory:
        if (hint->GetAsAudioContextRenderSizeCategory() ==
            V8AudioContextRenderSizeCategory::Enum::kHardware) {
          // Use `nullopt` to indicate a "hardware" hint.
          render_quantum_frames.reset();
        }
        break;
    }
  }

  // The empty string means the default audio device.
  auto frame_token = window.GetLocalFrameToken();
  WebAudioSinkDescriptor sink_descriptor(g_empty_string, frame_token);
  // In order to not break echo cancellation of PeerConnection audio, we must
  // not update the echo cancellation reference unless the sink ID is explicitly
  // specified.
  bool update_echo_cancellation_on_first_start = false;

  if (window.IsSecureContext() && context_options->hasSinkId()) {
    // Only try to update the echo cancellation reference if `sinkId` was
    // explicitly passed in the `AudioContextOptions` dictionary.
    update_echo_cancellation_on_first_start = true;
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

  // Pre-validation for renderSizeHint if sampleRate is provided. This prevents
  // allocating excessive memory for clearly invalid renderSizeHint values
  // before the AudioContext is fully constructed.
  bool render_quantum_frames_validated = false;
  if (sample_rate.has_value() && render_quantum_frames.has_value()) {
    if (!audio_utilities::IsValidRenderQuantumSize(
            render_quantum_frames.value(), sample_rate.value())) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          ExceptionMessages::IndexOutsideRange(
              "renderSizeHint", render_quantum_frames.value(),
              audio_utilities::MinRenderQuantumSize(),
              ExceptionMessages::kInclusiveBound,
              audio_utilities::MaxRenderQuantumSize(sample_rate.value()),
              ExceptionMessages::kInclusiveBound));
      return nullptr;
    }
    render_quantum_frames_validated = true;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("WebAudio.AudioContext.CreateTime");
  AudioContext* audio_context = MakeGarbageCollected<AudioContext>(
      window, latency_hint, sample_rate, sink_descriptor,
      update_echo_cancellation_on_first_start, render_quantum_frames);
  ++hardware_context_count;
  audio_context->UpdateStateIfNeeded();

  // If the render quantum size was not able to be validated before due to the
  // sample rate or hardware buffer size not being known, validate it here.
  if (!render_quantum_frames_validated &&
      !audio_utilities::IsValidRenderQuantumSize(
          audio_context->renderQuantumSize(), audio_context->sampleRate())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "renderSizeHint", audio_context->renderQuantumSize(),
            audio_utilities::MinRenderQuantumSize(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxRenderQuantumSize(audio_context->sampleRate()),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  // This starts the audio thread. The destination node's
  // provideInput() method will now be called repeatedly to render
  // audio.  Each time provideInput() is called, a portion of the
  // audio stream is rendered. Let's call this time period a "render
  // quantum". NOTE: for now AudioContext does not need an explicit
  // startRendering() call from JavaScript.  We may want to consider
  // requiring it for symmetry with OfflineAudioContext.
  audio_context->MaybeAllowAutoplayWithUnlockType(
      AutoplayUnlockType::kContextConstructor);
  if (audio_context->IsAllowedToStart(/*should_suppress_warning=*/true)) {
    TRACE_EVENT1("webaudio", "AudioContext::Create - allowed to start",
                 "UUID", audio_context->Uuid());
    audio_context->StartRendering();
    audio_context->SetContextState(V8AudioContextState::Enum::kRunning);
  } else {
    TRACE_EVENT1("webaudio", "AudioContext::Create - NOT allowed to start",
                 "UUID", audio_context->Uuid());
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
                           std::optional<float> sample_rate,
                           WebAudioSinkDescriptor sink_descriptor,
                           bool update_echo_cancellation_on_first_start,
                           std::optional<uint32_t> render_quantum_frames)
    : BaseAudioContext(&window,
                       ContextType::kRealtimeContext,
                       render_quantum_frames.value_or(128)),
      FrameVisibilityObserver(GetLocalFrame()),
      PageVisibilityObserver(GetPageFromFrame()),
      context_id_(context_id++),
      audio_context_manager_(&window),
      permission_service_(&window),
      permission_receiver_(this, &window),
      sink_descriptor_(sink_descriptor),
      v8_sink_id_(
          MakeGarbageCollected<V8UnionAudioSinkInfoOrString>(g_empty_string)),
      media_device_service_(&window),
      media_device_service_receiver_(this, &window),
      should_interrupt_when_frame_is_hidden_(!CanPlayWhileHidden()),
      player_id_(GetNextMediaPlayerId()),
      media_player_host_(&window),
      media_player_receiver_(this, &window),
      media_player_observer_(&window),
      stats_update_restrictor_(std::make_unique<StatsUpdateRestrictor>()) {
  RecordAudioContextOperation(AudioContextOperation::kCreate);
  SendLogMessage(__func__, GetAudioContextLogString(latency_hint, sample_rate));

  if (should_interrupt_when_frame_is_hidden_) {
    // The "media-playback-while-not-visible" permission policy default value
    // was overridden, which means that either this frame or an ancestor frame
    // changed the permission policy's default value. This should only happen if
    // the MediaPlaybackWhileNotVisiblePermissionPolicyEnabled runtime flag is
    // enabled.
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kMediaPlaybackWhileNotVisiblePermissionPolicy);
  }

  destination_node_ = RealtimeAudioDestinationNode::Create(
      this, sink_descriptor_, latency_hint, sample_rate,
      update_echo_cancellation_on_first_start);

  TRACE_EVENT2("webaudio", "AudioContext::AudioContext", "UUID", Uuid(),
               "AutoplayPolicy", static_cast<int>(GetAutoplayPolicy()));

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
            blink::BindOnce(&AudioContext::ResumeOnPrerenderActivation,
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
  base_latency_ =
      GetRealtimeAudioDestinationNode()->GetOwnHandler().GetFramesPerBuffer() /
      static_cast<double>(sampleRate());
  SendLogMessage(__func__, String::Format("=> (base latency=%.3f seconds))",
                                          base_latency_));

  // Perform the initial permission check for the output latency precision.
  auto microphone_permission_name = mojom::blink::PermissionName::AUDIO_CAPTURE;
  ConnectToPermissionService(&window,
                             permission_service_.BindNewPipeAndPassReceiver(
                                 window.GetTaskRunner(TaskType::kPermission)));
  permission_service_->HasPermission(
      CreatePermissionDescriptor(microphone_permission_name),
      blink::BindOnce(&AudioContext::DidInitialPermissionCheck,
                      WrapPersistent(this),
                      CreatePermissionDescriptor(microphone_permission_name)));

  // Initializes MediaDeviceService and `output_device_ids_` only for a valid
  // device identifier that is not the default sink or a silent sink.
  if (sink_descriptor_.Type() ==
          WebAudioSinkDescriptor::AudioSinkType::kAudible &&
      !sink_descriptor_.IsDefaultSinkId()) {
    InitializeMediaDeviceService();
  }

  // Initializes `v8_sink_id_` with the given `sink_descriptor_`.
  UpdateV8SinkId();

  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextCreated(context_id_);
  }

  stats_update_restrictor_->SetVisible(GetPage() && GetPage()->IsPageVisible());
}

void AudioContext::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  DCHECK_NE(hardware_context_count, 0u);
  SendLogMessage(__func__, "");
  --hardware_context_count;
  StopRendering();
  DidClose();
  RecordAutoplayMetrics();
  UninitializeMediaDeviceService();
  BaseAudioContext::Uninitialize();
}

AudioContext::~AudioContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  RecordAudioContextOperation(AudioContextOperation::kDelete);

  // If the context is destroyed while still audible, account for the
  // remaining audible time until destruction.
  if (!audible_start_timestamp_.is_null()) {
    total_audible_duration_ +=
        base::TimeTicks::Now() - audible_start_timestamp_;
  }
  UMA_HISTOGRAM_EXACT_LINEAR("WebAudio.AudioContext.AudibleTime",
                             total_audible_duration_.InSeconds(),
                             /*exclusive_max=*/8);
  UMA_HISTOGRAM_BOOLEAN("WebAudio.AudioContext.DestroyedWithoutClose",
                        !is_closed_);

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
  visitor->Trace(audio_playout_stats_);
  visitor->Trace(audio_context_manager_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_receiver_);
  visitor->Trace(set_sink_id_resolvers_);
  visitor->Trace(media_device_service_);
  visitor->Trace(media_device_service_receiver_);
  visitor->Trace(v8_sink_id_);
  visitor->Trace(media_player_host_);
  visitor->Trace(media_player_receiver_);
  visitor->Trace(media_player_observer_);
  BaseAudioContext::Trace(visitor);
  FrameVisibilityObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
}

ScriptPromise<IDLUndefined> AudioContext::suspendContext(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
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
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> AudioContext::resumeContext(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot resume a closed AudioContext."));
  } else if (ContextState() == V8AudioContextState::Enum::kInterrupted) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot resume an interrupted AudioContext."));
  } else if (ContextState() == V8AudioContextState::Enum::kSuspended &&
             is_interrupted_while_suspended_) {
    // When the interruption ends, the context should be in the running state.
    should_transition_to_running_after_interruption_ = true;
    SetContextState(V8AudioContextState::Enum::kInterrupted);
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot resume an interrupted AudioContext."));
  } else if (is_frame_hidden_ && should_interrupt_when_frame_is_hidden_) {
    RecordMediaPlaybackInterruptionType(
        AudioContextInterruptionType::kPlayAttemptWhileFrameHidden);
    StartContextInterruption();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // If we're already running, just resolve; nothing else needs to be done.
  if (ContextState() == V8AudioContextState::Enum::kRunning) {
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
    DeferredTaskHandler::GraphAutoLocker locker(this);
    pending_promises_resolvers_.push_back(resolver);
  }

  return promise;
}

bool AudioContext::IsPullingAudioGraph() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  if (!destination()) {
    return false;
  }

  // The realtime context is pulling on the audio graph if the realtime
  // destination allows it.
  return GetRealtimeAudioDestinationNode()
      ->GetOwnHandler()
      .IsPullingAudioGraphAllowed();
}

AudioTimestamp* AudioContext::getOutputTimestamp(
    ScriptState* script_state) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  AudioTimestamp* result = AudioTimestamp::Create();
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    return result;
  }
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  DeferredTaskHandler::GraphAutoLocker locker(this);
  double performance_time = performance->MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks() + base::Seconds(output_position_.timestamp));
  result->setContextTime(output_position_.position);
  result->setPerformanceTime(performance_time < 0.0 ? 0.0 : performance_time);
  return result;
}

ScriptPromise<IDLUndefined> AudioContext::closeContext(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot close a closed AudioContext."));
  }

  close_resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = close_resolver_->Promise();

  // Stops the rendering, but it doesn't release the resources here.
  StopRendering();

  // The promise from closing context resolves immediately after this function.
  DidClose();

  probe::DidCloseAudioContext(GetExecutionContext());
  RecordAudioContextOperation(AudioContextOperation::kClose);

  return promise;
}

void AudioContext::DidClose() {
  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextClosed(context_id_);
  }

  SetContextState(V8AudioContextState::Enum::kClosed);

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
  is_closed_ = true;
}

bool AudioContext::IsContextCleared() const {
  return close_resolver_ || BaseAudioContext::IsContextCleared();
}

void AudioContext::StartRendering() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  SendLogMessage(__func__, "");
  TRACE_EVENT1("webaudio", __func__, "UUID", Uuid());

  if (!keep_alive_) {
    keep_alive_ = this;
  }
  BaseAudioContext::StartRendering();
}

void AudioContext::StopRendering() {
  DCHECK(destination());
  SendLogMessage(__func__, "");
  TRACE_EVENT1("webaudio", __func__, "UUID", Uuid());

  // It is okay to perform the following on a suspended AudioContext because
  // this method gets called from ExecutionContext::ContextDestroyed() meaning
  // the AudioContext is already unreachable from the user code.
  if (ContextState() != V8AudioContextState::Enum::kClosed) {
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(V8AudioContextState::Enum::kClosed);
    GetDeferredTaskHandler().ClearHandlersToBeDeleted();
    keep_alive_.Clear();
  }
}

void AudioContext::SuspendRendering() {
  DCHECK(destination());
  SendLogMessage(__func__, "");
  TRACE_EVENT2("webaudio", __func__, "UUID", Uuid(),
               "state", static_cast<int>(ContextState()));

  if (ContextState() == V8AudioContextState::Enum::kRunning ||
      ContextState() == V8AudioContextState::Enum::kInterrupted) {
    if (is_interrupted_while_suspended_) {
      should_transition_to_running_after_interruption_ = false;
    }
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(V8AudioContextState::Enum::kSuspended);
  }
}

void AudioContext::SetVolumeMultiplier(double multiplier) {
  volume_multiplier_ = multiplier;
}

double AudioContext::baseLatency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  DCHECK(destination());

  return base_latency_;
}

double AudioContext::outputLatency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  DCHECK(destination());

  DeferredTaskHandler::GraphAutoLocker locker(this);

  double factor = GetOutputLatencyQuantizingFactor();
  return std::round(output_position_.hardware_output_latency / factor) * factor;
}

AudioPlayoutStats* AudioContext::playoutStats() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  if (!audio_playout_stats_) {
    audio_playout_stats_ = MakeGarbageCollected<AudioPlayoutStats>(this);
  }
  return audio_playout_stats_.Get();
}

ScriptPromise<IDLUndefined> AudioContext::setSinkId(
    ScriptState* script_state,
    const V8UnionAudioSinkOptionsOrString* v8_sink_id,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  TRACE_EVENT0("webaudio", "AudioContext::setSinkId");

  // setSinkId invoked from a detached document should throw kInvalidStateError
  // DOMException.
  if (!GetExecutionContext()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot proceed setSinkId on a detached document."));
  }

  // setSinkId invoked from a closed AudioContext should throw
  // kInvalidStateError DOMException.
  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kInvalidStateError,
            "Cannot proceed setSinkId on a closed AudioContext."));
  }

  SetSinkIdResolver* resolver =
      MakeGarbageCollected<SetSinkIdResolver>(script_state, *this, *v8_sink_id);
  auto promise = resolver->GetPromise();

  set_sink_id_resolvers_.push_back(resolver);

  // Lazily initializes MediaDeviceService upon setSinkId() call.
  if (!is_media_device_service_initialized_) {
    InitializeMediaDeviceService();
  } else {
    // MediaDeviceService is initialized, so we can start a resolver if it is
    // the only request in the queue.
    if (set_sink_id_resolvers_.size() == 1 &&
        (pending_device_list_updates_ == 0)) {
      resolver->Start();
    }
  }

  return promise;
}

MediaElementAudioSourceNode* AudioContext::createMediaElementSource(
    HTMLMediaElement* media_element,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  return MediaElementAudioSourceNode::Create(*this, *media_element,
                                             exception_state);
}

MediaStreamAudioSourceNode* AudioContext::createMediaStreamSource(
    MediaStream* media_stream,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  return MediaStreamAudioSourceNode::Create(*this, *media_stream,
                                            exception_state);
}

MediaStreamAudioDestinationNode* AudioContext::createMediaStreamDestination(
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  // Set number of output channels to stereo by default.
  return MediaStreamAudioDestinationNode::Create(*this, 2, exception_state);
}

void AudioContext::NotifySourceNodeStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  // Do nothing when the context is already closed. (crbug.com/1292101)
  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return;
  }

  source_node_started_ = true;
  if (!user_gesture_required_) {
    return;
  }

  MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kSourceNodeStart);

  if (ContextState() == V8AudioContextState::Enum::kSuspended &&
      !suspended_by_user_ && IsAllowedToStart()) {
    StartRendering();
    SetContextState(V8AudioContextState::Enum::kRunning);
  }
}

AutoplayPolicy::Type AudioContext::GetAutoplayPolicy() const {
  LocalDOMWindow* window = GetWindow();
  DCHECK(window);

  return AutoplayPolicy::GetAutoplayPolicyForDocument(*window->document());
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

bool AudioContext::IsAllowedToStart(bool should_suppress_warning) const {
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
    case AutoplayPolicy::Type::kUserGestureRequired:
      DCHECK(window->GetFrame());
      DCHECK(window->GetFrame()->IsCrossOriginToOutermostMainFrame());
      if (!should_suppress_warning) {
        window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "The AudioContext was not allowed to start. It must be resumed (or "
            "created) from a user gesture event handler. "
            "https://developer.chrome.com/blog/autoplay/#web_audio"));
      }
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      if (!should_suppress_warning) {
        window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "The AudioContext was not allowed to start. It must be resumed (or "
            "created) after a user gesture on the page. "
            "https://developer.chrome.com/blog/autoplay/#web_audio"));
      }
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
  return ((ContextState() != V8AudioContextState::Enum::kClosed) &&
          BaseAudioContext::HasPendingActivity()) ||
         permission_receiver_.is_bound();
}

RealtimeAudioDestinationNode* AudioContext::GetRealtimeAudioDestinationNode()
    const {
  return static_cast<RealtimeAudioDestinationNode*>(destination());
}

bool AudioContext::HandlePreRenderTasks(
    uint32_t frames_to_process,
    const AudioIOPosition* output_position,
    const AudioCallbackMetric* metric,
    base::TimeDelta playout_delay,
    const media::AudioGlitchInfo& glitch_info) {
  DCHECK(IsAudioThread());

  pending_audio_frame_stats_.Update(frames_to_process, sampleRate(),
                                    playout_delay, glitch_info);

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

    // Update output timestamp and metric. The timestamp of what is currently
    // being played (e.g. getOutputTimestamp().contextTime) cannot be later
    // than what is being rendered. (AudioContext.currentTime)
    output_position_ = *output_position;
    if (output_position_.position > currentTime()) {
      output_position_.position = currentTime();
    }

    callback_metric_ = *metric;

    if (stats_update_restrictor_->StatUpdateAllowed()) {
      audio_frame_stats_.Absorb(pending_audio_frame_stats_);
    }

    unlock();
  }

  // Realtime context ignores the return result, but return true, just in case.
  return true;
}

void AudioContext::NotifyAudibleAudioStarted() {
  DCHECK(audible_start_timestamp_.is_null());
  audible_start_timestamp_ = base::TimeTicks::Now();

  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextAudiblePlaybackStarted(context_id_);
  }

  EnsureMediaPlayerConnection();
  if (media_player_observer_.is_bound()) {
    media_player_observer_->OnMediaMetadataChanged(
        /*has_audio=*/true, /*has_video=*/false,
        media::MediaContentType::kAmbient);
    media_player_observer_->OnMediaPlaying();
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

void AudioContext::HandleVolumeMultiplier(AudioBus* destination_bus) {
  if (volume_multiplier_ != 1.0) {
    destination_bus->Scale(volume_multiplier_);
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
  if (!is_resolving_resume_promises_ &&
      pending_promises_resolvers_.size() > 0) {
    is_resolving_resume_promises_ = true;
    ScheduleMainThreadCleanup();
  }
}

void AudioContext::NotifyAudibleAudioStopped() {
  DCHECK(!audible_start_timestamp_.is_null());
  total_audible_duration_ += base::TimeTicks::Now() - audible_start_timestamp_;
  audible_start_timestamp_ = base::TimeTicks();

  EnsureAudioContextManagerService();
  if (audio_context_manager_.is_bound()) {
    audio_context_manager_->AudioContextAudiblePlaybackStopped(context_id_);
  }

  EnsureMediaPlayerConnection();
  if (media_player_observer_.is_bound()) {
    media_player_observer_->OnMediaMetadataChanged(
        /*has_audio=*/false, /*has_video=*/false,
        media::MediaContentType::kAmbient);
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

  audio_context_manager_.set_disconnect_handler(blink::BindOnce(
      &AudioContext::OnAudioContextManagerServiceConnectionError,
      WrapWeakPersistent(this)));
}

void AudioContext::OnAudioContextManagerServiceConnectionError() {
  audio_context_manager_.reset();
}

AudioCallbackMetric AudioContext::GetCallbackMetric() const {
  // Return a copy under the graph lock because returning a reference would
  // allow seeing the audio thread changing the struct values. This method
  // gets called once per second and the size of the struct is small, so
  // creating a copy is acceptable here.
  DeferredTaskHandler::GraphAutoLocker locker(this);
  return callback_metric_;
}

base::TimeDelta AudioContext::PlatformBufferDuration() const {
  return GetRealtimeAudioDestinationNode()
      ->GetOwnHandler()
      .GetPlatformBufferDuration();
}

void AudioContext::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  stats_update_restrictor_->SetCapturePermission(status);
  microphone_permission_status_ = status;
  if (is_media_device_service_initialized_) {
    CHECK_LT(pending_device_list_updates_, std::numeric_limits<int>::max());
    pending_device_list_updates_++;
    media_device_service_->EnumerateDevices(
        /* audio input */ false,
        /* video input */ false,
        /* audio output */ true,
        /* request_video_input_capabilities */ false,
        /* request_audio_input_capabilities */ false,
        blink::BindOnce(&AudioContext::DevicesEnumerated,
                        WrapWeakPersistent(this)));
  }
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
    stats_update_restrictor_->SetCapturePermission(status);
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
      microphone_permission_status_, std::move(observer));
}

double AudioContext::GetOutputLatencyQuantizingFactor() const {
  return microphone_permission_status_ ==
                 mojom::blink::PermissionStatus::GRANTED
             ? kOutputLatencyMaxPrecisionFactor
             : kOutputLatencyQuatizingFactor;
}

void AudioContext::NotifySetSinkIdBegins() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  // This performs step 5 to 9 from the second part of setSinkId() algorithm:
  // https://webaudio.github.io/web-audio-api/#dom-audiocontext-setsinkid-domstring-or-audiosinkoptions-sinkid
  sink_transition_flag_was_running_ =
      ContextState() == V8AudioContextState::Enum::kRunning;
  destination()->GetAudioDestinationHandler().StopRendering();
  if (sink_transition_flag_was_running_) {
    SetContextState(V8AudioContextState::Enum::kSuspended);
  }
}

void AudioContext::NotifySetSinkIdIsDone(
    WebAudioSinkDescriptor pending_sink_descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  sink_descriptor_ = pending_sink_descriptor;

  // This performs steps 11 and 12 from the second part of the setSinkId()
  // algorithm:
  // https://webaudio.github.io/web-audio-api/#dom-audiocontext-setsinkid-domstring-or-audiosinkoptions-sinkid
  UpdateV8SinkId();
  DispatchEvent(*Event::Create(event_type_names::kSinkchange));
  if (sink_transition_flag_was_running_) {
    destination()->GetAudioDestinationHandler().StartRendering();
    SetContextState(V8AudioContextState::Enum::kRunning);
    sink_transition_flag_was_running_ = false;
  }

  // The sink ID was given and has been accepted; it will be used as an output
  // audio device.
  is_sink_id_given_ = true;
}

void AudioContext::InitializeMediaDeviceService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

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

  CHECK_LT(pending_device_list_updates_, std::numeric_limits<int>::max());
  pending_device_list_updates_++;
  media_device_service_->EnumerateDevices(
      /* audio input */ false,
      /* video input */ false,
      /* audio output */ true,
      /* request_video_input_capabilities */ false,
      /* request_audio_input_capabilities */ false,
      blink::BindOnce(&AudioContext::DevicesEnumerated,
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
          mojom::blink::MediaDeviceType::kMediaAudioOutput)];

  TRACE_EVENT1("webaudio", "AudioContext::DevicesEnumerated",
               "DeviceEnumeration",
               audio_utilities::GetDeviceEnumerationForTracing(output_devices));

  OnDevicesChanged(mojom::blink::MediaDeviceType::kMediaAudioOutput,
                   output_devices);

  CHECK_GT(pending_device_list_updates_, 0);
  pending_device_list_updates_--;

  // Start the first resolver in the queue once `output_device_ids_` is
  // initialized from `OnDeviceChanged()` above.
  if (!set_sink_id_resolvers_.empty() && (pending_device_list_updates_ == 0)) {
    set_sink_id_resolvers_.front()->Start();
  }
}

void AudioContext::OnDevicesChanged(mojom::blink::MediaDeviceType device_type,
                                    const Vector<WebMediaDeviceInfo>& devices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  SendLogMessage(__func__, "");

  if (device_type == mojom::blink::MediaDeviceType::kMediaAudioOutput) {
    output_device_ids_.clear();
    for (auto device : devices) {
      if (device.device_id == media::AudioDeviceDescription::kDefaultDeviceId) {
        // Use the empty string to represent the default audio sink.
        output_device_ids_.insert(g_empty_string);
        output_device_ids_.insert(
            String(media::AudioDeviceDescription::kDefaultDeviceId));
      } else {
        output_device_ids_.insert(String::FromUTF8(device.device_id));
      }
    }
  }

  // If the device in use was disconnected (i.e. the current `sink_descriptor_`
  // is invalid), we need to decide how to handle the rendering.
  if (!IsValidSinkDescriptor(sink_descriptor_)) {
    SendLogMessage(__func__, "=> invalid sink descriptor");
    if (is_sink_id_given_) {
      // If the user's intent is to select a specific output device, do not
      // fallback to the default audio device. Invoke `RenderError` routine
      // instead.
      SendLogMessage(__func__,
                     "=> sink was explicitly specified, throwing error.");
      HandleRenderError();
    } else {
      // If there was no sink selected, manually call `SetSinkDescriptor()` to
      // fallback to the default audio output device to keep the audio playing.
      SendLogMessage(__func__,
                     "=> sink was not explicitly specified, falling back to "
                     "default sink.");
      DispatchEvent(*Event::Create(event_type_names::kError));
      GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<
                                               ConsoleMessage>(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kInfo,
          StrCat(
              {"[AudioContext] Fallback to the default device due to an invalid"
               " audio device change. (",
               String(sink_descriptor_.SinkId().Utf8()), ")"})));
      sink_descriptor_ = WebAudioSinkDescriptor(
          g_empty_string,
          To<LocalDOMWindow>(GetExecutionContext())->GetLocalFrameToken());
      auto* destination_node = GetRealtimeAudioDestinationNode();
      if (destination_node) {
        destination_node->SetSinkDescriptor(sink_descriptor_,
                                            base::DoNothing());
      }
      UpdateV8SinkId();
    }
  }
}

void AudioContext::FrameVisibilityChanged(
    mojom::blink::FrameVisibility frame_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  TRACE_EVENT2("webaudio", __func__, "UUID", Uuid(),
               "frame_visibility", static_cast<int>(frame_visibility));

  bool is_frame_hidden =
      (frame_visibility == mojom::blink::FrameVisibility::kNotRendered);
  if (is_frame_hidden == is_frame_hidden_) {
    return;
  }

  is_frame_hidden_ = is_frame_hidden;

  if (!should_interrupt_when_frame_is_hidden_) {
    return;
  }

  if (is_frame_hidden_) {
    RecordMediaPlaybackInterruptionType(
        AudioContextInterruptionType::kFrameHiddenWhilePlaying);
    // The frame is not rendered, so the audio context should be suspended.
    StartContextInterruption();
  } else {
    // The frame is rendered, so the audio context should be resumed.
    EndContextInterruption();
  }
}

void AudioContext::PageVisibilityChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  stats_update_restrictor_->SetVisible(GetPage() && GetPage()->IsPageVisible());
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

void AudioContext::OnRenderError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  CHECK(GetExecutionContext());
  render_error_occurred_ = true;
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(FROM_HERE, blink::BindOnce(&AudioContext::HandleRenderError,
                                            WrapPersistent(this)));
}

void AudioContext::ResumeOnPrerenderActivation() {
  CHECK(blocked_by_prerendering_);
  TRACE_EVENT2("webaudio", __func__, "UUID", Uuid(),
               "state", static_cast<int>(ContextState()));
  blocked_by_prerendering_ = false;
  switch (ContextState()) {
    case V8AudioContextState::Enum::kSuspended:
      StartRendering();
      break;
    case V8AudioContextState::Enum::kRunning:
      NOTREACHED();
    case V8AudioContextState::Enum::kClosed:
    // Prerender activation doesn't automatically resume audio playback
    // when the context is in the `interrupted` state.
    // TODO(crbug.com/374805121): Add the spec URL for this interruption
    // behavior when it has been published.
    case V8AudioContextState::Enum::kInterrupted:
      break;
  }
}

void AudioContext::TransferAudioFrameStatsTo(
    AudioFrameStatsAccumulator& receiver) {
  DeferredTaskHandler::GraphAutoLocker locker(this);
  receiver.Absorb(audio_frame_stats_);
}

int AudioContext::PendingDeviceListUpdates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);

  return pending_device_list_updates_;
}

void AudioContext::StartContextInterruption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  TRACE_EVENT2("webaudio", __func__, "UUID", Uuid(),
               "state", static_cast<int>(ContextState()));
  SendLogMessage(__func__, "");
  V8AudioContextState::Enum context_state = ContextState();
  if (context_state == V8AudioContextState::Enum::kClosed ||
      context_state == V8AudioContextState::Enum::kInterrupted) {
    return;
  }

  if (context_state == V8AudioContextState::Enum::kRunning) {
    // The context is running, so we need to stop the rendering.
    destination()->GetAudioDestinationHandler().StopRendering();
    should_transition_to_running_after_interruption_ = true;
    SetContextState(V8AudioContextState::Enum::kInterrupted);
  }

  // If the context is suspended, we don't make the transition to interrupted
  // state, because of privacy reasons. The suspended->interrupted transition is
  // only made when resumeContext() is called during an ongoing interruption.
  is_interrupted_while_suspended_ = true;
}

void AudioContext::EndContextInterruption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_thread_sequence_checker_);
  TRACE_EVENT2("webaudio", __func__, "UUID", Uuid(),
               "state", static_cast<int>(ContextState()));
  SendLogMessage(__func__, "");
  is_interrupted_while_suspended_ = false;
  if (ContextState() == V8AudioContextState::Enum::kClosed) {
    return;
  }

  if (should_transition_to_running_after_interruption_) {
    destination()->GetAudioDestinationHandler().StartRendering();
    should_transition_to_running_after_interruption_ = false;
    SetContextState(V8AudioContextState::Enum::kRunning);
  }
}

void AudioContext::HandleRenderError() {
  SendLogMessage(__func__, "");

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  if (window && window->GetFrame()) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kError,
        "The AudioContext encountered an error from the audio device or the "
        "WebAudio renderer."));
  }

  // Implements
  // https://webaudio.github.io/web-audio-api/#error-handling-on-a-running-audio-context
  switch (ContextState()) {
    case V8AudioContextState::Enum::kRunning:
    case V8AudioContextState::Enum::kInterrupted:
      // TODO(https://crbug.com/353641602): starting or stopping the renderer
      // should happen on the render thread, but this is the current convention.
      destination()->GetAudioDestinationHandler().StopRendering();

      DispatchEvent(*Event::Create(event_type_names::kError));
      suspended_by_user_ = false;
      SetContextState(V8AudioContextState::Enum::kSuspended);
      return;
    case V8AudioContextState::Enum::kSuspended:
      DispatchEvent(*Event::Create(event_type_names::kError));
      return;
    case V8AudioContextState::Enum::kClosed:
      return;
  }
  NOTREACHED();
}

void AudioContext::invoke_onrendererror_from_platform_for_testing() {
  GetRealtimeAudioDestinationNode()
      ->GetOwnHandler()
      .invoke_onrendererror_from_platform_for_testing();
}

void AudioContext::set_clock_for_testing(const base::TickClock* clock) {
  stats_update_restrictor_->SetClockForTesting(clock);
}

void AudioContext::SendLogMessage(const char* const function_name,
                                  const String& message) {
  WebRtcLogMessage(base::StrCat(
      {"[WA]AC::", function_name, " ", message.Utf8(), " [state=",
       state().AsCStr(), " sink_descriptor_=", sink_descriptor_.SinkId().Utf8(),
       ", sink_id_given_=", base::ToString(is_sink_id_given_), "]"}));
}

LocalFrame* AudioContext::GetLocalFrame() const {
  LocalDOMWindow* window = GetWindow();
  if (!window) {
    return nullptr;
  }

  return window->GetFrame();
}

Page* AudioContext::GetPageFromFrame() const {
  LocalFrame* frame = GetLocalFrame();
  if (!frame) {
    return nullptr;
  }

  return frame->GetPage();
}

void AudioContext::EnsureMediaPlayerConnection() {
  if (media_player_host_.is_bound() || !GetWindow()) {
    return;
  }

  GetWindow()
      ->GetFrame()
      ->GetRemoteNavigationAssociatedInterfaces()
      ->GetInterface(media_player_host_.BindNewEndpointAndPassReceiver(
          GetWindow()->GetTaskRunner(TaskType::kInternalMedia)));
  media_player_host_.set_disconnect_handler(blink::BindOnce(
      &AudioContext::OnMediaPlayerDisconnect, WrapWeakPersistent(this)));

  media_player_host_->OnMediaPlayerAdded(
      media_player_receiver_.BindNewEndpointAndPassRemote(
          GetWindow()->GetTaskRunner(TaskType::kInternalMedia)),
      media_player_observer_.BindNewEndpointAndPassReceiver(
          GetWindow()->GetTaskRunner(TaskType::kInternalMedia)),
      player_id_);
  media_player_observer_.set_disconnect_handler(blink::BindOnce(
      &AudioContext::OnMediaPlayerDisconnect, WrapWeakPersistent(this)));
}

void AudioContext::OnMediaPlayerDisconnect() {
  media_player_host_.reset();
  media_player_observer_.reset();
  volume_multiplier_ = 1.0;
}

bool AudioContext::CanPlayWhileHidden() const {
  return GetExecutionContext()->IsFeatureEnabled(
      network::mojom::blink::PermissionsPolicyFeature::
          kMediaPlaybackWhileNotVisible,
      ReportOptions::kDoNotReport);
}

}  // namespace blink
