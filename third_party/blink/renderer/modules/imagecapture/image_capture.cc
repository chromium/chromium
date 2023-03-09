// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_fill_light_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_settings_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_photo_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_photo_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constrainbooleanparameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindomstringparameters_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constraindoublerange_double.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainpoint2dparameters_point2dsequence.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture_frame_grabber.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

enum class ImageCapture::MediaTrackConstraintSetType {
  kBasic,
  // TODO(crbug.com/1408091): Remove this. The first advanced constraint set
  //                          should not be special.
  kFirstAdvanced,
  kAdvanced
};

namespace {

using BackgroundBlurMode = media::mojom::blink::BackgroundBlurMode;
using FillLightMode = media::mojom::blink::FillLightMode;
using MeteringMode = media::mojom::blink::MeteringMode;
using RedEyeReduction = media::mojom::blink::RedEyeReduction;

using MediaTrackConstraintSetType = ImageCapture::MediaTrackConstraintSetType;

const char kNoServiceError[] = "ImageCapture service unavailable.";

const char kInvalidStateTrackError[] =
    "The associated Track is in an invalid state";

// This adapter simplifies iteration over all basic and advanced
// MediaTrackConstraintSets in a MediaTrackConstraints.
// A MediaTrackConstraints is itself a (basic) MediaTrackConstraintSet and it
// may contain advanced MediaTrackConstraintSets.
class AllConstraintSets {
 public:
  class ForwardIterator {
   public:
    ForwardIterator(const MediaTrackConstraints* constraints, wtf_size_t index)
        : constraints_(constraints), index_(index) {}
    const MediaTrackConstraintSet* operator*() const {
      if (index_ == 0u) {
        // The basic constraint set.
        return constraints_;
      }
      // The advanced constraint sets.
      wtf_size_t advanced_index = index_ - 1u;
      return constraints_->advanced()[advanced_index];
    }
    ForwardIterator& operator++() {
      ++index_;
      return *this;
    }
    ForwardIterator operator++(int) {
      return ForwardIterator(constraints_, index_++);
    }
    bool operator==(const ForwardIterator& other) const {
      // Equality between iterators related to different MediaTrackConstraints
      // objects is not defined.
      DCHECK_EQ(constraints_, other.constraints_);
      return index_ == other.index_;
    }
    bool operator!=(const ForwardIterator& other) const {
      return !(*this == other);
    }

   private:
    Persistent<const MediaTrackConstraints> constraints_;
    wtf_size_t index_;
  };

  explicit AllConstraintSets(const MediaTrackConstraints* constraints)
      : constraints_(constraints) {}
  ForwardIterator begin() const {
    return ForwardIterator(GetConstraints(), 0u);
  }
  ForwardIterator end() const {
    const auto* constraints = GetConstraints();
    return ForwardIterator(
        constraints,
        constraints->hasAdvanced() ? 1u + constraints->advanced().size() : 1u);
  }

  const MediaTrackConstraints* GetConstraints() const { return constraints_; }

 private:
  Persistent<const MediaTrackConstraints> constraints_;
};

// This adapter simplifies iteration over supported advanced
// MediaTrackConstraintSets in a MediaTrackConstraints.
// A MediaTrackConstraints is itself a (basic) MediaTrackConstraintSet and it
// may contain advanced MediaTrackConstraintSets. So far, only the first
// advanced MediaTrackConstraintSet is supported by this implementation.
// TODO(crbug.com/1408091): Add support for the basic constraint set and for
// advanced constraint sets beyond the first one and remove this helper class.
class AllSupportedConstraintSets {
 public:
  using ForwardIterator = AllConstraintSets::ForwardIterator;

  explicit AllSupportedConstraintSets(const MediaTrackConstraints* constraints)
      : all_constraint_sets_(constraints) {}
  ForwardIterator begin() const {
    const auto* constraints = all_constraint_sets_.GetConstraints();
    return ForwardIterator(constraints, 1u);
  }
  ForwardIterator end() const {
    const auto* constraints = all_constraint_sets_.GetConstraints();
    return ForwardIterator(constraints, constraints->hasAdvanced() &&
                                                !constraints->advanced().empty()
                                            ? 2u
                                            : 1u);
  }

