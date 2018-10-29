// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_timestamp.h"
#include "third_party/blink/renderer/modules/webaudio/default_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/histogram.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

// Number of AudioContexts still alive.  It's incremented when an
// AudioContext is created and decremented when the context is closed.
static unsigned g_hardware_context_count = 0;

// A context ID that is incremented for each context that is created.
// This initializes the internal id for the context.
static unsigned g_context_id = 0;

AudioContext* AudioContext::Create(Document& document,
                                   const AudioContextOptions& context_options,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  UseCounter::CountCrossOriginIframe(
      document, WebFeature::kAudioContextCrossOriginIframe);

  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
  if (context_options.latencyHint().IsAudioContextLatencyCategory()) {
    latency_hint = WebAudioLatencyHint(
        context_options.latencyHint().GetAsAudioContextLatencyCategory());
  } else if (context_options.latencyHint().IsDouble()) {
    // This should be the requested output latency in seconds, without taking
    // into account double buffering (same as baseLatency).
    latency_hint =
        WebAudioLatencyHint(context_options.latencyHint().GetAsDouble());
  }

  AudioContext* audio_context = new AudioContext(document, latency_hint);
  audio_context->PauseIfNeeded();

  if (!audio_utilities::IsValidAudioBufferSampleRate(
          audio_context->sampleRate())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "hardware sample rate", audio_context->sampleRate(),
            audio_utilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return audio_context;
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
  if (audio_context->IsAllowedToStart()) {
    audio_context->StartRendering();
    audio_context->SetContextState(kRunning);
  }
  ++g_hardware_context_count;
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::AudioContext(): %u #%u\n",
          audio_context, audio_context->context_id_, g_hardware_context_count);
#endif

  DEFINE_STATIC_LOCAL(SparseHistogram, max_channel_count_histogram,
                      ("WebAudio.AudioContext.MaxChannelsAvailable"));
  DEFINE_STATIC_LOCAL(SparseHistogram, sample_rate_histogram,
                      ("WebAudio.AudioContext.HardwareSampleRate"));
  max_channel_count_histogram.Sample(
      audio_context->destination()->maxChannelCount());
  sample_rate_histogram.Sample(audio_context->sampleRate());

  // Warn users about new autoplay policy when it does not apply to them.
  if (RuntimeEnabledFeatures::AutoplayIgnoresWebAudioEnabled()) {
    document.AddConsoleMessage(ConsoleMessage::Create(
        kOtherMessageSource, kWarningMessageLevel,
        "The Web Audio autoplay policy will be re-enabled in Chrome 71 ("
        "December 2018). Please check that your website is compatible with it. "
        "https://goo.gl/7K7WLu"));
  }

  probe::didCreateAudioContext(&document);

  return audio_context;
}

AudioContext::AudioContext(Document& document,
                           const WebAudioLatencyHint& latency_hint)
    : BaseAudioContext(&document, kRealtimeContext),
      context_id_(g_context_id++) {
  destination_node_ = DefaultAudioDestinationNode::Create(this, latency_hint);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      if (document.GetFrame() &&
          document.GetFrame()->IsCrossOriginSubframe()) {
        autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
        user_gesture_required_ = true;
      }
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
      user_gesture_required_ = true;
      break;
  }

  Initialize();
}

void AudioContext::Uninitialize() {
  DCHECK(IsMainThread());
  DCHECK_NE(g_hardware_context_count, 0u);
  --g_hardware_context_count;

  DidClose();
  RecordAutoplayMetrics();
  BaseAudioContext::Uninitialize();
}

AudioContext::~AudioContext() {
  DCHECK(!autoplay_status_.has_value());
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::~AudioContext(): %u\n", this,
          context_id_);
#endif
}

void AudioContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(close_resolver_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise AudioContext::suspendContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ContextState() == kClosed) {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "Cannot suspend a context that has been closed"));
  } else {
    // Stop rendering now.
    if (destination())
      StopRendering();

    // Since we don't have any way of knowing when the hardware actually stops,
    // we'll just resolve the promise now.
    resolver->Resolve();

    // Probe reports the suspension only when the promise is resolved.
    probe::didSuspendAudioContext(GetDocument());
  }

  return promise;
}

