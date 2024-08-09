// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"
#include "media/base/video_types.h"
#include "media/mojo/mojom/display_media_information.mojom-blink.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

using ResolutionSet = media_constraints::ResolutionSet;
using DoubleRangeSet = media_constraints::NumericRangeSet<double>;
using IntRangeSet = media_constraints::NumericRangeSet<int32_t>;
using BoolSet = media_constraints::DiscreteSet<bool>;
// TODO(crbug.com/704136): Replace VideoInputDeviceCapabilities with Blink
// mojo pointer type once dependent types are migrated to Blink.
using DeviceInfo = VideoInputDeviceCapabilities;
using DistanceVector = WTF::Vector<double>;

// Number of default settings to be used as final tie-breaking criteria for
// settings that are equally good at satisfying constraints:
// device ID, noise reduction, resolution and frame rate.
const int kNumDefaultDistanceEntries = 4;

WebString ToWebString(mojom::blink::FacingMode facing_mode) {
  switch (facing_mode) {
    case mojom::blink::FacingMode::kUser:
      return WebString::FromASCII("user");
    case mojom::blink::FacingMode::kEnvironment:
      return WebString::FromASCII("environment");
    default:
      return WebString();
  }
}

double BoolSetFitness(const BooleanConstraint& constraint, const BoolSet& set) {
  DCHECK(!set.IsEmpty());

  if (!constraint.HasIdeal()) {
    return 0.0;
  }

  bool ideal = constraint.Ideal();
  return set.Contains(ideal) ? 0.0 : 1.0;
}

// Returns the fitness distance between the ideal value of |constraint| and
// |value|. Based on
// https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
template <typename NumericConstraint>
double NumericValueFitness(const NumericConstraint& constraint,
                           decltype(constraint.Min()) value) {
  return constraint.HasIdeal()
             ? NumericConstraintFitnessDistance(value, constraint.Ideal())
             : 0.0;
}

// Returns the fitness distance between the ideal value of |constraint| and the
// closest value to it in the range [min, max].
// If the ideal value is contained in the range, returns 0.
// If there is no ideal value, returns 0;
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
template <typename NumericConstraint>
double NumericRangeSetFitness(
    const NumericConstraint& constraint,
    const media_constraints::NumericRangeSet<decltype(constraint.Min())>&
        range) {
  DCHECK(!range.IsEmpty());

  if (!constraint.HasIdeal()) {
    return 0.0;
  }

  auto ideal = constraint.Ideal();
  if (range.Max().has_value() && ideal > *range.Max()) {
    return NumericConstraintFitnessDistance(ideal, *range.Max());
  } else if (range.Min().has_value() && ideal < *range.Min()) {
    return NumericConstraintFitnessDistance(ideal, *range.Min());
  }

  return 0.0;  // |range| contains |ideal|
}

// Returns the fitness distance between the ideal value of |constraint| and the
// closest value to it in the range [min, max] if the constraint is supported.
// If the constraint is present but not supported, returns 1.
// If the ideal value is contained in the range, returns 0.
// If there is no ideal value, returns 0;
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
template <typename NumericConstraint>
double NumericRangeSupportFitness(
    const NumericConstraint& constraint,
    const media_constraints::NumericRangeSet<decltype(constraint.Min())>& range,
    bool constraint_present,
    bool constraint_supported) {
  DCHECK(!range.IsEmpty());

  if (constraint_present && !constraint_supported) {
    return 1.0;
  }

  return NumericRangeSetFitness(constraint, range);
}

// Returns a custom distance between |native_value| and the ideal value and
// allowed range for a constrainable property. The ideal value is obtained from
// |constraint| and the allowed range is specified by |min| and |max|.
// The allowed range is not obtained from |constraint| because it might be the
// result of the application of multiple constraint sets.
// The custom distance is computed using the spec-defined fitness distance
// between |native_value| and the value within the range [|min|, |max|] closest
// to the ideal value.
// If there is no ideal value and |native_value| is greater than |max|, the
// distance between |max| and |native_value| is returned.
// The purpose of this function is to be used to break ties among equally good
// candidates by penalizing those whose native settings are further from the
// range and ideal values specified by constraints.
template <typename NumericConstraint>
double NumericRangeNativeFitness(const NumericConstraint& constraint,
                                 decltype(constraint.Min()) min,
                                 decltype(constraint.Min()) max,
                                 decltype(constraint.Min()) native_value) {
  auto reference_value = constraint.HasIdeal()
                             ? std::max(std::min(constraint.Ideal(), max), min)
                             : max;
  return NumericConstraintFitnessDistance(native_value, reference_value);
}

// Returns the fitness distance between the ideal value of |constraint| and
// an optional boolean |value|.
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
double OptionalBoolFitness(const std::optional<bool>& value,
                           const BooleanConstraint& constraint) {
  if (!constraint.HasIdeal()) {
    return 0.0;
  }

  return value && value == constraint.Ideal() ? 0.0 : 1.0;
}

// If |failed_constraint_name| is not null, this function updates it with the
// name of |constraint|.
void UpdateFailedConstraintName(const BaseConstraint& constraint,
                                const char** failed_constraint_name) {
  if (failed_constraint_name) {
    *failed_constraint_name = constraint.GetName();
  }
}

// The CandidateFormat class keeps track of the effect of constraint sets on
// the range of values supported by a video-capture format. For example, suppose
// a device supports a width of 1024. Then, in principle, it can support any
// width below 1024 using cropping and rescaling. Suppose the first advanced
// constraint set requests a maximum width of 640, and the second advanced
// constraint set requests a minimum of 800. Separately, the camera supports
// both advanced sets. However, if the first set is supported, the second set
// can no longer be supported because width can no longer exceed 640. The
// CandidateFormat class keeps track of this.
class CandidateFormat {
 public:
  class ApplyConstraintSetResult {
   public:
    ApplyConstraintSetResult() = default;

   private:
    friend class CandidateFormat;

    DoubleRangeSet constrained_frame_rate_;
    IntRangeSet constrained_width_;
    IntRangeSet constrained_height_;
    DoubleRangeSet constrained_aspect_ratio_;