 private:
  AllConstraintSets all_constraint_sets_;
};

using CopyPanTiltZoom = base::StrongAlias<class CopyPanTiltZoomTag, bool>;

template <typename T>
void CopyCommonMembers(const T* source,
                       T* destination,
                       CopyPanTiltZoom copy_pan_tilt_zoom) {
  DCHECK(source);
  DCHECK(destination);
  // Merge any present |source| common members into |destination|.
  if (source->hasWhiteBalanceMode()) {
    destination->setWhiteBalanceMode(source->whiteBalanceMode());
  }
  if (source->hasExposureMode()) {
    destination->setExposureMode(source->exposureMode());
  }
  if (source->hasFocusMode()) {
    destination->setFocusMode(source->focusMode());
  }
  if (source->hasExposureCompensation()) {
    destination->setExposureCompensation(source->exposureCompensation());
  }
  if (source->hasExposureTime()) {
    destination->setExposureTime(source->exposureTime());
  }
  if (source->hasColorTemperature()) {
    destination->setColorTemperature(source->colorTemperature());
  }
  if (source->hasIso()) {
    destination->setIso(source->iso());
  }
  if (source->hasBrightness()) {
    destination->setBrightness(source->brightness());
  }
  if (source->hasContrast()) {
    destination->setContrast(source->contrast());
  }
  if (source->hasSaturation()) {
    destination->setSaturation(source->saturation());
  }
  if (source->hasSharpness()) {
    destination->setSharpness(source->sharpness());
  }
  if (source->hasFocusDistance()) {
    destination->setFocusDistance(source->focusDistance());
  }
  if (copy_pan_tilt_zoom) {
    if (source->hasPan()) {
      destination->setPan(source->pan());
    }
    if (source->hasTilt()) {
      destination->setTilt(source->tilt());
    }
    if (source->hasZoom()) {
      destination->setZoom(source->zoom());
    }
  }
  if (source->hasTorch()) {
    destination->setTorch(source->torch());
  }
  if (source->hasBackgroundBlur()) {
    destination->setBackgroundBlur(source->backgroundBlur());
  }
}

void CopyCapabilities(const MediaTrackCapabilities* source,
                      MediaTrackCapabilities* destination,
                      CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
}

void CopyConstraintSet(const MediaTrackConstraintSet* source,
                       MediaTrackConstraintSet* destination,
                       CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
  if (source->hasPointsOfInterest()) {
    destination->setPointsOfInterest(source->pointsOfInterest());
  }
}

void CopySettings(const MediaTrackSettings* source,
                  MediaTrackSettings* destination,
                  CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
  if (source->hasPointsOfInterest() && !source->pointsOfInterest().empty()) {
    destination->setPointsOfInterest(source->pointsOfInterest());
  }
}

// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove this support enum.
enum class ConstraintType {
  // An empty sequence.
  kEmptySequence,
  // A boolean |false| constraint for a non-boolean constrainable property.
  kBooleanFalse,
  // A boolean |false| constraint for a non-boolean constrainable property.
  kBooleanTrue,
  // A bare value.
  kBareValue,
  kBareValueDOMStringSequence,
  // An empty dictionary constraint.
  kEmptyDictionary,
  // An effectively empty dictionary constraint
  // (members which are empty sequences are ignored).
  kEffectivelyEmptyDictionary,
  // A dictionary constraint with only one effective member: 'ideal'
  // (members which are empty sequences are ignored).
  kIdealDictionary,
  // A dictionary constraint with one to four effective members: at least
  // 'exact', 'max' and/or 'min' and additionally maybe also 'ideal'
  // (members which are empty sequences are ignored).
  kMandatoryDictionary
};

bool IsEmptySequence(bool /*constraint*/) {
  // A boolean is not a sequence so it cannot be an empty sequence.
  return false;
}

bool IsEmptySequence(const V8UnionStringOrStringSequence* constraint) {
  return constraint->IsStringSequence() &&
         constraint->GetAsStringSequence().empty();
}

template <typename Constraint>
ConstraintType GetConstraintType(const Constraint* constraint) {
  DCHECK(constraint);
  if (!constraint->hasExact() && !constraint->hasIdeal()) {
    return ConstraintType::kEmptyDictionary;
  }
  // If an empty list has been given as the value for a constraint, it MUST be
  // interpreted as if the constraint were not specified (in other words,
  // an empty constraint == no constraint).
  // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
  if (constraint->hasExact() && !IsEmptySequence(constraint->exact())) {
    return ConstraintType::kMandatoryDictionary;
  }
  // Ditto.
  if (constraint->hasIdeal() && !IsEmptySequence(constraint->ideal())) {
    return ConstraintType::kIdealDictionary;
  }
  return ConstraintType::kEffectivelyEmptyDictionary;
}

ConstraintType GetConstraintType(const ConstrainDoubleRange* constraint) {
  DCHECK(constraint);
  if (constraint->hasExact() || constraint->hasMax() || constraint->hasMin()) {
    return ConstraintType::kMandatoryDictionary;
  }
  if (constraint->hasIdeal()) {
    return ConstraintType::kIdealDictionary;
  }
  return ConstraintType::kEmptyDictionary;
}

ConstraintType GetConstraintType(
    const V8UnionBooleanOrConstrainBooleanParameters* constraint) {
  DCHECK(constraint);
  if (constraint->IsConstrainBooleanParameters()) {
    return GetConstraintType(constraint->GetAsConstrainBooleanParameters());
  }
  return ConstraintType::kBareValue;
}

ConstraintType GetConstraintType(
    const V8UnionBooleanOrConstrainDoubleRangeOrDouble* constraint) {
  DCHECK(constraint);
  if (constraint->IsBoolean()) {
    return constraint->GetAsBoolean() ? ConstraintType::kBooleanTrue
                                      : ConstraintType::kBooleanFalse;
  }
  if (constraint->IsConstrainDoubleRange()) {
    return GetConstraintType(constraint->GetAsConstrainDoubleRange());
  }
  return ConstraintType::kBareValue;
}

ConstraintType GetConstraintType(
    const V8UnionConstrainDOMStringParametersOrStringOrStringSequence*
        constraint) {
  DCHECK(constraint);
  if (constraint->IsConstrainDOMStringParameters()) {
    return GetConstraintType(constraint->GetAsConstrainDOMStringParameters());
  }
  if (constraint->IsStringSequence()) {
    if (constraint->GetAsStringSequence().empty()) {
      return ConstraintType::kEmptySequence;
    }
    return ConstraintType::kBareValueDOMStringSequence;
  }
  return ConstraintType::kBareValue;
}

ConstraintType GetConstraintType(
    const V8UnionConstrainDoubleRangeOrDouble* constraint) {
  DCHECK(constraint);
  if (constraint->IsConstrainDoubleRange()) {
    return GetConstraintType(constraint->GetAsConstrainDoubleRange());
  }
  return ConstraintType::kBareValue;
}

MediaTrackConstraintSetType GetMediaTrackConstraintSetType(
    const MediaTrackConstraintSet* constraint_set,
    const MediaTrackConstraints* constraints) {
  DCHECK(constraint_set);
  DCHECK(constraints);

  if (constraint_set == constraints) {
    return MediaTrackConstraintSetType::kBasic;
  }

  DCHECK(constraints->hasAdvanced());
  DCHECK(!constraints->advanced().empty());
  if (constraint_set == constraints->advanced()[0]) {
    return MediaTrackConstraintSetType::kFirstAdvanced;
  }
  return MediaTrackConstraintSetType::kAdvanced;
}

bool IsBareValueToBeTreatedAsExact(
    MediaTrackConstraintSetType constraint_set_type) {
  return constraint_set_type != MediaTrackConstraintSetType::kBasic;
}

bool IsBooleanFalseConstraint(
    V8UnionBooleanOrConstrainDoubleRangeOrDouble* constraint) {
  DCHECK(constraint);
  return constraint->IsBoolean() && !constraint->GetAsBoolean();
}

bool TrackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  return track.readyState() != "live" || !track.enabled() || track.muted();
}

MeteringMode ParseMeteringMode(const String& blink_mode) {
  if (blink_mode == "manual")
    return MeteringMode::MANUAL;
  if (blink_mode == "single-shot")
    return MeteringMode::SINGLE_SHOT;
  if (blink_mode == "continuous")
    return MeteringMode::CONTINUOUS;
  if (blink_mode == "none")
    return MeteringMode::NONE;
  NOTREACHED();
  return MeteringMode::NONE;
}

FillLightMode ParseFillLightMode(const String& blink_mode) {
  if (blink_mode == "off")
    return FillLightMode::OFF;
  if (blink_mode == "auto")
    return FillLightMode::AUTO;
  if (blink_mode == "flash")
    return FillLightMode::FLASH;
  NOTREACHED();
  return FillLightMode::OFF;
}

bool ToBooleanMode(BackgroundBlurMode mode) {
  switch (mode) {
    case BackgroundBlurMode::OFF:
      return false;
    case BackgroundBlurMode::BLUR:
      return true;
  }
}

WebString ToString(MeteringMode value) {
  switch (value) {
    case MeteringMode::NONE:
      return WebString::FromUTF8("none");
    case MeteringMode::MANUAL:
      return WebString::FromUTF8("manual");
    case MeteringMode::SINGLE_SHOT:
      return WebString::FromUTF8("single-shot");
    case MeteringMode::CONTINUOUS:
      return WebString::FromUTF8("continuous");
  }
}

V8FillLightMode ToV8FillLightMode(FillLightMode value) {
  switch (value) {
    case FillLightMode::OFF:
      return V8FillLightMode(V8FillLightMode::Enum::kOff);
    case FillLightMode::AUTO:
      return V8FillLightMode(V8FillLightMode::Enum::kAuto);
    case FillLightMode::FLASH:
      return V8FillLightMode(V8FillLightMode::Enum::kFlash);
  }
}

WebString ToString(RedEyeReduction value) {
  switch (value) {
    case RedEyeReduction::NEVER:
      return WebString::FromUTF8("never");
    case RedEyeReduction::ALWAYS:
      return WebString::FromUTF8("always");
    case RedEyeReduction::CONTROLLABLE:
      return WebString::FromUTF8("controllable");
  }
}

MediaSettingsRange* ToMediaSettingsRange(
    const media::mojom::blink::Range& range) {
  MediaSettingsRange* result = MediaSettingsRange::Create();
  result->setMax(range.max);
  result->setMin(range.min);
  result->setStep(range.step);
  return result;
}

