// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_boolean_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_dom_string_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_point_2d_parameters.h"
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
using EyeGazeCorrectionMode = media::mojom::blink::EyeGazeCorrectionMode;
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
      return constraints_->advanced()[advanced_index].Get();
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
  if (source->hasBackgroundSegmentationMask()) {
    destination->setBackgroundSegmentationMask(
        source->backgroundSegmentationMask());
  }
  if (source->hasEyeGazeCorrection()) {
    destination->setEyeGazeCorrection(source->eyeGazeCorrection());
  }
  if (source->hasFaceFraming()) {
    destination->setFaceFraming(source->faceFraming());
  }
}

void CopyCapabilities(const MediaTrackCapabilities* source,
                      MediaTrackCapabilities* destination,
                      CopyPanTiltZoom copy_pan_tilt_zoom) {
  // Merge any present |source| members into |destination|.
  CopyCommonMembers(source, destination, copy_pan_tilt_zoom);
}

void CopyConstraintSet(const MediaTrackConstraintSet* source,
                       MediaTrackConstraintSet* destination) {
  // Merge any present |source| members into |destination|.
  // Constraints come always from JavaScript (unlike capabilities and settings)
  // so pan, tilt and zoom constraints are never privileged information and can
  // always be copied.
  CopyCommonMembers(source, destination, CopyPanTiltZoom(true));
  if (source->hasPointsOfInterest()) {
    destination->setPointsOfInterest(source->pointsOfInterest());
  }
}

void CopyConstraints(const MediaTrackConstraints* source,
                     MediaTrackConstraints* destination) {
  HeapVector<Member<MediaTrackConstraintSet>> destination_constraint_sets;
  if (source->hasAdvanced() && !source->advanced().empty()) {
    destination_constraint_sets.reserve(source->advanced().size());
  }
  for (const auto* source_constraint_set : AllConstraintSets(source)) {
    if (source_constraint_set == source) {
      CopyConstraintSet(source_constraint_set, destination);
    } else {
      auto* destination_constraint_set = MediaTrackConstraintSet::Create();
      CopyConstraintSet(source_constraint_set, destination_constraint_set);
      destination_constraint_sets.push_back(destination_constraint_set);
    }
  }
  if (!destination_constraint_sets.empty()) {
    destination->setAdvanced(std::move(destination_constraint_sets));
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

MediaSettingsRange* DuplicateRange(const MediaSettingsRange* range) {
  MediaSettingsRange* copy = MediaSettingsRange::Create();
  copy->setMax(range->max());
  copy->setMin(range->min());
  if (range->hasStep()) {
    copy->setStep(range->step());
  }
  return copy;
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

bool IsEmptySequence(const HeapVector<Member<Point2D>>& constraint) {
  return constraint.empty();
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

ConstraintType GetConstraintType(
    const V8UnionConstrainPoint2DParametersOrPoint2DSequence* constraint) {
  DCHECK(constraint);
  if (constraint->IsConstrainPoint2DParameters()) {
    return GetConstraintType(constraint->GetAsConstrainPoint2DParameters());
  }
  if (constraint->GetAsPoint2DSequence().empty()) {
    return ConstraintType::kEmptySequence;
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

// Check if a constraint is to be considered here as a value constraint.
// Here we consider a constraint to be a value constraint only if it depends on
// capability values (and not just the existence of the capability) whether
// the capability satisfies the constraint.
bool IsValueConstraintType(ConstraintType constraint_type,
                           MediaTrackConstraintSetType constraint_set_type) {
  // TODO(crbug.com/1408091): This is not spec compliant. Remove this.
  if (constraint_set_type == MediaTrackConstraintSetType::kFirstAdvanced) {
    // In the first advanced constraint set, everything but some bare value
    // constraints are unsupported.
    switch (constraint_type) {
      case ConstraintType::kBareValue:
        break;
      // TODO(crbug.com/1408091): A DOMString sequence is not a special bare
      // value in the spec. Merge with kBareValue.
      case ConstraintType::kBareValueDOMStringSequence:
      default:
        return false;
    }
  }

  switch (constraint_type) {
    case ConstraintType::kEmptySequence:
      // If an empty list has been given as the value for a constraint, it MUST
      // be interpreted as if the constraint were not specified (in other
      // words, an empty constraint == no constraint).
      // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
      // Thus, an empty sequence does not constrain.
      return false;
    case ConstraintType::kBooleanFalse:
    case ConstraintType::kBooleanTrue:
      // Boolean constraints for non-boolean constrainable properties constrain
      // the capability existence but not the value.
      return false;
    case ConstraintType::kBareValue:
    case ConstraintType::kBareValueDOMStringSequence:
      // A bare value constraint is to be treated as ideal in the basic
      // constraint set and as exact in advanced constraint sets.
      // In the both cases, it has an effect on the SelectSettings algorithm.
      return true;
    case ConstraintType::kEmptyDictionary:
      // An empty dictionary does not constrain.
      return false;
    case ConstraintType::kEffectivelyEmptyDictionary:
      // If an empty list has been given as the value for a constraint, it MUST
      // be interpreted as if the constraint were not specified (in other
      // words, an empty constraint == no constraint).
      // https://w3c.github.io/mediacapture-main/#dfn-selectsettings
      // Thus, a dictionary containing only empty sequences does not constrain.
      return false;
    case ConstraintType::kIdealDictionary:
      // Ideal constraints have an effect on the SelectSettings algorithm in
      // the basic constraint set but not in the advanced constraint sets.
      return constraint_set_type == MediaTrackConstraintSetType::kBasic;
    case ConstraintType::kMandatoryDictionary:
      // Mandatory exact, max and min constraints have always an effect on
      // the SelectSettings algorithm.
      return true;
  }
}

template <typename Constraint>
bool IsValueConstraint(const Constraint* constraint,
                       MediaTrackConstraintSetType constraint_set_type) {
  return IsValueConstraintType(GetConstraintType(constraint),
                               constraint_set_type);
}

bool MayRejectWithOverconstrainedError(
    MediaTrackConstraintSetType constraint_set_type) {
  // TODO(crbug.com/1408091): This is not spec compliant. Remove this.
  if (constraint_set_type == MediaTrackConstraintSetType::kFirstAdvanced) {
    return true;
  }

  // Only required constraints (in the basic constraint set) may cause
  // the applyConstraints returned promise to reject with
  // an OverconstrainedError.
  // Advanced constraints (in the advanced constraint sets) may only cause
  // those constraint sets to be discarded.
  return constraint_set_type == MediaTrackConstraintSetType::kBasic;
}

bool TrackIsInactive(const MediaStreamTrack& track) {
  // Spec instructs to return an exception if the Track's readyState() is not
  // "live". Also reject if the track is disabled or muted.
  // TODO(https://crbug.com/1462012): Do not consider muted tracks inactive.
  return track.readyState() != "live" || !track.enabled();
}

BackgroundBlurMode ParseBackgroundBlur(bool blink_mode) {
  return blink_mode ? BackgroundBlurMode::BLUR : BackgroundBlurMode::OFF;
}

EyeGazeCorrectionMode ParseEyeGazeCorrection(bool blink_mode) {
  return blink_mode ? EyeGazeCorrectionMode::ON : EyeGazeCorrectionMode::OFF;
}

MeteringMode ParseFaceFraming(bool blink_mode) {
  return blink_mode ? MeteringMode::CONTINUOUS : MeteringMode::NONE;
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
}

FillLightMode V8EnumToFillLightMode(V8FillLightMode::Enum blink_mode) {
  switch (blink_mode) {
    case V8FillLightMode::Enum::kOff:
      return FillLightMode::OFF;
    case V8FillLightMode::Enum::kAuto:
      return FillLightMode::AUTO;
    case V8FillLightMode::Enum::kFlash:
      return FillLightMode::FLASH;
  }
  NOTREACHED();
}

bool ToBooleanMode(BackgroundBlurMode mode) {
  switch (mode) {
    case BackgroundBlurMode::OFF:
      return false;
    case BackgroundBlurMode::BLUR:
      return true;
  }
  NOTREACHED();
}

bool ToBooleanMode(EyeGazeCorrectionMode mode) {
  switch (mode) {
    case EyeGazeCorrectionMode::OFF:
      return false;
    case EyeGazeCorrectionMode::ON:
    case EyeGazeCorrectionMode::STARE:
      return true;
  }
  NOTREACHED();
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
  NOTREACHED();
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
  NOTREACHED();
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
  NOTREACHED();
}

MediaSettingsRange* ToMediaSettingsRange(
    const media::mojom::blink::Range& range) {
  MediaSettingsRange* result = MediaSettingsRange::Create();
  result->setMax(range.max);
  result->setMin(range.min);
  result->setStep(range.step);
  return result;
}

// Check exact value constraints.
//
// The checks can fail only if the exact value constraint is not satisfied by
// an effective capability (which takes taking into consideration restrictions
// placed by other constraints).
// https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
// Step 2 & More definitions
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.

// For exact `sequence<Point2D>` constraints such as `pointsOfInterest`.
// There is no capability for `pointsOfInterest` in `MediaTrackCapabilities`
// to be used as a storage for an effective capability.
// As a substitute, we use `MediaTrackSettings` and its `pointsOfInterest`
// field to convey restrictions placed by previous exact `pointsOfInterest`
// constraints.
bool CheckExactValueConstraint(
    const HeapVector<Member<Point2D>>* effective_setting,
    const HeapVector<Member<Point2D>>& exact_constraint) {
  if (!effective_setting) {
    // The |effective_setting| does not represent a previous exact constraint
    // thus accept everything.
    return true;
  }
  // There is a previous exact constraint represented by |effective_setting|.
  // |exact_constraint| must be effectively equal to it (coordinates clamped to
  // [0, 1] must be equal).
  return effective_setting->size() == exact_constraint.size() &&
         std::equal(effective_setting->begin(), effective_setting->end(),
                    exact_constraint.begin(),
                    [](const Point2D* a, const Point2D* b) {
                      return (a->x() <= 0.0   ? b->x() <= 0.0
                              : a->x() >= 1.0 ? b->x() >= 1.0
                                              : b->x() == a->x()) &&
                             (a->y() <= 0.0   ? b->y() <= 0.0
                              : a->y() >= 1.0 ? b->y() >= 1.0
                                              : b->y() == a->y());
                    });
}

// For exact `double` constraints and `MediaSettingsRange` effective
// capabilities such as exposureCompensation, ..., zoom.
bool CheckExactValueConstraint(const MediaSettingsRange* effective_capability,
                               double exact_constraint) {
  if (effective_capability->hasMax() &&
      exact_constraint > effective_capability->max()) {
    return false;
  }
  if (effective_capability->hasMin() &&
      exact_constraint < effective_capability->min()) {
    return false;
  }
  return true;
}

// For exact `DOMString` constraints and `sequence<DOMString>` effective
// capabilities such as whiteBalanceMode, exposureMode and focusMode.
bool CheckExactValueConstraint(const Vector<String>& effective_capability,
                               const String& exact_constraint) {
  return base::Contains(effective_capability, exact_constraint);
}

// For exact `sequence<DOMString>` constraints and `sequence<DOMString>`
// effective capabilities such as whiteBalanceMode, exposureMode and focusMode.
bool CheckExactValueConstraint(const Vector<String>& effective_capability,
                               const Vector<String>& exact_constraints) {
  for (const auto& exact_constraint : exact_constraints) {
    if (base::Contains(effective_capability, exact_constraint)) {
      return true;
    }
  }
  return false;
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

// Check value constraints.
//
// For value constraints, the checks can fail only if the value constraint is
// mandatory ('exact', 'max' or 'min' or a bare value to be treated as exact),
// not an empty sequence (which MUST be interpreted as if the constraint were
// not specified) and not satisfied by an effective capability (which takes
// taking into consideration restrictions placed by other constraints).
// https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
// Step 2 & More definitions
// https://w3c.github.io/mediacapture-main/#dfn-selectsettings
//
// For non-value constraints, the checks always succeed.
// This is to simplify `CheckMediaTrackConstraintSet()`.
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.

// For `ConstrainPoint2D` constraints such as `pointsOfInterest`.
// There is no capability for `pointsOfInterest` in `MediaTrackCapabilities`
// to be used as a storage for an effective capability.
// As a substitute, we use `MediaTrackSettings` and its `pointsOfInterest`
// field to convey restrictions placed by previous exact `pointsOfInterest`
// constraints.
bool CheckValueConstraint(
    const HeapVector<Member<Point2D>>* effective_setting,
    const V8UnionConstrainPoint2DParametersOrPoint2DSequence* constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    return true;
  }
  using ContentType =
      V8UnionConstrainPoint2DParametersOrPoint2DSequence::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kPoint2DSequence:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return CheckExactValueConstraint(effective_setting,
                                         constraint->GetAsPoint2DSequence());
      }
      return true;
    case ContentType::kConstrainPoint2DParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainPoint2DParameters();
      if (dictionary_constraint->hasExact()) {
        return CheckExactValueConstraint(effective_setting,
                                         dictionary_constraint->exact());
      }
      return true;
    }
  }
}

// For `ConstrainDouble` constraints and `MediaSettingsRange` effective
// capabilities such as exposureCompensation, ..., focusDistance.
bool CheckValueConstraint(const MediaSettingsRange* effective_capability,
                          const V8UnionConstrainDoubleRangeOrDouble* constraint,
                          MediaTrackConstraintSetType constraint_set_type) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    return true;
  }
  using ContentType = V8UnionConstrainDoubleRangeOrDouble::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kDouble:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return CheckExactValueConstraint(effective_capability,
                                         constraint->GetAsDouble());
      }
      return true;
    case ContentType::kConstrainDoubleRange: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainDoubleRange();
      if (dictionary_constraint->hasExact()) {
        const double exact_constraint = dictionary_constraint->exact();
        if (dictionary_constraint->hasMax() &&
            exact_constraint > dictionary_constraint->max()) {
          return false;  // Reject self-contradiction.
        }
        if (dictionary_constraint->hasMin() &&
            exact_constraint < dictionary_constraint->min()) {
          return false;  // Reject self-contradiction.
        }
        if (!CheckExactValueConstraint(effective_capability,
                                       exact_constraint)) {
          return false;
        }
      }
      if (dictionary_constraint->hasMax()) {
        const double max_constraint = dictionary_constraint->max();
        if (dictionary_constraint->hasMin() &&
            max_constraint < dictionary_constraint->min()) {
          return false;  // Reject self-contradiction.
        }
        if (effective_capability->hasMin() &&
            max_constraint < effective_capability->min()) {
          return false;
        }
      }
      if (dictionary_constraint->hasMin()) {
        const double min_constraint = dictionary_constraint->min();
        if (effective_capability->hasMax() &&
            min_constraint > effective_capability->max()) {
          return false;
        }
      }
      return true;
    }
  }
}

