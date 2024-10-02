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

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/capture_controller.h"
#include "third_party/blink/renderer/modules/mediastream/identifiability_metrics.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_set.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_tracker.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

using mojom::blink::MediaStreamType;
using Result = mojom::blink::MediaStreamRequestResult;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayMediaIncludeExcludeConstraint {
  kNotSpecified = 0,
  kInclude = 1,
  kExclude = 2,
  kMaxValue = kExclude
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayMediaConstraintsDisplaySurface {
  kNotSpecified = 0,
  kTab = 1,
  kWindow = 2,
  kMonitor = 3,
  kMaxValue = kMonitor
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayMediaBooleanConstraint {
  kNotSpecified = 0,
  kTrue = 1,
  kFalse = 2,
  kMaxValue = kFalse
};

void RecordUma(GetDisplayMediaConstraintsDisplaySurface value) {
  base::UmaHistogramEnumeration(
      "Media.GetDisplayMedia.Constraints.DisplaySurface", value);
}

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

  FeatureCounter(const FeatureCounter&) = delete;
  FeatureCounter& operator=(const FeatureCounter&) = delete;

  void Count(WebFeature feature) {
    UseCounter::Count(context_, feature);
    is_unconstrained_ = false;
  }
  bool IsUnconstrained() { return is_unconstrained_; }

 private:
  Persistent<ExecutionContext> context_;
  bool is_unconstrained_;
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

void RecordGetDisplayMediaIncludeExcludeConstraintUma(
    std::optional<V8DisplayMediaIncludeOrExclude::Enum> include_or_exclude,
    const std::string& histogram_name) {
  const GetDisplayMediaIncludeExcludeConstraint value =
      (!include_or_exclude.has_value()
           ? GetDisplayMediaIncludeExcludeConstraint::kNotSpecified
       : include_or_exclude == V8DisplayMediaIncludeOrExclude::Enum::kInclude
           ? GetDisplayMediaIncludeExcludeConstraint::kInclude
           : GetDisplayMediaIncludeExcludeConstraint::kExclude);
  base::UmaHistogramEnumeration(histogram_name, value);
}

void RecordPreferredDisplaySurfaceConstraintUma(
    const mojom::blink::PreferredDisplaySurface preferred_display_surface) {
  switch (preferred_display_surface) {
    case mojom::blink::PreferredDisplaySurface::NO_PREFERENCE:
      RecordUma(GetDisplayMediaConstraintsDisplaySurface::kNotSpecified);
      return;
    case mojom::blink::PreferredDisplaySurface::MONITOR:
      RecordUma(GetDisplayMediaConstraintsDisplaySurface::kMonitor);
      return;
    case mojom::blink::PreferredDisplaySurface::WINDOW:
      RecordUma(GetDisplayMediaConstraintsDisplaySurface::kWindow);
      return;
    case mojom::blink::PreferredDisplaySurface::BROWSER:
      RecordUma(GetDisplayMediaConstraintsDisplaySurface::kTab);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void RecordSuppressLocalAudioPlaybackConstraintUma(
    std::optional<bool> suppress_local_audio_playback) {
  const GetDisplayMediaBooleanConstraint value =
      (!suppress_local_audio_playback.has_value()
           ? GetDisplayMediaBooleanConstraint::kNotSpecified
       : suppress_local_audio_playback.value()
           ? GetDisplayMediaBooleanConstraint::kTrue
           : GetDisplayMediaBooleanConstraint::kFalse);
  base::UmaHistogramEnumeration(
      "Media.GetDisplayMedia.Constraints.SuppressLocalAudioPlayback", value);
}

MediaConstraints ParseOptions(
    ExecutionContext* execution_context,
    const V8UnionBooleanOrMediaTrackConstraints* options,
    ExceptionState& exception_state) {
  if (!options)
    return MediaConstraints();
  switch (options->GetContentType()) {
    case V8UnionBooleanOrMediaTrackConstraints::ContentType::kBoolean:
      if (options->GetAsBoolean())
        return media_constraints_impl::Create();
      else
        return MediaConstraints();
    case V8UnionBooleanOrMediaTrackConstraints::ContentType::
        kMediaTrackConstraints:
      String error_message;
      auto constraints = media_constraints_impl::Create(
          execution_context, options->GetAsMediaTrackConstraints(),
          error_message);
      if (constraints.IsNull()) {
        exception_state.ThrowTypeError(error_message);
      }
      return constraints;
  }
  NOTREACHED_IN_MIGRATION();
  return MediaConstraints();
}

}  // namespace

UserMediaRequest* UserMediaRequest::Create(
    ExecutionContext* context,
    UserMediaClient* client,
    UserMediaRequestType media_type,
    const MediaStreamConstraints* options,
    Callbacks* callbacks,
    ExceptionState& exception_state,
    IdentifiableSurface surface) {
  MediaConstraints audio =
      ParseOptions(context, options->audio(), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  MediaConstraints video =
      ParseOptions(context, options->video(), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  std::string display_surface_constraint;
  std::optional<bool> suppress_local_audio_playback;

  if (media_type == UserMediaRequestType::kUserMedia) {
    if (audio.IsNull() && video.IsNull()) {
      exception_state.ThrowTypeError(
          "At least one of audio and video must be requested");
      return nullptr;
    } else if (!video.IsNull()) {
      auto& video_basic = video.MutableBasic();
      const BaseConstraint* constraints[] = {
          &video_basic.pan,
          &video_basic.tilt,
          &video_basic.zoom,
          &video_basic.background_blur,
          &video_basic.background_segmentation_mask,
          &video_basic.eye_gaze_correction,
          &video_basic.face_framing,
      };
      for (const BaseConstraint* constraint : constraints) {
        if (constraint->HasMandatory()) {
          exception_state.ThrowTypeError(
              String::Format("Mandatory %s constraints are not supported",
                             constraint->GetName()));
          return nullptr;
        }
      }
      BaseConstraint* compatibility_constraints[] = {
          &video_basic.exposure_compensation,
          &video_basic.exposure_time,
          &video_basic.color_temperature,
          &video_basic.iso,
          &video_basic.brightness,
          &video_basic.contrast,
          &video_basic.saturation,
          &video_basic.sharpness,
          &video_basic.focus_distance,
          &video_basic.torch,
      };
      for (BaseConstraint* constraint : compatibility_constraints) {
        if (constraint->HasMandatory()) {
          // This should throw a TypeError, but that cannot be done due
          // to backward compatibility.
          // Thus instead of that, let's ignore the constraint.
          constraint->ResetToUnconstrained();
        }
      }
    }
  } else if (media_type == UserMediaRequestType::kDisplayMedia ||
             media_type == UserMediaRequestType::kAllScreensMedia) {
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
    if (media_type == UserMediaRequestType::kAllScreensMedia) {
      if (!audio.IsNull()) {
        exception_state.ThrowTypeError("Audio requests are not supported");
        return nullptr;
      } else if (options->preferCurrentTab()) {
        exception_state.ThrowTypeError("preferCurrentTab is not supported");
        return nullptr;
      }
    }

    if (audio.IsNull() && video.IsNull()) {
      exception_state.ThrowTypeError("either audio or video must be requested");
      return nullptr;
    }

    if ((!audio.IsNull() && !audio.Advanced().empty()) ||
        (!video.IsNull() && !video.Advanced().empty())) {
      exception_state.ThrowTypeError("Advanced constraints are not supported");
      return nullptr;
    }

    if ((!audio.IsNull() && audio.Basic().HasMin()) ||
        (!video.IsNull() && video.Basic().HasMin())) {
      exception_state.ThrowTypeError("min constraints are not supported");
      return nullptr;
    }

    if ((!audio.IsNull() && audio.Basic().HasExact()) ||
        (!video.IsNull() && video.Basic().HasExact())) {
      exception_state.ThrowTypeError("exact constraints are not supported");
      return nullptr;
    }

    if (!video.IsNull() && video.Basic().display_surface.HasIdeal() &&
        video.Basic().display_surface.Ideal().size() > 0) {
      display_surface_constraint =
          video.Basic().display_surface.Ideal()[0].Utf8();
    }

    if (!audio.IsNull() &&
        audio.Basic().suppress_local_audio_playback.HasIdeal()) {
      suppress_local_audio_playback =
          audio.Basic().suppress_local_audio_playback.Ideal();
    }
  }

  if (!audio.IsNull())
    CountAudioConstraintUses(context, audio);
  if (!video.IsNull())
    CountVideoConstraintUses(context, video);

  UserMediaRequest* const result = MakeGarbageCollected<UserMediaRequest>(
      context, client, media_type, audio, video, options->preferCurrentTab(),
      options->getControllerOr(nullptr), callbacks, surface);

  // The default is to include.
  // Note that this option is no-op if audio is not requested.
  result->set_exclude_system_audio(
      options->hasSystemAudio() &&
      options->systemAudio().AsEnum() ==
          V8DisplayMediaIncludeOrExclude::Enum::kExclude);
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    std::optional<V8DisplayMediaIncludeOrExclude::Enum> include_or_exclude;
    if (options->hasSystemAudio()) {
      include_or_exclude = options->systemAudio().AsEnum();
    }
    RecordGetDisplayMediaIncludeExcludeConstraintUma(
        include_or_exclude, "Media.GetDisplayMedia.Constraints.SystemAudio");
  }

  // The default is to include.
  const bool exclude_self_browser_surface =
      options->hasSelfBrowserSurface() &&
      options->selfBrowserSurface().AsEnum() ==
          V8DisplayMediaIncludeOrExclude::Enum::kExclude;
  if (exclude_self_browser_surface && options->preferCurrentTab()) {
    exception_state.ThrowTypeError(
        "Self-contradictory configuration (preferCurrentTab and "
        "selfBrowserSurface=exclude).");
    return nullptr;
  }
  result->set_exclude_self_browser_surface(exclude_self_browser_surface);
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    std::optional<V8DisplayMediaIncludeOrExclude::Enum> include_or_exclude;
    if (options->hasSelfBrowserSurface()) {
      include_or_exclude = options->selfBrowserSurface().AsEnum();
    }
    RecordGetDisplayMediaIncludeExcludeConstraintUma(
        include_or_exclude,
        "Media.GetDisplayMedia.Constraints.SelfBrowserSurface");
  }

  mojom::blink::PreferredDisplaySurface preferred_display_surface =
      mojom::blink::PreferredDisplaySurface::NO_PREFERENCE;
  if (display_surface_constraint == "monitor") {
    preferred_display_surface = mojom::blink::PreferredDisplaySurface::MONITOR;
  } else if (display_surface_constraint == "window") {
    preferred_display_surface = mojom::blink::PreferredDisplaySurface::WINDOW;
  } else if (display_surface_constraint == "browser") {
    preferred_display_surface = mojom::blink::PreferredDisplaySurface::BROWSER;
  }
  result->set_preferred_display_surface(preferred_display_surface);
  if (media_type == UserMediaRequestType::kDisplayMedia)
    RecordPreferredDisplaySurfaceConstraintUma(preferred_display_surface);

  // The default is to request dynamic surface switching.
  result->set_dynamic_surface_switching_requested(
      !options->hasSurfaceSwitching() ||
      options->surfaceSwitching().AsEnum() ==
          V8DisplayMediaIncludeOrExclude::Enum::kInclude);
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    std::optional<V8DisplayMediaIncludeOrExclude::Enum> include_or_exclude;
    if (options->hasSurfaceSwitching()) {
      include_or_exclude = options->surfaceSwitching().AsEnum();
    }
    RecordGetDisplayMediaIncludeExcludeConstraintUma(
        include_or_exclude,
        "Media.GetDisplayMedia.Constraints.SurfaceSwitching");
  }

  // The default is to include.
  const bool exclude_monitor_type_surfaces =
      options->hasMonitorTypeSurfaces() &&
      options->monitorTypeSurfaces().AsEnum() ==
          V8DisplayMediaIncludeOrExclude::Enum::kExclude;
  if (exclude_monitor_type_surfaces &&
      display_surface_constraint == "monitor") {
    exception_state.ThrowTypeError(
        "Self-contradictory configuration (displaySurface=monitor and "
        "monitorTypeSurfaces=exclude).");
    return nullptr;
  }
  result->set_exclude_monitor_type_surfaces(exclude_monitor_type_surfaces);
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    std::optional<V8DisplayMediaIncludeOrExclude::Enum> include_or_exclude;
    if (options->hasMonitorTypeSurfaces()) {
      include_or_exclude = options->monitorTypeSurfaces().AsEnum();
    }
    RecordGetDisplayMediaIncludeExcludeConstraintUma(
        include_or_exclude,
        "Media.GetDisplayMedia.Constraints.MonitorTypeSurfaces");
  }

  result->set_suppress_local_audio_playback(
      suppress_local_audio_playback.value_or(false));
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    RecordSuppressLocalAudioPlaybackConstraintUma(
        suppress_local_audio_playback);
  }

  return result;
}

