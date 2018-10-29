// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util_video_device.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_constraints_util_sets.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "media/base/limits.h"
#include "media/mojo/interfaces/display_media_information.mojom.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

namespace {

using ResolutionSet = media_constraints::ResolutionSet;
using DoubleRangeSet = media_constraints::NumericRangeSet<double>;
using DeviceInfo = mojo::StructPtr<blink::mojom::VideoInputDeviceCapabilities>;
using DistanceVector = std::vector<double>;

// Number of default settings to be used as final tie-breaking criteria for
// settings that are equally good at satisfying constraints:
// device ID, noise reduction, resolution and frame rate.
const int kNumDefaultDistanceEntries = 4;

// VideoKind enum values. See https://w3c.github.io/mediacapture-depth.
const char kVideoKindColor[] = "color";
const char kVideoKindDepth[] = "depth";

blink::WebString ToWebString(media::VideoFacingMode facing_mode) {
  switch (facing_mode) {
    case media::MEDIA_VIDEO_FACING_USER:
      return blink::WebString::FromASCII("user");
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return blink::WebString::FromASCII("environment");
    default:
      return blink::WebString();
  }
}

// Returns the fitness distance between the ideal value of |constraint| and the
// closest value to it in the range [min, max].
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
template <typename NumericConstraint>
double NumericRangeFitness(const NumericConstraint& constraint,
                           decltype(constraint.Min()) min,
                           decltype(constraint.Min()) max) {
  if (!constraint.HasIdeal())
    return 0.0;

  if (constraint.Ideal() > max)
    return NumericConstraintFitnessDistance(max, constraint.Ideal());
  if (constraint.Ideal() < min)
    return NumericConstraintFitnessDistance(min, constraint.Ideal());

  return 0.0;
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
double OptionalBoolFitness(const base::Optional<bool>& value,
                           const blink::BooleanConstraint& constraint) {
  if (!constraint.HasIdeal())
    return 0.0;

  return value && value == constraint.Ideal() ? 0.0 : 1.0;
}

// If |failed_constraint_name| is not null, this function updates it with the
// name of |constraint|.
void UpdateFailedConstraintName(const blink::BaseConstraint& constraint,
                                const char** failed_constraint_name) {
  if (failed_constraint_name)
    *failed_constraint_name = constraint.GetName();
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
  const base::Optional<double>& MinFrameRateConstraint() const {
    return constrained_frame_rate_.Min();
  }
  const base::Optional<double>& MaxFrameRateConstraint() const {
    return constrained_frame_rate_.Max();
  }

  // Accessors that return the minimum and maximum frame rates supported by
  // this format, subject to applied constraints.
  double MaxFrameRate() const {
    if (MaxFrameRateConstraint())
      return std::min(*MaxFrameRateConstraint(), NativeFrameRate());
    return NativeFrameRate();
  }
  double MinFrameRate() const {
    if (MinFrameRateConstraint())
      return std::max(*MinFrameRateConstraint(), kMinDeviceCaptureFrameRate);
    return kMinDeviceCaptureFrameRate;
  }

  // Convenience accessor for video kind using Blink type.
  blink::WebString VideoKind() const { return GetVideoKindForFormat(format_); }

  // This function tries to apply |constraint_set| to this candidate format
  // and returns true if successful. If |constraint_set| cannot be satisfied,
  // false is returned, and the name of one of the constraints that
  // could not be satisfied is returned in |failed_constraint_name| if
  // |failed_constraint_name| is not null.
  bool ApplyConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const char** failed_constraint_name = nullptr) {
    auto resolution_intersection = resolution_set_.Intersection(
        ResolutionSet::FromConstraintSet(constraint_set));
    if (resolution_intersection.IsWidthEmpty()) {
      UpdateFailedConstraintName(constraint_set.width, failed_constraint_name);
      return false;
    }
    if (resolution_intersection.IsHeightEmpty()) {
      UpdateFailedConstraintName(constraint_set.height, failed_constraint_name);
      return false;
    }
    if (resolution_intersection.IsAspectRatioEmpty()) {
      UpdateFailedConstraintName(constraint_set.aspect_ratio,
                                 failed_constraint_name);
      return false;
    }

    if (!SatisfiesFrameRateConstraint(constraint_set.frame_rate)) {
      UpdateFailedConstraintName(constraint_set.frame_rate,
                                 failed_constraint_name);
      return false;
    }

    if (!constraint_set.video_kind.Matches(VideoKind())) {
      UpdateFailedConstraintName(constraint_set.video_kind,
                                 failed_constraint_name);
      return false;
    }

    resolution_set_ = resolution_intersection;
    constrained_frame_rate_ = constrained_frame_rate_.Intersection(
        DoubleRangeSet::FromConstraint(constraint_set.frame_rate, 0.0,
                                       media::limits::kMaxFramesPerSecond));

    return true;
  }

  // Returns the fitness distance for this candidate format.
  // Since a format can support multiple track settings, this function returns
  // the best fitness that can be achieved with this format.
  // The fitness function is based on
  // https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
  double Fitness(
      const blink::WebMediaTrackConstraintSet& constraint_set) const {
    return NumericRangeFitness(constraint_set.aspect_ratio, MinAspectRatio(),
                               MaxAspectRatio()) +
           NumericRangeFitness(constraint_set.height, MinHeight(),
                               MaxHeight()) +
           NumericRangeFitness(constraint_set.width, MinWidth(), MaxWidth()) +
           NumericRangeFitness(constraint_set.frame_rate, MinFrameRate(),
                               MaxFrameRate()) +
           StringConstraintFitnessDistance(VideoKind(),
                                           constraint_set.video_kind);
  }

  // Returns a custom "native" fitness distance that expresses how close the
  // native settings of this format are to the ideal and allowed ranges for
  // the corresponding width, height and frameRate properties.
  // This distance is intended to be used to break ties among candidates that
  // are equally good according to the standard fitness distance.
  double NativeFitness(
      const blink::WebMediaTrackConstraintSet& constraint_set) const {
    return NumericRangeNativeFitness(constraint_set.width, MinWidth(),
                                     MaxWidth(), NativeWidth()) +
           NumericRangeNativeFitness(constraint_set.height, MinHeight(),
                                     MaxHeight(), NativeHeight()) +
           NumericRangeNativeFitness(constraint_set.frame_rate, MinFrameRate(),
                                     MaxFrameRate(), NativeFrameRate());
  }

 private:
  bool SatisfiesFrameRateConstraint(const blink::DoubleConstraint& constraint) {
    double constraint_min =
        ConstraintHasMin(constraint) ? ConstraintMin(constraint) : -1.0;
    double constraint_max = ConstraintHasMax(constraint)
                                ? ConstraintMax(constraint)
                                : media::limits::kMaxFramesPerSecond;
    bool constraint_min_out_of_range =
        ((constraint_min > NativeFrameRate()) ||
         (constraint_min > MaxFrameRateConstraint().value_or(
                               media::limits::kMaxFramesPerSecond) +
                               blink::DoubleConstraint::kConstraintEpsilon));
    bool constraint_max_out_of_range =
        ((constraint_max < kMinDeviceCaptureFrameRate) ||
         (constraint_max < MinFrameRateConstraint().value_or(0.0) -
                               blink::DoubleConstraint::kConstraintEpsilon));
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
};

// Returns true if the facing mode |value| satisfies |constraints|, false
// otherwise.
bool FacingModeSatisfiesConstraint(media::VideoFacingMode value,
                                   const blink::StringConstraint& constraint) {
  blink::WebString string_value = ToWebString(value);
  if (string_value.IsNull())
    return constraint.Exact().empty();

  return constraint.Matches(string_value);
  }

  // Returns true if |constraint_set| can be satisfied by |device|. Otherwise,
  // returns false and, if |failed_constraint_name| is not null, updates
  // |failed_constraint_name| with the name of a constraint that could not be
  // satisfied.
  bool DeviceSatisfiesConstraintSet(
      const DeviceInfo& device,
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const char** failed_constraint_name = nullptr) {
    if (!constraint_set.device_id.Matches(
            blink::WebString::FromUTF8(device->device_id))) {
      UpdateFailedConstraintName(constraint_set.device_id,
                                 failed_constraint_name);
      return false;
    }

    if (!constraint_set.group_id.Matches(
            blink::WebString::FromUTF8(device->group_id))) {
      UpdateFailedConstraintName(constraint_set.group_id,
                                 failed_constraint_name);
      return false;
    }

    if (!FacingModeSatisfiesConstraint(device->facing_mode,
                                       constraint_set.facing_mode)) {
      UpdateFailedConstraintName(constraint_set.facing_mode,
                                 failed_constraint_name);
      return false;
    }

    return true;
}

// Returns true if |value| satisfies the given |constraint|, false otherwise.
// If |constraint| is not satisfied and |failed_constraint_name| is not null,
// |failed_constraint_name| is set to |constraints|'s name.
bool OptionalBoolSatisfiesConstraint(
    const base::Optional<bool>& value,
    const blink::BooleanConstraint& constraint,
    const char** failed_constraint_name = nullptr) {
  if (!constraint.HasExact())
    return true;

  if (value && *value == constraint.Exact())
    return true;

  UpdateFailedConstraintName(constraint, failed_constraint_name);
  return false;
}

double DeviceFitness(const DeviceInfo& device,
                     const blink::WebMediaTrackConstraintSet& constraint_set) {
  return StringConstraintFitnessDistance(
             blink::WebString::FromUTF8(device->device_id),
             constraint_set.device_id) +
         StringConstraintFitnessDistance(
             blink::WebString::FromUTF8(device->group_id),
             constraint_set.group_id) +
         StringConstraintFitnessDistance(ToWebString(device->facing_mode),
                                         constraint_set.facing_mode);
}

// Returns the fitness distance between |constraint_set| and |candidate| given
// that the configuration is already constrained by |candidate_format|.
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
double CandidateFitness(
    const DeviceInfo& device,
    const CandidateFormat& candidate_format,
    const base::Optional<bool>& noise_reduction,
    const blink::WebMediaTrackConstraintSet& constraint_set) {
  return DeviceFitness(device, constraint_set) +
         candidate_format.Fitness(constraint_set) +
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
    const base::Optional<bool>& noise_reduction,
    const VideoDeviceCaptureCapabilities& capabilities,
    int default_width,
    int default_height,
    double default_frame_rate,
    DistanceVector* distance_vector) {
  // Favor IDs that appear first in the enumeration.
  for (size_t i = 0; i < capabilities.device_capabilities.size(); ++i) {
    if (device->device_id == capabilities.device_capabilities[i]->device_id) {
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

VideoCaptureSettings ComputeVideoDeviceCaptureSettings(
    const DeviceInfo& device,
    const base::Optional<bool>& noise_reduction,
    const CandidateFormat& candidate_format,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  media::VideoCaptureParams capture_params;
  capture_params.requested_format = candidate_format.format();
  auto track_adapter_settings = SelectVideoTrackAdapterSettings(
      basic_constraint_set, candidate_format.resolution_set(),
      candidate_format.constrained_frame_rate(),
      capture_params.requested_format);

  return VideoCaptureSettings(device->device_id, capture_params,
                              noise_reduction, track_adapter_settings,
                              candidate_format.constrained_frame_rate().Min(),
                              candidate_format.constrained_frame_rate().Max());
}

}  // namespace

blink::WebString GetVideoKindForFormat(
    const media::VideoCaptureFormat& format) {
  return (format.pixel_format == media::PIXEL_FORMAT_Y16)
             ? blink::WebString::FromASCII(kVideoKindDepth)
             : blink::WebString::FromASCII(kVideoKindColor);
}

blink::WebMediaStreamTrack::FacingMode ToWebFacingMode(
    media::VideoFacingMode video_facing) {
  switch (video_facing) {
    case media::MEDIA_VIDEO_FACING_NONE:
      return blink::WebMediaStreamTrack::FacingMode::kNone;
    case media::MEDIA_VIDEO_FACING_USER:
      return blink::WebMediaStreamTrack::FacingMode::kUser;
    case media::MEDIA_VIDEO_FACING_ENVIRONMENT:
      return blink::WebMediaStreamTrack::FacingMode::kEnvironment;
    default:
      return blink::WebMediaStreamTrack::FacingMode::kNone;
  }
}

blink::WebMediaStreamTrack::DisplayCaptureSurfaceType ToWebDisplaySurface(
    media::mojom::DisplayCaptureSurfaceType display_surface) {
  switch (display_surface) {
    case media::mojom::DisplayCaptureSurfaceType::MONITOR:
      return blink::WebMediaStreamTrack::DisplayCaptureSurfaceType::kMonitor;
    case media::mojom::DisplayCaptureSurfaceType::WINDOW:
      return blink::WebMediaStreamTrack::DisplayCaptureSurfaceType::kWindow;
    case media::mojom::DisplayCaptureSurfaceType::APPLICATION:
      return blink::WebMediaStreamTrack::DisplayCaptureSurfaceType::
          kApplication;
    case media::mojom::DisplayCaptureSurfaceType::BROWSER:
      return blink::WebMediaStreamTrack::DisplayCaptureSurfaceType::kBrowser;
  }
}

blink::WebMediaStreamTrack::CursorCaptureType ToWebCursorCaptureType(
    media::mojom::CursorCaptureType cursor) {
  switch (cursor) {
    case media::mojom::CursorCaptureType::NEVER:
      return blink::WebMediaStreamTrack::CursorCaptureType::kNever;
    case media::mojom::CursorCaptureType::ALWAYS:
      return blink::WebMediaStreamTrack::CursorCaptureType::kAlways;
    case media::mojom::CursorCaptureType::MOTION:
      return blink::WebMediaStreamTrack::CursorCaptureType::kMotion;
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
    const blink::WebMediaConstraints& constraints,
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

    for (auto& format : device->formats) {
      CandidateFormat candidate_format(format);
      if (!candidate_format.ApplyConstraintSet(constraints.Basic(),
                                               &failed_constraint_name)) {
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
          bool satisfies_advanced_set =
              DeviceSatisfiesConstraintSet(device, advanced_set) &&
              OptionalBoolSatisfiesConstraint(
                  noise_reduction, advanced_set.goog_noise_reduction) &&
              // This must be the last in the condition since it is the only
              // one that has side effects. It should be executed only if the
              // previous two are true.
              candidate_format.ApplyConstraintSet(advanced_set);

          candidate_distance_vector.push_back(
              satisfies_advanced_set ? 0 : HUGE_VAL);
        }

        // Second criterion is fitness distance.
        candidate_distance_vector.push_back(CandidateFitness(
            device, candidate_format, noise_reduction, constraints.Basic()));

        // Third criterion is native fitness distance.
        candidate_distance_vector.push_back(
            candidate_format.NativeFitness(constraints.Basic()));

        // Final criteria are custom distances to default settings.
        AppendDistancesFromDefault(device, candidate_format, noise_reduction,
                                   capabilities, default_width, default_height,
                                   default_frame_rate,
                                   &candidate_distance_vector);

        DCHECK_EQ(best_distance.size(), candidate_distance_vector.size());
        if (candidate_distance_vector < best_distance) {
          best_distance = candidate_distance_vector;
          result = ComputeVideoDeviceCaptureSettings(
              device, noise_reduction, candidate_format, constraints.Basic());
        }
      }
    }
  }

  if (!result.HasValue())
    return VideoCaptureSettings(failed_constraint_name);

  return result;
}

}  // namespace content
