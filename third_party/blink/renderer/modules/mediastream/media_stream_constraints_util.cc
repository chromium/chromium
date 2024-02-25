// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_sets.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util_video_device.h"

namespace blink {

namespace {

template <typename P, typename T>
bool ScanConstraintsForExactValue(const MediaConstraints& constraints,
                                  P picker,
                                  T* value) {
  if (constraints.IsNull())
    return false;

  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMaxValue(const MediaConstraints& constraints,
                                P picker,
                                T* value) {
  if (constraints.IsNull())
    return false;
  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasMax()) {
    *value = the_field.Max();
    return true;
  }
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasMax()) {
      *value = advanced_field.Max();
      return true;
    }
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
      return true;
    }
  }
  return false;
}

template <typename P, typename T>
bool ScanConstraintsForMinValue(const MediaConstraints& constraints,
                                P picker,
                                T* value) {
  if (constraints.IsNull())
    return false;
  const auto& the_field = constraints.Basic().*picker;
  if (the_field.HasMin()) {
    *value = the_field.Min();
    return true;
  }
  if (the_field.HasExact()) {
    *value = the_field.Exact();
    return true;
  }
  for (const auto& advanced_constraint : constraints.Advanced()) {
    const auto& advanced_field = advanced_constraint.*picker;
    if (advanced_field.HasMin()) {
      *value = advanced_field.Min();
      return true;
    }
    if (advanced_field.HasExact()) {
      *value = advanced_field.Exact();
      return true;
    }
  }
  return false;
}

}  // namespace

const double kMinDeviceCaptureFrameRate = std::numeric_limits<double>::min();

VideoCaptureSettings::VideoCaptureSettings() : VideoCaptureSettings("") {}

