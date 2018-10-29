// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_H_

#include <string>

#include "base/logging.h"
#include "content/common/content_export.h"
#include "content/renderer/media/stream/media_stream_audio_processor_options.h"
#include "content/renderer/media/stream/video_track_adapter.h"
#include "media/base/video_facing.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/platform/modules/mediastream/media_devices.mojom.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"

namespace content {

namespace media_constraints {
class ResolutionSet;
template <typename T>
class NumericRangeSet;
}  // namespace media_constraints

extern const double kMinDeviceCaptureFrameRate;

// This class represents the output the SelectSettings algorithm for video
// constraints (see https://w3c.github.io/mediacapture-main/#dfn-selectsettings)
// The input to SelectSettings is a user-supplied constraints object, and its
// output is a set of implementation-specific settings that are used to
// configure other Chromium objects such as sources, tracks and sinks so that
// they work in the way indicated by the specification. VideoCaptureSettings may
// also be used to implement other constraints-related functionality, such as
// the getSettings() function.
// The following fields are used to control MediaStreamVideoSource objects:
//   * device_id: used for device selection and obtained from the deviceId
//   * capture_params: used to initialize video capture. Its values are obtained
//     from the width, height, aspectRatio, frame_rate, and googNoiseReduction
//     constraints.
// The following fields are used to control MediaStreamVideoTrack objects:
//   * track_adapter_settings: All track objects use a VideoTrackAdapter object
//     that may perform cropping and frame-rate adjustment. This field contains
//     the adapter settings suitable for the track the constraints are being
//     to. These settings are derived from the width, height, aspectRatio and
//     frameRate constraints.
// Some MediaStreamVideoSink objects (e.g. MediaStreamVideoWebRtcSink) require
// configuration derived from constraints that cannot be obtained from the
// source and track settings indicated above. The following fields are used
// to configure sinks:
//   * noise_reduction: used to control noise reduction for a screen-capture
//     track sent to a peer connection. Derive from the googNoiseReduction
//     constraint.
//   * min_frame_rate and max_frame_rate: used to control frame refreshes in
//     screen-capture tracks sent to a peer connection. Derived from the
//     frameRate constraint.
// If SelectSettings fails, the HasValue() method returns false and
// failed_constraint_name() returns the name of one of the (possibly multiple)
// constraints that could not be satisfied.
class CONTENT_EXPORT VideoCaptureSettings {
 public:
  // Creates an object without value and with an empty failed constraint name.
  VideoCaptureSettings();

  // Creates an object without value and with the given
  // |failed_constraint_name|. Does not take ownership of
  // |failed_constraint_name|, so it must point to a string that remains
  // accessible. |failed_constraint_name| must be non-null.
  explicit VideoCaptureSettings(const char* failed_constraint_name);

  // Creates an object with the given values.
  VideoCaptureSettings(std::string device_id,
                       media::VideoCaptureParams capture_params_,
                       base::Optional<bool> noise_reduction_,
                       const VideoTrackAdapterSettings& track_adapter_settings,
                       base::Optional<double> min_frame_rate,
                       base::Optional<double> max_frame_rate);

  VideoCaptureSettings(const VideoCaptureSettings& other);
  VideoCaptureSettings& operator=(const VideoCaptureSettings& other);
  VideoCaptureSettings(VideoCaptureSettings&& other);
  VideoCaptureSettings& operator=(VideoCaptureSettings&& other);
  ~VideoCaptureSettings();

  bool HasValue() const { return failed_constraint_name_ == nullptr; }

  // Convenience accessors for fields embedded in |capture_params_|.
  const media::VideoCaptureFormat& Format() const {
    return capture_params_.requested_format;
  }
  int Width() const {
    DCHECK(HasValue());
    return capture_params_.requested_format.frame_size.width();
  }
  int Height() const {
    DCHECK(HasValue());
    return capture_params_.requested_format.frame_size.height();
  }
  float FrameRate() const {
    DCHECK(HasValue());
    return capture_params_.requested_format.frame_rate;
  }
  media::ResolutionChangePolicy ResolutionChangePolicy() const {
    DCHECK(HasValue());
    return capture_params_.resolution_change_policy;
  }

  // Other accessors.
  const char* failed_constraint_name() const { return failed_constraint_name_; }