    BoolSet rescale_intersection_;
    ResolutionSet resolution_intersection_;
  };

  explicit CandidateFormat(const media::VideoCaptureFormat& format)
      : format_(format),
        resolution_set_(1,
                        format.frame_size.height(),
                        1,
                        format.frame_size.width(),
                        0.0,
                        HUGE_VAL) {}

  const media::VideoCaptureFormat& format() const { return format_; }
  const ResolutionSet& resolution_set() const { return resolution_set_; }
  const DoubleRangeSet& constrained_frame_rate() const {
    return constrained_frame_rate_;
  }

  // Convenience accessors for format() fields.
  int NativeHeight() const { return format_.frame_size.height(); }
  int NativeWidth() const { return format_.frame_size.width(); }
  double NativeAspectRatio() const {
    DCHECK(NativeWidth() > 0 || NativeHeight() > 0);
    return static_cast<double>(NativeWidth()) / NativeHeight();
  }
  double NativeFrameRate() const { return format_.frame_rate; }

  // Convenience accessors for accessors for resolution_set() fields. They
  // return the minimum and maximum resolution settings supported by this
  // format, subject to applied constraints.
  int MinHeight() const { return resolution_set_.min_height(); }
  int MaxHeight() const { return resolution_set_.max_height(); }
  int MinWidth() const { return resolution_set_.min_width(); }
  int MaxWidth() const { return resolution_set_.max_width(); }
  double MinAspectRatio() const {
    return std::max(resolution_set_.min_aspect_ratio(),
                    static_cast<double>(MinWidth()) / MaxHeight());
  }
  double MaxAspectRatio() const {
    return std::min(resolution_set_.max_aspect_ratio(),
                    static_cast<double>(MaxWidth()) / MinHeight());
  }

  // Convenience accessors for constrained_frame_rate() fields.
  const std::optional<double>& MinFrameRateConstraint() const {
    return constrained_frame_rate_.Min();
  }
  const std::optional<double>& MaxFrameRateConstraint() const {
    return constrained_frame_rate_.Max();
  }

  // Accessors that return the minimum and maximum frame rates supported by
  // this format, subject to applied constraints.
  double MaxFrameRate() const {
    if (MaxFrameRateConstraint()) {
      return std::min(*MaxFrameRateConstraint(), NativeFrameRate());
    }
    return NativeFrameRate();
  }
  double MinFrameRate() const {
    if (MinFrameRateConstraint()) {
      return std::max(*MinFrameRateConstraint(), kMinDeviceCaptureFrameRate);
    }
    return kMinDeviceCaptureFrameRate;
  }

  // This function tries to apply |constraint_set| and returns the result
  // if successful. If |constraint_set| cannot be satisfied,
  // a nullopt is returned, and the name of one of the constraints that
  // could not be satisfied is returned in |failed_constraint_name| if
  // |failed_constraint_name| is not null.
  std::optional<ApplyConstraintSetResult> TryToApplyConstraintSet(
      const MediaTrackConstraintSetPlatform& constraint_set,
      const char** failed_constraint_name = nullptr) const {
    std::optional<ApplyConstraintSetResult> result(std::in_place);

    result->rescale_intersection_ =
        rescale_set_.Intersection(media_constraints::RescaleSetFromConstraint(
            constraint_set.resize_mode));
    if (result->rescale_intersection_.IsEmpty()) {
      UpdateFailedConstraintName(constraint_set.resize_mode,
                                 failed_constraint_name);
      return std::nullopt;
    }

    result->resolution_intersection_ = resolution_set_.Intersection(
        ResolutionSet::FromConstraintSet(constraint_set));
    if (!result->rescale_intersection_.Contains(true)) {
      // If rescaling is not allowed, only the native resolution is allowed.
      result->resolution_intersection_ =
          result->resolution_intersection_.Intersection(
              ResolutionSet::FromExactResolution(NativeWidth(),
                                                 NativeHeight()));
    }
    if (result->resolution_intersection_.IsWidthEmpty()) {
      UpdateFailedConstraintName(constraint_set.width, failed_constraint_name);
      return std::nullopt;
    }
    if (result->resolution_intersection_.IsHeightEmpty()) {
      UpdateFailedConstraintName(constraint_set.height, failed_constraint_name);
      return std::nullopt;
    }
    if (result->resolution_intersection_.IsAspectRatioEmpty()) {
      UpdateFailedConstraintName(constraint_set.aspect_ratio,
                                 failed_constraint_name);
      return std::nullopt;
    }

    if (!SatisfiesFrameRateConstraint(constraint_set.frame_rate)) {
      UpdateFailedConstraintName(constraint_set.frame_rate,
                                 failed_constraint_name);
      return std::nullopt;
    }

    result->constrained_frame_rate_ = constrained_frame_rate_.Intersection(
        DoubleRangeSet::FromConstraint(constraint_set.frame_rate, 0.0,
                                       media::limits::kMaxFramesPerSecond));
    result->constrained_width_ =
        constrained_width_.Intersection(IntRangeSet::FromConstraint(
            constraint_set.width, 1L, ResolutionSet::kMaxDimension));
    result->constrained_height_ =
        constrained_height_.Intersection(IntRangeSet::FromConstraint(
            constraint_set.height, 1L, ResolutionSet::kMaxDimension));
    result->constrained_aspect_ratio_ =
        constrained_aspect_ratio_.Intersection(DoubleRangeSet::FromConstraint(
            constraint_set.aspect_ratio, 0.0, HUGE_VAL));

    return result;
  }

  void ApplyResult(const ApplyConstraintSetResult& result) {
    constrained_frame_rate_ = result.constrained_frame_rate_;
    constrained_width_ = result.constrained_width_;
    constrained_height_ = result.constrained_height_;
    constrained_aspect_ratio_ = result.constrained_aspect_ratio_;
    resolution_set_ = result.resolution_intersection_;
    rescale_set_ = result.rescale_intersection_;
  }

