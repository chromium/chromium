/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"

#include <type_traits>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

template <typename NumericConstraint>
bool SetUsesNumericConstraint(
    const MediaTrackConstraintSetPlatform& set,
    NumericConstraint MediaTrackConstraintSetPlatform::*field) {
  return (set.*field).HasExact() || (set.*field).HasIdeal() ||
         (set.*field).HasMin() || (set.*field).HasMax();
}

template <typename DiscreteConstraint>
bool SetUsesDiscreteConstraint(
    const MediaTrackConstraintSetPlatform& set,
    DiscreteConstraint MediaTrackConstraintSetPlatform::*field) {
  return (set.*field).HasExact() || (set.*field).HasIdeal();
}

template <typename NumericConstraint>
bool RequestUsesNumericConstraint(
    const MediaConstraints& constraints,
    NumericConstraint MediaTrackConstraintSetPlatform::*field) {
  if (SetUsesNumericConstraint(constraints.Basic(), field))
    return true;
  for (const auto& advanced_set : constraints.Advanced()) {
    if (SetUsesNumericConstraint(advanced_set, field))
      return true;
  }
  return false;
}

template <typename DiscreteConstraint>
bool RequestUsesDiscreteConstraint(
    const MediaConstraints& constraints,
    DiscreteConstraint MediaTrackConstraintSetPlatform::*field) {
  static_assert(
      std::is_same<
          decltype(field),
          StringConstraint MediaTrackConstraintSetPlatform::*>::value ||
          std::is_same<
              decltype(field),
              BooleanConstraint MediaTrackConstraintSetPlatform::*>::value,
      "Must use StringConstraint or BooleanConstraint");
  if (SetUsesDiscreteConstraint(constraints.Basic(), field))
    return true;
  for (const auto& advanced_set : constraints.Advanced()) {
    if (SetUsesDiscreteConstraint(advanced_set, field))
      return true;
  }
  return false;
}

class FeatureCounter {
 public:
  explicit FeatureCounter(ExecutionContext* context)
      : context_(context), is_unconstrained_(true) {}
  void Count(WebFeature feature) {
    UseCounter::Count(context_, feature);
    is_unconstrained_ = false;
  }
  bool IsUnconstrained() { return is_unconstrained_; }

 private:
  Persistent<ExecutionContext> context_;
  bool is_unconstrained_;

  DISALLOW_COPY_AND_ASSIGN(FeatureCounter);
};

void CountAudioConstraintUses(ExecutionContext* context,
                              const MediaConstraints& constraints) {
  FeatureCounter counter(context);
  if (RequestUsesNumericConstraint(
          constraints, &MediaTrackConstraintSetPlatform::sample_rate)) {
    counter.Count(WebFeature::kMediaStreamConstraintsSampleRate);
  }
  if (RequestUsesNumericConstraint(
          constraints, &MediaTrackConstraintSetPlatform::sample_size)) {
    counter.Count(WebFeature::kMediaStreamConstraintsSampleSize);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::echo_cancellation)) {
    counter.Count(WebFeature::kMediaStreamConstraintsEchoCancellation);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &MediaTrackConstraintSetPlatform::latency)) {
    counter.Count(WebFeature::kMediaStreamConstraintsLatency);
  }
  if (RequestUsesNumericConstraint(
          constraints, &MediaTrackConstraintSetPlatform::channel_count)) {
    counter.Count(WebFeature::kMediaStreamConstraintsChannelCount);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::device_id)) {
    counter.Count(WebFeature::kMediaStreamConstraintsDeviceIdAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::disable_local_echo)) {
    counter.Count(WebFeature::kMediaStreamConstraintsDisableLocalEcho);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::group_id)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGroupIdAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::media_stream_source)) {
    counter.Count(WebFeature::kMediaStreamConstraintsMediaStreamSourceAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::render_to_associated_sink)) {
    counter.Count(WebFeature::kMediaStreamConstraintsRenderToAssociatedSink);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_echo_cancellation)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogEchoCancellation);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &MediaTrackConstraintSetPlatform::
                                        goog_experimental_echo_cancellation)) {
    counter.Count(
        WebFeature::kMediaStreamConstraintsGoogExperimentalEchoCancellation);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_auto_gain_control)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogAutoGainControl);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &MediaTrackConstraintSetPlatform::
                                        goog_experimental_auto_gain_control)) {
    counter.Count(
        WebFeature::kMediaStreamConstraintsGoogExperimentalAutoGainControl);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_noise_suppression)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogNoiseSuppression);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_highpass_filter)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogHighpassFilter);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &MediaTrackConstraintSetPlatform::
                                        goog_experimental_noise_suppression)) {
    counter.Count(
        WebFeature::kMediaStreamConstraintsGoogExperimentalNoiseSuppression);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_audio_mirroring)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogAudioMirroring);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_da_echo_cancellation)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogDAEchoCancellation);
  }

  UseCounter::Count(context, WebFeature::kMediaStreamConstraintsAudio);
  if (counter.IsUnconstrained()) {
    UseCounter::Count(context,
                      WebFeature::kMediaStreamConstraintsAudioUnconstrained);
  }
}