// For `(boolean or ConstrainDouble)` constraints and `MediaSettingsRange`
// effective capabilities such as pan, tilt and zoom.
bool CheckValueConstraint(
    const MediaSettingsRange* effective_capability,
    const V8UnionBooleanOrConstrainDoubleRangeOrDouble* constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    return true;
  }
  // We classify boolean constraints for double constrainable properties as
  // existence constraints instead of as value constraints.
  DCHECK(!constraint->IsBoolean());
  return CheckValueConstraint(
      effective_capability,
      constraint->GetAsV8UnionConstrainDoubleRangeOrDouble(),
      constraint_set_type);
}

// For `ConstrainBoolean` constraints and `sequence<boolean>` effective
// capabilities such as torch and backgroundBlur.
bool CheckValueConstraint(
    const Vector<bool>& effective_capability,
    const V8UnionBooleanOrConstrainBooleanParameters* constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    return true;
  }
  using ContentType = V8UnionBooleanOrConstrainBooleanParameters::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kBoolean:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        const bool exact_constraint = constraint->GetAsBoolean();
        return base::Contains(effective_capability, exact_constraint);
      }
      return true;
    case ContentType::kConstrainBooleanParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainBooleanParameters();
      if (dictionary_constraint->hasExact()) {
        const bool exact_constraint = dictionary_constraint->exact();
        return base::Contains(effective_capability, exact_constraint);
      }
      return true;
    }
  }
}

// For `ConstrainDOMString` constraints and `sequence<DOMString>` effective
// capabilities such as whiteBalanceMode, exposureMode and focusMode.
bool CheckValueConstraint(
    const Vector<String>& effective_capability,
    const V8UnionConstrainDOMStringParametersOrStringOrStringSequence*
        constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    return true;
  }
  using ContentType =
      V8UnionConstrainDOMStringParametersOrStringOrStringSequence::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kString:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return CheckExactValueConstraint(effective_capability,
                                         constraint->GetAsString());
      }
      return true;
    case ContentType::kStringSequence:
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return CheckExactValueConstraint(effective_capability,
                                         constraint->GetAsStringSequence());
      }
      return true;
    case ContentType::kConstrainDOMStringParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainDOMStringParameters();
      if (dictionary_constraint->hasExact()) {
        const auto* exact_constraint = dictionary_constraint->exact();
        switch (exact_constraint->GetContentType()) {
          case V8UnionStringOrStringSequence::ContentType::kString:
            return CheckExactValueConstraint(effective_capability,
                                             exact_constraint->GetAsString());
          case V8UnionStringOrStringSequence::ContentType::kStringSequence:
            return CheckExactValueConstraint(
                effective_capability, exact_constraint->GetAsStringSequence());
        }
      }
      return true;
    }
  }
}

// Apply exact value constraints to photo settings and return new effective
// capabilities.
//
// Roughly the SelectSettings algorithm steps 3 and 5.
// https://www.w3.org/TR/mediacapture-streams/#dfn-selectsettings
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.

// For exact `boolean` constraints and `sequence<boolean>` effective
// capabilities such as torch and backgroundBlur.
Vector<bool> ApplyExactValueConstraint(bool* has_setting_ptr,
                                       bool* setting_ptr,
                                       const Vector<bool>& effective_capability,
                                       bool exact_constraint) {
  // Update the setting.
  *has_setting_ptr = true;
  *setting_ptr = exact_constraint;
  // Update the effective capability.
  return {exact_constraint};
}

