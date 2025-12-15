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
#include "media/base/media_switches.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
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
enum class GetDisplayMediaSystemWindowOrExcludeConstraint {
  kNotSpecified = 0,
  kSystem = 1,
  kWindow = 2,
  kExclude = 3,
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
              BooleanConstraint MediaTrackConstraintSetPlatform::*>::value ||
          std::is_same<decltype(field),
                       BooleanOrStringConstraint
                           MediaTrackConstraintSetPlatform::*>::value,
      "Must use StringConstraint, BooleanConstraint or "
      "BooleanOrStringConstraint");
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

  void CountDeprecation(WebFeature feature) {
    UseCounter::CountDeprecation(context_, feature);
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
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::auto_gain_control)) {
    counter.Count(WebFeature::kMediaStreamConstraintsAutoGainControl);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &MediaTrackConstraintSetPlatform::noise_suppression)) {
    counter.Count(WebFeature::kMediaStreamConstraintsNoiseSuppression);
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

void RecordGetDisplayMediaSystemWindowOrExcludeConstraintUma(
    std::optional<V8DisplayMediaSystemWindowOrExclude::Enum>
        system_window_or_exclude,
    const std::string& histogram_name) {
  GetDisplayMediaSystemWindowOrExcludeConstraint value =
      GetDisplayMediaSystemWindowOrExcludeConstraint::kNotSpecified;
  if (system_window_or_exclude.has_value()) {
    switch (system_window_or_exclude.value()) {
      case V8DisplayMediaSystemWindowOrExclude::Enum::kExclude:
        value = GetDisplayMediaSystemWindowOrExcludeConstraint::kExclude;
        break;
      case V8DisplayMediaSystemWindowOrExclude::Enum::kWindow:
        value = GetDisplayMediaSystemWindowOrExcludeConstraint::kWindow;
        break;
      case V8DisplayMediaSystemWindowOrExclude::Enum::kSystem:
        value = GetDisplayMediaSystemWindowOrExcludeConstraint::kSystem;
        break;
    }
  }
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
  NOTREACHED();
}

void RecordBooleanConstraintUma(std::optional<bool> boolean,
                                const std::string& histogram_name) {
  const GetDisplayMediaBooleanConstraint value =
      (!boolean.has_value() ? GetDisplayMediaBooleanConstraint::kNotSpecified
       : boolean.value()    ? GetDisplayMediaBooleanConstraint::kTrue
                            : GetDisplayMediaBooleanConstraint::kFalse);
  base::UmaHistogramEnumeration(histogram_name, value);
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
  NOTREACHED();
}

}  // namespace