void CountVideoConstraintUses(ExecutionContext* context,
                              const MediaConstraints& constraints) {
  FeatureCounter counter(context);
  if (RequestUsesNumericConstraint(constraints,
                                   &MediaTrackConstraintSetPlatform::width)) {
    counter.Count(WebFeature::kMediaStreamConstraintsWidth);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &MediaTrackConstraintSetPlatform::height)) {
    counter.Count(WebFeature::kMediaStreamConstraintsHeight);
  }
  if (RequestUsesNumericConstraint(
          constraints, &MediaTrackConstraintSetPlatform::aspect_ratio)) {
    counter.Count(WebFeature::kMediaStreamConstraintsAspectRatio);
  }
  if (RequestUsesNumericConstraint(
          constraints, &MediaTrackConstraintSetPlatform::frame_rate)) {
    counter.Count(WebFeature::kMediaStreamConstraintsFrameRate);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::facing_mode)) {
    counter.Count(WebFeature::kMediaStreamConstraintsFacingMode);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::device_id)) {
    counter.Count(WebFeature::kMediaStreamConstraintsDeviceIdVideo);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::group_id)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGroupIdVideo);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::video_kind)) {
    counter.Count(WebFeature::kMediaStreamConstraintsVideoKind);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::media_stream_source)) {
    counter.Count(WebFeature::kMediaStreamConstraintsMediaStreamSourceVideo);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &MediaTrackConstraintSetPlatform::goog_noise_reduction)) {
    counter.Count(WebFeature::kMediaStreamConstraintsGoogNoiseReduction);
  }

  UseCounter::Count(context, WebFeature::kMediaStreamConstraintsVideo);
  if (counter.IsUnconstrained()) {
    UseCounter::Count(context,
                      WebFeature::kMediaStreamConstraintsVideoUnconstrained);
  }
}

MediaConstraints ParseOptions(ExecutionContext* context,
                              const BooleanOrMediaTrackConstraints& options,
                              MediaErrorState& error_state) {
  MediaConstraints constraints;

  if (options.IsNull()) {
    // Do nothing.
  } else if (options.IsMediaTrackConstraints()) {
    constraints = media_constraints_impl::Create(
        context, options.GetAsMediaTrackConstraints(), error_state);
  } else {
    DCHECK(options.IsBoolean());
    if (options.GetAsBoolean()) {
      constraints = media_constraints_impl::Create();
    }
  }

  return constraints;
}

}  // namespace

class UserMediaRequest::V8Callbacks final : public UserMediaRequest::Callbacks {
 public:
  V8Callbacks(V8NavigatorUserMediaSuccessCallback* success_callback,
              V8NavigatorUserMediaErrorCallback* error_callback)
      : success_callback_(success_callback), error_callback_(error_callback) {}
  ~V8Callbacks() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(success_callback_);
    visitor->Trace(error_callback_);
    UserMediaRequest::Callbacks::Trace(visitor);
  }

  void OnSuccess(ScriptWrappable* callback_this_value,
                 MediaStream* stream) override {
    success_callback_->InvokeAndReportException(callback_this_value, stream);
  }
  void OnError(ScriptWrappable* callback_this_value,
               DOMExceptionOrOverconstrainedError error) override {
    error_callback_->InvokeAndReportException(callback_this_value, error);
  }

 private:
  Member<V8NavigatorUserMediaSuccessCallback> success_callback_;
  Member<V8NavigatorUserMediaErrorCallback> error_callback_;
};