  // Returns the best fitness distance that can be achieved with this candidate
  // format based on distance from the ideal values in |basic_constraint_set|.
  // The track settings that correspond to this fitness are returned on the
  // |track_settings| output parameter. The fitness function is based on
  // https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
  double Fitness(const MediaTrackConstraintSetPlatform& basic_constraint_set,
                 VideoTrackAdapterSettings* track_settings) const {
    DCHECK(!rescale_set_.IsEmpty());
    double track_fitness_with_rescale = HUGE_VAL;
    VideoTrackAdapterSettings track_settings_with_rescale;
    if (rescale_set_.Contains(true)) {
      track_settings_with_rescale = SelectVideoTrackAdapterSettings(
          basic_constraint_set, resolution_set(), constrained_frame_rate(),
          format(), true /* enable_rescale */);
      DCHECK(track_settings_with_rescale.target_size().has_value());
      double target_aspect_ratio =
          static_cast<double>(track_settings_with_rescale.target_width()) /
          track_settings_with_rescale.target_height();
      DCHECK(!std::isnan(target_aspect_ratio));
      std::optional<double> best_supported_frame_rate =
          track_settings_with_rescale.max_frame_rate();
      if (!best_supported_frame_rate.has_value() ||
          *best_supported_frame_rate > NativeFrameRate()) {
        best_supported_frame_rate = NativeFrameRate();
      }

      track_fitness_with_rescale =
          NumericValueFitness(basic_constraint_set.aspect_ratio,
                              target_aspect_ratio) +
          NumericValueFitness(basic_constraint_set.height,
                              track_settings_with_rescale.target_height()) +
          NumericValueFitness(basic_constraint_set.width,
                              track_settings_with_rescale.target_width()) +
          NumericValueFitness(basic_constraint_set.frame_rate,
                              *best_supported_frame_rate);
    }

    double track_fitness_without_rescale = HUGE_VAL;
    VideoTrackAdapterSettings track_settings_without_rescale;
    if (rescale_set_.Contains(false)) {
      bool can_use_native_resolution =
          constrained_width_.Contains(NativeWidth()) &&
          constrained_height_.Contains(NativeHeight()) &&
          constrained_aspect_ratio_.Contains(NativeAspectRatio());
      if (can_use_native_resolution) {
        track_settings_without_rescale = SelectVideoTrackAdapterSettings(
            basic_constraint_set, resolution_set(), constrained_frame_rate(),
            format(), false /* enable_rescale */);
        DCHECK(!track_settings_without_rescale.target_size().has_value());
        std::optional<double> best_supported_frame_rate =
            track_settings_without_rescale.max_frame_rate();
        if (!best_supported_frame_rate.has_value() ||
            *best_supported_frame_rate > NativeFrameRate()) {
          best_supported_frame_rate = NativeFrameRate();
        }
        track_fitness_without_rescale =
            NumericValueFitness(basic_constraint_set.aspect_ratio,
                                NativeAspectRatio()) +
            NumericValueFitness(basic_constraint_set.height, NativeHeight()) +
            NumericValueFitness(basic_constraint_set.width, NativeWidth()) +
            NumericValueFitness(basic_constraint_set.frame_rate,
                                *best_supported_frame_rate);
      }
    }

    if (basic_constraint_set.resize_mode.HasIdeal()) {
      if (!base::Contains(basic_constraint_set.resize_mode.Ideal(),
                          WebMediaStreamTrack::kResizeModeNone)) {
        track_fitness_without_rescale += 1.0;
      }
      if (!base::Contains(basic_constraint_set.resize_mode.Ideal(),
                          WebMediaStreamTrack::kResizeModeRescale)) {
        track_fitness_with_rescale += 1.0;
      }
    }

    // If rescaling and not rescaling have the same fitness, prefer not
    // rescaling.
    if (track_fitness_without_rescale <= track_fitness_with_rescale) {
      *track_settings = track_settings_without_rescale;
      return track_fitness_without_rescale;
    }

    *track_settings = track_settings_with_rescale;
    return track_fitness_with_rescale;
  }

  // Returns a custom "native" fitness distance that expresses how close the
  // native settings of this format are to the ideal and allowed ranges for
  // the corresponding width, height and frameRate properties.
  // This distance is intended to be used to break ties among candidates that
  // are equally good according to the standard fitness distance.
  double NativeFitness(
      const MediaTrackConstraintSetPlatform& constraint_set) const {
    return NumericRangeNativeFitness(constraint_set.width, MinWidth(),
                                     MaxWidth(), NativeWidth()) +
           NumericRangeNativeFitness(constraint_set.height, MinHeight(),
                                     MaxHeight(), NativeHeight()) +
           NumericRangeNativeFitness(constraint_set.frame_rate, MinFrameRate(),
                                     MaxFrameRate(), NativeFrameRate());
  }

 private:
  bool SatisfiesFrameRateConstraint(const DoubleConstraint& constraint) const {
    double constraint_min =
        ConstraintHasMin(constraint) ? ConstraintMin(constraint) : -1.0;
    double constraint_max =
        ConstraintHasMax(constraint)
            ? ConstraintMax(constraint)
            : static_cast<double>(media::limits::kMaxFramesPerSecond);
    bool constraint_min_out_of_range =
        ((constraint_min > NativeFrameRate()) ||
         (constraint_min > MaxFrameRateConstraint().value_or(
                               media::limits::kMaxFramesPerSecond) +
                               DoubleConstraint::kConstraintEpsilon));
    bool constraint_max_out_of_range =
        ((constraint_max < kMinDeviceCaptureFrameRate) ||
         (constraint_max < MinFrameRateConstraint().value_or(0.0) -
                               DoubleConstraint::kConstraintEpsilon));
    bool constraint_self_contradicts = constraint_min > constraint_max;

    return !constraint_min_out_of_range && !constraint_max_out_of_range &&
           !constraint_self_contradicts;
  }

  // Native format for this candidate.
  media::VideoCaptureFormat format_;

  // Contains the set of allowed resolutions allowed by |format_| and subject
  // to applied constraints.
  ResolutionSet resolution_set_;

  // Contains the constrained range for the frameRate property, regardless
  // of what the native frame rate is. The intersection of this range and the
  // range [kMinDeviceCaptureFrameRate, NativeframeRate()] is the set of
  // frame rates supported by this candidate.
  DoubleRangeSet constrained_frame_rate_;
  IntRangeSet constrained_width_;
  IntRangeSet constrained_height_;
  DoubleRangeSet constrained_aspect_ratio_;