using CapabilityExists = base::StrongAlias<class HasCapabilityTag, bool>;

// Check if the existence of a capability satisfies a constraint.
// The check can fail only if the constraint is mandatory ('exact', 'max' or
// 'min' or a bare value to be treated as exact) and is not an empty sequence
// (which MUST be interpreted as if the constraint were not specified).
// Usually the check fails only if the capability does not exists but in
// the case of pan/tilt/zoom: false constraints in advanced constraint sets (to
// be treated as exact) the check fails only if the capability exists.
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.
bool CheckIfCapabilityExistenceSatisfiesConstraintType(
    ConstraintType constraint_type,
    CapabilityExists capability_exists,
    MediaTrackConstraintSetType constraint_set_type) {
  switch (constraint_type) {
    case ConstraintType::kEmptySequence:
      // If an empty list has been given as the value for a constraint, it MUST
      // be interpreted as if the constraint were not specified (in other
      // words, an empty constraint == no constraint).
      // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
      // Thus, it does not matter whether the capability exists.
      return true;
    case ConstraintType::kBooleanFalse:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        // The capability must not exist.
        return !capability_exists;
      }
      // It does not matter whether the capability exists.
      return true;
    case ConstraintType::kBooleanTrue:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        // The capability must exist.
        return !!capability_exists;
      }
      // It does not matter whether the capability exists.
      return true;
    case ConstraintType::kBareValue:
    case ConstraintType::kBareValueDOMStringSequence:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        // The capability must exist.
        return !!capability_exists;
      }
      // It does not matter whether the capability exists.
      return true;
    case ConstraintType::kEmptyDictionary:
    case ConstraintType::kEffectivelyEmptyDictionary:
    case ConstraintType::kIdealDictionary:
      // It does not matter whether the capability exists.
      return true;
    case ConstraintType::kMandatoryDictionary:
      // The capability must exist.
      return !!capability_exists;
  }
}

template <typename Constraint>
bool CheckIfCapabilityExistenceSatisfiesConstraint(
    const Constraint* constraint,
    CapabilityExists capability_exists,
    MediaTrackConstraintSetType constraint_set_type) {
  return CheckIfCapabilityExistenceSatisfiesConstraintType(
      GetConstraintType(constraint), capability_exists, constraint_set_type);
}

}  // anonymous namespace

ImageCapture* ImageCapture::Create(ExecutionContext* context,
                                   MediaStreamTrack* track,
                                   ExceptionState& exception_state) {
  if (track->kind() != "video") {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot create an ImageCapturer from a non-video Track.");
    return nullptr;
  }

  // The initial PTZ permission comes from the internal ImageCapture object of
  // the track, if already created.
  bool pan_tilt_zoom_allowed =
      (track->GetImageCapture() &&
       track->GetImageCapture()->HasPanTiltZoomPermissionGranted());

  return MakeGarbageCollected<ImageCapture>(
      context, track, pan_tilt_zoom_allowed, base::DoNothing());
}

ImageCapture::~ImageCapture() {
  // There should be no more outstanding |service_requests_| at this point
  // since each of them holds a persistent handle to this object.
  DCHECK(service_requests_.empty());
}

void ImageCapture::ContextDestroyed() {
  service_requests_.clear();
}

ScriptPromise ImageCapture::getPhotoCapabilities(ScriptState* script_state) {
  return GetMojoPhotoState(
      script_state, WTF::BindOnce(&ImageCapture::ResolveWithPhotoCapabilities,
                                  WrapPersistent(this)));
}

ScriptPromise ImageCapture::getPhotoSettings(ScriptState* script_state) {
  return GetMojoPhotoState(
      script_state, WTF::BindOnce(&ImageCapture::ResolveWithPhotoSettings,
                                  WrapPersistent(this)));
}

ScriptPromise ImageCapture::takePhoto(ScriptState* script_state,
                                      const PhotoSettings* photo_settings) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::takePhoto", TRACE_EVENT_SCOPE_PROCESS);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  // TODO(mcasas): should be using a mojo::StructTraits instead.
  auto settings = media::mojom::blink::PhotoSettings::New();

  settings->has_height = photo_settings->hasImageHeight();
  if (settings->has_height) {
    const double height = photo_settings->imageHeight();
    if (photo_capabilities_ && photo_capabilities_->hasImageHeight() &&
        (height < photo_capabilities_->imageHeight()->min() ||
         height > photo_capabilities_->imageHeight()->max())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "imageHeight setting out of range"));
      return promise;
    }
    settings->height = height;
  }
  settings->has_width = photo_settings->hasImageWidth();
  if (settings->has_width) {
    const double width = photo_settings->imageWidth();
    if (photo_capabilities_ && photo_capabilities_->hasImageWidth() &&
        (width < photo_capabilities_->imageWidth()->min() ||
         width > photo_capabilities_->imageWidth()->max())) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "imageWidth setting out of range"));
      return promise;
    }
    settings->width = width;
  }

  settings->has_red_eye_reduction = photo_settings->hasRedEyeReduction();
  if (settings->has_red_eye_reduction) {
    if (photo_capabilities_ && photo_capabilities_->hasRedEyeReduction() &&
        photo_capabilities_->redEyeReduction() !=
            V8RedEyeReduction::Enum::kControllable) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "redEyeReduction is not controllable."));
      return promise;
    }
    settings->red_eye_reduction = photo_settings->redEyeReduction();
  }

  settings->has_fill_light_mode = photo_settings->hasFillLightMode();
  if (settings->has_fill_light_mode) {
    const String fill_light_mode = photo_settings->fillLightMode();
    if (photo_capabilities_ && photo_capabilities_->hasFillLightMode() &&
        photo_capabilities_->fillLightMode().Find(fill_light_mode) ==
            kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported fillLightMode"));
      return promise;
    }
    settings->fill_light_mode = ParseFillLightMode(fill_light_mode);
  }

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/true));
  return promise;
}

ScriptPromise ImageCapture::grabFrame(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  // Create |m_frameGrabber| the first time.
  if (!frame_grabber_) {
    frame_grabber_ = std::make_unique<ImageCaptureFrameGrabber>();
  }

  if (!frame_grabber_) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "Couldn't create platform resources"));
    return promise;
  }

  auto resolver_callback_adapter =
      std::make_unique<CallbackPromiseAdapter<ImageBitmap, void>>(resolver);
  frame_grabber_->GrabFrame(stream_track_->Component(),
                            std::move(resolver_callback_adapter),
                            ExecutionContext::From(script_state)
                                ->GetTaskRunner(TaskType::kDOMManipulation));

  return promise;
}

void ImageCapture::UpdateAndCheckMediaTrackSettingsAndCapabilities(
    base::OnceCallback<void(bool)> callback) {
  service_->GetPhotoState(
      stream_track_->Component()->Source()->Id(),
      WTF::BindOnce(&ImageCapture::GotPhotoState, WrapPersistent(this),
                    std::move(callback)));
}