UserMediaRequest* UserMediaRequest::CreateForTesting(
    const MediaConstraints& audio,
    const MediaConstraints& video) {
  return MakeGarbageCollected<UserMediaRequest>(
      nullptr, nullptr, UserMediaRequestType::kUserMedia, audio, video,
      /*should_prefer_current_tab=*/false,
      /*capture_controller=*/nullptr, /*callbacks=*/nullptr,
      IdentifiableSurface());
}

UserMediaRequest::UserMediaRequest(ExecutionContext* context,
                                   UserMediaClient* client,
                                   UserMediaRequestType media_type,
                                   MediaConstraints audio,
                                   MediaConstraints video,
                                   bool should_prefer_current_tab,
                                   CaptureController* capture_controller,
                                   Callbacks* callbacks,
                                   IdentifiableSurface surface)
    : ExecutionContextLifecycleObserver(context),
      media_type_(media_type),
      audio_(audio),
      video_(video),
      capture_controller_(capture_controller),
      should_prefer_current_tab_(should_prefer_current_tab),
      should_disable_hardware_noise_suppression_(
          RuntimeEnabledFeatures::DisableHardwareNoiseSuppressionEnabled(
              context)),
      client_(client),
      callbacks_(callbacks),
      surface_(surface) {
  if (should_disable_hardware_noise_suppression_) {
    UseCounter::Count(context,
                      WebFeature::kUserMediaDisableHardwareNoiseSuppression);
  }
}

