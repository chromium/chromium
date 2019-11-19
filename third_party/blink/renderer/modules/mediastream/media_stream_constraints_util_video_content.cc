// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_content.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "media/base/limits.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"

namespace blink {

const int kMinScreenCastDimension = 1;
// Use kMaxDimension/2 as maximum to ensure selected resolutions have area less
// than media::limits::kMaxCanvas.
const int kMaxScreenCastDimension = media::limits::kMaxDimension / 2;
static_assert(kMaxScreenCastDimension * kMaxScreenCastDimension <
                  media::limits::kMaxCanvas,
              "Invalid kMaxScreenCastDimension");

const int kDefaultScreenCastWidth = 2880;
const int kDefaultScreenCastHeight = 1800;
static_assert(kDefaultScreenCastWidth <= kMaxScreenCastDimension,
              "Invalid kDefaultScreenCastWidth");
static_assert(kDefaultScreenCastHeight <= kMaxScreenCastDimension,
              "Invalid kDefaultScreenCastHeight");

const double kMaxScreenCastFrameRate = 120.0;
const double kDefaultScreenCastFrameRate =
    MediaStreamVideoSource::kDefaultFrameRate;

namespace {

using ResolutionSet = media_constraints::ResolutionSet;
using Point = ResolutionSet::Point;
using StringSet = media_constraints::DiscreteSet<std::string>;
using BoolSet = media_constraints::DiscreteSet<bool>;
using DoubleRangeSet = media_constraints::NumericRangeSet<double>;

constexpr double kMinScreenCastAspectRatio =
    static_cast<double>(kMinScreenCastDimension) /
    static_cast<double>(kMaxScreenCastDimension);
constexpr double kMaxScreenCastAspectRatio =
    static_cast<double>(kMaxScreenCastDimension) /
    static_cast<double>(kMinScreenCastDimension);

class VideoContentCaptureCandidates {
 public:
  VideoContentCaptureCandidates()
      : has_explicit_max_height_(false), has_explicit_max_width_(false) {}
  explicit VideoContentCaptureCandidates(
      const WebMediaTrackConstraintSet& constraint_set)
      : resolution_set_(ResolutionSet::FromConstraintSet(constraint_set)),
        has_explicit_max_height_(ConstraintHasMax(constraint_set.height) &&
                                 ConstraintMax(constraint_set.height) <=
                                     kMaxScreenCastDimension),
        has_explicit_max_width_(ConstraintHasMax(constraint_set.width) &&
                                ConstraintMax(constraint_set.width) <=
                                    kMaxScreenCastDimension),
        frame_rate_set_(
            DoubleRangeSet::FromConstraint(constraint_set.frame_rate,
                                           0.0,
                                           kMaxScreenCastFrameRate)),
        device_id_set_(media_constraints::StringSetFromConstraint(
            constraint_set.device_id)),
        noise_reduction_set_(media_constraints::BoolSetFromConstraint(
            constraint_set.goog_noise_reduction)),
        rescale_set_(media_constraints::RescaleSetFromConstraint(
            constraint_set.resize_mode)) {}

  VideoContentCaptureCandidates(VideoContentCaptureCandidates&& other) =
      default;
  VideoContentCaptureCandidates& operator=(
      VideoContentCaptureCandidates&& other) = default;

  bool IsEmpty() const {
    return resolution_set_.IsEmpty() || frame_rate_set_.IsEmpty() ||
           device_id_set_.IsEmpty() || noise_reduction_set_.IsEmpty() ||
           rescale_set_.IsEmpty();
  }

  VideoContentCaptureCandidates Intersection(
      const VideoContentCaptureCandidates& other) {
    VideoContentCaptureCandidates intersection;
    intersection.resolution_set_ =
        resolution_set_.Intersection(other.resolution_set_);
    intersection.has_explicit_max_height_ =
        has_explicit_max_height_ || other.has_explicit_max_height_;
    intersection.has_explicit_max_width_ =
        has_explicit_max_width_ || other.has_explicit_max_width_;
    intersection.frame_rate_set_ =
        frame_rate_set_.Intersection(other.frame_rate_set_);
    intersection.device_id_set_ =
        device_id_set_.Intersection(other.device_id_set_);
    intersection.noise_reduction_set_ =
        noise_reduction_set_.Intersection(other.noise_reduction_set_);
    intersection.rescale_set_ = rescale_set_.Intersection(other.rescale_set_);
    return intersection;
  }