void ImageCapture::GotPhotoState(
    base::OnceCallback<void(bool)> callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  MediaTrackSettings* settings = MediaTrackSettings::Create();
  MediaTrackCapabilities* capabilities = MediaTrackCapabilities::Create();

  // Take a snapshot of local track settings and capabilities.
  CopySettings(settings_, settings, CopyPanTiltZoom(true));
  CopyCapabilities(capabilities_, capabilities, CopyPanTiltZoom(true));

  // Update local track settings and capabilities.
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  // Check whether background blur settings and capabilities have changed.
  if (settings_->hasBackgroundBlur() != settings->hasBackgroundBlur() ||
      (settings_->hasBackgroundBlur() &&
       settings_->backgroundBlur() != settings->backgroundBlur()) ||
      capabilities_->hasBackgroundBlur() != capabilities->hasBackgroundBlur() ||
      (capabilities_->hasBackgroundBlur() &&
       capabilities_->backgroundBlur() != capabilities->backgroundBlur())) {
    std::move(callback).Run(true);
    return;
  }

  std::move(callback).Run(false);
}

bool ImageCapture::CheckAndApplyMediaTrackConstraintsToSettings(
    media::mojom::blink::PhotoSettings* settings,
    const MediaTrackConstraints* constraints,
    ScriptPromiseResolver* resolver) {
  if (!IsPageVisible()) {
    for (const MediaTrackConstraintSet* constraint_set :
         AllSupportedConstraintSets(constraints)) {
      if ((constraint_set->hasPan() &&
           !IsBooleanFalseConstraint(constraint_set->pan())) ||
          (constraint_set->hasTilt() &&
           !IsBooleanFalseConstraint(constraint_set->tilt())) ||
          (constraint_set->hasZoom() &&
           !IsBooleanFalseConstraint(constraint_set->zoom()))) {
        resolver->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kSecurityError, "the page is not visible"));
        return false;
      }
    }
  }

  MediaTrackConstraintSet* temp_constraint_set =
      current_constraint_set_ ? current_constraint_set_.Get()
                              : MediaTrackConstraintSet::Create();

  for (const MediaTrackConstraintSet* constraint_set :
       AllSupportedConstraintSets(constraints)) {
    const MediaTrackConstraintSetType constraint_set_type =
        GetMediaTrackConstraintSetType(constraint_set, constraints);

    // TODO(crbug.com/1408091): Add support for the basic constraint set and for
    // advanced constraint sets beyond the first one and remove check.
    DCHECK_EQ(constraint_set, constraints->advanced()[0]);

    if (absl::optional<const char*> name =
            GetConstraintWithCapabilityExistenceMismatch(constraint_set,
                                                         constraint_set_type)) {
      resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
          name.value(), "Unsupported constraint"));
      return false;
    }

    // TODO(mcasas): support other Mode types beyond simple string i.e. the
    // equivalents of "sequence<DOMString>"" or "ConstrainDOMStringParameters".
    settings->has_white_balance_mode =
        constraint_set->hasWhiteBalanceMode() &&
        constraint_set->whiteBalanceMode()->IsString();
    if (settings->has_white_balance_mode) {
      const auto white_balance_mode =
          constraint_set->whiteBalanceMode()->GetAsString();
      if (capabilities_->whiteBalanceMode().Find(white_balance_mode) ==
          kNotFound) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "whiteBalanceMode", "Unsupported whiteBalanceMode."));
        return false;
      }
      temp_constraint_set->setWhiteBalanceMode(
          constraint_set->whiteBalanceMode());
      settings->white_balance_mode = ParseMeteringMode(white_balance_mode);
    }
    settings->has_exposure_mode = constraint_set->hasExposureMode() &&
                                  constraint_set->exposureMode()->IsString();
    if (settings->has_exposure_mode) {
      const auto exposure_mode = constraint_set->exposureMode()->GetAsString();
      if (capabilities_->exposureMode().Find(exposure_mode) == kNotFound) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "exposureMode", "Unsupported exposureMode."));
        return false;
      }
      temp_constraint_set->setExposureMode(constraint_set->exposureMode());
      settings->exposure_mode = ParseMeteringMode(exposure_mode);
    }

    settings->has_focus_mode = constraint_set->hasFocusMode() &&
                               constraint_set->focusMode()->IsString();
    if (settings->has_focus_mode) {
      const auto focus_mode = constraint_set->focusMode()->GetAsString();
      if (capabilities_->focusMode().Find(focus_mode) == kNotFound) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "focusMode", "Unsupported focusMode."));
        return false;
      }
      temp_constraint_set->setFocusMode(constraint_set->focusMode());
      settings->focus_mode = ParseMeteringMode(focus_mode);
    }

    // TODO(mcasas): support ConstrainPoint2DParameters.
    if (constraint_set->hasPointsOfInterest() &&
        constraint_set->pointsOfInterest()->IsPoint2DSequence()) {
      for (const auto& point :
           constraint_set->pointsOfInterest()->GetAsPoint2DSequence()) {
        auto mojo_point = media::mojom::blink::Point2D::New();
        mojo_point->x = point->x();
        mojo_point->y = point->y();
        settings->points_of_interest.push_back(std::move(mojo_point));
      }
      temp_constraint_set->setPointsOfInterest(
          constraint_set->pointsOfInterest());
    }

    // TODO(mcasas): support ConstrainDoubleRange where applicable.
    settings->has_exposure_compensation =
        constraint_set->hasExposureCompensation() &&
        constraint_set->exposureCompensation()->IsDouble();
    if (settings->has_exposure_compensation) {
      const auto exposure_compensation =
          constraint_set->exposureCompensation()->GetAsDouble();
      if (exposure_compensation <
              capabilities_->exposureCompensation()->min() ||
          exposure_compensation >
              capabilities_->exposureCompensation()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "exposureCompensation",
            "exposureCompensation setting out of range"));
        return false;
      }
      temp_constraint_set->setExposureCompensation(
          constraint_set->exposureCompensation());
      settings->exposure_compensation = exposure_compensation;
    }

    settings->has_exposure_time = constraint_set->hasExposureTime() &&
                                  constraint_set->exposureTime()->IsDouble();
    if (settings->has_exposure_time) {
      const auto exposure_time = constraint_set->exposureTime()->GetAsDouble();
      if (exposure_time < capabilities_->exposureTime()->min() ||
          exposure_time > capabilities_->exposureTime()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "exposureTime", "exposureTime setting out of range"));
        return false;
      }
      temp_constraint_set->setExposureTime(constraint_set->exposureTime());
      settings->exposure_time = exposure_time;
    }
    settings->has_color_temperature =
        constraint_set->hasColorTemperature() &&
        constraint_set->colorTemperature()->IsDouble();
    if (settings->has_color_temperature) {
      const auto color_temperature =
          constraint_set->colorTemperature()->GetAsDouble();
      if (color_temperature < capabilities_->colorTemperature()->min() ||
          color_temperature > capabilities_->colorTemperature()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "colorTemperature", "colorTemperature setting out of range"));
        return false;
      }
      temp_constraint_set->setColorTemperature(
          constraint_set->colorTemperature());
      settings->color_temperature = color_temperature;
    }
    settings->has_iso =
        constraint_set->hasIso() && constraint_set->iso()->IsDouble();
    if (settings->has_iso) {
      const auto iso = constraint_set->iso()->GetAsDouble();
      if (iso < capabilities_->iso()->min() ||
          iso > capabilities_->iso()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "iso", "iso setting out of range"));
        return false;
      }
      temp_constraint_set->setIso(constraint_set->iso());
      settings->iso = iso;
    }

    settings->has_brightness = constraint_set->hasBrightness() &&
                               constraint_set->brightness()->IsDouble();
    if (settings->has_brightness) {
      const auto brightness = constraint_set->brightness()->GetAsDouble();
      if (brightness < capabilities_->brightness()->min() ||
          brightness > capabilities_->brightness()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "brightness", "brightness setting out of range"));
        return false;
      }
      temp_constraint_set->setBrightness(constraint_set->brightness());
      settings->brightness = brightness;
    }
    settings->has_contrast =
        constraint_set->hasContrast() && constraint_set->contrast()->IsDouble();
    if (settings->has_contrast) {
      const auto contrast = constraint_set->contrast()->GetAsDouble();
      if (contrast < capabilities_->contrast()->min() ||
          contrast > capabilities_->contrast()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "contrast", "contrast setting out of range"));
        return false;
      }
      temp_constraint_set->setContrast(constraint_set->contrast());
      settings->contrast = contrast;
    }
    settings->has_saturation = constraint_set->hasSaturation() &&
                               constraint_set->saturation()->IsDouble();
    if (settings->has_saturation) {
      const auto saturation = constraint_set->saturation()->GetAsDouble();
      if (saturation < capabilities_->saturation()->min() ||
          saturation > capabilities_->saturation()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "saturation", "saturation setting out of range"));
        return false;
      }
      temp_constraint_set->setSaturation(constraint_set->saturation());
      settings->saturation = saturation;
    }
    settings->has_sharpness = constraint_set->hasSharpness() &&
                              constraint_set->sharpness()->IsDouble();
    if (settings->has_sharpness) {
      const auto sharpness = constraint_set->sharpness()->GetAsDouble();
      if (sharpness < capabilities_->sharpness()->min() ||
          sharpness > capabilities_->sharpness()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "sharpness", "sharpness setting out of range"));
        return false;
      }
      temp_constraint_set->setSharpness(constraint_set->sharpness());
      settings->sharpness = sharpness;
    }

    settings->has_focus_distance = constraint_set->hasFocusDistance() &&
                                   constraint_set->focusDistance()->IsDouble();
    if (settings->has_focus_distance) {
      const auto focus_distance =
          constraint_set->focusDistance()->GetAsDouble();
      if (focus_distance < capabilities_->focusDistance()->min() ||
          focus_distance > capabilities_->focusDistance()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "focusDistance", "focusDistance setting out of range"));
        return false;
      }
      temp_constraint_set->setFocusDistance(constraint_set->focusDistance());
      settings->focus_distance = focus_distance;
    }

    settings->has_pan =
        constraint_set->hasPan() && constraint_set->pan()->IsDouble();
    if (settings->has_pan) {
      const auto pan = constraint_set->pan()->GetAsDouble();
      if (pan < capabilities_->pan()->min() ||
          pan > capabilities_->pan()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "pan", "pan setting out of range"));
        return false;
      }
      temp_constraint_set->setPan(constraint_set->pan());
      settings->pan = pan;
    }

    settings->has_tilt =
        constraint_set->hasTilt() && constraint_set->tilt()->IsDouble();
    if (settings->has_tilt) {
      const auto tilt = constraint_set->tilt()->GetAsDouble();
      if (tilt < capabilities_->tilt()->min() ||
          tilt > capabilities_->tilt()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "tilt", "tilt setting out of range"));
        return false;
      }
      temp_constraint_set->setTilt(constraint_set->tilt());
      settings->tilt = tilt;
    }

    settings->has_zoom =
        constraint_set->hasZoom() && constraint_set->zoom()->IsDouble();
    if (settings->has_zoom) {
      const auto zoom = constraint_set->zoom()->GetAsDouble();
      if (zoom < capabilities_->zoom()->min() ||
          zoom > capabilities_->zoom()->max()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "zoom", "zoom setting out of range"));
        return false;
      }
      temp_constraint_set->setZoom(constraint_set->zoom());
      settings->zoom = zoom;
    }

    // TODO(mcasas): support ConstrainBooleanParameters where applicable.
    settings->has_torch =
        constraint_set->hasTorch() && constraint_set->torch()->IsBoolean();
    if (settings->has_torch) {
      const auto torch = constraint_set->torch()->GetAsBoolean();
      if (torch && !capabilities_->torch()) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "torch", "torch not supported"));
        return false;
      }
      temp_constraint_set->setTorch(constraint_set->torch());
      settings->torch = torch;
    }

    settings->has_background_blur_mode =
        constraint_set->hasBackgroundBlur() &&
        constraint_set->backgroundBlur()->IsBoolean();
    if (settings->has_background_blur_mode) {
      const auto background_blur =
          constraint_set->backgroundBlur()->GetAsBoolean();
      if (!base::Contains(capabilities_->backgroundBlur(), background_blur)) {
        resolver->Reject(MakeGarbageCollected<OverconstrainedError>(
            "backgroundBlur", "backgroundBlur setting value not supported"));
        return false;
      }
      temp_constraint_set->setBackgroundBlur(constraint_set->backgroundBlur());
      settings->background_blur_mode =
          background_blur ? BackgroundBlurMode::BLUR : BackgroundBlurMode::OFF;
    }
  }

  current_constraint_set_ = temp_constraint_set;

  return true;
}