UserMediaRequest* UserMediaRequest::Create(
    ExecutionContext* context,
    UserMediaClient* client,
    UserMediaRequestType media_type,
    const MediaStreamConstraints* options,
    Callbacks* callbacks,
    ExceptionState& exception_state) {
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
  std::optional<bool> restrict_own_audio;

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
    if (!audio.IsNull() && audio.Basic().restrict_own_audio.HasIdeal() &&
        media::IsRestrictOwnAudioSupported()) {
      restrict_own_audio = audio.Basic().restrict_own_audio.Ideal();
    }
  }

  if (!audio.IsNull())
    CountAudioConstraintUses(context, audio);
  if (!video.IsNull())
    CountVideoConstraintUses(context, video);

  UserMediaRequest* const result = MakeGarbageCollected<UserMediaRequest>(
      context, client, media_type, audio, video, options->preferCurrentTab(),
      options->getControllerOr(nullptr), callbacks);

  // The default is to include.
  // Note that this option is no-op if audio is not requested.
  result->set_exclude_system_audio(
      options->hasSystemAudio() &&
      options->systemAudio().AsEnum() ==
          V8DisplayMediaIncludeOrExclude::Enum::kExclude);

  if (RuntimeEnabledFeatures::GetDisplayMediaWindowAudioCaptureEnabled()) {
    // Default is kSystem
    mojom::blink::WindowAudioPreference value =
        mojom::blink::WindowAudioPreference::kSystem;
    if (options->hasWindowAudio()) {
      switch (options->windowAudio().AsEnum()) {
        case V8DisplayMediaSystemWindowOrExclude::Enum::kExclude:
          value = mojom::blink::WindowAudioPreference::kExclude;
          break;
        case V8DisplayMediaSystemWindowOrExclude::Enum::kWindow:
          value = mojom::blink::WindowAudioPreference::kWindow;
          break;
        case V8DisplayMediaSystemWindowOrExclude::Enum::kSystem:
          value = mojom::blink::WindowAudioPreference::kSystem;
          break;
      }
    }
    result->set_window_audio_preference(value);
    if (media_type == UserMediaRequestType::kDisplayMedia) {
      std::optional<V8DisplayMediaSystemWindowOrExclude::Enum>
          window_audio_preference;
      if (options->hasWindowAudio()) {
        window_audio_preference = options->windowAudio().AsEnum();
      }
      RecordGetDisplayMediaSystemWindowOrExcludeConstraintUma(
          window_audio_preference,
          "Media.GetDisplayMedia.Constraints.WindowAudio");
    }
  } else {
    // if the feature is not enabled, we'll set kExclude to never share audio
    // when sharing windows.
    result->set_window_audio_preference(
        mojom::blink::WindowAudioPreference::kExclude);
  }
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
    RecordBooleanConstraintUma(
        suppress_local_audio_playback,
        "Media.GetDisplayMedia.Constraints.SuppressLocalAudioPlayback");
  }
  result->set_restrict_own_audio(restrict_own_audio.value_or(false));
  if (RuntimeEnabledFeatures::RestrictOwnAudioEnabled()) {
    if (media_type == UserMediaRequestType::kDisplayMedia) {
      RecordBooleanConstraintUma(
          restrict_own_audio,
          "Media.GetDisplayMedia.Constraints.RestrictOwnAudio");
    }
  }

  return result;
}

UserMediaRequest* UserMediaRequest::CreateForTesting(
    const MediaConstraints& audio,
    const MediaConstraints& video,
    bool is_user_media) {
  return MakeGarbageCollected<UserMediaRequest>(
      nullptr, nullptr,
      is_user_media ? UserMediaRequestType::kUserMedia
                    : UserMediaRequestType::kDisplayMedia,
      audio, video,
      /*should_prefer_current_tab=*/false,
      /*capture_controller=*/nullptr, /*callbacks=*/nullptr);
}

UserMediaRequest::UserMediaRequest(ExecutionContext* context,
                                   UserMediaClient* client,
                                   UserMediaRequestType media_type,
                                   MediaConstraints audio,
                                   MediaConstraints video,
                                   bool should_prefer_current_tab,
                                   CaptureController* capture_controller,
                                   Callbacks* callbacks)
    : ExecutionContextLifecycleObserver(context),
      media_type_(media_type),
      audio_(audio),
      video_(video),
      capture_controller_(capture_controller),
      should_prefer_current_tab_(should_prefer_current_tab),
      client_(client),
      callbacks_(callbacks) {}

UserMediaRequest::~UserMediaRequest() = default;

UserMediaRequestType UserMediaRequest::MediaRequestType() const {
  return media_type_;
}

bool UserMediaRequest::IsGumExtensionRequest() const {
  auto audio_type = AudioMediaStreamType();
  auto video_type = VideoMediaStreamType();
  if (audio_type == MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE ||
      audio_type == MediaStreamType::GUM_TAB_AUDIO_CAPTURE ||
      video_type == MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE ||
      video_type == MediaStreamType::GUM_TAB_VIDEO_CAPTURE) {
    return true;
  }
  return false;
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
  auto media_type = MediaRequestType();
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    return MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  }
  if (media_type != UserMediaRequestType::kUserMedia) {
    return MediaStreamType::NO_SERVICE;
  }

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
  auto media_type = MediaRequestType();
  if (media_type == UserMediaRequestType::kDisplayMedia) {
    return should_prefer_current_tab()
               ? MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB
               : MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  }
  if (media_type == UserMediaRequestType::kAllScreensMedia) {
    DCHECK(!should_prefer_current_tab());
    return MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET;
  }
  if (media_type != UserMediaRequestType::kUserMedia) {
    return MediaStreamType::NO_SERVICE;
  }

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