// For exact `double` constraints and `MediaSettingsRange` effective
// capabilities such as exposureCompensation, ..., zoom.
MediaSettingsRange* ApplyExactValueConstraint(
    bool* has_setting_ptr,
    double* setting_ptr,
    const MediaSettingsRange* effective_capability,
    double exact_constraint) {
  // Update the setting.
  *has_setting_ptr = true;
  *setting_ptr = exact_constraint;
  // Update the effective capability.
  auto* new_effective_capability = MediaSettingsRange::Create();
  new_effective_capability->setMax(exact_constraint);
  new_effective_capability->setMin(exact_constraint);
  return new_effective_capability;
}

// For exact `DOMString` constraints and `sequence<DOMString>` effective
// capabilities such as whiteBalanceMode, exposureMode and focusMode.
Vector<String> ApplyExactValueConstraint(
    bool* has_setting_ptr,
    MeteringMode* setting_ptr,
    const Vector<String>& effective_capability,
    const String& exact_constraint) {
  // Update the setting.
  *has_setting_ptr = true;
  *setting_ptr = ParseMeteringMode(exact_constraint);
  // Update the effective capability.
  return {exact_constraint};
}

// For exact `sequence<DOMString>` constraints and `sequence<DOMString>`
// effective capabilities such as whiteBalanceMode, exposureMode and focusMode.
Vector<String> ApplyExactValueConstraint(
    bool* has_setting_ptr,
    MeteringMode* setting_ptr,
    const Vector<String>& effective_capability,
    const Vector<String>& exact_constraints) {
  // Update the effective capability.
  Vector<String> new_effective_capability;
  for (const auto& exact_constraint : exact_constraints) {
    if (base::Contains(effective_capability, exact_constraint)) {
      new_effective_capability.push_back(exact_constraint);
    }
  }
  DCHECK(!new_effective_capability.empty());
  // Clamp and update the setting.
  if (!*has_setting_ptr ||
      !base::Contains(exact_constraints,
                      static_cast<const String&>(ToString(*setting_ptr)))) {
    *has_setting_ptr = true;
    *setting_ptr = ParseMeteringMode(new_effective_capability[0]);
  }
  return new_effective_capability;
}

// Apply ideal value constraints to photo settings and return effective
// capabilities intact (ideal constraints have no effect on effective
// capabilities).
//
// Roughly the SelectSettings algorithm step 3.
// https://www.w3.org/TR/mediacapture-streams/#dfn-selectsettings
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.

// For ideal `boolean` constraints and `sequence<boolean>` effective
// capabilities such as torch and backgroundBlur.
Vector<bool> ApplyIdealValueConstraint(bool* has_setting_ptr,
                                       bool* setting_ptr,
                                       const Vector<bool>& effective_capability,
                                       bool ideal_constraint) {
  // Clamp and update the setting.
  *has_setting_ptr = true;
  *setting_ptr = base::Contains(effective_capability, ideal_constraint)
                     ? ideal_constraint
                     : effective_capability[0];
  // Keep the effective capability intact.
  return effective_capability;
}

// For ideal `double` constraints and `MediaSettingsRange` effective
// capabilities such as exposureCompensation, ..., zoom.
MediaSettingsRange* ApplyIdealValueConstraint(
    bool* has_setting_ptr,
    double* setting_ptr,
    MediaSettingsRange* effective_capability,
    std::optional<double> ideal_constraint,
    double current_setting) {
  // Clamp and update the setting.
  *has_setting_ptr = true;
  *setting_ptr =
      std::clamp(ideal_constraint ? *ideal_constraint : current_setting,
                 effective_capability->min(), effective_capability->max());
  // Keep the effective capability intact.
  return effective_capability;
}

// For ideal `DOMString` constraints and `sequence<DOMString>` effective
// capabilities such as whiteBalanceMode, exposureMode and focusMode.
Vector<String> ApplyIdealValueConstraint(
    bool* has_setting_ptr,
    MeteringMode* setting_ptr,
    const Vector<String>& effective_capability,
    const String& ideal_constraint,
    const String& current_setting) {
  // Validate and update the setting.
  *has_setting_ptr = true;
  *setting_ptr = ParseMeteringMode(
      base::Contains(effective_capability, ideal_constraint) ? ideal_constraint
                                                             : current_setting);
  // Keep the effective capability intact.
  return effective_capability;
}

// For ideal `sequence<DOMString>` constraints and `sequence<DOMString>`
// effective capabilities such as whiteBalanceMode, exposureMode and focusMode.
Vector<String> ApplyIdealValueConstraint(
    bool* has_setting_ptr,
    MeteringMode* setting_ptr,
    const Vector<String>& effective_capability,
    const Vector<String>& ideal_constraints,
    const String& current_setting) {
  // Clamp and update the setting.
  if (!*has_setting_ptr ||
      !base::Contains(ideal_constraints,
                      static_cast<const String&>(ToString(*setting_ptr)))) {
    String setting_name = current_setting;
    for (const auto& ideal_constraint : ideal_constraints) {
      if (base::Contains(effective_capability, ideal_constraint)) {
        setting_name = ideal_constraint;
        break;
      }
    }
    *has_setting_ptr = true;
    *setting_ptr = ParseMeteringMode(setting_name);
  }
  // Keep the effective capability intact.
  return effective_capability;
}

// Apply value constraints to photo settings and return new effective
// capabilities.
//
// Roughly the SelectSettings algorithm steps 3 and 5.
// https://www.w3.org/TR/mediacapture-streams/#dfn-selectsettings
//
// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove these support functions.

// For `ConstrainBoolean` constraints and `sequence<boolean>` effective
// capabilities such as torch and backgroundBlur.
Vector<bool> ApplyValueConstraint(
    bool* has_setting_ptr,
    bool* setting_ptr,
    const Vector<bool>& effective_capability,
    const V8UnionBooleanOrConstrainBooleanParameters* constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  DCHECK(CheckValueConstraint(effective_capability, constraint,
                              constraint_set_type));
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    // Keep the effective capability intact.
    return effective_capability;
  }
  using ContentType = V8UnionBooleanOrConstrainBooleanParameters::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kBoolean:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         constraint->GetAsBoolean());
      }
      // We classify ideal bare value constraints as value constraints only in
      // the basic constraint set in which they have an effect on
      // the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      return ApplyIdealValueConstraint(has_setting_ptr, setting_ptr,
                                       effective_capability,
                                       constraint->GetAsBoolean());
    case ContentType::kConstrainBooleanParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainBooleanParameters();
      if (dictionary_constraint->hasExact()) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         dictionary_constraint->exact());
      }
      // We classify `ConstrainBooleanParameters` constraints containing only
      // the `ideal` member as value constraints only in the basic constraint
      // set in which they have an effect on the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      return ApplyIdealValueConstraint(has_setting_ptr, setting_ptr,
                                       effective_capability,
                                       dictionary_constraint->ideal());
    }
  }
}

// For `ConstrainDouble` constraints and `MediaSettingsRange` effective
// capabilities such as exposureCompensation, ..., focusDistance.
MediaSettingsRange* ApplyValueConstraint(
    bool* has_setting_ptr,
    double* setting_ptr,
    const MediaSettingsRange* effective_capability,
    const V8UnionConstrainDoubleRangeOrDouble* constraint,
    MediaTrackConstraintSetType constraint_set_type,
    double current_setting) {
  DCHECK(CheckValueConstraint(effective_capability, constraint,
                              constraint_set_type));
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    // Keep the effective capability intact.
    return const_cast<MediaSettingsRange*>(effective_capability);
  }
  using ContentType = V8UnionConstrainDoubleRangeOrDouble::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kDouble:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         constraint->GetAsDouble());
      }
      // We classify ideal bare value constraints as value constraints only in
      // the basic constraint set in which they have an effect on
      // the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      return ApplyIdealValueConstraint(
          has_setting_ptr, setting_ptr,
          const_cast<MediaSettingsRange*>(effective_capability),
          constraint->GetAsDouble(), current_setting);
    case ContentType::kConstrainDoubleRange: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainDoubleRange();
      if (dictionary_constraint->hasExact()) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         dictionary_constraint->exact());
      }
      // Update the effective capability.
      auto* new_effective_capability = DuplicateRange(effective_capability);
      if (dictionary_constraint->hasMax()) {
        new_effective_capability->setMax(std::min(dictionary_constraint->max(),
                                                  effective_capability->max()));
      }
      if (dictionary_constraint->hasMin()) {
        new_effective_capability->setMin(std::max(dictionary_constraint->min(),
                                                  effective_capability->min()));
      }
      // Ideal constraints have an effect on the SelectSettings algorithm only
      // in the basic constraint set. Always call `ApplyIdealValueConstraint()`
      // so that either the ideal value constraint or the current setting is
      // clamped so that the setting is within the new effective capability.
      DCHECK(
          (dictionary_constraint->hasIdeal() &&
           constraint_set_type == MediaTrackConstraintSetType::kBasic) ||
          (dictionary_constraint->hasMax() || dictionary_constraint->hasMin()));
      return ApplyIdealValueConstraint(
          has_setting_ptr, setting_ptr, new_effective_capability,
          (dictionary_constraint->hasIdeal() &&
           constraint_set_type == MediaTrackConstraintSetType::kBasic)
              ? std::make_optional(dictionary_constraint->ideal())
              : std::nullopt,
          current_setting);
    }
  }
}