  // Contains the set of allowed rescale modes subject to applied constraints.
  BoolSet rescale_set_;
};

// Returns true if the facing mode |value| satisfies |constraints|, false
// otherwise.
bool FacingModeSatisfiesConstraint(mojom::blink::FacingMode value,
                                   const StringConstraint& constraint) {
  WebString string_value = ToWebString(value);
  if (string_value.IsNull()) {
    return constraint.Exact().empty();
  }

  return constraint.Matches(string_value);
}

class PTZDeviceState {
 public:
  explicit PTZDeviceState(const MediaTrackConstraintSetPlatform& constraint_set)
      : pan_set_(DoubleRangeSet::FromConstraint(constraint_set.pan)),
        tilt_set_(DoubleRangeSet::FromConstraint(constraint_set.tilt)),
        zoom_set_(DoubleRangeSet::FromConstraint(constraint_set.zoom)) {}

  PTZDeviceState(const DoubleRangeSet& pan_set,
                 const DoubleRangeSet& tilt_set,
                 const DoubleRangeSet& zoom_set)
      : pan_set_(pan_set), tilt_set_(tilt_set), zoom_set_(zoom_set) {}

  PTZDeviceState(const PTZDeviceState& other) = default;
  PTZDeviceState& operator=(const PTZDeviceState& other) = default;

  PTZDeviceState Intersection(
      const MediaTrackConstraintSetPlatform& constraint_set) const {
    DoubleRangeSet pan_intersection = pan_set_.Intersection(
        DoubleRangeSet::FromConstraint(constraint_set.pan));
    DoubleRangeSet tilt_intersection = tilt_set_.Intersection(
        DoubleRangeSet::FromConstraint(constraint_set.tilt));
    DoubleRangeSet zoom_intersection = zoom_set_.Intersection(
        DoubleRangeSet::FromConstraint(constraint_set.zoom));

    return PTZDeviceState(pan_intersection, tilt_intersection,
                          zoom_intersection);
  }

  bool IsEmpty() const {
    return pan_set_.IsEmpty() || tilt_set_.IsEmpty() || zoom_set_.IsEmpty();
  }

  double Fitness(
      const MediaTrackConstraintSetPlatform& basic_set,
      const media::VideoCaptureControlSupport& control_support) const {
    return NumericRangeSupportFitness(basic_set.pan, pan_set_,
                                      basic_set.pan.IsPresent(),
                                      control_support.pan) +
           NumericRangeSupportFitness(basic_set.tilt, tilt_set_,
                                      basic_set.tilt.IsPresent(),
                                      control_support.tilt) +
           NumericRangeSupportFitness(basic_set.zoom, zoom_set_,
                                      basic_set.zoom.IsPresent(),
                                      control_support.zoom);
  }

  const char* FailedConstraintName() const {
    MediaTrackConstraintSetPlatform dummy;
    if (pan_set_.IsEmpty()) {
      return dummy.pan.GetName();
    }
    if (tilt_set_.IsEmpty()) {
      return dummy.tilt.GetName();
    }
    if (zoom_set_.IsEmpty()) {
      return dummy.zoom.GetName();
    }

    // No failed constraint.
    return nullptr;
  }

  std::optional<double> SelectPan(
      const MediaTrackConstraintSetPlatform& basic_set) const {
    return SelectProperty(&PTZDeviceState::pan_set_, basic_set,
                          &MediaTrackConstraintSetPlatform::pan);
  }

  std::optional<double> SelectTilt(
      const MediaTrackConstraintSetPlatform& basic_set) const {
    return SelectProperty(&PTZDeviceState::tilt_set_, basic_set,
                          &MediaTrackConstraintSetPlatform::tilt);
  }

  std::optional<double> SelectZoom(
      const MediaTrackConstraintSetPlatform& basic_set) const {
    return SelectProperty(&PTZDeviceState::zoom_set_, basic_set,
                          &MediaTrackConstraintSetPlatform::zoom);
  }

 private:
  // Select the target value of a property based on the ideal value in
  // |basic_set| as follows:
  // If an ideal value is provided, return the value in the range closest to
  // ideal.
  // If no ideal value is provided:
  // * If minimum is provided, return minimum.
  // * Otherwise, if maximum is provided, return maximum.
  // * Otherwise, return nullopt.
  std::optional<double> SelectProperty(
      DoubleRangeSet PTZDeviceState::*ptz_field,
      const MediaTrackConstraintSetPlatform& basic_set,
      DoubleConstraint MediaTrackConstraintSetPlatform::*basic_set_field)
      const {
    if (!(basic_set.*basic_set_field).HasIdeal()) {
      return (this->*ptz_field).Min().has_value() ? (this->*ptz_field).Min()
                                                  : (this->*ptz_field).Max();
    }

    auto ideal = (basic_set.*basic_set_field).Ideal();
    if ((this->*ptz_field).Min().has_value() &&
        ideal < (this->*ptz_field).Min().value()) {
      return (this->*ptz_field).Min();
    }
    if ((this->*ptz_field).Max().has_value() &&
        ideal > (this->*ptz_field).Max().value()) {
      return (this->*ptz_field).Max();
    }

    return ideal;
  }

  DoubleRangeSet pan_set_;
  DoubleRangeSet tilt_set_;
  DoubleRangeSet zoom_set_;
};

class ImageCaptureDeviceState {
 public:
  class ApplyConstraintSetResult {
   public:
    ApplyConstraintSetResult() = default;

   private:
    friend class ImageCaptureDeviceState;

    std::optional<DoubleRangeSet> exposure_compensation_intersection_;
    std::optional<DoubleRangeSet> exposure_time_intersection_;
    std::optional<DoubleRangeSet> color_temperature_intersection_;
    std::optional<DoubleRangeSet> iso_intersection_;
    std::optional<DoubleRangeSet> brightness_intersection_;
    std::optional<DoubleRangeSet> contrast_intersection_;
    std::optional<DoubleRangeSet> saturation_intersection_;
    std::optional<DoubleRangeSet> sharpness_intersection_;
    std::optional<DoubleRangeSet> focus_distance_intersection_;
    std::optional<BoolSet> torch_intersection_;
    std::optional<BoolSet> background_blur_intersection_;
    std::optional<BoolSet> background_segmentation_mask_intersection_;
    std::optional<BoolSet> eye_gaze_correction_intersection_;
    std::optional<BoolSet> face_framing_intersection_;
  };