ScriptPromise AudioContext::resumeContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  if (IsContextClosed()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidAccessError,
                             "cannot resume a closed AudioContext"));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we're already running, just resolve; nothing else needs to be
  // done.
  if (ContextState() == kRunning) {
    resolver->Resolve();
    return promise;
  }
  // Restart the destination node to pull on the audio graph.
  if (destination()) {
    MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kContextResume);
    if (IsAllowedToStart()) {
      // Do not set the state to running here.  We wait for the
      // destination to start to set the state.
      StartRendering();

      // Probe reports only when the user gesture allows the audio rendering.
      probe::didResumeAudioContext(GetDocument());
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

void AudioContext::getOutputTimestamp(ScriptState* script_state,
                                      AudioTimestamp& result) {
  DCHECK(IsMainThread());
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window)
    return;

  if (!destination()) {
    result.setContextTime(0.0);
    result.setPerformanceTime(0.0);
    return;
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
      TimeTicksFromSeconds(position.timestamp));
  if (performance_time < 0.0)
    performance_time = 0.0;

  result.setContextTime(position.position);
  result.setPerformanceTime(performance_time);
}

ScriptPromise AudioContext::closeContext(ScriptState* script_state) {
  if (IsContextClosed()) {
    // We've already closed the context previously, but it hasn't yet been
    // resolved, so just create a new promise and reject it.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(DOMExceptionCode::kInvalidStateError,
                             "Cannot close a context that is being closed or "
                             "has already been closed."));
  }

  close_resolver_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = close_resolver_->Promise();

  // Stops the rendering, but it doesn't release the resources here.
  StopRendering();

  // The promise from closing context resolves immediately after this function.
  DidClose();

  probe::didCloseAudioContext(GetDocument());

  return promise;
}

void AudioContext::DidClose() {
  SetContextState(kClosed);

  if (close_resolver_)
    close_resolver_->Resolve();
}

bool AudioContext::IsContextClosed() const {
  return close_resolver_ || BaseAudioContext::IsContextClosed();
}

void AudioContext::StopRendering() {
  DCHECK(IsMainThread());
  DCHECK(destination());

  if (ContextState() == kRunning) {
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(kSuspended);
    GetDeferredTaskHandler().ClearHandlersToBeDeleted();
  }
}

double AudioContext::baseLatency() const {
  DCHECK(IsMainThread());
  DCHECK(destination());

  // TODO(hongchan): Due to the incompatible constructor between
  // AudioDestinationNode and DefaultAudioDestinationNode, casting directly from
  // |destination()| is impossible. This is a temporary workaround until the
  // refactoring is completed.
  DefaultAudioDestinationHandler& destination_handler =
      static_cast<DefaultAudioDestinationHandler&>(
          destination()->GetAudioDestinationHandler());
  return destination_handler.GetFramesPerBuffer() /
         static_cast<double>(sampleRate());
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
  source_node_started_ = true;
  if (!user_gesture_required_)
    return;

  MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kSourceNodeStart);

  if (IsAllowedToStart())
    StartRendering();
}

AutoplayPolicy::Type AudioContext::GetAutoplayPolicy() const {
  Document* document = GetDocument();
  DCHECK(document);

  auto autoplay_policy =
      AutoplayPolicy::GetAutoplayPolicyForDocument(*document);

  if (autoplay_policy ==
          AutoplayPolicy::Type::kDocumentUserActivationRequired &&
      RuntimeEnabledFeatures::AutoplayIgnoresWebAudioEnabled()) {
// When ignored, the policy is different on Android compared to Desktop.
#if defined(OS_ANDROID)
    return AutoplayPolicy::Type::kUserGestureRequired;
#else
    // Force no user gesture required on desktop.
    return AutoplayPolicy::Type::kNoUserGestureRequired;
#endif
  }

  return autoplay_policy;
}