  const ResolutionSet& resolution_set() const { return resolution_set_; }
  bool has_explicit_max_height() const { return has_explicit_max_height_; }
  bool has_explicit_max_width() const { return has_explicit_max_width_; }
  const DoubleRangeSet& frame_rate_set() const { return frame_rate_set_; }
  const StringSet& device_id_set() const { return device_id_set_; }
  const BoolSet& noise_reduction_set() const { return noise_reduction_set_; }
  const BoolSet& rescale_set() const { return rescale_set_; }
  void set_resolution_set(const ResolutionSet& set) { resolution_set_ = set; }
  void set_frame_rate_set(const DoubleRangeSet& set) { frame_rate_set_ = set; }

 private:
  ResolutionSet resolution_set_;
  bool has_explicit_max_height_;
  bool has_explicit_max_width_;
  DoubleRangeSet frame_rate_set_;
  StringSet device_id_set_;
  BoolSet noise_reduction_set_;
  BoolSet rescale_set_;
};

ResolutionSet ScreenCastResolutionCapabilities() {
  return ResolutionSet(kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastDimension, kMaxScreenCastDimension,
                       kMinScreenCastAspectRatio, kMaxScreenCastAspectRatio);
}

// This algorithm for selecting policy matches the old non-spec compliant
// algorithm in order to be more compatible with existing applications.
// TODO(guidou): Update this algorithm to properly take into account the minimum
// width and height, and the aspect_ratio constraint once most existing
// applications migrate to the new syntax. See https://crbug.com/701302.
media::ResolutionChangePolicy SelectResolutionPolicyFromCandidates(
    const ResolutionSet& resolution_set,
    media::ResolutionChangePolicy default_policy) {
  if (resolution_set.max_height() < kMaxScreenCastDimension &&
      resolution_set.max_width() < kMaxScreenCastDimension &&
      resolution_set.min_height() > kMinScreenCastDimension &&
      resolution_set.min_width() > kMinScreenCastDimension) {
    if (resolution_set.min_height() == resolution_set.max_height() &&
        resolution_set.min_width() == resolution_set.max_width()) {
      return media::ResolutionChangePolicy::FIXED_RESOLUTION;
    }

    int approx_aspect_ratio_min_resolution =
        100 * resolution_set.min_width() / resolution_set.min_height();
    int approx_aspect_ratio_max_resolution =
        100 * resolution_set.max_width() / resolution_set.max_height();
    if (approx_aspect_ratio_min_resolution ==
        approx_aspect_ratio_max_resolution) {
      return media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    }

    return media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }

  return default_policy;
}

int RoundToInt(double d) {
  return static_cast<int>(std::round(d));
}

gfx::Size ToGfxSize(const Point& point) {
  return gfx::Size(RoundToInt(point.width()), RoundToInt(point.height()));
}

double SelectFrameRateFromCandidates(
    const DoubleRangeSet& candidate_set,
    const WebMediaTrackConstraintSet& basic_constraint_set,
    double default_frame_rate) {
  double frame_rate = basic_constraint_set.frame_rate.HasIdeal()
                          ? basic_constraint_set.frame_rate.Ideal()
                          : default_frame_rate;
  if (candidate_set.Max() && frame_rate > *candidate_set.Max())
    frame_rate = *candidate_set.Max();
  else if (candidate_set.Min() && frame_rate < *candidate_set.Min())
    frame_rate = *candidate_set.Min();

  return frame_rate;
}

media::VideoCaptureParams SelectVideoCaptureParamsFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const WebMediaTrackConstraintSet& basic_constraint_set,
    int default_height,
    int default_width,
    double default_frame_rate,
    media::ResolutionChangePolicy default_resolution_policy) {
  double requested_frame_rate = SelectFrameRateFromCandidates(
      candidates.frame_rate_set(), basic_constraint_set, default_frame_rate);
  Point requested_resolution =
      candidates.resolution_set().SelectClosestPointToIdeal(
          basic_constraint_set, default_height, default_width);
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      ToGfxSize(requested_resolution), static_cast<float>(requested_frame_rate),
      media::PIXEL_FORMAT_I420);
  params.resolution_change_policy = SelectResolutionPolicyFromCandidates(
      candidates.resolution_set(), default_resolution_policy);
  // Content capture always uses default power-line frequency.
  DCHECK(params.IsValid());