// For `(boolean or ConstrainDouble)` constraints and `MediaSettingsRange`
// effective capabilities such as pan, tilt and zoom.
MediaSettingsRange* ApplyValueConstraint(
    bool* has_setting_ptr,
    double* setting_ptr,
    const MediaSettingsRange* effective_capability,
    const V8UnionBooleanOrConstrainDoubleRangeOrDouble* constraint,
    MediaTrackConstraintSetType constraint_set_type,
    double current_setting) {
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    // Keep the effective capability intact.
    return const_cast<MediaSettingsRange*>(effective_capability);
  }
  // We classify boolean constraints for double constrainable properties as
  // existence constraints instead of as value constraints.
  DCHECK(!constraint->IsBoolean());
  return ApplyValueConstraint(
      has_setting_ptr, setting_ptr, effective_capability,
      constraint->GetAsV8UnionConstrainDoubleRangeOrDouble(),
      constraint_set_type, current_setting);
}

// For `ConstrainDOMString` constraints and `sequence<DOMString>` effective
// capabilities such as whiteBalanceMode, exposureMode and focusMode.
Vector<String> ApplyValueConstraint(
    bool* has_setting_ptr,
    MeteringMode* setting_ptr,
    const Vector<String>& effective_capability,
    const V8UnionConstrainDOMStringParametersOrStringOrStringSequence*
        constraint,
    MediaTrackConstraintSetType constraint_set_type,
    const String& current_setting) {
  DCHECK(CheckValueConstraint(effective_capability, constraint,
                              constraint_set_type));
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    // Keep the effective capability intact.
    return effective_capability;
  }
  using ContentType =
      V8UnionConstrainDOMStringParametersOrStringOrStringSequence::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kString:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         constraint->GetAsString());
      }
      // We classify ideal bare value constraints as value constraints only in
      // the basic constraint set in which they have an effect on
      // the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      return ApplyIdealValueConstraint(
          has_setting_ptr, setting_ptr, effective_capability,
          constraint->GetAsString(), current_setting);
    case ContentType::kStringSequence:
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                         effective_capability,
                                         constraint->GetAsStringSequence());
      }
      // We classify ideal bare value constraints as value constraints only in
      // the basic constraint set in which they have an effect on
      // the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      return ApplyIdealValueConstraint(
          has_setting_ptr, setting_ptr, effective_capability,
          constraint->GetAsStringSequence(), current_setting);
    case ContentType::kConstrainDOMStringParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainDOMStringParameters();
      if (dictionary_constraint->hasExact()) {
        const V8UnionStringOrStringSequence* exact_constraint =
            dictionary_constraint->exact();
        switch (exact_constraint->GetContentType()) {
          case V8UnionStringOrStringSequence::ContentType::kString:
            return ApplyExactValueConstraint(has_setting_ptr, setting_ptr,
                                             effective_capability,
                                             exact_constraint->GetAsString());
          case V8UnionStringOrStringSequence::ContentType::kStringSequence:
            return ApplyExactValueConstraint(
                has_setting_ptr, setting_ptr, effective_capability,
                exact_constraint->GetAsStringSequence());
        }
      }
      // We classify `ConstrainDOMStringParameters` constraints containing only
      // the `ideal` member as value constraints only in the basic constraint
      // set in which they have an effect on the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      const V8UnionStringOrStringSequence* ideal_constraint =
          dictionary_constraint->ideal();
      switch (ideal_constraint->GetContentType()) {
        case V8UnionStringOrStringSequence::ContentType::kString:
          return ApplyIdealValueConstraint(
              has_setting_ptr, setting_ptr, effective_capability,
              ideal_constraint->GetAsString(), current_setting);
        case V8UnionStringOrStringSequence::ContentType::kStringSequence:
          return ApplyIdealValueConstraint(
              has_setting_ptr, setting_ptr, effective_capability,
              ideal_constraint->GetAsStringSequence(), current_setting);
      }
    }
  }
}

// For `ConstrainPoint2D` constraints such as `pointsOfInterest`.
// There is no capability for `pointsOfInterest` in `MediaTrackCapabilities`
// to be used as a storage for an effective capability.
// As a substitute, we use `MediaTrackSettings` and its `pointsOfInterest`
// field to convey restrictions placed by previous exact `pointsOfInterest`
// constraints.
void ApplyValueConstraint(bool* has_setting_ptr,
                          Vector<media::mojom::blink::Point2DPtr>* setting_ptr,
                          const HeapVector<Member<Point2D>>* effective_setting,
                          const HeapVector<Member<Point2D>>& constraint) {
  // Update the setting.
  *has_setting_ptr = true;
  setting_ptr->clear();
  for (const auto& point : constraint) {
    auto mojo_point = media::mojom::blink::Point2D::New();
    mojo_point->x = std::clamp(point->x(), 0.0, 1.0);
    mojo_point->y = std::clamp(point->y(), 0.0, 1.0);
    setting_ptr->push_back(std::move(mojo_point));
  }
}

// For `ConstrainPoint2D` constraints such as `pointsOfInterest`.
// There is no capability for `pointsOfInterest` in `MediaTrackCapabilities`
// to be used as a storage for an effective capability.
// As a substitute, we use `MediaTrackSettings` and its `pointsOfInterest`
// field to convey restrictions placed by previous exact `pointsOfInterest`
// constraints.
std::optional<HeapVector<Member<Point2D>>> ApplyValueConstraint(
    bool* has_setting_ptr,
    Vector<media::mojom::blink::Point2DPtr>* setting_ptr,
    const HeapVector<Member<Point2D>>* effective_setting,
    const V8UnionConstrainPoint2DParametersOrPoint2DSequence* constraint,
    MediaTrackConstraintSetType constraint_set_type) {
  DCHECK(
      CheckValueConstraint(effective_setting, constraint, constraint_set_type));
  if (!IsValueConstraint(constraint, constraint_set_type)) {
    // Keep the effective capability intact.
    return std::nullopt;
  }
  using ContentType =
      V8UnionConstrainPoint2DParametersOrPoint2DSequence::ContentType;
  switch (constraint->GetContentType()) {
    case ContentType::kPoint2DSequence:
      if (IsBareValueToBeTreatedAsExact(constraint_set_type)) {
        ApplyValueConstraint(has_setting_ptr, setting_ptr, effective_setting,
                             constraint->GetAsPoint2DSequence());
        return constraint->GetAsPoint2DSequence();
      }
      // We classify ideal bare value constraints as value constraints only in
      // the basic constraint set in which they have an effect on
      // the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      ApplyValueConstraint(has_setting_ptr, setting_ptr, effective_setting,
                           constraint->GetAsPoint2DSequence());
      return std::nullopt;
    case ContentType::kConstrainPoint2DParameters: {
      DCHECK_NE(constraint_set_type,
                MediaTrackConstraintSetType::kFirstAdvanced);
      const auto* dictionary_constraint =
          constraint->GetAsConstrainPoint2DParameters();
      if (dictionary_constraint->hasExact()) {
        ApplyValueConstraint(has_setting_ptr, setting_ptr, effective_setting,
                             dictionary_constraint->exact());
        return dictionary_constraint->exact();
      }
      // We classify `ConstrainPoint2DParameters` constraints containing only
      // the `ideal` member as value constraints only in the basic constraint
      // set in which they have an effect on the SelectSettings algorithm.
      DCHECK_EQ(constraint_set_type, MediaTrackConstraintSetType::kBasic);
      ApplyValueConstraint(has_setting_ptr, setting_ptr, effective_setting,
                           dictionary_constraint->ideal());
      return std::nullopt;
    }
  }
}

void MaybeSetBackgroundBlurSetting(bool value,
                                   const Vector<bool>& capability,
                                   bool& has_setting,
                                   BackgroundBlurMode& setting) {
  if (!base::Contains(capability, value)) {
    return;
  }

  has_setting = true;
  setting = ParseBackgroundBlur(value);
}

void MaybeSetBoolSetting(bool value,
                         const Vector<bool>& capability,
                         std::optional<bool>& setting) {
  if (!base::Contains(capability, value)) {
    return;
  }

  setting = value;
}

void MaybeSetBoolSetting(bool value,
                         const Vector<bool>& capability,
                         bool& has_setting,
                         bool& setting) {
  if (!base::Contains(capability, value)) {
    return;
  }

  has_setting = true;
  setting = value;
}

void MaybeSetEyeGazeCorrectionSetting(
    bool value,
    const Vector<bool>& capability,
    std::optional<EyeGazeCorrectionMode>& setting) {
  if (!base::Contains(capability, value)) {
    return;
  }

  setting = ParseEyeGazeCorrection(value);
}

void MaybeSetFaceFramingSetting(bool value,
                                const Vector<bool>& capability,
                                bool& has_setting,
                                MeteringMode& setting) {
  if (!base::Contains(capability, value)) {
    return;
  }

  has_setting = true;
  setting = ParseFaceFraming(value);
}

void MaybeSetDoubleSetting(double value,
                           const MediaSettingsRange& capability,
                           bool& has_setting,
                           double& setting) {
  if (!(capability.min() <= value && value <= capability.max())) {
    return;
  }

  has_setting = true;
  setting = value;
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
  frame_grabber_.reset();
}