UserMediaRequest* UserMediaRequest::Create(
    ExecutionContext* context,
    UserMediaController* controller,
    UserMediaRequest::MediaType media_type,
    const MediaStreamConstraints* options,
    Callbacks* callbacks,
    MediaErrorState& error_state,
    IdentifiableSurface surface) {
  MediaConstraints audio = ParseOptions(context, options->audio(), error_state);
  if (error_state.HadException())
    return nullptr;

  MediaConstraints video = ParseOptions(context, options->video(), error_state);
  if (error_state.HadException())
    return nullptr;

  if (media_type == UserMediaRequest::MediaType::kUserMedia &&
      !video.IsNull()) {
    if (video.Basic().pan.HasMandatory()) {
      error_state.ThrowTypeError("Mandatory pan constraint is not supported");
      return nullptr;
    }
    if (video.Basic().tilt.HasMandatory()) {
      error_state.ThrowTypeError("Mandatory tilt constraint is not supported");
      return nullptr;
    }
    if (video.Basic().zoom.HasMandatory()) {
      error_state.ThrowTypeError("Mandatory zoom constraint is not supported");
      return nullptr;
    }
  } else if (media_type == UserMediaRequest::MediaType::kDisplayMedia) {
    // https://w3c.github.io/mediacapture-screen-share/#mediadevices-additions
    // MediaDevices Additions
    // The user agent MUST reject audio-only requests.
    // 1. Let constraints be the method's first argument.
    // 2. For each member present in constraints whose value, value, is a
    // dictionary, run the following steps:
    //   1. If value contains a member named advanced, return a promise rejected
    //   with a newly created TypeError.
    //   2. If value contains a member which in turn is a dictionary containing
    //   a member named either min or exact, return a promise rejected with a
    //   newly created TypeError.
    // 3. Let requestedMediaTypes be the set of media types in constraints with
    // either a dictionary value or a value of true.
    // 4. If requestedMediaTypes is the empty set, set requestedMediaTypes to a
    // set containing "video".
    if ((!audio.IsNull() && !audio.Advanced().IsEmpty()) ||
        (!video.IsNull() && !video.Advanced().IsEmpty())) {
      error_state.ThrowTypeError("Advanced constraints are not supported");
      return nullptr;
    }
    if ((!audio.IsNull() && audio.Basic().HasMin()) ||
        (!video.IsNull() && video.Basic().HasMin())) {
      error_state.ThrowTypeError("min constraints are not supported");
      return nullptr;
    }
    if ((!audio.IsNull() && audio.Basic().HasExact()) ||
        (!video.IsNull() && video.Basic().HasExact())) {
      error_state.ThrowTypeError("exact constraints are not supported");
      return nullptr;
    }
    if (!audio.IsNull() && video.IsNull()) {
      error_state.ThrowTypeError("Audio only requests are not supported");
      return nullptr;
    }
    if (audio.IsNull() && video.IsNull()) {
      video = ParseOptions(context,
                           BooleanOrMediaTrackConstraints::FromBoolean(true),
                           error_state);
      if (error_state.HadException())
        return nullptr;
    }
  }

  if (audio.IsNull() && video.IsNull()) {
    error_state.ThrowTypeError(
        "At least one of audio and video must be requested");
    return nullptr;
  }

  if (!audio.IsNull())
    CountAudioConstraintUses(context, audio);
  if (!video.IsNull())
    CountVideoConstraintUses(context, video);

  return MakeGarbageCollected<UserMediaRequest>(
      context, controller, media_type, audio, video, callbacks, surface);
}