VideoCaptureSettings::VideoCaptureSettings(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {
  DCHECK(failed_constraint_name_);
}

VideoCaptureSettings::VideoCaptureSettings(
    std::string device_id,
    media::VideoCaptureParams capture_params,
    std::optional<bool> noise_reduction,
    const VideoTrackAdapterSettings& track_adapter_settings,
    std::optional<double> min_frame_rate,
    std::optional<double> max_frame_rate,
    std::optional<ImageCaptureDeviceSettings> image_capture_device_settings)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      capture_params_(capture_params),
      noise_reduction_(noise_reduction),
      track_adapter_settings_(track_adapter_settings),
      min_frame_rate_(min_frame_rate),
      max_frame_rate_(max_frame_rate),
      image_capture_device_settings_(image_capture_device_settings) {
  DCHECK(!min_frame_rate ||
         *min_frame_rate_ <= capture_params.requested_format.frame_rate);
  DCHECK(!track_adapter_settings.target_size() ||
         track_adapter_settings.target_size()->width() <=
             capture_params.requested_format.frame_size.width());
  DCHECK(!track_adapter_settings_.target_size() ||
         track_adapter_settings_.target_size()->height() <=
             capture_params.requested_format.frame_size.height());
}

VideoCaptureSettings::VideoCaptureSettings(const VideoCaptureSettings& other) =
    default;
VideoCaptureSettings::VideoCaptureSettings(VideoCaptureSettings&& other) =
    default;
VideoCaptureSettings::~VideoCaptureSettings() = default;
VideoCaptureSettings& VideoCaptureSettings::operator=(
    const VideoCaptureSettings& other) = default;
VideoCaptureSettings& VideoCaptureSettings::operator=(
    VideoCaptureSettings&& other) = default;

AudioCaptureSettings::AudioCaptureSettings() : AudioCaptureSettings("") {}

AudioCaptureSettings::AudioCaptureSettings(const char* failed_constraint_name)
    : failed_constraint_name_(failed_constraint_name) {
  DCHECK(failed_constraint_name_);
}

AudioCaptureSettings::AudioCaptureSettings(
    std::string device_id,
    const std::optional<int>& requested_buffer_size,
    bool disable_local_echo,
    bool enable_automatic_output_device_selection,
    ProcessingType processing_type,
    const AudioProcessingProperties& audio_processing_properties,
    int num_channels)
    : failed_constraint_name_(nullptr),
      device_id_(std::move(device_id)),
      requested_buffer_size_(requested_buffer_size),
      disable_local_echo_(disable_local_echo),
      render_to_associated_sink_(enable_automatic_output_device_selection),
      processing_type_(processing_type),
      audio_processing_properties_(audio_processing_properties),
      num_channels_(num_channels) {}

AudioCaptureSettings::AudioCaptureSettings(const AudioCaptureSettings& other) =
    default;
AudioCaptureSettings& AudioCaptureSettings::operator=(
    const AudioCaptureSettings& other) = default;
AudioCaptureSettings::AudioCaptureSettings(AudioCaptureSettings&& other) =
    default;
AudioCaptureSettings& AudioCaptureSettings::operator=(
    AudioCaptureSettings&& other) = default;

bool GetConstraintValueAsBoolean(
    const MediaConstraints& constraints,
    const BooleanConstraint MediaTrackConstraintSetPlatform::*picker,
    bool* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintValueAsInteger(
    const MediaConstraints& constraints,
    const LongConstraint MediaTrackConstraintSetPlatform::*picker,
    int* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

bool GetConstraintMinAsInteger(
    const MediaConstraints& constraints,
    const LongConstraint MediaTrackConstraintSetPlatform::*picker,
    int* value) {
  return ScanConstraintsForMinValue(constraints, picker, value);
}

bool GetConstraintMaxAsInteger(
    const MediaConstraints& constraints,
    const LongConstraint MediaTrackConstraintSetPlatform::*picker,
    int* value) {
  return ScanConstraintsForMaxValue(constraints, picker, value);
}

bool GetConstraintValueAsDouble(
    const MediaConstraints& constraints,
    const DoubleConstraint MediaTrackConstraintSetPlatform::*picker,
    double* value) {
  return ScanConstraintsForExactValue(constraints, picker, value);
}

VideoTrackAdapterSettings SelectVideoTrackAdapterSettings(
    const MediaTrackConstraintSetPlatform& basic_constraint_set,
    const media_constraints::ResolutionSet& resolution_set,
    const media_constraints::NumericRangeSet<double>& frame_rate_set,
    const media::VideoCaptureFormat& source_format,
    bool enable_rescale) {
  std::optional<gfx::Size> target_resolution;
  if (enable_rescale) {
    media_constraints::ResolutionSet::Point resolution =
        resolution_set.SelectClosestPointToIdeal(
            basic_constraint_set, source_format.frame_size.height(),
            source_format.frame_size.width());
    int track_target_height = static_cast<int>(std::round(resolution.height()));
    int track_target_width = static_cast<int>(std::round(resolution.width()));
    target_resolution = gfx::Size(track_target_width, track_target_height);
  }
  double track_min_aspect_ratio =
      std::max(resolution_set.min_aspect_ratio(),
               static_cast<double>(resolution_set.min_width()) /
                   static_cast<double>(resolution_set.max_height()));
  double track_max_aspect_ratio =
      std::min(resolution_set.max_aspect_ratio(),
               static_cast<double>(resolution_set.max_width()) /
                   static_cast<double>(resolution_set.min_height()));
  // VideoTrackAdapter uses an unset frame rate to disable frame-rate
  // adjustment.
  std::optional<double> track_max_frame_rate = frame_rate_set.Max();
  if (basic_constraint_set.frame_rate.HasIdeal()) {
    track_max_frame_rate = std::max(basic_constraint_set.frame_rate.Ideal(),
                                    kMinDeviceCaptureFrameRate);
    if (frame_rate_set.Min() && *track_max_frame_rate < *frame_rate_set.Min()) {
      track_max_frame_rate = *frame_rate_set.Min();
    }
    if (frame_rate_set.Max() && *track_max_frame_rate > *frame_rate_set.Max()) {
      track_max_frame_rate = *frame_rate_set.Max();
    }
  }

  return VideoTrackAdapterSettings(target_resolution, track_min_aspect_ratio,
                                   track_max_aspect_ratio,
                                   track_max_frame_rate);
}

double NumericConstraintFitnessDistance(double value1, double value2) {
  if (std::fabs(value1 - value2) <= DoubleConstraint::kConstraintEpsilon)
    return 0.0;

  return std::fabs(value1 - value2) /
         std::max(std::fabs(value1), std::fabs(value2));
}

double StringConstraintFitnessDistance(const WebString& value,
                                       const StringConstraint& constraint) {
  if (!constraint.HasIdeal())
    return 0.0;

  for (auto& ideal_value : constraint.Ideal()) {
    // TODO(crbug.com/787254): Remove the explicit conversion to WebString when
    // this method operates solely over WTF::String.
    if (value == WebString(ideal_value))
      return 0.0;
  }

  return 1.0;
}

MediaStreamSource::Capabilities ComputeCapabilitiesForVideoSource(
    const String& device_id,
    const media::VideoCaptureFormats& formats,
    mojom::blink::FacingMode facing_mode,
    bool is_device_capture,
    const std::optional<std::string>& group_id) {
  MediaStreamSource::Capabilities capabilities;
  capabilities.device_id = std::move(device_id);
  if (is_device_capture) {
    capabilities.facing_mode = ToPlatformFacingMode(facing_mode);
    if (group_id)
      capabilities.group_id = String::FromUTF8(*group_id);
  }
  if (!formats.empty()) {
    int max_width = 1;
    int max_height = 1;
    float min_frame_rate =
        is_device_capture ? kMinDeviceCaptureFrameRate : 0.0f;
    float max_frame_rate = min_frame_rate;
    for (const auto& format : formats) {
      max_width = std::max(max_width, format.frame_size.width());
      max_height = std::max(max_height, format.frame_size.height());
      max_frame_rate = std::max(max_frame_rate, format.frame_rate);
    }
    capabilities.width = {1, static_cast<uint32_t>(max_width)};
    capabilities.height = {1, static_cast<uint32_t>(max_height)};
    capabilities.aspect_ratio = {1.0 / max_height,
                                 static_cast<double>(max_width)};
    capabilities.frame_rate = {min_frame_rate, max_frame_rate};
  }
  return capabilities;
}

}  // namespace blink