bool AudioContext::AreAutoplayRequirementsFulfilled() const {
  DCHECK(GetDocument());

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      return true;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      return LocalFrame::HasTransientUserActivation(GetDocument()->GetFrame());
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      return AutoplayPolicy::IsDocumentAllowedToPlay(*GetDocument());
  }

  NOTREACHED();
  return false;
}

void AudioContext::MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType type) {
  if (!user_gesture_required_ || !AreAutoplayRequirementsFulfilled())
    return;

  DCHECK(!autoplay_status_.has_value() ||
         autoplay_status_ != AutoplayStatus::kAutoplayStatusSucceeded);

  user_gesture_required_ = false;
  autoplay_status_ = AutoplayStatus::kAutoplayStatusSucceeded;

  DCHECK(!autoplay_unlock_type_.has_value());
  autoplay_unlock_type_ = type;
}

bool AudioContext::IsAllowedToStart() const {
  if (!user_gesture_required_)
    return true;

  Document* document = To<Document>(GetExecutionContext());
  DCHECK(document);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      NOTREACHED();
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      DCHECK(document->GetFrame() &&
             document->GetFrame()->IsCrossOriginSubframe());
      document->AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kWarningMessageLevel,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) from a user gesture event handler. https://goo.gl/7K7WLu"));
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      document->AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kWarningMessageLevel,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) after a user gesture on the page. https://goo.gl/7K7WLu"));
      break;
  }

  return false;
}

void AudioContext::RecordAutoplayMetrics() {
  if (!autoplay_status_.has_value() || !GetDocument())
    return;

  ukm::UkmRecorder* ukm_recorder = GetDocument()->UkmRecorder();
  DCHECK(ukm_recorder);
  ukm::builders::Media_Autoplay_AudioContext(GetDocument()->UkmSourceID())
      .SetStatus(autoplay_status_.value())
      .SetUnlockType(autoplay_unlock_type_
                         ? static_cast<int>(autoplay_unlock_type_.value())
                         : -1)
      .SetSourceNodeStarted(source_node_started_)
      .Record(ukm_recorder);

  // Record autoplay_status_ value.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplay_histogram,
      ("WebAudio.Autoplay", AutoplayStatus::kAutoplayStatusCount));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, cross_origin_autoplay_histogram,
      ("WebAudio.Autoplay.CrossOrigin", AutoplayStatus::kAutoplayStatusCount));

  autoplay_histogram.Count(autoplay_status_.value());

  if (GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->IsCrossOriginSubframe()) {
    cross_origin_autoplay_histogram.Count(autoplay_status_.value());
  }

  autoplay_status_.reset();

  // Record autoplay_unlock_type_ value.
  if (autoplay_unlock_type_.has_value()) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, autoplay_unlock_type_histogram,
                        ("WebAudio.Autoplay.UnlockType",
                         static_cast<int>(AutoplayUnlockType::kCount)));

    autoplay_unlock_type_histogram.Count(
        static_cast<int>(autoplay_unlock_type_.value()));

    autoplay_unlock_type_.reset();
  }
}

void AudioContext::ContextDestroyed(ExecutionContext*) {
  Uninitialize();
}

void AudioContext::NotifyAudibleAudioStarted() {
  DCHECK(IsMainThread());

  if (!audio_context_manager_) {
    Document* document = GetDocument();

    // If there's no document don't bother to try to create the mojom interface.
    // This can happen if the document has been reloaded while the audio thread
    // is still running.
    if (!document) {
      return;
    }

    document->GetFrame()->GetInterfaceProvider().GetInterface(
        mojo::MakeRequest(&audio_context_manager_));
  }

  DCHECK(audio_context_manager_);
  audio_context_manager_->AudioContextAudiblePlaybackStarted(context_id_);
}

void AudioContext::NotifyAudibleAudioStopped() {
  DCHECK(IsMainThread());
  DCHECK(audio_context_manager_);

  // If we don't have a document, we don't need to notify anyone that we've
  // stopped.
  if (GetDocument()) {
    audio_context_manager_->AudioContextAudiblePlaybackStopped(context_id_);
  }
}

}  // namespace blink