UserMediaRequest::~UserMediaRequest() = default;

UserMediaRequestType UserMediaRequest::MediaRequestType() const {
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

MediaStreamType UserMediaRequest::AudioMediaStreamType() const {
  if (!Audio()) {
    return MediaStreamType::NO_SERVICE;
  }
  if (MediaRequestType() == UserMediaRequestType::kDisplayMedia) {
    return MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  }
  if (MediaRequestType() == UserMediaRequestType::kAllScreensMedia) {
    return MediaStreamType::NO_SERVICE;
  }
  DCHECK_EQ(UserMediaRequestType::kUserMedia, MediaRequestType());

  // Check if this is a getUserMedia display capture.
  const MediaConstraints& constraints = AudioConstraints();
  String source_constraint =
      constraints.Basic().media_stream_source.Exact().empty()
          ? String()
          : String(constraints.Basic().media_stream_source.Exact()[0]);
  if (!source_constraint.empty()) {
    // This is a getUserMedia display capture call.
    if (source_constraint == blink::kMediaStreamSourceTab) {
      return MediaStreamType::GUM_TAB_AUDIO_CAPTURE;
    } else if (source_constraint == blink::kMediaStreamSourceDesktop ||
               source_constraint == blink::kMediaStreamSourceSystem) {
      return MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;
    }
    return MediaStreamType::NO_SERVICE;
  }

  return MediaStreamType::DEVICE_AUDIO_CAPTURE;
}

MediaStreamType UserMediaRequest::VideoMediaStreamType() const {
  if (!Video()) {
    return MediaStreamType::NO_SERVICE;
  }
  if (MediaRequestType() == UserMediaRequestType::kDisplayMedia) {
    return should_prefer_current_tab()
               ? MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB
               : MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  }
  if (MediaRequestType() == UserMediaRequestType::kAllScreensMedia) {
    DCHECK(!should_prefer_current_tab());
    return MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
  }
  DCHECK_EQ(UserMediaRequestType::kUserMedia, MediaRequestType());

  // Check if this is a getUserMedia display capture.
  const MediaConstraints& constraints = VideoConstraints();
  String source_constraint =
      constraints.Basic().media_stream_source.Exact().empty()
          ? String()
          : String(constraints.Basic().media_stream_source.Exact()[0]);
  if (!source_constraint.empty()) {
    // This is a getUserMedia display capture call.
    if (source_constraint == blink::kMediaStreamSourceTab) {
      return MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
    } else if (source_constraint == blink::kMediaStreamSourceDesktop ||
               source_constraint == blink::kMediaStreamSourceScreen) {
      return MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
    }
    return MediaStreamType::NO_SERVICE;
  }

  return MediaStreamType::DEVICE_VIDEO_CAPTURE;
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

    // Permissions policy deprecation messages.
    if (Audio()) {
      if (!window->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kMicrophone,
              ReportOptions::kReportOnFailure)) {
        UseCounter::Count(
            window, WebFeature::kMicrophoneDisabledByFeaturePolicyEstimate);
      }
    }
    if (Video() &&
        VideoMediaStreamType() != MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET) {
      if (!window->IsFeatureEnabled(
              mojom::blink::PermissionsPolicyFeature::kCamera,
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
  if (client_)
    client_->RequestUserMedia(this);
}

void UserMediaRequest::Succeed(
    const MediaStreamDescriptorVector& streams_descriptors) {
  DCHECK(!is_resolved_);
  DCHECK(transferred_track_ == nullptr);
  if (!GetExecutionContext())
    return;

  MediaStreamSet::Create(
      GetExecutionContext(), streams_descriptors, media_type_,
      WTF::BindOnce(&UserMediaRequest::OnMediaStreamsInitialized,
                    WrapPersistent(this)));
}

void UserMediaRequest::OnMediaStreamInitialized(MediaStream* stream) {
  OnMediaStreamsInitialized({stream});
}

void UserMediaRequest::OnMediaStreamsInitialized(MediaStreamVector streams) {
  DCHECK(!is_resolved_);

  for (const Member<MediaStream>& stream : streams) {
    MediaStreamTrackVector audio_tracks = stream->getAudioTracks();
    for (const auto& audio_track : audio_tracks)
      audio_track->SetInitialConstraints(audio_);

    MediaStreamTrackVector video_tracks = stream->getVideoTracks();
    for (const auto& video_track : video_tracks)
      video_track->SetInitialConstraints(video_);

    RecordIdentifiabilityMetric(
        surface_, GetExecutionContext(),
        IdentifiabilityBenignStringToken(g_empty_string));
    if (auto* window = GetWindow()) {
      if (media_type_ == UserMediaRequestType::kUserMedia) {
        PeerConnectionTracker::From(*window).TrackGetUserMediaSuccess(this,
                                                                      stream);
      } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
                 media_type_ == UserMediaRequestType::kAllScreensMedia) {
        PeerConnectionTracker::From(*window).TrackGetDisplayMediaSuccess(
            this, stream);
      } else {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }
  // After this call, the execution context may be invalid.
  callbacks_->OnSuccess(streams, capture_controller_);
  is_resolved_ = true;
}

void UserMediaRequest::FailConstraint(const String& constraint_name,
                                      const String& message) {
  DCHECK(!constraint_name.empty());
  DCHECK(!is_resolved_);
  if (!GetExecutionContext())
    return;
  RecordIdentifiabilityMetric(surface_, GetExecutionContext(),
                              IdentifiabilityBenignStringToken(message));
  if (auto* window = GetWindow()) {
    if (media_type_ == UserMediaRequestType::kUserMedia) {
      PeerConnectionTracker::From(*window).TrackGetUserMediaFailure(
          this, "OverConstrainedError", message);
    } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
               media_type_ == UserMediaRequestType::kAllScreensMedia) {
      PeerConnectionTracker::From(*window).TrackGetDisplayMediaFailure(
          this, "OverConstrainedError", message);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
  // After this call, the execution context may be invalid.
  callbacks_->OnError(
      nullptr,
      MakeGarbageCollected<V8MediaStreamError>(
          OverconstrainedError::Create(constraint_name, message)),
      capture_controller_, UserMediaRequestResult::kOverConstrainedError);
  is_resolved_ = true;
}

void UserMediaRequest::Fail(Result error, const String& message) {
  DCHECK(!is_resolved_);
  if (!GetExecutionContext())
    return;
  DOMExceptionCode exception_code = DOMExceptionCode::kNotSupportedError;
  UserMediaRequestResult result_enum =
      UserMediaRequestResult::kNotSupportedError;
  switch (error) {
    case Result::PERMISSION_DENIED:
    case Result::PERMISSION_DISMISSED:
    case Result::KILL_SWITCH_ON:
    case Result::SYSTEM_PERMISSION_DENIED:
      exception_code = DOMExceptionCode::kNotAllowedError;
      result_enum = UserMediaRequestResult::kNotAllowedError;
      break;
    case Result::NO_HARDWARE:
      exception_code = DOMExceptionCode::kNotFoundError;
      result_enum = UserMediaRequestResult::kNotFoundError;
      break;
    case Result::INVALID_STATE:
    case Result::FAILED_DUE_TO_SHUTDOWN:
    case Result::TAB_CAPTURE_FAILURE:
    case Result::SCREEN_CAPTURE_FAILURE:
    case Result::CAPTURE_FAILURE:
      exception_code = DOMExceptionCode::kAbortError;
      result_enum = UserMediaRequestResult::kAbortError;
      break;
    case Result::TRACK_START_FAILURE_AUDIO:
    case Result::TRACK_START_FAILURE_VIDEO:
    case Result::DEVICE_IN_USE:
      exception_code = DOMExceptionCode::kNotReadableError;
      result_enum = UserMediaRequestResult::kNotReadableError;
      break;
    case Result::NOT_SUPPORTED:
      exception_code = DOMExceptionCode::kNotSupportedError;
      result_enum = UserMediaRequestResult::kNotSupportedError;
      break;
    case Result::INVALID_SECURITY_ORIGIN:
      exception_code = DOMExceptionCode::kSecurityError;
      result_enum = UserMediaRequestResult::kSecurityError;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  RecordIdentifiabilityMetric(surface_, GetExecutionContext(),
                              IdentifiabilityBenignStringToken(message));

  if (auto* window = GetWindow()) {
    if (media_type_ == UserMediaRequestType::kUserMedia) {
      PeerConnectionTracker::From(*window).TrackGetUserMediaFailure(
          this, DOMException::GetErrorName(exception_code), message);
    } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
               media_type_ == UserMediaRequestType::kAllScreensMedia) {
      PeerConnectionTracker::From(*window).TrackGetDisplayMediaFailure(
          this, DOMException::GetErrorName(exception_code), message);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  // After this call, the execution context may be invalid.
  callbacks_->OnError(
      nullptr,
      MakeGarbageCollected<V8MediaStreamError>(
          MakeGarbageCollected<DOMException>(exception_code, message)),
      capture_controller_, result_enum);
  is_resolved_ = true;
}

void UserMediaRequest::ContextDestroyed() {
  if (!is_resolved_)
    blink::WebRtcLogMessage("UMR::ContextDestroyed. Request not resolved.");
  if (client_) {
    client_->CancelUserMediaRequest(this);
    if (!is_resolved_) {
      blink::WebRtcLogMessage(base::StringPrintf(
          "UMR::ContextDestroyed. Resolving unsolved request. "
          "audio constraints=%s, video constraints=%s",
          AudioConstraints().ToString().Utf8().c_str(),
          VideoConstraints().ToString().Utf8().c_str()));
      callbacks_->OnError(
          nullptr,
          MakeGarbageCollected<V8MediaStreamError>(
              MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                                 "Context destroyed")),
          capture_controller_, UserMediaRequestResult::kContextDestroyed);
    }
    client_ = nullptr;
  }
}

void UserMediaRequest::SetTransferredTrackComponent(
    MediaStreamComponent* component) {
  transferred_track_->SetComponentImplementation(component);
}

void UserMediaRequest::FinalizeTransferredTrackInitialization(
    const MediaStreamDescriptorVector& streams_descriptors) {
  DCHECK(transferred_track_);
  DCHECK_EQ(streams_descriptors.size(), 1u);
  if (!GetExecutionContext())
    return;

  MediaStream::Create(GetExecutionContext(), streams_descriptors[0],
                      transferred_track_,
                      WTF::BindOnce(&UserMediaRequest::OnMediaStreamInitialized,
                                    WrapPersistent(this)));
}

void UserMediaRequest::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  visitor->Trace(callbacks_);
  visitor->Trace(transferred_track_);
  visitor->Trace(capture_controller_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