void ImageCapture::GetMediaTrackCapabilities(
    MediaTrackCapabilities* capabilities) const {
  // Merge any present |capabilities_| members into |capabilities|.
  CopyCapabilities(capabilities_, capabilities,
                   CopyPanTiltZoom(HasPanTiltZoomPermissionGranted()));
}

// TODO(mcasas): make the implementation fully Spec compliant, see the TODOs
// inside the method, https://crbug.com/708723.
void ImageCapture::SetMediaTrackConstraints(
    ScriptPromiseResolver* resolver,
    const MediaTrackConstraints* constraints) {
  DCHECK(constraints);
  if (!constraints->hasAdvanced() || constraints->advanced().empty()) {
    // TODO(crbug.com/1408091): This is not spec compliant.
    // If there are no advanced constraints (but only required and optional
    // constraints), the required and optional constraints should be applied.
    ClearMediaTrackConstraints();
    resolver->Resolve();
    return;
  }

  ExecutionContext* context = GetExecutionContext();
  for (const MediaTrackConstraintSet* constraint_set :
       AllSupportedConstraintSets(constraints)) {
    if (constraint_set->hasWhiteBalanceMode()) {
      UseCounter::Count(context, WebFeature::kImageCaptureWhiteBalanceMode);
    }
    if (constraint_set->hasExposureMode()) {
      UseCounter::Count(context, WebFeature::kImageCaptureExposureMode);
    }
    if (constraint_set->hasFocusMode()) {
      UseCounter::Count(context, WebFeature::kImageCaptureFocusMode);
    }
    if (constraint_set->hasPointsOfInterest()) {
      UseCounter::Count(context, WebFeature::kImageCapturePointsOfInterest);
    }
    if (constraint_set->hasExposureCompensation()) {
      UseCounter::Count(context, WebFeature::kImageCaptureExposureCompensation);
    }
    if (constraint_set->hasExposureTime()) {
      UseCounter::Count(context, WebFeature::kImageCaptureExposureTime);
    }
    if (constraint_set->hasColorTemperature()) {
      UseCounter::Count(context, WebFeature::kImageCaptureColorTemperature);
    }
    if (constraint_set->hasIso()) {
      UseCounter::Count(context, WebFeature::kImageCaptureIso);
    }
    if (constraint_set->hasBrightness()) {
      UseCounter::Count(context, WebFeature::kImageCaptureBrightness);
    }
    if (constraint_set->hasContrast()) {
      UseCounter::Count(context, WebFeature::kImageCaptureContrast);
    }
    if (constraint_set->hasSaturation()) {
      UseCounter::Count(context, WebFeature::kImageCaptureSaturation);
    }
    if (constraint_set->hasSharpness()) {
      UseCounter::Count(context, WebFeature::kImageCaptureSharpness);
    }
    if (constraint_set->hasFocusDistance()) {
      UseCounter::Count(context, WebFeature::kImageCaptureFocusDistance);
    }
    if (constraint_set->hasPan()) {
      UseCounter::Count(context, WebFeature::kImageCapturePan);
    }
    if (constraint_set->hasTilt()) {
      UseCounter::Count(context, WebFeature::kImageCaptureTilt);
    }
    if (constraint_set->hasZoom()) {
      UseCounter::Count(context, WebFeature::kImageCaptureZoom);
    }
    if (constraint_set->hasTorch()) {
      UseCounter::Count(context, WebFeature::kImageCaptureTorch);
    }
    // TODO(eero.hakkinen@intel.com): count how many times backgroundBlur is
    // used.
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return;
  }

  auto settings = media::mojom::blink::PhotoSettings::New();

  if (!CheckAndApplyMediaTrackConstraintsToSettings(&*settings, constraints,
                                                    resolver)) {
    return;
  }

  service_requests_.insert(resolver);

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/false));
}