  return params;
}

std::string SelectDeviceIDFromCandidates(
    const StringSet& candidates,
    const WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.device_id.HasIdeal()) {
    // If there are multiple elements specified by ideal, break ties by choosing
    // the first one that satisfies the constraints.
    for (const auto& ideal_entry : basic_constraint_set.device_id.Ideal()) {
      std::string ideal_value = ideal_entry.Ascii();
      if (candidates.Contains(ideal_value)) {
        return ideal_value;
      }
    }
  }

  // Return the empty string if nothing is specified in the constraints.
  // The empty string is treated as a default device ID by the browser.
  if (candidates.is_universal()) {
    return std::string();
  }

  // If there are multiple elements that satisfy the constraints, break ties by
  // using the element that was specified first.
  return candidates.FirstElement();
}

base::Optional<bool> SelectNoiseReductionFromCandidates(
    const BoolSet& candidates,
    const WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.goog_noise_reduction.HasIdeal() &&
      candidates.Contains(basic_constraint_set.goog_noise_reduction.Ideal())) {
    return base::Optional<bool>(
        basic_constraint_set.goog_noise_reduction.Ideal());
  }

  if (candidates.is_universal())
    return base::Optional<bool>();

  // A non-universal BoolSet can have at most one element.
  return base::Optional<bool>(candidates.FirstElement());
}

bool SelectRescaleFromCandidates(
    const BoolSet& candidates,
    const WebMediaTrackConstraintSet& basic_constraint_set) {
  DCHECK(!candidates.IsEmpty());
  if (basic_constraint_set.resize_mode.HasIdeal()) {
    for (const auto& ideal_resize_value :
         basic_constraint_set.resize_mode.Ideal()) {
      if (ideal_resize_value == WebMediaStreamTrack::kResizeModeNone &&
          candidates.Contains(false)) {
        return false;
      } else if (ideal_resize_value ==
                     WebMediaStreamTrack::kResizeModeRescale &&
                 candidates.Contains(true)) {
        return true;
      }
    }
  }

  DCHECK(!candidates.HasExplicitElements() ||
         candidates.elements().size() == 1);
  // Rescaling is the default for content capture.
  return candidates.HasExplicitElements() ? candidates.FirstElement() : true;
}

int ClampToValidScreenCastDimension(int value) {
  if (value > kMaxScreenCastDimension)
    return kMaxScreenCastDimension;
  else if (value < kMinScreenCastDimension)
    return kMinScreenCastDimension;
  return value;
}