  const std::string& device_id() const {
    DCHECK(HasValue());
    return device_id_;
  }
  const media::VideoCaptureParams& capture_params() const {
    DCHECK(HasValue());
    return capture_params_;
  }
  const base::Optional<bool>& noise_reduction() const {
    DCHECK(HasValue());
    return noise_reduction_;
  }
  const VideoTrackAdapterSettings& track_adapter_settings() const {
    DCHECK(HasValue());
    return track_adapter_settings_;
  }
  const base::Optional<double>& min_frame_rate() const {
    DCHECK(HasValue());
    return min_frame_rate_;
  }
  const base::Optional<double>& max_frame_rate() const {
    DCHECK(HasValue());
    return max_frame_rate_;
  }

 private:
  const char* failed_constraint_name_;
  std::string device_id_;
  media::VideoCaptureParams capture_params_;
  base::Optional<bool> noise_reduction_;
  VideoTrackAdapterSettings track_adapter_settings_;
  base::Optional<double> min_frame_rate_;
  base::Optional<double> max_frame_rate_;
};

// This class represents the output the SelectSettings algorithm for audio
// constraints (see https://w3c.github.io/mediacapture-main/#dfn-selectsettings)
// The input to SelectSettings is a user-supplied constraints object, and its
// output is a set of implementation-specific settings that are used to
// configure other Chromium objects such as sources, tracks and sinks so that
// they work in the way indicated by the specification. AudioCaptureSettings may
// also be used to implement other constraints-related functionality, such as
// the getSettings() function.
// The following fields are used to control MediaStreamVideoSource objects:
//   * device_id: used for device selection and obtained from the deviceId
//   * device_parameters: these are the hardware parameters for the device
//     selected by SelectSettings. They can be used to verify that the
//     parameters with which the audio stream is actually created corresponds
//     to what SelectSettings selected. It can also be used to implement
//     getSettings() for device-related properties such as sampleRate and
//     channelCount.
// The following fields are used to control various audio features:
//   * hotword_enabled
//   * disable_local_echo
//   * render_to_associated_sink
// The audio_properties field is used to control the audio-processing module,
// which provides features such as software-based echo cancellation.
// If SelectSettings fails, the HasValue() method returns false and
// failed_constraint_name() returns the name of one of the (possibly multiple)
// constraints that could not be satisfied.
class CONTENT_EXPORT AudioCaptureSettings {
 public:
  // Creates an object without value and with an empty failed constraint name.
  AudioCaptureSettings();

  // Creates an object without value and with the given
  // |failed_constraint_name|. Does not take ownership of
  // |failed_constraint_name|, so it must point to a string that remains
  // accessible. |failed_constraint_name| must be non-null.
  explicit AudioCaptureSettings(const char* failed_constraint_name);

  // Creates an object with the given values.
  explicit AudioCaptureSettings(
      std::string device_id,
      bool enable_hotword,
      bool disable_local_echo,
      bool enable_automatic_output_device_selection,
      const AudioProcessingProperties& audio_processing_properties);
  AudioCaptureSettings(const AudioCaptureSettings& other);
  AudioCaptureSettings& operator=(const AudioCaptureSettings& other);
  AudioCaptureSettings(AudioCaptureSettings&& other);
  AudioCaptureSettings& operator=(AudioCaptureSettings&& other);

  bool HasValue() const { return failed_constraint_name_ == nullptr; }

  // Accessors.
  const char* failed_constraint_name() const { return failed_constraint_name_; }
  const std::string& device_id() const {
    DCHECK(HasValue());
    return device_id_;
  }
  bool hotword_enabled() const {
    DCHECK(HasValue());
    return hotword_enabled_;
  }
  bool disable_local_echo() const {
    DCHECK(HasValue());
    return disable_local_echo_;
  }
  bool render_to_associated_sink() const {
    DCHECK(HasValue());
    return render_to_associated_sink_;
  }
  AudioProcessingProperties audio_processing_properties() const {
    DCHECK(HasValue());
    return audio_processing_properties_;
  }

 private:
  const char* failed_constraint_name_;
  std::string device_id_;
  bool hotword_enabled_;
  bool disable_local_echo_;
  bool render_to_associated_sink_;
  AudioProcessingProperties audio_processing_properties_;
};

// Method to get boolean value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsBoolean(
    const blink::WebMediaConstraints& constraints,
    const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker,
    bool* value);

// Method to get int value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or Optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value);

bool CONTENT_EXPORT GetConstraintMinAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value);

bool CONTENT_EXPORT GetConstraintMaxAsInteger(
    const blink::WebMediaConstraints& constraints,
    const blink::LongConstraint blink::WebMediaTrackConstraintSet::*picker,
    int* value);

// Method to get double precision value of constraint with |name| from
// constraints. Returns true if the constraint is specified in either mandatory
// or Optional constraints.
bool CONTENT_EXPORT GetConstraintValueAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value);