  explicit ImageCaptureDeviceState(const DeviceInfo& device) {}

  std::optional<ApplyConstraintSetResult> TryToApplyConstraintSet(
      const MediaTrackConstraintSetPlatform& constraint_set,
      const char** failed_constraint_name = nullptr) const {
    std::optional<ApplyConstraintSetResult> result(std::in_place);

    if (!(TryToApplyConstraint(constraint_set.exposure_compensation,
                               exposure_compensation_set_,
                               result->exposure_compensation_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.exposure_time, exposure_time_set_,
                               result->exposure_time_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.color_temperature,
                               color_temperature_set_,
                               result->color_temperature_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.iso, iso_set_,
                               result->iso_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.brightness, brightness_set_,
                               result->brightness_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.contrast, contrast_set_,
                               result->contrast_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.saturation, saturation_set_,
                               result->saturation_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.sharpness, sharpness_set_,
                               result->sharpness_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(
              constraint_set.focus_distance, focus_distance_set_,
              result->focus_distance_intersection_, failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.torch, torch_set_,
                               result->torch_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(
              constraint_set.background_blur, background_blur_set_,
              result->background_blur_intersection_, failed_constraint_name) &&
          TryToApplyConstraint(
              constraint_set.background_segmentation_mask,
              background_segmentation_mask_set_,
              result->background_segmentation_mask_intersection_,
              failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.eye_gaze_correction,
                               eye_gaze_correction_set_,
                               result->eye_gaze_correction_intersection_,
                               failed_constraint_name) &&
          TryToApplyConstraint(constraint_set.face_framing, face_framing_set_,
                               result->face_framing_intersection_,
                               failed_constraint_name))) {
      result.reset();
    }

    return result;
  }

  void ApplyResult(const ApplyConstraintSetResult& result) {
    if (result.exposure_compensation_intersection_.has_value()) {
      exposure_compensation_set_ = *result.exposure_compensation_intersection_;
    }
    if (result.exposure_time_intersection_.has_value()) {
      exposure_time_set_ = *result.exposure_time_intersection_;
    }
    if (result.color_temperature_intersection_.has_value()) {
      color_temperature_set_ = *result.color_temperature_intersection_;
    }
    if (result.iso_intersection_.has_value()) {
      iso_set_ = *result.iso_intersection_;
    }
    if (result.brightness_intersection_.has_value()) {
      brightness_set_ = *result.brightness_intersection_;
    }
    if (result.contrast_intersection_.has_value()) {
      contrast_set_ = *result.contrast_intersection_;
    }
    if (result.saturation_intersection_.has_value()) {
      saturation_set_ = *result.saturation_intersection_;
    }
    if (result.sharpness_intersection_.has_value()) {
      sharpness_set_ = *result.sharpness_intersection_;
    }
    if (result.focus_distance_intersection_.has_value()) {
      focus_distance_set_ = *result.focus_distance_intersection_;
    }
    if (result.torch_intersection_.has_value()) {
      torch_set_ = *result.torch_intersection_;
    }
    if (result.background_blur_intersection_.has_value()) {
      background_blur_set_ = *result.background_blur_intersection_;
    }
    if (result.background_segmentation_mask_intersection_.has_value()) {
      background_segmentation_mask_set_ =
          *result.background_segmentation_mask_intersection_;
    }
    if (result.eye_gaze_correction_intersection_.has_value()) {
      eye_gaze_correction_set_ = *result.eye_gaze_correction_intersection_;
    }
    if (result.face_framing_intersection_.has_value()) {
      face_framing_set_ = *result.face_framing_intersection_;
    }
  }

  double Fitness(
      const MediaTrackConstraintSetPlatform& basic_constraint_set) const {
    return NumericRangeSetFitness(basic_constraint_set.exposure_compensation,
                                  exposure_compensation_set_) +
           NumericRangeSetFitness(basic_constraint_set.exposure_time,
                                  exposure_time_set_) +
           NumericRangeSetFitness(basic_constraint_set.color_temperature,
                                  color_temperature_set_) +
           NumericRangeSetFitness(basic_constraint_set.iso, iso_set_) +
           NumericRangeSetFitness(basic_constraint_set.brightness,
                                  brightness_set_) +
           NumericRangeSetFitness(basic_constraint_set.contrast,
                                  contrast_set_) +
           NumericRangeSetFitness(basic_constraint_set.saturation,
                                  saturation_set_) +
           NumericRangeSetFitness(basic_constraint_set.sharpness,
                                  sharpness_set_) +
           NumericRangeSetFitness(basic_constraint_set.focus_distance,
                                  focus_distance_set_) +
           BoolSetFitness(basic_constraint_set.torch, torch_set_) +
           BoolSetFitness(basic_constraint_set.background_blur,
                          background_blur_set_) +
           BoolSetFitness(basic_constraint_set.background_segmentation_mask,
                          background_segmentation_mask_set_) +
           BoolSetFitness(basic_constraint_set.eye_gaze_correction,
                          eye_gaze_correction_set_) +
           BoolSetFitness(basic_constraint_set.face_framing, face_framing_set_);
  }

  std::optional<ImageCaptureDeviceSettings> SelectSettings(
      const MediaTrackConstraintSetPlatform& basic_constraint_set,
      const PTZDeviceState& ptz_state) const {
    std::optional<ImageCaptureDeviceSettings> settings(std::in_place);

    settings->exposure_compensation = SelectSetting(
        basic_constraint_set.exposure_compensation, exposure_compensation_set_);
    settings->exposure_time =
        SelectSetting(basic_constraint_set.exposure_time, exposure_time_set_);
    settings->color_temperature = SelectSetting(
        basic_constraint_set.color_temperature, color_temperature_set_);
    settings->iso = SelectSetting(basic_constraint_set.iso, iso_set_);
    settings->brightness =
        SelectSetting(basic_constraint_set.brightness, brightness_set_);
    settings->contrast =
        SelectSetting(basic_constraint_set.contrast, contrast_set_);
    settings->saturation =
        SelectSetting(basic_constraint_set.saturation, saturation_set_);
    settings->sharpness =
        SelectSetting(basic_constraint_set.sharpness, sharpness_set_);
    settings->focus_distance =
        SelectSetting(basic_constraint_set.focus_distance, focus_distance_set_);

    settings->pan = ptz_state.SelectPan(basic_constraint_set);
    settings->tilt = ptz_state.SelectTilt(basic_constraint_set);
    settings->zoom = ptz_state.SelectZoom(basic_constraint_set);

    settings->torch = SelectSetting(basic_constraint_set.torch, torch_set_);
    settings->background_blur = SelectSetting(
        basic_constraint_set.background_blur, background_blur_set_);
    settings->background_segmentation_mask =
        SelectSetting(basic_constraint_set.background_segmentation_mask,
                      background_segmentation_mask_set_);
    settings->eye_gaze_correction = SelectSetting(
        basic_constraint_set.eye_gaze_correction, eye_gaze_correction_set_);
    settings->face_framing =
        SelectSetting(basic_constraint_set.face_framing, face_framing_set_);

    if (!(settings->exposure_compensation || settings->exposure_time ||
          settings->color_temperature || settings->iso ||
          settings->brightness || settings->contrast || settings->saturation ||
          settings->sharpness || settings->focus_distance || settings->pan ||
          settings->tilt || settings->zoom || settings->torch ||
          settings->background_blur || settings->background_segmentation_mask ||
          settings->eye_gaze_correction || settings->face_framing)) {
      settings.reset();
    }

    return settings;
  }

 private:
  std::optional<bool> SelectSetting(const BooleanConstraint& basic_constraint,
                                    const BoolSet& set) const {
    if (basic_constraint.HasIdeal()) {
      auto ideal = basic_constraint.Ideal();
      if (set.Contains(ideal)) {
        return ideal;
      }
    }
    if (set.is_universal()) {
      return std::nullopt;
    }
    return set.FirstElement();
  }

  std::optional<double> SelectSetting(const DoubleConstraint& basic_constraint,
                                      const DoubleRangeSet& set) const {
    if (basic_constraint.HasIdeal()) {
      auto ideal = basic_constraint.Ideal();
      if (set.Contains(ideal)) {
        return ideal;
      }
      if (set.Min().has_value() && ideal < *set.Min()) {
        return *set.Min();
      }
      if (set.Max().has_value() && ideal > *set.Max()) {
        return *set.Max();
      }
    }
    if (!set.Max().has_value()) {
      return set.Min();  // Returns nullopt if Min() does not have a value.
    }
    if (!set.Min().has_value()) {
      return set.Max();
    }
    return (*set.Min() + *set.Max()) / 2;
  }

  BoolSet SetFromConstraint(const BooleanConstraint& constraint) const {
    return media_constraints::BoolSetFromConstraint(constraint);
  }

  DoubleRangeSet SetFromConstraint(const DoubleConstraint& constraint) const {
    return DoubleRangeSet::FromConstraint(constraint);
  }

  template <typename Constraint, typename Set>
  bool TryToApplyConstraint(
      const Constraint& constraint,
      const Set& current_set,
      std::optional<Set>& intersection,
      const char** failed_constraint_name = nullptr) const {
    if (!constraint.HasMandatory()) {
      return true;
    }
    intersection = current_set.Intersection(SetFromConstraint(constraint));
    if (intersection->IsEmpty()) {
      UpdateFailedConstraintName(constraint, failed_constraint_name);
      return false;
    }
    return true;
  }

  DoubleRangeSet exposure_compensation_set_;
  DoubleRangeSet exposure_time_set_;
  DoubleRangeSet color_temperature_set_;
  DoubleRangeSet iso_set_;
  DoubleRangeSet brightness_set_;
  DoubleRangeSet contrast_set_;
  DoubleRangeSet saturation_set_;
  DoubleRangeSet sharpness_set_;
  DoubleRangeSet focus_distance_set_;
  BoolSet torch_set_;
  BoolSet background_blur_set_;
  BoolSet background_segmentation_mask_set_;
  BoolSet eye_gaze_correction_set_;
  BoolSet face_framing_set_;
};

// Returns true if |constraint_set| can be satisfied by |device|. Otherwise,
// returns false and, if |failed_constraint_name| is not null, updates
// |failed_constraint_name| with the name of a constraint that could not be
// satisfied.
bool DeviceSatisfiesConstraintSet(
    const DeviceInfo& device,
    const MediaTrackConstraintSetPlatform& constraint_set,
    const char** failed_constraint_name = nullptr) {
  if (!constraint_set.device_id.Matches(WebString(device.device_id))) {
    UpdateFailedConstraintName(constraint_set.device_id,
                               failed_constraint_name);
    return false;
  }

  if (!constraint_set.group_id.Matches(WebString(device.group_id))) {
    UpdateFailedConstraintName(constraint_set.group_id, failed_constraint_name);
    return false;
  }

  if (!FacingModeSatisfiesConstraint(device.facing_mode,
                                     constraint_set.facing_mode)) {
    UpdateFailedConstraintName(constraint_set.facing_mode,
                               failed_constraint_name);
    return false;
  }

  if (constraint_set.pan.HasMandatory() && !device.control_support.pan) {
    UpdateFailedConstraintName(constraint_set.pan, failed_constraint_name);
    return false;
  }

  if (constraint_set.tilt.HasMandatory() && !device.control_support.tilt) {
    UpdateFailedConstraintName(constraint_set.tilt, failed_constraint_name);
    return false;
  }

  if (constraint_set.zoom.HasMandatory() && !device.control_support.zoom) {
    UpdateFailedConstraintName(constraint_set.zoom, failed_constraint_name);
    return false;
  }

  return true;
}

// Returns true if |value| satisfies the given |constraint|, false otherwise.
// If |constraint| is not satisfied and |failed_constraint_name| is not null,
// |failed_constraint_name| is set to |constraints|'s name.
bool OptionalBoolSatisfiesConstraint(
    const std::optional<bool>& value,
    const BooleanConstraint& constraint,
    const char** failed_constraint_name = nullptr) {
  if (!constraint.HasExact()) {
    return true;
  }

  if (value && *value == constraint.Exact()) {
    return true;
  }

  UpdateFailedConstraintName(constraint, failed_constraint_name);
  return false;
}

double DeviceFitness(const DeviceInfo& device,
                     const MediaTrackConstraintSetPlatform& constraint_set) {
  return StringConstraintFitnessDistance(WebString(device.device_id),
                                         constraint_set.device_id) +
         StringConstraintFitnessDistance(WebString(device.group_id),
                                         constraint_set.group_id) +
         StringConstraintFitnessDistance(ToWebString(device.facing_mode),
                                         constraint_set.facing_mode);
}

// Returns the fitness distance between |constraint_set| and |candidate| given
// that the configuration is already constrained by |candidate_format|.
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
// The track settings for |candidate| that correspond to the returned fitness
// are returned in |track_settings|.
double CandidateFitness(
    const DeviceInfo& device,
    const PTZDeviceState& ptz_state,
    const CandidateFormat& candidate_format,
    const ImageCaptureDeviceState& image_capture_device_state,
    const std::optional<bool>& noise_reduction,
    const MediaTrackConstraintSetPlatform& constraint_set,
    VideoTrackAdapterSettings* track_settings) {
  return DeviceFitness(device, constraint_set) +
         ptz_state.Fitness(constraint_set, device.control_support) +
         candidate_format.Fitness(constraint_set, track_settings) +
         image_capture_device_state.Fitness(constraint_set) +
         OptionalBoolFitness(noise_reduction,
                             constraint_set.goog_noise_reduction);
}

// This function appends additional entries to |distance_vector| based on
// custom distance metrics between some default settings and the candidate
// represented by |device|, |candidate_format| and |noise_reduction|.
// These entries are to be used as the final tie breaker for candidates that
// are equally good according to the spec and the custom distance functions
// between candidates and constraints.
void AppendDistancesFromDefault(
    const DeviceInfo& device,
    const CandidateFormat& candidate_format,
    const std::optional<bool>& noise_reduction,
    const VideoDeviceCaptureCapabilities& capabilities,
    int default_width,
    int default_height,
    double default_frame_rate,
    DistanceVector* distance_vector) {
  // Favor IDs that appear first in the enumeration.
  for (WTF::wtf_size_t i = 0; i < capabilities.device_capabilities.size();
       ++i) {
    if (device.device_id == capabilities.device_capabilities[i].device_id) {
      distance_vector->push_back(i);
      break;
    }
  }

  // Prefer not having a specific noise-reduction value and let the lower-layer
  // implementation choose a noise-reduction strategy.
  double noise_reduction_distance = noise_reduction ? HUGE_VAL : 0.0;
  distance_vector->push_back(noise_reduction_distance);

  // Prefer a native resolution closest to the default.
  double resolution_distance = ResolutionSet::Point::SquareEuclideanDistance(
      ResolutionSet::Point(candidate_format.NativeHeight(),
                           candidate_format.NativeWidth()),
      ResolutionSet::Point(default_height, default_width));
  distance_vector->push_back(resolution_distance);

  // Prefer a native frame rate close to the default.
  double frame_rate_distance = NumericConstraintFitnessDistance(
      candidate_format.NativeFrameRate(), default_frame_rate);
  distance_vector->push_back(frame_rate_distance);
}

}  // namespace

VideoInputDeviceCapabilities::VideoInputDeviceCapabilities() = default;

VideoInputDeviceCapabilities::VideoInputDeviceCapabilities(
    String device_id,
    String group_id,
    const media::VideoCaptureControlSupport& control_support,
    Vector<media::VideoCaptureFormat> formats,
    mojom::blink::FacingMode facing_mode)
    : device_id(std::move(device_id)),
      group_id(std::move(group_id)),
      control_support(control_support),
      formats(std::move(formats)),
      facing_mode(facing_mode) {}

VideoInputDeviceCapabilities::VideoInputDeviceCapabilities(
    VideoInputDeviceCapabilities&& other) = default;
VideoInputDeviceCapabilities& VideoInputDeviceCapabilities::operator=(
    VideoInputDeviceCapabilities&& other) = default;

VideoInputDeviceCapabilities::~VideoInputDeviceCapabilities() = default;

MediaStreamTrackPlatform::FacingMode ToPlatformFacingMode(
    mojom::blink::FacingMode video_facing) {
  switch (video_facing) {
    case mojom::blink::FacingMode::kNone:
      return MediaStreamTrackPlatform::FacingMode::kNone;
    case mojom::blink::FacingMode::kUser:
      return MediaStreamTrackPlatform::FacingMode::kUser;
    case mojom::blink::FacingMode::kEnvironment:
      return MediaStreamTrackPlatform::FacingMode::kEnvironment;
    default:
      return MediaStreamTrackPlatform::FacingMode::kNone;
  }
}

VideoDeviceCaptureCapabilities::VideoDeviceCaptureCapabilities() = default;
VideoDeviceCaptureCapabilities::VideoDeviceCaptureCapabilities(
    VideoDeviceCaptureCapabilities&& other) = default;
VideoDeviceCaptureCapabilities::~VideoDeviceCaptureCapabilities() = default;
VideoDeviceCaptureCapabilities& VideoDeviceCaptureCapabilities::operator=(
    VideoDeviceCaptureCapabilities&& other) = default;

VideoCaptureSettings SelectSettingsVideoDeviceCapture(
    const VideoDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints,
    int default_width,
    int default_height,
    double default_frame_rate) {
  DCHECK_GT(default_width, 0);
  DCHECK_GT(default_height, 0);
  DCHECK_GE(default_frame_rate, 0.0);
  // This function works only if infinity is defined for the double type.
  static_assert(std::numeric_limits<double>::has_infinity, "Requires infinity");

  // A distance vector contains:
  // a) For each advanced constraint set, a 0/Infinity value indicating if the
  //    candidate satisfies the corresponding constraint set.
  // b) Fitness distance for the candidate based on support for the ideal values
  //    of the basic constraint set.
  // c) A custom distance value based on how far the native format for a
  //    candidate is from the allowed and ideal resolution and frame rate after
  //    applying all constraint sets.
  // d) A custom distance value based on how close the candidate is to default
  //    settings.
  // Parts (a) and (b) are according to spec. Parts (c) and (d) are
  // implementation specific and used to break ties.
  DistanceVector best_distance(constraints.Advanced().size() + 2 +
                               kNumDefaultDistanceEntries);
  std::fill(best_distance.begin(), best_distance.end(), HUGE_VAL);
  VideoCaptureSettings result;
  const char* failed_constraint_name = result.failed_constraint_name();

  for (auto& device : capabilities.device_capabilities) {
    if (!DeviceSatisfiesConstraintSet(device, constraints.Basic(),
                                      &failed_constraint_name)) {
      continue;
    }

    ImageCaptureDeviceState image_capture_device_state(device);
    if (auto image_capture_device_result =
            image_capture_device_state.TryToApplyConstraintSet(
                constraints.Basic(), &failed_constraint_name)) {
      image_capture_device_state.ApplyResult(*image_capture_device_result);
    } else {
      continue;
    }

    PTZDeviceState ptz_device_state(constraints.Basic());
    if (ptz_device_state.IsEmpty()) {
      failed_constraint_name = ptz_device_state.FailedConstraintName();
      continue;
    }

    for (auto& format : device.formats) {
      PTZDeviceState ptz_state_for_format = ptz_device_state;
      CandidateFormat candidate_format(format);
      if (auto candidate_format_result =
              candidate_format.TryToApplyConstraintSet(
                  constraints.Basic(), &failed_constraint_name)) {
        candidate_format.ApplyResult(*candidate_format_result);
      } else {
        continue;
      }

      for (auto& noise_reduction : capabilities.noise_reduction_capabilities) {
        if (!OptionalBoolSatisfiesConstraint(
                noise_reduction, constraints.Basic().goog_noise_reduction,
                &failed_constraint_name)) {
          continue;
        }

        // At this point we have a candidate that satisfies all basic
        // constraints. The candidate consists of |device|, |candidate_format|
        // and |noise_reduction|.
        DistanceVector candidate_distance_vector;

        // First criteria for valid candidates is satisfaction of advanced
        // constraint sets.
        for (const auto& advanced_set : constraints.Advanced()) {
          PTZDeviceState ptz_advanced_state =
              ptz_state_for_format.Intersection(advanced_set);
          bool satisfies_advanced_set = false;

          if (DeviceSatisfiesConstraintSet(device, advanced_set) &&
              !ptz_advanced_state.IsEmpty() &&
              OptionalBoolSatisfiesConstraint(
                  noise_reduction, advanced_set.goog_noise_reduction)) {
            if (auto candidate_format_result =
                    candidate_format.TryToApplyConstraintSet(advanced_set)) {
              if (auto image_capture_device_result =
                      image_capture_device_state.TryToApplyConstraintSet(
                          advanced_set)) {
                satisfies_advanced_set = true;
                candidate_format.ApplyResult(*candidate_format_result);
                image_capture_device_state.ApplyResult(
                    *image_capture_device_result);
                ptz_state_for_format = ptz_advanced_state;
              }
            }
          }

          candidate_distance_vector.push_back(
              satisfies_advanced_set ? 0 : HUGE_VAL);
        }

        VideoTrackAdapterSettings track_settings;
        // Second criterion is fitness distance.
        candidate_distance_vector.push_back(
            CandidateFitness(device, ptz_state_for_format, candidate_format,
                             image_capture_device_state, noise_reduction,
                             constraints.Basic(), &track_settings));

        // Third criterion is native fitness distance.
        candidate_distance_vector.push_back(
            candidate_format.NativeFitness(constraints.Basic()));

        // Final criteria are custom distances to default settings.
        AppendDistancesFromDefault(device, candidate_format, noise_reduction,
                                   capabilities, default_width, default_height,
                                   default_frame_rate,
                                   &candidate_distance_vector);

        DCHECK_EQ(best_distance.size(), candidate_distance_vector.size());
        if (std::lexicographical_compare(candidate_distance_vector.begin(),
                                         candidate_distance_vector.end(),
                                         best_distance.begin(),
                                         best_distance.end())) {
          best_distance = candidate_distance_vector;

          media::VideoCaptureParams capture_params;
          capture_params.requested_format = candidate_format.format();
          result = VideoCaptureSettings(
              device.device_id.Utf8(), capture_params, noise_reduction,
              track_settings, candidate_format.constrained_frame_rate().Min(),
              candidate_format.constrained_frame_rate().Max(),
              image_capture_device_state.SelectSettings(constraints.Basic(),
                                                        ptz_state_for_format));
        }
      }
    }
  }

  if (!result.HasValue()) {
    return VideoCaptureSettings(failed_constraint_name);
  }

  return result;
}

base::expected<Vector<VideoCaptureSettings>, std::string>
SelectEligibleSettingsVideoDeviceCapture(
    const VideoDeviceCaptureCapabilities& capabilities,
    const MediaConstraints& constraints,
    int default_width,
    int default_height,
    double default_frame_rate) {
  Vector<VideoCaptureSettings> settings;
  std::string failed_constraint_name;
  for (const auto& device : capabilities.device_capabilities) {
    VideoDeviceCaptureCapabilities device_capabilities;
    device_capabilities.device_capabilities.emplace_back(
        device.device_id, device.group_id, device.control_support,
        device.formats, device.facing_mode);
    device_capabilities.noise_reduction_capabilities =
        capabilities.noise_reduction_capabilities;
    const auto device_settings = SelectSettingsVideoDeviceCapture(
        device_capabilities, constraints, default_width, default_height,
        default_frame_rate);
    if (device_settings.HasValue()) {
      settings.push_back(device_settings);
    } else {
      failed_constraint_name = device_settings.failed_constraint_name();
    }
  }

  if (settings.empty()) {
    return base::unexpected(failed_constraint_name);
  }
  return settings;
}

}  // namespace blink