ScriptPromise<PhotoCapabilities> ImageCapture::getPhotoCapabilities(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PhotoCapabilities>>(
          script_state);
  auto promise = resolver->Promise();
  GetMojoPhotoState(resolver,
                    WTF::BindOnce(&ImageCapture::ResolveWithPhotoCapabilities,
                                  WrapPersistent(this)));
  return promise;
}

ScriptPromise<PhotoSettings> ImageCapture::getPhotoSettings(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PhotoSettings>>(script_state);
  auto promise = resolver->Promise();
  GetMojoPhotoState(resolver,
                    WTF::BindOnce(&ImageCapture::ResolveWithPhotoSettings,
                                  WrapPersistent(this)));
  return promise;
}

ScriptPromise<Blob> ImageCapture::takePhoto(
    ScriptState* script_state,
    const PhotoSettings* photo_settings) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::takePhoto");

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<Blob>>(script_state);
  auto promise = resolver->Promise();

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
    auto fill_light_mode = photo_settings->fillLightMode();
    if (photo_capabilities_ && photo_capabilities_->hasFillLightMode() &&
        photo_capabilities_->fillLightMode().Find(fill_light_mode) ==
            kNotFound) {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "Unsupported fillLightMode"));
      return promise;
    }
    settings->fill_light_mode = V8EnumToFillLightMode(fill_light_mode.AsEnum());
  }

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/true));
  return promise;
}

ScriptPromise<ImageBitmap> ImageCapture::grabFrame(ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ImageBitmap>>(script_state);
  auto promise = resolver->Promise();

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
                                ->GetTaskRunner(TaskType::kDOMManipulation),
                            grab_frame_timeout_);

  return promise;
}

void ImageCapture::UpdateAndCheckMediaTrackSettingsAndCapabilities(
    base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::UpdateAndCheckMediaTrackSettingsAndCapabilities");
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

  // Check whether face framing settings and capabilities have changed.
  if (settings_->hasFaceFraming() != settings->hasFaceFraming() ||
      (settings_->hasFaceFraming() &&
       settings_->faceFraming() != settings->faceFraming()) ||
      capabilities_->hasFaceFraming() != capabilities->hasFaceFraming() ||
      (capabilities_->hasFaceFraming() &&
       capabilities_->faceFraming() != capabilities->faceFraming())) {
    std::move(callback).Run(true);
    return;
  }

  std::move(callback).Run(false);
}

bool ImageCapture::CheckAndApplyMediaTrackConstraintsToSettings(
    media::mojom::blink::PhotoSettings* settings,
    const MediaTrackConstraints* constraints,
    ScriptPromiseResolverBase* resolver) const {
  if (!IsPageVisible()) {
    for (const MediaTrackConstraintSet* constraint_set :
         AllConstraintSets(constraints)) {
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

  // The "effective capability" C of an object O as the possibly proper subset
  // of the possible values of C (as returned by getCapabilities) taking into
  // consideration environmental limitations and/or restrictions placed by
  // other constraints.
  // https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
  // More definitions
  auto* effective_capabilities = MediaTrackCapabilities::Create();
  CopyCapabilities(capabilities_, effective_capabilities,
                   CopyPanTiltZoom(HasPanTiltZoomPermissionGranted()));

  // There is no capability for `pointsOfInterest` in `MediaTrackCapabilities`
  // to be used as a storage for an effective capability for `pointsOfInterest`.
  // There is a capability for `torch` in `MediaTrackCapabilities` but it is
  // a boolean instead of a sequence of booleans so not suitable to be used as
  // a storage for an effective capability for `torch`.
  // As a substitute, we use `MediaTrackSettings` and its `pointsOfInterest`
  // `torch` fields to convey restrictions placed by previous exact
  // `pointsOfInterest` and `torch` constraints.
  auto* effective_settings = MediaTrackSettings::Create();

  for (const MediaTrackConstraintSet* constraint_set :
       AllConstraintSets(constraints)) {
    const MediaTrackConstraintSetType constraint_set_type =
        GetMediaTrackConstraintSetType(constraint_set, constraints);
    const bool may_reject =
        MayRejectWithOverconstrainedError(constraint_set_type);
    if (CheckMediaTrackConstraintSet(effective_capabilities, effective_settings,
                                     constraint_set, constraint_set_type,
                                     may_reject ? resolver : nullptr)) {
      ApplyMediaTrackConstraintSetToSettings(&*settings, effective_capabilities,
                                             effective_settings, constraint_set,
                                             constraint_set_type);
    } else if (may_reject) {
      return false;
    }
  }

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
    ScriptPromiseResolverBase* resolver,
    const MediaTrackConstraints* constraints) {
  DCHECK(constraints);

  ExecutionContext* context = GetExecutionContext();
  for (const MediaTrackConstraintSet* constraint_set :
       AllConstraintSets(constraints)) {
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
    if (RuntimeEnabledFeatures::MediaCaptureBackgroundBlurEnabled(context) &&
        constraint_set->hasBackgroundBlur()) {
      UseCounter::Count(context, WebFeature::kImageCaptureBackgroundBlur);
    }
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

  // TODO(crbug.com/1423282): This is not spec compliant. The current
  // constraints are used by `GetMediaTrackConstraints()` which is used by
  // `MediaStreamTrackImpl::getConstraints()` which should return
  // the constraints that were the argument to the most recent successful
  // invocation of the ApplyConstraints algorithm.
  // https://w3c.github.io/mediacapture-main/#dom-constrainablepattern-getconstraints
  //
  // At this point the ApplyConstraints algorithm is still ongoing and not
  // succeeded yet. Move this to `OnMojoSetPhotoOptions()` or such.
  current_constraints_ = MediaTrackConstraints::Create();
  CopyConstraints(constraints, current_constraints_);

  service_requests_.insert(resolver);

  service_->SetPhotoOptions(
      SourceId(), std::move(settings),
      WTF::BindOnce(&ImageCapture::OnMojoSetPhotoOptions, WrapPersistent(this),
                    WrapPersistent(resolver), /*trigger_take_photo=*/false));
}

void ImageCapture::SetVideoTrackDeviceSettingsFromTrack(
    base::OnceClosure initialized_callback,
    media::mojom::blink::PhotoStatePtr photo_state) {
  UpdateMediaTrackSettingsAndCapabilities(base::DoNothing(),
                                          std::move(photo_state));

  auto* video_track = MediaStreamVideoTrack::From(stream_track_->Component());
  DCHECK(video_track);

  const auto& device_settings = video_track->image_capture_device_settings();

  if (device_settings) {
    ExecutionContext* context = GetExecutionContext();
    if (device_settings->exposure_compensation.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureExposureCompensation);
    }
    if (device_settings->exposure_time.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureExposureTime);
    }
    if (device_settings->color_temperature.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureColorTemperature);
    }
    if (device_settings->iso.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureIso);
    }
    if (device_settings->brightness.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureBrightness);
    }
    if (device_settings->contrast.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureContrast);
    }
    if (device_settings->saturation.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureSaturation);
    }
    if (device_settings->sharpness.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureSharpness);
    }
    if (device_settings->focus_distance.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureFocusDistance);
    }
    if (device_settings->pan.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCapturePan);
    }
    if (device_settings->tilt.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureTilt);
    }
    if (device_settings->zoom.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureZoom);
    }
    if (device_settings->torch.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureTorch);
    }
    if (device_settings->background_blur.has_value()) {
      UseCounter::Count(context, WebFeature::kImageCaptureBackgroundBlur);
    }

    auto settings = media::mojom::blink::PhotoSettings::New();

    if (device_settings->exposure_compensation.has_value() &&
        capabilities_->hasExposureCompensation()) {
      MaybeSetDoubleSetting(*device_settings->exposure_compensation,
                            *capabilities_->exposureCompensation(),
                            settings->has_exposure_compensation,
                            settings->exposure_compensation);
    }
    if (device_settings->exposure_time.has_value() &&
        capabilities_->hasExposureTime()) {
      MaybeSetDoubleSetting(
          *device_settings->exposure_time, *capabilities_->exposureTime(),
          settings->has_exposure_time, settings->exposure_time);
    }
    if (device_settings->color_temperature.has_value() &&
        capabilities_->hasColorTemperature()) {
      MaybeSetDoubleSetting(*device_settings->color_temperature,
                            *capabilities_->colorTemperature(),
                            settings->has_color_temperature,
                            settings->color_temperature);
    }
    if (device_settings->iso.has_value() && capabilities_->hasIso()) {
      MaybeSetDoubleSetting(*device_settings->iso, *capabilities_->iso(),
                            settings->has_iso, settings->iso);
    }
    if (device_settings->brightness.has_value() &&
        capabilities_->hasBrightness()) {
      MaybeSetDoubleSetting(*device_settings->brightness,
                            *capabilities_->brightness(),
                            settings->has_brightness, settings->brightness);
    }
    if (device_settings->contrast.has_value() && capabilities_->hasContrast()) {
      MaybeSetDoubleSetting(*device_settings->contrast,
                            *capabilities_->contrast(), settings->has_contrast,
                            settings->contrast);
    }
    if (device_settings->saturation.has_value() &&
        capabilities_->hasSaturation()) {
      MaybeSetDoubleSetting(*device_settings->saturation,
                            *capabilities_->saturation(),
                            settings->has_saturation, settings->saturation);
    }
    if (device_settings->sharpness.has_value() &&
        capabilities_->hasSharpness()) {
      MaybeSetDoubleSetting(*device_settings->sharpness,
                            *capabilities_->sharpness(),
                            settings->has_sharpness, settings->sharpness);
    }
    if (device_settings->focus_distance.has_value() &&
        capabilities_->hasFocusDistance()) {
      MaybeSetDoubleSetting(
          *device_settings->focus_distance, *capabilities_->focusDistance(),
          settings->has_focus_distance, settings->focus_distance);
    }
    if (HasPanTiltZoomPermissionGranted()) {
      if (device_settings->pan.has_value() && capabilities_->hasPan()) {
        MaybeSetDoubleSetting(*device_settings->pan, *capabilities_->pan(),
                              settings->has_pan, settings->pan);
      }
      if (device_settings->tilt.has_value() && capabilities_->hasTilt()) {
        MaybeSetDoubleSetting(*device_settings->tilt, *capabilities_->tilt(),
                              settings->has_tilt, settings->tilt);
      }
      if (device_settings->zoom.has_value() && capabilities_->hasZoom()) {
        MaybeSetDoubleSetting(*device_settings->zoom, *capabilities_->zoom(),
                              settings->has_zoom, settings->zoom);
      }
    }
    if (device_settings->torch.has_value() && capabilities_->hasTorch()) {
      MaybeSetBoolSetting(
          *device_settings->torch,
          capabilities_->torch() ? Vector<bool>({false, true}) : Vector<bool>(),
          settings->has_torch, settings->torch);
    }
    if (device_settings->background_blur.has_value() &&
        capabilities_->hasBackgroundBlur()) {
      MaybeSetBackgroundBlurSetting(
          *device_settings->background_blur, capabilities_->backgroundBlur(),
          settings->has_background_blur_mode, settings->background_blur_mode);
    }
    if (device_settings->background_segmentation_mask.has_value() &&
        capabilities_->hasBackgroundSegmentationMask()) {
      MaybeSetBoolSetting(*device_settings->background_segmentation_mask,
                          capabilities_->backgroundSegmentationMask(),
                          settings->background_segmentation_mask_state);
    }
    if (device_settings->eye_gaze_correction.has_value() &&
        capabilities_->hasEyeGazeCorrection()) {
      MaybeSetEyeGazeCorrectionSetting(*device_settings->eye_gaze_correction,
                                       capabilities_->eyeGazeCorrection(),
                                       settings->eye_gaze_correction_mode);
    }
    if (device_settings->face_framing.has_value() &&
        capabilities_->hasFaceFraming()) {
      MaybeSetFaceFramingSetting(
          *device_settings->face_framing, capabilities_->faceFraming(),
          settings->has_face_framing_mode, settings->face_framing_mode);
    }

    if (service_.is_bound() &&
        (settings->has_exposure_compensation || settings->has_exposure_time ||
         settings->has_color_temperature || settings->has_iso ||
         settings->has_brightness || settings->has_contrast ||
         settings->has_saturation || settings->has_sharpness ||
         settings->has_focus_distance || settings->has_pan ||
         settings->has_tilt || settings->has_zoom || settings->has_torch ||
         settings->has_background_blur_mode ||
         settings->has_face_framing_mode ||
         settings->eye_gaze_correction_mode.has_value() ||
         settings->background_segmentation_mask_state.has_value())) {
      service_->SetPhotoOptions(
          SourceId(), std::move(settings),
          WTF::BindOnce(&ImageCapture::OnSetVideoTrackDeviceSettingsFromTrack,
                        WrapPersistent(this), std::move(initialized_callback)));
      return;
    }
  }

  std::move(initialized_callback).Run();
}