UserMediaRequest* UserMediaRequest::Create(
    ExecutionContext* context,
    UserMediaController* controller,
    const MediaStreamConstraints* options,
    V8NavigatorUserMediaSuccessCallback* success_callback,
    V8NavigatorUserMediaErrorCallback* error_callback,
    MediaErrorState& error_state,
    IdentifiableSurface surface) {
  return Create(
      context, controller, UserMediaRequest::MediaType::kUserMedia, options,
      MakeGarbageCollected<V8Callbacks>(success_callback, error_callback),
      error_state, surface);
}

UserMediaRequest* UserMediaRequest::CreateForTesting(
    const MediaConstraints& audio,
    const MediaConstraints& video) {
  return MakeGarbageCollected<UserMediaRequest>(
      nullptr, nullptr, UserMediaRequest::MediaType::kUserMedia, audio, video,
      nullptr, IdentifiableSurface());
}

UserMediaRequest::UserMediaRequest(ExecutionContext* context,
                                   UserMediaController* controller,
                                   UserMediaRequest::MediaType media_type,
                                   MediaConstraints audio,
                                   MediaConstraints video,
                                   Callbacks* callbacks,
                                   IdentifiableSurface surface)
    : ExecutionContextLifecycleObserver(context),
      media_type_(media_type),
      audio_(audio),
      video_(video),
      should_disable_hardware_noise_suppression_(
          RuntimeEnabledFeatures::DisableHardwareNoiseSuppressionEnabled(
              context)),
      controller_(controller),
      callbacks_(callbacks),
      surface_(surface) {
  if (should_disable_hardware_noise_suppression_) {
    UseCounter::Count(context,
                      WebFeature::kUserMediaDisableHardwareNoiseSuppression);
  }
}

UserMediaRequest::~UserMediaRequest() = default;

UserMediaRequest::MediaType UserMediaRequest::MediaRequestType() const {
  return media_type_;
}

bool UserMediaRequest::Audio() const {
  return !audio_.IsNull();
}

bool UserMediaRequest::Video() const {
  return !video_.IsNull();
}

MediaConstraints UserMediaRequest::AudioConstraints() const {
  return audio_;
}

MediaConstraints UserMediaRequest::VideoConstraints() const {
  return video_;
}

bool UserMediaRequest::ShouldDisableHardwareNoiseSuppression() const {
  return should_disable_hardware_noise_suppression_;
}

bool UserMediaRequest::IsSecureContextUse(String& error_message) {
  LocalDOMWindow* window = GetWindow();

  if (window->IsSecureContext(error_message)) {
    UseCounter::Count(window, WebFeature::kGetUserMediaSecureOrigin);
    window->CountUseOnlyInCrossOriginIframe(
        WebFeature::kGetUserMediaSecureOriginIframe);

    // Feature policy deprecation messages.
    if (Audio()) {
      if (!window->IsFeatureEnabled(
              mojom::blink::FeaturePolicyFeature::kMicrophone,
              ReportOptions::kReportOnFailure)) {
        UseCounter::Count(
            window, WebFeature::kMicrophoneDisabledByFeaturePolicyEstimate);
      }
    }
    if (Video()) {
      if (!window->IsFeatureEnabled(mojom::blink::FeaturePolicyFeature::kCamera,
                                    ReportOptions::kReportOnFailure)) {
        UseCounter::Count(window,
                          WebFeature::kCameraDisabledByFeaturePolicyEstimate);
      }
    }

    return true;
  }

  // While getUserMedia is blocked on insecure origins, we still want to
  // count attempts to use it.
  Deprecation::CountDeprecation(window,
                                WebFeature::kGetUserMediaInsecureOrigin);
  Deprecation::CountDeprecationCrossOriginIframe(
      window, WebFeature::kGetUserMediaInsecureOriginIframe);
  return false;
}

LocalDOMWindow* UserMediaRequest::GetWindow() {
  return To<LocalDOMWindow>(GetExecutionContext());
}

void UserMediaRequest::Start() {
  if (controller_)
    controller_->RequestUserMedia(this);
}