VideoCaptureSettings SelectResultFromCandidates(
    const VideoContentCaptureCandidates& candidates,
    const WebMediaTrackConstraintSet& basic_constraint_set,
    mojom::MediaStreamType stream_type,
    int screen_width,
    int screen_height) {
  std::string device_id = SelectDeviceIDFromCandidates(
      candidates.device_id_set(), basic_constraint_set);
  // If a maximum width or height is explicitly given, use them as default.
  // If only one of them is given, use the default aspect ratio to determine the
  // other default value.
  int default_width = screen_width;
  int default_height = screen_height;
  double default_aspect_ratio =
      static_cast<double>(default_width) / default_height;
  if (candidates.has_explicit_max_height() &&
      candidates.has_explicit_max_width()) {
    default_height = candidates.resolution_set().max_height();
    default_width = candidates.resolution_set().max_width();
  } else if (candidates.has_explicit_max_height()) {
    default_height = candidates.resolution_set().max_height();
    default_width =
        static_cast<int>(std::round(default_height * default_aspect_ratio));
  } else if (candidates.has_explicit_max_width()) {
    default_width = candidates.resolution_set().max_width();
    default_height =
        static_cast<int>(std::round(default_width / default_aspect_ratio));
  }
  // When the given maximum values are large, the computed values using default
  // aspect ratio may fall out of range. Ensure the defaults are in the valid
  // range.
  default_height = ClampToValidScreenCastDimension(default_height);
  default_width = ClampToValidScreenCastDimension(default_width);

  // If a maximum frame rate is explicitly given, use it as default for
  // better compatibility with the old constraints algorithm.
  // TODO(guidou): Use the actual default when applications migrate to the new
  // constraint syntax.  https://crbug.com/710800
  double default_frame_rate =
      candidates.frame_rate_set().Max().value_or(kDefaultScreenCastFrameRate);

  // This default comes from the old algorithm.
  media::ResolutionChangePolicy default_resolution_policy =
      stream_type == mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE
          ? media::ResolutionChangePolicy::FIXED_RESOLUTION
          : media::ResolutionChangePolicy::ANY_WITHIN_LIMIT;

  media::VideoCaptureParams capture_params =
      SelectVideoCaptureParamsFromCandidates(
          candidates, basic_constraint_set, default_height, default_width,
          default_frame_rate, default_resolution_policy);

  base::Optional<bool> noise_reduction = SelectNoiseReductionFromCandidates(
      candidates.noise_reduction_set(), basic_constraint_set);

  bool enable_rescale = SelectRescaleFromCandidates(candidates.rescale_set(),
                                                    basic_constraint_set);

  auto track_adapter_settings = SelectVideoTrackAdapterSettings(
      basic_constraint_set, candidates.resolution_set(),
      candidates.frame_rate_set(), capture_params.requested_format,
      enable_rescale);

  return VideoCaptureSettings(std::move(device_id), capture_params,
                              noise_reduction, track_adapter_settings,
                              candidates.frame_rate_set().Min(),
                              candidates.frame_rate_set().Max());
}

VideoCaptureSettings UnsatisfiedConstraintsResult(
    const VideoContentCaptureCandidates& candidates,
    const WebMediaTrackConstraintSet& constraint_set) {
  DCHECK(candidates.IsEmpty());
  if (candidates.resolution_set().IsHeightEmpty()) {
    return VideoCaptureSettings(constraint_set.height.GetName());
  } else if (candidates.resolution_set().IsWidthEmpty()) {
    return VideoCaptureSettings(constraint_set.width.GetName());
  } else if (candidates.resolution_set().IsAspectRatioEmpty()) {
    return VideoCaptureSettings(constraint_set.aspect_ratio.GetName());
  } else if (candidates.frame_rate_set().IsEmpty()) {
    return VideoCaptureSettings(constraint_set.frame_rate.GetName());
  } else if (candidates.noise_reduction_set().IsEmpty()) {
    return VideoCaptureSettings(constraint_set.goog_noise_reduction.GetName());
  } else if (candidates.rescale_set().IsEmpty()) {
    return VideoCaptureSettings(constraint_set.resize_mode.GetName());
  } else {
    DCHECK(candidates.device_id_set().IsEmpty());
    return VideoCaptureSettings(constraint_set.device_id.GetName());
  }
}

}  // namespace

VideoCaptureSettings SelectSettingsVideoContentCapture(
    const WebMediaConstraints& constraints,
    mojom::MediaStreamType stream_type,
    int screen_width,
    int screen_height) {
  VideoContentCaptureCandidates candidates;
  candidates.set_resolution_set(ScreenCastResolutionCapabilities());

  candidates = candidates.Intersection(
      VideoContentCaptureCandidates(constraints.Basic()));
  if (candidates.IsEmpty())
    return UnsatisfiedConstraintsResult(candidates, constraints.Basic());

  for (const auto& advanced_set : constraints.Advanced()) {
    VideoContentCaptureCandidates advanced_candidates(advanced_set);
    VideoContentCaptureCandidates intersection =
        candidates.Intersection(advanced_candidates);
    if (!intersection.IsEmpty())
      candidates = std::move(intersection);
  }

  DCHECK(!candidates.IsEmpty());
  return SelectResultFromCandidates(candidates, constraints.Basic(),
                                    stream_type, screen_width, screen_height);
}

}  // namespace blink