void ImageCapture::SetPanTiltZoomSettingsFromTrack(
    base::OnceClosure initialized_callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  auto* video_track = MediaStreamVideoTrack::From(stream_track_->Component());
  DCHECK(video_track);

  absl::optional<double> pan = video_track->pan();
  absl::optional<double> tilt = video_track->tilt();
  absl::optional<double> zoom = video_track->zoom();

  const bool ptz_requested =
      pan.has_value() || tilt.has_value() || zoom.has_value();
  const bool ptz_supported = capabilities_->hasPan() ||
                             capabilities_->hasTilt() ||
                             capabilities_->hasZoom();
  if (!ptz_supported || !ptz_requested || !HasPanTiltZoomPermissionGranted() ||
      !service_.is_bound()) {
    std::move(initialized_callback).Run();
    return;
  }

  ExecutionContext* context = GetExecutionContext();
  if (pan.has_value())
    UseCounter::Count(context, WebFeature::kImageCapturePan);
  if (tilt.has_value())
    UseCounter::Count(context, WebFeature::kImageCaptureTilt);
  if (zoom.has_value())
    UseCounter::Count(context, WebFeature::kImageCaptureZoom);

  auto settings = media::mojom::blink::PhotoSettings::New();

  if (capabilities_->hasPan() && pan.has_value() &&
      pan.value() >= capabilities_->pan()->min() &&
      pan.value() <= capabilities_->pan()->max()) {
    settings->has_pan = true;
    settings->pan = pan.value();
  }
  if (capabilities_->hasTilt() && tilt.has_value() &&
      tilt.value() >= capabilities_->tilt()->min() &&
      tilt.value() <= capabilities_->tilt()->max()) {
    settings->has_tilt = true;
    settings->tilt = tilt.value();
  }
  if (capabilities_->hasZoom() && zoom.has_value() &&
      zoom.value() >= capabilities_->zoom()->min() &&
      zoom.value() <= capabilities_->zoom()->max()) {
    settings->has_zoom = true;
    settings->zoom = zoom.value();
  }

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnSetPanTiltZoomSettingsFromTrack,
                    WrapPersistent(this), std::move(initialized_callback)));
}

void ImageCapture::OnSetPanTiltZoomSettingsFromTrack(
    base::OnceClosure done_callback,
    bool result) {
  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::UpdateMediaTrackSettingsAndCapabilities,
                    WrapPersistent(this), std::move(done_callback)));
}

MediaTrackConstraints* ImageCapture::GetMediaTrackConstraints() const {
  if (!current_constraint_set_) {
    return nullptr;
  }
  MediaTrackConstraints* constraints = MediaTrackConstraints::Create();
  HeapVector<Member<MediaTrackConstraintSet>> advanced_constraints;
  advanced_constraints.push_back(current_constraint_set_);
  constraints->setAdvanced(advanced_constraints);
  return constraints;
}

void ImageCapture::ClearMediaTrackConstraints() {
  current_constraint_set_ = nullptr;

  // TODO(mcasas): Clear also any PhotoSettings that the device might have got
  // configured, for that we need to know a "default" state of the device; take
  // a snapshot upon first opening. https://crbug.com/700607.
}

void ImageCapture::GetMediaTrackSettings(MediaTrackSettings* settings) const {
  // Merge any present |settings_| members into |settings|.
  CopySettings(settings_, settings,
               CopyPanTiltZoom(HasPanTiltZoomPermissionGranted()));
}

ImageCapture::ImageCapture(ExecutionContext* context,
                           MediaStreamTrack* track,
                           bool pan_tilt_zoom_allowed,
                           base::OnceClosure initialized_callback)
    : ExecutionContextLifecycleObserver(context),
      stream_track_(track),
      service_(context),
      pan_tilt_zoom_permission_(pan_tilt_zoom_allowed
                                    ? mojom::blink::PermissionStatus::GRANTED
                                    : mojom::blink::PermissionStatus::ASK),
      permission_service_(context),
      permission_observer_receiver_(this, context),
      capabilities_(MediaTrackCapabilities::Create()),
      settings_(MediaTrackSettings::Create()),
      photo_settings_(PhotoSettings::Create()) {
  DCHECK(stream_track_);
  DCHECK(!service_.is_bound());
  DCHECK(!permission_service_.is_bound());

  // This object may be constructed over an ExecutionContext that has already
  // been detached. In this case the ImageCapture service will not be available.
  if (!DomWindow())
    return;

  DomWindow()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(
          context->GetTaskRunner(TaskType::kDOMManipulation)));

  service_.set_disconnect_handler(WTF::BindOnce(
      &ImageCapture::OnServiceConnectionError, WrapWeakPersistent(this)));

  // Launch a retrieval of the current photo state, which arrive asynchronously
  // to avoid blocking the main UI thread.
  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::SetPanTiltZoomSettingsFromTrack,
                    WrapPersistent(this), std::move(initialized_callback)));

  ConnectToPermissionService(
      context, permission_service_.BindNewPipeAndPassReceiver(
                   context->GetTaskRunner(TaskType::kMiscPlatformAPI)));

  mojo::PendingRemote<mojom::blink::PermissionObserver> observer;
  permission_observer_receiver_.Bind(
      observer.InitWithNewPipeAndPassReceiver(),
      context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  permission_service_->AddPermissionObserver(
      CreateVideoCapturePermissionDescriptor(/*pan_tilt_zoom=*/true),
      pan_tilt_zoom_permission_, std::move(observer));
}

void ImageCapture::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  pan_tilt_zoom_permission_ = status;
}

bool ImageCapture::HasPanTiltZoomPermissionGranted() const {
  return pan_tilt_zoom_permission_ == mojom::blink::PermissionStatus::GRANTED;
}

ScriptPromise ImageCapture::GetMojoPhotoState(
    ScriptState* script_state,
    PromiseResolverFunction resolver_cb) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return promise;
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return promise;
  }
  service_requests_.insert(resolver);

  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(resolver_cb),
                    /*trigger_take_photo=*/false));
  return promise;
}