bool UserMediaRequest::IsSecureContextUse(String& error_message) {
  LocalDOMWindow* window = GetWindow();

  if (window->IsSecureContext(error_message)) {
    UseCounter::Count(window, WebFeature::kGetUserMediaSecureOrigin);
    window->CountUseOnlyInCrossOriginIframe(
        WebFeature::kGetUserMediaSecureOriginIframe);

    // Permissions policy deprecation messages.
    if (Audio()) {
      if (!window->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kMicrophone,
              ReportOptions::kReportOnFailure)) {
        UseCounter::Count(
            window, WebFeature::kMicrophoneDisabledByFeaturePolicyEstimate);
      }
    }
    if (Video() &&
        VideoMediaStreamType() != MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET) {
      if (!window->IsFeatureEnabled(
              network::mojom::PermissionsPolicyFeature::kCamera,
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
    const GCedMediaStreamDescriptorVector& streams_descriptors) {
  DCHECK(!is_resolved_);
  DCHECK(transferred_track_ == nullptr);
  if (!GetExecutionContext())
    return;

  MediaStreamSet::Create(GetExecutionContext(), streams_descriptors,
                         media_type_,
                         BindOnce(&UserMediaRequest::OnMediaStreamsInitialized,
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

    if (auto* window = GetWindow()) {
      if (media_type_ == UserMediaRequestType::kUserMedia) {
        PeerConnectionTracker::From(*window).TrackGetUserMediaSuccess(this,
                                                                      stream);
      } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
                 media_type_ == UserMediaRequestType::kAllScreensMedia) {
        PeerConnectionTracker::From(*window).TrackGetDisplayMediaSuccess(
            this, stream);
      } else {
        NOTREACHED();
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
  if (auto* window = GetWindow()) {
    if (media_type_ == UserMediaRequestType::kUserMedia) {
      PeerConnectionTracker::From(*window).TrackGetUserMediaFailure(
          this, "OverConstrainedError", message);
    } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
               media_type_ == UserMediaRequestType::kAllScreensMedia) {
      PeerConnectionTracker::From(*window).TrackGetDisplayMediaFailure(
          this, "OverConstrainedError", message);
    } else {
      NOTREACHED();
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

  if (!GetExecutionContext()) {
    return;
  }

  std::optional<DOMExceptionCode> exception_code;
  std::optional<UserMediaRequestResult> result_enum;

  switch (error) {
    case Result::OK:
      NOTREACHED();  // Not a failure.
    case Result::PERMISSION_DENIED:
    case Result::PERMISSION_DENIED_BY_SYSTEM:
    case Result::PERMISSION_DISMISSED:
    case Result::SAFE_BROWSING_OBSERVER:
    case Result::INVALID_DEVICE_TYPE_REQUEST:
      // TODO(crbug.com/453600255): Use `result_enum` kAbortError for
      // INVALID_DEVICE_TYPE_REQUEST once all new enum values are added.
    case Result::ANDROID_CANT_REQUEST_PERMISSION:
    case Result::PERMISSION_DENIED_BY_EMBEDDER_CONTEXT:
    case Result::DLP_PERMISSION_DENIED:
    case Result::NO_TRANSIENT_ACTIVATION:
      // TODO(crbug.com/453600255): Use `result_enum` kInvalidStateError for
      // NO_TRANSIENT_ACTIVATION once all new enum values are added.
    case Result::CAPTURE_NOT_ALLOWED_BY_POLICY:
    case Result::MULTI_CAPTURE_NOT_SUPPORTED:
      // TODO(crbug.com/453600255): Use `result_enum` kNotSupportedError for
      // MULTI_CAPTURE_NOT_SUPPORTED once all new enum values are added.
    case Result::KILL_SWITCH_ON:
    case Result::PERMISSION_DENIED_BY_CONTROLLER:
      exception_code = DOMExceptionCode::kNotAllowedError;
      result_enum = UserMediaRequestResult::kNotAllowedError;
      break;
    case Result::PERMISSION_DENIED_BY_USER:
      exception_code = DOMExceptionCode::kNotAllowedError;
      result_enum = UserMediaRequestResult::kNotAllowedByUserError;
      break;
    case Result::NO_HARDWARE:
      exception_code = DOMExceptionCode::kNotFoundError;
      result_enum = UserMediaRequestResult::kNotFoundError;
      break;
    case Result::INVALID_STATE:
    case Result::INVALID_VIDEO_DEVICE_ID:
    case Result::FAILED_DUE_TO_SHUTDOWN:
      // TODO(crbug.com/453600255): Use `result_enum` kContextDestroyed and
      // `exception_code` kInvalidStateError for
      // FAILED_DUE_TO_SHUTDOWN once all new enum values are added.
    case Result::INVALID_EXTENSION_TYPE_REQUEST:
    case Result::CAPTURED_TAB_DESTROYED:
      // TODO(crbug.com/453600255): Use `result_enum` kNotFoundError for
      // CAPTURED_TAB_DESTROYED once all new enum values are added.
    case Result::CAPTURE_NOT_ENABLED:
    case Result::CAPTURE_NOT_ALLOWED_FOR_LONG_DOMAINS:
    case Result::CAPTURE_FROM_BACKGROUND_PAGE_ON_MAC:
      // TODO(crbug.com/453600255): Use `result_enum` kInvalidStateError for
      // CAPTURE_FROM_BACKGROUND_PAGE_ON_MAC once all new enum values are added.
    case Result::TAB_CAPTURE_FAILURE:
    case Result::STREAM_NOT_FOUND_IN_REGISTRY:
    case Result::REGISTRY_REQUEST_UNVERIFIED:
    case Result::SCREEN_CAPTURE_FAILURE:
    case Result::CAPTURE_FAILURE:
    case Result::START_TIMEOUT:
    case Result::INVALID_DISPLAY_CAPTURE_CONSTRAINTS:
    case Result::INVALID_GUM_TAB_CAPTURE_CONSTRAINTS:
    case Result::INVALID_GUM_SCREEN_CAPTURE_CONSTRAINTS:
      // TODO(crbug.com/453600255): Use `result_enum` kOverconstrainedError for
      // INVALID_DISPLAY_CAPTURE_CONSTRAINTS,
      // INVALID_GUM_TAB_CAPTURE_CONSTRAINTS and
      // INVALID_GUM_SCREEN_CAPTURE_CONSTRAINTS once all new enum values are
      // added.
      exception_code = DOMExceptionCode::kAbortError;
      result_enum = UserMediaRequestResult::kAbortError;
      break;
    case Result::TRACK_START_FAILURE_AUDIO:
    case Result::TRACK_START_FAILURE_VIDEO:
    case Result::AUDIO_DEVICE_SOCKET_ERROR:
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
    case Result::CONSTRAINT_NOT_SATISFIED:
      // TODO(crbug.com/416456028): Either handle these or document why
      // they cannot be encountered by this method.
      NOTREACHED();
    case Result::REQUEST_CANCELLED:  // Deprecated, use FAILED_DUE_TO_SHUTDOWN
      NOTREACHED();  // Not a valid enum value.
  }
  CHECK(exception_code.has_value());
  CHECK(result_enum.has_value());

  if (auto* window = GetWindow()) {
    if (media_type_ == UserMediaRequestType::kUserMedia) {
      PeerConnectionTracker::From(*window).TrackGetUserMediaFailure(
          this, DOMException::GetErrorName(*exception_code), message);
    } else if (media_type_ == UserMediaRequestType::kDisplayMedia ||
               media_type_ == UserMediaRequestType::kAllScreensMedia) {
      PeerConnectionTracker::From(*window).TrackGetDisplayMediaFailure(
          this, DOMException::GetErrorName(*exception_code), message);
    } else {
      NOTREACHED();
    }
  }

  // After this call, the execution context may be invalid.
  callbacks_->OnError(
      nullptr,
      MakeGarbageCollected<V8MediaStreamError>(
          MakeGarbageCollected<DOMException>(*exception_code, message)),
      capture_controller_, *result_enum);
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
              MakeGarbageCollected<DOMException>(
                  DOMExceptionCode::kInvalidStateError, "Context destroyed")),
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
    const GCedMediaStreamDescriptorVector& streams_descriptors) {
  DCHECK(transferred_track_);
  DCHECK_EQ(streams_descriptors.size(), 1u);
  if (!GetExecutionContext())
    return;

  MediaStream::Create(GetExecutionContext(), streams_descriptors[0],
                      transferred_track_,
                      BindOnce(&UserMediaRequest::OnMediaStreamInitialized,
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