bool CONTENT_EXPORT GetConstraintMinAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value);

bool CONTENT_EXPORT GetConstraintMaxAsDouble(
    const blink::WebMediaConstraints& constraints,
    const blink::DoubleConstraint blink::WebMediaTrackConstraintSet::*picker,
    double* value);

// Method to get std::string value of constraint with |name| from constraints.
// Returns true if the constraint is specified in either mandatory or Optional
// constraints.
bool CONTENT_EXPORT GetConstraintValueAsString(
    const blink::WebMediaConstraints& constraints,
    const blink::StringConstraint blink::WebMediaTrackConstraintSet::*picker,
    std::string* value);

template <typename ConstraintType>
bool ConstraintHasMax(const ConstraintType& constraint) {
  return constraint.HasMax() || constraint.HasExact();
}

template <typename ConstraintType>
bool ConstraintHasMin(const ConstraintType& constraint) {
  return constraint.HasMin() || constraint.HasExact();
}

template <typename ConstraintType>
auto ConstraintMax(const ConstraintType& constraint)
    -> decltype(constraint.Max()) {
  DCHECK(ConstraintHasMax(constraint));
  return constraint.HasExact() ? constraint.Exact() : constraint.Max();
}

template <typename ConstraintType>
auto ConstraintMin(const ConstraintType& constraint)
    -> decltype(constraint.Min()) {
  DCHECK(ConstraintHasMin(constraint));
  return constraint.HasExact() ? constraint.Exact() : constraint.Min();
}

// If |value| is outside the range of |constraint|, returns the name of the
// failed constraint. Otherwise, returns nullptr. The return value converts to
// bool in the expected way.
template <typename NumericConstraintType, typename ValueType>
const char* IsOutsideConstraintRange(NumericConstraintType constraint,
                                     ValueType value) {
  return (ConstraintHasMin(constraint) && value < ConstraintMin(constraint)) ||
                 (ConstraintHasMax(constraint) &&
                  value > ConstraintMax(constraint))
             ? constraint.GetName()
             : nullptr;
}

std::string GetMediaStreamSource(const blink::WebMediaConstraints& constraints);
bool IsDeviceCapture(const blink::WebMediaConstraints& constraints);

// This function selects track settings from a set of candidate resolutions and
// frame rates, given the source video-capture format and ideal values.
// The output are settings for a VideoTrackAdapter, which can adjust the
// resolution and frame rate of the source, and consist of maximum (or
// target) width, height and frame rate, and minimum and maximum aspect ratio.
// * Minimum and maximum aspect ratios are taken from |resolution_set| and are
//   not affected by ideal values.
// * The selected frame rate is always the value within the |frame_rate_set|
//   range that is closest to the ideal frame rate (or the source frame rate
//   if no ideal is supplied). If the chosen frame rate is greater than or equal
//   to the source's frame rate, a value of 0.0 is returned, which means that
//   there will be no frame-rate adjustment.
// * Width and height are selected using the
//   ResolutionSet::SelectClosestPointToIdeal function, using ideal values from
//   |basic_constraint_set| and using the source's width and height as the
//   default resolution. The width and height returned by
//   SelectClosestPointToIdeal are rounded to the nearest int. For more details,
//   see the documentation for ResolutionSet::SelectClosestPointToIdeal.
// Note that this function ignores the min/max/exact values from
// |basic_constraint_set|. Only its ideal values are used.
// This function has undefined behavior if any of |resolution_set| or
// |frame_rate_set| are empty.
VideoTrackAdapterSettings CONTENT_EXPORT SelectVideoTrackAdapterSettings(
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const media_constraints::ResolutionSet& resolution_set,
    const media_constraints::NumericRangeSet<double>& frame_rate_set,
    const media::VideoCaptureFormat& source_format);

// Generic distance function between two values for numeric constraints. Based
// on the fitness-distance function described in
// https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
double NumericConstraintFitnessDistance(double value1, double value2);

// Fitness distance between |value| and |constraint|.
// Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance.
double StringConstraintFitnessDistance(
    const blink::WebString& value,
    const blink::StringConstraint& constraint);

// This method computes capabilities for a video source based on the given
// |formats|. |facing_mode| is valid only in case of video device capture.
blink::WebMediaStreamSource::Capabilities CONTENT_EXPORT
ComputeCapabilitiesForVideoSource(
    const blink::WebString& device_id,
    const media::VideoCaptureFormats& formats,
    media::VideoFacingMode facing_mode,
    bool is_device_capture,
    const base::Optional<std::string>& group_id = base::nullopt);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_CONSTRAINTS_UTIL_H_