void ImageCapture::OnMojoGetPhotoState(
    ScriptPromiseResolver* resolver,
    PromiseResolverFunction resolve_function,
    bool trigger_take_photo,
    media::mojom::blink::PhotoStatePtr photo_state) {
  DCHECK(service_requests_.Contains(resolver));

  if (photo_state.is_null()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
    service_requests_.erase(resolver);
    return;
  }

  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, kInvalidStateTrackError));
    service_requests_.erase(resolver);
    return;
  }

  photo_settings_ = PhotoSettings::Create();
  photo_settings_->setImageHeight(photo_state->height->current);
  photo_settings_->setImageWidth(photo_state->width->current);
  // TODO(mcasas): collect the remaining two entries https://crbug.com/732521.

  photo_capabilities_ = MakeGarbageCollected<PhotoCapabilities>();
  photo_capabilities_->setRedEyeReduction(
      ToString(photo_state->red_eye_reduction));
  if (photo_state->height->min != 0 || photo_state->height->max != 0) {
    photo_capabilities_->setImageHeight(
        ToMediaSettingsRange(*photo_state->height));
  }
  if (photo_state->width->min != 0 || photo_state->width->max != 0) {
    photo_capabilities_->setImageWidth(
        ToMediaSettingsRange(*photo_state->width));
  }

  WTF::Vector<V8FillLightMode> fill_light_mode;
  for (const auto& mode : photo_state->fill_light_mode) {
    fill_light_mode.push_back(ToV8FillLightMode(mode));
  }
  if (!fill_light_mode.empty())
    photo_capabilities_->setFillLightMode(fill_light_mode);

  // Update the local track photo_state cache.
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  if (trigger_take_photo) {
    service_->TakePhoto(
        SourceId(),
        WTF::BindOnce(&ImageCapture::OnMojoTakePhoto, WrapPersistent(this),
                      WrapPersistent(resolver)));
    return;
  }

  std::move(resolve_function).Run(resolver);
  service_requests_.erase(resolver);
}

void ImageCapture::OnMojoSetPhotoOptions(ScriptPromiseResolver* resolver,
                                         bool trigger_take_photo,
                                         bool result) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoSetPhotoOptions",
                       TRACE_EVENT_SCOPE_PROCESS);

  if (!result) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "setPhotoOptions failed"));
    service_requests_.erase(resolver);
    return;
  }

  auto resolver_cb =
      WTF::BindOnce(&ImageCapture::ResolveWithNothing, WrapPersistent(this));

  // Retrieve the current device status after setting the options.
  service_->GetPhotoState(
      SourceId(), WTF::BindOnce(&ImageCapture::OnMojoGetPhotoState,
                                WrapPersistent(this), WrapPersistent(resolver),
                                std::move(resolver_cb), trigger_take_photo));
}

void ImageCapture::OnMojoTakePhoto(ScriptPromiseResolver* resolver,
                                   media::mojom::blink::BlobPtr blob) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                       "ImageCapture::OnMojoTakePhoto",
                       TRACE_EVENT_SCOPE_PROCESS);

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.empty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
  } else {
    resolver->Resolve(
        Blob::Create(blob->data.data(), blob->data.size(), blob->mime_type));
  }
  service_requests_.erase(resolver);
}

void ImageCapture::UpdateMediaTrackSettingsAndCapabilities(
    base::OnceClosure initialized_callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  if (!photo_state) {
    std::move(initialized_callback).Run();
    return;
  }

  WTF::Vector<WTF::String> supported_white_balance_modes;
  supported_white_balance_modes.ReserveInitialCapacity(
      photo_state->supported_white_balance_modes.size());
  for (const auto& supported_mode : photo_state->supported_white_balance_modes)
    supported_white_balance_modes.push_back(ToString(supported_mode));
  if (!supported_white_balance_modes.empty()) {
    capabilities_->setWhiteBalanceMode(
        std::move(supported_white_balance_modes));
    settings_->setWhiteBalanceMode(
        ToString(photo_state->current_white_balance_mode));
  }

  WTF::Vector<WTF::String> supported_exposure_modes;
  supported_exposure_modes.ReserveInitialCapacity(
      photo_state->supported_exposure_modes.size());
  for (const auto& supported_mode : photo_state->supported_exposure_modes)
    supported_exposure_modes.push_back(ToString(supported_mode));
  if (!supported_exposure_modes.empty()) {
    capabilities_->setExposureMode(std::move(supported_exposure_modes));
    settings_->setExposureMode(ToString(photo_state->current_exposure_mode));
  }

  WTF::Vector<WTF::String> supported_focus_modes;
  supported_focus_modes.ReserveInitialCapacity(
      photo_state->supported_focus_modes.size());
  for (const auto& supported_mode : photo_state->supported_focus_modes)
    supported_focus_modes.push_back(ToString(supported_mode));
  if (!supported_focus_modes.empty()) {
    capabilities_->setFocusMode(std::move(supported_focus_modes));
    settings_->setFocusMode(ToString(photo_state->current_focus_mode));
  }

  HeapVector<Member<Point2D>> current_points_of_interest;
  if (!photo_state->points_of_interest.empty()) {
    for (const auto& point : photo_state->points_of_interest) {
      Point2D* web_point = Point2D::Create();
      web_point->setX(point->x);
      web_point->setY(point->y);
      current_points_of_interest.push_back(web_point);
    }
  }
  settings_->setPointsOfInterest(current_points_of_interest);

  if (photo_state->exposure_compensation->max !=
      photo_state->exposure_compensation->min) {
    capabilities_->setExposureCompensation(
        ToMediaSettingsRange(*photo_state->exposure_compensation));
    settings_->setExposureCompensation(
        photo_state->exposure_compensation->current);
  }
  if (photo_state->exposure_time->max != photo_state->exposure_time->min) {
    capabilities_->setExposureTime(
        ToMediaSettingsRange(*photo_state->exposure_time));
    settings_->setExposureTime(photo_state->exposure_time->current);
  }
  if (photo_state->color_temperature->max !=
      photo_state->color_temperature->min) {
    capabilities_->setColorTemperature(
        ToMediaSettingsRange(*photo_state->color_temperature));
    settings_->setColorTemperature(photo_state->color_temperature->current);
  }
  if (photo_state->iso->max != photo_state->iso->min) {
    capabilities_->setIso(ToMediaSettingsRange(*photo_state->iso));
    settings_->setIso(photo_state->iso->current);
  }

  if (photo_state->brightness->max != photo_state->brightness->min) {
    capabilities_->setBrightness(
        ToMediaSettingsRange(*photo_state->brightness));
    settings_->setBrightness(photo_state->brightness->current);
  }
  if (photo_state->contrast->max != photo_state->contrast->min) {
    capabilities_->setContrast(ToMediaSettingsRange(*photo_state->contrast));
    settings_->setContrast(photo_state->contrast->current);
  }
  if (photo_state->saturation->max != photo_state->saturation->min) {
    capabilities_->setSaturation(
        ToMediaSettingsRange(*photo_state->saturation));
    settings_->setSaturation(photo_state->saturation->current);
  }
  if (photo_state->sharpness->max != photo_state->sharpness->min) {
    capabilities_->setSharpness(ToMediaSettingsRange(*photo_state->sharpness));
    settings_->setSharpness(photo_state->sharpness->current);
  }

  if (photo_state->focus_distance->max != photo_state->focus_distance->min) {
    capabilities_->setFocusDistance(
        ToMediaSettingsRange(*photo_state->focus_distance));
    settings_->setFocusDistance(photo_state->focus_distance->current);
  }

  if (HasPanTiltZoomPermissionGranted()) {
    if (photo_state->pan->max != photo_state->pan->min) {
      capabilities_->setPan(ToMediaSettingsRange(*photo_state->pan));
      settings_->setPan(photo_state->pan->current);
    }
    if (photo_state->tilt->max != photo_state->tilt->min) {
      capabilities_->setTilt(ToMediaSettingsRange(*photo_state->tilt));
      settings_->setTilt(photo_state->tilt->current);
    }
    if (photo_state->zoom->max != photo_state->zoom->min) {
      capabilities_->setZoom(ToMediaSettingsRange(*photo_state->zoom));
      settings_->setZoom(photo_state->zoom->current);
    }
  }

  if (photo_state->supports_torch)
    capabilities_->setTorch(photo_state->supports_torch);
  if (photo_state->supports_torch)
    settings_->setTorch(photo_state->torch);

  if (photo_state->supported_background_blur_modes &&
      !photo_state->supported_background_blur_modes->empty()) {
    Vector<bool> supported_background_blur_modes;
    for (auto mode : *photo_state->supported_background_blur_modes)
      supported_background_blur_modes.push_back(ToBooleanMode(mode));
    capabilities_->setBackgroundBlur(
        std::move(supported_background_blur_modes));
    settings_->setBackgroundBlur(
        ToBooleanMode(photo_state->background_blur_mode));
  }

  std::move(initialized_callback).Run();
}