void UserMediaRequest::Succeed(MediaStreamDescriptor* stream_descriptor) {
  DCHECK(!is_resolved_);
  if (!GetExecutionContext())
    return;

  MediaStream::Create(GetExecutionContext(), stream_descriptor,
                      WTF::Bind(&UserMediaRequest::OnMediaStreamInitialized,
                                WrapPersistent(this)));
}

void UserMediaRequest::OnMediaStreamInitialized(MediaStream* stream) {
  DCHECK(!is_resolved_);

  MediaStreamTrackVector audio_tracks = stream->getAudioTracks();
  for (const auto& audio_track : audio_tracks)
    audio_track->SetConstraints(audio_);

  MediaStreamTrackVector video_tracks = stream->getVideoTracks();
  for (const auto& video_track : video_tracks)
    video_track->SetConstraints(video_);

  callbacks_->OnSuccess(nullptr, stream);
  RecordIdentifiabilityMetric(surface_, GetExecutionContext(),
                              IdentifiabilityBenignStringToken(g_empty_string));
  is_resolved_ = true;
}

void UserMediaRequest::FailConstraint(const String& constraint_name,
                                      const String& message) {
  DCHECK(!constraint_name.IsEmpty());
  DCHECK(!is_resolved_);
  if (!GetExecutionContext())
    return;
  callbacks_->OnError(
      nullptr, DOMExceptionOrOverconstrainedError::FromOverconstrainedError(
                   OverconstrainedError::Create(constraint_name, message)));
  RecordIdentifiabilityMetric(surface_, GetExecutionContext(),
                              IdentifiabilityBenignStringToken(message));
  is_resolved_ = true;
}

void UserMediaRequest::Fail(Error name, const String& message) {
  DCHECK(!is_resolved_);
  if (!GetExecutionContext())
    return;

  DOMExceptionCode exception_code = DOMExceptionCode::kNotSupportedError;
  switch (name) {
    case Error::kPermissionDenied:
    case Error::kPermissionDismissed:
    case Error::kInvalidState:
    case Error::kFailedDueToShutdown:
    case Error::kKillSwitchOn:
    case Error::kSystemPermissionDenied:
      exception_code = DOMExceptionCode::kNotAllowedError;
      break;
    case Error::kDevicesNotFound:
      exception_code = DOMExceptionCode::kNotFoundError;
      break;
    case Error::kTabCapture:
    case Error::kScreenCapture:
    case Error::kCapture:
      exception_code = DOMExceptionCode::kAbortError;
      break;
    case Error::kTrackStart:
      exception_code = DOMExceptionCode::kNotReadableError;
      break;
    case Error::kNotSupported:
      exception_code = DOMExceptionCode::kNotSupportedError;
      break;
    case Error::kSecurityError:
      exception_code = DOMExceptionCode::kSecurityError;
      break;
    default:
      NOTREACHED();
  }
  callbacks_->OnError(
      nullptr,
      DOMExceptionOrOverconstrainedError::FromDOMException(
          MakeGarbageCollected<DOMException>(exception_code, message)));
  RecordIdentifiabilityMetric(surface_, GetExecutionContext(),
                              IdentifiabilityBenignStringToken(message));
  is_resolved_ = true;
}

void UserMediaRequest::ContextDestroyed() {
  if (!is_resolved_)
    blink::WebRtcLogMessage("UMR::ContextDestroyed. Request not resolved.");
  if (controller_) {
    controller_->CancelUserMediaRequest(this);
    if (!is_resolved_) {
      blink::WebRtcLogMessage(base::StringPrintf(
          "UMR::ContextDestroyed. Resolving unsolved request. "
          "audio constraints=%s, video constraints=%s",
          AudioConstraints().ToString().Utf8().c_str(),
          VideoConstraints().ToString().Utf8().c_str()));
      callbacks_->OnError(
          nullptr,
          DOMExceptionOrOverconstrainedError::FromDOMException(
              MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                 "Context destroyed")));
    }
    controller_ = nullptr;
  }
}

void UserMediaRequest::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  visitor->Trace(callbacks_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