void ImageCapture::OnSetVideoTrackDeviceSettingsFromTrack(
    base::OnceClosure done_callback,
    bool result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::OnSetVideoTrackDeviceSettingsFromTrack");
  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::UpdateMediaTrackSettingsAndCapabilities,
                    WrapPersistent(this), std::move(done_callback)));
}

MediaTrackConstraints* ImageCapture::GetMediaTrackConstraints() const {
  return current_constraints_.Get();
}

void ImageCapture::ClearMediaTrackConstraints() {
  current_constraints_ = nullptr;

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
                           base::OnceClosure initialized_callback,
                           base::TimeDelta grab_frame_timeout)
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
      photo_settings_(PhotoSettings::Create()),
      grab_frame_timeout_(grab_frame_timeout) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::CreateImageCapture");
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
      WTF::BindOnce(&ImageCapture::SetVideoTrackDeviceSettingsFromTrack,
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

// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove this support function.
void ImageCapture::ApplyMediaTrackConstraintSetToSettings(
    media::mojom::blink::PhotoSettings* settings,
    MediaTrackCapabilities* effective_capabilities,
    MediaTrackSettings* effective_settings,
    const MediaTrackConstraintSet* constraint_set,
    MediaTrackConstraintSetType constraint_set_type) const {
  // Apply value constraints to photo settings and update effective
  // capabilities.
  //
  // Roughly the SelectSettings algorithm steps 3 and 5.
  // https://www.w3.org/TR/mediacapture-streams/#dfn-selectsettings
  if (constraint_set->hasWhiteBalanceMode() &&
      effective_capabilities->hasWhiteBalanceMode()) {
    effective_capabilities->setWhiteBalanceMode(ApplyValueConstraint(
        &settings->has_white_balance_mode, &settings->white_balance_mode,
        effective_capabilities->whiteBalanceMode(),
        constraint_set->whiteBalanceMode(), constraint_set_type,
        settings_->whiteBalanceMode()));
  }
  if (constraint_set->hasExposureMode() &&
      effective_capabilities->hasExposureMode()) {
    effective_capabilities->setExposureMode(ApplyValueConstraint(
        &settings->has_exposure_mode, &settings->exposure_mode,
        effective_capabilities->exposureMode(), constraint_set->exposureMode(),
        constraint_set_type, settings_->exposureMode()));
  }
  if (constraint_set->hasFocusMode() &&
      effective_capabilities->hasFocusMode()) {
    effective_capabilities->setFocusMode(ApplyValueConstraint(
        &settings->has_focus_mode, &settings->focus_mode,
        effective_capabilities->focusMode(), constraint_set->focusMode(),
        constraint_set_type, settings_->focusMode()));
  }
  if (constraint_set->hasPointsOfInterest()) {
    // There is no |settings->has_points_of_interest|.
    bool has_points_of_interest = !settings->points_of_interest.empty();
    std::optional new_effective_setting = ApplyValueConstraint(
        &has_points_of_interest, &settings->points_of_interest,
        effective_settings->hasPointsOfInterest()
            ? &effective_settings->pointsOfInterest()
            : nullptr,
        constraint_set->pointsOfInterest(), constraint_set_type);
    if (new_effective_setting) {
      effective_settings->setPointsOfInterest(*new_effective_setting);
    }
  }
  if (constraint_set->hasExposureCompensation() &&
      effective_capabilities->hasExposureCompensation()) {
    effective_capabilities->setExposureCompensation(ApplyValueConstraint(
        &settings->has_exposure_compensation, &settings->exposure_compensation,
        effective_capabilities->exposureCompensation(),
        constraint_set->exposureCompensation(), constraint_set_type,
        settings_->exposureCompensation()));
  }
  if (constraint_set->hasExposureTime() &&
      effective_capabilities->hasExposureTime()) {
    effective_capabilities->setExposureTime(ApplyValueConstraint(
        &settings->has_exposure_time, &settings->exposure_time,
        effective_capabilities->exposureTime(), constraint_set->exposureTime(),
        constraint_set_type, settings_->exposureTime()));
  }
  if (constraint_set->hasColorTemperature() &&
      effective_capabilities->hasColorTemperature()) {
    effective_capabilities->setColorTemperature(ApplyValueConstraint(
        &settings->has_color_temperature, &settings->color_temperature,
        effective_capabilities->colorTemperature(),
        constraint_set->colorTemperature(), constraint_set_type,
        settings_->colorTemperature()));
  }
  if (constraint_set->hasIso() && effective_capabilities->hasIso()) {
    effective_capabilities->setIso(ApplyValueConstraint(
        &settings->has_iso, &settings->iso, effective_capabilities->iso(),
        constraint_set->iso(), constraint_set_type, settings_->iso()));
  }
  if (constraint_set->hasBrightness() &&
      effective_capabilities->hasBrightness()) {
    effective_capabilities->setBrightness(ApplyValueConstraint(
        &settings->has_brightness, &settings->brightness,
        effective_capabilities->brightness(), constraint_set->brightness(),
        constraint_set_type, settings_->brightness()));
  }
  if (constraint_set->hasContrast() && effective_capabilities->hasContrast()) {
    effective_capabilities->setContrast(ApplyValueConstraint(
        &settings->has_contrast, &settings->contrast,
        effective_capabilities->contrast(), constraint_set->contrast(),
        constraint_set_type, settings_->contrast()));
  }
  if (constraint_set->hasSaturation() &&
      effective_capabilities->hasSaturation()) {
    effective_capabilities->setSaturation(ApplyValueConstraint(
        &settings->has_saturation, &settings->saturation,
        effective_capabilities->saturation(), constraint_set->saturation(),
        constraint_set_type, settings_->saturation()));
  }
  if (constraint_set->hasSharpness() &&
      effective_capabilities->hasSharpness()) {
    effective_capabilities->setSharpness(ApplyValueConstraint(
        &settings->has_sharpness, &settings->sharpness,
        effective_capabilities->sharpness(), constraint_set->sharpness(),
        constraint_set_type, settings_->sharpness()));
  }
  if (constraint_set->hasFocusDistance() &&
      effective_capabilities->hasFocusDistance()) {
    effective_capabilities->setFocusDistance(ApplyValueConstraint(
        &settings->has_focus_distance, &settings->focus_distance,
        effective_capabilities->focusDistance(),
        constraint_set->focusDistance(), constraint_set_type,
        settings_->focusDistance()));
  }
  if (constraint_set->hasPan() && effective_capabilities->hasPan()) {
    effective_capabilities->setPan(ApplyValueConstraint(
        &settings->has_pan, &settings->pan, effective_capabilities->pan(),
        constraint_set->pan(), constraint_set_type, settings_->pan()));
  }
  if (constraint_set->hasTilt() && effective_capabilities->hasTilt()) {
    effective_capabilities->setTilt(ApplyValueConstraint(
        &settings->has_tilt, &settings->tilt, effective_capabilities->tilt(),
        constraint_set->tilt(), constraint_set_type, settings_->tilt()));
  }
  if (constraint_set->hasZoom() && effective_capabilities->hasZoom()) {
    effective_capabilities->setZoom(ApplyValueConstraint(
        &settings->has_zoom, &settings->zoom, effective_capabilities->zoom(),
        constraint_set->zoom(), constraint_set_type, settings_->zoom()));
  }
  if (constraint_set->hasTorch() && effective_capabilities->hasTorch() &&
      effective_capabilities->torch()) {
    const auto& new_effective_capability =
        ApplyValueConstraint(&settings->has_torch, &settings->torch,
                             effective_settings->hasTorch()
                                 ? Vector<bool>({effective_settings->torch()})
                                 : Vector<bool>({false, true}),
                             constraint_set->torch(), constraint_set_type);
    if (new_effective_capability.size() == 1u) {
      effective_settings->setTorch(new_effective_capability[0]);
    }
  }
  if (constraint_set->hasBackgroundBlur() &&
      effective_capabilities->hasBackgroundBlur()) {
    bool has_setting = false;
    bool setting;
    effective_capabilities->setBackgroundBlur(ApplyValueConstraint(
        &has_setting, &setting, effective_capabilities->backgroundBlur(),
        constraint_set->backgroundBlur(), constraint_set_type));
    if (has_setting) {
      settings->has_background_blur_mode = true;
      settings->background_blur_mode = ParseBackgroundBlur(setting);
    }
  }
  if (constraint_set->hasBackgroundSegmentationMask() &&
      effective_capabilities->hasBackgroundSegmentationMask()) {
    bool has_setting = false;
    bool setting;
    effective_capabilities->setBackgroundSegmentationMask(ApplyValueConstraint(
        &has_setting, &setting,
        effective_capabilities->backgroundSegmentationMask(),
        constraint_set->backgroundSegmentationMask(), constraint_set_type));
    if (has_setting) {
      settings->background_segmentation_mask_state.emplace(setting);
    }
  }
  if (constraint_set->hasEyeGazeCorrection() &&
      effective_capabilities->hasEyeGazeCorrection()) {
    bool has_setting = false;
    bool setting;
    effective_capabilities->setEyeGazeCorrection(ApplyValueConstraint(
        &has_setting, &setting, effective_capabilities->eyeGazeCorrection(),
        constraint_set->eyeGazeCorrection(), constraint_set_type));
    if (has_setting) {
      settings->eye_gaze_correction_mode.emplace(
          ParseEyeGazeCorrection(setting));
    }
  }
  if (constraint_set->hasFaceFraming() &&
      effective_capabilities->hasFaceFraming()) {
    bool has_setting = false;
    bool setting;
    effective_capabilities->setFaceFraming(ApplyValueConstraint(
        &has_setting, &setting, effective_capabilities->faceFraming(),
        constraint_set->faceFraming(), constraint_set_type));
    if (has_setting) {
      settings->has_face_framing_mode = true;
      settings->face_framing_mode = ParseFaceFraming(setting);
    }
  }
}

// TODO(crbug.com/708723): Integrate image capture constraints processing with
// the main implementation and remove this support function.
bool ImageCapture::CheckMediaTrackConstraintSet(
    const MediaTrackCapabilities* effective_capabilities,
    const MediaTrackSettings* effective_settings,
    const MediaTrackConstraintSet* constraint_set,
    MediaTrackConstraintSetType constraint_set_type,
    ScriptPromiseResolverBase* resolver) const {
  if (std::optional<const char*> name =
          GetConstraintWithCapabilityExistenceMismatch(constraint_set,
                                                       constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, name.value(),
                                        "Unsupported constraint");
    return false;
  }

  if (constraint_set->hasWhiteBalanceMode() &&
      effective_capabilities->hasWhiteBalanceMode() &&
      !CheckValueConstraint(effective_capabilities->whiteBalanceMode(),
                            constraint_set->whiteBalanceMode(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "whiteBalanceMode",
                                        "Unsupported whiteBalanceMode.");
    return false;
  }
  if (constraint_set->hasExposureMode() &&
      effective_capabilities->hasExposureMode() &&
      !CheckValueConstraint(effective_capabilities->exposureMode(),
                            constraint_set->exposureMode(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "exposureMode",
                                        "Unsupported exposureMode.");
    return false;
  }
  if (constraint_set->hasFocusMode() &&
      effective_capabilities->hasFocusMode() &&
      !CheckValueConstraint(effective_capabilities->focusMode(),
                            constraint_set->focusMode(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "focusMode",
                                        "Unsupported focusMode.");
    return false;
  }
  if (constraint_set->hasPointsOfInterest() &&
      !CheckValueConstraint(effective_settings->hasPointsOfInterest()
                                ? &effective_settings->pointsOfInterest()
                                : nullptr,
                            constraint_set->pointsOfInterest(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "pointsOfInterest", "pointsOfInterest setting out of range");
    return false;
  }
  if (constraint_set->hasExposureCompensation() &&
      effective_capabilities->hasExposureCompensation() &&
      !CheckValueConstraint(effective_capabilities->exposureCompensation(),
                            constraint_set->exposureCompensation(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "exposureCompensation",
        "exposureCompensation setting out of range");
    return false;
  }
  if (constraint_set->hasExposureTime() &&
      effective_capabilities->hasExposureTime() &&
      !CheckValueConstraint(effective_capabilities->exposureTime(),
                            constraint_set->exposureTime(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "exposureTime",
                                        "exposureTime setting out of range");
    return false;
  }
  if (constraint_set->hasColorTemperature() &&
      effective_capabilities->hasColorTemperature() &&
      !CheckValueConstraint(effective_capabilities->colorTemperature(),
                            constraint_set->colorTemperature(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "colorTemperature", "colorTemperature setting out of range");
    return false;
  }
  if (constraint_set->hasIso() && effective_capabilities->hasIso() &&
      !CheckValueConstraint(effective_capabilities->iso(),
                            constraint_set->iso(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "iso",
                                        "iso setting out of range");
    return false;
  }
  if (constraint_set->hasBrightness() &&
      effective_capabilities->hasBrightness() &&
      !CheckValueConstraint(effective_capabilities->brightness(),
                            constraint_set->brightness(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "brightness",
                                        "brightness setting out of range");
    return false;
  }
  if (constraint_set->hasContrast() && effective_capabilities->hasContrast() &&
      !CheckValueConstraint(effective_capabilities->contrast(),
                            constraint_set->contrast(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "contrast",
                                        "contrast setting out of range");
    return false;
  }
  if (constraint_set->hasSaturation() &&
      effective_capabilities->hasSaturation() &&
      !CheckValueConstraint(effective_capabilities->saturation(),
                            constraint_set->saturation(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "saturation",
                                        "saturation setting out of range");
    return false;
  }
  if (constraint_set->hasSharpness() &&
      effective_capabilities->hasSharpness() &&
      !CheckValueConstraint(effective_capabilities->sharpness(),
                            constraint_set->sharpness(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "sharpness",
                                        "sharpness setting out of range");
    return false;
  }
  if (constraint_set->hasFocusDistance() &&
      effective_capabilities->hasFocusDistance() &&
      !CheckValueConstraint(effective_capabilities->focusDistance(),
                            constraint_set->focusDistance(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "focusDistance",
                                        "focusDistance setting out of range");
    return false;
  }
  if (constraint_set->hasPan() && effective_capabilities->hasPan() &&
      !CheckValueConstraint(effective_capabilities->pan(),
                            constraint_set->pan(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "pan",
                                        "pan setting out of range");
    return false;
  }
  if (constraint_set->hasTilt() && effective_capabilities->hasTilt() &&
      !CheckValueConstraint(effective_capabilities->tilt(),
                            constraint_set->tilt(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "tilt",
                                        "tilt setting out of range");
    return false;
  }
  if (constraint_set->hasZoom() && effective_capabilities->hasZoom() &&
      !CheckValueConstraint(effective_capabilities->zoom(),
                            constraint_set->zoom(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "zoom",
                                        "zoom setting out of range");
    return false;
  }
  if (constraint_set->hasTorch() && effective_capabilities->hasTorch() &&
      effective_capabilities->torch() &&
      !CheckValueConstraint(effective_settings->hasTorch()
                                ? Vector<bool>({effective_settings->torch()})
                                : Vector<bool>({false, true}),
                            constraint_set->torch(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(resolver, "torch",
                                        "torch not supported");
    return false;
  }
  if (constraint_set->hasBackgroundBlur() &&
      effective_capabilities->hasBackgroundBlur() &&
      !CheckValueConstraint(effective_capabilities->backgroundBlur(),
                            constraint_set->backgroundBlur(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "backgroundBlur",
        "backgroundBlur setting value not supported");
    return false;
  }
  if (constraint_set->hasBackgroundSegmentationMask() &&
      effective_capabilities->hasBackgroundSegmentationMask() &&
      !CheckValueConstraint(
          effective_capabilities->backgroundSegmentationMask(),
          constraint_set->backgroundSegmentationMask(), constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "backgroundSegmentationMask",
        "backgroundSegmentationMask setting value not supported");
    return false;
  }
  if (constraint_set->hasEyeGazeCorrection() &&
      effective_capabilities->hasEyeGazeCorrection() &&
      !CheckValueConstraint(effective_capabilities->eyeGazeCorrection(),
                            constraint_set->eyeGazeCorrection(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "eyeGazeCorrection",
        "eyeGazeCorrection setting value not supported");
    return false;
  }
  if (constraint_set->hasFaceFraming() &&
      effective_capabilities->hasFaceFraming() &&
      !CheckValueConstraint(effective_capabilities->faceFraming(),
                            constraint_set->faceFraming(),
                            constraint_set_type)) {
    MaybeRejectWithOverconstrainedError(
        resolver, "faceFraming", "faceFraming setting value not supported");
    return false;
  }

  return true;
}

void ImageCapture::OnPermissionStatusChange(
    mojom::blink::PermissionStatus status) {
  pan_tilt_zoom_permission_ = status;
}

bool ImageCapture::HasPanTiltZoomPermissionGranted() const {
  return pan_tilt_zoom_permission_ == mojom::blink::PermissionStatus::GRANTED;
}

void ImageCapture::GetMojoPhotoState(ScriptPromiseResolverBase* resolver,
                                     PromiseResolverFunction resolver_cb) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::GetMojoPhotoState");
  if (TrackIsInactive(*stream_track_)) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, kInvalidStateTrackError));
    return;
  }

  if (!service_.is_bound()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
    return;
  }
  service_requests_.insert(resolver);

  service_->GetPhotoState(
      SourceId(),
      WTF::BindOnce(&ImageCapture::OnMojoGetPhotoState, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(resolver_cb),
                    /*trigger_take_photo=*/false));
}

void ImageCapture::OnMojoGetPhotoState(
    ScriptPromiseResolverBase* resolver,
    PromiseResolverFunction resolve_function,
    bool trigger_take_photo,
    media::mojom::blink::PhotoStatePtr photo_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::OnMojoGetPhotoState");
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

void ImageCapture::OnMojoSetPhotoOptions(ScriptPromiseResolverBase* resolver,
                                         bool trigger_take_photo,
                                         bool result) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::OnMojoSetPhotoOptions");

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

void ImageCapture::OnMojoTakePhoto(ScriptPromiseResolverBase* resolver,
                                   media::mojom::blink::BlobPtr blob) {
  DCHECK(service_requests_.Contains(resolver));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "ImageCapture::OnMojoTakePhoto", "blob_size", blob->data.size());

  // TODO(mcasas): Should be using a mojo::StructTraits.
  if (blob->data.empty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, "platform error"));
  } else {
    resolver->DowncastTo<Blob>()->Resolve(
        Blob::Create(blob->data, blob->mime_type));
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
    for (auto mode : *photo_state->supported_background_blur_modes) {
      bool boolean_mode = ToBooleanMode(mode);
      if (!base::Contains(supported_background_blur_modes, boolean_mode)) {
        supported_background_blur_modes.push_back(boolean_mode);
      }
    }
    capabilities_->setBackgroundBlur(
        std::move(supported_background_blur_modes));
    settings_->setBackgroundBlur(
        ToBooleanMode(photo_state->background_blur_mode));
  }

  if (photo_state->supported_background_segmentation_mask_states &&
      !photo_state->supported_background_segmentation_mask_states->empty()) {
    capabilities_->setBackgroundSegmentationMask(
        *photo_state->supported_background_segmentation_mask_states);
    settings_->setBackgroundSegmentationMask(
        photo_state->current_background_segmentation_mask_state);
  }

  if (photo_state->supported_eye_gaze_correction_modes &&
      !photo_state->supported_eye_gaze_correction_modes->empty()) {
    Vector<bool> supported_eye_gaze_correction_modes;
    for (const auto& mode : *photo_state->supported_eye_gaze_correction_modes) {
      bool boolean_mode = ToBooleanMode(mode);
      if (!base::Contains(supported_eye_gaze_correction_modes, boolean_mode)) {
        supported_eye_gaze_correction_modes.push_back(boolean_mode);
      }
    }
    capabilities_->setEyeGazeCorrection(
        std::move(supported_eye_gaze_correction_modes));
    settings_->setEyeGazeCorrection(
        ToBooleanMode(photo_state->current_eye_gaze_correction_mode));
  }

  if (photo_state->supported_face_framing_modes &&
      !photo_state->supported_face_framing_modes->empty()) {
    Vector<bool> supported_face_framing_modes;
    for (auto mode : *photo_state->supported_face_framing_modes) {
      if (mode == MeteringMode::CONTINUOUS ||
          mode == MeteringMode::SINGLE_SHOT) {
        supported_face_framing_modes.push_back(true);
      } else if (mode == MeteringMode::NONE) {
        supported_face_framing_modes.push_back(false);
      }
    }
    if (!supported_face_framing_modes.empty()) {
      capabilities_->setFaceFraming(supported_face_framing_modes);
      settings_->setFaceFraming(photo_state->current_face_framing_mode !=
                                MeteringMode::NONE);
    }
  }

  std::move(initialized_callback).Run();
}

void ImageCapture::OnServiceConnectionError() {
  service_.reset();

  HeapHashSet<Member<ScriptPromiseResolverBase>> resolvers;
  resolvers.swap(service_requests_);
  for (ScriptPromiseResolverBase* resolver : resolvers) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotFoundError, kNoServiceError));
  }
}

void ImageCapture::MaybeRejectWithOverconstrainedError(
    ScriptPromiseResolverBase* resolver,
    const char* constraint,
    const char* message) const {
  if (!resolver) {
    return;
  }
  resolver->Reject(
      MakeGarbageCollected<OverconstrainedError>(constraint, message));
}

void ImageCapture::ResolveWithNothing(ScriptPromiseResolverBase* resolver) {
  DCHECK(resolver);
  resolver->DowncastTo<IDLUndefined>()->Resolve();
}

void ImageCapture::ResolveWithPhotoSettings(
    ScriptPromiseResolverBase* resolver) {
  DCHECK(resolver);
  resolver->DowncastTo<PhotoSettings>()->Resolve(photo_settings_);
}

void ImageCapture::ResolveWithPhotoCapabilities(
    ScriptPromiseResolverBase* resolver) {
  DCHECK(resolver);
  resolver->DowncastTo<PhotoCapabilities>()->Resolve(photo_capabilities_);
}

bool ImageCapture::IsPageVisible() const {
  return DomWindow() && DomWindow()->document()->IsPageVisible();
}

const String& ImageCapture::SourceId() const {
  return stream_track_->Component()->Source()->Id();
}

const std::optional<const char*>
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
  if (constraint_set->hasBackgroundSegmentationMask() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->backgroundSegmentationMask(),
          CapabilityExists(capabilities_->hasBackgroundSegmentationMask()),
          constraint_set_type)) {
    return "backgroundSegmentationMask";
  }
  if (constraint_set->hasEyeGazeCorrection() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->eyeGazeCorrection(),
          CapabilityExists(capabilities_->hasEyeGazeCorrection()),
          constraint_set_type)) {
    return "eyeGazeCorrection";
  }
  if (constraint_set->hasFaceFraming() &&
      !CheckIfCapabilityExistenceSatisfiesConstraint(
          constraint_set->faceFraming(),
          CapabilityExists(capabilities_->hasFaceFraming()),
          constraint_set_type)) {
    return "faceFraming";
  }
  return std::nullopt;
}

ImageCapture* ImageCapture::Clone() const {
  ImageCapture* clone = MakeGarbageCollected<ImageCapture>(
      GetExecutionContext(), stream_track_, HasPanTiltZoomPermissionGranted(),
      /*callback=*/base::DoNothing());

  // Copy capabilities.
  CopyCapabilities(capabilities_, clone->capabilities_, CopyPanTiltZoom(true));

  // Copy settings.
  CopySettings(settings_, clone->settings_, CopyPanTiltZoom(true));

  // Copy current constraints.
  if (current_constraints_) {
    clone->current_constraints_ = MediaTrackConstraints::Create();
    CopyConstraints(current_constraints_, clone->current_constraints_);
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
  visitor->Trace(current_constraints_);
  visitor->Trace(photo_capabilities_);
  visitor->Trace(service_requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