void ImageCapture::OnServiceConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolver>> resolvers;
  resolvers.swap(service_requests_);
  for (ScriptPromiseResolver* resolver : resolvers) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
  }
}

void ImageCapture::ResolveWithNothing(ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve();
}

void ImageCapture::ResolveWithPhotoSettings(ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve(photo_settings_);
}

void ImageCapture::ResolveWithPhotoCapabilities(
    ScriptPromiseResolver* resolver) {
  DCHECK(resolver);
  resolver->Resolve(photo_capabilities_);
}

bool ImageCapture::IsPageVisible() {
  return DomWindow() ? DomWindow()->document()->IsPageVisible() : false;
}

const String& ImageCapture::SourceId() const {
  return stream_track_->Component()->Source()->Id();
}

const absl::optional<const char*>
ImageCapture::GetConstraintWithCapabilityExistenceMismatch(
    const MediaTrackConstraintSet* constraint_set,
    MediaTrackConstraintSetType constraint_set_type) const {
  if (constraint_set->hasWhiteBalanceMode() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->whiteBalanceMode(),
          CapabilityExists(capabilities_->hasWhiteBalanceMode()),
          constraint_set_type)) {
    return "whiteBalanceMode";
  }
  if (constraint_set->hasExposureMode() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->exposureMode(),
          CapabilityExists(capabilities_->hasExposureMode()),
          constraint_set_type)) {
    return "exposureMode";
  }
  if (constraint_set->hasFocusMode() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->focusMode(),
          CapabilityExists(capabilities_->hasFocusMode()),
          constraint_set_type)) {
    return "focusMode";
  }
  if (constraint_set->hasExposureCompensation() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->exposureCompensation(),
          CapabilityExists(capabilities_->hasExposureCompensation()),
          constraint_set_type)) {
    return "exposureCompensation";
  }
  if (constraint_set->hasExposureTime() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->exposureTime(),
          CapabilityExists(capabilities_->hasExposureTime()),
          constraint_set_type)) {
    return "exposureTime";
  }
  if (constraint_set->hasColorTemperature() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->colorTemperature(),
          CapabilityExists(capabilities_->hasColorTemperature()),
          constraint_set_type)) {
    return "colorTemperature";
  }
  if (constraint_set->hasIso() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->iso(), CapabilityExists(capabilities_->hasIso()),
          constraint_set_type)) {
    return "iso";
  }
  if (constraint_set->hasBrightness() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->brightness(),
          CapabilityExists(capabilities_->hasBrightness()),
          constraint_set_type)) {
    return "brightness";
  }
  if (constraint_set->hasContrast() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->contrast(),
          CapabilityExists(capabilities_->hasContrast()),
          constraint_set_type)) {
    return "contrast";
  }
  if (constraint_set->hasSaturation() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->saturation(),
          CapabilityExists(capabilities_->hasSaturation()),
          constraint_set_type)) {
    return "saturation";
  }
  if (constraint_set->hasSharpness() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->sharpness(),
          CapabilityExists(capabilities_->hasSharpness()),
          constraint_set_type)) {
    return "sharpness";
  }
  if (constraint_set->hasFocusDistance() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->focusDistance(),
          CapabilityExists(capabilities_->hasFocusDistance()),
          constraint_set_type)) {
    return "focusDistance";
  }
  if (constraint_set->hasPan() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->pan(),
          CapabilityExists(capabilities_->hasPan() &&
                           HasPanTiltZoomPermissionGranted()),
          constraint_set_type)) {
    return "pan";
  }
  if (constraint_set->hasTilt() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->tilt(),
          CapabilityExists(capabilities_->hasTilt() &&
                           HasPanTiltZoomPermissionGranted()),
          constraint_set_type)) {
    return "tilt";
  }
  if (constraint_set->hasZoom() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->zoom(),
          CapabilityExists(capabilities_->hasZoom() &&
                           HasPanTiltZoomPermissionGranted()),
          constraint_set_type)) {
    return "zoom";
  }
  if (constraint_set->hasTorch() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->torch(), CapabilityExists(capabilities_->hasTorch()),
          constraint_set_type)) {
    return "torch";
  }
  if (constraint_set->hasBackgroundBlur() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->backgroundBlur(),
          CapabilityExists(capabilities_->hasBackgroundBlur()),
          constraint_set_type)) {
    return "backgroundBlur";
  }
  return absl::nullopt;
}

ImageCapture* ImageCapture::Clone() const {
  ImageCapture* clone = MakeGarbageCollected<ImageCapture>(
      GetExecutionContext(), stream_track_, HasPanTiltZoomPermissionGranted(),
      /*callback=*/base::DoNothing());

  // Copy capabilities.
  CopyCapabilities(capabilities_, clone->capabilities_, CopyPanTiltZoom(true));

  // Copy settings.
  CopySettings(settings_, clone->settings_, CopyPanTiltZoom(true));

  // Copy current constraint set.
  if (current_constraint_set_) {
    clone->current_constraint_set_ = MediaTrackConstraintSet::Create();
    CopyConstraintSet(current_constraint_set_, clone->current_constraint_set_,
                      CopyPanTiltZoom(true));
  }

  return clone;
}

void ImageCapture::Trace(Visitor* visitor) const {
  visitor->Trace(stream_track_);
  visitor->Trace(service_);
  visitor->Trace(permission_service_);
  visitor->Trace(permission_observer_receiver_);
  visitor->Trace(capabilities_);
  visitor->Trace(settings_);
  visitor->Trace(photo_settings_);
  visitor->Trace(current_constraint_set_);
  visitor->Trace(photo_capabilities_);
  visitor->Trace(service_requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
